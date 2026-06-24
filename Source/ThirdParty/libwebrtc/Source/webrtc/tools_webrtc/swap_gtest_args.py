#  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.
"""Swaps gtest macro arguments with "Yoda testing"

"Yoda testing" is where the constant comes first, and the value being
tested comes second. This script will detect some cases of that, and
swap the arguments.

It depends on detecting constants, which is a heuristic, and it only handles
order-independent cases (ASSERT_EQ), not comparators like ASSERT_LT.
"""

import re
import sys


def is_constant(arg):
    arg = arg.strip()
    # Casts
    if (arg.startswith('static_cast<') or arg.startswith('reinterpret_cast<')):
        match = re.search(r'cast<.*?>\s*\((.*)\)', arg, re.DOTALL)
        if match:
            return is_constant(match.group(1))
    # Casts by brace initialization
    match = re.match(r'^\w+\{\s*(.\w*)\}$', arg)
    if match:
        return is_constant(match.group(1))

    # Numeric literals
    if re.match(r'^-?\d+[Uu]?[Ll]{0,2}$', arg):
        return True
    if re.match(r'^0x[0-9a-fA-F]+[Uu]?$', arg):
        return True
    if re.match(r'^-?\d+\.\d+[fF]?$', arg):
        return True

    # String literals
    if arg.startswith('"') and arg.endswith('"'):
        return True

    # Boolean literals
    if arg in ['true', 'false']:
        return True

    # Nulls
    if arg in ['nullptr', 'NULL', 'std::nullopt']:
        return True

    # k-prefixed constants
    if re.match(r'^k[A-Z]\w*$', arg):
        return True

    # ALL_CAPS
    if re.match(r'^[A-Z][A-Z0-9_]*$', arg):
        return True

    # Qualified enums
    if '::' in arg:
        parts = arg.split('::')
        last_part = parts[-1].strip()
        if (re.match(r'^[A-Z][A-Z0-9_]*$', last_part)
                or re.match(r'^k[A-Z][a-zA-Z0-9]*$', last_part)):
            return True

    return False


def split_args(args_str):
    args = []
    current = []
    depth = 0
    in_string = False
    i = 0
    while i < len(args_str):
        char = args_str[i]
        if char == '"' and (i == 0 or args_str[i - 1] != '\\'):
            in_string = not in_string

        if not in_string:
            if char in '({[<':
                depth += 1
            elif char in ')}]>':
                depth -= 1
            elif char == ',' and depth == 0:
                args.append(''.join(current))
                current = []
                i += 1
                continue
        current.append(char)
        i += 1
    args.append(''.join(current))
    return args


def find_matching_paren(content, start_idx):
    depth = 0
    in_string = False
    for i in range(start_idx, len(content)):
        char = content[i]
        if char == '"' and (i == 0 or content[i - 1] != '\\'):
            in_string = not in_string
        if not in_string:
            if char == '(':
                depth += 1
            elif char == ')':
                depth -= 1
                if depth == 0:
                    return i
    return -1


def find_macro_calls(content):
    macro_names = ['EXPECT_EQ', 'EXPECT_NE', 'ASSERT_EQ', 'ASSERT_NE']
    results = []
    for name in macro_names:
        start_pos = 0
        while True:
            idx = content.find(name + '(', start_pos)
            if idx == -1:
                break

            end_idx = find_matching_paren(content, idx + len(name))

            if end_idx != -1:
                args_content = content[idx + len(name) + 1:end_idx]
                results.append((idx, end_idx + 1, name, args_content))
                start_pos = end_idx + 1
            else:
                start_pos = idx + len(name)
    return sorted(results, key=lambda x: x[0], reverse=True)


def process_content(content):
    calls = find_macro_calls(content)

    new_content = content
    for start, end, macro, args_str in calls:
        args = split_args(args_str)
        if len(args) != 2:
            continue

        arg1 = args[0]
        arg2 = args[1]

        if is_constant(arg1) and not is_constant(arg2):
            new_call = f"{macro}({arg2.strip()}, {arg1.strip()})"
            new_content = new_content[:start] + new_call + new_content[end:]
    return new_content


def process_file(file_path):
    with open(file_path, 'r') as f_in:
        content = f_in.read()

    new_content = process_content(content)

    with open(file_path, 'w') as f_out:
        f_out.write(new_content)


if __name__ == "__main__":
    process_file(sys.argv[1])
