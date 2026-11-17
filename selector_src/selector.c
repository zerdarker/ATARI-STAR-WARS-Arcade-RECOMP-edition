/* selector.c - the game selection page: two framed title logos drawn with
 * the games' own vector beam, in the style of the 1983/1985 attract modes.
 *
 * The logos are not stored here: on first run each game draws its attract
 * mode hidden and fast (--turbo --hidden --shot pN logo.vec), and the
 * selector keeps the title's blue/cyan strokes from that picture.  So this
 * program carries no ROM data, like the games themselves.
 *
 * Files next to the exe: selection.ini, selection.log, logos\gameN.vec.
 * Picks with arrows / mouse / stick, plays with Enter / click / any button,
 * comes back here when the game quits; Esc leaves.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "framework.h"
#include "sys_gl.h"
#include "rawinput.h"
#include "joystick.h"
#include "vector_draw.h"
#include "mat4.h"
#include "ini.h"
#include "colordefs.h"
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

#define PIC_HH 300.0f
#define PIC_HW (PIC_HH * 16.0f / 9.0f)

/* ---- window: what the framework files expect from their host ---- */
static HWND hWnd;
static int  g_quit;
int SCREEN_W = 1280, SCREEN_H = 720, DESIGN_W = 1280, DESIGN_H = 720;
HWND win_get_window(void) { return hWnd; }
int KeyCheck(int k) { return key[k]; }

static int is_fullscreen;
static RECT windowed_rect;
static LONG_PTR windowed_style, windowed_exstyle;
int IsFullscreen(void) { return is_fullscreen; }

void ToggleFullscreen(void)
{
    if (is_fullscreen) {
        SetWindowLongPtr(hWnd, GWL_STYLE, windowed_style);
        SetWindowLongPtr(hWnd, GWL_EXSTYLE, windowed_exstyle);
        SetWindowPos(hWnd, HWND_NOTOPMOST, windowed_rect.left, windowed_rect.top,
                     windowed_rect.right - windowed_rect.left, windowed_rect.bottom - windowed_rect.top,
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        is_fullscreen = 0;
    } else {
        MONITORINFO mi = { sizeof(mi) };
        GetMonitorInfo(MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST), &mi);
        GetWindowRect(hWnd, &windowed_rect);
        windowed_style = GetWindowLongPtr(hWnd, GWL_STYLE);
        windowed_exstyle = GetWindowLongPtr(hWnd, GWL_EXSTYLE);
        SetWindowLongPtr(hWnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowLongPtr(hWnd, GWL_EXSTYLE, WS_EX_APPWINDOW | WS_EX_TOPMOST);
        SetWindowPos(hWnd, HWND_TOPMOST, mi.rcMonitor.left, mi.rcMonitor.top,
                     mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top,
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        is_fullscreen = 1;
    }
}

static float beam_proj[16];
static float beam_px = 2.0f;

static void update_beam_width(void)
{
    double vw = (double)SCREEN_W, span = (double)PIC_HW * 2.0;
    if ((double)SCREEN_H * DESIGN_W < vw * DESIGN_H) vw = (double)SCREEN_H * DESIGN_W / (double)DESIGN_H;
    if (vw < 1.0) vw = 1.0;
    beam_set_smoothing((float)(span / vw));
    beam_set_linewidth((float)((beam_px - 1.0) * span / (double)DESIGN_W + span / vw));
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_SIZE:
        if (LOWORD(lParam) && HIWORD(lParam)) {
            SCREEN_W = LOWORD(lParam);
            SCREEN_H = HIWORD(lParam);
            ViewOrthoScaled(SCREEN_W, SCREEN_H, DESIGN_W, DESIGN_H);
            update_beam_width();
        }
        return 0;
    case WM_ACTIVATE:
        if (LOWORD(wParam) == WA_INACTIVE) memset(key, 0, sizeof key);
        return 0;
    case WM_SYSKEYDOWN:
        if (wParam == VK_RETURN && (lParam & (1 << 29))) { ToggleFullscreen(); return 0; }
        return DefWindowProc(hwnd, message, wParam, lParam);
    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT) { SetCursor(NULL); return 1; }
        return DefWindowProc(hwnd, message, wParam, lParam);
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: ValidateRect(hwnd, NULL); return 0;
    case WM_CLOSE: g_quit = 1; return 0;
    case WM_INPUT:
        RawInput_ProcessInput(hwnd, wParam, lParam);
        return DefWindowProc(hwnd, message, wParam, lParam);
    case WM_DEVICECHANGE:
        joystick_device_change();
        return DefWindowProc(hwnd, message, wParam, lParam);
    default:
        return DefWindowProc(hwnd, message, wParam, lParam);
    }
}

