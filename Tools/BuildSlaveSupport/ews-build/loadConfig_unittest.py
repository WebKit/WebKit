#!/usr/bin/env python
#
# Copyright (C) 2018 Apple Inc. All rights reserved.
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

import loadConfig


class ConfigDotJSONTest(unittest.TestCase):
    def test_configuration(self):
        cwd = os.path.dirname(os.path.abspath(__file__))
        loadConfig.loadBuilderConfig({}, master_prefix_path=cwd)

    def test_builder_keys(self):
        cwd = os.path.dirname(os.path.abspath(__file__))
        config = json.load(open(os.path.join(cwd, 'config.json')))
        valid_builder_keys = ['additionalArguments', 'architectures', 'builddir', 'configuration', 'description',
                              'defaultProperties', 'env', 'factory', 'locks', 'name', 'platform', 'properties', 'shortname', 'tags',
                              'triggers', 'workernames', 'workerbuilddir']
        for builder in config.get('builders', []):
            for key in builder:
                self.assertTrue(key in valid_builder_keys, 'Unexpected key {} for builder {}'.format(key, builder.get('name')))


class TagsForBuilderTeest(unittest.TestCase):
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
        self.verifyTags('GTK-Webkit2-EWS', ['GTK', 'Webkit2'])
        self.verifyTags('macOS-Sierra-Release-WK1-EWS', ['Sierra', 'Release', 'macOS', 'WK1'])
        self.verifyTags('macOS-High-Sierra-Release-32bit-WK2-EWS', ['macOS', 'High', 'Sierra', 'Release', 'WK2', '32bit'])

    def test_tags_type(self):
        tags = loadConfig.getTagsForBuilder({'name': u'iOS-11-EWS'})
        self.assertEqual(tags, ['iOS'])
        self.assertEqual(type(tags[0]), str)

    def test_getBlackListedTags(self):
        blacklistedTags = loadConfig.getBlackListedTags()
        expectedTags = ['0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '10',
                        '11', '12', '13', '14', '15', '16', '17', '18', '19', 'EWS', 'TryBot']
        self.assertEqual(blacklistedTags, expectedTags)


class TestcheckValidWorker(unittest.TestCase):
    def test_invalid_worker(self):
        with self.assertRaises(Exception) as context:
            loadConfig.checkValidWorker({})
        self.assertEqual(context.exception.args, ('Worker is None or Empty.',))

    def test_worker_with_missing_name(self):
        with self.assertRaises(Exception) as context:
            loadConfig.checkValidWorker({'platform': 'mac-sierra'})
        self.assertEqual(context.exception.args, ('Worker "{\'platform\': \'mac-sierra\'}" does not have name defined.',))

    def test_worker_with_missing_platName(self):
        with self.assertRaises(Exception) as context:
            loadConfig.checkValidWorker({'name': 'ews101'})
        self.assertEqual(context.exception.args, ('Worker ews101 does not have platform defined.',))

    def test_valid_worker(self):
        loadConfig.checkValidWorker({'name': 'ews101', 'platform': 'mac-sierra'})


