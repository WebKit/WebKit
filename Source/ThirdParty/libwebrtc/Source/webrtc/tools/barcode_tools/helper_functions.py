#!/usr/bin/env python
# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import multiprocessing
import os
import subprocess
import sys

_DEFAULT_PADDING = 4


class HelperError(Exception):
  """Exception raised for errors in the helper."""
  pass


def zero_pad(number, padding=_DEFAULT_PADDING):
  """Converts an int into a zero padded string.

  Args:
    number(int): The number to convert.
    padding(int): The number of chars in the output. Note that if you pass for
      example number=23456 and padding=4, the output will still be '23456',
      i.e. it will not be cropped. If you pass number=2 and padding=4, the
      return value will be '0002'.
  Return:
    (string): The zero padded number converted to string.
  """
  return str(number).zfill(padding)


def run_shell_command(cmd_list, fail_msg=None):
  """Executes a command.

  Args:
    cmd_list(list): Command list to execute.
    fail_msg(string): Message describing the error in case the command fails.

  Return:
    (string): The standard output from running the command.

  Raise:
    HelperError: If command fails.
  """
  process = subprocess.Popen(cmd_list, stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE)
  output, error = process.communicate()
  if process.returncode != 0:
    if fail_msg:
      print >> sys.stderr, fail_msg
    raise HelperError('Failed to run %s: command returned %d and printed '
                      '%s and %s' % (' '.join(cmd_list), process.returncode,
                                     output, error))
  return output.strip()


def perform_action_on_all_files(directory, file_pattern, file_extension,
                                start_number, action, **kwargs):
  """Function that performs a given action on all files matching a pattern.

  It is assumed that the files are named file_patternxxxx.file_extension, where
  xxxx are digits starting from start_number.

  Args:
    directory(string): The directory where the files live.
    file_pattern(string): The name pattern of the files.
    file_extension(string): The files' extension.
    start_number(int): From where to start to count frames.
    action(function): The action to be performed over the files. Must return
        False if the action failed, True otherwise. It should take a file name
        as the first argument and **kwargs as arguments. The function must be
        possible to pickle, so it cannot be a bound function (for instance).

  Return:
    (bool): Whether performing the action over all files was successful or not.
  """
  file_prefix = os.path.join(directory, file_pattern)
  file_number = start_number

  process_pool = multiprocessing.Pool(processes=multiprocessing.cpu_count())
  results = []
  while True:
    zero_padded_file_number = zero_pad(file_number)
    file_name = file_prefix + zero_padded_file_number + '.' + file_extension
    if not os.path.isfile(file_name):
      break
    future = process_pool.apply_async(action, args=(file_name,), kwds=kwargs)
    results.append(future)
    file_number += 1

  successful = True
  for result in results:
    if not result.get():
      print "At least one action %s failed for files %sxxxx.%s." % (
          action, file_pattern, file_extension)
      successful = False

  process_pool.close()
  return successful
