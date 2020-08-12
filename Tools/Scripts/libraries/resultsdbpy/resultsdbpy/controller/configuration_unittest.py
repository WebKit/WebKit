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


class ConfigurationUnittest(unittest.TestCase):

    def test_version_integer_conversion(self):
        self.assertEqual(1002003, Configuration.version_to_integer([1, 2, 3]))
        self.assertEqual(1000000, Configuration.version_to_integer([1]))
        self.assertEqual(1002003, Configuration.version_to_integer('1.2.3'))
        self.assertEqual(1000000, Configuration.version_to_integer('1'))
        self.assertEqual(1, Configuration.version_to_integer(1))

        self.assertEqual('1.2.3', Configuration.integer_to_version(1002003))
        self.assertEqual('1.2.0', Configuration.integer_to_version(1002000))
        self.assertEqual('1.0.0', Configuration.integer_to_version(1000000))
        self.assertEqual('0.1.2', Configuration.integer_to_version(1002))
        self.assertEqual('0.0.1', Configuration.integer_to_version(1))

    def test_wildcard_compare(self):
        reference = Configuration(platform='Mac', version='10.14', is_simulator=False, architecture='x86_64', style='Debug', flavor='wk2')

        should_match = [
            Configuration(platform='Mac'),
            Configuration(version='10.14'),
            Configuration(is_simulator=False),
            Configuration(architecture='x86_64'),
            Configuration(style='Debug'),
            Configuration(flavor='wk2'),
        ]
        for element in should_match:
            self.assertEqual(element, reference)
            self.assertFalse(element != reference)

        shouldnt_match = [
            Configuration(platform='iOS'),
            Configuration(version='10.13'),
            Configuration(is_simulator=True),
            Configuration(architecture='arm64'),
            Configuration(style='Release'),
            Configuration(flavor='wk1'),
        ]
        for element in shouldnt_match:
            self.assertNotEqual(element, reference)
            self.assertFalse(element == reference)

    def test_jsonify(self):
        configs = [
            Configuration(platform='Mac', version='10.14', is_simulator=False, architecture='x86_64', style='Debug', flavor='wk2'),
            Configuration(platform='Mac', version='10.14', style='Production', flavor='wk2'),
            Configuration(platform='iOS', version='11', is_simulator=True, architecture='x86_64', style='Debug'),
            Configuration(platform='iOS', version='12', is_simulator=False, architecture='arm64', style='Release'),
        ]

        for config in configs:
            converted_config = Configuration.from_json(config.to_json())
            self.assertEqual(config, converted_config)

    def test_to_query(self):
        self.assertEqual(
            Configuration(platform='Mac', version='10.14', is_simulator=False, architecture='x86_64', style='Debug', flavor='wk2').to_query(),
            'platform=Mac&is_simulator=False&version=10.14.0&architecture=x86_64&style=Debug&flavor=wk2',
        )
        self.assertEqual(
            Configuration(platform='Mac', version='10.14', style='Production', flavor='wk2').to_query(),
            'platform=Mac&version=10.14.0&style=Production&flavor=wk2',
        )
        self.assertEqual(
            Configuration(platform='iOS', version='11', is_simulator=True, architecture='x86_64', style='Debug').to_query(),
            'platform=iOS&is_simulator=True&version=11.0.0&architecture=x86_64&style=Debug',
        )
        self.assertEqual(
            Configuration(platform='iOS', version='12', is_simulator=False, architecture='arm64', style='Release').to_query(),
            'platform=iOS&is_simulator=False&version=12.0.0&architecture=arm64&style=Release',
        )
