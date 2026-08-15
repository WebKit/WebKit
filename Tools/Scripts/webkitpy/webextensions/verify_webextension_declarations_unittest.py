# Copyright (C) 2026 Google LLC. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.

import unittest


def diff_dictionary(dict_name, idl_dict, parse_func, all_parse_funcs):
    findings = []
    all_accepted_types = dict(parse_func['types'])
    all_required = set(parse_func['required'])
    for p_name in parse_func.get('parents', []):
        if p_name in all_parse_funcs:
            all_accepted_types.update(all_parse_funcs[p_name]['types'])
            all_required.update(all_parse_funcs[p_name]['required'])

    idl_members = idl_dict['members']
    for impl_key, impl_cls in all_accepted_types.items():
        if impl_key not in idl_members:
            findings.append(('MISSING', dict_name, impl_key))

    for idl_key in idl_members:
        if idl_key not in all_accepted_types:
            findings.append(('EXTRA', dict_name, idl_key))

    for key in set(idl_members.keys()).intersection(all_accepted_types.keys()):
        idl_m, impl_cls = idl_members[key], all_accepted_types[key]
        if impl_cls == 'NSString.class' and idl_m['type'] != 'DOMString':
            findings.append(('TYPE', dict_name, key))
        if (key in all_required) != idl_m['required']:
            findings.append(('OPTIONALITY', dict_name, key))

    return findings


class TestVerifyWebExtensionDeclarations(unittest.TestCase):
    def setUp(self):
        self.clean_idl_dict = {
            'members': {
                'url': {'type': 'DOMString', 'required': False},
                'active': {'type': 'boolean', 'required': False},
                'index': {'type': 'double', 'required': False},
            }
        }
        self.clean_parse_func = {
            'types': {
                'url': 'NSString.class',
                'active': '@YES.class',
                'index': 'NSNumber.class',
            },
            'required': set(),
            'parents': []
        }

    def test_clean_input_has_zero_findings(self):
        findings = diff_dictionary('TabCreateProperties', self.clean_idl_dict, self.clean_parse_func, {})
        self.assertEqual(findings, [])

    def test_catches_missing_key(self):
        broken_idl = {'members': {'active': {'type': 'boolean', 'required': False}}}
        findings = diff_dictionary('TabCreateProperties', broken_idl, self.clean_parse_func, {})
        kinds = [f[0] for f in findings]
        self.assertIn('MISSING', kinds)

    def test_catches_type_mismatch(self):
        broken_idl = {
            'members': {
                'url': {'type': 'boolean', 'required': False},
                'active': {'type': 'boolean', 'required': False},
                'index': {'type': 'double', 'required': False},
            }
        }
        findings = diff_dictionary('TabCreateProperties', broken_idl, self.clean_parse_func, {})
        kinds = [f[0] for f in findings]
        self.assertIn('TYPE', kinds)

    def test_catches_optionality_mismatch(self):
        broken_idl = {
            'members': {
                'url': {'type': 'DOMString', 'required': True},
                'active': {'type': 'boolean', 'required': False},
                'index': {'type': 'double', 'required': False},
            }
        }
        findings = diff_dictionary('TabCreateProperties', broken_idl, self.clean_parse_func, {})
        kinds = [f[0] for f in findings]
        self.assertIn('OPTIONALITY', kinds)


if __name__ == '__main__':
    unittest.main()
