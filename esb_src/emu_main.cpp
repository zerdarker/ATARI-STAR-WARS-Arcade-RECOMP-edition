/* emu_main.cpp - the Star Wars emulator twin as a machine for a host
 * (platform/sw_platform.h): the real ROMs on two 6809 cores (cpu/, AAE's core)
 * on the chip models (hw.cpp, board.cpp).  It is the machine the C port in
 * ../c_src was verified against, behind the same window, sound and controls,
 * so the two can be run side by side.
 *
 * This directory is self-contained: every file here is its own copy (the chip
 * models, the board, the Windows backend), and nothing is taken from ../c_src
 * at build time.  There is no translated C in it.
 *
 * Everything a player sees and hears is made here from the chips: the picture
 * from the vector generator's display list (avg.c, timed by its state PROM),
 * the sound from four POKEYs and the speech chip on the sound board's cycle
 * clock.
 *
 * Pacing: machine time is CPU cycles at 1.512 MHz, and sw_app_run() runs the
 * machine up to the wall clock.  Nothing else sets the speed: the 246 Hz
 * interrupt, the vector generator's HALT and the game's frame rate all come
 * from the cycle clock.
 */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>
#include "board.h"
#include "platform/sw_platform.h"
extern "C" {
#include "c012294.h"
}
#include "platform/windows/miniz.h"

extern "C" int plat_wide;                   /* 1 = 16:9 (plat_win.c, the options menu) */

