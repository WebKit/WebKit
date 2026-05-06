#!/usr/bin/env python3
#
# Copyright (C) 2018-2022 Apple Inc. All rights reserved.
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

from datetime import datetime, timedelta, timezone
from twisted.internet import defer

from . import loadConfig


class ConfigDotJSONTest(unittest.TestCase):
    DUPLICATED_TRIGGERS = ['try', 'pull_request']

    def get_config(self):
        cwd = os.path.dirname(os.path.abspath(__file__))
        with open(os.path.join(cwd, 'config.json'), 'r') as config:
            return json.load(config)

    def get_builder_from_config(self, config, builder_name):
        for builder in config['builders']:
            if builder_name == builder.get('name'):
                return builder

    def test_configuration(self):
        cwd = os.path.dirname(os.path.abspath(__file__))
        loadConfig.loadBuilderConfig({}, is_test_mode_enabled=True, master_prefix_path=cwd)

    def test_tab_character(self):
        cwd = os.path.dirname(os.path.abspath(__file__))
        with open(os.path.join(cwd, 'config.json'), 'r') as config:
            self.assertTrue('\t' not in config.read(), 'Tab character found in config.json, please use spaces instead of tabs.')

    def test_builder_keys(self):
        config = self.get_config()
        valid_builder_keys = ['additionalArguments', 'architectures', 'builddir', 'configuration', 'description',
                              'defaultProperties', 'deployment_target', 'env', 'factory', 'icon', 'locks', 'name',
                              'platform', 'pr_column', 'properties', 'rebuild_without_change_on_builder', 'remotes', 'runTests',
                              'safe_merge_queue', 'shortname', 'tags', 'triggers', 'triggered_by', 'workernames',
                              'workerbuilddir']
        for builder in config.get('builders', []):
            for key in builder:
                self.assertTrue(key in valid_builder_keys, 'Unexpected key "{}" for builder {}'.format(key, builder.get('name')))

    def test_multiple_scheduers_for_builder(self):
        config = self.get_config()
        builder_to_schduler_map = {}
        triggered_by_schedulers = []
        for builder in config['builders']:
            triggered_by = builder.get('triggered_by')
            if triggered_by:
                triggered_by_schedulers.extend(triggered_by)

        for scheduler in config.get('schedulers'):
            if scheduler['name'] in triggered_by_schedulers:
                continue
            for buildername in scheduler.get('builderNames'):
                if scheduler.get('name') in self.DUPLICATED_TRIGGERS and builder_to_schduler_map.get(buildername):
                    self.assertTrue(builder_to_schduler_map[buildername] in self.DUPLICATED_TRIGGERS)
                else:
                    self.assertTrue(buildername not in builder_to_schduler_map, 'builder {} appears multiple times in schedulers.'.format(buildername))
                builder_to_schduler_map[buildername] = scheduler.get('name')

    def test_schduler_contains_valid_builder_name(self):
        config = self.get_config()
        builder_name_list = [builder['name'] for builder in config['builders']]
        for scheduler in config.get('schedulers'):
            for buildername in scheduler.get('builderNames'):
                self.assertTrue(buildername in builder_name_list, 'builder "{}" in scheduler "{}" is invalid.'.format(buildername, scheduler['name']))

    def test_single_builder_for_triggerable_scheduler(self):
        config = self.get_config()
        for scheduler in config['schedulers']:
            if scheduler.get('type') == 'Triggerable':
                self.assertTrue(len(scheduler.get('builderNames')) == 1, 'scheduler "{}" triggers multiple builders.'.format(scheduler['name']))

    def test_incorrect_triggered_by(self):
        config = self.get_config()
        schedulers_to_buildername_map = {}
        for scheduler in config['schedulers']:
            schedulers_to_buildername_map[scheduler['name']] = scheduler['builderNames']

        for builder in config['builders']:
            for key, value in builder.items():
                if key == 'triggered_by':
                    self.assertTrue(len(value) == 1, 'triggered_by "{}" is invalid, it should contain a single trigger.'.format(value))
                    self.assertTrue(value[0] in list(schedulers_to_buildername_map.keys()),
                                    'triggered_by "{}" for builder "{}" is not listed in schedulers section.'.format(value[0], builder['name']))

                    # Ensure that the triggered_by is correct, verify by matching that the builder for the triggered_by scheduler actually triggers current builder
                    triggered_by = value[0]
                    triggering_builder_name = schedulers_to_buildername_map.get(triggered_by)[0]
                    triggering_builder = self.get_builder_from_config(config, triggering_builder_name)
                    self.assertTrue(triggering_builder, 'builder "{}" in scheduler "{}" is invalid.'.format(triggering_builder_name, triggered_by))
                    triggering_builder_triggers = triggering_builder.get('triggers')
                    triggering_builder_triggers_buildernames = [schedulers_to_buildername_map[scheduler][0] for scheduler in triggering_builder_triggers]
                    self.assertTrue(builder['name'] in triggering_builder_triggers_buildernames,
                                    'Incorrect triggered_by "{}" in builder "{}", this builder is not in corresponding builder triggers "{}".'
                                    .format(triggered_by, builder['name'], triggering_builder_triggers_buildernames))

    def test_rebuild_without_change_only_on_builder_queues(self):
        config = self.get_config()
        for builder in config['builders']:
            if builder.get('rebuild_without_change_on_builder'):
                self.assertTrue(builder.get('triggers'), f'rebuild_without_change_on_builder should only be set on builders with triggers, but found on: {builder["name"]}')

    def test_deployment_target_on_intended_queues(self):
        config = self.get_config()
        for builder in config['builders']:
            if builder.get('deployment_target'):
                self.assertTrue(builder.get('factory') == 'macOSBuildFactory',
                                f'custom deployment target only implemented for macOS builders, but found on "{builder["name"]}" with factory "{builder["factory"]}"')


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
        self.verifyTags('iOS-EWS', ['iOS'])
        self.verifyTags('iOS_iOS', ['iOS'])
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

    def test_rebuild_without_change_on_builder_only_on_builders(self):
        with self.assertRaises(Exception) as context:
            loadConfig.checkValidBuilder({}, {'name': 'macOS-Tests-EWS', 'shortname': 'mac-tests', 'configuration': 'release', 'factory': 'TestFactory', 'platform': 'mac-sierra', 'rebuild_without_change_on_builder': True})
        self.assertEqual(context.exception.args, ('rebuild_without_change_on_builder can only be set on builders with triggers. Builder: macOS-Tests-EWS',))

    def test_rebuild_without_change_on_builder_valid(self):
        config = {'schedulers': [{'name': 'some-trigger', 'type': 'Triggerable'}]}
        loadConfig.checkValidBuilder(config, {'name': 'macOS-Build-EWS', 'shortname': 'mac', 'configuration': 'release', 'factory': 'BuildFactory', 'platform': 'mac-sierra', 'rebuild_without_change_on_builder': True, 'triggers': ['some-trigger']})

    def test_builder_with_invalid_safe_merge_queue(self):
        with self.assertRaises(Exception) as context:
            loadConfig.checkValidBuilder({}, {'name': 'mac-wk2', 'shortname': 'mac-wk2', 'configuration': 'release', 'factory': 'WK2Factory', 'platform': 'mac-sierra', 'safe_merge_queue': 'bogus'})
        self.assertIn('has invalid safe_merge_queue value: bogus', context.exception.args[0])

    def test_builder_with_valid_safe_merge_queue(self):
        loadConfig.checkValidBuilder({}, {'name': 'macOS-WK2-EWS', 'shortname': 'mac-wk2', 'configuration': 'release', 'factory': 'WK2Factory', 'platform': 'mac-sierra', 'safe_merge_queue': 'macos'})

    def test_builder_with_invalid_pr_column(self):
        with self.assertRaises(Exception) as context:
            loadConfig.checkValidBuilder({}, {'name': 'mac-wk2', 'shortname': 'mac-wk2', 'configuration': 'release', 'factory': 'WK2Factory', 'platform': 'mac-sierra', 'pr_column': 'bogus'})
        self.assertIn('has invalid pr_column value: bogus', context.exception.args[0])

    def test_builder_with_valid_pr_column(self):
        loadConfig.checkValidBuilder({}, {'name': 'macOS-WK2-EWS', 'shortname': 'mac-wk2', 'configuration': 'release', 'factory': 'WK2Factory', 'platform': 'mac-sierra', 'pr_column': 'macos'})