static void pump_messages(void)
{
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) { g_quit = 1; break; }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

/* ---- stroke text, the games' options-menu font ---- */
typedef struct { char c; const char *s; } vglyph;
static const vglyph vfont[] = {
    { 'A', "00 04 26 44 40 , 02 42" }, { 'B', "00 06 36 45 44 33 42 41 30 00 , 03 33" },
    { 'C', "46 16 05 01 10 40" }, { 'D', "00 06 36 45 41 30 00" }, { 'E', "46 06 00 40 , 03 23" },
    { 'F', "46 06 00 , 03 23" }, { 'G', "46 16 05 01 10 40 43 23" }, { 'H', "00 06 , 40 46 , 03 43" },
    { 'I', "10 30 , 20 26 , 16 36" }, { 'J', "01 10 30 41 46 , 26 46" }, { 'K', "00 06 , 46 03 40" },
    { 'L', "06 00 40" }, { 'M', "00 06 24 46 40" }, { 'N', "00 06 40 46" },
    { 'O', "10 01 05 16 36 45 41 30 10" }, { 'P', "00 06 36 45 44 33 03" },
    { 'Q', "10 01 05 16 36 45 41 30 10 , 22 40" }, { 'R', "00 06 36 45 44 33 03 , 13 40" },
    { 'S', "01 10 30 41 42 33 13 04 05 16 36 45" }, { 'T', "06 46 , 20 26" },
    { 'U', "06 01 10 30 41 46" }, { 'V', "06 20 46" }, { 'W', "06 10 23 30 46" },
    { 'X', "00 46 , 06 40" }, { 'Y', "06 23 46 , 23 20" }, { 'Z', "06 46 00 40" },
    { '0', "10 01 05 16 36 45 41 30 10" }, { '1', "10 30 , 20 26 15" }, { '2', "05 16 36 45 43 00 40" },
    { '3', "06 46 24 33 42 41 30 10 01" }, { '4', "30 36 02 42" }, { '5', "46 06 03 33 42 41 30 10 01" },
    { '6', "45 36 16 05 01 10 30 41 42 33 13 02" }, { '7', "06 46 10" },
    { '8', "13 04 05 16 36 45 44 33 13 02 01 10 30 41 42 33" }, { '9', "01 10 30 41 45 36 16 05 04 13 33 42" },
    { ':', "21 22 , 24 25" }, { '.', "20 21" }, { '-', "03 43" }, { '/', "00 46" },
    { '<', "45 13 41" }, { '>', "05 33 01" }, { 0, NULL }
};

static void vfont_char(float x, float y, float sc, rgb_t col, char c)
{
    const char *p = NULL;
    float px = 0.0f, py = 0.0f;
    int have = 0, i;
    for (i = 0; vfont[i].s; i++)
        if (vfont[i].c == c) { p = vfont[i].s; break; }
    if (!p) return;
    while (*p) {
        if (*p == ' ') { p++; continue; }
        if (*p == ',') { have = 0; p++; continue; }
        {
            float nx = x + (float)(p[0] - '0') * sc, ny = y + (float)(p[1] - '0') * sc;
            if (have) beam_add_line_w(px, py, nx, ny, 0xFF, col, sc * 0.22f);
            px = nx; py = ny; have = 1;
            p += 2;
        }
    }
}

