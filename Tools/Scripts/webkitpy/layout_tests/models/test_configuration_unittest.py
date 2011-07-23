# Copyright (C) 2011 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the Google name nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import unittest

from webkitpy.layout_tests import port
from webkitpy.layout_tests.models.test_configuration import *


class TestConfigurationTest(unittest.TestCase):
    def test_items(self):
        config = TestConfiguration(None, 'xp', 'x86', 'release', 'cpu')
        result_config_dict = {}
        for category, specifier in config.items():
            result_config_dict[category] = specifier
        self.assertEquals({'version': 'xp', 'architecture': 'x86', 'build_type': 'release', 'graphics_type': 'cpu'}, result_config_dict)

    def test_keys(self):
        config = TestConfiguration(None, 'xp', 'x86', 'release', 'cpu')
        result_config_keys = []
        for category in config.keys():
            result_config_keys.append(category)
        self.assertEquals(set(['graphics_type', 'version', 'architecture', 'build_type']), set(result_config_keys))

    def test_str(self):
        config = TestConfiguration(None, 'xp', 'x86', 'release', 'cpu')
        self.assertEquals('<xp, x86, release, cpu>', str(config))

    def test_repr(self):
        config = TestConfiguration(None, 'xp', 'x86', 'release', 'cpu')
        self.assertEquals("TestConfig(version='xp', architecture='x86', build_type='release', graphics_type='cpu')", repr(config))

    def test_values(self):
        config = TestConfiguration(None, 'xp', 'x86', 'release', 'cpu')
        result_config_values = []
        for value in config.values():
            result_config_values.append(value)
        self.assertEquals(set(['xp', 'x86', 'release', 'cpu']), set(result_config_values))

    def test_port_init(self):
        config = TestConfiguration(port.get('test-win-xp', None))
        self.assertEquals('<xp, x86, release, cpu>', str(config))

    def test_all_test_configurations(self):
        all_configs = TestConfiguration(None, 'xp', 'x86', 'release', 'cpu').all_test_configurations()
        for config in all_configs:
            self.assertTrue(isinstance(config, TestConfiguration))