class TestcheckValidStatusBubbleOrder(unittest.TestCase):
    def _builder(self, shortname):
        return {'name': '{}-EWS'.format(shortname), 'shortname': shortname}

    def test_missing_status_bubble_order(self):
        config = {'builders': [self._builder('mac')]}
        with self.assertRaises(Exception) as context:
            loadConfig.checkValidStatusBubbleOrder(config)
        self.assertIn('missing top-level "status_bubble_order"', context.exception.args[0])

    def test_duplicate_entry(self):
        config = {'builders': [self._builder('mac')], 'status_bubble_order': ['mac', 'mac']}
        with self.assertRaises(Exception) as context:
            loadConfig.checkValidStatusBubbleOrder(config)
        self.assertIn('Duplicate entry in status_bubble_order: mac', context.exception.args[0])

    def test_unknown_shortname(self):
        config = {'builders': [self._builder('mac')], 'status_bubble_order': ['mac', 'ghost']}
        with self.assertRaises(Exception) as context:
            loadConfig.checkValidStatusBubbleOrder(config)
        self.assertIn("unknown shortnames: ['ghost']", context.exception.args[0])

    def test_missing_coverage(self):
        config = {'builders': [self._builder('mac'), self._builder('ios')], 'status_bubble_order': ['mac']}
        with self.assertRaises(Exception) as context:
            loadConfig.checkValidStatusBubbleOrder(config)
        self.assertIn("missing builder shortnames: ['ios']", context.exception.args[0])

    def test_handler_queues_are_exempt_from_coverage(self):
        config = {
            'builders': [self._builder('mac'), self._builder('commit'), self._builder('merge'), self._builder('unsafe-merge'), self._builder('safe-merge')],
            'status_bubble_order': ['mac'],
        }
        loadConfig.checkValidStatusBubbleOrder(config)

    def test_success(self):
        config = {'builders': [self._builder('mac'), self._builder('ios')], 'status_bubble_order': ['mac', 'ios']}
        loadConfig.checkValidStatusBubbleOrder(config)


