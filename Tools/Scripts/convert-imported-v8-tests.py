#!/usr/bin/env python3

# Copyright (C) 2022 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# This tool aids with the importing of V8 test files into WebKit.
# It performs most of the common substitutions.

import argparse
import os
import re
import sys
import types
from pathlib import Path

FILE_MODE = "File"
LIST_MODE = "List"

RULE_REGEXP_IGNORE = 1
RULE_REGEXP_MANUAL_EDIT = 2
RULE_STRING_REPLACE = 3
RULE_REGEXP_REPLACE = 4


log_file = sys.stderr
files_converted = 0
header = ""

files_loaded_notes = None
files_needing_manual_changes = None


def print_log(*a):
    print(*a, file=log_file)


def print_stderr(*a):
    print(*a, file=sys.stderr)


def handle_loadfile_replacements(matchobj):
    global files_loaded_notes

    originalPath = matchobj.group(1)
    fileToLoad = matchobj.group(2)
    files_loaded_notes.add_note(fileToLoad, originalPath)
    return "load(\"{0}\");".format(fileToLoad)


def handle_print_with_parens(matchobj):
    possible_func_call = matchobj.group(3)
    if possible_func_call:
        print("found possible func call \"{0}\" in: {1}".format(possible_func_call, matchobj.group(0)))
    else:
        return "{white_space}// {print_stmnt}".format(white_space=matchobj.group(1), print_stmnt=matchobj.group(2))


def handle_percent_call(matchobj):
    return "call to " + matchobj.group(1) + ")"


rules = [
    (RULE_REGEXP_IGNORE, r'^\s*\/\/'),
    (RULE_REGEXP_REPLACE, r'd8\.file\.execute\s*\("((?:[^\/]+\/)*([^"]+))"\)(?:;)?', handle_loadfile_replacements),
    (RULE_REGEXP_REPLACE, r'd8\.file\.execute\s*\(\'((?:[^\/]+\/)*([^\']+))\'\)(?:;)?', handle_loadfile_replacements),
    (RULE_REGEXP_REPLACE, r'^(\s*)(print\([^\(]+\);?)', r'\1// \2'),
    (RULE_REGEXP_REPLACE, r'^(\s*)(print\(\'[^\']+\'\);?)', r'\1// \2'),
    (RULE_REGEXP_REPLACE, r'^(\s*)(print\(\"[^\"]+\"\);?)', r'\1// \2'),
    (RULE_REGEXP_REPLACE, r'^(\s*)(print\(.*([_a-zA-Z][_0-9a-zA-Z]\()?.*\);?)', handle_print_with_parens),
    (RULE_REGEXP_MANUAL_EDIT, r'^\s*print\(', "call to print()"),
    (RULE_REGEXP_MANUAL_EDIT, r'console.info', "call to console.info()"),
    (RULE_REGEXP_MANUAL_EDIT, r'd8\.debugger\.enable\(\)', "call to db.debugger.enable()"),
    (RULE_REGEXP_MANUAL_EDIT, r'd8\.profiler\.', "call to db.profiler.*()"),
    (RULE_REGEXP_MANUAL_EDIT, r'(%[_a-zA-Z][_0-9a-zA-Z]*\()', handle_percent_call)]


def parse_args():
    global header

    parser = argparse.ArgumentParser(description='Process V8 test files for inclusion in WebKit.',
                                     usage='%(prog)s [-f] [-v] [-h <header-string>] (-s <source-file> -d <dest-file> | -l <file-with-list-of-source-files> [-o <output-dir>] [-S <source-dir>] )')
    parser.add_argument('-s', '--source', default=None, type=Path, dest='source_file_name', required=False,
                        help='source file to convert')
    parser.add_argument('-d', '--dest', default=None, type=Path, dest='dest_file_name', required=False,
                        help='destination file from convert')
    parser.add_argument('-H', '--header', default=None, dest='header_string', required=False,
                        help='String to add to the top of converted files')
    parser.add_argument('-l', '--file-list', default=None, type=Path, dest='file_list_file_name', required=False,
                        help='file containing list of files to convert')
    parser.add_argument('-S', '--source-dir', default=None, type=Path, dest='source_dir', required=False,
                        help='source directory for files in file list')
    parser.add_argument('-o', '--output-dir', default='.', type=Path, dest='output_dir', required=False,
                        help='output directory (defaults to working directory)')
    parser.add_argument('-f', '--force', action='store_true', required=False, help='overwrite existing destination files')
    parser.add_argument('-v', '--verbose', action='store_true', required=False, help='verbose output')

    args = parser.parse_args()

    argument_errors = 0

    if args.source_file_name is not None and args.dest_file_name is None:
        argument_errors += 1
        print_stderr("Use of -s option requires the -d option")

    if args.source_file_name is None and args.dest_file_name is not None:
        argument_errors += 1
        print_stderr("Use of -d option requires the -s option")

    if args.source_file_name is not None and args.file_list_file_name is not None:
        argument_errors += 1
        print_stderr("Either use -s and -d options or -l and -o option pairs")

    if argument_errors:
        parser.print_help()
        exit(1)

    if args.header_string:
        header = args.header_string + "\n"

    if args.source_file_name:
        args.mode = FILE_MODE
    elif args.file_list_file_name:
        args.mode = LIST_MODE

    return args


