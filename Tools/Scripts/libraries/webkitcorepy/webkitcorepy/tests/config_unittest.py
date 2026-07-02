# Copyright (C) 2026 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import io
import json
import unittest

from webkitcorepy.config import Config


class ConfigRenderTestCase(unittest.TestCase):
    def test_render_string(self):
        self.assertEqual(Config.render('hello'), 'hello')

    def test_render_number(self):
        self.assertEqual(Config.render(42), 42)

    def test_render_list(self):
        self.assertEqual(Config.render([1, 2, 3]), [1, 2, 3])

    def test_render_simple_dict_returns_config(self):
        result = Config.render({'key': 'value'})
        self.assertIsInstance(result, Config)
        self.assertDictEqual(result, {'key': 'value'})

    def test_render_dict_with_context_symbol_returns_config(self):
        result = Config.render({'$context': {}, 'key': 'value'})
        self.assertIsInstance(result, Config)
        self.assertDictEqual(result, {'key': 'value'})

    def test_render_context_symbol_excluded_from_result(self):
        result = Config.render({'$context': {'x': 1}, 'key': 'value'})
        self.assertDictEqual(result, {'key': 'value'})

    def test_render_context_symbol_values_available_in_keys(self):
        result = Config.render({
            '$context': {'greeting': 'hello'},
            'message': {'$eval': 'greeting'},
        })
        self.assertDictEqual(result, {'message': 'hello'})

    def test_render_external_context_available(self):
        result = Config.render({'value': {'$eval': 'x'}}, context={'x': 42})
        self.assertDictEqual(result, {'value': 42})

    def test_render_rendered_key_available_to_subsequent_keys(self):
        result = Config.render({
            '$context': {},
            'a': 1,
            'b': {'$eval': 'a + 1'},
        })
        self.assertDictEqual(result, {'a': 1, 'b': 2})

    def test_render_list_value_available_to_subsequent_keys(self):
        result = Config.render({
            '$context': {},
            'items': [10, 20, 30],
            'count': {'$eval': 'len(items)'},
        })
        self.assertDictEqual(result, {'items': [10, 20, 30], 'count': 3})

    def test_render_dict_value_available_to_subsequent_keys(self):
        result = Config.render({
            '$context': {},
            'info': {'name': 'webkit'},
            'label': {'$eval': 'info["name"]'},
        })
        self.assertDictEqual(result, {'info': {'name': 'webkit'}, 'label': 'webkit'})

    def test_render_does_not_mutate_external_context(self):
        original = {'x': 1}
        Config.render({'$context': {'y': 2}, 'z': 3}, context=original)
        self.assertDictEqual(original, {'x': 1})

    def test_render_context_symbol_rendered_with_templates(self):
        result = Config.render({
            '$context': {'doubled': {'$eval': 'x * 2'}},
            'value': {'$eval': 'doubled'},
        }, context={'x': 5})
        self.assertDictEqual(result, {'value': 10})


class ConfigLoadsTestCase(unittest.TestCase):
    def test_loads_yaml(self):
        result = Config.loads('key: value\n')
        self.assertIsInstance(result, Config)
        self.assertDictEqual(result, {'key': 'value'})

    def test_loads_yaml_explicit_mode(self):
        result = Config.loads('key: value\n', mode=Config.Mode.YAML)
        self.assertDictEqual(result, {'key': 'value'})

    def test_loads_json(self):
        result = Config.loads('{"key": "value"}', mode=Config.Mode.JSON)
        self.assertIsInstance(result, Config)
        self.assertDictEqual(result, {'key': 'value'})

    def test_loads_json_via_yaml(self):
        result = Config.loads('{"key": "value"}')
        self.assertDictEqual(result, {'key': 'value'})

    def test_loads_renders_templates(self):
        result = Config.loads('key: "${1 + 1}"\n')
        self.assertDictEqual(result, {'key': '2'})

    def test_loads_yaml_multikey(self):
        result = Config.loads('a: 1\nb: 2\n')
        self.assertDictEqual(result, {'a': 1, 'b': 2})

    def test_loads_json_multikey(self):
        result = Config.loads('{"a": 1, "b": 2}', mode=Config.Mode.JSON)
        self.assertDictEqual(result, {'a': 1, 'b': 2})


