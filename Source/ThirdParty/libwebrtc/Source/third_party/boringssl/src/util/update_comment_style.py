# Copyright 2026 The BoringSSL Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import re
import argparse
import sys
import os


def update_comment_style(cpp_code):
    # Identifies C/C++ line and block comments.
    comment_regex = re.compile(r'//.*|/\*[\s\S]*?\*/')
    # Matches |pipes| surrounding a symbol or function call, or expression
    # containing two such symbols joined by -> or + operators.
    target_regex = re.compile(r'(?<!\|)\|(\*?[a-zA-Z_][a-zA-Z0-9_\.\-\*()]*((\->|\+)[a-zA-Z_][a-zA-Z0-9_\.\-\*()]*)?)\|(?!\|)')

    total_replacements = 0

    def process_comment(comment_match):
        nonlocal total_replacements
        comment_text = comment_match.group(0)
        modified_comment, subs_made = target_regex.subn(r'`\1`', comment_text)
        total_replacements += subs_made
        return modified_comment

    updated_code = comment_regex.sub(process_comment, cpp_code)

    unreplaced_lines = []
    for line_num, line in enumerate(updated_code.splitlines(), start=1):
        if '|' in line:
            unreplaced_lines.append((line_num, line))

    return updated_code, total_replacements, unreplaced_lines


def main():
    parser = argparse.ArgumentParser(description="Update pipe formatting to backticks within C/C++ comments.")
    parser.add_argument("filepath", help="Path to the C/C++ file to modify.")

    args = parser.parse_args()
    file_path = args.filepath

    if not os.path.isfile(file_path):
        print(f"Error: '{file_path}' not found.")
        sys.exit(1)

    try:
        with open(file_path, 'r', encoding='utf-8') as file:
            code_file = file.read()
    except Exception as e:
        print(f"Error reading file '{file_path}': {e}")
        sys.exit(1)

    new_code, replace_count, leftover_pipes = update_comment_style(code_file)

    try:
        with open(file_path, 'w', encoding='utf-8') as file:
            file.write(new_code)
    except Exception as e:
        print(f"Error writing to file '{file_path}': {e}")
        sys.exit(1)

    print("-" * 40)
    print(f"--- SUMMARY FOR: {file_path} ---")
    print(f"Total replacements made: {replace_count}")

    if leftover_pipes:
        print(f"\nFound {len(leftover_pipes)} line(s) containing unreplaced pipe '|' characters:")
        for line_num, text in leftover_pipes:
            print(f"  Line {line_num:02d}: {text.strip()}")
    else:
        print("\nNo unreplaced pipe characters found in the resulting file.")

if __name__ == "__main__":
    main()
