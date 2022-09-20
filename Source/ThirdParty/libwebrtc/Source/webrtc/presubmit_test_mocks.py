#!/usr/bin/env vpython3

# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

# This file is inspired to [1].
# [1] - https://cs.chromium.org/chromium/src/PRESUBMIT_test_mocks.py

from __future__ import absolute_import
import os.path
import re


class MockInputApi:
  """Mock class for the InputApi class.

  This class can be used for unittests for presubmit by initializing the files
  attribute as the list of changed files.
  """

  def __init__(self):
    self.change = MockChange([], [])
    self.files = []
    self.presubmit_local_path = os.path.dirname(__file__)
    self.re = re  # pylint: disable=invalid-name

  def AffectedSourceFiles(self, file_filter=None):
    return self.AffectedFiles(file_filter=file_filter)

  def AffectedFiles(self, file_filter=None, include_deletes=False):
    for f in self.files:
      if file_filter and not file_filter(f):
        continue
      if not include_deletes and f.Action() == 'D':
        continue
      yield f

  @classmethod
  def FilterSourceFile(cls, affected_file, files_to_check=(), files_to_skip=()):
    # pylint: disable=unused-argument
    return True

  def PresubmitLocalPath(self):
    return self.presubmit_local_path

  def ReadFile(self, affected_file, mode='r'):
    filename = affected_file.AbsoluteLocalPath()
    for f in self.files:
      if f.LocalPath() == filename:
        with open(filename, mode) as f:
          return f.read()
    # Otherwise, file is not in our mock API.
    raise IOError("No such file or directory: '%s'" % filename)


class MockOutputApi:
  """Mock class for the OutputApi class.

  An instance of this class can be passed to presubmit unittests for outputing
  various types of results.
  """

  class PresubmitResult:
    def __init__(self, message, items=None, long_text=''):
      self.message = message
      self.items = items
      self.long_text = long_text

    def __repr__(self):
      return self.message

  class PresubmitError(PresubmitResult):
    def __init__(self, message, items=None, long_text=''):
      MockOutputApi.PresubmitResult.__init__(self, message, items, long_text)
      self.type = 'error'


class MockChange:
  """Mock class for Change class.

  This class can be used in presubmit unittests to mock the query of the
  current change.
  """

  def __init__(self, changed_files, bugs_from_description, tags=None):
    self._changed_files = changed_files
    self._bugs_from_description = bugs_from_description
    self.tags = dict() if not tags else tags

  def BugsFromDescription(self):
    return self._bugs_from_description

  def __getattr__(self, attr):
    """Return tags directly as attributes on the object."""
    if not re.match(r"^[A-Z_]*$", attr):
      raise AttributeError(self, attr)
    return self.tags.get(attr)


class MockFile:
  """Mock class for the File class.

  This class can be used to form the mock list of changed files in
  MockInputApi for presubmit unittests.
  """

  def __init__(self,
               local_path,
               new_contents=None,
               old_contents=None,
               action='A'):
    if new_contents is None:
      new_contents = ["Data"]
    self._local_path = local_path
    self._new_contents = new_contents
    self._changed_contents = [(i + 1, l) for i, l in enumerate(new_contents)]
    self._action = action
    self._old_contents = old_contents

  def Action(self):
    return self._action

  def ChangedContents(self):
    return self._changed_contents

  def NewContents(self):
    return self._new_contents

  def LocalPath(self):
    return self._local_path

  def AbsoluteLocalPath(self):
    return self._local_path

  def OldContents(self):
    return self._old_contents
