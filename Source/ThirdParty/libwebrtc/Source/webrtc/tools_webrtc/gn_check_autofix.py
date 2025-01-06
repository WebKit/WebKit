#!/usr/bin/env vpython3

# Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.
"""
This tool tries to fix (some) errors reported by `gn gen --check` or
`gn check`.
If a command line flag `-C out/<dir>` is supplied, it will run `gn gen --check`
in that directory. Otherwise it will run `mb gen` in a temporary directory
which is useful to check for different configurations.

Usage:
    $ vpython3 tools_webrtc/gn_check_autofix.py -C out/Default
    or
    $ vpython3 tools_webrtc/gn_check_autofix.py -m some_mater -b some_bot
    or
    $ vpython3 tools_webrtc/gn_check_autofix.py -c some_mb_config
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile

from collections import defaultdict

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

CHROMIUM_DIRS = [
    'base', 'build', 'buildtools', 'testing', 'third_party', 'tools'
]

TARGET_RE = re.compile(
    r'(?P<indentation_level>\s*)\w*\("(?P<target_name>\w*)"\) {$')


class TemporaryDirectory:

    def __init__(self):
        self._closed = False
        self._name = None
        self._name = tempfile.mkdtemp()

    def __enter__(self):
        return self._name

    def __exit__(self, exc, value, _tb):
        if self._name and not self._closed:
            shutil.rmtree(self._name)
            self._closed = True


def Run(cmd):
    print('Running:', ' '.join(cmd))
    sub = subprocess.Popen(cmd,
                           stdout=subprocess.PIPE,
                           stderr=subprocess.PIPE,
                           universal_newlines=True)
    return sub.communicate()


def fix_errors(filename, missing_deps, deleted_sources):
    with open(filename) as file:
        lines = file.readlines()

    fixed_file = ''
    indentation_level = None
    for line in lines:
        match = TARGET_RE.match(line)
        if match:
            target = match.group('target_name')
            if target in missing_deps:
                indentation_level = match.group('indentation_level')
        elif indentation_level is not None:
            match = re.match(indentation_level + '}$', line)
            if match:
                line = ('deps = [\n' + ''.join('  "' + dep + '",\n'
                                               for dep in missing_deps[target])
                        + ']\n') + line
                indentation_level = None
            elif line.strip().startswith('deps = ['):
                joined_deps = ''.join('  "' + dep + '",\n'
                                      for dep in missing_deps[target])
                line = line.replace('deps = [', 'deps = [' + joined_deps)
                indentation_level = None

        if line.strip() not in deleted_sources:
            fixed_file += line

    with open(filename, 'w') as file:
        file.write(fixed_file)

    Run(['gn', 'format', filename])


def first_non_empty(iterable):
    """Return first item which evaluates to True, or fallback to None."""
    return next((x for x in iterable if x), None)


def rebase(base_path, dependency_path, dependency):
    """Adapt paths so they work both in stand-alone WebRTC and Chromium tree.

  To cope with varying top-level directory (WebRTC VS Chromium), we use:
    * relative paths for WebRTC modules.
    * absolute paths for shared ones.
  E.g. '//common_audio/...' -> '../../common_audio/'
       '//third_party/...' remains as is.

  Args:
    base_path: current module path  (E.g. '//video')
    dependency_path: path from root (E.g. '//rtc_base/time')
    dependency: target itself       (E.g. 'timestamp_extrapolator')

  Returns:
    Full target path (E.g. '../rtc_base/time:timestamp_extrapolator').
  """

    root = first_non_empty(dependency_path.split('/'))
    if root in CHROMIUM_DIRS:
        # Chromium paths must remain absolute. E.g.
        # //third_party//abseil-cpp...
        rebased = dependency_path
    else:
        base_path = base_path.split(os.path.sep)
        dependency_path = dependency_path.split(os.path.sep)

        first_difference = None
        shortest_length = min(len(dependency_path), len(base_path))
        for i in range(shortest_length):
            if dependency_path[i] != base_path[i]:
                first_difference = i
                break

        first_difference = first_difference or shortest_length
        base_path = base_path[first_difference:]
        dependency_path = dependency_path[first_difference:]
        rebased = os.path.sep.join((['..'] * len(base_path)) + dependency_path)
    return rebased + ':' + dependency


def main():
    helptext = """
This tool tries to fix (some) errors reported by `gn gen --check`.

If a command line flag `-C out/<dir>` is supplied, it will run `gn gen --check`
in that directory. Otherwise it will run `mb gen` in a temporary directory
with all other command line arguments forwarded to `mb gen`. This mode is
useful to check for different configurations."""

    parser = argparse.ArgumentParser(
        description=helptext,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('-C',
                        dest='local_build_dir',
                        help='Path lo a local build dir, e.g. out/Default')
    (flags, argv_to_forward) = parser.parse_known_args(sys.argv[1:])

    deleted_sources = set()
    errors_by_file = defaultdict(lambda: defaultdict(set))

    if flags.local_build_dir:
        mb_output = Run(["gn", "gen", "--check", flags.local_build_dir])
    else:
        with TemporaryDirectory() as tmp_dir:
            mb_script_path = os.path.join(SCRIPT_DIR, 'mb', 'mb.py')
            mb_config_file_path = os.path.join(SCRIPT_DIR, 'mb',
                                               'mb_config.pyl')
            mb_gen_command = ([
                mb_script_path,
                'gen',
                tmp_dir,
                '--config-file',
                mb_config_file_path,
            ] + argv_to_forward)
        mb_output = Run(mb_gen_command)

    errors = mb_output[0].split('ERROR')[1:]

    if mb_output[1]:
        print(mb_output[1])
        return 1

    for error in errors:
        error = error.split('\n')
        target_msg = 'The target:'
        if target_msg not in error:
            target_msg = 'It is not in any dependency of'
        if target_msg not in error:
            print('\n'.join(error))
            continue
        index = error.index(target_msg) + 1
        path, target = error[index].strip().split(':')
        if error[index + 1] in ('is including a file from the target:',
                                'The include file is in the target(s):'):
            dep = error[index + 2].strip()
            dep_path, dep = dep.split(':')
            dep = rebase(path, dep_path, dep)
            # Replacing /target:target with /target
            dep = re.sub(r'/(\w+):(\1)$', r'/\1', dep)
            # Replacing target:target with target
            dep = re.sub(r'^(\w+):(\1)$', r'\1', dep)
            path = os.path.join(path[2:], 'BUILD.gn')
            errors_by_file[path][target].add(dep)
        elif error[index + 1] == 'has a source file:':
            deleted_file = '"' + os.path.basename(
                error[index + 2].strip()) + '",'
            deleted_sources.add(deleted_file)
        else:
            print('\n'.join(error))
            continue

    for path, missing_deps in list(errors_by_file.items()):
        fix_errors(path, missing_deps, deleted_sources)

    return 0


if __name__ == '__main__':
    sys.exit(main())
