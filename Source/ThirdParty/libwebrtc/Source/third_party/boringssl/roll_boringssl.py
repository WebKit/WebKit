#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Rolls third_party/boringssl/src in DEPS and updates generated build files."""

import os
import os.path
import shutil
import subprocess
import sys


SCRIPT_PATH = os.path.abspath(__file__)
SRC_PATH = os.path.dirname(os.path.dirname(os.path.dirname(SCRIPT_PATH)))
DEPS_PATH = os.path.join(SRC_PATH, 'DEPS')
BORINGSSL_PATH = os.path.join(SRC_PATH, 'third_party', 'boringssl')
BORINGSSL_SRC_PATH = os.path.join(BORINGSSL_PATH, 'src')

if not os.path.isfile(DEPS_PATH) or not os.path.isdir(BORINGSSL_SRC_PATH):
  raise Exception('Could not find Chromium checkout')

# Pull OS_ARCH_COMBOS out of the BoringSSL script.
sys.path.append(os.path.join(BORINGSSL_SRC_PATH, 'util'))
import generate_build_files

GENERATED_FILES = [
    'BUILD.generated.gni',
    'BUILD.generated_tests.gni',
    'err_data.c',
]


def IsPristine(repo):
  """Returns True if a git checkout is pristine."""
  cmd = ['git', 'diff', '--ignore-submodules']
  return not (subprocess.check_output(cmd, cwd=repo).strip() or
              subprocess.check_output(cmd + ['--cached'], cwd=repo).strip())


def RevParse(repo, rev):
  """Resolves a string to a git commit."""
  return subprocess.check_output(['git', 'rev-parse', rev], cwd=repo).strip()


def UpdateDEPS(deps, from_hash, to_hash):
  """Updates all references of |from_hash| to |to_hash| in |deps|."""
  with open(deps, 'rb') as f:
    contents = f.read()
    if from_hash not in contents:
      raise Exception('%s not in DEPS' % from_hash)
  contents = contents.replace(from_hash, to_hash)
  with open(deps, 'wb') as f:
    f.write(contents)


def main():
  if len(sys.argv) > 2:
    sys.stderr.write('Usage: %s [COMMIT]' % sys.argv[0])
    return 1

  if not IsPristine(SRC_PATH):
    print >>sys.stderr, 'Chromium checkout not pristine.'
    return 0
  if not IsPristine(BORINGSSL_SRC_PATH):
    print >>sys.stderr, 'BoringSSL checkout not pristine.'
    return 0

  if len(sys.argv) > 1:
    commit = RevParse(BORINGSSL_SRC_PATH, sys.argv[1])
  else:
    subprocess.check_call(['git', 'fetch', 'origin'], cwd=BORINGSSL_SRC_PATH)
    commit = RevParse(BORINGSSL_SRC_PATH, 'origin/master')

  head = RevParse(BORINGSSL_SRC_PATH, 'HEAD')
  if head == commit:
    print 'BoringSSL already up to date.'
    return 0

  print 'Rolling BoringSSL from %s to %s...' % (head, commit)

  UpdateDEPS(DEPS_PATH, head, commit)

  # Checkout third_party/boringssl/src to generate new files.
  subprocess.check_call(['git', 'checkout', commit], cwd=BORINGSSL_SRC_PATH)

  # Clear the old generated files.
  for (osname, arch, _, _, _) in generate_build_files.OS_ARCH_COMBOS:
    path = os.path.join(BORINGSSL_PATH, osname + '-' + arch)
    shutil.rmtree(path)
  for file in GENERATED_FILES:
    path = os.path.join(BORINGSSL_PATH, file)
    os.unlink(path)

  # Generate new ones.
  subprocess.check_call(['python',
                         os.path.join(BORINGSSL_SRC_PATH, 'util',
                                      'generate_build_files.py'),
                         'gn'],
                        cwd=BORINGSSL_PATH)

  # Commit everything.
  subprocess.check_call(['git', 'add', DEPS_PATH], cwd=SRC_PATH)
  for (osname, arch, _, _, _) in generate_build_files.OS_ARCH_COMBOS:
    path = os.path.join(BORINGSSL_PATH, osname + '-' + arch)
    subprocess.check_call(['git', 'add', path], cwd=SRC_PATH)
  for file in GENERATED_FILES:
    path = os.path.join(BORINGSSL_PATH, file)
    subprocess.check_call(['git', 'add', path], cwd=SRC_PATH)

  message = """Roll src/third_party/boringssl/src %s..%s

https://boringssl.googlesource.com/boringssl/+log/%s..%s

BUG=none
""" % (head[:9], commit[:9], head, commit)
  subprocess.check_call(['git', 'commit', '-m', message], cwd=SRC_PATH)

  return 0


if __name__ == '__main__':
  sys.exit(main())