static void vfont_center(float cx, float y, float sc, rgb_t col, const char *s)
{
    float x = cx - (float)strlen(s) * 3.0f * sc;
    for (; *s; s++, x += 6.0f * sc) vfont_char(x, y, sc, col, *s);
}

/* ---- the games ---- */
typedef struct { float x0, y0, x1, y1; rgb_t rgb; int z; } lseg;
typedef struct {
    char title[64], exe[MAX_PATH], logo[MAX_PATH];
    int  picture;                           /* the attract picture that shows the title */
    lseg *seg; int n;
    float cx, cy, half_w, half_h;           /* the logo's bounding box */
} game;

static game games[2];
static char exe_dir[MAX_PATH];

/* Run exe from its own folder; wait_ms: 0 = do not wait, INFINITE = until it quits. */
static int run_game(const char *exe, const char *args, DWORD wait_ms)
{
    char cmd[2 * MAX_PATH + 128], dir[MAX_PATH], *slash;
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    snprintf(dir, sizeof dir, "%s", exe);
    slash = strrchr(dir, '\\');
    if (slash) *slash = '\0';
    snprintf(cmd, sizeof cmd, "\"%s\" %s", exe, args);
    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, dir, &si, &pi)) {
        LOG_ERROR("cannot start %s (error %lu)", exe, GetLastError());
        return 1;
    }
    if (wait_ms && WaitForSingleObject(pi.hProcess, wait_ms) == WAIT_TIMEOUT)
        TerminateProcess(pi.hProcess, 1);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 0;
}

/* The title strokes of an attract picture: blue or cyan, below the score line. */
static void load_logo(game *g)
{
    FILE *f;
    char line[160];
    float x0, y0, x1, y1, minx = 1e9f, maxx = -1e9f, miny = 1e9f, maxy = -1e9f;
    unsigned rgb;
    int z, cap = 0;

    if (GetFileAttributesA(g->logo) == INVALID_FILE_ATTRIBUTES) {
        char args[MAX_PATH + 64];
        snprintf(args, sizeof args, "--turbo --hidden --no-nvram --shot p%d \"%s\"", g->picture, g->logo);
        LOG_INFO("drawing the logo of %s: %s %s", g->title, g->exe, args);
        run_game(g->exe, args, 60000);
    }
    f = fopen(g->logo, "r");
    if (!f) { LOG_ERROR("no logo for %s (%s)", g->title, g->logo); return; }
    while (fgets(line, sizeof line, f)) {
        if (sscanf(line, "%f %f %f %f %x %d", &x0, &y0, &x1, &y1, &rgb, &z) != 6) continue;
        if ((rgb != 0xFF0000 && rgb != 0xFFFF00) || y0 > 200.0f || y1 > 200.0f) continue;   /* blue, cyan */
        if (g->n == cap) {
            lseg *p = (lseg *)realloc(g->seg, sizeof(lseg) * (size_t)(cap = cap ? cap * 2 : 256));
            if (!p) break;
            g->seg = p;
        }
        g->seg[g->n].x0 = x0; g->seg[g->n].y0 = y0; g->seg[g->n].x1 = x1; g->seg[g->n].y1 = y1;
        g->seg[g->n].rgb = (rgb_t)rgb | 0xFF000000u; g->seg[g->n].z = z;
        g->n++;
        minx = fminf(minx, fminf(x0, x1)); maxx = fmaxf(maxx, fmaxf(x0, x1));
        miny = fminf(miny, fminf(y0, y1)); maxy = fmaxf(maxy, fmaxf(y0, y1));
    }
    fclose(f);
    if (g->n) {
        g->cx = (minx + maxx) * 0.5f; g->cy = (miny + maxy) * 0.5f;
        g->half_w = (maxx - minx) * 0.5f; g->half_h = (maxy - miny) * 0.5f;
    }
    LOG_INFO("%s: %d logo strokes", g->title, g->n);
}