class ItemNotes(object):
    def __init__(self, title, item_heading, note_heading):
        self.notes_by_item = {}
        self.note_title = title
        self.item_heading = item_heading
        self.note_heading = note_heading
        self.max_item_length = 0
        self.max_note_length = 0

    def add_note(self, item, note):
        if item not in self.notes_by_item:
            self.notes_by_item[item] = []
            if len(item) > self.max_item_length:
                self.max_item_length = len(item)

        if note not in self.notes_by_item[item]:
            self.notes_by_item[item].append(note)
            if len(note) > self.max_note_length:
                self.max_note_length = len(note)

    def print_notes(self):
        if not len(self.notes_by_item):
            return

        item_width = round((self.max_item_length + 2) / 5) * 5
        item_divider = ""
        item_spacer = ""

        for i in range(item_width):
            item_divider += '-'
            item_spacer += ' '

        note_divider = ""

        for i in range(self.max_note_length):
            note_divider += '-'

        print(self.note_title)
        print("{item_heading:<{item_width}}   {note_heading}".format(item_width=item_width, item_heading=self.item_heading, note_heading=self.note_heading))
        print("{item_divider}   {note_divider}".format(item_divider=item_divider, note_divider=note_divider))

        for k, v in sorted(self.notes_by_item.items()):
            print("{item:<{width}}   ".format(width=item_width, item=k), end="")
            note_count = 0
            for note in v:
                if note_count:
                    print("{spacer}   ".format(spacer=item_spacer), end="")
                print("{note}".format(note=note))
                note_count += 1
        print()


def convert_file(source_file_name, dest_file_name, force_overwrite=False):
    global files_converted
    global files_needing_manual_changes

    files_converted += 1
    line_number = 0
    dest_open_mode = 'w' if force_overwrite else 'x'

    try:
        buffer = ""
        manual_issues = set()
        with open(source_file_name, 'r') as src_file:
            for line in src_file:
                line_number += 1
                original_line = line

                for rule in rules:
                    type = rule[0]

                    if type == RULE_REGEXP_IGNORE:
                        if re.search(rule[1], line):
                            break
                    elif type == RULE_REGEXP_MANUAL_EDIT:
                        matchobj = re.search(rule[1], line)
                        if matchobj:
                            reason = None
                            if isinstance(rule[2], str):
                                reason = rule[2]
                            if isinstance(rule[2], types.FunctionType):
                                reason = rule[2](matchobj)
                            files_needing_manual_changes.add_note(str(source_file_name.name),
                                                                  "Line {line_number:>4}, Found {reason}".format(line_number=line_number, reason=reason))
                            manual_issues.add("// {reason}\n".format(reason=reason))
                            break
                    elif type == RULE_STRING_REPLACE:
                        line = line.replace(rule[1], rule[2])
                    elif type == RULE_REGEXP_REPLACE:
                        line = re.sub(rule[1], rule[2], line)

                    if line != original_line:
                        break

                buffer += line

        with open(dest_file_name, dest_open_mode) as dest_file:
            if len(header):
                dest_file.write(header)

            if len(manual_issues):
                dest_file.write("//@ skip\n")
                dest_file.write("// Skipping this test due to the following issues:\n")
                for issue in sorted(manual_issues):
                    dest_file.write(issue)
                dest_file.write("\n")

            dest_file.write(buffer)

    except IOError as e:
        print_stderr("Error: {0}".format(e))
        return False

    return True


def main():
    global files_loaded_notes
    global files_needing_manual_changes

    args = parse_args()

    force_overwrite = args.force

    files_loaded_notes = ItemNotes("Processed List of Loaded Files:", "File Name", "Original Path(s) Found")
    files_needing_manual_changes = ItemNotes("Converted Files Needing Manual Changes", "File Name", "Reason")

    if args.mode == FILE_MODE:
        convert_file(args.source_file_name, args.dest_file_name, force_overwrite)
    elif args.mode == LIST_MODE:
        src_root_dir = args.source_dir
        dest_root_dir = args.output_dir
        try:
            with open(args.file_list_file_name, 'r') as file_list:
                for file in file_list:
                    file = Path(file.strip())
                    name = file.name

                    src_file = src_root_dir / file if src_root_dir else file

                    if not os.path.exists(src_file):
                        print_stderr("Cannot open \"{0}\", skipping".format(src_file))
                    dest_file = dest_root_dir / name if dest_root_dir else name
                    convert_file(src_file, dest_file, force_overwrite)
        except IOError as e:
            print_stderr("Error: {0}".format(e))
            return False

    if args.verbose:
        print("Processed {count} files".format(count=files_converted))
    files_loaded_notes.print_notes()
    files_needing_manual_changes.print_notes()


if __name__ == "__main__":
    main()
