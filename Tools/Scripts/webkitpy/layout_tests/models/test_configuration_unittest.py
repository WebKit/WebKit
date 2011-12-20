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

from webkitpy.common.host_mock import MockHost

from webkitpy.layout_tests.models.test_configuration import *


def make_mock_all_test_configurations_set():
    all_test_configurations = set()
    for version, architecture in (('snowleopard', 'x86'), ('xp', 'x86'), ('win7', 'x86'), ('lucid', 'x86'), ('lucid', 'x86_64')):
        for build_type in ('debug', 'release'):
            for graphics_type in ('cpu', 'gpu'):
                all_test_configurations.add(TestConfiguration(version, architecture, build_type, graphics_type))
    return all_test_configurations

MOCK_MACROS = {
    'mac': ['snowleopard'],
    'win': ['xp', 'win7'],
    'linux': ['lucid'],
}


class TestConfigurationTest(unittest.TestCase):
    def test_items(self):
        config = TestConfiguration('xp', 'x86', 'release', 'cpu')
        result_config_dict = {}
        for category, specifier in config.items():
            result_config_dict[category] = specifier
        self.assertEquals({'version': 'xp', 'architecture': 'x86', 'build_type': 'release', 'graphics_type': 'cpu'}, result_config_dict)

    def test_keys(self):
        config = TestConfiguration('xp', 'x86', 'release', 'cpu')
        result_config_keys = []
        for category in config.keys():
            result_config_keys.append(category)
        self.assertEquals(set(['graphics_type', 'version', 'architecture', 'build_type']), set(result_config_keys))

    def test_str(self):
        config = TestConfiguration('xp', 'x86', 'release', 'cpu')
        self.assertEquals('<xp, x86, release, cpu>', str(config))

    def test_repr(self):
        config = TestConfiguration('xp', 'x86', 'release', 'cpu')
        self.assertEquals("TestConfig(version='xp', architecture='x86', build_type='release', graphics_type='cpu')", repr(config))

    def test_hash(self):
        config_dict = {}
        config_dict[TestConfiguration('xp', 'x86', 'release', 'cpu')] = True
        self.assertTrue(TestConfiguration('xp', 'x86', 'release', 'cpu') in config_dict)
        self.assertTrue(config_dict[TestConfiguration('xp', 'x86', 'release', 'cpu')])
        config_dict[TestConfiguration('xp', 'x86', 'release', 'gpu')] = False
        self.assertFalse(config_dict[TestConfiguration('xp', 'x86', 'release', 'gpu')])

        def query_unknown_key():
            config_dict[TestConfiguration('xp', 'x86', 'debug', 'gpu')]

        self.assertRaises(KeyError, query_unknown_key)
        self.assertTrue(TestConfiguration('xp', 'x86', 'release', 'gpu') in config_dict)
        self.assertFalse(TestConfiguration('xp', 'x86', 'debug', 'gpu') in config_dict)
        configs_list = [TestConfiguration('xp', 'x86', 'release', 'gpu'), TestConfiguration('xp', 'x86', 'debug', 'gpu'), TestConfiguration('xp', 'x86', 'debug', 'gpu')]
        self.assertEquals(len(configs_list), 3)
        self.assertEquals(len(set(configs_list)), 2)

    def test_eq(self):
        self.assertEquals(TestConfiguration('xp', 'x86', 'release', 'cpu'), TestConfiguration('xp', 'x86', 'release', 'cpu'))
        host = MockHost()
        test_port = host.port_factory.get('test-win-xp', None)
        self.assertNotEquals(TestConfiguration('xp', 'x86', 'release', 'gpu'), TestConfiguration('xp', 'x86', 'release', 'cpu'))
        self.assertNotEquals(TestConfiguration('xp', 'x86', 'debug', 'cpu'), TestConfiguration('xp', 'x86', 'release', 'cpu'))

    def test_values(self):
        config = TestConfiguration('xp', 'x86', 'release', 'cpu')
        result_config_values = []
        for value in config.values():
            result_config_values.append(value)
        self.assertEquals(set(['xp', 'x86', 'release', 'cpu']), set(result_config_values))


