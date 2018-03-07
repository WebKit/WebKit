#!/usr/bin/env python
# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import os
import re
import string


# TARGET_RE matches a GN target, and extracts the target name and the contents.
TARGET_RE = re.compile(r'(?P<indent>\s*)\w+\("(?P<target_name>\w+)"\) {'
                       r'(?P<target_contents>.*?)'
                       r'(?P=indent)}',
                       re.MULTILINE | re.DOTALL)

# SOURCES_RE matches a block of sources inside a GN target.
SOURCES_RE = re.compile(
    r'(sources|public|common_objc_headers) \+?= \[(?P<sources>.*?)\]',
    re.MULTILINE | re.DOTALL)

SOURCE_FILE_RE = re.compile(r'.*\"(?P<source_file>.*)\"')


class NoBuildGnFoundError(Exception):
  pass


class WrongFileTypeError(Exception):
  pass


def _ReadFile(file_path):
  """Returns the content of file_path in a string.

  Args:
    file_path: the path of the file to read.
  Returns:
    A string with the content of the file.
  """
  with open(file_path) as f:
    return f.read()


def GetBuildGnPathFromFilePath(file_path, file_exists_check, root_dir_path):
  """Returns the BUILD.gn file responsible for file_path.

  Args:
    file_path: the absolute path to the .h file to check.
    file_exists_check: a function that defines how to check if a file exists
      on the file system.
    root_dir_path: the absolute path of the root of project.

  Returns:
    A string with the absolute path to the BUILD.gn file responsible to include
    file_path in a target.
  """
  if not file_path.endswith('.h'):
    raise WrongFileTypeError(
        'File {} is not an header file (.h)'.format(file_path))
  candidate_dir = os.path.dirname(file_path)
  while candidate_dir.startswith(root_dir_path):
    candidate_build_gn_path = os.path.join(candidate_dir, 'BUILD.gn')
    if file_exists_check(candidate_build_gn_path):
      return candidate_build_gn_path
    else:
      candidate_dir = os.path.abspath(os.path.join(candidate_dir,
                                                   os.pardir))
  raise NoBuildGnFoundError(
      'No BUILD.gn file found for file: `{}`'.format(file_path))


def IsHeaderInBuildGn(header_path, build_gn_path):
  """Returns True if the header is listed in the BUILD.gn file.

  Args:
    header_path: the absolute path to the header to check.
    build_gn_path: the absolute path to the header to check.

  Returns:
    bool: True if the header_path is an header that is listed in
      at least one GN target in the BUILD.gn file specified by
      the argument build_gn_path.
  """
  target_abs_path = os.path.dirname(build_gn_path)
  build_gn_content = _ReadFile(build_gn_path)
  headers_in_build_gn = GetHeadersInBuildGnFileSources(build_gn_content,
                                                       target_abs_path)
  return header_path in headers_in_build_gn


def GetHeadersInBuildGnFileSources(file_content, target_abs_path):
  """Returns a set with all the .h files in the file_content.

  Args:
    file_content: a string with the content of the BUILD.gn file.
    target_abs_path: the absolute path to the directory where the
      BUILD.gn file lives.

  Returns:
    A set with all the headers (.h file) in the file_content.
    The set contains absolute paths.
  """
  headers_in_sources = set([])
  for target_match in TARGET_RE.finditer(file_content):
    target_contents = target_match.group('target_contents')
    for sources_match in SOURCES_RE.finditer(target_contents):
      sources = sources_match.group('sources')
      for source_file_match in SOURCE_FILE_RE.finditer(sources):
        source_file = source_file_match.group('source_file')
        if source_file.endswith('.h'):
          source_file_tokens = string.split(source_file, '/')
          # pylint: disable=star-args
          headers_in_sources.add(os.path.join(target_abs_path,
                                              *source_file_tokens))
  return headers_in_sources