static void load_games(void)
{
    static const char *const sec[2] = { "game1", "game2" };
    static const char *const def_title[2] = { "STAR WARS - 1983", "THE EMPIRE STRIKES BACK - 1985" };
    static const char *const def_exe[2] = { "..\\StarWars-Arcade\\StarWars-16-9\\sw_win.exe",
                                            "..\\empire strikes back - Arcade\\ESB-16-9\\esb.exe" };
    static const int def_picture[2] = { 580, 590 };
    char dir[MAX_PATH];
    int i;

    snprintf(dir, sizeof dir, "%slogos", exe_dir);
    CreateDirectoryA(dir, NULL);
    for (i = 0; i < 2; i++) {
        game *g = &games[i];
        char *s = get_config_string(sec[i], "title", def_title[i]);
        snprintf(g->title, sizeof g->title, "%s", s ? s : def_title[i]);
        set_config_string(sec[i], "title", g->title);
        if (s) free(s);
        s = get_config_string(sec[i], "exe", def_exe[i]);
        if (!s) s = _strdup(def_exe[i]);
        set_config_string(sec[i], "exe", s);
        if (s[0] && (s[1] == ':' || s[0] == '\\')) snprintf(g->exe, sizeof g->exe, "%s", s);
        else snprintf(g->exe, sizeof g->exe, "%s%s", exe_dir, s);
        free(s);
        g->picture = get_config_int(sec[i], "logo_picture", def_picture[i]);
        set_config_int(sec[i], "logo_picture", g->picture);
        snprintf(g->logo, sizeof g->logo, "%s\\game%d.vec", dir, i + 1);
        load_logo(g);
    }
}

/* ---- drawing ---- */
#define N_STARS 120
static float star_x[N_STARS], star_y[N_STARS], star_z[N_STARS];

static float frand(void) { return (float)rand() / (float)RAND_MAX; }

static void stars(float dt)
{
    for (int i = 0; i < N_STARS; i++) {
        float sx, sy, br;
        star_z[i] -= dt * 0.25f;
        if (star_z[i] <= 0.05f) { star_x[i] = frand() * 2.0f - 1.0f; star_y[i] = frand() * 2.0f - 1.0f; star_z[i] = 1.0f; }
        sx = star_x[i] / star_z[i] * 300.0f;
        sy = star_y[i] / star_z[i] * 300.0f;
        if (fabsf(sx) > PIC_HW || fabsf(sy) > PIC_HH) { star_z[i] = 0.0f; continue; }
        br = 1.0f - star_z[i];
        beam_add_line(sx, sy, sx + 0.5f, sy, 0xFF, MAKE_RGB((int)(255 * br), (int)(255 * br), (int)(255 * br)));
    }
}

static void frame_box(float x0, float y0, float x1, float y1, rgb_t col)
{
    beam_add_line(x0, y0, x1, y0, 0xFF, col);
    beam_add_line(x1, y0, x1, y1, 0xFF, col);
    beam_add_line(x1, y1, x0, y1, 0xFF, col);
    beam_add_line(x0, y1, x0, y0, 0xFF, col);
}

static rgb_t dim(rgb_t c, float k)
{
    return MAKE_RGB((int)(RGB_RED(c) * k), (int)(RGB_GREEN(c) * k), (int)(RGB_BLUE(c) * k));
}

