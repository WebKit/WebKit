#!/usr/bin/python

import sys
import os
import subprocess
import glob
from operator import itemgetter


def runBash(cmd):
    p = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE)
    out = p.stdout.read().strip()
    return out

current_arch = os.getenv("CURRENT_ARCH")

if current_arch != "x86_64" and current_arch != "arm64":
    sys.exit()

dir = os.getenv("OBJECT_FILE_DIR_" + os.getenv("CURRENT_VARIANT")) + "/" + current_arch

new_loc = os.getenv("BUILT_PRODUCTS_DIR") + "/" + os.getenv("JAVASCRIPTCORE_RESOURCES_DIR") + "/JavaScriptCoreRuntime"

symbol_table_loc = new_loc + "/JavaScriptCoreRuntime.symtbl"

symbol_table = {}

modified = False

runBash("mkdir -p " + new_loc)

for f in glob.iglob(dir + "/*.o"):
    if not os.path.isfile(f + ".bc") or os.path.getmtime(f) >= os.path.getmtime(f + ".bc"):
        modified = True
        print("ASSEMBLING: " + f)
        runBash("/usr/local/bin/llvm-as " + f)

        bcfile = f + ".bc"
        bcbase = os.path.basename(bcfile)

        print("COPYING BITCODE")
        runBash("cp " + bcfile + " " + new_loc + "/" + bcbase)

        print("APPENDING SYMBOLS")
        lines = runBash("/usr/local/bin/llvm-nm --defined-only -P " + bcfile).splitlines()

        for l in lines:
            sym, _, ty = l.partition(" ")
            if ty != "d" and ty != "D":
                symbol_table[sym] = bcbase

if modified:
    if os.path.isfile(symbol_table_loc):
        with open(symbol_table_loc, 'r') as file:
            print("LOADING SYMBOL TABLE")
            for l in iter(file.readline, ''):
                symbol, _, loc = l[:-1].partition(" ")
                if not symbol in symbol_table:
                    symbol_table[symbol] = loc

    symbol_list = symbol_table.items()

    print("WRITING SYMBOL TABLE")
    with open(symbol_table_loc, "w") as file:
        file.writelines(map(lambda (symbol, loc): symbol + " " + loc + "\n", symbol_list))
    print("DONE")
