#!/usr/bin/env python3
import re
import sys

from argparse import ArgumentParser

# This is a very dumb regex based method of translating from
# gn variable declarations and simple if statements to the
# equivalent cmake code. It only supports a few constructs and
# assumes the code is formatted a certain way. ANGLE gn files
# are auto-formatted so this should work most of the time.

# The output may need a bit of manual fixup, but it beats
# translating the whole thing by hand. If new constructs
# are added to the ANGLE gn files, hopefully we can just add
# a few extra regexes here.

parser = ArgumentParser(
    prog='gni-to-cmake', description='converts a .gni file to .cmake', usage='%(prog)s [options]')
parser.add_argument('gni', help='the .gni file to parse')
parser.add_argument('cmake', help='the .cmake file to output')
parser.add_argument('--prepend', default='', help='the path to prepend to each file name')

args = parser.parse_args()

with open(args.gni, 'r', encoding="utf-8") as gni:
    file = gni.read()

    # Prepend path to every string.
    file = re.sub(r'"((\\\"|[^"])+)"', '"' + args.prepend + r'\1"', file)

    # Remove import, assert, config, and angle_source_set statements.
    file = re.sub(
        r'^\s*(import|assert|config|angle_source_set)\([^)]+\)( {.*})?$',
        r'',
        file,
        flags=re.MULTILINE | re.DOTALL)

    # Translate gn single line list declaration:
    file = re.sub(r'\[ ((?:"[^"]*",? )*)\]$', r' \1)', file, flags=re.MULTILINE)

    # Translate gn single line list append:
    file = re.sub(
        r'^(\s*)(\w+) \+= (\w+)$', r'\1 set(\2, "${\2};${\3}")', file, flags=re.MULTILINE)

    # Translate gn list declaration:
    # variable_name = [
    #   "file/name/foo.cpp",
    # ]
    # to cmake list declaration:
    # set(variable_name
    #     "file/name/foo.cpp"
    # )
    file = re.sub(r'^(\s*)(\w+) = ?\[?', r'\1\1set(\2', file, flags=re.MULTILINE)
    file = re.sub(r'^(\s*)("[^"]+"),$', r'\1\1\2', file, flags=re.MULTILINE)
    file = re.sub(r'^(\s*)]$', r'\1\1)', file, flags=re.MULTILINE)

    # Translate list append fom gn to cmake
    file = re.sub(r'^(\s*)(\w+) \+= ?\[?', r'\1\1list(APPEND \2', file, flags=re.MULTILINE)

    # Translate if statements from gn to cmake
    file = re.sub(r'^(\s*)((?:} else )?)if \((.+)\) {$', r'\1\1\2if(\3)', file, flags=re.MULTILINE)
    file = re.sub(r'^} else if$', r'elseif', file, flags=re.MULTILINE)
    file = re.sub(r'^(\s*)} else {$', r'\1\1else()', file, flags=re.MULTILINE)
    file = re.sub(r'^(\s*)}$', r'\1\1endif()', file, flags=re.MULTILINE)

    # Translate logic ops from gn to cmake
    file = re.sub(r' \|\| ', r' OR ', file, flags=re.MULTILINE)
    file = re.sub(r' \&\& ', r' AND ', file, flags=re.MULTILINE)
    file = re.sub(r' == ', r' STREQUAL ', file, flags=re.MULTILINE)
    file = re.sub(r'!', r' NOT ', file, flags=re.MULTILINE)

    with open(args.cmake, 'w', encoding="utf-8") as cmake:
        cmake.write('# This file was generated with the command:\n')
        cmake.write('# ' + ' '.join(['"' + arg.replace('"', '\\"') + '"' for arg in sys.argv]))
        cmake.write('\n\n')
        cmake.write(file)