static void draw(int sel, double t, float dt)
{
    const rgb_t C_TITLE = MAKE_RGB(255, 40, 40), C_TEXT = MAKE_RGB(40, 255, 60);
    const rgb_t C_SEL = MAKE_RGB(255, 230, 50), C_BOX = MAKE_RGB(40, 60, 255), C_DIM = MAKE_RGB(170, 40, 40);
    const float bw = 470.0f, bh = 320.0f, by = -10.0f;       /* box width/height, box centre y */
    float pulse = 0.75f + 0.25f * (float)sin(t * 5.0);
    int i;

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    beam_clear();
    stars(dt);
    vfont_center(0.0f, 228.0f, 7.0f, C_TITLE, "SELECTION");
    for (i = 0; i < 2; i++) {
        const game *g = &games[i];
        float cx = (i == 0 ? -1.0f : 1.0f) * (bw * 0.5f + 20.0f);
        float x0 = cx - bw * 0.5f, x1 = cx + bw * 0.5f, y0 = by - bh * 0.5f, y1 = by + bh * 0.5f;
        float k = i == sel ? 1.0f : 0.45f;
        if (i == sel) {
            rgb_t c = dim(C_SEL, pulse);
            frame_box(x0, y0, x1, y1, c);
            frame_box(x0 - 7.0f, y0 - 7.0f, x1 + 7.0f, y1 + 7.0f, c);
        } else {
            frame_box(x0, y0, x1, y1, dim(C_BOX, 0.8f));
        }
        if (g->n) {                                          /* the logo fitted in the upper part */
            float aw = bw - 40.0f, ah = bh - 90.0f;
            float s = fminf(aw / (2.0f * g->half_w), ah / (2.0f * g->half_h));
            float ox = cx, oy = y1 - 20.0f - ah * 0.5f;
            for (int k2 = 0; k2 < g->n; k2++) {
                const lseg *l = &g->seg[k2];
                float b = (float)l->z / 255.0f * 1.8f * k;
                if (b > 1.0f) b = 1.0f;
                beam_add_line(ox + (l->x0 - g->cx) * s, oy + (l->y0 - g->cy) * s,
                              ox + (l->x1 - g->cx) * s, oy + (l->y1 - g->cy) * s, 0xFF, dim(l->rgb, b));
            }
        } else {
            vfont_center(cx, by + 20.0f, 4.0f, dim(C_TEXT, k), "LOGO ?");
        }
        vfont_center(cx, y0 + 30.0f, fminf(3.6f, (bw - 40.0f) / (6.0f * (float)strlen(g->title))),
                     i == sel ? C_SEL : dim(C_TEXT, 0.6f), g->title);
    }
    vfont_center(0.0f, -268.0f, 2.6f, C_DIM, "<  >: CHOISIR    ENTREE / TIR: JOUER    ECHAP: QUITTER");
    beam_draw_all(beam_proj);
}

/* ---- input: edges of left / right / play / quit from keys, mouse and sticks ---- */
enum { IN_LEFT = 1, IN_RIGHT = 2, IN_PLAY = 4, IN_QUIT = 8 };
static char joy_ignore[256];

static int joy_ignored(int d)
{
    const char *name = joystick_get_display_name(d), *p = joy_ignore;
    while (*p) {
        char tok[64];
        size_t n = strcspn(p, ";");
        if (n > 0 && n < sizeof tok) {
            memcpy(tok, p, n); tok[n] = '\0';
            if (strstr(name, tok)) return 1;
        }
        p += n + (p[n] == ';');
    }
    return 0;
}

