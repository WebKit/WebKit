#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
angle_presubmit_utils: Mock depot_tools class for ANGLE presubmit checks's unittests
"""


import os
import json

class Change_mock():

    def __init__(self, description_text):
        self.description_text = description_text

    def DescriptionText(self):
        return self.description_text


class AffectedFile_mock():

    def __init__(self, diff, local_path='', old_contents=None, new_contents=None):
        self.diff = diff
        self._local_path = local_path
        self._old_contents = old_contents or []
        self._new_contents = new_contents or []

    def LocalPath(self):
        return self._local_path

    def GenerateScmDiff(self):
        return self.diff

    def OldContents(self):
        return self._old_contents

    def NewContents(self):
        return self._new_contents


class InputAPI_mock():

    def __init__(self, description_text, source_files=[], affected_files=[]):
        self.change = Change_mock(description_text)
        self.source_files = source_files
        self.affected_files = affected_files
        self.os_path = os.path
        self.json = json

    def PresubmitLocalPath(self):
        return self.cwd

    def AffectedSourceFiles(self, source_filter):
        return self.source_files

    def AffectedFiles(self):
        return self.affected_files


class _PresubmitResult(object):
    """Base class for result objects."""
    fatal = False
    should_prompt = False

    def __init__(self, message, long_text=''):
        self._message = message

    def __eq__(self, other):
        return self.fatal == other.fatal and self.should_prompt == other.should_prompt \
            and self._message == other._message


# Top level object so multiprocessing can pickle
# Public access through OutputApi object.
class _PresubmitError(_PresubmitResult):
    """A hard presubmit error."""
    fatal = True


# Top level object so multiprocessing can pickle
# Public access through OutputApi object.
class _PresubmitPromptWarning(_PresubmitResult):
    """An warning that prompts the user if they want to continue."""
    should_prompt = True


# Top level object so multiprocessing can pickle
# Public access through OutputApi object.
class _PresubmitNotifyResult(_PresubmitResult):
    """Just print something to the screen -- but it's not even a warning."""
    pass


class OutputAPI_mock():
    PresubmitResult = _PresubmitResult
    PresubmitError = _PresubmitError
    PresubmitPromptWarning = _PresubmitPromptWarning
    PresubmitNotifyResult = _PresubmitNotifyResult