class TestcheckValidPrCommentStructure(unittest.TestCase):
    def _builder(self, shortname, pr_column='misc'):
        b = {'name': '{}-EWS'.format(shortname), 'shortname': shortname}
        if pr_column is not None:
            b['pr_column'] = pr_column
        return b

    def test_missing_pr_column_on_non_handler(self):
        config = {'builders': [self._builder('mac', pr_column=None)], 'pr_comment_misc_handlers': []}
        with self.assertRaises(Exception) as context:
            loadConfig.checkValidPrCommentStructure(config)
        self.assertIn("missing required pr_column: ['mac']", context.exception.args[0])

    def test_handler_queues_are_exempt(self):
        config = {
            'builders': [self._builder('mac'), self._builder('merge', pr_column=None), self._builder('unsafe-merge', pr_column=None)],
            'pr_comment_misc_handlers': ['merge', 'unsafe-merge'],
        }
        loadConfig.checkValidPrCommentStructure(config)

    def test_handler_in_list_must_be_known_handler(self):
        config = {
            'builders': [self._builder('mac'), self._builder('merge', pr_column=None)],
            'pr_comment_misc_handlers': ['mac'],
        }
        with self.assertRaises(Exception) as context:
            loadConfig.checkValidPrCommentStructure(config)
        self.assertIn('pr_comment_misc_handlers entry mac is not a handler queue', context.exception.args[0])

    def test_handler_must_exist_as_builder(self):
        config = {
            'builders': [self._builder('mac')],
            'pr_comment_misc_handlers': ['merge'],
        }
        with self.assertRaises(Exception) as context:
            loadConfig.checkValidPrCommentStructure(config)
        self.assertIn('pr_comment_misc_handlers entry merge does not correspond to any builder', context.exception.args[0])

    def test_handlers_must_be_a_list(self):
        config = {'builders': [self._builder('mac')], 'pr_comment_misc_handlers': 'merge'}
        with self.assertRaises(Exception) as context:
            loadConfig.checkValidPrCommentStructure(config)
        self.assertIn('pr_comment_misc_handlers must be a list', context.exception.args[0])

    def test_success(self):
        config = {
            'builders': [self._builder('mac', 'macos'), self._builder('merge', pr_column=None), self._builder('unsafe-merge', pr_column=None)],
            'pr_comment_misc_handlers': ['merge', 'unsafe-merge'],
        }
        loadConfig.checkValidPrCommentStructure(config)