namespace esb {

#define CPU_HZ      1512000.0
#define PHASE       0x4841u

static sw_app_opts opt;
static char err[300];
static std::vector<uint8_t> rom_main, rom_bank1, rom_bank2, rom_slap, rom_snd, rom_mb, rom_avg;
static sw_roms roms;
static sw_app_stats st;
static double t0_ms;
static uint64_t cyc0;
static int started;

const char *sw_app_error(void) { return err; }

static uint8_t *main_mem(void) { return SW.mem; }

/* ------------------------------------------------------------------ */
/* ROM images                                                           */
/* ------------------------------------------------------------------ */
/* One ROM of the MAME esb set (roms\esb.zip), found by its CRC as MAME does
 * (sets name the files differently), copied in pieces of `piece` bytes to the
 * offsets that follow - MAME's ROM_LOAD + ROM_CONTINUE. */
static int load_zip(mz_zip_archive *z, const char *name, mz_uint32 crc, size_t size, size_t piece,
                    std::initializer_list<std::pair<std::vector<uint8_t> *, size_t>> dst)
{
    std::vector<uint8_t> buf(size);
    for (mz_uint i = 0; i < mz_zip_reader_get_num_files(z); i++) {
        mz_zip_archive_file_stat fs;
        if (!mz_zip_reader_file_stat(z, i, &fs) || fs.m_crc32 != crc || fs.m_uncomp_size != size) continue;
        if (!mz_zip_reader_extract_to_mem(z, i, buf.data(), size, 0)) break;
        size_t off = 0;
        for (const auto &d : dst) { memcpy(d.first->data() + d.second, buf.data() + off, piece); off += piece; }
        return 0;
    }
    snprintf(err, sizeof err, "ROM %s (CRC %08X) is missing from %s\\esb.zip\n\n"
             "Put your MAME 'esb' ROM set (esb.zip) in that folder.", name, (unsigned)crc, opt.rom_dir);
    return 1;
}

/* 16:9: the picture is widened in the renderer (plat_win.c); these changes,
 * made in memory and never to esb.zip, widen the game's own field to match
 * - the same ones Star Wars got (StarWars-16-9\PATCHS-16-9.md). */
static void put16(std::vector<uint8_t> &v, size_t off, uint16_t w) { v[off] = (uint8_t)(w >> 8); v[off + 1] = (uint8_t)w; }

static void widescreen_patch(void)
{
    /* 3D object clamp, bank 2: CMPX/LDX #VGEDGL-24 and #VGEDGR+24 (+-510 -> +-680) */
    put16(rom_bank2, 0xD9B3 + 1 - 0xA000, 0xFD40);
    put16(rom_bank2, 0xD9B8 + 1 - 0xA000, 0xFD40);
    put16(rom_bank2, 0xD9BD + 1 - 0xA000, 0x02C0);
    put16(rom_bank2, 0xD9C2 + 1 - 0xA000, 0x02C0);

    /* star view cone, bank 1: "SUBD M_XPS" (width test) becomes a call to a
       routine in the free space at $7F00 that compares with X*2 (as Star
       Wars' wsstar.c: x4/3 left the outer edges of the wide picture bare) */
    static const uint8_t cone[] = {
        0x34, 0x06,             /* PSHS D          Y                       */
        0xFC, 0x50, 0x70,       /* LDD  $5070      X                       */
        0x58, 0x49,             /* ASLB ROLA       X * 2                   */
        0xED, 0xE3,             /* STD  ,--S                               */
        0xEC, 0x62,             /* LDD  2,S        Y                       */
        0xA3, 0xE1,             /* SUBD ,S++       Y - X*2, C = borrow      */
        0x32, 0x62,             /* LEAS 2,S        (flags kept)            */
        0x39                    /* RTS                                     */
    };
    memcpy(rom_bank1.data() + 0x7F00 - 0x6000, cone, sizeof cone);
    for (unsigned at : { 0x7B5Au, 0x7C6Au }) {
        uint8_t *p = rom_bank1.data() + at - 0x6000;
        p[0] = 0xBD; p[1] = 0x7F; p[2] = 0x00;     /* JSR $7F00 */
    }

    /* the guns: the first (blank) move of each gun picture in the vector ROM
       goes straight down from the hood; give it a sideways part, so each gun
       sits at 4/3 of its 4:3 offset (as Star Wars' +-480 -> +-650) */
    const int SPEEDER = 60, SPACE = 88;          /* vector units */
    put16(rom_main, 0x3424, (uint16_t)( SPEEDER & 0x1fff));   /* snowspeeder, right */
    put16(rom_main, 0x33B0, (uint16_t)(-SPEEDER & 0x1fff));   /* snowspeeder, left  */
    put16(rom_main, 0x349E, (uint16_t)( SPACE & 0x1fff));     /* space, right       */
    put16(rom_main, 0x3498, (uint16_t)(-SPACE & 0x1fff));     /* space, left        */

    /* the HUD's left block (scores, their box): its centre-relative X moves
       out by 170, as Star Wars' VGHUDL (-480 -> -650); the right block already
       reaches the 16:9 edge.  Found from the vector RAM: CNTR then LDD #dx. */
    for (unsigned at : { 0xAA4Au, 0xAA4Fu, 0xAAC4u, 0xAACEu, 0xAAF7u,     /* LDD #dx operands */
                         0xAB18u, 0xAB1Cu, 0xAB20u, 0xAB24u }) {           /* score lines {dy, dx} table */
        int v = (rom_main[at] << 8 | rom_main[at + 1]) & 0x1fff;
        if (v & 0x1000) v -= 0x2000;
        put16(rom_main, at, (uint16_t)((v - 170) & 0x1fff));
    }

    /* where the lasers start: slapstic page 3, table at $96B5, 8 bytes an
       entry {dy, dy2, dx left, dx right}; entries 0, 1, 4 fire from the
       snowspeeder's guns, 2 and 5 from the space guns */
    static const int shift[6] = { SPEEDER, SPEEDER, SPACE, 0, SPEEDER, SPACE };
    for (int i = 0; i < 6; i++) {
        size_t e = 0x6000 + 0x96B5 - 0x8000 + 8 * (size_t)i;
        int l = (int16_t)(rom_slap[e + 4] << 8 | rom_slap[e + 5]);
        int r = (int16_t)(rom_slap[e + 6] << 8 | rom_slap[e + 7]);
        if (!l && !r) continue;
        put16(rom_slap, e + 4, (uint16_t)(l - shift[i]));
        put16(rom_slap, e + 6, (uint16_t)(r + shift[i]));
    }
}

/* The ROM as loaded (4:3); 16:9 is it with widescreen_patch().  The options
 * menu can switch while the game runs, so the banks are rewritten in place
 * (the board keeps pointers to them). */
static std::vector<uint8_t> tube_main, tube_bank1, tube_bank2, tube_slap;
static int patched_wide = -1;

static void set_screen(void)
{
    memcpy(rom_main.data(), tube_main.data(), rom_main.size());
    memcpy(rom_bank1.data(), tube_bank1.data(), rom_bank1.size());
    memcpy(rom_bank2.data(), tube_bank2.data(), rom_bank2.size());
    memcpy(rom_slap.data(), tube_slap.data(), rom_slap.size());
    if (plat_wide) widescreen_patch();
    patched_wide = plat_wide;
}

static int load_esb(void)
{
    mz_zip_archive z;
    std::string zip = std::string(opt.rom_dir) + "\\esb.zip";
    memset(&z, 0, sizeof z);
    if (!mz_zip_reader_init_file(&z, zip.c_str(), 0)) {
        snprintf(err, sizeof err, "cannot open %s\n\nPut your MAME 'esb' ROM set (esb.zip) in that folder.", zip.c_str());
        return 1;
    }
    rom_main.assign(0x10000, 0);  rom_bank1.assign(0x2000, 0); rom_bank2.assign(0x6000, 0);
    rom_slap.assign(0x8000, 0);   rom_snd.assign(0x10000, 0);  rom_mb.assign(0x1000, 0);
    rom_avg.assign(0x100, 0);
    int bad = load_zip(&z, "136031-101.1f",  0xef1e3ae5, 0x4000, 0x2000, { { &rom_main, 0x6000 }, { &rom_bank1, 0x0000 } }) ||
              load_zip(&z, "136031-102.1jk", 0x62ce5c12, 0x4000, 0x2000, { { &rom_main, 0xa000 }, { &rom_bank2, 0x0000 } }) ||
              load_zip(&z, "136031-203.1kl", 0x27b0889b, 0x4000, 0x2000, { { &rom_main, 0xc000 }, { &rom_bank2, 0x2000 } }) ||
              load_zip(&z, "136031-104.1m",  0xfd5c725e, 0x4000, 0x2000, { { &rom_main, 0xe000 }, { &rom_bank2, 0x4000 } }) ||
              load_zip(&z, "136031-105.3u",  0xea9e4dce, 0x4000, 0x4000, { { &rom_slap, 0x0000 } }) ||
              load_zip(&z, "136031-106.2u",  0x76d07f59, 0x4000, 0x4000, { { &rom_slap, 0x4000 } }) ||
              load_zip(&z, "136031-111.1l",  0xb1f9bd12, 0x1000, 0x1000, { { &rom_main, 0x3000 } }) ||
              load_zip(&z, "136031-113.1jk", 0x24ae3815, 0x4000, 0x2000, { { &rom_snd, 0x4000 }, { &rom_snd, 0xc000 } }) ||
              load_zip(&z, "136031-112.1h",  0xca72d341, 0x4000, 0x2000, { { &rom_snd, 0x6000 }, { &rom_snd, 0xe000 } }) ||
              load_zip(&z, "136021-109.4b",  0x82fc3eb2, 0x0100, 0x0100, { { &rom_avg, 0 } }) ||
              load_zip(&z, "136031-110.7h",  0xb8d0f69d, 0x0400, 0x0400, { { &rom_mb, 0x000 } }) ||
              load_zip(&z, "136031-109.7j",  0x6a2a4d98, 0x0400, 0x0400, { { &rom_mb, 0x400 } }) ||
              load_zip(&z, "136031-108.7k",  0x6a76138f, 0x0400, 0x0400, { { &rom_mb, 0x800 } }) ||
              load_zip(&z, "136031-107.7l",  0xafbf6e01, 0x0400, 0x0400, { { &rom_mb, 0xc00 } });
    mz_zip_reader_end(&z);
    return bad;
}

/* ------------------------------------------------------------------ */
/* sound: four POKEYs and the speech chip, on the sound board's clock   */
/* ------------------------------------------------------------------ */
static ad_pokey pokey[4];
static uint64_t pokey_cyc;                  /* the sound-board cycle the POKEYs have been clocked to */
static int audio_live;
#define TMS_RING 16384
static short tms_ring[TMS_RING];
static uint64_t tms_w;                      /* speech samples received (8 kHz) */
static uint64_t out_n;                      /* output samples pushed */
static short tms_last;

static void pokeys_to(uint64_t cyc)
{
    if (cyc <= pokey_cyc) return;
    uint64_t n = cyc - pokey_cyc;
    if (n > 1512000) n = 1512000;
    for (int i = 0; i < 4; i++) ad_pokey_advance(&pokey[i], (uint32_t)n);
    pokey_cyc = cyc;
}

static void pokey_hook(int chip, int reg, uint8_t v)
{
    pokeys_to(SW.snd_cyc);
    ad_pokey_write(&pokey[chip & 3], (uint8_t)reg, v);
}

static void tms_hook(short s) { tms_ring[tms_w++ & (TMS_RING - 1)] = s; }

static void audio_pump(void)
{
    static int16_t buf[4][2048], mix[2048];
    pokeys_to(SW.snd_cyc);
    for (;;) {
        uint32_t n = 2048;
        for (int i = 0; i < 4; i++) {
            uint32_t a = ad_pokey_audio_available(&pokey[i]);
            if (a < n) n = a;
        }
        if (n == 0) break;
        for (int i = 0; i < 4; i++) ad_pokey_audio_read(&pokey[i], buf[i], (int)n);
        for (uint32_t k = 0; k < n; k++) {
            /* the speech sample for this output sample: 8 kHz held, a few samples behind */
            uint64_t idx = (out_n + k) * 8000u / SW_AUDIO_RATE;
            if (idx < tms_w && tms_w - idx < TMS_RING) tms_last = tms_ring[idx & (TMS_RING - 1)];
            int v = ((int)buf[0][k] + buf[1][k] + buf[2][k] + buf[3][k]) * opt.pokey_volume / 400
                  + (int)tms_last * opt.speech_volume / 200;
            mix[k] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
        }
        out_n += n;
        plat_audio_push(mix, (int)n);            /* the backend drops it when it has no stream */
    }
}

/* ------------------------------------------------------------------ */
/* picture: the display list the vector generator has just drawn        */
/* ------------------------------------------------------------------ */
static uint64_t shown_run = ~0ull;
static void on_pass(void);

static void vg_hook(int go)
{
    (void)go;
    const avg *a = &SW.vg;
    if (a->nvect == 0 || a->run_start == shown_run) return;
    shown_run = a->run_start;
    plat_video_begin();
    float px = 125.0f, py = 140.0f;
    int lit = 0;
    for (uint32_t i = 0; i < a->nvect; i++) {
        const avg_point *p = &a->vect[i];
        float x = (float)p->x / 65536.0f, y = (float)p->y / 65536.0f;
        if (p->z) {
            uint32_t rgb = 0xFF000000u | ((p->color & 4) ? 0xFFu : 0) | ((p->color & 2) ? 0xFF00u : 0)
                         | ((p->color & 1) ? 0xFF0000u : 0);
            plat_video_line(px, py, x, y, rgb, p->z);
            lit++;
        }
        px = x; py = y;
    }
    plat_video_present();
    st.pictures++;
    st.points = lit;
    on_pass();                  /* ESB's mainline is not Star Wars' $602F: a "pass" is a picture */
}

/* ------------------------------------------------------------------ */
/* script: a tests/scenarios file (tests/refrun.cpp's format)           */
/* ------------------------------------------------------------------ */
enum { SC_BYTE, SC_BIT_LOW, SC_RESET, SC_END, SC_POKE };
struct script_line { unsigned pass; int kind; uint8_t *port; uint8_t mask, val; unsigned addr; };
static std::vector<script_line> script;
static size_t script_pos;

static int load_script(const char *path)
{
    static const struct { const char *name; int kind; uint8_t *port; uint8_t mask; } ports[] = {
        { "in0", SC_BYTE, &SW.in0, 0 },       { "in1", SC_BYTE, &SW.in1, 0 },
        { "dsw0", SC_BYTE, &SW.dsw0, 0 },     { "dsw1", SC_BYTE, &SW.dsw1, 0 },
        { "pitch", SC_BYTE, &SW.adc_in[0], 0 }, { "yaw", SC_BYTE, &SW.adc_in[1], 0 },
        { "coinr", SC_BIT_LOW, &SW.in0, 0x01 }, { "coinl", SC_BIT_LOW, &SW.in0, 0x02 },
        { "coinaux", SC_BIT_LOW, &SW.in0, 0x04 }, { "slam", SC_BIT_LOW, &SW.in0, 0x08 },
        { "test", SC_BIT_LOW, &SW.in0, 0x10 },
        { "firer", SC_BIT_LOW, &SW.in0, 0x40 }, { "firel", SC_BIT_LOW, &SW.in0, 0x80 },
        { "diag", SC_BIT_LOW, &SW.in1, 0x04 },
        { "thumbr", SC_BIT_LOW, &SW.in1, 0x10 }, { "thumbl", SC_BIT_LOW, &SW.in1, 0x20 },
        { "sndtest", SC_BIT_LOW, &SW.snd_test, 0x10 },
        { "reset", SC_RESET, nullptr, 0 },    { "end", SC_END, nullptr, 0 },
    };
    char line[256], port[32];
    FILE *f = fopen(path, "r");
    if (!f) { snprintf(err, sizeof err, "cannot open script %s", path); return 1; }
    while (fgets(line, sizeof line, f)) {
        unsigned pass;
        int a = 0, b = 0;
        char *h = strchr(line, '#');
        if (h) *h = 0;
        int nf = sscanf(line, "%u %31s %i %i", &pass, port, &a, &b);
        if (nf < 2) continue;
        if (!strcmp(port, "poke") && nf == 4) {
            script.push_back({ pass, SC_POKE, nullptr, 0, (uint8_t)b, (unsigned)a });
            continue;
        }
        int found = 0;
        for (const auto &p : ports)
            if (!strcmp(port, p.name)) {
                script.push_back({ pass, p.kind, p.port, p.mask, (uint8_t)a, 0 });
                found = 1;
            }
        if (!found) { snprintf(err, sizeof err, "bad script line: %s", line); fclose(f); return 1; }
    }
    fclose(f);
    return 0;
}

static int reset_request;

static void apply_script(unsigned pass)
{
    while (script_pos < script.size() && script[script_pos].pass <= pass) {
        const script_line &s = script[script_pos++];
        switch (s.kind) {
        case SC_BYTE:    *s.port = s.val; break;
        case SC_BIT_LOW: *s.port = (uint8_t)(s.val ? (*s.port & ~s.mask) : (*s.port | s.mask)); break;
        case SC_POKE:    if (s.addr >= 0x4800 && s.addr < 0x6000) main_mem()[s.addr] = s.val; break;
        case SC_RESET:   reset_request = 1; break;
        default:         st.script_ended = 1; break;
        }
    }
}

static void on_pass(void)
{
    st.passes++;
    if (!script.empty()) apply_script((unsigned)st.passes);
}

/* ------------------------------------------------------------------ */
/* the machine                                                          */
/* ------------------------------------------------------------------ */
int sw_app_init(const sw_app_opts *o)
{
    opt = *o;
    if (load_esb()) return 1;
    tube_main = rom_main; tube_bank1 = rom_bank1; tube_bank2 = rom_bank2; tube_slap = rom_slap;
    set_screen();
    roms.main_rom = rom_main.data();
    roms.bank1 = rom_bank1.data();
    roms.bank2 = rom_bank2.data();
    roms.slapstic = rom_slap.data();
    roms.sound_rom = rom_snd.data();
    roms.mathbox_proms = rom_mb.data();
    roms.avg_prom = rom_avg.data();

    sw_init(&roms);
    SW.dsw0 = plat_dsw0();
    SW.dsw1 = plat_dsw1();
    SW.vg_hook = vg_hook;
    SW.pokey_hook = pokey_hook;
    SW.tms_hook = tms_hook;
    plat_nvram_read(SW.nv.eeprom, 0x100);        /* no file: a blank chip, and the program initialises it */

    for (int i = 0; i < 4; i++) {
        ad_pokey_init(&pokey[i], 1512000, SW_AUDIO_RATE);
        ad_pokey_set_measured_audio(&pokey[i], 20.0, 1.0);
    }
    audio_live = plat_audio_open(SW_AUDIO_RATE) == 0;

    if (opt.script && load_script(opt.script)) return 1;
    apply_script(0);
    sw_power_on();
    started = 0;
    return 0;
}

static void run_until(uint64_t target)
{
    while (SW.cyc < target) {
        sw_begin_step();
        if (reset_request) {
            reset_request = 0;
            sw_machine_reset(SW_RST_SCRIPT);
            continue;
        }
        sw_exec_step();
    }
}

/* The switches and the yoke, as the ports read them (board.cpp's header):
 * every switch is active low. */
static void live_inputs(void)
{
    plat_inputs in;
    plat_input_poll(&in);
    SW.in0 = (uint8_t)(0xdf & ~((in.coin_r ? 0x01 : 0) | (in.coin_l ? 0x02 : 0) | (in.coin_aux ? 0x04 : 0) |
                                (in.slam ? 0x08 : 0) | (in.test ? 0x10 : 0) | (in.fire_r ? 0x40 : 0) |
                                (in.fire_l ? 0x80 : 0)));
    SW.in1 = (uint8_t)(0x34 & ~((in.diag ? 0x04 : 0) | (in.thumb_r ? 0x10 : 0) | (in.thumb_l ? 0x20 : 0)));
    SW.snd_test = in.snd_test ? 0x00 : 0x10;
    SW.adc_in[0] = in.pitch;
    SW.adc_in[1] = in.yaw;
}

void sw_app_run(double now_ms)
{
    uint64_t now_cyc = SW.cyc;
    uint64_t target;

    if (script.empty()) live_inputs();

    if (opt.turbo) {
        target = now_cyc + 6144;                 /* one interrupt period per call */
    } else {
        if (!started) { started = 1; t0_ms = now_ms; cyc0 = now_cyc; }
        double want = (double)cyc0 + (now_ms - t0_ms) * (CPU_HZ / 1000.0);
        if (want - (double)now_cyc > CPU_HZ * 0.1) {     /* more than 100 ms behind: do not chase it */
            t0_ms = now_ms;
            cyc0 = now_cyc;
            want = (double)now_cyc + CPU_HZ * 0.004;
        }
        if (want <= (double)now_cyc) return;
        target = (uint64_t)want;
    }
    if (patched_wide != plat_wide) set_screen();
    run_until(target);
    audio_pump();
}

void sw_app_get_stats(sw_app_stats *s)
{
    st.cycles = SW.cyc;
    st.irqs = SW.n_irq;
    st.irqs_lost = SW.n_irq_lost;
    st.resets = SW.n_reset[SW_RST_WATCHDOG] + SW.n_reset[SW_RST_SCRIPT];
    st.watchdog_bites = SW.n_wd_bites;
    st.snd_resets = SW.n_snd_resets;
    st.swi = SW.n_slap_switch + SW.n_slap_alt;      /* slapstic bank switches */
    st.phase = main_mem()[PHASE];
    *s = st;
}

void sw_app_exit(void)
{
    /* The host stores the NOVRAM's RAM side when it closes, so that scores,
     * statistics and options survive.  (The program itself strobes NSTORE only
     * from the option page's NOVRAM test; what stores the chip on a real board
     * at power-down was not examined here.) */
    x2212_store(&SW.nv);
    plat_nvram_write(SW.nv.eeprom, 0x100);
    if (audio_live) plat_audio_close();
}

const char *sw_app_name(void) { return "ESB ROMs on 6809 cores"; }

}   /* namespace esb */

/* The machine as the Trilogy platform layer calls it (plat_win.c, --game esb). */
extern "C" {
int  esb_app_init(const sw_app_opts *o) { return esb::sw_app_init(o); }
void esb_app_run(double now_ms) { esb::sw_app_run(now_ms); }
void esb_app_exit(void) { esb::sw_app_exit(); }
void esb_app_get_stats(sw_app_stats *s) { esb::sw_app_get_stats(s); }
const char *esb_app_error(void) { return esb::sw_app_error(); }
const char *esb_app_name(void) { return esb::sw_app_name(); }
}
