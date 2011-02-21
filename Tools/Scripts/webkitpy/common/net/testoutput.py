#!/usr/bin/env python
# Copyright (C) 2010 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import os
import re


class NaiveImageDiffer(object):
    def same_image(self, img1, img2):
        return img1 == img2


class TestOutput(object):
    """Represents the output that a single layout test generates when it is run
    on a particular platform.
    Note that this is the raw output that is produced when the layout test is
    run, not the results of the subsequent comparison between that output and
    the expected output."""
    def __init__(self, platform, output_type, files):
        self._output_type = output_type
        self._files = files
        file = files[0]  # Pick some file to do test name calculation.
        self._name = self._extract_test_name(file.name())
        self._is_actual = '-actual.' in file.name()

        self._platform = platform or self._extract_platform(file.name())

    def _extract_platform(self, filename):
        """Calculates the platform from the name of the file if it isn't known already"""
        path = filename.split(os.path.sep)
        if 'platform' in path:
            return path[path.index('platform') + 1]
        return None

    def _extract_test_name(self, filename):
        path = filename.split(os.path.sep)
        if 'LayoutTests' in path:
            path = path[1 + path.index('LayoutTests'):]
        if 'layout-test-results' in path:
            path = path[1 + path.index('layout-test-results'):]
        if 'platform' in path:
            path = path[2 + path.index('platform'):]

        filename = path[-1]
        filename = re.sub('-expected\..*$', '', filename)
        filename = re.sub('-actual\..*$', '', filename)
        path[-1] = filename
        return os.path.sep.join(path)

    def save_to(self, path):
        """Have the files in this TestOutput write themselves to the disk at the specified location."""
        for file in self._files:
            file.save_to(path)

    def is_actual(self):
        """Is this output the actual output of a test? (As opposed to expected output.)"""
        return self._is_actual

    def name(self):
        """The name of this test (doesn't include extension)"""
        return self._name

    def __eq__(self, other):
        return (other != None and
                self.name() == other.name() and
                self.type() == other.type() and
                self.platform() == other.platform() and
                self.is_actual() == other.is_actual() and
                self.same_content(other))

    def __hash__(self):
        return hash(str(self.name()) + str(self.type()) + str(self.platform()))

    def is_new_baseline_for(self, other):
        return (self.name() == other.name() and
                self.type() == other.type() and
                self.platform() == other.platform() and
                self.is_actual() and
                (not other.is_actual()))

    def __str__(self):
        actual_str = '[A] ' if self.is_actual() else ''
        return "TestOutput[%s/%s] %s%s" % (self._platform, self._output_type, actual_str, self.name())

    def type(self):
        return self._output_type

    def platform(self):
        return self._platform

    def _path_to_platform(self):
        """Returns the path that tests for this platform are stored in."""
        if self._platform is None:
            return ""
        else:
            return os.path.join("self._platform", self._platform)

    def _save_expected_result(self, file, path):
        path = os.path.join(path, self._path_to_platform())
        extension = os.path.splitext(file.name())[1]
        filename = self.name() + '-expected' + extension
        file.save_to(path, filename)

    def save_expected_results(self, path_to_layout_tests):
        """Save the files of this TestOutput to the appropriate directory
        inside the LayoutTests directory. Typically this means that these files
        will be saved in "LayoutTests/platform/<platform>/, or simply
        LayoutTests if the platform is None."""
        for file in self._files:
            self._save_expected_result(file, path_to_layout_tests)

    def delete(self):
        """Deletes the files that comprise this TestOutput from disk. This
        fails if the files are virtual files (eg: the files may reside inside a
        remote zip file)."""
        for file in self._files:
            file.delete()


class TextTestOutput(TestOutput):
    """Represents a text output of a single test on a single platform"""
    def __init__(self, platform, text_file):
        self._text_file = text_file
        TestOutput.__init__(self, platform, 'text', [text_file])

    def same_content(self, other):
        return self._text_file.contents() == other._text_file.contents()

    def retarget(self, platform):
        return TextTestOutput(platform, self._text_file)


class ImageTestOutput(TestOutput):
    image_differ = NaiveImageDiffer()
    """Represents an image output of a single test on a single platform"""
    def __init__(self, platform, image_file, checksum_file):
        self._checksum_file = checksum_file
        self._image_file = image_file
        files = filter(bool, [self._checksum_file, self._image_file])
        TestOutput.__init__(self, platform, 'image', files)

    def has_checksum(self):
        return self._checksum_file is not None

    def same_content(self, other):
        # FIXME This should not assume that checksums are up to date.
        if self.has_checksum() and other.has_checksum():
            return self._checksum_file.contents() == other._checksum_file.contents()
        else:
            self_contents = self._image_file.contents()
            other_contents = other._image_file.contents()
            return ImageTestOutput.image_differ.same_image(self_contents, other_contents)

    def retarget(self, platform):
        return ImageTestOutput(platform, self._image_file, self._checksum_file)

    def checksum(self):
        return self._checksum_file.contents()