static int read_inputs(void)
{
    static int mouse_acc;
    int mx, my, s = 0, d, b;

    if (key[KEY_LEFT]) s |= IN_LEFT;
    if (key[KEY_RIGHT]) s |= IN_RIGHT;
    if (key[KEY_ENTER] || key[KEY_SPACE] || key[KEY_LCONTROL] || (mouse_b & 1)) s |= IN_PLAY;
    if (key[KEY_ESC]) s |= IN_QUIT;
    get_mouse_mickeys(&mx, &my);
    mouse_acc += mx;
    if (mouse_acc < -60) { s |= IN_LEFT; mouse_acc = 0; }
    if (mouse_acc > 60) { s |= IN_RIGHT; mouse_acc = 0; }
    poll_joystick();
    for (d = 0; d < num_joysticks; d++) {
        if (!joystick_is_connected(d) || !joystick_has_stick(d) || joy_ignored(d)) continue;
        if (joy[d].stick[0].axis[0].pos < -64) s |= IN_LEFT;
        if (joy[d].stick[0].axis[0].pos > 64) s |= IN_RIGHT;
        for (b = 0; b < joy[d].num_buttons && b < 4; b++)
            if (joy[d].button[b].b) s |= IN_PLAY;
    }
    return s;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrev, LPSTR cmd, int show)
{
    WNDCLASS wc;
    DWORD style = WS_OVERLAPPEDWINDOW;
    RECT wr = { 0, 0, 1280, 720 };
    char path[MAX_PATH], *slash;
    int sel, prev = -1, start_full;
    LARGE_INTEGER freq, t0, now;
    double last = 0.0;
    (void)hPrev; (void)cmd; (void)show;

    GetModuleFileNameA(NULL, exe_dir, sizeof exe_dir);
    slash = strrchr(exe_dir, '\\');
    if (slash) slash[1] = '\0';
    snprintf(path, sizeof path, "%sselection.log", exe_dir);
    log_open(path);
    log_set_level(LOG_LEVEL_INFO);
    snprintf(path, sizeof path, "%sselection.ini", exe_dir);
    set_config_file(path);

    SetProcessDPIAware();
    memset(&wc, 0, sizeof wc);
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = L"StarWarsSelection";
    RegisterClass(&wc);
    AdjustWindowRect(&wr, style, FALSE);
    hWnd = CreateWindow(L"StarWarsSelection", L"Star Wars Arcade", style, CW_USEDEFAULT, CW_USEDEFAULT,
                        wr.right - wr.left, wr.bottom - wr.top, NULL, NULL, hInstance, NULL);
    if (!hWnd || !CreateGLContext()) { MessageBoxA(NULL, "No OpenGL window.", "Star Wars Arcade", MB_OK); return 1; }
    SetSwapMode(1);
    if (!beam_init()) { MessageBoxA(NULL, "The beam renderer needs OpenGL 3.3.", "Star Wars Arcade", MB_OK); return 1; }
    beam_set_color_mode(1);
    mat4_ortho(beam_proj, -PIC_HW, PIC_HW, -PIC_HH, PIC_HH, -1.0f, 1.0f);
    RawInput_Initialize(hWnd);

    {
        char *s = get_config_string("joystick", "ignore", "ROCCAT");
        snprintf(joy_ignore, sizeof joy_ignore, "%s", s ? s : "");
        set_config_string("joystick", "ignore", joy_ignore);
        if (s) free(s);
    }
    install_joystick();
    sel = get_config_int("main", "last", 0) ? 1 : 0;
    start_full = get_config_int("main", "fullscreen", 1) != 0;
    set_config_int("main", "fullscreen", start_full);
    load_games();

    for (int i = 0; i < N_STARS; i++) { star_x[i] = frand() * 2 - 1; star_y[i] = frand() * 2 - 1; star_z[i] = frand(); }
    ShowWindow(hWnd, SW_SHOW);
    SetForegroundWindow(hWnd);
    if (start_full) ToggleFullscreen();
    timeBeginPeriod(1);
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t0);

    while (!g_quit) {
        int in, edge;
        double t;
        pump_messages();
        QueryPerformanceCounter(&now);
        t = (double)(now.QuadPart - t0.QuadPart) / (double)freq.QuadPart;
        in = read_inputs();
        edge = prev < 0 ? 0 : in & ~prev;        /* nothing held at start (or on return) counts */
        prev = in;
        if (edge & IN_QUIT) break;
        if (edge & IN_LEFT) sel = 0;
        if (edge & IN_RIGHT) sel = 1;
        if (edge & IN_PLAY) {
            set_config_int("main", "last", sel);
            LOG_INFO("playing %s", games[sel].title);
            if (is_fullscreen) ToggleFullscreen();
            ShowWindow(hWnd, SW_HIDE);
            run_game(games[sel].exe, "", INFINITE);
            ShowWindow(hWnd, SW_SHOW);
            SetForegroundWindow(hWnd);
            if (start_full) ToggleFullscreen();
            memset(key, 0, sizeof key);
            mouse_b = 0;
            prev = -1;                           /* the key that quit the game must not act here */
            QueryPerformanceCounter(&now);
            last = (double)(now.QuadPart - t0.QuadPart) / (double)freq.QuadPart;
            continue;
        }
        draw(sel, t, (float)(t - last));
        last = t;
        glSwap();
    }

    timeEndPeriod(1);
    beam_shutdown();
    remove_joystick();
    DeleteGLContext();
    log_close();
    return 0;
}
