# Copyright (C) 2019 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import unittest

from resultsdbpy.controller.configuration import Configuration
from resultsdbpy.controller.configuration_controller import configuration_for_query


class ConfigurationControllerTest(unittest.TestCase):
    @configuration_for_query()
    def func(self, configurations=None):
        return configurations

    def test_configuration_from_no_query(self):
        self.assertEqual([Configuration()], self.func())

    def test_configurations_from_query_expansion(self):
        self.assertEqual(
            sorted([Configuration(platform='iOS'), Configuration(platform='Mac')]),
            sorted(self.func(platform=['iOS', 'Mac'])),
        )
        self.assertEqual(
            sorted([
                Configuration(platform='iOS', style='Release'),
                Configuration(platform='Mac', style='Debug'),
                Configuration(platform='iOS', style='Debug'),
                Configuration(platform='Mac', style='Release'),
            ]), sorted(self.func(platform=['iOS', 'Mac'], style=['Debug', 'Release'])),
        )

    def test_configurations_from_query_platform(self):
        self.assertEqual(
            [Configuration(platform='iOS')],
            self.func(platform=['iOS']),
        )

    def test_configuration_from_query_version(self):
        self.assertEqual(
            [Configuration(version='1.0.1')],
            self.func(version=['1.0.1']),
        )

    def test_configuration_from_query_version_name(self):
        self.assertEqual(
            [Configuration(version_name='High Sierra')],
            self.func(version_name=['High Sierra']),
        )

    def test_configuration_from_query_is_simulator(self):
        self.assertEqual(
            [Configuration(is_simulator=True)],
            self.func(is_simulator=['true']),
        )

    def test_configuration_from_query_architecture(self):
        self.assertEqual(
            [Configuration(architecture='arm64')],
            self.func(architecture=['arm64']),
        )

    def test_configuration_from_query_type(self):
        self.assertEqual(
            [Configuration(style='Debug')],
            self.func(style=['Debug']),
        )

    def test_configuration_from_query_flavor(self):
        self.assertEqual(
            [Configuration(flavor='wk2')],
            self.func(flavor=['wk2']),
        )
