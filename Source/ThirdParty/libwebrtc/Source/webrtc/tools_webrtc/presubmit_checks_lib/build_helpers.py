#!/usr/bin/env vpython3

# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.
"""This script helps to invoke gn and ninja which lie in depot_tools
repository."""

import json
import os
import re
import shutil
import subprocess
import sys
import tempfile


def find_root_path():
    """Returns the absolute path to the highest level repo root.

    If this repo is checked out as a submodule of the chromium/src
    superproject, this returns the superproect root. Otherwise, it returns the
    webrtc/src repo root.
    """
    root_dir = os.path.dirname(os.path.abspath(__file__))
    while os.path.basename(root_dir) not in ('src', 'chromium'):
        par_dir = os.path.normpath(os.path.join(root_dir, os.pardir))
        if par_dir == root_dir:
            raise RuntimeError('Could not find the repo root.')
        root_dir = par_dir
    return root_dir


ROOT_DIR = find_root_path()
sys.path.append(os.path.join(ROOT_DIR, 'build'))
import find_depot_tools


def run_gn_command(args, root_dir=None):
    """Runs `gn` with provided args and return error if any."""
    try:
        command = [
            sys.executable,
            os.path.join(find_depot_tools.DEPOT_TOOLS_PATH, 'gn.py')
        ] + args
        subprocess.check_output(command, cwd=root_dir)
    except subprocess.CalledProcessError as err:
        return err.output
    return None


# GN_ERROR_RE matches the summary of an error output by `gn check`.
# Matches "ERROR" and following lines until it sees an empty line or a line
# containing just underscores.
GN_ERROR_RE = re.compile(r'^ERROR .+(?:\n.*[^_\n].*$)+', re.MULTILINE)


def run_gn_check(root_dir=None):
    """Runs `gn gen --check` with default args to detect mismatches between
  #includes and dependencies in the BUILD.gn files, as well as general build
  errors.

  Returns a list of error summary strings.
  """
    out_dir = tempfile.mkdtemp('gn')
    try:
        error = run_gn_command(['gen', '--check', out_dir], root_dir)
    finally:
        shutil.rmtree(out_dir, ignore_errors=True)
    return GN_ERROR_RE.findall(error.decode('utf-8')) if error else []


def run_ninja_command(args, root_dir=None):
    """Runs ninja quietly. Any failure (e.g. clang not found) is
     silently discarded, since this is unlikely an error in submitted CL."""
    command = [os.path.join(ROOT_DIR, 'third_party', 'ninja', 'ninja')] + args
    proc = subprocess.Popen(command,
                            cwd=root_dir,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE)
    out, _ = proc.communicate()
    return out


def get_clang_tidy_path():
    """POC/WIP! Use the one we have, even it doesn't match clang's version."""
    tidy = ('third_party/android_toolchain/toolchains/'
            'llvm/prebuilt/linux-x86_64/bin/clang-tidy')
    return os.path.join(ROOT_DIR, tidy)


def get_compilation_db(root_dir=None):
    """Run ninja compdb tool to get proper flags, defines and include paths."""
    # The compdb tool expect a rule.
    commands = json.loads(run_ninja_command(['-t', 'compdb', 'cxx'], root_dir))
    # Turns 'file' field into a key.
    return {v['file']: v for v in commands}


def get_compilation_command(filepath, gn_args, work_dir):
    """Get the whole command used to compile one cc file.
  Typically, clang++ with flags, defines and include paths.

  Args:
      filepath: path to .cc file.
      gen_args: build configuration for gn.
      work_dir: build dir.

  Returns:
    Command as a list, ready to be consumed by subprocess.Popen.
  """
    gn_errors = run_gn_command(['gen'] + gn_args + [work_dir])
    if gn_errors:
        raise RuntimeError('FYI, cannot complete check due to gn error:\n%s\n'
                           'Please open a bug.' % gn_errors)

    # Needed for single file compilation.
    commands = get_compilation_db(work_dir)

    # Path as referenced by ninja.
    rel_path = os.path.relpath(os.path.abspath(filepath), work_dir)

    # Gather defines, include path and flags (such as -std=c++11).
    try:
        compilation_entry = commands[rel_path]
    except KeyError as not_found:
        raise ValueError('%s: Not found in compilation database.\n'
                         'Please check the path.' % filepath) from not_found
    command = compilation_entry['command'].split()

    # Remove troublesome flags. May trigger an error otherwise.
    if '-MMD' in command:
        command.remove('-MMD')
    if '-MF' in command:
        index = command.index('-MF')
        del command[index:index + 2]  # Remove filename as well.

    return command
