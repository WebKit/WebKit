#!/usr/bin/env python
#  Copyright 2018 The ANGLE Project Authors. All rights reserved.
#  Use of this source code is governed by a BSD-style license that can be
#  found in the LICENSE file.

# Generate library file with compressed symbols per Android build
# process.
#

import argparse
import os
import subprocess
import sys


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument(
      '--objcopy',
      required=True,
      help='The objcopy binary to run',
      metavar='PATH')
  parser.add_argument(
      '--nm', required=True, help='The nm binary to run', metavar='PATH')
  parser.add_argument(
      '--sofile',
      required=True,
      help='Shared object file produced by linking command',
      metavar='FILE')
  parser.add_argument(
      '--output',
      required=True,
      help='Final output shared object file',
      metavar='FILE')
  parser.add_argument(
      '--unstrippedsofile',
      required=True,
      help='Unstripped shared object file produced by linking command',
      metavar='FILE')
  args = parser.parse_args()

  copy_cmd = ["cp", args.sofile, args.output]
  result = subprocess.call(copy_cmd)

  objcopy_cmd = [args.objcopy]
  objcopy_cmd.append('--only-keep-debug')
  objcopy_cmd.append(args.unstrippedsofile)
  objcopy_cmd.append(args.output + '.debug')
  result = subprocess.call(objcopy_cmd)

  nm_cmd = subprocess.Popen(
      [args.nm, args.unstrippedsofile, '--format=posix', '--defined-only'],
      stdout=subprocess.PIPE)

  awk_cmd = subprocess.Popen(['awk', '{ print $1}'],
                             stdin=nm_cmd.stdout,
                             stdout=subprocess.PIPE)

  dynsym_out = open(args.output + '.dynsyms', 'w')
  sort_cmd = subprocess.Popen(['sort'], stdin=awk_cmd.stdout, stdout=dynsym_out)
  dynsym_out.close()

  nm_cmd = subprocess.Popen(
      [args.nm, args.unstrippedsofile, '--format=posix', '--defined-only'],
      stdout=subprocess.PIPE)

  awk_cmd = subprocess.Popen(
      ['awk', '{ if ($2 == "T" || $2 == "t" ||' + ' $2 == "D") print $1 }'],
      stdin=nm_cmd.stdout,
      stdout=subprocess.PIPE)

  funcsyms_out = open(args.output + '.funcsyms', 'w')
  sort_cmd = subprocess.Popen(['sort'],
                              stdin=awk_cmd.stdout,
                              stdout=funcsyms_out)
  funcsyms_out.close()

  keep_symbols = open(args.output + '.keep_symbols', 'w')
  sort_cmd = subprocess.Popen(
      ['comm', '-13', args.output + '.dynsyms', args.output + '.funcsyms'],
      stdin=awk_cmd.stdout,
      stdout=keep_symbols)

  # Ensure that the keep_symbols file is not empty.
  keep_symbols.write("\n")
  keep_symbols.close()

  objcopy_cmd = [
      args.objcopy, '--rename-section', '.debug_frame=saved_debug_frame',
      args.output + '.debug', args.output + ".mini_debuginfo"
  ]
  subprocess.check_call(objcopy_cmd)

  objcopy_cmd = [
      args.objcopy, '-S', '--remove-section', '.gdb_index', '--remove-section',
      '.comment', '--keep-symbols=' + args.output + '.keep_symbols',
      args.output + '.mini_debuginfo'
  ]
  subprocess.check_call(objcopy_cmd)

  objcopy_cmd = [
      args.objcopy, '--rename-section', '.saved_debug_frame=.debug_frame',
      args.output + ".mini_debuginfo"
  ]
  subprocess.check_call(objcopy_cmd)

  xz_cmd = ['xz', args.output + '.mini_debuginfo']
  subprocess.check_call(xz_cmd)

  objcopy_cmd = [
      args.objcopy, '--add-section',
      '.gnu_debugdata=' + args.output + '.mini_debuginfo.xz', args.output
  ]
  subprocess.check_call(objcopy_cmd)

  # Clean out scratch files
  rm_cmd = [
      'rm', '-f', args.output + '.dynsyms', args.output + '.funcsyms',
      args.output + '.keep_symbols', args.output + '.debug',
      args.output + '.mini_debuginfo', args.output + '.mini_debuginfo.xz'
  ]
  result = subprocess.call(rm_cmd)

  return result


if __name__ == "__main__":
  sys.exit(main())