class ConfigLoadTestCase(unittest.TestCase):
    def test_load_yaml(self):
        result = Config.load(io.StringIO('key: value\n'))
        self.assertIsInstance(result, Config)
        self.assertDictEqual(result, {'key': 'value'})

    def test_load_json(self):
        result = Config.load(io.StringIO('{"key": "value"}'), mode=Config.Mode.JSON)
        self.assertIsInstance(result, Config)
        self.assertDictEqual(result, {'key': 'value'})

    def test_load_infers_json_from_extension(self):
        f = io.StringIO('{"key": "value"}')
        f.name = 'config.json'
        self.assertDictEqual(Config.load(f), {'key': 'value'})

    def test_load_infers_yaml_from_extension(self):
        for ext in ('.yaml', '.yml'):
            f = io.StringIO('key: value\n')
            f.name = f'config{ext}'
            self.assertDictEqual(Config.load(f), {'key': 'value'})

    def test_load_falls_back_to_yaml_without_extension(self):
        f = io.StringIO('key: value\n')
        f.name = 'config.txt'
        self.assertDictEqual(Config.load(f), {'key': 'value'})


class ConfigDumpsTestCase(unittest.TestCase):
    def test_dumps_yaml_default(self):
        result = Config(a=1, b=2).dumps()
        import yaml
        self.assertDictEqual(yaml.safe_load(result), {'a': 1, 'b': 2})

    def test_dumps_yaml_explicit_mode(self):
        result = Config(a=1, b=2).dumps(mode=Config.Mode.YAML)
        import yaml
        self.assertDictEqual(yaml.safe_load(result), {'a': 1, 'b': 2})

    def test_dumps_json(self):
        result = Config(a=1, b=2).dumps(mode=Config.Mode.JSON)
        self.assertDictEqual(json.loads(result), {'a': 1, 'b': 2})

    def test_dumps_json_indent(self):
        result = Config(key='value').dumps(mode=Config.Mode.JSON, indent=4)
        self.assertEqual(result, json.dumps({'key': 'value'}, indent=4))

    def test_dumps_roundtrip_yaml(self):
        original = Config(x=1, y='hello')
        self.assertDictEqual(Config.loads(original.dumps()), dict(original))

    def test_dumps_roundtrip_json(self):
        original = Config(x=1, y='hello')
        self.assertDictEqual(Config.loads(original.dumps(mode=Config.Mode.JSON), mode=Config.Mode.JSON), dict(original))


class ConfigPopTestCase(unittest.TestCase):
    def test_render_pop_subsequent_key_sees_reduced_list(self):
        result = Config.render({
            '$context': {},
            'items': [1, 2, 3],
            'first': {'$eval': 'pop(items)'},
            'count': {'$eval': 'len(items)'},
        })
        self.assertEqual(result['first'], 1)
        self.assertEqual(result['count'], 2)

    def test_render_pop_filter_matches_dict_items(self):
        result = Config.render({
            '$context': {},
            'items': [{'kind': 'a', 'val': 1}, {'kind': 'b', 'val': 2}, {'kind': 'a', 'val': 3}],
            'matched': {'$eval': "pop(items, \"kind == 'b'\")"},
            'count': {'$eval': 'len(items)'},
        })
        self.assertDictEqual(result['matched'], {'kind': 'b', 'val': 2})
        self.assertEqual(result['count'], 2)

    def test_render_pop_filter_no_match_raises(self):
        with self.assertRaises(ValueError):
            Config.render({
                '$context': {},
                'items': [{'kind': 'a'}],
                'matched': {'$eval': "pop(items, \"kind == 'b'\")"},
            })

    def test_render_pop_exhausted_list_raises(self):
        with self.assertRaises(IndexError):
            Config.render({
                '$context': {},
                'items': [1, 2],
                'a': {'$eval': 'pop(items)'},
                'b': {'$eval': 'pop(items)'},
                'c': {'$eval': 'pop(items)'},
            })

    def test_render_pop_filter_exhausted_list_raises(self):
        with self.assertRaises(ValueError):
            Config.render({
                '$context': {},
                'items': [{'kind': 'a'}],
                'first': {'$eval': "pop(items, \"kind == 'a'\")"},
                'second': {'$eval': "pop(items, \"kind == 'a'\")"},
            })