class TestQueueMetadata(unittest.TestCase):
    # Legacy hardcoded lists, kept here as drift guards against accidental config.json edits.
    # Remove once the derived values are trusted.
    LEGACY_STATUS_BUBBLE_ORDER = [
        'style', 'ios', 'ios-sim', 'mac', 'mac-AS-debug', 'vision', 'vision-sim', 'tv', 'tv-sim',
        'watch', 'watch-sim', 'gtk', 'wpe', 'gtk3-libwebrtc', 'playstation', 'win', 'win-tests',
        'ios-wk2', 'ios-wk2-wpt', 'mac-wk1', 'mac-wk2', 'mac-wk2-stress', 'mac-intel-wk2',
        'mac-AS-debug-wk2', 'mac-safer-cpp', 'ios-safer-cpp', 'vision-wk2', 'gtk-wk2', 'wpe-wk2',
        'api-ios', 'api-mac', 'api-mac-debug', 'api-gtk', 'api-wpe', 'bindings', 'jsc-x86-64',
        'jsc-debug-arm64', 'jsc-armv7', 'jsc-armv7-tests', 'webkitperl', 'webkitpy', 'services',
    ]
    LEGACY_EMBEDDED = ['ios', 'ios-safer-cpp', 'ios-sim', 'ios-wk2', 'ios-wk2-wpt', 'api-ios', 'vision', 'vision-sim', 'vision-wk2', 'tv', 'tv-sim', 'watch', 'watch-sim']
    LEGACY_MACOS = ['mac', 'mac-AS-debug', 'api-mac', 'api-mac-debug', 'mac-wk1', 'mac-wk2', 'mac-AS-debug-wk2', 'mac-wk2-stress', 'mac-safer-cpp', 'jsc-x86-64', 'jsc-debug-arm64']
    LEGACY_LINUX = ['gtk', 'gtk-wk2', 'api-gtk', 'wpe', 'gtk3-libwebrtc', 'wpe-wk2', 'api-wpe']
    LEGACY_WINDOWS = ['win']

    LEGACY_PR_COLUMN = {
        'style': 'misc', 'bindings': 'misc', 'webkitperl': 'misc', 'webkitpy': 'misc',
        'jsc-x86-64': 'misc', 'jsc-debug-arm64': 'misc', 'services': 'misc',
        'ios': 'embedded', 'ios-sim': 'embedded', 'ios-wk2': 'embedded', 'ios-wk2-wpt': 'embedded',
        'api-ios': 'embedded', 'ios-safer-cpp': 'embedded', 'vision': 'embedded',
        'vision-sim': 'embedded', 'vision-wk2': 'embedded', 'tv': 'embedded', 'tv-sim': 'embedded',
        'watch': 'embedded', 'watch-sim': 'embedded',
        'mac': 'macos', 'mac-AS-debug': 'macos', 'api-mac': 'macos', 'api-mac-debug': 'macos',
        'mac-wk1': 'macos', 'mac-wk2': 'macos', 'mac-AS-debug-wk2': 'macos',
        'mac-wk2-stress': 'macos', 'mac-intel-wk2': 'macos', 'mac-safer-cpp': 'macos',
        'wpe': 'linux', 'wpe-wk2': 'linux', 'api-wpe': 'linux', 'gtk3-libwebrtc': 'linux',
        'gtk': 'linux', 'gtk-wk2': 'linux', 'api-gtk': 'linux', 'playstation': 'linux',
        'jsc-armv7': 'linux', 'jsc-armv7-tests': 'linux',
        'win': 'windows', 'win-tests': 'windows',
    }
    LEGACY_MISC_HANDLERS = ['merge', 'unsafe-merge']

    def setUp(self):
        cwd = os.path.dirname(os.path.abspath(__file__))
        loadConfig.loadBuilderConfig({}, is_test_mode_enabled=True, master_prefix_path=cwd)

    def test_status_bubble_order_matches_legacy(self):
        self.assertEqual(loadConfig.STATUS_BUBBLE_ORDER, self.LEGACY_STATUS_BUBBLE_ORDER)

    def test_safe_merge_buckets_match_legacy(self):
        self.assertEqual(set(loadConfig.SAFE_MERGE_BUCKETS['embedded']), set(self.LEGACY_EMBEDDED))
        self.assertEqual(set(loadConfig.SAFE_MERGE_BUCKETS['macos']), set(self.LEGACY_MACOS))
        self.assertEqual(set(loadConfig.SAFE_MERGE_BUCKETS['linux']), set(self.LEGACY_LINUX))
        self.assertEqual(set(loadConfig.SAFE_MERGE_BUCKETS['windows']), set(self.LEGACY_WINDOWS))

    def test_pr_column_by_shortname_matches_legacy(self):
        self.assertEqual(loadConfig.PR_COLUMN_BY_SHORTNAME, self.LEGACY_PR_COLUMN)

    def test_pr_comment_misc_handlers_matches_legacy(self):
        self.assertEqual(loadConfig.PR_COMMENT_MISC_HANDLERS, self.LEGACY_MISC_HANDLERS)


