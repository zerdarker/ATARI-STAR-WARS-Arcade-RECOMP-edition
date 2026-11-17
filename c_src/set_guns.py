"""set_guns.py R L : pose les ancres X des canons (droite positive, gauche negative)
dans les deux images main du port C, quel que soit l'etat precedent.
Ex. 16:9 : python set_guns.py 650 -650  (origine 4:3 : 480 -480)"""
import os, sys

R = int(sys.argv[1]); L = int(sys.argv[2])
BUILD = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "disasm", "build")
FILES = [os.path.join(BUILD, "starwars_main_rev1.bin"),
         os.path.join(BUILD, "starwars_main_rev2.bin")]
SLOTS = [(0x341C, R), (0x34B6, R), (0x35CE, L), (0x3668, L)]
for path in FILES:
    data = bytearray(open(path, "rb").read())
    for off, v in SLOTS:
        w = v & 0x1FFF
        data[off] = w >> 8; data[off + 1] = w & 0xFF
    open(path, "wb").write(data)
    print(path, "-> R=%d L=%d" % (R, L))