class TestcheckValidBuilder(unittest.TestCase):
    def test_invalid_builder(self):
        with self.assertRaises(Exception) as context:
            loadConfig.checkValidBuilder({}, {})
        self.assertEqual(context.exception.args, ('Builder is None or Empty.',))

    def test_builder_with_missing_name(self):
        with self.assertRaises(Exception) as context:
            loadConfig.checkValidBuilder({}, {'platform': 'mac-sierra'})
        self.assertEqual(context.exception.args, ('Builder "{\'platform\': \'mac-sierra\'}" does not have name defined.',))

    def test_builder_with_missing_shortname(self):
        with self.assertRaises(Exception) as context:
            loadConfig.checkValidBuilder({}, {'platform': 'mac-sierra', 'name': 'mac-wk2(test)'})
        self.assertEqual(context.exception.args, ('Builder "mac-wk2(test)" does not have short name defined. This name is needed for EWS status bubbles.',))

    def test_builder_with_invalid_identifier(self):
        with self.assertRaises(Exception) as context:
            loadConfig.checkValidBuilder({}, {'name': 'mac-wk2(test)', 'shortname': 'mac-wk2'})
        self.assertEqual(context.exception.args, ('Builder name mac-wk2(test) is not a valid buildbot identifier.',))

    def test_builder_with_extra_long_name(self):
        longName = 'a' * 71
        with self.assertRaises(Exception) as context:
            loadConfig.checkValidBuilder({}, {'name': longName, 'shortname': 'a'})
        self.assertEqual(context.exception.args, ('Builder name {} is longer than maximum allowed by Buildbot (70 characters).'.format(longName),))

    def test_builder_with_invalid_configuration(self):
        with self.assertRaises(Exception) as context:
            loadConfig.checkValidBuilder({}, {'name': 'mac-wk2', 'shortname': 'mac-wk2', 'configuration': 'asan'})
        self.assertEqual(context.exception.args, ('Invalid configuration: asan for builder: mac-wk2',))

    def test_builder_with_missing_factory(self):
        with self.assertRaises(Exception) as context:
            loadConfig.checkValidBuilder({}, {'name': 'mac-wk2', 'shortname': 'mac-wk2', 'configuration': 'release'})
        self.assertEqual(context.exception.args, ('Builder mac-wk2 does not have factory defined.',))

    def test_builder_with_missing_scheduler(self):
        with self.assertRaises(Exception) as context:
            loadConfig.checkValidBuilder({}, {'name': 'mac-wk2', 'shortname': 'mac-wk2', 'configuration': 'release', 'factory': 'WK2Factory', 'platform': 'mac-sierra', 'triggers': ['api-tests-mac-ews']})
        self.assertEqual(context.exception.args, ('Trigger: api-tests-mac-ews in builder mac-wk2 does not exist in list of Trigerrable schedulers.',))

    def test_builder_with_missing_platform(self):
        with self.assertRaises(Exception) as context:
            loadConfig.checkValidBuilder({}, {'name': 'mac-wk2', 'shortname': 'mac-wk2', 'configuration': 'release', 'factory': 'WK2Factory'})
        self.assertEqual(context.exception.args, ('Builder mac-wk2 does not have platform defined.',))

    def test_valid_builder(self):
        loadConfig.checkValidBuilder({}, {'name': 'macOS-High-Sierra-WK2-EWS', 'shortname': 'mac-wk2', 'configuration': 'release', 'factory': 'WK2Factory', 'platform': 'mac-sierra'})


class TestcheckWorkersAndBuildersForConsistency(unittest.TestCase):
    def __init__(self, *args, **kwargs):
        self.WK2Builder = {'name': 'macOS-High-Sierra-WK2-EWS', 'shortname': 'mac-wk2', 'factory': 'WK2Factory', 'platform': 'mac-sierra', 'workernames': ['ews101', 'ews102']}
        self.ews101 = {'name': 'ews101', 'platform': 'mac-sierra'}
        self.ews102 = {'name': 'ews102', 'platform': 'ios-11'}
        super(TestcheckWorkersAndBuildersForConsistency, self).__init__(*args, **kwargs)

    def test_checkWorkersAndBuildersForConsistency(self):
        with self.assertRaises(Exception) as context:
            loadConfig.checkWorkersAndBuildersForConsistency({}, [], [self.WK2Builder])
        self.assertEqual(context.exception.args, ('Builder macOS-High-Sierra-WK2-EWS has worker ews101, which is not defined in workers list!',))

    def test_checkWorkersAndBuildersForConsistency1(self):
        with self.assertRaises(Exception) as context:
            loadConfig.checkWorkersAndBuildersForConsistency({}, [self.ews101, self.ews102], [self.WK2Builder])
        self.assertEqual(context.exception.args, ('Builder macOS-High-Sierra-WK2-EWS is for platform mac-sierra, but has worker ews102 for platform ios-11!',))

    def test_success(self):
        loadConfig.checkWorkersAndBuildersForConsistency({}, [self.ews101, {'name': 'ews102', 'platform': 'mac-sierra'}], [self.WK2Builder])


if __name__ == '__main__':
    unittest.main()