class TestcheckWorkersAndBuildersForConsistency(unittest.TestCase):
    def __init__(self, *args, **kwargs):
        self.WK2Builder = {'name': 'macOS-High-Sierra-WK2-EWS', 'shortname': 'mac-wk2', 'factory': 'WK2Factory', 'platform': 'mac-sierra', 'workernames': ['ews101', 'ews102']}
        self.ews101 = {'name': 'ews101', 'platform': 'mac-sierra'}
        self.ews102 = {'name': 'ews102', 'platform': 'ios-11'}
        super().__init__(*args, **kwargs)

    def test_checkWorkersAndBuildersForConsistency(self):
        with self.assertRaises(Exception) as context:
            loadConfig.checkWorkersAndBuildersForConsistency({}, [], [self.WK2Builder])
        self.assertEqual(context.exception.args, ('Builder macOS-High-Sierra-WK2-EWS has worker ews101, which is not defined in workers list!',))

    def test_checkWorkersAndBuildersForConsistency1(self):
        with self.assertRaises(Exception) as context:
            loadConfig.checkWorkersAndBuildersForConsistency({}, [self.ews101, self.ews102], [self.WK2Builder])
        self.assertEqual(context.exception.args, ('Builder "macOS-High-Sierra-WK2-EWS" is for platform "mac-sierra", but has worker "ews102" for platform "ios-11"!',))

    def test_duplicate_worker(self):
        with self.assertRaises(Exception) as context:
            loadConfig.checkWorkersAndBuildersForConsistency({}, [self.ews101, self.ews101], [self.WK2Builder])
        self.assertEqual(context.exception.args, ('Duplicate worker entry found for ews101.',))

    def test_success(self):
        loadConfig.checkWorkersAndBuildersForConsistency({}, [self.ews101, {'name': 'ews102', 'platform': 'mac-sierra'}], [self.WK2Builder])


class TestPrioritizeBuilders(unittest.TestCase):
    class MockBuilder(object):
        def __init__(self, name, building=False, oldestRequestTime=None):
            self.name = name
            self.building = building
            self.old_building = False
            self._oldestRequestTime = oldestRequestTime or datetime.now(timezone.utc)

        def getOldestRequestTime(self):
            return self._oldestRequestTime

    def test_builders_over_testers(self):
        builders = [
            self.MockBuilder('macOS-BigSur-Debug-Build-EWS'),
            self.MockBuilder('macOS-BigSur-Debug-WK1-Tests-EWS'),
            self.MockBuilder('macOS-BigSur-Release-Build-EWS'),
        ]
        sorted_builders = loadConfig.prioritizeBuilders(None, builders)
        self.assertEqual(
            ['macOS-BigSur-Debug-Build-EWS', 'macOS-BigSur-Release-Build-EWS', 'macOS-BigSur-Debug-WK1-Tests-EWS'],
            [builder.name for builder in sorted_builders],
        )

    def test_starvation(self):
        builders = [
            self.MockBuilder('Commit-Queue', oldestRequestTime=datetime.now(timezone.utc) - timedelta(seconds=30)),
            self.MockBuilder('Merge-Queue', oldestRequestTime=datetime.now(timezone.utc) - timedelta(seconds=10)),
            self.MockBuilder('Unsafe-Merge-Queue', oldestRequestTime=datetime.now(timezone.utc) - timedelta(seconds=60)),
        ]
        sorted_builders = loadConfig.prioritizeBuilders(None, builders)
        self.assertEqual(
            ['Unsafe-Merge-Queue', 'Commit-Queue', 'Merge-Queue'],
            [builder.name for builder in sorted_builders],
        )

    def test_starvation_prioritize_commit_queue(self):
        builders = [
            self.MockBuilder('Commit-Queue', oldestRequestTime=datetime.now(timezone.utc) - timedelta(seconds=10)),
            self.MockBuilder('Merge-Queue', oldestRequestTime=datetime.now(timezone.utc) - timedelta(seconds=60)),
            self.MockBuilder('Unsafe-Merge-Queue', oldestRequestTime=datetime.now(timezone.utc) - timedelta(seconds=20)),
        ]
        sorted_builders = loadConfig.prioritizeBuilders(None, builders)
        self.assertEqual(
            ['Unsafe-Merge-Queue', 'Commit-Queue', 'Merge-Queue'],
            [builder.name for builder in sorted_builders],
        )


if __name__ == '__main__':
    unittest.main()
