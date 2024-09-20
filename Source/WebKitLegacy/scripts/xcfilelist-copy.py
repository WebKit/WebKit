#!/usr/bin/env python3
import argparse
import json
import os
import shutil
import subprocess
import sys
from fnmatch import fnmatch
from pathlib import Path

postprocess_rule_env = os.environ.copy()
postprocess_rule_env["SCRIPT_OUTPUT_FILE_COUNT"] = "1"
postprocess_rule_env["SCRIPT_OUTPUT_FILE_LIST_COUNT"] = "0"

excluded_patterns = list(map(Path, os.environ.get("EXCLUDED_SOURCE_FILE_NAMES", "").split()))
included_patterns = list(map(Path, os.environ.get("INCLUDED_SOURCE_FILE_NAMES", "").split()))

parser = argparse.ArgumentParser()
parser.add_argument('--timestamp')
parser.add_argument('--tapi-filelist')
parser.add_argument('--rule', dest='migrate_rule')

opts = parser.parse_args()
tapi_filelist = {"headers": [], "version": "3"}


def pattern_match(path, pattern):
    # Using the number of path components in `pattern`, only match that many
    # trailing components in `path`. This is an imitation of Xcode's path
    # filtering behavior.
    components_to_match = zip(reversed(path.parts), reversed(pattern.parts))
    return all(fnmatch(path_component, pattern_component) for path_component, pattern_component in components_to_match)


for file_number in range(int(os.environ["SCRIPT_OUTPUT_FILE_LIST_COUNT"])):
    input_file_list = os.environ[f"SCRIPT_INPUT_FILE_LIST_{file_number}"]
    output_file_list = os.environ[f"SCRIPT_OUTPUT_FILE_LIST_{file_number}"]
    if not input_file_list and not output_file_list:
        # Empty string indicates a filelist which is present in other build
        # configurations only.
        continue
    for input_file, output_file in zip(open(input_file_list), open(output_file_list)):
        input_file = Path(input_file.rstrip())
        output_file = Path(output_file.rstrip())

        if input_file.name != output_file.name:
            sys.exit(f"error: Trying to copy {input_file} to {output_file}, but the file names don't match. "
                     "Files should always have the same name when copied. "
                     "Ensure that the paths in this script phase's input and output xcfilelists match.")

        if any(pattern_match(input_file, pattern) for pattern in excluded_patterns) \
                and not any(pattern_match(input_file, pattern) for pattern in included_patterns):
            continue

        visibility = None
        if "PrivateHeaders" in output_file.parts:
            visibility = "private"
        elif "Headers" in output_file.parts:
            visibility = "public"
        if visibility:
            tapi_filelist["headers"].append({
                "path": str(output_file),
                "type": visibility
            })

        if opts.migrate_rule:
            postprocess_rule_env["INPUT_FILE_PATH"] = input_file
            postprocess_rule_env["INPUT_FILE_NAME"] = input_file.name
            postprocess_rule_env["SCRIPT_OUTPUT_FILE_0"] = output_file
            if visibility:
                postprocess_rule_env["SCRIPT_HEADER_VISIBILITY"] = visibility
            print("RuleScriptExecution", output_file, input_file, file=sys.stderr)
            subprocess.check_call((opts.migrate_rule), env=postprocess_rule_env)
        else:
            print(output_file, "->", input_file, file=sys.stderr)
            shutil.copy(input_file, output_file)

if opts.tapi_filelist:
    with open(opts.tapi_filelist, "w") as fd:
        json.dump(tapi_filelist, fd, indent=2)

if opts.timestamp:
    Path(opts.timestamp).touch()
