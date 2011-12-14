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

from webkitpy.common.system.directoryfileset import DirectoryFileSet
from webkitpy.common.system.zipfileset import ZipFileSet
import re
import testoutput
import urllib


class TestOutputSet(object):
    def __init__(self, name, platform, zip_file, **kwargs):
        self._name = name
        self._platform = platform
        self._zip_file = zip_file
        self._include_expected = kwargs.get('include_expected', True)

    @classmethod
    def from_zip_url(cls, platform, zip_path):
        return TestOutputSet('local zip %s builder' % platform, platform, ZipFileSet(zip_path))

    @classmethod
    def from_zip(cls, platform, zip):
        return TestOutputSet('local zip %s builder' % platform, platform, zip)

    @classmethod
    def from_zip_map(cls, zip_map):
        output_sets = []
        for k, v in zip_map.items():
            output_sets.append(TestOutputSet.from_zip(k, v))
        return AggregateTestOutputSet(output_sets)

    @classmethod
    def from_path(self, path, platform=None):
        return TestOutputSet('local %s builder' % platform, platform, DirectoryFileSet(path))

    def name(self):
        return self._name

    def set_platform(self, platform):
        self._platform = platform

    def files(self):
        return [self._zip_file.open(filename) for filename in self._zip_file.namelist()]

    def _extract_output_files(self, name, exact_match):
        name_matcher = re.compile(name)
        actual_matcher = re.compile(r'-actual\.')
        expected_matcher = re.compile(r'-expected\.')

        checksum_files = []
        text_files = []
        image_files = []
        for output_file in self.files():
            name_match = name_matcher.search(output_file.name())
            actual_match = actual_matcher.search(output_file.name())
            expected_match = expected_matcher.search(output_file.name())
            if not (name_match and (actual_match or (self._include_expected and expected_match))):
                continue
            if output_file.name().endswith('.checksum'):
                checksum_files.append(output_file)
            elif output_file.name().endswith('.txt'):
                text_files.append(output_file)
            elif output_file.name().endswith('.png'):
                image_files.append(output_file)

        return (checksum_files, text_files, image_files)

    def _extract_file_with_name(self, name, files):
        for file in files:
            if file.name() == name:
                return file
        return None

    def _make_output_from_image(self, image_file, checksum_files):
        checksum_file_name = re.sub('\.png', '.checksum', image_file.name())
        checksum_file = self._extract_file_with_name(checksum_file_name, checksum_files)
        return testoutput.ImageTestOutput(self._platform, image_file, checksum_file)

    def outputs_for(self, name, **kwargs):
        target_type = kwargs.get('target_type', None)
        exact_match = kwargs.get('exact_match', False)
        if re.search(r'\.x?html', name):
            name = name[:name.rindex('.')]

        (checksum_files, text_files, image_files) = self._extract_output_files(name, exact_match)

        outputs = [self._make_output_from_image(image_file, checksum_files) for image_file in image_files]

        outputs += [testoutput.TextTestOutput(self._platform, text_file) for text_file in text_files]

        if exact_match:
            outputs = filter(lambda output: output.name() == name, outputs)

        outputs = filter(lambda r: target_type in [None, r.type()], outputs)

        return outputs


class AggregateTestOutputSet(object):
    """Set of test outputs from a list of builders"""
    def __init__(self, builders):
        self._builders = builders

    def outputs_for(self, name, **kwargs):
        return sum([builder.outputs_for(name, **kwargs) for builder in self._builders], [])

    def builders(self):
        return self._builders
