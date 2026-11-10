#!/usr/bin/env python3
#
# Copyright (C) 2025 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import argparse
import os
import re


def get_primary_header_name(filename):
    if not any(filename.endswith(ext) for ext in ['.cpp', '.m', '.mm']):
        return None
    return os.path.basename(os.path.splitext(filename)[0] + ".h")


# block is a section of includes without a space or #if.
def sort_include_block(block, primary_header_name):
    soft_links = []
    project_includes = []
    system_includes = []

    for line in block:
        # FIXME: Handle the config block.
        if '"config.h"' in line:
            return block
        elif 'SoftLink.h' in line:
            soft_links.append(line)
        elif re.search(r'#\s*(include|import)\s*"', line):
            project_includes.append(line)
        else:
            system_includes.append(line)

    soft_links.sort()
    project_includes.sort()
    system_includes.sort()

    sorted_block = []
    sorted_block.extend(project_includes)
    sorted_block.extend(system_includes)
    sorted_block.extend(soft_links)
    return sorted_block


def sort_includes_in_file(filename):
    try:
        with open(filename, 'r') as f:
            lines = f.readlines()
    except IOError:
        print(f"Error: Could not open file {filename}")
        return

    primary_header_name = get_primary_header_name(filename)
    new_lines = []
    i = 0
    while i < len(lines):
        line = lines[i]
        if not re.match(r'^\s*[@#]\s*(include|import)\s', line):
            new_lines.append(line)
            i += 1
            continue

        # Found the start of an include block
        start_index = i
        current_block = []
        while i < len(lines) and re.match(r'^\s*([@#]\s*(include|import)|#\s*(if|else|elif|endif)).*', lines[i]):
            current_block.append(lines[i])
            i += 1

        # Split block by #if/#else/#endif
        sub_blocks = []
        current_sub_block = []
        for line in current_block:
            if re.match(r'^\s*#\s*(if|else|elif|endif)', line):
                if current_sub_block:
                    sub_blocks.append(current_sub_block)
                sub_blocks.append([line])
                current_sub_block = []
            else:
                current_sub_block.append(line)
        if current_sub_block:
            sub_blocks.append(current_sub_block)

        for sub_block in sub_blocks:
            if sub_block and re.match(r'^\s*[@#]\s*(include|import)\s', sub_block[0]):
                new_lines.extend(sort_include_block(sub_block, primary_header_name))
            else:
                new_lines.extend(sub_block)

    with open(filename, 'w') as f:
        f.writelines(new_lines)
    print(f"Sorted includes in {filename}")


def main():
    parser = argparse.ArgumentParser(description='Sort C/Obj-C family include statements in WebKit files.')
    parser.add_argument('files', nargs='+', help='A list of C/Obj-C family files to process.')
    args = parser.parse_args()

    for f in args.files:
        sort_includes_in_file(f)


if __name__ == '__main__':
    main()
