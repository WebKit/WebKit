#!/usr/bin/python

import glob
import os
import subprocess
import sys
import shutil
import re
from sets import Set
from operator import itemgetter

print("Building Index Table")

RUNTIME_INSTALL_DIR = sys.argv[1]
JSC_DIR = sys.argv[2]
DERIVED_SOURCES_DIR = sys.argv[3]
LLVM_BINS = sys.argv[4]

try:
    os.mkdir(os.path.join(RUNTIME_INSTALL_DIR, "runtime"))
except OSError:
    pass

symbol_table_location = os.path.join(RUNTIME_INSTALL_DIR, "runtime", "Runtime.symtbl")

symbol_table = {}

symbol_table_is_out_of_date = False

symbol_table_modification_time = 0

if os.path.isfile(symbol_table_location):
    symbol_table_modification_time = os.path.getmtime(symbol_table_location)

file_suffix = "bc"
file_suffix_length = len(file_suffix)

tested_symbols_location = os.path.join(JSC_DIR, "tested-symbols.symlst")
include_symbol_table_location = os.path.join(DERIVED_SOURCES_DIR, "InlineRuntimeSymbolTable.h")

tested_symbols = Set([])

if os.path.isfile(tested_symbols_location):
    with open(tested_symbols_location, 'r') as file:
        print("Loading tested symbols")
        for line in file:
            tested_symbols.add(line[:-1])

for bitcode_file in glob.iglob(os.path.join(RUNTIME_INSTALL_DIR, "runtime", "*." + file_suffix)):
    bitcode_basename = os.path.basename(bitcode_file)

    print("Appending symbols from " + bitcode_basename)
    lines = subprocess.check_output([os.path.join(LLVM_BINS, "llvm-nm"), "--defined-only", bitcode_file]).splitlines()

    for line in lines:
        symbol = line.split()[1]
        if (symbol[:1] == "_" and symbol[-3:] != ".eh" and (("_" + symbol in tested_symbols) or ("_" + symbol[:7] + "L" + symbol[7:] in tested_symbols))):
            symbol_table[symbol] = bitcode_basename

if os.path.isfile(symbol_table_location):
    with open(symbol_table_location, 'r') as file:
        print("Loading symbol table")
        for line in file:
            symbol, _, location = line[:-1].partition(" ")
            # don't overwrite new symbols with old locations
            if not symbol in symbol_table:
                symbol_table[symbol] = location

symbol_list = symbol_table.items()

print("Writing symbol table: " + symbol_table_location)
print("Writing inline file: " + include_symbol_table_location)

with open(symbol_table_location, "w") as symbol_file:
    with open(include_symbol_table_location, "w") as include_file:
        include_file.write("#define FOR_EACH_LIBRARY_SYMBOL(macro)")
        for symbol, location in symbol_list:
            symbol_file.write("{} {}\n".format(symbol, location))
            include_file.write(" \\\nmacro(\"{}\", \"{}\")".format(symbol, location))
        include_file.write("\n")
print("Done")
