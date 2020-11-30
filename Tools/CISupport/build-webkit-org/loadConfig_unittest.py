#!/usr/bin/env python
#
# Copyright (C) 2020 Apple Inc. All rights reserved.
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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


import json
import os
import unittest


class ConfigDotJSONTest(unittest.TestCase):
    def get_config(self):
        cwd = os.path.dirname(os.path.abspath(__file__))
        return json.load(open(os.path.join(cwd, 'config.json')))

    def test_builder_keys(self):
        config = self.get_config()
        valid_builder_keys = ['additionalArguments', 'architectures', 'builddir', 'configuration', 'description',
                              'defaultProperties', 'device_model', 'env', 'factory', 'icon', 'locks', 'name', 'platform', 'properties',
                              'remotes', 'runTests', 'shortname', 'tags', 'triggers', 'workernames', 'workerbuilddir']
        for builder in config.get('builders', []):
            for key in builder:
                self.assertTrue(key in valid_builder_keys, 'Unexpected key "{}" for builder {}'.format(key, builder.get('name')))

    def test_multiple_scheduers_for_builder(self):
        config = self.get_config()
        builder_to_schduler_map = {}

        for scheduler in config.get('schedulers'):
            for buildername in scheduler.get('builderNames'):
                self.assertTrue(buildername not in builder_to_schduler_map, 'builder {} appears multiple times in schedulers.'.format(buildername))
                builder_to_schduler_map[buildername] = scheduler.get('name')

    def test_schduler_contains_valid_builder_name(self):
        config = self.get_config()
        builder_name_list = [builder['name'] for builder in config['builders']]
        for scheduler in config.get('schedulers'):
            for buildername in scheduler.get('builderNames'):
                self.assertTrue(buildername in builder_name_list, 'builder "{}" in scheduler "{}" is invalid.'.format(buildername, scheduler.get('name')))

    def test_single_builder_for_triggerable_scheduler(self):
        config = self.get_config()
        for scheduler in config['schedulers']:
            if scheduler.get('type') == 'Triggerable':
                self.assertTrue(len(scheduler.get('builderNames')) == 1, 'scheduler "{}" triggers multiple builders.'.format(scheduler['name']))


class TagsForBuilderTest(unittest.TestCase):
    def verifyTags(self, builderName, expectedTags):
        tags = loadConfig.getTagsForBuilder({'name': builderName})
        self.assertEqual(sorted(tags), sorted(expectedTags))

    def test_getTagsForBuilder(self):
        self.verifyTags('EWS', [])
        self.verifyTags('TryBot-10-EWS', [])
        self.verifyTags('11-EWS', [])
        self.verifyTags('32-EWS', ['32'])
        self.verifyTags('iOS-11-EWS', ['iOS'])
        self.verifyTags('iOS(11),(test)-EWS', ['iOS', 'test'])
        self.verifyTags('Windows-EWS', ['Windows'])
        self.verifyTags('Windows_Windows', ['Windows'])
        self.verifyTags('GTK-Build-EWS', ['GTK', 'Build'])
        self.verifyTags('GTK-WK2-Tests-EWS', ['GTK', 'WK2', 'Tests'])
        self.verifyTags('macOS-Sierra-Release-WK1-EWS', ['Sierra', 'Release', 'macOS', 'WK1'])
        self.verifyTags('macOS-High-Sierra-Release-32bit-WK2-EWS', ['macOS', 'High', 'Sierra', 'Release', 'WK2', '32bit'])

    def test_tags_type(self):
        tags = loadConfig.getTagsForBuilder({'name': 'iOS-11-EWS'})
        self.assertEqual(tags, ['iOS'])
        self.assertEqual(type(tags[0]), str)

    def test_getInvalidTags(self):
        invalidTags = loadConfig.getInvalidTags()
        expectedTags = ['0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '10',
                        '11', '12', '13', '14', '15', '16', '17', '18', '19', 'EWS', 'TryBot']
        self.assertEqual(invalidTags, expectedTags)


if __name__ == '__main__':
    from steps_unittest_old import BuildBotConfigLoader
    BuildBotConfigLoader()._add_dependent_modules_to_sys_modules()
    import loadConfig
    unittest.main()
