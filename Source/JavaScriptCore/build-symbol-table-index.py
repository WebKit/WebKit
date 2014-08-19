#!/usr/bin/python

import glob
import os
import subprocess
import sys
from sets import Set
from operator import itemgetter


current_arch = sys.argv[1]

print("Building Index Table")

if current_arch not in ("x86_64", "arm64"):
    sys.exit()

bitcode_file_original_directory = os.path.join(os.getenv("TARGET_TEMP_DIR"), "Objects-" + os.getenv("CURRENT_VARIANT"), current_arch)

if not os.path.isdir(bitcode_file_original_directory):
    print("Failed to build index table at " + bitcode_file_original_directory)
    sys.exit()

framework_directory = os.path.join(os.getenv("BUILT_PRODUCTS_DIR"), os.getenv("JAVASCRIPTCORE_RESOURCES_DIR"), "Runtime", current_arch)


symbol_table_location = os.path.join(framework_directory, "Runtime.symtbl")

symbol_table = {}

symbol_table_is_out_of_date = False

symbol_table_modification_time = 0

if os.path.isfile(symbol_table_location):
    symbol_table_modification_time = os.path.getmtime(symbol_table_location)

file_suffix = "bc"
file_suffix_length = len(file_suffix)

tested_symbols_location = "./tested-symbols.symlst"
include_symbol_table_location = os.path.join(os.getenv("SHARED_DERIVED_FILE_DIR"), "JavaScriptCore/InlineRuntimeSymbolTable.h")

tested_symbols = Set([])

if os.path.isfile(tested_symbols_location):
    with open(tested_symbols_location, 'r') as file:
        print("Loading tested symbols")
        for line in file:
            tested_symbols.add(line[:-1])

print ("Original directory: " + bitcode_file_original_directory)

for bitcode_file in glob.iglob(os.path.join(framework_directory, "*." + file_suffix)):
    bitcode_basename = os.path.basename(bitcode_file)
    bitcode_file_original = os.path.join(bitcode_file_original_directory, bitcode_basename[:-file_suffix_length] + "o")
    if os.path.getmtime(bitcode_file_original) < symbol_table_modification_time:
        continue

    symbol_table_is_out_of_date = True

    print("Appending symbols from " + bitcode_basename)
    lines = subprocess.check_output(["nm", "-U", "-j", bitcode_file]).splitlines()

    for symbol in lines:
        if symbol[:2] == "__" and symbol[-3:] != ".eh" and symbol in tested_symbols:
            symbol_table[symbol[1:]] = bitcode_basename

if not symbol_table_is_out_of_date:
    sys.exit()

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