class SpecifierSorterTest(unittest.TestCase):
    def __init__(self, testFunc):
        self._all_test_configurations = make_mock_all_test_configurations_set()
        unittest.TestCase.__init__(self, testFunc)

    def test_init(self):
        sorter = SpecifierSorter()
        self.assertEquals(sorter.category_for_specifier('control'), None)
        sorter = SpecifierSorter(self._all_test_configurations)
        self.assertEquals(sorter.category_for_specifier('xp'), 'version')
        sorter = SpecifierSorter(self._all_test_configurations, MOCK_MACROS)
        self.assertEquals(sorter.category_for_specifier('mac'), 'version')

    def test_add_specifier(self):
        sorter = SpecifierSorter()
        self.assertEquals(sorter.category_for_specifier('control'), None)
        sorter.add_specifier('version', 'control')
        self.assertEquals(sorter.category_for_specifier('control'), 'version')
        sorter.add_specifier('version', 'one')
        self.assertEquals(sorter.category_for_specifier('one'), 'version')
        sorter.add_specifier('architecture', 'renaissance')
        self.assertEquals(sorter.category_for_specifier('one'), 'version')
        self.assertEquals(sorter.category_for_specifier('renaissance'), 'architecture')

    def test_add_macros(self):
        sorter = SpecifierSorter(self._all_test_configurations)
        sorter.add_macros(MOCK_MACROS)
        self.assertEquals(sorter.category_for_specifier('mac'), 'version')
        self.assertEquals(sorter.category_for_specifier('win'), 'version')
        self.assertEquals(sorter.category_for_specifier('x86'), 'architecture')

    def test_category_priority(self):
        sorter = SpecifierSorter(self._all_test_configurations)
        self.assertEquals(sorter.category_priority('version'), 0)
        self.assertEquals(sorter.category_priority('build_type'), 2)

    def test_specifier_priority(self):
        sorter = SpecifierSorter(self._all_test_configurations)
        self.assertEquals(sorter.specifier_priority('x86'), 1)
        self.assertEquals(sorter.specifier_priority('gpu'), 3)
        self.assertEquals(sorter.specifier_priority('snowleopard'), 0)

    def test_sort_specifiers(self):
        sorter = SpecifierSorter(self._all_test_configurations, MOCK_MACROS)
        self.assertEquals(sorter.sort_specifiers(set()), [])
        self.assertEquals(sorter.sort_specifiers(set(['x86'])), ['x86'])
        self.assertEquals(sorter.sort_specifiers(set(['x86', 'win7'])), ['win7', 'x86'])
        self.assertEquals(sorter.sort_specifiers(set(['gpu', 'x86', 'debug', 'win7'])), ['win7', 'x86', 'debug', 'gpu'])
        self.assertEquals(sorter.sort_specifiers(set(['gpu', 'snowleopard', 'x86', 'debug', 'win7'])), ['snowleopard', 'win7', 'x86', 'debug', 'gpu'])
        self.assertEquals(sorter.sort_specifiers(set(['gpu', 'x86', 'mac', 'debug', 'win7'])), ['mac', 'win7', 'x86', 'debug', 'gpu'])


class TestConfigurationConverterTest(unittest.TestCase):
    def __init__(self, testFunc):
        self._all_test_configurations = make_mock_all_test_configurations_set()
        unittest.TestCase.__init__(self, testFunc)

    def test_to_config_set(self):
        converter = TestConfigurationConverter(self._all_test_configurations)

        self.assertEquals(converter.to_config_set(set()), self._all_test_configurations)

        self.assertEquals(converter.to_config_set(set(['foo'])), set())

        self.assertEquals(converter.to_config_set(set(['xp', 'foo'])), set())

        errors = []
        self.assertEquals(converter.to_config_set(set(['xp', 'foo']), errors), set())
        self.assertEquals(errors, ["Unrecognized modifier 'foo'"])

        self.assertEquals(converter.to_config_set(set(['xp', 'x86_64'])), set())

        configs_to_match = set([
            TestConfiguration('xp', 'x86', 'release', 'gpu'),
            TestConfiguration('xp', 'x86', 'release', 'cpu'),
        ])
        self.assertEquals(converter.to_config_set(set(['xp', 'release'])), configs_to_match)

        configs_to_match = set([
            TestConfiguration('snowleopard', 'x86', 'release', 'gpu'),
            TestConfiguration('win7', 'x86', 'release', 'gpu'),
            TestConfiguration('xp', 'x86', 'release', 'gpu'),
            TestConfiguration('lucid', 'x86', 'release', 'gpu'),
            TestConfiguration('lucid', 'x86_64', 'release', 'gpu'),
       ])
        self.assertEquals(converter.to_config_set(set(['gpu', 'release'])), configs_to_match)

        configs_to_match = set([
             TestConfiguration('lucid', 'x86_64', 'release', 'gpu'),
             TestConfiguration('lucid', 'x86_64', 'debug', 'gpu'),
             TestConfiguration('lucid', 'x86_64', 'release', 'cpu'),
             TestConfiguration('lucid', 'x86_64', 'debug', 'cpu'),
        ])
        self.assertEquals(converter.to_config_set(set(['x86_64'])), configs_to_match)

        configs_to_match = set([
            TestConfiguration('lucid', 'x86_64', 'release', 'gpu'),
            TestConfiguration('lucid', 'x86_64', 'debug', 'gpu'),
            TestConfiguration('lucid', 'x86_64', 'release', 'cpu'),
            TestConfiguration('lucid', 'x86_64', 'debug', 'cpu'),
            TestConfiguration('lucid', 'x86', 'release', 'gpu'),
            TestConfiguration('lucid', 'x86', 'debug', 'gpu'),
            TestConfiguration('lucid', 'x86', 'release', 'cpu'),
            TestConfiguration('lucid', 'x86', 'debug', 'cpu'),
            TestConfiguration('snowleopard', 'x86', 'release', 'gpu'),
            TestConfiguration('snowleopard', 'x86', 'debug', 'gpu'),
            TestConfiguration('snowleopard', 'x86', 'release', 'cpu'),
            TestConfiguration('snowleopard', 'x86', 'debug', 'cpu'),
        ])
        self.assertEquals(converter.to_config_set(set(['lucid', 'snowleopard'])), configs_to_match)

        configs_to_match = set([
            TestConfiguration('lucid', 'x86', 'release', 'gpu'),
            TestConfiguration('lucid', 'x86', 'debug', 'gpu'),
            TestConfiguration('lucid', 'x86', 'release', 'cpu'),
            TestConfiguration('lucid', 'x86', 'debug', 'cpu'),
            TestConfiguration('snowleopard', 'x86', 'release', 'gpu'),
            TestConfiguration('snowleopard', 'x86', 'debug', 'gpu'),
            TestConfiguration('snowleopard', 'x86', 'release', 'cpu'),
            TestConfiguration('snowleopard', 'x86', 'debug', 'cpu'),
        ])
        self.assertEquals(converter.to_config_set(set(['lucid', 'snowleopard', 'x86'])), configs_to_match)

        configs_to_match = set([
            TestConfiguration('lucid', 'x86_64', 'release', 'cpu'),
            TestConfiguration('lucid', 'x86', 'release', 'cpu'),
            TestConfiguration('snowleopard', 'x86', 'release', 'cpu'),
        ])
        self.assertEquals(converter.to_config_set(set(['lucid', 'snowleopard', 'release', 'cpu'])), configs_to_match)

    def test_macro_expansion(self):
        converter = TestConfigurationConverter(self._all_test_configurations, MOCK_MACROS)

        configs_to_match = set([
            TestConfiguration('xp', 'x86', 'release', 'gpu'),
            TestConfiguration('xp', 'x86', 'release', 'cpu'),
            TestConfiguration('win7', 'x86', 'release', 'gpu'),
            TestConfiguration('win7', 'x86', 'release', 'cpu'),
        ])
        self.assertEquals(converter.to_config_set(set(['win', 'release'])), configs_to_match)

        configs_to_match = set([
            TestConfiguration('xp', 'x86', 'release', 'gpu'),
            TestConfiguration('win7', 'x86', 'release', 'gpu'),
            TestConfiguration('lucid', 'x86', 'release', 'gpu'),
            TestConfiguration('lucid', 'x86_64', 'release', 'gpu'),
        ])
        self.assertEquals(converter.to_config_set(set(['win', 'lucid', 'release', 'gpu'])), configs_to_match)

        configs_to_match = set([
            TestConfiguration('xp', 'x86', 'release', 'gpu'),
            TestConfiguration('win7', 'x86', 'release', 'gpu'),
            TestConfiguration('snowleopard', 'x86', 'release', 'gpu'),
        ])
        self.assertEquals(converter.to_config_set(set(['win', 'mac', 'release', 'gpu'])), configs_to_match)

    def test_to_specifier_lists(self):
        converter = TestConfigurationConverter(self._all_test_configurations)

        self.assertEquals(converter.to_specifiers_list(set(self._all_test_configurations)), [[]])
        self.assertEquals(converter.to_specifiers_list(set()), [])

        configs_to_match = set([
            TestConfiguration('xp', 'x86', 'release', 'gpu'),
            TestConfiguration('xp', 'x86', 'release', 'cpu'),
        ])
        self.assertEquals(converter.to_specifiers_list(configs_to_match), [set(['release', 'xp'])])

        configs_to_match = set([
            TestConfiguration('xp', 'x86', 'release', 'gpu'),
            TestConfiguration('xp', 'x86', 'release', 'cpu'),
            TestConfiguration('xp', 'x86', 'debug', 'gpu'),
            TestConfiguration('xp', 'x86', 'debug', 'cpu'),
        ])
        self.assertEquals(converter.to_specifiers_list(configs_to_match), [set(['xp'])])

        configs_to_match = set([
            TestConfiguration('xp', 'x86', 'release', 'gpu'),
            TestConfiguration('lucid', 'x86_64', 'debug', 'cpu'),
        ])
        self.assertEquals(converter.to_specifiers_list(configs_to_match), [set(['debug', 'x86_64', 'lucid', 'cpu']), set(['release', 'gpu', 'xp'])])

        configs_to_match = set([
            TestConfiguration('xp', 'x86', 'release', 'gpu'),
            TestConfiguration('xp', 'x86', 'release', 'cpu'),
            TestConfiguration('lucid', 'x86_64', 'debug', 'cpu'),
            TestConfiguration('lucid', 'x86', 'debug', 'cpu'),
            TestConfiguration('lucid', 'x86_64', 'debug', 'gpu'),
            TestConfiguration('lucid', 'x86', 'debug', 'gpu'),
        ])
        self.assertEquals(converter.to_specifiers_list(configs_to_match), [set(['release', 'xp']), set(['debug', 'lucid'])])

        configs_to_match = set([
            TestConfiguration('xp', 'x86', 'release', 'gpu'),
            TestConfiguration('snowleopard', 'x86', 'release', 'gpu'),
            TestConfiguration('win7', 'x86', 'release', 'gpu'),
            TestConfiguration('lucid', 'x86', 'release', 'gpu'),
            TestConfiguration('lucid', 'x86_64', 'release', 'gpu'),
        ])
        self.assertEquals(converter.to_specifiers_list(configs_to_match), [set(['release', 'gpu'])])

        configs_to_match = set([
            TestConfiguration('xp', 'x86', 'release', 'gpu'),
            TestConfiguration('snowleopard', 'x86', 'release', 'gpu'),
        ])
        self.assertEquals(converter.to_specifiers_list(configs_to_match), [set(['xp', 'snowleopard', 'release', 'gpu'])])

        configs_to_match = set([
            TestConfiguration('xp', 'x86', 'release', 'gpu'),
            TestConfiguration('snowleopard', 'x86', 'release', 'gpu'),
            TestConfiguration('win7', 'x86', 'release', 'gpu'),
            TestConfiguration('win7', 'x86', 'debug', 'gpu'),
            TestConfiguration('lucid', 'x86', 'release', 'gpu'),
        ])
        self.assertEquals(converter.to_specifiers_list(configs_to_match), [set(['release', 'gpu', 'lucid', 'x86']), set(['gpu', 'win7']), set(['release', 'gpu', 'xp', 'snowleopard'])])

    def test_macro_collapsing(self):
        macros = {'foo': ['bar', 'baz'], 'people': ['bob', 'alice', 'john']}

        specifiers_list = [set(['john', 'godzilla', 'bob', 'alice'])]
        TestConfigurationConverter.collapse_macros(macros, specifiers_list)
        self.assertEquals(specifiers_list, [set(['people', 'godzilla'])])

        specifiers_list = [set(['john', 'godzilla', 'alice'])]
        TestConfigurationConverter.collapse_macros(macros, specifiers_list)
        self.assertEquals(specifiers_list, [set(['john', 'godzilla', 'alice', 'godzilla'])])

        specifiers_list = [set(['bar', 'godzilla', 'baz', 'bob', 'alice', 'john'])]
        TestConfigurationConverter.collapse_macros(macros, specifiers_list)
        self.assertEquals(specifiers_list, [set(['foo', 'godzilla', 'people'])])

        specifiers_list = [set(['bar', 'godzilla', 'baz', 'bob']), set(['bar', 'baz']), set(['people', 'alice', 'bob', 'john'])]
        TestConfigurationConverter.collapse_macros(macros, specifiers_list)
        self.assertEquals(specifiers_list, [set(['bob', 'foo', 'godzilla']), set(['foo']), set(['people'])])

    def test_converter_macro_collapsing(self):
        converter = TestConfigurationConverter(self._all_test_configurations, MOCK_MACROS)

        configs_to_match = set([
            TestConfiguration('xp', 'x86', 'release', 'gpu'),
            TestConfiguration('xp', 'x86', 'release', 'cpu'),
            TestConfiguration('win7', 'x86', 'release', 'gpu'),
            TestConfiguration('win7', 'x86', 'release', 'cpu'),
        ])
        self.assertEquals(converter.to_specifiers_list(configs_to_match), [set(['win', 'release'])])

        configs_to_match = set([
            TestConfiguration('xp', 'x86', 'release', 'gpu'),
            TestConfiguration('win7', 'x86', 'release', 'gpu'),
            TestConfiguration('lucid', 'x86', 'release', 'gpu'),
            TestConfiguration('lucid', 'x86_64', 'release', 'gpu'),
        ])
        self.assertEquals(converter.to_specifiers_list(configs_to_match), [set(['win', 'linux', 'release', 'gpu'])])

        configs_to_match = set([
            TestConfiguration('xp', 'x86', 'release', 'gpu'),
            TestConfiguration('win7', 'x86', 'release', 'gpu'),
            TestConfiguration('snowleopard', 'x86', 'release', 'gpu'),
        ])
        self.assertEquals(converter.to_specifiers_list(configs_to_match), [set(['win', 'mac', 'release', 'gpu'])])

    def test_specifier_converter_access(self):
        specifier_sorter = TestConfigurationConverter(self._all_test_configurations, MOCK_MACROS).specifier_sorter()
        self.assertEquals(specifier_sorter.category_for_specifier('snowleopard'), 'version')
        self.assertEquals(specifier_sorter.category_for_specifier('mac'), 'version')
