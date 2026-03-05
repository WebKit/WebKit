#!/usr/bin/env python3
#
# Copyright (C) 2022-2023 Apple Inc. All rights reserved.
# Copyright (C) 2024-2026 Samuel Weinig <sam@webkit.org>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.

import argparse
import collections
import enum
import functools
import itertools
import json
import os
import re
import subprocess
import sys
import textwrap


def stringify_iterable(iterable):
    return (str(x) for x in iterable)

def quote_iterable(iterable, *, mark='"', suffix=''):
    return (f'{mark}{x}{mark}{suffix}' for x in iterable)

def count_iterable(iterable):
    return sum(1 for _ in iterable)

def compact(iterable):
    return filter(lambda value: value is not None, iterable)

def compact_map(function, iterable):
    return compact(map(function, iterable))

def flatten(list_to_flatten):
    flattened_list = []
    for element in list_to_flatten:
        if type(element) is list:
            flattened_list += element
        else:
            flattened_list += [element]
    return flattened_list


class Writer:
    def __init__(self, output):
        self.output = output
        self._indentation_level = 0

    TAB_SIZE = 4

    @property
    def _current_indent(self):
        return (self._indentation_level * Writer.TAB_SIZE) * ' '

    def write(self, text):
        self.output.write(self._current_indent)
        self.output.write(text)
        return self.newline()

    def write_block(self, text):
        self.output.write(textwrap.indent(textwrap.dedent(text), self._current_indent))
        return self.newline()

    def write_lines(self, iterable):
        for line in iterable:
            self.write(line)
        return self

    def newline(self):
        self.output.write(f'\n')
        return self

    class Indent:
        def __init__(self, writer):
            self.writer = writer

        def __enter__(self):
            self.writer._indentation_level += 1

        def __exit__(self, exc_type, exc_value, traceback):
            self.writer._indentation_level -= 1

    def indent(self):
        return Writer.Indent(self)


class Schema:
    class Entry:
        def __init__(self, key, *, allowed_types, default_value=None, required=False, convert_to=None):
            if default_value and required:
                raise Exception(f"Invalid Schema.Entry for '{key}'. Cannot specify both 'default_value' and 'required'.")

            self.key = key
            self.allowed_types = allowed_types
            self.default_value = default_value
            self.required = required
            self.convert_to = convert_to

    def __init__(self, *entries):
        self.entries = {}

        # Give the schema an implement entry called `comment`, allowing each group to have a top level comment.
        self.entries["comment"] = Schema.Entry("comment", allowed_types=[str])

        # For each entry, add it to the entries dictionary along with a support entry with key `*-comment` to
        # support all fields having an implicit `*-comment` field to go along with it.
        for entry in entries:
            self.entries[entry.key] = entry
            self.entries[entry.key + "-comment"] = Schema.Entry(entry.key + "-comment", allowed_types=[str])

    def __add__(self, other):
        return Schema(*list({**self.entries, **other.entries}.values()))

    def set_attributes_from_dictionary(self, dictionary, *, instance):
        for entry in self.entries.values():
            setattr(instance, entry.key.replace("-", "_"), dictionary.get(entry.key, entry.default_value))

    def validate_keys(self, parsing_context, key_path, dictionary, *, label):
        invalid_keys = list(filter(lambda key: key not in self.entries.keys(), dictionary.keys()))
        if len(invalid_keys) == 1:
            raise Exception(f"Invalid key for '{label} - {key_path}': {invalid_keys[0]}")
        if len(invalid_keys) > 1:
            raise Exception(f"Invalid keys for '{label} - {key_path}': {invalid_keys}")

    def validate_types(self, parsing_context, key_path, dictionary, *, label):
        for key, value in dictionary.items():
            if type(value) not in self.entries[key].allowed_types:
                raise Exception(f"Invalid type '{type(value)}' for key '{key}' in '{label} - {key_path}'. Expected type in set '{self.entries[key].allowed_types}'.")

    def validate_requirements(self, parsing_context, key_path, dictionary, *, label):
        for key, entry in self.entries.items():
            if entry.required and key not in dictionary:
                raise Exception(f"Required key '{key}' not found in '{label} - {key_path}'.")

    def apply_conversions(self, parsing_context, key_path, dictionary, *, label):
        for key, entry in self.entries.items():
            if entry.convert_to and key in dictionary:
                dictionary[key] = entry.convert_to.from_json(parsing_context, key_path, dictionary[key])

    def validate_dictionary(self, parsing_context, key_path, dictionary, *, label):
        self.validate_keys(parsing_context, key_path, dictionary, label=label)
        self.validate_types(parsing_context, key_path, dictionary, label=label)
        self.validate_requirements(parsing_context, key_path, dictionary, label=label)
        self.apply_conversions(parsing_context, key_path, dictionary, label=label)


class Name(object):
    special_case_name_to_id = {
        "url": "URL",
    }

    def __init__(self, name):
        self.name = name
        self.id_without_prefix = Name.convert_name_to_id(self.name)

    def __str__(self):
        return self.name

    def __repr__(self):
        return self.__str__()

    @staticmethod
    def convert_name_to_id(name):
        return Name.special_case_name_to_id.get(name) or re.sub(r'(^[^-])|-(.)', lambda m: (m[1] or m[2]).upper(), name).replace('.', '_').replace('(', '').replace(')', '')

    @property
    def id_without_prefix_with_lowercase_first_letter(self):
        return self.id_without_prefix[0].lower() + self.id_without_prefix[1:]


class PropertyName(Name):
    def __init__(self, name):
        super().__init__(name)

    def __str__(self):
        return self.name

    def __repr__(self):
        return self.__str__()

    @property
    def id_without_scope(self):
        return f"CSSProperty{self.id_without_prefix}"

    @property
    def id(self):
        return f"CSSPropertyID::CSSProperty{self.id_without_prefix}"

    @property
    def name_for_methods(self):
        return self.id_without_prefix.replace("Webkit", "")


class ValueKeywordName(Name):
    special_case_name_to_enum = {
        'WindRule': { 'Nonzero': 'NonZero', 'Evenodd': 'EvenOdd' },
        'FlexWrap': { 'Nowrap': 'NoWrap' },
        'TextDirection': { 'Ltr': 'LTR', 'Rtl': 'RTL' }
    }

    def __init__(self, name):
        super().__init__(name)

    def __str__(self):
        return self.name

    def __repr__(self):
        return self.__str__()

    def from_json(parsing_context, key_path, json_value):
        assert(type(json_value) is str)
        return ValueKeywordName(json_value)

    @property
    def id_without_scope(self):
        return f"CSSValue{self.id_without_prefix}"

    @property
    def id(self):
        return f"CSSValueID::CSSValue{self.id_without_prefix}"

    def cpp_enum_literal(self, base):
        override_id = ValueKeywordName.special_case_name_to_enum.get(base, {}).get(self.id_without_prefix)
        if override_id:
            return f"{base}::{override_id}"
        return f"{base}::{self.id_without_prefix}"

    @property
    def cpp_literal(self):
        return f"CSS::Keyword::{self.id_without_prefix} {{ }}"

    @property
    def requires_using_namespace_css_literals(self):
        return False


class NumericLiteral(object):
    class Kind(enum.Enum):
        NUMBER      = ''
        PERCENTAGE  = '%'
        PX          = 'px'
        S           = 's'
        MS          = 'ms'
        DEG         = 'deg'

        @staticmethod
        def from_suffix(suffix):
            for kind in NumericLiteral.Kind:
                if kind.value == suffix:
                    return kind
            raise Exception(f"Invalid numeric literal suffix: {suffix}")

    def __init__(self, string):
        match = re.fullmatch(r"(\d+)([a-z%]*)", string)
        if not match:
            raise Exception(f"Invalid numeric literal specified: {string}")

        digits, suffix = match.groups()
        self.digits = digits
        self.kind = NumericLiteral.Kind.from_suffix(suffix)

    def __str__(self):
        return f"NumericLiteral {self.digits}{self.kind.value}"

    def __repr__(self):
        return self.__str__()

    @property
    def cpp_unit_type(self):
        if self.kind == NumericLiteral.Kind.NUMBER:
            return f"CSSUnitType::CSS_NUMBER"
        elif self.kind == NumericLiteral.Kind.PERCENTAGE:
            return f"CSSUnitType::CSS_PERCENTAGE"
        else:
            return f"CSSUnitType::CSS_{self.kind.value.upper()}"

    @property
    def cpp_literal(self):
        if self.kind == NumericLiteral.Kind.NUMBER:
            return f"{self.digits}_css_number"
        elif self.kind == NumericLiteral.Kind.PERCENTAGE:
            return f"{self.digits}_css_percentage"
        else:
            return f"{self.digits}_css_{self.kind.value}"

    @property
    def requires_using_namespace_css_literals(self):
        return True


class SpecialLiteral(Name):
    def __init__(self, name):
        super().__init__(name)

    def __str__(self):
        return f"SpecialLiteral {self.name}"

    def __repr__(self):
        return self.__str__()

    @property
    def requires_using_namespace_css_literals(self):
        return False


class InitialValue(object):
    @staticmethod
    def process(element):
        if re.fullmatch(r"[A-Za-z-]+", element):
            return ValueKeywordName(element)
        elif re.fullmatch(r"\d+[a-z%]*", element):
            return NumericLiteral(element)
        elif re.fullmatch(r"@[A-Za-z-]+", element):
            return SpecialLiteral(element)
        else:
            raise Exception(f"Unknown element '{element}' in initial value")

    def __init__(self, string):
        self.string = string
        self.list = [InitialValue.process(element) for element in re.split(r",?\s+", string)]

    def __str__(self):
        return f"InitialValue {vars(self)}"

    def __repr__(self):
        return self.__str__()

    def __eq__(self, other):
        return self.string == other.string

    def __hash__(self):
        return hash(self.string)

    @property
    def requires_using_namespace_css_literals(self):
        return any(element.requires_using_namespace_css_literals for element in self.list)


class Status:
    schema = Schema(
        Schema.Entry("enabled-by-default", allowed_types=[bool]),
        Schema.Entry("status", allowed_types=[str]),
    )

    def __init__(self, **dictionary):
        Status.schema.set_attributes_from_dictionary(dictionary, instance=self)

    def __str__(self):
        return f"Status {vars(self)}"

    def __repr__(self):
        return self.__str__()

    @staticmethod
    def from_json(parsing_context, key_path, json_value):
        if type(json_value) is str:
            return Status(status=json_value)

        assert(type(json_value) is dict)
        Status.schema.validate_dictionary(parsing_context, f"{key_path}.status", json_value, label=f"Status")

        return Status(**json_value)


class Specification:
    schema = Schema(
        Schema.Entry("category", allowed_types=[str]),
        Schema.Entry("description", allowed_types=[str]),
        Schema.Entry("documentation-url", allowed_types=[str]),
        Schema.Entry("keywords", allowed_types=[list], default_value=[]),
        Schema.Entry("non-canonical-url", allowed_types=[str]),
        Schema.Entry("obsolete-category", allowed_types=[str]),
        Schema.Entry("obsolete-url", allowed_types=[str]),
        Schema.Entry("url", allowed_types=[str]),
    )

    def __init__(self, **dictionary):
        Specification.schema.set_attributes_from_dictionary(dictionary, instance=self)

    def __str__(self):
        return f"Specification {vars(self)}"

    def __repr__(self):
        return self.__str__()

    @staticmethod
    def from_json(parsing_context, key_path, json_value):
        assert(type(json_value) is dict)
        Specification.schema.validate_dictionary(parsing_context, f"{key_path}.specification", json_value, label=f"Specification")
        return Specification(**json_value)


class Value:
    schema = Schema(
        Schema.Entry("enable-if", allowed_types=[str]),
        Schema.Entry("settings-flag", allowed_types=[str]),
        Schema.Entry("status", allowed_types=[str]),
        Schema.Entry("url", allowed_types=[str]),
        Schema.Entry("value", allowed_types=[str], required=True),
    )

    def __init__(self, **dictionary):
        Value.schema.set_attributes_from_dictionary(dictionary, instance=self)
        self.value_keyword_name = ValueKeywordName(self.value)
        self.keyword_term = self._build_keyword_term()

    def _build_keyword_term(self):
        return KeywordTerm(self.value_keyword_name, comment=self.comment, settings_flag=self.settings_flag, status=self.status)

    def __str__(self):
        return f"Value {vars(self)}"

    def __repr__(self):
        return self.__str__()

    def __eq__(self, other):
        return self.value == other.value and self.settings_flag == other.settings_flag

    def __lt__(self, other):
        return self.value < other.value

    @staticmethod
    def from_json(parsing_context, key_path, json_value):
        if type(json_value) is str:
            return Value.from_json(parsing_context, key_path, {"value": json_value})

        assert(type(json_value) is dict)
        Value.schema.validate_dictionary(parsing_context, f"{key_path}.values", json_value, label=f"Value")

        if "enable-if" in json_value and not parsing_context.is_enabled(conditional=json_value["enable-if"]):
            if parsing_context.verbose:
                print(f"SKIPPED value {json_value['value']} in {key_path} due to failing to satisfy 'enable-if' condition, '{json_value['enable-if']}', with active macro set")
            return None

        if "status" in json_value and (json_value["status"] == "unimplemented" or json_value["status"] == "removed" or json_value["status"] == "not considering"):
            if parsing_context.verbose:
                print(f"SKIPPED value {json_value['value']} in {key_path} due to '{json_value['status']}' status designation.")
            return None

        if json_value.get("status", None) != "internal" and json_value["value"].startswith("-internal-"):
            raise Exception(f'Value "{json_value["value"]}" starts with "-internal-" but does not have "status": "internal" set.')

        return Value(**json_value)

    @property
    def id_without_prefix(self):
        return self.value_keyword_name.id_without_prefix

    @property
    def id_without_prefix_with_lowercase_first_letter(self):
        return self.value_keyword_name.id_without_prefix_with_lowercase_first_letter

    @property
    def id_without_scope(self):
        return self.value_keyword_name.id_without_scope

    @property
    def id(self):
        return self.value_keyword_name.id

    @property
    def name_for_methods(self):
        return self.value_keyword_name.name_for_methods

    @property
    def name(self):
        return self.value_keyword_name.name


class LogicalPropertyGroup:
    schema = Schema(
        Schema.Entry("name", allowed_types=[str], required=True),
        Schema.Entry("resolver", allowed_types=[str], required=True),
    )

    logical_property_group_resolvers = {
        "logical": {
            # Order matches LogicalBoxAxis enum in Source/WebCore/platform/BoxSides.h.
            "axis": ["inline", "block"],
            # Order matches LogicalBoxSide enum in Source/WebCore/platform/BoxSides.h.
            "side": ["block-start", "inline-end", "block-end", "inline-start"],
            # Order matches LogicalBoxCorner enum in Source/WebCore/platform/BoxSides.h.
            "corner": ["start-start", "start-end", "end-start", "end-end"],
        },
        "physical": {
            # Order matches BoxAxis enum in Source/WebCore/platform/BoxSides.h.
            "axis": ["horizontal", "vertical"],
            # Order matches BoxSide enum in Source/WebCore/platform/BoxSides.h.
            "side": ["top", "right", "bottom", "left"],
            # Order matches BoxCorner enum in Source/WebCore/platform/BoxSides.h.
            "corner": ["top-left", "top-right", "bottom-left", "bottom-right"],
        },
    }

    def __init__(self, **dictionary):
        LogicalPropertyGroup.schema.set_attributes_from_dictionary(dictionary, instance=self)
        self._update_kind_and_logic()

    def __str__(self):
        return f"LogicalPropertyGroup {vars(self)}"

    def __repr__(self):
        return self.__str__()

    def _update_kind_and_logic(self):
        for current_logic, current_resolvers_for_logic in LogicalPropertyGroup.logical_property_group_resolvers.items():
            for current_kind, resolver_list in current_resolvers_for_logic.items():
                for current_resolver in resolver_list:
                    if current_resolver == self.resolver:
                        self.kind = current_kind
                        self.logic = current_logic
                        return
        raise Exception(f"Unrecognized resolver \"{self.resolver}\"")

    @staticmethod
    def from_json(parsing_context, key_path, json_value):
        assert(type(json_value) is dict)
        LogicalPropertyGroup.schema.validate_dictionary(parsing_context, f"{key_path}.logical-property-group", json_value, label=f"LogicalPropertyGroup")
        return LogicalPropertyGroup(**json_value)


class Longhand:
    schema = Schema(
        Schema.Entry("enable-if", allowed_types=[str]),
        Schema.Entry("value", allowed_types=[str], required=True),
    )

    def __init__(self, **dictionary):
        Longhand.schema.set_attributes_from_dictionary(dictionary, instance=self)

    def __str__(self):
        return f"Longhand {vars(self)}"

    def __repr__(self):
        return self.__str__()

    @staticmethod
    def from_json(parsing_context, key_path, json_value):
        if type(json_value) is str:
            return Longhand.from_json(parsing_context, key_path, {"value": json_value})

        assert(type(json_value) is dict)
        Longhand.schema.validate_dictionary(parsing_context, f"{key_path}.longhands", json_value, label=f"Longhand")

        if "enable-if" in json_value and not parsing_context.is_enabled(conditional=json_value["enable-if"]):
            if parsing_context.verbose:
                print(f"SKIPPED longhand {json_value['value']} in {key_path} due to failing to satisfy 'enable-if' condition, '{json_value['enable-if']}', with active macro set")
            return None

        return Longhand(**json_value)


class StylePropertyCodeGenProperties:
    schema = Schema(
        Schema.Entry("accepts-quirky-angle", allowed_types=[bool], default_value=False),
        Schema.Entry("accepts-quirky-color", allowed_types=[bool], default_value=False),
        Schema.Entry("accepts-quirky-length", allowed_types=[bool], default_value=False),
        Schema.Entry("aliases", allowed_types=[list], default_value=[]),
        Schema.Entry("animation-wrapper-acceleration", allowed_types=[str]),
        Schema.Entry("animation-wrapper-requires-additional-parameters", allowed_types=[list], default_value=[]),
        Schema.Entry("animation-wrapper-requires-getter", allowed_types=[str]),
        Schema.Entry("animation-wrapper-requires-non-additive-or-cumulative-interpolation", allowed_types=[bool], default_value=False),
        Schema.Entry("animation-wrapper-requires-non-normalized-discrete-interpolation", allowed_types=[bool], default_value=False),
        Schema.Entry("animation-wrapper-requires-override-parameters", allowed_types=[list]),
        Schema.Entry("animation-wrapper-requires-setter", allowed_types=[str]),
        Schema.Entry("animation-wrapper", allowed_types=[str]),
        Schema.Entry("cascade-alias", allowed_types=[str]),
        Schema.Entry("color-property-traits-color-custom", allowed_types=[bool], default_value=False),
        Schema.Entry("color-property-traits-requires-excludes-visited-link-color", allowed_types=[bool], default_value=False),
        Schema.Entry("color-property-traits-requires-resolving-current-color", allowed_types=[bool], default_value=False),
        Schema.Entry("color-property-traits-visited-link-color-custom", allowed_types=[bool], default_value=False),
        Schema.Entry("color-property", allowed_types=[bool], default_value=False),
        Schema.Entry("computed-style-changed-for-animation-custom", allowed_types=[bool], default_value=False),
        Schema.Entry("computed-style-getter-constexpr", allowed_types=[bool], default_value=False),
        Schema.Entry("computed-style-getter-custom", allowed_types=[bool], default_value=False),
        Schema.Entry("computed-style-getter-exported", allowed_types=[bool], default_value=False),
        Schema.Entry("computed-style-getter-inline", allowed_types=[bool], default_value=True),
        Schema.Entry("computed-style-getter", allowed_types=[str]),
        Schema.Entry("computed-style-has-explicitly-set-getter-custom", allowed_types=[bool], default_value=False),
        Schema.Entry("computed-style-has-explicitly-set-policy", allowed_types=[str]),
        Schema.Entry("computed-style-has-explicitly-set-setter-custom", allowed_types=[bool], default_value=False),
        Schema.Entry("computed-style-has-explicitly-set-storage-container", allowed_types=[str], default_value='data'),
        Schema.Entry("computed-style-has-explicitly-set-storage-name", allowed_types=[str]),
        Schema.Entry("computed-style-has-explicitly-set-storage-path", allowed_types=[list]),
        Schema.Entry("computed-style-initial-constexpr", allowed_types=[bool], default_value=False),
        Schema.Entry("computed-style-initial-custom", allowed_types=[bool], default_value=False),
        Schema.Entry("computed-style-initial-exported", allowed_types=[bool], default_value=False),
        Schema.Entry("computed-style-initial-inline", allowed_types=[bool], default_value=True),
        Schema.Entry("computed-style-initial", allowed_types=[str]),
        Schema.Entry("computed-style-name-for-methods", allowed_types=[str]),
        Schema.Entry("computed-style-resolving-current-color-applying-color-filter-exported", allowed_types=[bool], default_value=False),
        Schema.Entry("computed-style-resolving-current-color-exported", allowed_types=[bool], default_value=False),
        Schema.Entry("computed-style-setter-constexpr", allowed_types=[bool], default_value=False),
        Schema.Entry("computed-style-setter-custom", allowed_types=[bool], default_value=False),
        Schema.Entry("computed-style-setter-exported", allowed_types=[bool], default_value=False),
        Schema.Entry("computed-style-setter-inline", allowed_types=[bool], default_value=True),
        Schema.Entry("computed-style-setter-requires-did-set", allowed_types=[bool], default_value=False),
        Schema.Entry("computed-style-setter-returns-if-changed", allowed_types=[bool], default_value=False),
        Schema.Entry("computed-style-setter", allowed_types=[str]),
        Schema.Entry("computed-style-storage-container", allowed_types=[str], default_value='data'),
        Schema.Entry("computed-style-storage-kind", allowed_types=[str]),
        Schema.Entry("computed-style-storage-name", allowed_types=[str]),
        Schema.Entry("computed-style-storage-path", allowed_types=[list]),
        Schema.Entry("computed-style-type", allowed_types=[str]),
        Schema.Entry("computed-style-visited-dependent-applying-color-filter-exported", allowed_types=[bool], default_value=False),
        Schema.Entry("computed-style-visited-dependent-exported", allowed_types=[bool], default_value=False),
        Schema.Entry("computed-style-visited-link-getter-custom", allowed_types=[bool], default_value=False),
        Schema.Entry("computed-style-visited-link-resolving-current-color-applying-color-filter-exported", allowed_types=[bool], default_value=False),
        Schema.Entry("computed-style-visited-link-resolving-current-color-exported", allowed_types=[bool], default_value=False),
        Schema.Entry("computed-style-visited-link-setter-custom", allowed_types=[bool], default_value=False),
        Schema.Entry("computed-style-visited-link-storage-container", allowed_types=[str], default_value='data'),
        Schema.Entry("computed-style-visited-link-storage-name", allowed_types=[str]),
        Schema.Entry("computed-style-visited-link-storage-path", allowed_types=[list]),
        Schema.Entry("coordinated-value-list-property-getter", allowed_types=[str]),
        Schema.Entry("coordinated-value-list-property-initial", allowed_types=[str]),
        Schema.Entry("coordinated-value-list-property-item-type", allowed_types=[str]),
        Schema.Entry("coordinated-value-list-property-name-for-methods", allowed_types=[str]),
        Schema.Entry("coordinated-value-list-property-setter", allowed_types=[str]),
        Schema.Entry("coordinated-value-list-property", allowed_types=[bool], default_value=False),
        Schema.Entry("disables-native-appearance", allowed_types=[bool], default_value=False),
        Schema.Entry("enable-if", allowed_types=[str]),
        Schema.Entry("fast-path-inherited", allowed_types=[bool], default_value=False),
        Schema.Entry("font-description-getter", allowed_types=[str]),
        Schema.Entry("font-description-initial", allowed_types=[str]),
        Schema.Entry("font-description-name-for-methods", allowed_types=[str]),
        Schema.Entry("font-description-setter", allowed_types=[str]),
        Schema.Entry("font-property", allowed_types=[bool], default_value=False),
        Schema.Entry("high-priority", allowed_types=[bool], default_value=False),
        Schema.Entry("internal-only", allowed_types=[bool], default_value=False),
        Schema.Entry("logical-property-group", allowed_types=[dict]),
        Schema.Entry("longhands", allowed_types=[list]),
        Schema.Entry("medium-priority", allowed_types=[bool], default_value=False),
        Schema.Entry("parser-exported", allowed_types=[bool]),
        Schema.Entry("parser-function-allows-number-or-integer-input", allowed_types=[bool], default_value=False),
        Schema.Entry("parser-function", allowed_types=[str]),
        Schema.Entry("parser-grammar-unused-reason", allowed_types=[str]),
        Schema.Entry("parser-grammar-unused", allowed_types=[str]),
        Schema.Entry("parser-grammar", allowed_types=[str]),
        Schema.Entry("parser-shorthand", allowed_types=[str]),
        Schema.Entry("separator", allowed_types=[str]),
        Schema.Entry("settings-flag", allowed_types=[str]),
        Schema.Entry("shorthand-parser-pattern", allowed_types=[str]),
        Schema.Entry("shorthand-pattern", allowed_types=[str]),
        Schema.Entry("shorthand-style-extractor-pattern", allowed_types=[str]),
        Schema.Entry("sink-priority", allowed_types=[bool], default_value=False),
        Schema.Entry("skip-codegen", allowed_types=[bool], default_value=False),
        Schema.Entry("skip-computed-style-getter", allowed_types=[bool], default_value=False),
        Schema.Entry("skip-computed-style-initial", allowed_types=[bool], default_value=False),
        Schema.Entry("skip-computed-style-setter", allowed_types=[bool], default_value=False),
        Schema.Entry("skip-computed-style", allowed_types=[bool], default_value=False),
        Schema.Entry("skip-parser", allowed_types=[bool], default_value=False),
        Schema.Entry("skip-render-style-getter", allowed_types=[bool], default_value=False),
        Schema.Entry("skip-render-style-setter", allowed_types=[bool], default_value=False),
        Schema.Entry("skip-render-style", allowed_types=[bool], default_value=False),
        Schema.Entry("skip-style-builder", allowed_types=[bool], default_value=False),
        Schema.Entry("skip-style-extractor", allowed_types=[bool], default_value=False),
        Schema.Entry("status", allowed_types=[str]),
        Schema.Entry("style-builder-custom", allowed_types=[str]),
        Schema.Entry("style-builder-requires-system-font-shorthand-check", allowed_types=[bool], default_value=False),
        Schema.Entry("style-extractor-custom", allowed_types=[bool], default_value=False),
        Schema.Entry("top-priority-reason", allowed_types=[str]),
        Schema.Entry("top-priority", allowed_types=[bool], default_value=False),
        Schema.Entry("url", allowed_types=[str]),
        Schema.Entry("visited-link-color-support", allowed_types=[bool], default_value=False),
    )

    def __init__(self, property_name, **dictionary):
        StylePropertyCodeGenProperties.schema.set_attributes_from_dictionary(dictionary, instance=self)
        self.property_name = property_name

    def __str__(self):
        return f"StylePropertyCodeGenProperties {vars(self)}"

    def __repr__(self):
        return self.__str__()

    @staticmethod
    def _complete_name_family(json_value, family_name, property_name):
        if f"{family_name}-name-for-methods" not in json_value:
            json_value[f"{family_name}-name-for-methods"] = property_name.name_for_methods

        if f"{family_name}-getter" not in json_value:
            json_value[f"{family_name}-getter"] = json_value[f"{family_name}-name-for-methods"][0].lower() + json_value[f"{family_name}-name-for-methods"][1:]

        if f"{family_name}-setter" not in json_value:
            json_value[f"{family_name}-setter"] = "set" + json_value[f"{family_name}-name-for-methods"]

        if f"{family_name}-initial" not in json_value:
            json_value[f"{family_name}-initial"] = f"initial" + json_value[f"{family_name}-name-for-methods"]

    @staticmethod
    def from_json(parsing_context, key_path, name, json_value):
        if type(json_value) is list:
            json_value = parsing_context.select_enabled_variant(json_value, label=f"{key_path}.codegen-properties")

        assert(type(json_value) is dict)
        StylePropertyCodeGenProperties.schema.validate_dictionary(parsing_context, f"{key_path}.codegen-properties", json_value, label=f"StylePropertyCodeGenProperties")

        property_name = PropertyName(name)

        StylePropertyCodeGenProperties._complete_name_family(json_value, "computed-style", property_name)

        if "font-property" in json_value:
            StylePropertyCodeGenProperties._complete_name_family(json_value, "font-description", property_name)

        if "coordinated-value-list-property" in json_value:
            StylePropertyCodeGenProperties._complete_name_family(json_value, "coordinated-value-list-property", property_name)

        if "animation-wrapper-acceleration" in json_value:
            if json_value["animation-wrapper-acceleration"] not in ['always', 'threaded-only']:
                raise Exception(f"{key_path} must be either 'always' or 'threaded-only'.")
            if json_value["animation-wrapper-acceleration"] == 'threaded-only' and not parsing_context.is_enabled(conditional="ENABLE_THREADED_ANIMATIONS"):
                json_value["animation-wrapper-acceleration"] = None

        if "computed-style-storage-kind" in json_value:
            if json_value["computed-style-storage-kind"] not in ['reference', 'value', 'enum', 'raw']:
                raise Exception(f"{key_path} must be either 'reference', 'value', 'enum' or 'raw'.")

            # Default the `computed-style-initial` function to constexpr if the values is an 'enum' or 'raw'.
            # FIXME: This should eventually be replaced by inspecting the grammar in inferring 'constexpr' as long as the grammar does not rely on storing a variable number of elements or use <length-percentage>, <custom-ident>, <dashed-ident>, <string>.
            if "computed-style-initial-constexpr" not in json_value:
                if json_value["computed-style-storage-kind"] in ['enum', 'raw']:
                    json_value["computed-style-initial-constexpr"] = True

        if "computed-style-storage-container" in json_value:
            if json_value["computed-style-storage-container"] not in ['data', 'struct', 'physical-group', 'opaque']:
                raise Exception(f"{key_path} must be either 'data', 'struct', 'physical-group' or 'opaque'.")
            if json_value["computed-style-storage-container"] == 'opaque':
                json_value["computed-style-getter-custom"] = True
                json_value["computed-style-setter-custom"] = True

        if "computed-style-visited-link-storage-container" in json_value:
            if json_value["computed-style-visited-link-storage-container"] not in ['data', 'struct', 'physical-group', 'opaque']:
                raise Exception(f"{key_path} must be either 'data', 'struct', 'physical-group' or 'opaque'.")
            if json_value["computed-style-visited-link-storage-container"] == 'opaque':
                json_value["computed-style-visited-link-getter-custom"] = True
                json_value["computed-style-visited-link-setter-custom"] = True

        if "computed-style-has-explicitly-set-storage-container" in json_value:
            if json_value["computed-style-has-explicitly-set-storage-container"] not in ['data', 'struct', 'physical-group', 'opaque']:
                raise Exception(f"{key_path} must be either 'data', 'struct', 'physical-group' or 'opaque'.")
            if json_value["computed-style-has-explicitly-set-storage-container"] == 'opaque':
                json_value["computed-style-has-explicitly-set-getter-custom"] = True
                json_value["computed-style-has-explicitly-set-setter-custom"] = True

        if "computed-style-storage-name" not in json_value:
            json_value["computed-style-storage-name"] = json_value["computed-style-getter"]

        if "computed-style-visited-link-storage-name" not in json_value:
            json_value["computed-style-visited-link-storage-name"] = f"visitedLink{json_value['computed-style-name-for-methods']}"

        if "computed-style-has-explicitly-set-storage-name" not in json_value:
            json_value["computed-style-has-explicitly-set-storage-name"] = f"hasExplicitlySet{json_value['computed-style-name-for-methods']}"

        if "computed-style-has-explicitly-set-policy" in json_value:
            if json_value["computed-style-has-explicitly-set-policy"] not in ['all-author-origin', 'all-border-radius', 'value-only']:
                raise Exception(f"{key_path} must be either 'all-author-origin', 'all-border-radius', 'value-only'.")

        # NOTE: `skip-computed-style{...}` implies `skip-render-style{...}` unless `skip-render-style{...}` has been explicitly set.
        if "skip-computed-style" in json_value:
            if "skip-computed-style-getter" not in json_value:
                json_value["skip-computed-style-getter"] = json_value["skip-computed-style"]
            if "skip-computed-style-initial" not in json_value:
                json_value["skip-computed-style-initial"] = json_value["skip-computed-style"]
            if "skip-computed-style-setter" not in json_value:
                json_value["skip-computed-style-setter"] = json_value["skip-computed-style"]
            if "skip-computed-style-setter" not in json_value:
                json_value["skip-computed-style-setter"] = json_value["skip-computed-style"]
            if "skip-render-style" not in json_value:
                json_value["skip-render-style"] = json_value["skip-computed-style"]

        if "skip-render-style" in json_value:
            if "skip-render-style-getter" not in json_value:
                json_value["skip-render-style-getter"] = json_value["skip-render-style"]
            if "skip-render-style-setter" in json_value:
                json_value["skip-render-style-setter"] = json_value["skip-render-style"]

        if "skip-computed-style-getter" in json_value:
            if "skip-render-style-getter" not in json_value:
                json_value["skip-render-style-getter"] = json_value["skip-computed-style-getter"]

        if "skip-computed-style-setter" in json_value:
            if "skip-render-style-setter" not in json_value:
                json_value["skip-render-style-setter"] = json_value["skip-computed-style-setter"]

        if "style-builder-custom" not in json_value:
            json_value["style-builder-custom"] = ""
        elif json_value["style-builder-custom"] == "All":
            json_value["style-builder-custom"] = "Initial|Inherit|Value"
        json_value["style-builder-custom"] = frozenset(json_value["style-builder-custom"].split("|"))

        if "shorthand-pattern" in json_value:
            if "shorthand-parser-pattern" in json_value:
                raise Exception(f"{key_path} can't specify both 'shorthand-pattern' and 'shorthand-parser-pattern'.")
            if "shorthand-style-extractor-pattern" in json_value:
                raise Exception(f"{key_path} can't specify both 'shorthand-pattern' and 'shorthand-style-extractor-pattern'.")
            json_value["shorthand-parser-pattern"] = json_value["shorthand-pattern"]
            json_value["shorthand-style-extractor-pattern"] = json_value["shorthand-pattern"]

        if "logical-property-group" in json_value:
            if json_value.get("longhands"):
                raise Exception(f"{key_path} is a shorthand, but belongs to a logical property group.")
            json_value["logical-property-group"] = LogicalPropertyGroup.from_json(parsing_context, f"{key_path}.codegen-properties", json_value["logical-property-group"])

        if "longhands" in json_value:
            json_value["longhands"] = list(compact_map(lambda value: Longhand.from_json(parsing_context, f"{key_path}.codegen-properties", value), json_value["longhands"]))
            if not json_value["longhands"]:
                del json_value["longhands"]
            else:
                if "parser-shorthand" not in json_value:
                    json_value["parser-shorthand"] = f"{property_name.id_without_prefix_with_lowercase_first_letter}Shorthand"

        if json_value.get("top-priority", False):
            if json_value.get("top-priority-reason") is None:
                raise Exception(f"{key_path} has top priority, but no reason justifying it.")
            if json_value.get("longhands"):
                raise Exception(f"{key_path} is a shorthand, but has top priority.")
            if json_value.get("high-priority", False):
                raise Exception(f"{key_path} can't have conflicting top/high priority.")
            if json_value.get("medium-priority", False):
                raise Exception(f"{key_path} can't have conflicting top/medium priority.")

        if json_value.get("high-priority", False):
            if json_value.get("medium-priority", False):
                raise Exception(f"{key_path} can't have conflicting high/medium priority.")
            if json_value.get("longhands"):
                raise Exception(f"{key_path} is a shorthand, but has high priority.")

        if json_value.get("medium-priority", False):
            if json_value.get("longhands"):
                raise Exception(f"{key_path} is a shorthand, but has medium priority.")

        if json_value.get("sink-priority", False):
            if json_value.get("longhands") is not None:
                raise Exception(f"{key_path} is a shorthand, but has sink priority.")

        if json_value.get("cascade-alias"):
            if json_value.get("cascade-alias") == name:
                raise Exception(f"{key_path} can't have itself as a cascade alias property.")
            if json_value.get("longhands"):
                raise Exception(f"{key_path} can't be both a cascade alias and a shorthand.")

        if json_value.get("parser-grammar"):
            for entry_name in ["parser-function", "skip-parser"]:
                if entry_name in json_value:
                    raise Exception(f"{key_path} can't have both 'parser-grammar' and '{entry_name}'.")
            grammar = Grammar.from_string(parsing_context, f"{key_path}", name, json_value["parser-grammar"])
            grammar.perform_fixups(parsing_context.parsed_shared_grammar_rules)
            json_value["parser-grammar"] = grammar

        if json_value.get("parser-grammar-unused"):
            if "parser-grammar-unused-reason" not in json_value:
                raise Exception(f"{key_path} must have 'parser-grammar-unused-reason' specified when using 'parser-grammar-unused'.")
            # If we have a "parser-grammar-unused" specified, we still process it to ensure that at least it is syntactically valid, we just
            # won't actually use it for generation.
            grammar = Grammar.from_string(parsing_context, f"{key_path}", name, json_value["parser-grammar-unused"])
            grammar.perform_fixups(parsing_context.parsed_shared_grammar_rules)
            json_value["parser-grammar-unused"] = grammar

        if json_value.get("parser-grammar-unused-reason"):
            if "parser-grammar-unused" not in json_value:
                raise Exception(f"{key_path} must have 'parser-grammar-unused' specified when using 'parser-grammar-unused-reason'.")

        if json_value.get("parser-function"):
            if "parser-grammar-unused" not in json_value:
                raise Exception(f"{key_path} must have 'parser-grammar-unused' specified when using 'parser-function'.")
            for entry_name in ["skip-parser", "parser-grammar"]:
                if entry_name in json_value:
                    raise Exception(f"{key_path} can't have both 'parser-function' and '{entry_name}'.")

        return StylePropertyCodeGenProperties(property_name, **json_value)

    @property
    def is_logical(self):
        if not self.logical_property_group:
            return False

        resolver = self.logical_property_group.resolver
        for logical_resolvers in LogicalPropertyGroup.logical_property_group_resolvers["logical"].values():
            for logical_resolver in logical_resolvers:
                if resolver == logical_resolver:
                    return True
        return False

class StyleProperty:
    schema = Schema(
        Schema.Entry("animation-type", allowed_types=[str]),
        Schema.Entry("codegen-properties", allowed_types=[dict, list]),
        Schema.Entry("inherited", allowed_types=[bool], default_value=False),
        Schema.Entry("initial", allowed_types=[str]),
        Schema.Entry("specification", allowed_types=[dict], convert_to=Specification),
        Schema.Entry("status", allowed_types=[dict, str], convert_to=Status),
        Schema.Entry("values", allowed_types=[list]),
    )

    def __init__(self, **dictionary):
        StyleProperty.schema.set_attributes_from_dictionary(dictionary, instance=self)
        self.property_name = self.codegen_properties.property_name

    def __str__(self):
        return self.name

    def __repr__(self):
        return self.__str__()

    @staticmethod
    def from_json(parsing_context, key_path, name, json_value):
        assert(type(json_value) is dict)
        StyleProperty.schema.validate_dictionary(parsing_context, f"{key_path}.{name}", json_value, label=f"Property")

        codegen_properties = StylePropertyCodeGenProperties.from_json(parsing_context, f"{key_path}.{name}", name, json_value.get("codegen-properties", {}))
        json_value["codegen-properties"] = codegen_properties

        if codegen_properties.enable_if is not None and not parsing_context.is_enabled(conditional=codegen_properties.enable_if):
            if parsing_context.verbose:
                print(f"SKIPPED {name} due to failing to satisfy 'enable-if' condition, '{json_value['codegen-properties'].enable_if}', with active macro set")
            return None

        if codegen_properties.skip_codegen is not None and codegen_properties.skip_codegen:
            if parsing_context.verbose:
                print(f"SKIPPED {name} due to 'skip-codegen'")
            return None

        if "animation-type" in json_value:
            VALID_ANIMATION_TYPES = [
                'discrete',
                'by computed value',
                'repeatable list',
                'see prose',
                'not animatable',
                'not animatable (needs triage)',
                'not animatable (legacy)',
                'not animatable (internal)'
            ]
            if json_value["animation-type"] not in VALID_ANIMATION_TYPES:
                raise Exception(f"'{name}' specified invalid animation type '{json_value['animation-type']}'. Must specify an animation type from {VALID_ANIMATION_TYPES}.")
        else:
            if not (codegen_properties.is_logical or codegen_properties.longhands):
                raise Exception(f"'{name}' must specify an 'animation-type'.")

        if "initial" in json_value:
            json_value["initial"] = InitialValue(json_value["initial"])
        else:
            if not (codegen_properties.is_logical or codegen_properties.longhands or codegen_properties.cascade_alias or codegen_properties.skip_style_builder):
                raise Exception(f"'{name}' must specify 'initial'.")

        if "values" in json_value:
            if not (codegen_properties.parser_grammar or codegen_properties.skip_parser or codegen_properties.parser_function or codegen_properties.longhands):
                raise Exception(f"'{name}' must specify a 'parser-grammar', 'skip-parser', 'parser-function' or 'longhands' when specifying a 'values' array.")

            json_value["values"] = list(filter(lambda value: value is not None, map(lambda value: Value.from_json(parsing_context, f"{key_path}.{name}", value), json_value["values"])))

            if codegen_properties.parser_grammar:
                codegen_properties.parser_grammar.perform_fixups_for_values_references(json_value["values"])
            elif codegen_properties.parser_grammar_unused:
                codegen_properties.parser_grammar_unused.perform_fixups_for_values_references(json_value["values"])

            if codegen_properties.parser_grammar:
                codegen_properties.parser_grammar.check_against_values(json_value.get("values", []))
            elif codegen_properties.parser_grammar_unused:
                if parsing_context.check_unused_grammars_values or parsing_context.verbose:
                    codegen_properties.parser_grammar_unused.check_against_values(json_value.get("values", []))

        return StyleProperty(**json_value)

    def perform_fixups_for_longhands(self, all_properties):
        # If 'longhands' was specified, replace the names with references to the Property objects.
        if self.codegen_properties.longhands:
            self.codegen_properties.longhands = [all_properties.all_by_name[longhand.value] for longhand in self.codegen_properties.longhands]

    def perform_fixups_for_cascade_alias_properties(self, all_properties):
        if self.codegen_properties.cascade_alias:
            if self.codegen_properties.cascade_alias not in all_properties.all_by_name:
                raise Exception(f"Property {self.name} is a cascade alias for an unknown property: {self.codegen_properties.cascade_alias}.")

            if not self.codegen_properties.skip_style_builder:
                raise Exception(f"Property {self.name} is a cascade alias and should also set 'skip-style-builder'.")

            self.codegen_properties.cascade_alias = all_properties.all_by_name[self.codegen_properties.cascade_alias]

    def perform_fixups_for_logical_property_groups(self, all_properties):
        if self.codegen_properties.logical_property_group:
            group_name = self.codegen_properties.logical_property_group.name
            resolver = self.codegen_properties.logical_property_group.resolver
            kind = self.codegen_properties.logical_property_group.kind
            logic = self.codegen_properties.logical_property_group.logic

            all_properties.logical_property_groups.setdefault(group_name, {})

            existing_kind = all_properties.logical_property_groups[group_name].get("kind")
            if existing_kind and existing_kind != kind:
                raise Exception(f"Logical property group \"{group_name}\" has resolvers of different kinds: {kind} and {existing_kind}.")

            all_properties.logical_property_groups[group_name]["kind"] = kind

            existing_logic = all_properties.logical_property_groups[group_name].get(logic)
            if existing_logic:
                existing_property = existing_logic.get(resolver)
                if existing_property:
                    raise Exception(f"Logical property group \"{group_name}\" has multiple \"{resolver}\" properties: {self.name} and {existing_property.name}.")
            all_properties.logical_property_groups[group_name].setdefault(logic, {})
            all_properties.logical_property_groups[group_name][logic][resolver] = self

    def perform_fixups(self, all_properties):
        self.perform_fixups_for_longhands(all_properties)
        self.perform_fixups_for_cascade_alias_properties(all_properties)
        self.perform_fixups_for_logical_property_groups(all_properties)

    @property
    def id_without_prefix(self):
        return self.property_name.id_without_prefix

    @property
    def id_without_prefix_with_lowercase_first_letter(self):
        return self.property_name.id_without_prefix_with_lowercase_first_letter

    @property
    def id_without_scope(self):
        return self.property_name.id_without_scope

    @property
    def id(self):
        return self.property_name.id

    @property
    def id_or_cascade_alias_id(self):
        if self.codegen_properties.cascade_alias:
            return self.codegen_properties.cascade_alias.id
        return self.id

    # Used for parsing and consume methods. It is prefixed with a 'kind' for descriptors, and left un-prefixed for style properties.
    # Examples:
    #       style property 'column-width' would generate a consume method called `consumeColumnWidth`
    #       @font-face descriptor 'font-display' would generate a consume method called `consumeFontFaceFontDisplay`
    @property
    def name_for_parsing_methods(self):
        return self.id_without_prefix

    @property
    def name(self):
        return self.property_name.name

    @property
    def aliases(self):
        return self.codegen_properties.aliases

    @property
    def is_skipped_from_computed_style(self):
        if self.codegen_properties.internal_only:
            return True

        if self.codegen_properties.skip_style_extractor:
            return True

        if self.codegen_properties.skip_style_builder and not self.codegen_properties.is_logical and not self.codegen_properties.cascade_alias:
            return True

        if self.codegen_properties.longhands is not None:
            for longhand in self.codegen_properties.longhands:
                if not longhand.is_skipped_from_computed_style:
                    return True

        return False

    @property
    def is_animatable(self):
        NOT_ANIMATABLE_TYPES = [
            'not animatable',
            'not animatable (needs triage)',
            'not animatable (legacy)',
            'not animatable (internal)'
        ]
        return self.animation_type not in NOT_ANIMATABLE_TYPES

    # Specialized accessors for coordinated list value properties.

    @property
    def method_name_for_ensure_coordinated_value_list(self):
        if "animation-" in self.name:
            return "ensureAnimations"
        if "transition-" in self.name:
            return "ensureTransitions"
        if "background-" in self.name:
            return "ensureBackgroundLayers"
        if "mask-" in self.name:
            return "ensureMaskLayers"
        if "scroll-timeline-" in self.name:
            return "ensureScrollTimelines"
        if "view-timeline-" in self.name:
            return "ensureViewTimelines"
        raise Exception(f"Unrecognized coordinated list value property name: '{self.name}")

    @property
    def method_name_for_get_coordinated_value_list(self):
        if "animation-" in self.name:
            return "animations"
        if "transition-" in self.name:
            return "transitions"
        if "background-" in self.name:
            return "backgroundLayers"
        if "mask-" in self.name:
            return "maskLayers"
        if "scroll-timeline-" in self.name:
            return "scrollTimelines"
        if "view-timeline-" in self.name:
            return "viewTimelines"
        raise Exception(f"Unrecognized coordinated list value property name: '{self.name}")

    @property
    def method_name_for_set_coordinated_value_list(self):
        if "animation-" in self.name:
            return "setAnimations"
        if "transition-" in self.name:
            return "setTransitions"
        if "background-" in self.name:
            return "setBackgroundLayers"
        if "mask-" in self.name:
            return "setMaskLayers"
        if "scroll-timeline-" in self.name:
            return "setScrollTimelines"
        if "view-timeline-" in self.name:
            return "setViewTimelines"
        raise Exception(f"Unrecognized coordinated list value property name: '{self.name}")

    @property
    def method_name_for_initial_coordinated_value_list(self):
        if "animation-" in self.name:
            return "initialAnimations"
        if "transition-" in self.name:
            return "initialTransitions"
        if "background-" in self.name:
            return "initialBackgroundLayers"
        if "mask-" in self.name:
            return "initialMaskLayers"
        if "scroll-timeline-" in self.name:
            return "initialScrollTimelines"
        if "view-timeline-" in self.name:
            return "initialViewTimelines"
        raise Exception(f"Unrecognized coordinated list value property name: '{self.name}")

    @property
    def type_name_for_coordinated_value_list(self):
        if "animation-" in self.name:
            return "Animations"
        if "transition-" in self.name:
            return "Transitions"
        if "background-" in self.name:
            return "BackgroundLayers"
        if "mask-" in self.name:
            return "MaskLayers"
        if "scroll-timeline-" in self.name:
            return "ScrollTimelines"
        if "view-timeline-" in self.name:
            return "ViewTimelines"
        raise Exception(f"Unrecognized coordinated list value property name: '{self.name}")

    # Computes the return type of the getter.
    @property
    def getter_return_type(self):
        if self.codegen_properties.computed_style_storage_kind == 'reference':
            return f"const {self.codegen_properties.computed_style_type}&"
        return f"{self.codegen_properties.computed_style_type}"

    # Computes the argument type of the setter.
    @property
    def setter_argument_type(self):
        if self.codegen_properties.computed_style_storage_kind == 'reference':
            return f"{self.codegen_properties.computed_style_type}&&"
        return f"{self.codegen_properties.computed_style_type}"

    # Computes the return type of the setter.
    @property
    def setter_return_type(self):
        if self.codegen_properties.computed_style_setter_returns_if_changed:
            return f"bool"
        return f"void"

    # Computes the return type of the initial.
    @property
    def initial_return_type(self):
        return f"{self.codegen_properties.computed_style_type}"

    # Computes the function specifiers, if any, of the getter's declaration.
    @property
    def getter_declaration_function_specifiers(self):
        function_specifiers = []
        if self.codegen_properties.computed_style_getter_constexpr:
            function_specifiers += ['constexpr']
        elif self.codegen_properties.computed_style_getter_inline:
            function_specifiers += ['inline']
        elif self.codegen_properties.computed_style_getter_exported:
            function_specifiers += ['WEBCORE_EXPORT']
        return function_specifiers

    # Computes the function specifiers, if any, of the getter's definition.
    @property
    def getter_definition_function_specifiers(self):
        function_specifiers = []
        if self.codegen_properties.computed_style_getter_constexpr:
            function_specifiers += ['constexpr']
        elif self.codegen_properties.computed_style_getter_inline:
            function_specifiers += ['inline']
        return function_specifiers

    # Computes the function specifiers, if any, of the setter's declaration.
    @property
    def setter_declaration_function_specifiers(self):
        function_specifiers = []
        if self.codegen_properties.computed_style_setter_constexpr:
            function_specifiers += ['constexpr']
        elif self.codegen_properties.computed_style_setter_inline:
            function_specifiers += ['inline']
        elif self.codegen_properties.computed_style_setter_exported:
            function_specifiers += ['WEBCORE_EXPORT']
        return function_specifiers

    # Computes the function specifiers, if any, of the setter's definition.
    @property
    def setter_definition_function_specifiers(self):
        function_specifiers = []
        if self.codegen_properties.computed_style_setter_constexpr:
            function_specifiers += ['constexpr']
        elif self.codegen_properties.computed_style_setter_inline:
            function_specifiers += ['inline']
        return function_specifiers

    # Computes the function specifiers, if any, of the initial's declaration.
    @property
    def initial_declaration_function_specifiers(self):
        function_specifiers = ["static"]
        if self.codegen_properties.computed_style_initial_constexpr:
            function_specifiers += ['constexpr']
        elif self.codegen_properties.computed_style_initial_inline:
            function_specifiers += ['inline']
        elif self.codegen_properties.computed_style_initial_exported:
            function_specifiers += ['WEBCORE_EXPORT']
        return function_specifiers

    # Computes the function specifiers, if any, of the initial's definition.
    @property
    def initial_definition_function_specifiers(self):
        function_specifiers = []
        if self.codegen_properties.computed_style_initial_constexpr:
            function_specifiers += ['constexpr']
        elif self.codegen_properties.computed_style_initial_inline:
            function_specifiers += ['inline']
        return function_specifiers


class StyleProperties:
    def __init__(self, properties):
        self.properties = properties
        self.properties_by_name = {property.name: property for property in properties}
        self.logical_property_groups = {}
        self._all = None
        self._all_computed = None
        self._settings_flags = None

        self._perform_fixups()

        self.shorthand_by_longhand = {}
        for shorthand in self.all_shorthands:
            for longhand in shorthand.codegen_properties.longhands:
                self.shorthand_by_longhand[longhand] = shorthand

    def __str__(self):
        return "StyleProperties"

    def __repr__(self):
        return self.__str__()

    @property
    def id(self):
        return 'StyleProperty'

    @property
    def name(self):
        return 'style'

    @property
    def noun(self):
        return 'property'

    @property
    def supports_shorthands(self):
        return True

    @staticmethod
    def from_json(parsing_context, key_path, json_value):
        return StyleProperties(list(compact_map(lambda item: StyleProperty.from_json(parsing_context, key_path, item[0], item[1]), json_value.items())))

    # Updates any references to other properties that were by name (e.g. string) with a direct
    # reference to the property object.
    def _perform_fixups(self):
        for property in self.properties:
            property.perform_fixups(self)

    # Returns the set of all properties. Default decreasing priority and name sorting.
    @property
    def all(self):
        if not self._all:
            self._all = sorted(self.properties, key=functools.cmp_to_key(StyleProperties._sort_by_descending_priority_and_name))
        return self._all

    # Returns the map of property names to properties.
    @property
    def all_by_name(self):
        return self.properties_by_name

    # Returns the set of all properties that are included in computed styles. Sorted lexically by name with prefixed properties last.
    @property
    def all_computed(self):
        if not self._all_computed:
            self._all_computed = sorted([property for property in self.all if not property.is_skipped_from_computed_style], key=functools.cmp_to_key(StyleProperties._sort_with_prefixed_properties_last))
        return self._all_computed

    # Returns a generator for the set of properties that have an associate longhand, the so-called shorthands. Default decreasing priority and name sorting.
    @property
    def all_shorthands(self):
        return (property for property in self.all if property.codegen_properties.longhands)

    # Returns a generator for the set of properties that do not have an associate longhand. Default decreasing priority and name sorting.
    @property
    def all_non_shorthands(self):
        return (property for property in self.all if not property.codegen_properties.longhands)

    # Returns a generator for the set of properties that are direction-aware (aka flow-sensitive). Sorted first by property group name and then by property name.
    @property
    def all_direction_aware_properties(self):
        for group_name, property_group in sorted(self.logical_property_groups.items(), key=lambda x: x[0]):
            for resolver, property in sorted(property_group["logical"].items(), key=lambda x: x[1].name):
                yield property

    # Returns a generator for the set of properties that are in a logical property group, either logical or physical. Sorted first by property group name, then logical/physical, and then property name.
    @property
    def all_in_logical_property_group(self):
        for group_name, property_group in sorted(self.logical_property_groups.items(), key=lambda x: x[0]):
            for kind in ["logical", "physical"]:
                for resolver, property in sorted(property_group[kind].items(), key=lambda x: x[1].name):
                    yield property

    # Default sorting algorithm for properties.
    def _sort_by_descending_priority_and_name(a, b):
        # Sort shorthands to the back
        a_is_shorthand = a.codegen_properties.longhands is not None
        b_is_shorthand = b.codegen_properties.longhands is not None
        if a_is_shorthand and not b_is_shorthand:
            return 1
        if not a_is_shorthand and b_is_shorthand:
            return -1

        # Sort longhands with top priority to the front
        a_is_top_priority = a.codegen_properties.top_priority
        b_is_top_priority = b.codegen_properties.top_priority
        if a_is_top_priority and not b_is_top_priority:
            return -1
        if not a_is_top_priority and b_is_top_priority:
            return 1

        # Sort longhands with high priority to the front
        a_is_high_priority = a.codegen_properties.high_priority
        b_is_high_priority = b.codegen_properties.high_priority
        if a_is_high_priority and not b_is_high_priority:
            return -1
        if not a_is_high_priority and b_is_high_priority:
            return 1

        # Sort longhands with medium priority to the front
        a_is_medium_priority = a.codegen_properties.medium_priority
        b_is_medium_priority = b.codegen_properties.medium_priority
        if a_is_medium_priority and not b_is_medium_priority:
            return -1
        if not a_is_medium_priority and b_is_medium_priority:
            return 1

        # Sort logical longhands in a logical property group to the back, before shorthands.
        a_is_in_logical_property_group_logical = a.codegen_properties.logical_property_group and a.codegen_properties.logical_property_group.logic == 'logical'
        b_is_in_logical_property_group_logical = b.codegen_properties.logical_property_group and b.codegen_properties.logical_property_group.logic == 'logical'
        if a_is_in_logical_property_group_logical and not b_is_in_logical_property_group_logical:
            return 1
        if not a_is_in_logical_property_group_logical and b_is_in_logical_property_group_logical:
            return -1

        # Sort physical longhands in a logical property group to the back, before shorthands.
        a_is_in_logical_property_group_physical = a.codegen_properties.logical_property_group and a.codegen_properties.logical_property_group.logic == 'physical'
        b_is_in_logical_property_group_physical = b.codegen_properties.logical_property_group and b.codegen_properties.logical_property_group.logic == 'physical'
        if a_is_in_logical_property_group_physical and not b_is_in_logical_property_group_physical:
            return 1
        if not a_is_in_logical_property_group_physical and b_is_in_logical_property_group_physical:
            return -1

        # Sort sunken names at the end of their priority bucket.
        a_is_sink_priority = a.codegen_properties.sink_priority
        b_is_sink_priority = b.codegen_properties.sink_priority
        if a_is_sink_priority and not b_is_sink_priority:
            return 1
        if not a_is_sink_priority and b_is_sink_priority:
            return -1

        return StyleProperties._sort_with_prefixed_properties_last(a, b)

    def _sort_with_prefixed_properties_last(a, b):
        # Sort prefixed names to the back.
        a_starts_with_prefix = a.name[0] == "-"
        b_starts_with_prefix = b.name[0] == "-"
        if a_starts_with_prefix and not b_starts_with_prefix:
            return 1
        if not a_starts_with_prefix and b_starts_with_prefix:
            return -1

        # Finally, sort by name.
        if a.name < b.name:
            return -1
        elif a.name > b.name:
            return 1
        return 0


class DescriptorCodeGenProperties:
    schema = Schema(
        Schema.Entry("accepts-quirky-angle", allowed_types=[bool], default_value=False),
        Schema.Entry("accepts-quirky-color", allowed_types=[bool], default_value=False),
        Schema.Entry("accepts-quirky-length", allowed_types=[bool], default_value=False),
        Schema.Entry("aliases", allowed_types=[list], default_value=[]),
        Schema.Entry("enable-if", allowed_types=[str]),
        Schema.Entry("internal-only", allowed_types=[bool], default_value=False),
        Schema.Entry("longhands", allowed_types=[list]),
        Schema.Entry("parser-exported", allowed_types=[bool]),
        Schema.Entry("parser-function", allowed_types=[str]),
        Schema.Entry("parser-function-allows-number-or-integer-input", allowed_types=[bool], default_value=False),
        Schema.Entry("parser-grammar", allowed_types=[str]),
        Schema.Entry("parser-grammar-unused", allowed_types=[str]),
        Schema.Entry("parser-grammar-unused-reason", allowed_types=[str]),
        Schema.Entry("settings-flag", allowed_types=[str]),
        Schema.Entry("skip-codegen", allowed_types=[bool], default_value=False),
        Schema.Entry("skip-parser", allowed_types=[bool], default_value=False),
    )

    def __init__(self, descriptor_name, **dictionary):
        DescriptorCodeGenProperties.schema.set_attributes_from_dictionary(dictionary, instance=self)
        self.descriptor_name = descriptor_name

        # By defining these to None, we can utilize the shared sorting method, StyleProperties._sort_by_descending_priority_and_name.
        self.top_priority = None
        self.high_priority = None
        self.medium_priority = None
        self.sink_priority = None
        self.logical_property_group = None

    def __str__(self):
        return f"DescriptorCodeGenProperties {vars(self)}"

    def __repr__(self):
        return self.__str__()

    @staticmethod
    def from_json(parsing_context, key_path, name, json_value):
        if type(json_value) is list:
            json_value = parsing_context.select_enabled_variant(json_value, label=f"{key_path}.codegen-properties")

        assert(type(json_value) is dict)
        DescriptorCodeGenProperties.schema.validate_dictionary(parsing_context, f"{key_path}.codegen-properties", json_value, label=f"DescriptorCodeGenProperties")

        descriptor_name = PropertyName(name)

        if "longhands" in json_value:
            json_value["longhands"] = list(compact_map(lambda value: Longhand.from_json(parsing_context, f"{key_path}.codegen-properties", value), json_value["longhands"]))
            if not json_value["longhands"]:
                del json_value["longhands"]

        if json_value.get("parser-grammar"):
            for entry_name in ["parser-function", "skip-parser", "longhands"]:
                if entry_name in json_value:
                    raise Exception(f"{key_path} can't have both 'parser-grammar' and '{entry_name}.")
            grammar = Grammar.from_string(parsing_context, f"{key_path}", name, json_value["parser-grammar"])
            grammar.perform_fixups(parsing_context.parsed_shared_grammar_rules)
            json_value["parser-grammar"] = grammar

        if json_value.get("parser-grammar-unused"):
            if "parser-grammar-unused-reason" not in json_value:
                raise Exception(f"{key_path} must have 'parser-grammar-unused-reason' specified when using 'parser-grammar-unused'.")
            # If we have a "parser-grammar-unused" specified, we still process it to ensure that at least it is syntactically valid, we just
            # won't actually use it for generation.
            grammar = Grammar.from_string(parsing_context, f"{key_path}", name, json_value["parser-grammar-unused"])
            grammar.perform_fixups(parsing_context.parsed_shared_grammar_rules)
            json_value["parser-grammar-unused"] = grammar

        if json_value.get("parser-grammar-unused-reason"):
            if "parser-grammar-unused" not in json_value:
                raise Exception(f"{key_path} must have 'parser-grammar-unused' specified when using 'parser-grammar-unused-reason'.")

        if json_value.get("parser-function"):
            if "parser-grammar-unused" not in json_value:
                raise Exception(f"{key_path} must have 'parser-grammar-unused' specified when using 'parser-function'.")
            for entry_name in ["skip-parser", "longhands"]:
                if entry_name in json_value:
                    raise Exception(f"{key_path} can't have both 'parser-function' and '{entry_name}'.")

        return DescriptorCodeGenProperties(descriptor_name, **json_value)


class Descriptor:
    schema = Schema(
        Schema.Entry("codegen-properties", allowed_types=[dict, list]),
        Schema.Entry("specification", allowed_types=[dict], convert_to=Specification),
        Schema.Entry("status", allowed_types=[dict, str], convert_to=Status),
        Schema.Entry("values", allowed_types=[list]),
    )

    def __init__(self, descriptor_set_name, **dictionary):
        Descriptor.schema.set_attributes_from_dictionary(dictionary, instance=self)
        self.descriptor_set_name = descriptor_set_name
        self.descriptor_name = self.codegen_properties.descriptor_name

    def __str__(self):
        return f"{self.name} ({self.descriptor_set_name})"

    def __repr__(self):
        return self.__str__()

    @staticmethod
    def from_json(parsing_context, key_path, name, json_value, descriptor_set_name):
        assert(type(json_value) is dict)
        Descriptor.schema.validate_dictionary(parsing_context, f"{key_path}.{name}", json_value, label=f"Descriptor")

        codegen_properties = DescriptorCodeGenProperties.from_json(parsing_context, f"{key_path}.{name}", name, json_value.get("codegen-properties", {}))
        json_value["codegen-properties"] = codegen_properties

        if codegen_properties.enable_if is not None and not parsing_context.is_enabled(conditional=codegen_properties.enable_if):
            if parsing_context.verbose:
                print(f"SKIPPED {name} due to failing to satisfy 'enable-if' condition, '{json_value['codegen-properties'].enable_if}', with active macro set")
            return None

        if codegen_properties.skip_codegen is not None and codegen_properties.skip_codegen:
            if parsing_context.verbose:
                print(f"SKIPPED {name} due to 'skip-codegen'")
            return None

        if "values" in json_value:
            if not (codegen_properties.parser_grammar or codegen_properties.skip_parser or codegen_properties.parser_function or codegen_properties.longhands):
                raise Exception(f"'{name}' must specify a 'parser-grammar', 'skip-parser', 'parser-function' or 'longhands' when specifying a 'values' array.")

            json_value["values"] = list(filter(lambda value: value is not None, map(lambda value: Value.from_json(parsing_context, f"{key_path}.{name}", value), json_value["values"])))

            if codegen_properties.parser_grammar:
                codegen_properties.parser_grammar.perform_fixups_for_values_references(json_value["values"])
            elif codegen_properties.parser_grammar_unused:
                codegen_properties.parser_grammar_unused.perform_fixups_for_values_references(json_value["values"])

            if codegen_properties.parser_grammar:
                codegen_properties.parser_grammar.check_against_values(json_value.get("values", []))
            elif codegen_properties.parser_grammar_unused:
                if parsing_context.check_unused_grammars_values or parsing_context.verbose:
                    codegen_properties.parser_grammar_unused.check_against_values(json_value.get("values", []))

        return Descriptor(descriptor_set_name, **json_value)

    def perform_fixups_for_longhands(self, all_descriptors):
        # If 'longhands' was specified, replace the names with references to the Descriptor objects.
        if self.codegen_properties.longhands:
            self.codegen_properties.longhands = [all_descriptors.all_by_name[longhand.value] for longhand in self.codegen_properties.longhands]

    def perform_fixups(self, all_descriptors):
        self.perform_fixups_for_longhands(all_descriptors)

    @property
    def id_without_prefix(self):
        return self.descriptor_name.id_without_prefix

    # Used for parsing and consume methods. It is prefixed with the rule type for descriptors, and left un-prefixed for style properties.
    # Examples:
    #       style property 'column-width' would generate a consume method called `consumeColumnWidth`
    #       @font-face descriptor 'font-display' would generate a consume method called `consumeFontFaceFontDisplay`
    @property
    def name_for_parsing_methods(self):
        return Name.convert_name_to_id(self.descriptor_set_name[1:]) + self.descriptor_name.id_without_prefix

    @property
    def id_without_prefix_with_lowercase_first_letter(self):
        return self.descriptor_name.id_without_prefix_with_lowercase_first_letter

    @property
    def id_without_scope(self):
        return self.descriptor_name.id_without_scope

    @property
    def id(self):
        return self.descriptor_name.id

    @property
    def name(self):
        return self.descriptor_name.name

    @property
    def aliases(self):
        return self.codegen_properties.aliases


# Provides access to each descriptor in a grouped set of descriptor (e.g. @font-face, @counter-styles, etc.) There is
# one of these per rule type, e.g. @font-face, @counter-styles, etc.
class DescriptorSet:
    def __init__(self, name, descriptors):
        self.name = name
        self.descriptors = descriptors
        self.descriptors_by_name = {descriptor.name: descriptor for descriptor in descriptors}
        self._all = None
        self._perform_fixups()

    @staticmethod
    def from_json(parsing_context, key_path, name, json_value):
        return DescriptorSet(name, list(compact_map(lambda item: Descriptor.from_json(parsing_context, f"{key_path}.{name}", item[0], item[1], name), json_value.items())))

    def _perform_fixups(self):
        for descriptor in self.descriptors:
            descriptor.perform_fixups(self)

    @property
    def id(self):
        return f'{Name.convert_name_to_id(self.name[1:])}Descriptor'

    @property
    def noun(self):
        return 'descriptor'

    @property
    def supports_shorthands(self):
        return False

    @property
    def all(self):
        if not self._all:
            self._all = sorted(self.descriptors, key=functools.cmp_to_key(StyleProperties._sort_by_descending_priority_and_name))
        return self._all

    @property
    def all_by_name(self):
        return self.descriptors_by_name


# Provides access to each of the grouped sets of descriptor (e.g. @font-face, @counter-styles, etc. which are
# stored as DescriptorSet instances) via either the `descriptor_sets` list or by name as dynamic attributes.
#
# e.g. font_face_descriptor_set = descriptors.font_face
#
class Descriptors:
    def __init__(self, descriptor_sets):
        self.descriptor_sets = descriptor_sets
        for descriptor_set in descriptor_sets:
            setattr(self, descriptor_set.name.replace('@', 'at-').replace('-', '_'), descriptor_set)

    def __str__(self):
        return f"Descriptors"

    def __repr__(self):
        return self.__str__()

    @staticmethod
    def from_json(parsing_context, key_path, json_value):
        return Descriptors([DescriptorSet.from_json(parsing_context, key_path, name, descriptors) for (name, descriptors) in json_value.items()])

    # Returns a generator for the set of descriptors.
    @property
    def all(self):
        return itertools.chain.from_iterable(descriptor_set.all for descriptor_set in self.descriptor_sets)


class ComputedStyleStorageTreeNode:
    def __init__(self, name):
        self.name = name
        self.kind = "data"
        self.children = {}
        self.properties = []
        self.visited_link_properties = []

    def __str__(self):
        return f"ComputedStyleStorageTreeNode {vars(self)}"

    def __repr__(self):
        return self.__str__()

    def __eq__(self, other):
        return self.name == other.name

    def __hash__(self):
        return hash(self.name)


class PropertiesAndDescriptors:
    def __init__(self, style_properties, descriptors):
        self.style_properties = style_properties
        self.descriptors = descriptors
        self._all_grouped_by_name = None
        self._all_by_name = None
        self._all_unique = None
        self._settings_flags = None
        self._computed_style_storage_model = None

    def __str__(self):
        return "PropertiesAndDescriptors"

    def __repr__(self):
        return self.__str__()

    @staticmethod
    def from_json(parsing_context, *, properties_json_value, descriptors_json_value):
        return PropertiesAndDescriptors(
            StyleProperties.from_json(parsing_context, "properties", properties_json_value),
            Descriptors.from_json(parsing_context, "descriptors", descriptors_json_value),
        )

    def _compute_all_grouped_by_name(self):
        return [self.all_by_name[property.name] for property in self.all_unique]

    def _compute_all_by_name(self):
        result = {}
        for property in self.all_properties_and_descriptors:
            result.setdefault(property.name, []).append(property)
        return result

    def _compute_all_unique(self):
        # NOTE: This is computes the ordered set of properties and descriptors that correspond to the CSSPropertyID
        # enumeration and related lookup tables and functions.

        result = list(self.style_properties.all)
        name_set = set(self.style_properties.all_by_name.keys())

        for descriptor in self.descriptors.all:
            if descriptor.name in name_set:
                continue
            result.append(descriptor)
            name_set.add(descriptor.name)

        # FIXME: It doesn't make a lot of sense to sort the descriptors like this, but this maintains
        # the current behavior and has no negative side effect. In the future, we should either separate
        # the descriptors out of CSSPropertyID or the descriptor-only ones together in some fashion.
        return sorted(result, key=functools.cmp_to_key(StyleProperties._sort_by_descending_priority_and_name))

    def _compute_computed_style_storage_model(self):
        root = ComputedStyleStorageTreeNode('ComputedStyle')

        for property in self.style_properties.all:
            if property.codegen_properties.skip_computed_style:
                continue
            if property.codegen_properties.is_logical:
                continue
            if property.codegen_properties.longhands:
                continue
            if property.codegen_properties.cascade_alias:
                continue

            if not property.codegen_properties.computed_style_storage_path:
                raise Exception(f"Missing ComputedStyle storage path for property {property.id}.")

            tree_node = root
            for path_entry in property.codegen_properties.computed_style_storage_path:
                path_entry_node = ComputedStyleStorageTreeNode(path_entry)

                if path_entry_node.name not in tree_node.children:
                    tree_node.children.update({ path_entry_node.name: path_entry_node })
                tree_node = tree_node.children[path_entry_node.name]

            if tree_node.kind != "data" and tree_node.kind != property.codegen_properties.computed_style_storage_container:
                raise Exception(f"Storage container '{'/'.join(property.codegen_properties.computed_style_storage_path)}' has multiple container kinds specified: '{tree_node.kind}' and '{property.codegen_properties.computed_style_storage_container}'")

            tree_node.kind = property.codegen_properties.computed_style_storage_container
            tree_node.properties.append(property)

            if property.codegen_properties.computed_style_visited_link_storage_path:
                tree_node = root
                for path_entry in property.codegen_properties.computed_style_visited_link_storage_path:
                    path_entry_node = ComputedStyleStorageTreeNode(path_entry)

                    if path_entry_node.name not in tree_node.children:
                        tree_node.children.update({ path_entry_node.name: path_entry_node })
                    tree_node = tree_node.children[path_entry_node.name]

                if tree_node.kind != "data" and tree_node.kind != property.codegen_properties.computed_style_visited_link_storage_container:
                    raise Exception(f"Storage container '{'/'.join(property.codegen_properties.computed_style_visited_link_storage_path)}' has multiple container kinds specified: '{tree_node.kind}' and '{property.codegen_properties.computed_style_visited_link_storage_container}'")

                tree_node.kind = property.codegen_properties.computed_style_visited_link_storage_container
                tree_node.visited_link_properties.append(property)

        return root

    # Returns a generator for the set of all properties and descriptors.
    @property
    def all_properties_and_descriptors(self):
        return itertools.chain(self.style_properties.all, self.descriptors.all)

    # Returns a list of all the property or descriptor sets (e.g. 'style', '@counter-style', '@font-face', etc.).
    @property
    def all_sets(self):
        return [self.style_properties] + self.descriptors.descriptor_sets

    # Returns the set of properties and descriptors that have unique names, preferring style properties when
    # there is a conflict. This set corresponds one-to-one in membership and order with CSSPropertyID.
    @property
    def all_unique(self):
        if not self._all_unique:
            self._all_unique = self._compute_all_unique()
        return self._all_unique

    # Returns a parallel list to `all_unique`, but rather than containing the canonical property, each entry
    # in this list is a list of all properties or descriptors with the unique name.
    @property
    def all_grouped_by_name(self):
        if not self._all_grouped_by_name:
            self._all_grouped_by_name = self._compute_all_grouped_by_name()
        return self._all_grouped_by_name

    # Returns a map of names to lists of the properties or descriptors with that name.
    @property
    def all_by_name(self):
        if not self._all_by_name:
            self._all_by_name = self._compute_all_by_name()
        return self._all_by_name

    # Returns a generator for the set of properties and descriptors that are conditionally included depending on settings. If two properties
    # or descriptors have the same name, we only return the canonical one and only if all the variants have settings flags.
    #
    # For example, there are two "speak-as" entries. One is a style property and the other is @counter-style descriptor. Only the one of the
    # two, the @counter-style descriptor, has settings_flags set, so we don't return anything for that name.
    @property
    def all_unique_with_settings_flag(self):
        return (property_set[0] for property_set in self.all_grouped_by_name if all(property.codegen_properties.settings_flag for property in property_set))

    # Returns a generator for the subset of `self.all_unique` that are marked internal-only.
    @property
    def all_unique_internal_only(self):
        return (property for property in self.all_unique if property.codegen_properties.internal_only)

    # Returns a generator for the subset of `self.all_unique` that are NOT marked internal.
    @property
    def all_unique_non_internal_only(self):
        return (property for property in self.all_unique if not property.codegen_properties.internal_only)

    @property
    def all_descriptor_only(self):
        return (descriptor for descriptor in self.descriptors.all if descriptor.name not in self.style_properties.all_by_name)

    # Returns the set of settings-flags used by any property or descriptor. Uniqued and sorted lexically.
    @property
    def settings_flags(self):
        if not self._settings_flags:
            self._settings_flags = sorted(list(set([property.codegen_properties.settings_flag for property in self.all_properties_and_descriptors if property.codegen_properties.settings_flag])))
        return self._settings_flags

    # Returns a tree representing the storage structure underlying RenderStyle.
    @property
    def computed_style_storage_model(self):
        if not self._computed_style_storage_model:
            self._computed_style_storage_model = self._compute_computed_style_storage_model()
        return self._computed_style_storage_model


# MARK: - Property Parsing

class Term:
    @staticmethod
    def wrap_with_multiplier(multiplier, term):
        if multiplier.kind == BNFNodeMultiplier.Kind.ZERO_OR_ONE:
            return OptionalTerm.wrapping_term(term, annotation=multiplier.annotation)
        elif multiplier.kind == BNFNodeMultiplier.Kind.SPACE_SEPARATED_ZERO_OR_MORE:
            return UnboundedRepetitionTerm.wrapping_term(term, separator=' ', min=0, annotation=multiplier.annotation)
        elif multiplier.kind == BNFNodeMultiplier.Kind.SPACE_SEPARATED_ONE_OR_MORE:
            return UnboundedRepetitionTerm.wrapping_term(term, separator=' ', min=1, annotation=multiplier.annotation)
        elif multiplier.kind == BNFNodeMultiplier.Kind.SPACE_SEPARATED_EXACT:
            return BoundedRepetitionTerm.wrapping_term(term, separator=' ', min=multiplier.range.min, max=multiplier.range.min, annotation=multiplier.annotation)
        elif multiplier.kind == BNFNodeMultiplier.Kind.SPACE_SEPARATED_AT_LEAST:
            return UnboundedRepetitionTerm.wrapping_term(term, separator=' ', min=multiplier.range.min, annotation=multiplier.annotation)
        elif multiplier.kind == BNFNodeMultiplier.Kind.SPACE_SEPARATED_BETWEEN:
            return BoundedRepetitionTerm.wrapping_term(term, separator=' ', min=multiplier.range.min, max=multiplier.range.max, annotation=multiplier.annotation)
        elif multiplier.kind == BNFNodeMultiplier.Kind.COMMA_SEPARATED_ONE_OR_MORE:
            return UnboundedRepetitionTerm.wrapping_term(term, separator=',', min=1, annotation=multiplier.annotation)
        elif multiplier.kind == BNFNodeMultiplier.Kind.COMMA_SEPARATED_EXACT:
            return BoundedRepetitionTerm.wrapping_term(term, separator=',', min=multiplier.range.min, max=multiplier.range.min, annotation=multiplier.annotation)
        elif multiplier.kind == BNFNodeMultiplier.Kind.COMMA_SEPARATED_AT_LEAST:
            return UnboundedRepetitionTerm.wrapping_term(term, separator=',', min=multiplier.range.min, annotation=multiplier.annotation)
        elif multiplier.kind == BNFNodeMultiplier.Kind.COMMA_SEPARATED_BETWEEN:
            return BoundedRepetitionTerm.wrapping_term(term, separator=',', min=multiplier.range.min, max=multiplier.range.max, annotation=multiplier.annotation)

    @staticmethod
    def from_node(node):
        if isinstance(node, BNFGroupingNode):
            if node.kind == BNFGroupingNode.Kind.MATCH_ALL_ORDERED:
                # FIXME: This should be part of the GroupTerm's simplification.
                if len(node.members) == 1:
                    term = Term.from_node(node.members[0])
                else:
                    term = MatchAllOrderedTerm.from_node(node)
            elif node.kind == BNFGroupingNode.Kind.MATCH_ONE:
                term = MatchOneTerm.from_node(node)
            elif node.kind == BNFGroupingNode.Kind.MATCH_ALL_ANY_ORDER:
                term = MatchAllAnyOrderTerm.from_node(node)
            elif node.kind == BNFGroupingNode.Kind.MATCH_ONE_OR_MORE_ANY_ORDER:
                term = MatchOneOrMoreAnyOrderTerm.from_node(node)
            else:
                raise Exception(f"Unknown grouping kind '{node.kind}' in BNF parse tree node '{node}'")
        elif isinstance(node, BNFReferenceNode):
            term = ReferenceTerm.from_node(node)
        elif isinstance(node, BNFFunctionNode):
            term = FunctionTerm.from_node(node)
        elif isinstance(node, BNFKeywordNode):
            term = KeywordTerm.from_node(node)
        elif isinstance(node, BNFLiteralNode):
            term = LiteralTerm.from_node(node)
        else:
            raise Exception(f"Unknown node '{node}' in BNF parse tree")

        # If the node has an attached multiplier, wrap the node in
        # a term created from that multiplier.
        if node.multiplier.kind:
            term = Term.wrap_with_multiplier(node.multiplier, term)

        return term


class BuiltinSchema:
    class StringParameter:
        def __init__(self, name, *, mappings=None, default=None):
            self.name = name
            self.mappings = mappings
            self.default = default

    class RangeParameter:
        def __init__(self, name):
            self.name = name

    class Entry:
        def __init__(self, name, *parameter_descriptors):
            self.name = Name(name)

            # Mapping of descriptor name (e.g. 'mode') to StringParameter descriptor.
            self.string_parameter_descriptors = {}

            # RangeParameter descriptor, if specified.
            self.range_parameter_descriptor = None

            for parameter_descriptor in parameter_descriptors:
                if isinstance(parameter_descriptor, BuiltinSchema.StringParameter):
                    self.string_parameter_descriptors[parameter_descriptor.name] = parameter_descriptor
                if isinstance(parameter_descriptor, BuiltinSchema.RangeParameter):
                    if self.range_parameter_descriptor is not None:
                        raise Exception("BuiltinScheme entry {name} may only specify a since RangeParameter.")
                    self.range_parameter_descriptor = parameter_descriptor

            def builtin_schema_type_init(self, parameters):
                # Map from descriptor name (e.g. 'value_range' or 'mode') to value (e.g. `CSS::Range{0, CSS::Range::infinity}` or `HTMLStandardMode`) for all of the parameters.
                self.parameter_map = {}

                # Map from descriptor names that have been used so far.
                descriptors_used = {}

                # Example parameters: [ReferenceTerm.StringParameter('svg'), ReferenceTerm.StringParameter('excluding', ['auto','none']), ReferenceTerm.RangeParameter(0, 'inf')].
                for parameter in parameters:
                    if type(parameter) is ReferenceTerm.StringParameter:
                        if parameter.name not in self.entry.string_parameter_descriptors:
                            raise Exception(f"Unknown parameter '{parameter}' passed to <{self.entry.name.name}>. Supported parameters are {', '.join(quote_iterable(self.entry.string_parameter_descriptors.keys()))}.")

                        descriptor = self.entry.string_parameter_descriptors[parameter.name]
                        if descriptor.name in descriptors_used:
                            raise Exception(f"More than one parameter of type '{descriptor.name}` passed to <{self.entry.name.name}>, pick one: {descriptors_used[descriptor.name]}, {parameter}.")
                        descriptors_used[descriptor.name] = parameter.name

                        if descriptor.mappings:
                            if not parameter.value:
                                raise Exception(f"'{parameter}' passed to <{self.entry.name.name}> has no value associated with it. Supported values are {', '.join(quote_iterable(descriptor.mappings.keys()))}.")

                            for value in parameter.value:
                                if value not in descriptor.mappings:
                                    raise Exception(f"'{parameter}' passed to <{self.entry.name.name}> does not match any of the supported mappings. Supported mappings are {', '.join(quote_iterable(descriptor.mappings.keys()))}.")

                            self.parameter_map[descriptor.name] = ', '.join(map(lambda x: descriptor.mappings[x], parameter.value))
                        else:
                            self.parameter_map[descriptor.name] = parameter.value
                    elif type(parameter) is ReferenceTerm.RangeParameter:
                        if self.entry.range_parameter_descriptor is None:
                            raise Exception(f"Range parameter '{parameter}' passed to <{self.entry.name.name}>. Range parameters are not supported for this entry.")

                        descriptor = self.entry.range_parameter_descriptor
                        if descriptor.name in descriptors_used:
                            raise Exception(f"More than one parameter of type '{descriptor.name}` passed to <{self.entry.name.name}>, pick one: {descriptors_used[descriptor.name]}, {parameter}.")
                        descriptors_used[descriptor.name] = descriptor

                        min = '-CSS::Range::infinity' if parameter.min == '-inf' else parameter.min
                        max =  'CSS::Range::infinity' if parameter.max ==  'inf' else parameter.max
                        self.parameter_map[descriptor.name] = f'CSS::Range{{{min}, {max}}}'
                    else:
                        raise Exception(f"Unknown parameter '{parameter}' passed to <{self.entry.name.name}>. Supported parameters are {', '.join(quote_iterable(self.entry.value_to_descriptor.keys()))}.")

                # Fill `results` with mappings from names (e.g. 'value_range' or 'mode') to values (e.g. `CSS::Range(0, CSS::Range::infinity)` or `HTMLStandardMode`), pulling in default values for unspecified parameters.
                self.results = {}

                for descriptor in self.entry.string_parameter_descriptors.values():
                    if descriptor.name in self.parameter_map:
                        self.results[descriptor.name] = self.parameter_map[descriptor.name]
                    elif descriptor.mappings and descriptor.default:
                        self.results[descriptor.name] = ', '.join(map(lambda x: descriptor.mappings[x], descriptor.default if isinstance(descriptor.default, list) else [descriptor.default]))
                    else:
                        self.results[descriptor.name] = None

                if self.entry.range_parameter_descriptor is not None:
                    descriptor = self.entry.range_parameter_descriptor
                    if descriptor.name in self.parameter_map:
                        self.results[descriptor.name] = self.parameter_map[descriptor.name]
                    else:
                        # If no range parameter was specified in the grammar, the empty string will work to cause the default range to be used.
                        self.results[descriptor.name] = ""

            def builtin_schema_type_parameter_string_getter(name, self):
                return self.results[name]

            # Dynamically generate a class that can handle validation and generation.
            class_name = f"Builtin{self.name.id_without_prefix}Consumer"
            class_attributes = {
                "__init__": builtin_schema_type_init,
                "entry": self,
            }

            for name in self.string_parameter_descriptors.keys():
                class_attributes[name.replace('-', '_')] = property(functools.partial(builtin_schema_type_parameter_string_getter, name))
            if self.range_parameter_descriptor is not None:
                name = self.range_parameter_descriptor.name
                class_attributes[name.replace('-', '_')] = property(functools.partial(builtin_schema_type_parameter_string_getter, name))

            self.constructor = type(class_name, (), class_attributes)

            # Also add the type to the global scope for use in other classes.
            globals()[class_name] = self.constructor

    def __init__(self, *entries):
        self.entries = {entry.name.name: entry for entry in entries}

    def validate_and_construct_if_builtin(self, name, parameters):
        if name.name in self.entries:
            return self.entries[name.name].constructor(parameters)
        return None


# Reference terms look like keyword terms, but are surrounded by '<' and '>' characters (i.e. "<number>").
# They can either reference a rule from the grammar-rules set, in which case they will be replaced by
# the real term during fixup, or a builtin rule, in which case they will inform the generator to call
# out to a handwritten consumer. Example:
#
#   e.g. "<length unitless-allowed>"
#

# BuiltinSchema.StringParameter Mappings
UNITLESS_ZERO_MAPPINGS = {'allowed': 'UnitlessZeroQuirk::Allow', 'forbidden': 'UnitlessZeroQuirk::Forbid'}
ANCHOR_MAPPINGS = {'allowed': 'AnchorPolicy::Allow', 'forbidden': 'AnchorPolicy::Forbid'}
ANCHOR_SIZE_MAPPINGS = {'allowed': 'AnchorSizePolicy::Allow', 'forbidden': 'AnchorSizePolicy::Forbid'}
ALLOWED_COLOR_TYPES_MAPPINGS = {'absolute': 'CSS::ColorType::Absolute', 'current': 'CSS::ColorType::Current', 'system': 'CSS::ColorType::System'}
ALLOWED_IMAGE_TYPES_MAPPINGS = {'url': 'AllowedImageType::URLFunction', 'image-set': 'AllowedImageType::ImageSet', 'generated': 'AllowedImageType::GeneratedImage'}
ALLOWED_URL_MODIFIER_MAPPINGS = {'crossorigin': 'AllowedURLModifiers::CrossOrigin', 'integrity': 'AllowedURLModifiers::Integrity', 'referrerpolicy': 'AllowedURLModifiers::ReferrerPolicy'}

class ReferenceTerm:
    builtins = BuiltinSchema(
        BuiltinSchema.Entry('angle',
            BuiltinSchema.RangeParameter('value-range'),
            BuiltinSchema.StringParameter('unitless-zero', mappings=UNITLESS_ZERO_MAPPINGS, default='forbidden')),
        BuiltinSchema.Entry('length',
            BuiltinSchema.RangeParameter('value-range'),
            BuiltinSchema.StringParameter('unitless-zero', mappings=UNITLESS_ZERO_MAPPINGS, default='allowed')),
        BuiltinSchema.Entry('length-percentage',
            BuiltinSchema.RangeParameter('value-range'),
            BuiltinSchema.StringParameter('unitless-zero', mappings=UNITLESS_ZERO_MAPPINGS, default='allowed'),
            BuiltinSchema.StringParameter('anchor', mappings=ANCHOR_MAPPINGS, default='forbidden'),
            BuiltinSchema.StringParameter('anchor-size', mappings=ANCHOR_SIZE_MAPPINGS, default='forbidden')),
        BuiltinSchema.Entry('time',
            BuiltinSchema.RangeParameter('value-range')),
        BuiltinSchema.Entry('integer',
            BuiltinSchema.RangeParameter('value-range')),
        BuiltinSchema.Entry('number',
            BuiltinSchema.RangeParameter('value-range')),
        BuiltinSchema.Entry('percentage',
            BuiltinSchema.RangeParameter('value-range')),
        BuiltinSchema.Entry('resolution',
            BuiltinSchema.RangeParameter('value-range')),
        BuiltinSchema.Entry('number-or-percentage-resolved-to-number',
            BuiltinSchema.RangeParameter('value-range')),
        BuiltinSchema.Entry('position'),
        BuiltinSchema.Entry('color',
            BuiltinSchema.StringParameter('allowed-types', mappings=ALLOWED_COLOR_TYPES_MAPPINGS, default=['absolute', 'current', 'system'])),
        BuiltinSchema.Entry('image',
            BuiltinSchema.StringParameter('allowed-types', mappings=ALLOWED_IMAGE_TYPES_MAPPINGS, default=['url', 'image-set', 'generated'])),
        BuiltinSchema.Entry('string'),
        BuiltinSchema.Entry('custom-ident',
            BuiltinSchema.StringParameter('excluding')),
        BuiltinSchema.Entry('dashed-ident'),
        BuiltinSchema.Entry('url',
            BuiltinSchema.StringParameter('allowed-modifiers', mappings=ALLOWED_URL_MODIFIER_MAPPINGS)),
        BuiltinSchema.Entry('feature-tag-value'),
        BuiltinSchema.Entry('variation-tag-value'),
        BuiltinSchema.Entry('unicode-range-token'),
    )

    class StringParameter:
        def __init__(self, name, value):
            self.name = name
            self.value = value

        def __str__(self):
            if self.value:
                return str(self.name) + '=' + str(self.value)
            return str(self.name)

    class RangeParameter:
        def __init__(self, min, max):
            self.min = min
            self.max = max

        def __str__(self):
            return '[' + str(self.min) + ',' + str(self.max) + ']'

    class Parameter:
        @staticmethod
        def from_node(node):
            if type(node) is BNFReferenceNode.StringAttribute:
                return ReferenceTerm.StringParameter(node.name, node.value)
            if type(node) is BNFReferenceNode.RangeAttribute:
                return ReferenceTerm.RangeParameter(node.min, node.max)
            raise Exception(f"Unknown reference term attribute '{node}'.")

    def __init__(self, name, is_internal, is_function_reference, parameters, *, annotation=None, override_function=None):
        # Store the first (and perhaps only) part as the reference's name (e.g. for <length-percentage [0,inf] unitless-allowed> store 'length-percentage').
        self.name = Name(name)

        # Store whether this is an 'internal' reference (e.g. as indicated by the double angle brackets <<values>>).
        self.is_internal = is_internal

        # Store whether this is a function reference (e.g. as indicated by function notation <rect()>).
        self.is_function_reference = is_function_reference

        # Store any remaining parts as the parameters (e.g. for <length-percentage [0,inf] unitless-allowed> store ['[0,inf]', 'unitless-allowed']).
        self.parameters = parameters

        # Check name and parameters against the builtins schemas to verify if they are well formed.
        self.builtin = ReferenceTerm.builtins.validate_and_construct_if_builtin(self.name, self.parameters)

        # Store an explicit override function to call if provided.
        self.override_function = override_function

        # Additional annotations defined on the reference.
        self.annotation = annotation

        self.settings_flag = None
        self._process_annotation(annotation)

    def __str__(self):
        if self.is_function_reference:
            name = self.name.name + '()'
        else:
            name = self.name.name
        base = ' '.join([name] + list(stringify_iterable(self.parameters)))
        if self.is_internal:
            return f"<<{base}>>" + self.stringified_annotation
        return f"<{base}>" + self.stringified_annotation

    def __repr__(self):
        return self.__str__()

    @property
    def stringified_annotation(self):
        if not self.annotation:
            return ''
        return str(self.annotation)

    def _process_annotation(self, annotation):
        if not annotation:
            return
        for directive in annotation.directives:
            if directive.name == 'settings-flag':
                self.settings_flag = directive.value[0]
            else:
                raise Exception(f"Unknown reference annotation directive '{directive}'.")

    @staticmethod
    def from_node(node):
        assert(type(node) is BNFReferenceNode)
        return ReferenceTerm(node.name, node.is_internal, node.is_function_reference, [ReferenceTerm.Parameter.from_node(attribute) for attribute in node.attributes], annotation=node.annotation)

    def perform_fixups(self, all_rules):
        # Replace a reference with the term it references if it can be found.
        name_for_lookup = str(self)
        if name_for_lookup in all_rules.rules_by_name:
            return all_rules.rules_by_name[name_for_lookup].grammar.root_term.perform_fixups(all_rules)
        return self

    def perform_fixups_for_values_references(self, values):
        # NOTE: The actual name in the grammar is "<<values>>", which we store as is_internal + 'values'.
        if self.is_internal and self.name.name == "values":
            return MatchOneTerm.from_values(values)
        return self

    @property
    def is_builtin(self):
        return self.builtin is not None

    @property
    def supported_keywords(self):
        return set()

    @property
    def has_non_builtin_reference_terms(self):
        return not self.is_builtin


# LiteralTerm represents a direct match of a literal character or string. The
# syntax in the CSS specifications is either a bare delimiter character or a
# string surrounded by single quotes.
#
#   e.g. "'['" or ","
#
class LiteralTerm:
    def __init__(self, value):
        self.value = value

    def __str__(self):
        return f"'{self.value}'"

    def __repr__(self):
        return self.__str__()

    @staticmethod
    def from_node(node):
        assert(type(node) is BNFLiteralNode)
        return LiteralTerm(node.value)

    def perform_fixups(self, all_rules):
        return self

    def perform_fixups_for_values_references(self, values):
        return self

    @property
    def supported_keywords(self):
        return set()

    @property
    def has_non_builtin_reference_terms(self):
        return False


# KeywordTerm represents a direct keyword match. The syntax in the CSS specifications
# is a bare string.
#
#   e.g. "auto" or "box"
#
class KeywordTerm:
    def __init__(self, value, *, annotation=None, aliased_to=None, comment=None, settings_flag=None, status=None):
        self.value = value
        self.aliased_to = aliased_to
        self.comment = comment
        self.settings_flag = settings_flag
        self.status = status
        self.annotation = annotation

        self._process_annotation(annotation)

    def __str__(self):
        return self.value.name + self.stringified_annotation

    def __repr__(self):
        return self.__str__()

    @property
    def stringified_annotation(self):
        if not self.annotation:
            return ''
        return str(self.annotation)

    def _process_annotation(self, annotation):
        if not annotation:
            return
        for directive in annotation.directives:
            if directive.name == 'aliased-to':
                self.aliased_to = ValueKeywordName(directive.value[0])
            elif directive.name == 'settings-flag':
                self.settings_flag = directive.value[0]
            else:
                raise Exception(f"Unknown keyword annotation directive '{directive}'.")

    @staticmethod
    def from_node(node):
        assert(type(node) is BNFKeywordNode)
        return KeywordTerm(ValueKeywordName(node.keyword), annotation=node.annotation)

    def perform_fixups(self, all_rules):
        return self

    def perform_fixups_for_values_references(self, values):
        return self

    @property
    def supported_keywords(self):
        return {self.value.name}

    @property
    def has_non_builtin_reference_terms(self):
        return False

    @property
    def requires_state(self):
        return self.settings_flag or self.status == "internal"

    @property
    def is_eligible_for_fast_path(self):
        # Keyword terms that are aliased as not eligible for the fast path as the fast
        # path can only support a basic predicate.
        return not self.aliased_to

    @property
    def name(self):
        return self.value.name


# MatchOneTerm represents a set of terms, only one of which can match. The
# syntax in the CSS specifications is a '|' between terms.
#
#   e.g. "auto" | "reverse" | "<angle unitless-allowed unitless-zero-allowed>"
#
class MatchOneTerm:
    def __init__(self, subterms, *, annotation=None):
        self.subterms = subterms
        self.annotation = annotation

        self.settings_flag = None
        self._process_annotation(annotation)

    def __str__(self):
        return f"[ {' | '.join(stringify_iterable(self.subterms))} ]" + self.stringified_annotation

    def __repr__(self):
        return self.__str__()

    @property
    def stringified_annotation(self):
        if not self.annotation:
            return ''
        return str(self.annotation)

    def _process_annotation(self, annotation):
        if not annotation:
            return
        for directive in annotation.directives:
            if directive.name == 'settings-flag':
                self.settings_flag = directive.value[0]
            else:
                raise Exception(f"Unknown grouping annotation directive '{directive}'.")

    @staticmethod
    def from_node(node):
        assert(type(node) is BNFGroupingNode)
        assert(node.kind is BNFGroupingNode.Kind.MATCH_ONE)

        return MatchOneTerm(list(compact_map(lambda member: Term.from_node(member), node.members)), annotation=node.annotation)

    @staticmethod
    def from_values(values):
        return MatchOneTerm(list(compact_map(lambda value: value.keyword_term, values)))

    def perform_fixups(self, all_rules):
        self.subterms = MatchOneTerm.simplify(subterm.perform_fixups(all_rules) for subterm in self.subterms)

        if len(self.subterms) == 1:
            return self.subterms[0]
        return self

    def perform_fixups_for_values_references(self, values):
        self.subterms = MatchOneTerm.simplify(subterm.perform_fixups_for_values_references(values) for subterm in self.subterms)

        if len(self.subterms) == 1:
            return self.subterms[0]
        return self

    @staticmethod
    def simplify(subterms):
        simplified_subterms = []
        for subterm in subterms:
            if isinstance(subterm, MatchOneTerm):
                if subterm.settings_flag:
                    raise Exception(f"Simplifying {subterm} is not yet supported due to inability to merge settings flags down from MatchOneTerm to its subterms on simplification.")
                simplified_subterms += subterm.subterms
            else:
                simplified_subterms += [subterm]
        return simplified_subterms

    @property
    def has_keyword_term(self):
        return any(isinstance(subterm, KeywordTerm) for subterm in self.subterms)

    @property
    def has_only_keyword_terms(self):
        return all(isinstance(subterm, KeywordTerm) for subterm in self.subterms)

    @property
    def keyword_terms(self):
        return (subterm for subterm in self.subterms if isinstance(subterm, KeywordTerm))

    @property
    def fast_path_keyword_terms(self):
        return (subterm for subterm in self.keyword_terms if subterm.is_eligible_for_fast_path)

    @property
    def has_fast_path_keyword_terms(self):
        return any(subterm.is_eligible_for_fast_path for subterm in self.keyword_terms)

    @property
    def has_only_fast_path_keyword_terms(self):
        return all(isinstance(subterm, KeywordTerm) and subterm.is_eligible_for_fast_path for subterm in self.subterms)

    @property
    def supported_keywords(self):
        result = set()
        for subterm in self.subterms:
            result.update(subterm.supported_keywords)
        return result

    @property
    def has_non_builtin_reference_terms(self):
        return any(subterm.has_non_builtin_reference_terms for subterm in self.subterms)


# MatchOneOrMoreAnyOrderTerm represents matching a list of provided terms
# where one or more terms must match in any order. The syntax in the CSS
# specifications places the terms in brackets (these can be elided at the
# root level) with ' || ' between each term.
#
#   e.g. "[ <length> || <string> || <number> ]"
#
class MatchOneOrMoreAnyOrderTerm:
    def __init__(self, subterms, kind, annotation):
        self.subterms = subterms
        self.kind = kind
        self.annotation = annotation

        self.type = "CSSValueList"
        self.preserve_order = False
        self.single_value_optimization = True
        self.settings_flag = None
        self._process_annotation(annotation)

    def __str__(self):
        return '[ ' + self.stringified_without_brackets + ' ]' + self.stringified_annotation

    def __repr__(self):
        return self.__str__()

    @property
    def stringified_without_brackets(self):
        return ' || '.join(stringify_iterable(self.subterms))

    @property
    def stringified_annotation(self):
        if not self.annotation:
            return ''
        return str(self.annotation)

    def _process_annotation(self, annotation):
        if not annotation:
            return
        for directive in annotation.directives:
            if directive.name == 'type':
                self.type = directive.value[0]
            elif directive.name == 'preserve-order':
                self.preserve_order = True
            elif directive.name == 'no-single-item-opt':
                self.single_value_optimization = False
            elif directive.name == 'settings-flag':
                self.settings_flag = directive.value[0]
            else:
                raise Exception(f"Unknown grouping annotation directive '{directive}'.")

    @staticmethod
    def from_node(node):
        assert(type(node) is BNFGroupingNode)
        return MatchOneOrMoreAnyOrderTerm(list(compact_map(lambda member: Term.from_node(member), node.members)), node.kind, node.annotation)

    def perform_fixups(self, all_rules):
        self.subterms = [subterm.perform_fixups(all_rules) for subterm in self.subterms]

        if len(self.subterms) == 1:
            return self.subterms[0]
        return self

    def perform_fixups_for_values_references(self, values):
        self.subterms = [subterm.perform_fixups_for_values_references(values) for subterm in self.subterms]

        if len(self.subterms) == 1:
            return self.subterms[0]
        return self

    @property
    def supported_keywords(self):
        result = set()
        for subterm in self.subterms:
            result.update(subterm.supported_keywords)
        return result

    @property
    def has_non_builtin_reference_terms(self):
        return any(term.has_non_builtin_reference_terms for term in self.subterms)


# MatchAllOrderedTerm represents matching a list of provided terms
# that all must be matched in the specified order. The syntax in the
# CSS specifications places the terms in brackets (these can be elided
# at the root level) with spaces between each term.
#
#   e.g. "[ <length> <length> ]"
#
class MatchAllOrderedTerm:
    def __init__(self, subterms, kind, annotation):
        self.subterms = subterms
        self.kind = kind
        self.annotation = annotation

        self.type = "CSSValueList"
        self.single_value_optimization = True
        self.settings_flag = None
        self._process_annotation(annotation)

    def __str__(self):
        return '[ ' + self.stringified_without_brackets + ' ]' + self.stringified_annotation

    def __repr__(self):
        return self.__str__()

    @property
    def stringified_without_brackets(self):
        return ' '.join(stringify_iterable(self.subterms))

    @property
    def stringified_annotation(self):
        if not self.annotation:
            return ''
        return str(self.annotation)

    def _process_annotation(self, annotation):
        if not annotation:
            return
        for directive in annotation.directives:
            if directive.name == 'type':
                self.type = directive.value[0]
            elif directive.name == 'no-single-item-opt':
                self.single_value_optimization = False
            elif directive.name == 'settings-flag':
                self.settings_flag = directive.value[0]
            else:
                raise Exception(f"Unknown grouping annotation directive '{directive}'.")

    @staticmethod
    def from_node(node):
        assert(type(node) is BNFGroupingNode)
        return MatchAllOrderedTerm(list(compact_map(lambda member: Term.from_node(member), node.members)), node.kind, node.annotation)

    def perform_fixups(self, all_rules):
        self.subterms = [subterm.perform_fixups(all_rules) for subterm in self.subterms]

        if len(self.subterms) == 1:
            return self.subterms[0]
        return self

    def perform_fixups_for_values_references(self, values):
        self.subterms = [subterm.perform_fixups_for_values_references(values) for subterm in self.subterms]

        if len(self.subterms) == 1:
            return self.subterms[0]
        return self

    @property
    def supported_keywords(self):
        result = set()
        for subterm in self.subterms:
            result.update(subterm.supported_keywords)
        return result

    @property
    def has_non_builtin_reference_terms(self):
        return any(term.has_non_builtin_reference_terms for term in self.subterms)


# MatchAllAnyOrderTerm represents matching a list of provided terms
# that all must be matched, but can be done so in any order. The
# syntax in the CSS specifications places the terms in brackets (these
# can be elided at the root level) with ' && ' between each term.
#
#   e.g. "[ <foo> && <bar> && <baz> ]"
#
class MatchAllAnyOrderTerm:
    def __init__(self, subterms, kind, annotation):
        self.subterms = subterms
        self.kind = kind
        self.annotation = annotation

        self.type = "CSSValueList"
        self.preserve_order = False
        self.single_value_optimization = True
        self.settings_flag = None
        self._process_annotation(annotation)

    def __str__(self):
        return '[ ' + self.stringified_without_brackets + ' ]' + self.stringified_annotation

    def __repr__(self):
        return self.__str__()

    @property
    def stringified_without_brackets(self):
        return ' && '.join(stringify_iterable(self.subterms))

    @property
    def stringified_annotation(self):
        if not self.annotation:
            return ''
        return str(self.annotation)

    def _process_annotation(self, annotation):
        if not annotation:
            return
        for directive in annotation.directives:
            if directive.name == 'type':
                self.type = directive.value[0]
            elif directive.name == 'preserve-order':
                self.preserve_order = True
            elif directive.name == 'no-single-item-opt':
                self.single_value_optimization = False
            elif directive.name == 'settings-flag':
                self.settings_flag = directive.value[0]
            else:
                raise Exception(f"Unknown grouping annotation directive '{directive}'.")

    @staticmethod
    def from_node(node):
        assert(type(node) is BNFGroupingNode)
        return MatchAllAnyOrderTerm(list(compact_map(lambda member: Term.from_node(member), node.members)), node.kind, node.annotation)

    def perform_fixups(self, all_rules):
        self.subterms = [subterm.perform_fixups(all_rules) for subterm in self.subterms]

        if len(self.subterms) == 1:
            return self.subterms[0]
        return self

    def perform_fixups_for_values_references(self, values):
        self.subterms = [subterm.perform_fixups_for_values_references(values) for subterm in self.subterms]

        if len(self.subterms) == 1:
            return self.subterms[0]
        return self

    @property
    def supported_keywords(self):
        result = set()
        for subterm in self.subterms:
            result.update(subterm.supported_keywords)
        return result

    @property
    def has_non_builtin_reference_terms(self):
        return any(term.has_non_builtin_reference_terms for term in self.subterms)


# OptionalTerm represents matching a term that is allowed to
# be omitted. The syntax in the CSS specifications uses a
# trailing '?'.
#
#   e.g. "<length>?" or "[ <length> <string> ]?"
#
class OptionalTerm:
    def __init__(self, subterm, *, annotation):
        self.subterm = subterm
        self.annotation = annotation

        self._process_annotation(annotation)

    def __str__(self):
        return str(self.subterm) + '?' + self.stringified_annotation

    def __repr__(self):
        return self.__str__()

    @property
    def stringified_annotation(self):
        if not self.annotation:
            return ''
        return str(self.annotation)

    def _process_annotation(self, annotation):
        if not annotation:
            return
        for directive in annotation.directives:
            raise Exception(f"Unknown optional term annotation directive '{directive}'.")

    @staticmethod
    def wrapping_term(subterm, *, annotation):
        return OptionalTerm(subterm, annotation=annotation)

    def perform_fixups(self, all_rules):
        self.subterm = self.subterm.perform_fixups(all_rules)
        return self

    def perform_fixups_for_values_references(self, values):
        self.subterm = self.subterm.perform_fixups_for_values_references(values)
        return self

    @property
    def supported_keywords(self):
        return self.subterm.supported_keywords

    @property
    def has_non_builtin_reference_terms(self):
        return self.subterm.has_non_builtin_reference_terms


# UnboundedRepetitionTerm represents matching a list of terms
# separated by either spaces or commas. The syntax in the CSS
# specifications uses a trailing 'multiplier' such as '#', '*',
# '+', and '{A,}'.
#
#   e.g. "<length>#" or "<length>+"
#
class UnboundedRepetitionTerm:
    def __init__(self, repeated_term, *, separator, min, annotation):
        self.repeated_term = repeated_term
        self.separator = separator
        self.min = min
        self.annotation = annotation

        self.type = "CSSValueList"
        self.single_value_optimization = True
        self.settings_flag = None
        self._process_annotation(annotation)

    def __str__(self):
        return str(self.repeated_term) + self.stringified_suffix + self.stringified_annotation

    def __repr__(self):
        return self.__str__()

    @property
    def stringified_suffix(self):
        if self.separator == ' ':
            if self.min == 0:
                return '*'
            elif self.min == 1:
                return '+'
            else:
                return '{' + str(self.min) + ',}'
        if self.separator == ',':
            if self.min == 1:
                return '#'
            else:
                return '#{' + str(self.min) + ',}'
        raise Exception(f"Unknown UnboundedRepetitionTerm with separator '{self.separator}'")

    @property
    def stringified_annotation(self):
        if not self.annotation:
            return ''
        return str(self.annotation)

    def _process_annotation(self, annotation):
        if not annotation:
            return
        for directive in annotation.directives:
            if directive.name == 'type':
                self.type = directive.value[0]
            elif directive.name == 'no-single-item-opt':
                self.single_value_optimization = False
            elif directive.name == 'settings-flag':
                self.settings_flag = directive.value[0]
            else:
                raise Exception(f"Unknown multiplier annotation directive '{directive}'.")

    @staticmethod
    def wrapping_term(term, *, separator, min, annotation):
        return UnboundedRepetitionTerm(term, separator=separator, min=min, annotation=annotation)

    def perform_fixups(self, all_rules):
        self.repeated_term = self.repeated_term.perform_fixups(all_rules)
        return self

    def perform_fixups_for_values_references(self, values):
        self.repeated_term = self.repeated_term.perform_fixups_for_values_references(values)
        return self

    @property
    def supported_keywords(self):
        return self.repeated_term.supported_keywords

    @property
    def has_non_builtin_reference_terms(self):
        return self.repeated_term.has_non_builtin_reference_terms


# BoundedRepetitionTerm represents matching a list of terms
# separated by either spaces or commas where the list of terms
# has a length between provided upper and lower bounds . The
# syntax in the CSS specifications uses a trailing 'multiplier'
# range '{A,B}' with a '#' prefix for comma separation. If the
# upper and lower bounds are equal, it can be written alone
# without the comma.
#
#   e.g. "<length>{1,2}" or "<length>#{3,5}" or "<length>{2}"
#
class BoundedRepetitionTerm:
    def __init__(self, repeated_term, *, separator, min, max, annotation):
        self.repeated_term = repeated_term
        self.separator = separator
        self.min = min
        self.max = max
        self.annotation = annotation

        self.type = "CSSValueList"
        self.single_value_optimization = True
        self.default = None
        self.settings_flag = None
        self._process_annotation(annotation)

    def __str__(self):
        return str(self.repeated_term) + self.stringified_suffix + self.stringified_annotation

    def __repr__(self):
        return self.__str__()

    @property
    def stringified_suffix(self):
        if self.separator == ' ':
            if self.min == self.max:
                return '{' + str(self.min) + '}'
            return '{' + str(self.min) + ',' + str(self.max) + '}'
        if self.separator == ',':
            if self.min == self.max:
                return '#{' + str(self.min) + '}'
            return '#{' + str(self.min) + ',' + str(self.max) + '}'
        raise Exception(f"Unknown BoundedRepetitionTerm with separator '{self.separator}'")

    @property
    def stringified_annotation(self):
        if not self.annotation:
            return ''
        return str(self.annotation)

    def _process_annotation(self, annotation):
        if not annotation:
            return
        for directive in annotation.directives:
            if directive.name == 'type':
                self.type = directive.value[0]
            elif directive.name == 'no-single-item-opt':
                self.single_value_optimization = False
            elif directive.name == 'default':
                self.default = directive.value[0]
            elif directive.name == 'settings-flag':
                self.settings_flag = directive.value[0]
            else:
                raise Exception(f"Unknown bounded repetition term annotation directive '{directive}'.")

    @staticmethod
    def wrapping_term(term, *, separator, min, max, annotation):
        return BoundedRepetitionTerm(term, separator=separator, min=min, max=max, annotation=annotation)

    def perform_fixups(self, all_rules):
        self.repeated_term = self.repeated_term.perform_fixups(all_rules)
        return self

    def perform_fixups_for_values_references(self, values):
        self.repeated_term = self.repeated_term.perform_fixups_for_values_references(values)
        return self

    @property
    def supported_keywords(self):
        return self.repeated_term.supported_keywords

    @property
    def has_non_builtin_reference_terms(self):
        return self.repeated_term.has_non_builtin_reference_terms


# FunctionTerm represents matching a use of the CSS function call syntax
# which provides a way for specifications to differentiate groups by
# name. The syntax in the CSS specifications is an identifier followed
# by parenthesis with an optional group term inside the parenthesis.
#
#   e.g. "rect(<length>#{4})" or "ray()"
#
class FunctionTerm:
    def __init__(self, name, parameter_group_term, *, annotation):
        self.name = name
        self.parameter_group_term = parameter_group_term
        self.annotation = annotation

        self.settings_flag = None
        self._process_annotation(annotation)

    def __str__(self):
        return str(self.name) + '(' + str(self.parameter_group_term) + ')' + self.stringified_annotation

    def __repr__(self):
        return self.__str__()

    @property
    def stringified_annotation(self):
        if not self.annotation:
            return ''
        return str(self.annotation)

    def _process_annotation(self, annotation):
        if not annotation:
            return
        for directive in annotation.directives:
            if directive.name == 'settings-flag':
                self.settings_flag = directive.value[0]
            else:
                raise Exception(f"Unknown function term annotation directive '{directive}'.")

    @staticmethod
    def from_node(node):
        assert(type(node) is BNFFunctionNode)
        return FunctionTerm(ValueKeywordName(node.name), Term.from_node(node.parameter_group), annotation=node.annotation)

    def perform_fixups(self, all_rules):
        self.parameter_group_term = self.parameter_group_term.perform_fixups(all_rules)
        return self

    def perform_fixups_for_values_references(self, values):
        self.parameter_group_term = self.parameter_group_term.perform_fixups_for_values_references(values)
        return self

    @property
    def supported_keywords(self):
        return self.parameter_group_term.supported_keywords

    @property
    def has_non_builtin_reference_terms(self):
        return self.parameter_group_term.has_non_builtin_reference_terms


# Container for the name and root term for a grammar. Used for both shared rules and property specific grammars.
class Grammar:
    def __init__(self, name, root_term):
        self.name = name
        self.root_term = root_term
        self._fast_path_keyword_terms_sorted_by_name = None

    def __str__(self):
        return f"{self.name} {self.root_term}"

    def __repr__(self):
        return self.__str__()

    @staticmethod
    def from_string(parsing_context, key_path, name, string):
        assert(type(string) is str)
        return Grammar(name, Term.from_node(BNFParser(parsing_context, key_path, string).parse()))

    def perform_fixups(self, all_rules):
        self.root_term = self.root_term.perform_fixups(all_rules)

    def perform_fixups_for_values_references(self, values):
        self.root_term = self.root_term.perform_fixups_for_values_references(values)

    def check_against_values(self, values):
        if self.has_non_builtin_reference_terms:
            # If the grammar has any  non-builtin references, the grammar is incomplete and this check cannot be performed.
            return

        keywords_supported_by_grammar = self.supported_keywords
        keywords_listed_as_values = frozenset(value.name for value in values)

        mark = "'"
        keywords_only_in_grammar = keywords_supported_by_grammar - keywords_listed_as_values
        if keywords_only_in_grammar:
            raise Exception(f"ERROR: '{self.name}' Found some keywords in parser grammar not list in 'values' array: ({ ', '.join(quote_iterable((keyword for keyword in keywords_only_in_grammar), mark=mark)) })")
        keywords_only_in_values = keywords_listed_as_values - keywords_supported_by_grammar
        if keywords_only_in_values:
            raise Exception(f"ERROR: '{self.name}' Found some keywords in 'values' array not supported by the parser grammar: ({ ', '.join(quote_iterable((keyword for keyword in keywords_only_in_values), mark=mark)) })")

    @property
    def has_fast_path_keyword_terms(self):
        if isinstance(self.root_term, MatchOneTerm) and self.root_term.has_fast_path_keyword_terms:
            return True
        return False

    @property
    def has_only_keyword_terms(self):
        if isinstance(self.root_term, MatchOneTerm) and self.root_term.has_only_keyword_terms:
            return True
        return False

    @property
    def has_only_fast_path_keyword_terms(self):
        if isinstance(self.root_term, MatchOneTerm) and self.root_term.has_only_fast_path_keyword_terms:
            return True
        return False

    @property
    def fast_path_keyword_terms(self):
        if isinstance(self.root_term, MatchOneTerm):
            return self.root_term.fast_path_keyword_terms
        return []

    @property
    def fast_path_keyword_terms_sorted_by_name(self):
        if not self._fast_path_keyword_terms_sorted_by_name:
            self._fast_path_keyword_terms_sorted_by_name = sorted(self.fast_path_keyword_terms, key=functools.cmp_to_key(StyleProperties._sort_with_prefixed_properties_last))
        return self._fast_path_keyword_terms_sorted_by_name

    @property
    def supported_keywords(self):
        return self.root_term.supported_keywords

    @property
    def has_non_builtin_reference_terms(self):
        return self.root_term.has_non_builtin_reference_terms


# A shared grammar rule and metadata describing it. Part of the set of rules tracked by SharedGrammarRules.
class SharedGrammarRule:
    schema = Schema(
        Schema.Entry("exported", allowed_types=[bool], default_value=False),
        Schema.Entry("grammar", allowed_types=[str]),
        Schema.Entry("grammar-unused", allowed_types=[str]),
        Schema.Entry("grammar-unused-reason", allowed_types=[str]),
        Schema.Entry("grammar-function", allowed_types=[str]),
        Schema.Entry("specification", allowed_types=[dict], convert_to=Specification),
        Schema.Entry("status", allowed_types=[dict, str], convert_to=Status),
    )

    def __init__(self, name, **dictionary):
        SharedGrammarRule.schema.set_attributes_from_dictionary(dictionary, instance=self)
        self.name = name
        self.name_for_methods = Name(name[1:-1])

    def __str__(self):
        return self.name

    def __repr__(self):
        return self.__str__()

    @staticmethod
    def from_json(parsing_context, key_path, name, json_value):
        assert(type(json_value) is dict)
        SharedGrammarRule.schema.validate_dictionary(parsing_context, f"{key_path}.{name}", json_value, label=f"SharedGrammarRule")

        if json_value.get("grammar"):
            for entry_name in ["grammar-function"]:
                if entry_name in json_value:
                    raise Exception(f"{key_path} can't have both 'parser-grammar' and '{entry_name}.")
            json_value["grammar"] = Grammar.from_string(parsing_context, f"{key_path}.{name}", name, json_value["grammar"])

        if json_value.get("grammar-unused"):
            if "grammar-unused-reason" not in json_value:
                raise Exception(f"{key_path} must have 'grammar-unused-reason' specified when using 'grammar-unused'.")
            # If we have a "grammar-unused" specified, we still process it to ensure that at least it is syntactically valid, we just
            # won't actually use it for generation.
            json_value["grammar-unused"] = Grammar.from_string(parsing_context, f"{key_path}.{name}", name, json_value["grammar-unused"])

        if json_value.get("grammar-unused-reason"):
            if "grammar-unused" not in json_value:
                raise Exception(f"{key_path} must have 'grammar-unused' specified when using 'grammar-unused-reason'.")

        if json_value.get("grammar-function"):
            if "grammar-unused" not in json_value:
                raise Exception(f"{key_path} must have 'grammar-unused' specified when using 'grammar-function'.")
            json_value["grammar"] = Grammar(name, ReferenceTerm(name[1:-1] + "-override-function", False, False, [], override_function=json_value["grammar-function"]))

        return SharedGrammarRule(name, **json_value)

    def perform_fixups(self, all_rules):
        self.grammar.perform_fixups(all_rules)


# Shared grammar rules used to aid in defining property specific grammars.
class SharedGrammarRules:
    def __init__(self, rules):
        self.rules = rules
        self.rules_by_name = {rule.name: rule for rule in rules}
        self._all = None

        self._perform_fixups()

    def __str__(self):
        return "SharedGrammarRules"

    def __repr__(self):
        return self.__str__()

    @staticmethod
    def from_json(parsing_context, key_path, json_value):
        return SharedGrammarRules(list(compact_map(lambda item: SharedGrammarRule.from_json(parsing_context, key_path, item[0], item[1]), json_value.items())))

    # Updates any references to other rules with a direct reference to the rule object.
    def _perform_fixups(self):
        for rule in self.rules:
            rule.perform_fixups(self)

    # Returns the set of all shared property rules sorted by name.
    @property
    def all(self):
        if not self._all:
            self._all = sorted(self.rules, key=lambda rule: rule.name)
        return self._all


class ParsingContext:
    class TopLevelObject:
        schema = Schema(
            Schema.Entry("categories", allowed_types=[dict], required=True),
            Schema.Entry("instructions", allowed_types=[list], required=True),
            Schema.Entry("properties", allowed_types=[dict], required=True),
            Schema.Entry("descriptors", allowed_types=[dict], required=True),
            Schema.Entry("shared-grammar-rules", allowed_types=[dict], required=True),
        )

    def __init__(self, json_value, *, defines_string, parsing_for_codegen, check_unused_grammars_values, verbose):
        ParsingContext.TopLevelObject.schema.validate_dictionary(self, "$", json_value, label="top level object")

        self.json_value = json_value
        self.conditionals = frozenset((defines_string or '').split(' '))
        self.parsing_for_codegen = parsing_for_codegen
        self.check_unused_grammars_values = check_unused_grammars_values
        self.verbose = verbose
        self.parsed_shared_grammar_rules = None
        self.parsed_properties_and_descriptors = None

    def parse_shared_grammar_rules(self):
        self.parsed_shared_grammar_rules = SharedGrammarRules.from_json(self, "$shared-grammar-rules", self.json_value["shared-grammar-rules"])

    def parse_properties_and_descriptors(self):
        self.parsed_properties_and_descriptors = PropertiesAndDescriptors.from_json(self, properties_json_value=self.json_value["properties"], descriptors_json_value=self.json_value["descriptors"])

    def is_enabled(self, *, conditional):
        if conditional[0] == '!':
            return conditional[1:] not in self.conditionals
        return conditional in self.conditionals

    def select_enabled_variant(self, variants, *, label):
        for variant in variants:
            if "enable-if" not in variant:
                raise Exception(f"Invalid conditional definition for '{label}'. No 'enable-if' property found.")

            if self.is_enabled(conditional=variant["enable-if"]):
                return variant

        raise Exception(f"Invalid conditional definition for '{label}'. No 'enable-if' property matched the active set.")


# MARK: - Code Generation

class GenerationContext:
    def __init__(self, properties_and_descriptors, shared_grammar_rules, *, verbose, gperf_executable):
        self.properties_and_descriptors = properties_and_descriptors
        self.shared_grammar_rules = shared_grammar_rules
        self.verbose = verbose
        self.gperf_executable = gperf_executable

    # Shared generation constants.

    number_of_predefined_properties = 2

    # Shared generator templates.

    def generate_heading(self, *, to):
        to.write("// This file is automatically generated from CSSProperties.json by the process-css-properties.py script. Do not edit it.")
        to.newline()

    def generate_required_header_pragma(self, *, to):
        to.write(f"#pragma once")
        to.newline()

    def generate_open_namespaces(self, *, to, namespaces):
        for namespace in namespaces:
            if not namespace:
                to.write(f"namespace {{")
            else:
                to.write(f"namespace {namespace} {{")
        to.newline()

    def generate_close_namespaces(self, *, to, namespaces):
        for namespace in namespaces:
            if not namespace:
                to.write(f"}} // namespace (anonymous)")
            else:
                to.write(f"}} // namespace {namespace}")
        to.newline()

    def generate_open_namespace(self, *, to, namespace):
        self.generate_open_namespaces(to=to, namespaces=[namespace])

    def generate_close_namespace(self, *, to, namespace):
        self.generate_close_namespaces(to=to, namespaces=[namespace])

    class Namespaces:
        def __init__(self, generation_context, to, namespaces):
            self.generation_context = generation_context
            self.to = to
            self.namespaces = namespaces

        def __enter__(self):
            self.generation_context.generate_open_namespaces(to=self.to, namespaces=self.namespaces)

        def __exit__(self, exc_type, exc_value, traceback):
            self.generation_context.generate_close_namespaces(to=self.to, namespaces=self.namespaces)

    def namespace(self, namespace, *, to):
        return GenerationContext.Namespaces(self, to, [namespace])

    def namespaces(self, namespaces, *, to):
        return GenerationContext.Namespaces(self, to, namespaces)

    def generate_using_namespace_declarations(self, *, to, namespaces):
        for namespace in namespaces:
            to.write(f"using namespace {namespace};")
        to.newline()

    def generate_includes(self, *, to, headers=[], system_headers=[]):
        for header in headers:
            to.write(f"#include \"{header}\"")
        for header in system_headers:
            to.write(f"#include {header}")
        to.newline()

    def generate_cpp_required_includes(self, *, to, header):
        self.generate_includes(to=to, headers=["config.h", header])

    def generate_forward_declarations(self, *, to, structs=[], classes=[]):
        for struct in structs:
            to.write(f"struct {struct};")
        for class_ in classes:
            to.write(f"class {class_};")
        to.newline()

    def generate_function_declaration(self, *, to, function_specifiers=[], return_type, function_name, argument_types=[], function_qualifiers=[]):
        to.write(
              f"{''.join(map(lambda x: x + ' ', function_specifiers))}"
            + f"{return_type} "
            + f"{function_name}"
            + f"({', '.join(argument_types)})"
            + f"{''.join(map(lambda x: ' ' + x, function_qualifiers))};"
        )

    def generate_property_id_switch_function(self, *, to, signature, iterable, mapping, default, mapping_to_property=lambda p: p, prologue=None, epilogue=None):
        to.write(f"{signature}")
        to.write(f"{{")

        with to.indent():
            if prologue:
                to.write(prologue)

            to.write(f"switch (id) {{")

            for item in iterable:
                to.write(f"case {mapping_to_property(item).id}:")
                with to.indent():
                    to.write(f"{mapping(item)}")

            to.write(f"default:")
            with to.indent():
                to.write(f"{default}")
            to.write(f"}}")

            if epilogue:
                to.write(epilogue)

        to.write(f"}}")
        to.newline()

    def generate_property_id_switch_function_bool(self, *, to, signature, iterable, mapping_to_property=lambda p: p):
        to.write(f"{signature}")
        to.write(f"{{")

        with to.indent():
            to.write(f"switch (id) {{")

            for item in iterable:
                to.write(f"case {mapping_to_property(item).id}:")

            with to.indent():
                to.write(f"return true;")

            to.write(f"default:")
            with to.indent():
                to.write(f"return false;")

            to.write(f"}}")
        to.write(f"}}")
        to.newline()

    def generate_property_id_bit_set(self, *, to, name, iterable, mapping_to_property=lambda p: p):
        to.write(f"const WTF::BitSet<cssPropertyIDEnumValueCount> {name} = ([]() -> WTF::BitSet<cssPropertyIDEnumValueCount> {{")

        with to.indent():
            to.write(f"WTF::BitSet<cssPropertyIDEnumValueCount> result;")

            for item in iterable:
                to.write(f"result.set({mapping_to_property(item).id});")

            to.write(f"return result;")
        to.write(f"}})();")
        to.newline()


# Generates `CSSPropertyInitialValuesGeneratedInlines.h`.
class GenerateCSSPropertyInitialValues:
    def __init__(self, generation_context):
        self.generation_context = generation_context

    @property
    def properties_and_descriptors(self):
        return self.generation_context.properties_and_descriptors

    @property
    def properties(self):
        return self.generation_context.properties_and_descriptors.style_properties

    def generate(self):
        self.generate_css_property_initial_values_generated_inlines_h()

    # MARK: - Helper generator functions for CSSPropertyInitialValuesGeneratedInlines.h

    def _generate_css_property_initial_values_generated_inlines_h_types(self, *, to):
        to.write(f"struct InitialNumericValue {{")
        with to.indent():
            to.write(f"double number;")
            to.write(f"CSSUnitType type {{ CSSUnitType::CSS_NUMBER }};")
        to.write(f"}};")
        to.newline()

        to.write(f"using InitialValue = Variant<CSSValueID, InitialNumericValue>;")
        to.newline()

    def _generate_css_property_initial_values_generated_inlines_h_initial_value_for_longhand(self, *, to):
        to.write(f"static constexpr InitialValue initialValueForLonghand(CSSPropertyID longhand)")
        to.write(f"{{")
        with to.indent():
            to.write(f"switch (longhand) {{")

            initial_value_to_property_list = {}
            for property in self.properties_and_descriptors.style_properties.all_non_shorthands:
                if property.codegen_properties.internal_only:
                    continue
                if property.initial is None:
                    if self.generation_context.verbose:
                        to.write(f"// Skipping {property.id_without_scope}, initial is None")
                    continue
                if len(property.initial.list) != 1:
                    if self.generation_context.verbose:
                        to.write(f"// Skipping {property.id_without_scope}, initial is a list with multiple values {property.initial.list}")
                    continue
                if isinstance(property.initial.list[0], SpecialLiteral):
                    if self.generation_context.verbose:
                        to.write(f"// Skipping {property.id_without_scope}, initial is a special value {property.initial.list}")
                    continue
                initial_value_to_property_list.setdefault(property.initial, [])
                initial_value_to_property_list[property.initial].append(property)

            for initial, group in initial_value_to_property_list.items():
                for property in sorted(group, key=lambda x: x.id):
                    to.write(f"case {property.id}:")

                with to.indent():
                    if isinstance(initial.list[0], NumericLiteral):
                        to.write(f"return InitialNumericValue {{ {initial.list[0].digits}, {initial.list[0].cpp_unit_type} }};")
                    elif isinstance(initial.list[0], ValueKeywordName):
                        to.write(f"return {initial.list[0].id_without_scope};")

            to.write(f"default:")
            with to.indent():
                to.write(f"RELEASE_ASSERT_NOT_REACHED();")

            to.write(f"}}")
        to.write(f"}}")

    def generate_css_property_initial_values_generated_inlines_h(self):
        with open('CSSPropertyInitialValuesGeneratedInlines.h', 'w') as output_file:
            writer = Writer(output_file)

            self.generation_context.generate_heading(
                to=writer
            )

            self.generation_context.generate_required_header_pragma(
                to=writer
            )

            self.generation_context.generate_includes(
                to=writer,
                headers=[
                    "CSSPropertyNames.h",
                    "CSSUnits.h",
                    "CSSValueKeywords.h",
                ],
                system_headers=[
                    "<wtf/Variant.h>",
                ]
            )

            with self.generation_context.namespace("WebCore", to=writer):
                self._generate_css_property_initial_values_generated_inlines_h_types(
                    to=writer
                )

                self._generate_css_property_initial_values_generated_inlines_h_initial_value_for_longhand(
                    to=writer
                )


# Generates `CSSPropertyNames.h` and `CSSPropertyNames.cpp`.
class GenerateCSSPropertyNames:
    def __init__(self, generation_context):
        self.generation_context = generation_context

    @property
    def properties_and_descriptors(self):
        return self.generation_context.properties_and_descriptors

    @property
    def properties(self):
        return self.generation_context.properties_and_descriptors.style_properties

    def generate(self):
        self.generate_css_property_names_h()
        self.generate_css_property_names_gperf()
        self.run_gperf()

    # Runs `gperf` on the output of the generated file CSSPropertyNames.gperf
    def run_gperf(self):
        if not self.generation_context.gperf_executable:
            return

        gperf_result_code = subprocess.call([self.generation_context.gperf_executable, '--key-positions=*', '-D', '-n', '-s', '2', 'CSSPropertyNames.gperf', '--output-file=CSSPropertyNames.cpp'])
        if gperf_result_code != 0:
            raise Exception(f"Error when generating CSSPropertyNames.cpp from CSSPropertyNames.gperf: {gperf_result_code}")

    # MARK: - Helper generator functions for CSSPropertyNames.h

    def _generate_css_property_names_gperf_prelude(self, *, to):
        to.write("%{")

        self.generation_context.generate_heading(
            to=to
        )

        self.generation_context.generate_cpp_required_includes(
            to=to,
            header="CSSPropertyNames.h"
        )

        self.generation_context.generate_includes(
            to=to,
            headers=[
                "BoxSides.h",
                "CSSParserContext.h",
                "CSSProperty.h",
                "CSSValueKeywords.h",
                "DeprecatedGlobalSettings.h",
                "Settings.h",
            ],
            system_headers=[
                "<string.h>",
                "<wtf/ASCIICType.h>",
                "<wtf/Hasher.h>",
                "<wtf/text/AtomString.h>",
                "<wtf/text/TextStream.h>",
            ]
        )

        to.write_block("""
            IGNORE_WARNINGS_BEGIN("implicit-fallthrough")
            WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

            // Older versions of gperf like to use the `register` keyword.
            #define register
            """)

        self.generation_context.generate_open_namespace(
            to=to,
            namespace="WebCore"
        )

        to.write_block("""\
            static_assert(cssPropertyIDEnumValueCount <= (std::numeric_limits<uint16_t>::max() + 1), "CSSPropertyID should fit into uint16_t.");
            """)

        all_computed_property_ids = (f"{property.id}," for property in self.properties_and_descriptors.style_properties.all_computed)
        to.write(f"const std::array<CSSPropertyID, {count_iterable(self.properties_and_descriptors.style_properties.all_computed)}> computedPropertyIDs {{")
        with to.indent():
            to.write_lines(all_computed_property_ids)
        to.write("};")
        to.newline()

        all_property_name_strings = quote_iterable((f"{property.name}" for property in self.properties_and_descriptors.all_unique), suffix="_s,")
        to.write(f"constexpr ASCIILiteral propertyNameStrings[numCSSProperties] = {{")
        with to.indent():
            to.write_lines(all_property_name_strings)
        to.write("};")
        to.newline()

        to.write("%}")

    def _generate_css_property_names_gperf_footing(self, *, to):
        self.generation_context.generate_close_namespace(
            to=to,
            namespace="WebCore"
        )

        to.write("WTF_ALLOW_UNSAFE_BUFFER_USAGE_END")
        to.write("IGNORE_WARNINGS_END")

    def _generate_gperf_declarations(self, *, to):
        to.write_block("""\
            %struct-type
            struct CSSPropertyHashTableEntry {
                const char* name;
                uint16_t id;
            };
            %language=C++
            %readonly-tables
            %global-table
            %7bit
            %compare-strncmp
            %define class-name CSSPropertyNamesHash
            %enum
            """)

    def _generate_gperf_keywords(self, *, to):
        # Use a set to automatically deduplicate entries that would cause gperf
        # hash collisions. This handles cases where the same alias (like
        # "font-stretch") is defined multiple times.
        all_entries_set = set()

        # Add unique property names.
        for property in self.properties_and_descriptors.all_unique:
            all_entries_set.add(f'{property.name}, {property.id}')

        # Add aliases.
        for property in self.properties_and_descriptors.all_properties_and_descriptors:
            for alias in property.aliases:
                all_entries_set.add(f'{alias}, {property.id}')

        # Sort for consistent output.
        all_property_names_and_aliases_with_ids = sorted(all_entries_set)

        to.write("%%")
        to.write_lines(all_property_names_and_aliases_with_ids)
        to.write("%%")

    def _generate_lookup_functions(self, *, to):
        to.write_block("""
            CSSPropertyID findCSSProperty(const char* characters, unsigned length)
            {
                auto* value = CSSPropertyNamesHash::in_word_set(characters, length);
                return value ? static_cast<CSSPropertyID>(value->id) : CSSPropertyID::CSSPropertyInvalid;
            }

            ASCIILiteral nameLiteral(CSSPropertyID id)
            {
                if (id < firstCSSProperty)
                    return { };
                unsigned index = id - firstCSSProperty;
                if (index >= numCSSProperties)
                    return { };
                return propertyNameStrings[index];
            }

            const AtomString& nameString(CSSPropertyID id)
            {
                if (id < firstCSSProperty)
                    return nullAtom();
                unsigned index = id - firstCSSProperty;
                if (index >= numCSSProperties)
                    return nullAtom();

                static NeverDestroyed<std::array<AtomString, numCSSProperties>> atomStrings;
                auto& string = atomStrings.get()[index];
                if (string.isNull())
                    string = propertyNameStrings[index];
                return string;
            }

            String nameForIDL(CSSPropertyID id)
            {
                Latin1Character characters[maxCSSPropertyNameLength];
                const char* nameForCSS = nameLiteral(id);
                if (!nameForCSS)
                    return emptyString();

                auto* propertyNamePointer = nameForCSS;
                auto* nextCharacter = characters;
                while (char character = *propertyNamePointer++) {
                    if (character == '-') {
                        char nextCharacter = *propertyNamePointer++;
                        if (!nextCharacter)
                            break;
                        character = (propertyNamePointer - 2 != nameForCSS) ? toASCIIUpper(nextCharacter) : nextCharacter;
                    }
                    *nextCharacter++ = character;
                }
                return std::span<const Latin1Character> { characters, nextCharacter };
            }

            """)

    def _generate_physical_logical_conversion_function(self, *, to, signature, source, destination, resolver_enum_prefix):
        source_as_id = PropertyName.convert_name_to_id(source)
        destination_as_id = PropertyName.convert_name_to_id(destination)

        to.write(f"{signature}")
        to.write(f"{{")
        with to.indent():
            to.write(f"switch (id) {{")

            for group_name, property_group in sorted(self.properties_and_descriptors.style_properties.logical_property_groups.items(), key=lambda x: x[0]):
                kind = property_group["kind"]
                kind_as_id = PropertyName.convert_name_to_id(kind)

                destinations = LogicalPropertyGroup.logical_property_group_resolvers[destination][kind]
                properties = [property_group[destination][a_destination].id for a_destination in destinations]

                for resolver, property in sorted(property_group[source].items(), key=lambda x: x[0]):
                    resolver_as_id = PropertyName.convert_name_to_id(resolver)
                    resolver_enum = f"{resolver_enum_prefix}{kind_as_id}::{resolver_as_id}"

                    to.write(f"case {property.id}: {{")
                    with to.indent():
                        to.write(f"static constexpr CSSPropertyID properties[{len(properties)}] = {{ {', '.join(properties)} }};")
                        to.write(f"return properties[static_cast<size_t>(map{kind_as_id}{source_as_id}To{destination_as_id}(writingMode, {resolver_enum}))];")
                    to.write(f"}}")

            to.write(f"default:")
            with to.indent():
                to.write(f"return id;")

            to.write(f"}}")
        to.write(f"}}")
        to.newline()

    def _generate_is_exposed_functions(self, *, to):
        self.generation_context.generate_property_id_switch_function(
            to=to,
            signature="static bool isExposedNotInvalidAndNotInternal(CSSPropertyID id, const CSSPropertySettings& settings)",
            iterable=self.properties_and_descriptors.all_unique_with_settings_flag,
            mapping=lambda p: f"return settings.{p.codegen_properties.settings_flag};",
            default="return true;"
        )

        self.generation_context.generate_property_id_switch_function(
            to=to,
            signature="static bool isExposedNotInvalidAndNotInternal(CSSPropertyID id, const Settings& settings)",
            iterable=self.properties_and_descriptors.all_unique_with_settings_flag,
            mapping=lambda p: f"return settings.{p.codegen_properties.settings_flag}();",
            default="return true;"
        )

        to.write_block("""\
            bool isExposed(CSSPropertyID id, const CSSPropertySettings* settings)
            {
                if (id == CSSPropertyID::CSSPropertyInvalid || isInternal(id))
                    return false;
                if (!settings)
                    return true;
                return isExposedNotInvalidAndNotInternal(id, *settings);
            }

            bool isExposed(CSSPropertyID id, const CSSPropertySettings& settings)
            {
                if (id == CSSPropertyID::CSSPropertyInvalid || isInternal(id))
                    return false;
                return isExposedNotInvalidAndNotInternal(id, settings);
            }

            bool isExposed(CSSPropertyID id, const Settings* settings)
            {
                if (id == CSSPropertyID::CSSPropertyInvalid || isInternal(id))
                    return false;
                if (!settings)
                    return true;
                return isExposedNotInvalidAndNotInternal(id, *settings);
            }

            bool isExposed(CSSPropertyID id, const Settings& settings)
            {
                if (id == CSSPropertyID::CSSPropertyInvalid || isInternal(id))
                    return false;
                return isExposedNotInvalidAndNotInternal(id, settings);
            }
        """)

    def _generate_is_inherited_property(self, *, to):
        all_inherited_and_ids = (f'{"true " if hasattr(property, "inherited") and property.inherited else "false"}, // {property.id}' for property in self.properties_and_descriptors.all_unique)

        to.write(f"constexpr bool isInheritedPropertyTable[cssPropertyIDEnumValueCount] = {{")
        with to.indent():
            to.write(f"false, // CSSPropertyID::CSSPropertyInvalid")
            to.write(f"true , // CSSPropertyID::CSSPropertyCustom")
            to.write_lines(all_inherited_and_ids)
        to.write(f"}};")

        to.write_block("""
            bool CSSProperty::isInheritedProperty(CSSPropertyID id)
            {
                ASSERT(id < cssPropertyIDEnumValueCount);
                ASSERT(id != CSSPropertyID::CSSPropertyInvalid);
                return isInheritedPropertyTable[id];
            }
            """)

    def _generate_are_in_same_logical_property_group_with_different_mappings_logic(self, *, to):
        to.write(f"bool CSSProperty::areInSameLogicalPropertyGroupWithDifferentMappingLogic(CSSPropertyID id1, CSSPropertyID id2)")
        to.write(f"{{")
        with to.indent():
            to.write(f"switch (id1) {{")

            for group_name, property_group in sorted(self.properties_and_descriptors.style_properties.logical_property_groups.items(), key=lambda x: x[0]):
                logical = property_group["logical"]
                physical = property_group["physical"]
                for first in [logical, physical]:
                    second = physical if first is logical else logical
                    for resolver, property in sorted(first.items(), key=lambda x: x[1].name):
                        to.write(f"case {property.id}:")

                    with to.indent():
                        to.write(f"switch (id2) {{")
                        to.write_lines((f"case {property.id}:" for _, property in sorted(second.items(), key=lambda x: x[1].name)))

                        with to.indent():
                            to.write(f"return true;")
                        to.write(f"default:")
                        with to.indent():
                            to.write(f"return false;")
                        to.write(f"}}")

            to.write(f"default:")
            with to.indent():
                to.write(f"return false;")
            to.write(f"}}")
        to.write(f"}}")
        to.newline()

    def _generate_animation_property_functions(self, *, to):
        self.generation_context.generate_property_id_switch_function_bool(
            to=to,
            signature="bool CSSProperty::animationUsesNonAdditiveOrCumulativeInterpolation(CSSPropertyID id)",
            iterable=(p for p in self.properties_and_descriptors.style_properties.all if p.codegen_properties.animation_wrapper_requires_non_additive_or_cumulative_interpolation)
        )

        self.generation_context.generate_property_id_switch_function_bool(
            to=to,
            signature="bool CSSProperty::animationUsesNonNormalizedDiscreteInterpolation(CSSPropertyID id)",
            iterable=(p for p in self.properties_and_descriptors.style_properties.all if p.codegen_properties.animation_wrapper_requires_non_normalized_discrete_interpolation)
        )

        self.generation_context.generate_property_id_switch_function(
            to=to,
            signature="bool CSSProperty::animationIsAccelerated(CSSPropertyID id, [[maybe_unused]] const Settings& settings)",
            iterable=(p for p in self.properties_and_descriptors.style_properties.all if p.codegen_properties.animation_wrapper_acceleration),
            mapping=lambda p: f"return {self._property_is_accelerated_return_clause(p)};",
            default="return false;"
        )

        to.write(f"std::span<const CSSPropertyID> CSSProperty::allAcceleratedAnimationProperties([[maybe_unused]] const Settings& settings)")
        to.write(f"{{")

        with to.indent():
            to.write(f"static constexpr std::array propertiesExcludingThreadedOnly {{")

            with to.indent():
                has_threaded_acceleration = False
                for property in self.properties_and_descriptors.style_properties.all:
                    if property.codegen_properties.animation_wrapper_acceleration is None:
                        continue
                    if property.codegen_properties.animation_wrapper_acceleration == 'threaded-only':
                        has_threaded_acceleration = True
                        continue
                    to.write(f"{property.id},")

            to.write(f"}};")

            if has_threaded_acceleration:
                to.write(f"static constexpr std::array propertiesIncludingThreadedOnly {{")

                with to.indent():
                    for property in self.properties_and_descriptors.style_properties.all:
                        if property.codegen_properties.animation_wrapper_acceleration is None:
                            continue
                        to.write(f"{property.id},")

                to.write(f"}};")

                to.write(f"if (settings.threadedScrollDrivenAnimationsEnabled() || settings.threadedTimeBasedAnimationsEnabled())")
                with to.indent():
                    to.write(f"return std::span<const CSSPropertyID> {{ propertiesIncludingThreadedOnly }};")

            to.write(f"return std::span<const CSSPropertyID> {{ propertiesExcludingThreadedOnly }};")

        to.write(f"}}")
        to.newline()

    def _generate_css_property_settings_constructor(self, *, to):
        first_settings_initializer, *remaining_settings_initializers = [f"{flag} {{ settings.{flag}() }}" for flag in self.properties_and_descriptors.settings_flags]

        to.write(f"CSSPropertySettings::CSSPropertySettings(const Settings& settings)")
        with to.indent():
            to.write(f": {first_settings_initializer}")
            to.write_lines((f", {initializer}" for initializer in remaining_settings_initializers))

        to.write(f"{{")
        to.write(f"}}")
        to.newline()

    def _generate_css_property_settings_operator_equal(self, *, to):
        first, *middle, last = (f"a.{flag} == b.{flag}" for flag in self.properties_and_descriptors.settings_flags)

        to.write(f"bool operator==(const CSSPropertySettings& a, const CSSPropertySettings& b)")
        to.write(f"{{")
        with to.indent():
            to.write(f"return {first}")
            with to.indent():
                to.write_lines((f"&& {expression}" for expression in middle))
                to.write(f"&& {last};")

        to.write(f"}}")
        to.newline()

    def _generate_css_property_settings_hasher(self, *, to):
        first, *middle, last = [f"settings.{flag}" for flag in self.properties_and_descriptors.settings_flags]

        to.write(f"void add(Hasher& hasher, const CSSPropertySettings& settings)")
        to.write(f"{{")
        with to.indent():
            to.write(f"add(hasher, WTF::packBools(")
            with to.indent():
                to.write(f"{first},")
                to.write_lines((f"{expression}," for expression in middle))
                to.write(f"{last}")
            to.write(f"));")
        to.write(f"}}")
        to.newline()

    def _generate_css_property_id_text_stream(self, *, to):
        to.write_block("""
            TextStream& operator<<(TextStream& stream, CSSPropertyID property)
            {
                return stream << nameLiteral(property);
            }
            """)

    def _generate_valid_keywords_for_property(self, *, to):
        # Generate static arrays of valid keyword CSSValueIDs for each property
        # that has a 'values' array in CSSProperties.json. This is used by the
        # Inspector to provide completions for properties that aren't keyword-fast-path
        # eligible but still have enumerated values.

        # First, collect all properties with values and generate static arrays for them
        properties_with_values = []
        seen_array_names = set()
        for prop in self.properties_and_descriptors.style_properties.all:
            if hasattr(prop, 'values') and prop.values:
                # Filter to only include values that have a valid keyword_term (actual keywords)
                keyword_values = [value for value in prop.values if hasattr(value, 'value_keyword_name') and value.value_keyword_name]
                if keyword_values:
                    array_name = f"validKeywordsFor{prop.property_name.name_for_methods}"
                    # Skip if we've already generated an array with this name (handles aliases)
                    if array_name not in seen_array_names:
                        seen_array_names.add(array_name)
                        properties_with_values.append((prop, keyword_values, array_name))

        # Generate static arrays for each property with values
        for prop, keywordValues, array_name in properties_with_values:
            value_ids = [value.value_keyword_name.id for value in keywordValues]
            to.write(f"static constexpr std::array {array_name} {{")
            with to.indent():
                for value_id in value_ids:
                    to.write(f"{value_id},")
            to.write("};")
            to.newline()

        # Generate the switch function - include all properties, even those with duplicate array names
        all_properties_with_values = []
        for prop in self.properties_and_descriptors.style_properties.all:
            if hasattr(prop, 'values') and prop.values:
                keyword_values = [value for value in prop.values if hasattr(value, 'value_keyword_name') and value.value_keyword_name]
                if keyword_values:
                    array_name = f"validKeywordsFor{prop.property_name.name_for_methods}"
                    all_properties_with_values.append((prop, keyword_values, array_name))

        to.write("std::span<const CSSValueID> CSSProperty::validKeywordsForProperty(CSSPropertyID id)")
        to.write("{")
        with to.indent():
            to.write("switch (id) {")
            for prop, keyword_values, array_name in all_properties_with_values:
                to.write(f"case {prop.id}:")
                with to.indent():
                    to.write(f"return std::span<const CSSValueID> {{ {array_name} }};")
            to.write("default:")
            with to.indent():
                to.write("return { };")
            to.write("}")
        to.write("}")
        to.newline()

        # Generate isKeywordValidForPropertyValues function to check settings flags.
        # This is used by the Inspector to filter keywords based on enabled settings.

        # Collect properties that have any keywords with settings-flags
        properties_with_settings_flags = []
        for prop, keyword_values, array_name in all_properties_with_values:
            # Check if any keyword has a settings_flag
            keywords_with_flags = [(value, value.settings_flag) for value in keyword_values if value.settings_flag]
            if keywords_with_flags:
                properties_with_settings_flags.append((prop, keyword_values))

        # Generate helper functions for properties with settings-flagged keywords
        for prop, keyword_values in properties_with_settings_flags:
            func_name = f"isKeywordValidFor{prop.property_name.name_for_methods}Values"
            to.write(f"static bool {func_name}(CSSValueID keyword, const CSSParserContext& context)")
            to.write("{")
            with to.indent():
                to.write("switch (keyword) {")

                # Group keywords by their settings_flag (or lack thereof)
                # Keywords without settings_flag always return true
                keywords_without_flag = [value for value in keyword_values if not value.settings_flag]
                keywords_with_flag = [value for value in keyword_values if value.settings_flag]

                if keywords_without_flag:
                    for value in keywords_without_flag:
                        to.write(f"case {value.value_keyword_name.id}:")
                    with to.indent():
                        to.write("return true;")

                # Group keywords by their settings_flag for efficient switch generation
                from collections import defaultdict
                flag_to_keywords = defaultdict(list)
                for value in keywords_with_flag:
                    flag_to_keywords[value.settings_flag].append(value)

                for flag, keywordValues in flag_to_keywords.items():
                    for value in keywordValues:
                        to.write(f"case {value.value_keyword_name.id}:")
                    with to.indent():
                        # Check if this is a function call (e.g., DeprecatedGlobalSettings::attachmentElementEnabled())
                        if "::" in flag or "(" in flag:
                            to.write(f"return {flag};")
                        else:
                            to.write(f"return context.{flag};")

                to.write("default:")
                with to.indent():
                    to.write("return false;")
                to.write("}")
            to.write("}")
            to.newline()

        # Generate the main isKeywordValidForPropertyValues switch function
        to.write("bool CSSProperty::isKeywordValidForPropertyValues(CSSPropertyID id, CSSValueID keyword, const CSSParserContext& context)")
        to.write("{")
        with to.indent():
            to.write("switch (id) {")

            for prop, keyword_values, array_name in all_properties_with_values:
                to.write(f"case {prop.id}:")
                with to.indent():
                    # Check if this property has any keywords with settings flags
                    has_settings_flags = any(value.settings_flag for value in keyword_values)
                    if has_settings_flags:
                        func_name = f"isKeywordValidFor{prop.property_name.name_for_methods}Values"
                        to.write(f"return {func_name}(keyword, context);")
                    else:
                        # No settings flags, just check if keyword is in the valid set
                        to.write(f"return std::ranges::find({array_name}, keyword) != {array_name}.end();")

            to.write("default:")
            with to.indent():
                to.write("return false;")
            to.write("}")
        to.write("}")
        to.newline()

    def _term_matches_number_or_integer(self, term):
        if isinstance(term, MatchOneTerm):
            return any(self._term_matches_number_or_integer(inner_term) for inner_term in term.subterms)
        elif isinstance(term, MatchOneOrMoreAnyOrderTerm):
            return any(self._term_matches_number_or_integer(inner_term) for inner_term in term.subterms)
        elif isinstance(term, MatchAllOrderedTerm):
            any_term_matches = False
            for inner_term in term.subterms:
                if self._term_matches_number_or_integer(inner_term):
                    any_term_matches = True
                elif not isinstance(inner_term, OptionalTerm):
                    return False
            return any_term_matches
        elif isinstance(term, MatchAllAnyOrderTerm):
            any_term_matches = False
            for inner_term in term.subterms:
                if self._term_matches_number_or_integer(inner_term):
                    any_term_matches = True
                elif not isinstance(inner_term, OptionalTerm):
                    return False
            return any_term_matches
        elif isinstance(term, OptionalTerm):
            return self._term_matches_number_or_integer(term.subterm)
        elif isinstance(term, UnboundedRepetitionTerm):
            return self._term_matches_number_or_integer(term.repeated_term) and term.min < 2
        elif isinstance(term, BoundedRepetitionTerm):
            return self._term_matches_number_or_integer(term.repeated_term) and term.min < 2
        elif isinstance(term, ReferenceTerm):
            return term.name.name == "number" or term.name.name == "integer" or term.name.name == "number-or-percentage-resolved-to-number"
        elif isinstance(term, FunctionTerm):
            return False
        elif isinstance(term, LiteralTerm):
            return False
        elif isinstance(term, KeywordTerm):
            return False
        else:
            raise Exception(f"Unknown term type - {type(term)} - {term}")

    def _property_matches_number_or_integer(self, p):
        if p.codegen_properties.parser_function_allows_number_or_integer_input:
            return True
        if not p.codegen_properties.parser_grammar:
            return False
        return self._term_matches_number_or_integer(p.codegen_properties.parser_grammar.root_term)

    def _property_is_accelerated_return_clause(self, p):
        if p.codegen_properties.animation_wrapper_acceleration == 'threaded-only':
            return "settings.threadedScrollDrivenAnimationsEnabled() || settings.threadedTimeBasedAnimationsEnabled()"
        return "true"

    def generate_css_property_names_gperf(self):
        with open('CSSPropertyNames.gperf', 'w') as output_file:
            writer = Writer(output_file)

            self._generate_css_property_names_gperf_prelude(
                to=writer
            )

            self._generate_gperf_declarations(
                to=writer
            )

            self._generate_gperf_keywords(
                to=writer
            )

            self._generate_lookup_functions(
                to=writer
            )

            self.generation_context.generate_property_id_switch_function_bool(
                to=writer,
                signature="bool isInternal(CSSPropertyID id)",
                iterable=(p for p in self.properties_and_descriptors.all_unique if p.codegen_properties.internal_only)
            )

            self._generate_is_exposed_functions(
                to=writer
            )

            self._generate_is_inherited_property(
                to=writer
            )

            self.generation_context.generate_property_id_switch_function(
                to=writer,
                signature="CSSPropertyID cascadeAliasProperty(CSSPropertyID id)",
                iterable=(p for p in self.properties_and_descriptors.style_properties.all if p.codegen_properties.cascade_alias),
                mapping=lambda p: f"return {p.codegen_properties.cascade_alias.id};",
                default="return id;"
            )

            self.generation_context.generate_property_id_switch_function(
                to=writer,
                signature="Vector<String> CSSProperty::aliasesForProperty(CSSPropertyID id)",
                iterable=(p for p in self.properties_and_descriptors.style_properties.all if p.codegen_properties.aliases),
                mapping=lambda p: f"return {{ {', '.join(quote_iterable(p.codegen_properties.aliases, suffix='_s'))} }};",
                default="return { };"
            )

            self.generation_context.generate_property_id_bit_set(
                to=writer,
                name="CSSProperty::colorProperties",
                iterable=(p for p in self.properties_and_descriptors.style_properties.all if p.codegen_properties.color_property)
            )

            physical_properties = []
            for _, property_group in sorted(self.generation_context.properties_and_descriptors.style_properties.logical_property_groups.items(), key=lambda x: x[0]):
                kind = property_group["kind"]
                destinations = LogicalPropertyGroup.logical_property_group_resolvers["physical"][kind]
                for property in [property_group["physical"][a_destination] for a_destination in destinations]:
                    physical_properties.append(property)

            self.generation_context.generate_property_id_bit_set(
                to=writer,
                name="CSSProperty::physicalProperties",
                iterable=sorted(list(set(physical_properties)), key=lambda x: x.id)
            )

            self.generation_context.generate_property_id_switch_function(
                to=writer,
                signature="char16_t CSSProperty::listValuedPropertySeparator(CSSPropertyID id)",
                iterable=(p for p in self.properties_and_descriptors.style_properties.all if p.codegen_properties.separator),
                mapping=lambda p: f"return '{ p.codegen_properties.separator[0] }';",
                default="break;",
                epilogue="return '\\0';"
            )

            self.generation_context.generate_property_id_switch_function_bool(
                to=writer,
                signature="bool CSSProperty::allowsNumberOrIntegerInput(CSSPropertyID id)",
                iterable=(p for p in self.properties_and_descriptors.style_properties.all if self._property_matches_number_or_integer(p))
            )

            self.generation_context.generate_property_id_switch_function_bool(
                to=writer,
                signature="bool CSSProperty::disablesNativeAppearance(CSSPropertyID id)",
                iterable=(p for p in self.properties_and_descriptors.style_properties.all if p.codegen_properties.disables_native_appearance)
            )

            for group_name, property_group in sorted(self.generation_context.properties_and_descriptors.style_properties.logical_property_groups.items(), key=lambda x: x[0]):
                properties = set()
                for kind in ["logical", "physical"]:
                    for property in property_group[kind].values():
                        properties.add(property)
                        if property in self.generation_context.properties_and_descriptors.style_properties.shorthand_by_longhand:
                            properties.add(self.generation_context.properties_and_descriptors.style_properties.shorthand_by_longhand[property])

                group_id = PropertyName.convert_name_to_id(group_name)
                self.generation_context.generate_property_id_switch_function_bool(
                    to=writer,
                    signature=f"bool CSSProperty::is{group_id}Property(CSSPropertyID id)",
                    iterable=sorted(properties, key=lambda x: x.name)
                )

            self.generation_context.generate_property_id_switch_function_bool(
                to=writer,
                signature="bool CSSProperty::isInLogicalPropertyGroup(CSSPropertyID id)",
                iterable=self.properties_and_descriptors.style_properties.all_in_logical_property_group
            )

            self._generate_are_in_same_logical_property_group_with_different_mappings_logic(
                to=writer
            )

            self._generate_physical_logical_conversion_function(
                to=writer,
                signature="CSSPropertyID CSSProperty::resolveDirectionAwareProperty(CSSPropertyID id, WritingMode writingMode)",
                source="logical",
                destination="physical",
                resolver_enum_prefix="LogicalBox"
            )

            self._generate_physical_logical_conversion_function(
                to=writer,
                signature="CSSPropertyID CSSProperty::unresolvePhysicalProperty(CSSPropertyID id, WritingMode writingMode)",
                source="physical",
                destination="logical",
                resolver_enum_prefix="Box"
            )

            self.generation_context.generate_property_id_switch_function_bool(
                to=writer,
                signature="bool CSSProperty::isDescriptorOnly(CSSPropertyID id)",
                iterable=self.properties_and_descriptors.all_descriptor_only
            )

            self.generation_context.generate_property_id_switch_function_bool(
                to=writer,
                signature="bool CSSProperty::acceptsQuirkyColor(CSSPropertyID id)",
                iterable=(p for p in self.properties_and_descriptors.style_properties.all if p.codegen_properties.accepts_quirky_color)
            )

            self.generation_context.generate_property_id_switch_function_bool(
                to=writer,
                signature="bool CSSProperty::acceptsQuirkyLength(CSSPropertyID id)",
                iterable=(p for p in self.properties_and_descriptors.style_properties.all if p.codegen_properties.accepts_quirky_length)
            )

            self.generation_context.generate_property_id_switch_function_bool(
                to=writer,
                signature="bool CSSProperty::acceptsQuirkyAngle(CSSPropertyID id)",
                iterable=(p for p in self.properties_and_descriptors.style_properties.all if p.codegen_properties.accepts_quirky_angle)
            )

            self._generate_animation_property_functions(
                to=writer
            )

            self._generate_css_property_settings_constructor(
                to=writer
            )

            self._generate_css_property_settings_operator_equal(
                to=writer
            )

            self._generate_css_property_settings_hasher(
                to=writer
            )

            self._generate_css_property_id_text_stream(
                to=writer
            )

            self._generate_valid_keywords_for_property(
                to=writer
            )

            self._generate_css_property_names_gperf_footing(
                to=writer
            )

    # MARK: - Helper generator functions for CSSPropertyNames.h

    def _generate_css_property_names_h_property_constants(self, *, to):
        to.write(f"enum CSSPropertyID : uint16_t {{")
        with to.indent():
            to.write(f"CSSPropertyInvalid = 0,")
            to.write(f"CSSPropertyCustom = 1,")

            first = GenerationContext.number_of_predefined_properties
            count = GenerationContext.number_of_predefined_properties
            max_length = 0
            first_shorthand_property = None
            last_shorthand_property = None
            first_top_priority_property = None
            last_top_priority_property = None
            first_high_priority_property = None
            last_high_priority_property = None
            first_medium_priority_property = None
            last_medium_priority_property = None
            first_low_priority_property = None
            last_low_priority_property = None
            first_logical_group_physical_property = None
            last_logical_group_physical_property = None
            first_logical_group_logical_property = None
            last_logical_group_logical_property = None

            for property in self.properties_and_descriptors.all_unique:
                if property.codegen_properties.longhands:
                    if not first_shorthand_property:
                        first_shorthand_property = property
                    last_shorthand_property = property
                elif property.codegen_properties.top_priority:
                    if not first_top_priority_property:
                        first_top_priority_property = property
                    last_top_priority_property = property
                elif property.codegen_properties.high_priority:
                    if not first_high_priority_property:
                        first_high_priority_property = property
                    last_high_priority_property = property
                elif property.codegen_properties.medium_priority:
                    if not first_medium_priority_property:
                        first_medium_priority_property = property
                    last_medium_priority_property = property
                elif not property.codegen_properties.logical_property_group:
                    if not first_low_priority_property:
                        first_low_priority_property = property
                    last_low_priority_property = property
                elif property.codegen_properties.logical_property_group.logic == 'physical':
                    if not first_logical_group_physical_property:
                        first_logical_group_physical_property = property
                    last_logical_group_physical_property = property
                elif property.codegen_properties.logical_property_group.logic == 'logical':
                    if not first_logical_group_logical_property:
                        first_logical_group_logical_property = property
                    last_logical_group_logical_property = property
                else:
                    raise Exception(f"{property.id_without_scope} is not part of any priority bucket. {property.codegen_properties.logical_property_group}")

                to.write(f"{property.id_without_scope} = {count},")

                count += 1
                max_length = max(len(property.name), max_length)

            num = count - first

        to.write(f"}};")
        to.newline()

        to.write(f"// Enum value of the first \"real\" CSS property, which excludes")
        to.write(f"// CSSPropertyInvalid and CSSPropertyCustom.")
        to.write(f"constexpr uint16_t firstCSSProperty = {first};")

        to.write(f"// Total number of enum values in the CSSPropertyID enum. If making an array")
        to.write(f"// that can be indexed into using the enum value, use this as the size.")
        to.write(f"constexpr uint16_t cssPropertyIDEnumValueCount = {count};")

        to.write(f"// Number of \"real\" CSS properties. This differs from cssPropertyIDEnumValueCount,")
        to.write(f"// as this doesn't consider CSSPropertyInvalid and CSSPropertyCustom.")
        to.write(f"constexpr uint16_t numCSSProperties = {num};")

        to.write(f"constexpr unsigned maxCSSPropertyNameLength = {max_length};")
        to.write(f"constexpr auto firstTopPriorityProperty = {first_top_priority_property.id};")
        to.write(f"constexpr auto lastTopPriorityProperty = {last_top_priority_property.id};")
        to.write(f"constexpr auto firstHighPriorityProperty = {first_high_priority_property.id};")
        to.write(f"constexpr auto lastHighPriorityProperty = {last_high_priority_property.id};")
        to.write(f"constexpr auto firstMediumPriorityProperty = {first_medium_priority_property.id};")
        to.write(f"constexpr auto lastMediumPriorityProperty = {last_medium_priority_property.id};")
        to.write(f"constexpr auto firstLowPriorityProperty = {first_low_priority_property.id};")
        to.write(f"constexpr auto lastLowPriorityProperty = {last_low_priority_property.id};")
        to.write(f"constexpr auto firstLogicalGroupPhysicalProperty = {first_logical_group_physical_property.id};")
        to.write(f"constexpr auto lastLogicalGroupPhysicalProperty = {last_logical_group_physical_property.id};")
        to.write(f"constexpr auto firstLogicalGroupLogicalProperty = {first_logical_group_logical_property.id};")
        to.write(f"constexpr auto lastLogicalGroupLogicalProperty = {last_logical_group_logical_property.id};")
        to.write(f"constexpr auto firstLogicalGroupProperty = firstLogicalGroupPhysicalProperty;")
        to.write(f"constexpr auto lastLogicalGroupProperty = lastLogicalGroupLogicalProperty;")
        to.write(f"constexpr auto firstShorthandProperty = {first_shorthand_property.id};")
        to.write(f"constexpr auto lastShorthandProperty = {last_shorthand_property.id};")
        to.write(f"constexpr uint16_t numCSSPropertyLonghands = firstShorthandProperty - firstCSSProperty;")
        to.newline()

        to.write(f"extern const std::array<CSSPropertyID, {count_iterable(self.properties_and_descriptors.style_properties.all_computed)}> computedPropertyIDs;")
        to.newline()

        to.write(f"template<CSSPropertyID C> struct PropertyNameConstant {{")
        with to.indent():
            to.write(f"static constexpr auto value = C;")
            to.write(f"constexpr bool operator==(const PropertyNameConstant&) const = default;")
            to.write(f"constexpr bool operator==(CSSPropertyID other) const {{ return value == other; }}")
        to.write(f"}};")
        to.newline()

    def _generate_css_property_names_h_property_settings(self, *, to):
        settings_variable_declarations = (f"bool {flag} : 1 {{ false }};" for flag in self.properties_and_descriptors.settings_flags)

        to.write(f"struct CSSPropertySettings {{")
        with to.indent():
            to.write(f"WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(CSSPropertySettings);")
            to.newline()

            to.write_lines(settings_variable_declarations)
            to.newline()

            to.write(f"CSSPropertySettings() = default;")
            to.write(f"explicit CSSPropertySettings(const Settings&);")
        to.write(f"}};")
        to.newline()

        to.write(f"bool operator==(const CSSPropertySettings&, const CSSPropertySettings&);")
        to.write(f"void add(Hasher&, const CSSPropertySettings&);")
        to.newline()

    def _generate_css_property_names_h_declarations(self, *, to):
        to.write_block("""\
            constexpr bool isLonghand(CSSPropertyID);
            bool isInternal(CSSPropertyID);
            bool isExposed(CSSPropertyID, const Settings*);
            bool isExposed(CSSPropertyID, const Settings&);
            bool isExposed(CSSPropertyID, const CSSPropertySettings*);
            bool isExposed(CSSPropertyID, const CSSPropertySettings&);

            CSSPropertyID findCSSProperty(const char* characters, unsigned length);
            ASCIILiteral nameLiteral(CSSPropertyID);
            const AtomString& nameString(CSSPropertyID);
            String nameForIDL(CSSPropertyID);

            CSSPropertyID cascadeAliasProperty(CSSPropertyID);

            template<CSSPropertyID first, CSSPropertyID last> struct CSSPropertiesRange {
                struct Iterator {
                    uint16_t index { static_cast<uint16_t>(first) };
                    constexpr CSSPropertyID operator*() const { return static_cast<CSSPropertyID>(index); }
                    constexpr Iterator& operator++() { ++index; return *this; }
                    constexpr bool operator==(std::nullptr_t) const { return index > static_cast<uint16_t>(last); }
                };
                static constexpr Iterator begin() { return { }; }
                static constexpr std::nullptr_t end() { return nullptr; }
                static constexpr uint16_t size() { return last - first + 1; }
            };
            using AllCSSPropertiesRange = CSSPropertiesRange<static_cast<CSSPropertyID>(firstCSSProperty), lastShorthandProperty>;
            using AllLonghandCSSPropertiesRange = CSSPropertiesRange<static_cast<CSSPropertyID>(firstCSSProperty), lastLogicalGroupProperty>;
            constexpr AllCSSPropertiesRange allCSSProperties() { return { }; }
            constexpr AllLonghandCSSPropertiesRange allLonghandCSSProperties() { return { }; }

            constexpr bool isLonghand(CSSPropertyID property)
            {
                return static_cast<uint16_t>(property) >= firstCSSProperty
                    && static_cast<uint16_t>(property) < static_cast<uint16_t>(firstShorthandProperty);
            }
            constexpr bool isShorthand(CSSPropertyID property)
            {
                return static_cast<uint16_t>(property) >= static_cast<uint16_t>(firstShorthandProperty)
                    && static_cast<uint16_t>(property) <= static_cast<uint16_t>(lastShorthandProperty);
            }

            constexpr bool isLogicalPropertyGroupProperty(CSSPropertyID property)
            {
                return static_cast<uint16_t>(property) >= static_cast<uint16_t>(firstLogicalGroupPhysicalProperty)
                    && static_cast<uint16_t>(property) <= static_cast<uint16_t>(lastLogicalGroupLogicalProperty);
            }

            constexpr bool isLogicalPropertyGroupPhysicalProperty(CSSPropertyID property)
            {
                return static_cast<uint16_t>(property) >= static_cast<uint16_t>(firstLogicalGroupPhysicalProperty)
                    && static_cast<uint16_t>(property) <= static_cast<uint16_t>(lastLogicalGroupPhysicalProperty);
            }

            constexpr bool isLogicalPropertyGroupLogicalProperty(CSSPropertyID property)
            {
                return static_cast<uint16_t>(property) >= static_cast<uint16_t>(firstLogicalGroupLogicalProperty)
                    && static_cast<uint16_t>(property) <= static_cast<uint16_t>(lastLogicalGroupLogicalProperty);
            }

            WTF::TextStream& operator<<(WTF::TextStream&, CSSPropertyID);
            """)

    def _generate_css_property_names_h_hash_traits(self, *, to):
        with self.generation_context.namespace("WTF", to=to):
            to.write_block("""\
                template<> struct DefaultHash<WebCore::CSSPropertyID> : IntHash<unsigned> { };

                template<> struct HashTraits<WebCore::CSSPropertyID> : GenericHashTraits<WebCore::CSSPropertyID> {
                    static const bool emptyValueIsZero = true;
                    static void constructDeletedValue(WebCore::CSSPropertyID& slot) { slot = static_cast<WebCore::CSSPropertyID>(std::numeric_limits<uint16_t>::max()); }
                    static bool isDeletedValue(WebCore::CSSPropertyID value) { return static_cast<uint16_t>(value) == std::numeric_limits<uint16_t>::max(); }
                };
                """)

    def _generate_css_property_names_h_iterator_traits(self, *, to):
        with self.generation_context.namespace("std", to=to):
            to.write_block("""\
                template<> struct iterator_traits<WebCore::AllCSSPropertiesRange::Iterator> { using value_type = WebCore::CSSPropertyID; };
                template<> struct iterator_traits<WebCore::AllLonghandCSSPropertiesRange::Iterator> { using value_type = WebCore::CSSPropertyID; };
                """)

    def generate_css_property_names_h(self):
        with open('CSSPropertyNames.h', 'w') as output_file:
            writer = Writer(output_file)

            self.generation_context.generate_heading(
                to=writer
            )

            self.generation_context.generate_required_header_pragma(
                to=writer
            )

            self.generation_context.generate_includes(
                to=writer,
                system_headers=[
                    "<array>",
                    "<wtf/HashFunctions.h>",
                    "<wtf/HashTraits.h>",
                ]
            )

            with self.generation_context.namespace("WebCore", to=writer):
                self.generation_context.generate_forward_declarations(
                    to=writer,
                    classes=["Settings"]
                )

                self._generate_css_property_names_h_property_constants(
                    to=writer
                )

                self._generate_css_property_names_h_property_settings(
                    to=writer
                )

                self._generate_css_property_names_h_declarations(
                    to=writer
                )

            self._generate_css_property_names_h_hash_traits(
                to=writer
            )

            self._generate_css_property_names_h_iterator_traits(
                to=writer
            )


# Generates `CSSStyleProperties+PropertyNames.idl`.
class GenerateCSSStylePropertiesPropertyNames:
    def __init__(self, generation_context):
        self.generation_context = generation_context

    @property
    def properties_and_descriptors(self):
        return self.generation_context.properties_and_descriptors

    def generate(self):
        self.generate_css_style_declaration_property_names_idl()

    # MARK: - Helper generator functions for CSSStyleProperties+PropertyNames.idl

    def _generate_css_style_declaration_property_names_idl_typedefs(self, *, to):
        to.write_block("""\
            typedef USVString CSSOMString;
            """)

    def _generate_css_style_declaration_property_names_idl_open_interface(self, *, to):
        to.write("partial interface CSSStyleProperties {")

    def _generate_css_style_declaration_property_names_idl_close_interface(self, *, to):
        to.write("};")

    def _convert_css_property_to_idl_attribute(name, *, lowercase_first):
        # https://drafts.csswg.org/cssom/#css-property-to-idl-attribute
        output = ""
        uppercase_next = False

        if lowercase_first:
            name = name[1:]

        for character in name:
            if character == "-":
                uppercase_next = True
            elif uppercase_next:
                uppercase_next = False
                output += character.upper()
            else:
                output += character

        return output

    def _generate_css_style_declaration_property_names_idl_section(self, *, to, comment, names_and_aliases_with_properties, variant, convert_to_idl_attribute, lowercase_first=None):
        to.write_block(comment)

        for name_or_alias, property in names_and_aliases_with_properties:
            if convert_to_idl_attribute:
                idl_attribute_name = GenerateCSSStylePropertiesPropertyNames._convert_css_property_to_idl_attribute(name_or_alias, lowercase_first=lowercase_first)
            else:
                idl_attribute_name = name_or_alias

            extended_attributes_values = [f"DelegateToSharedSyntheticAttribute=propertyValueFor{variant}IDLAttribute", "CallWith=PropertyName"]
            if property.codegen_properties.settings_flag:
                extended_attributes_values += [f"EnabledBySetting={property.codegen_properties.settings_flag}"]

            to.write(f"[CEReactions=Needed, {', '.join(extended_attributes_values)}] attribute [LegacyNullToEmptyString] CSSOMString {idl_attribute_name};")

    def generate_css_style_declaration_property_names_idl(self):
        with open('CSSStyleProperties+PropertyNames.idl', 'w') as output_file:
            writer = Writer(output_file)

            self.generation_context.generate_heading(
                to=writer
            )

            name_or_alias_to_property = {}
            for property in self.properties_and_descriptors.all_unique_non_internal_only:
                name_or_alias_to_property[property.name] = property
                for alias in property.aliases:
                    name_or_alias_to_property[alias] = property

            names_and_aliases_with_properties = sorted(list(name_or_alias_to_property.items()), key=lambda x: x[0])

            self._generate_css_style_declaration_property_names_idl_typedefs(
                to=writer
            )

            self._generate_css_style_declaration_property_names_idl_open_interface(
                to=writer
            )

            with writer.indent():
                self._generate_css_style_declaration_property_names_idl_section(
                    to=writer,
                    comment="""\
                        // For each CSS property property that is a supported CSS property, the following
                        // partial interface applies where camel-cased attribute is obtained by running the
                        // CSS property to IDL attribute algorithm for property.
                        // Example: font-size -> element.style.fontSize
                        // Example: -webkit-transform -> element.style.WebkitTransform
                        // [CEReactions] attribute [LegacyNullToEmptyString] CSSOMString _camel_cased_attribute;
                        """,
                    names_and_aliases_with_properties=names_and_aliases_with_properties,
                    variant="CamelCased",
                    convert_to_idl_attribute=True,
                    lowercase_first=False
                )

                self._generate_css_style_declaration_property_names_idl_section(
                    to=writer,
                    comment="""
                        // For each CSS property property that is a supported CSS property and that begins
                        // with the string -webkit-, the following partial interface applies where webkit-cased
                        // attribute is obtained by running the CSS property to IDL attribute algorithm for
                        // property, with the lowercase first flag set.
                        // Example: -webkit-transform -> element.style.webkitTransform
                        // [CEReactions] attribute [LegacyNullToEmptyString] CSSOMString _webkit_cased_attribute;
                        """,
                    names_and_aliases_with_properties=filter(lambda item: item[0].startswith("-webkit-"), names_and_aliases_with_properties),
                    variant="WebKitCased",
                    convert_to_idl_attribute=True,
                    lowercase_first=True
                )

                self._generate_css_style_declaration_property_names_idl_section(
                    to=writer,
                    comment="""
                        // For each CSS property property that is a supported CSS property, except for
                        // properties that have no "-" (U+002D) in the property name, the following partial
                        // interface applies where dashed attribute is property.
                        // Example: font-size -> element.style['font-size']
                        // Example: -webkit-transform -> element.style.['-webkit-transform']
                        // [CEReactions] attribute [LegacyNullToEmptyString] CSSOMString _dashed_attribute;
                        """,
                    names_and_aliases_with_properties=filter(lambda item: "-" in item[0], names_and_aliases_with_properties),
                    variant="Dashed",
                    convert_to_idl_attribute=False
                )

                self._generate_css_style_declaration_property_names_idl_section(
                    to=writer,
                    comment="""
                        // Non-standard. Special case properties starting with -epub- like is done for
                        // -webkit-, where attribute is obtained by running the CSS property to IDL attribute
                        // algorithm for property, with the lowercase first flag set.
                        // Example: -epub-caption-side -> element.style.epubCaptionSide
                        """,
                    names_and_aliases_with_properties=filter(lambda item: item[0].startswith("-epub-"), names_and_aliases_with_properties),
                    variant="EpubCased",
                    convert_to_idl_attribute=True,
                    lowercase_first=True
                )

            self._generate_css_style_declaration_property_names_idl_close_interface(
                to=writer
            )


# Generates `StyleBuilderGenerated.cpp`.
class GenerateStyleBuilderGenerated:
    def __init__(self, generation_context):
        self.generation_context = generation_context

    @property
    def properties_and_descriptors(self):
        return self.generation_context.properties_and_descriptors

    @property
    def style_properties(self):
        return self.generation_context.properties_and_descriptors.style_properties

    def generate(self):
        self.generate_style_builder_generated_cpp()

    # MARK: - Helper generator functions for StyleBuilderGenerated.cpp

    def _converted_value(self, property, additional_parameters=[]):
        parameters = ['builderState', 'value'] + additional_parameters
        return f"toStyleFromCSSValue<{property.codegen_properties.computed_style_type}>({', '.join(parameters)})"

    # Color property setters.

    def _generate_visited_link_color_supporting_property_initial_value_setter(self, to, property):
        initial_function = "Style::ComputedStyle::" + property.codegen_properties.computed_style_initial
        to.write(f"if (builderState.applyPropertyToRegularStyle())")
        to.write(f"    builderState.style().{property.codegen_properties.computed_style_setter}({initial_function}());")
        to.write(f"if (builderState.applyPropertyToVisitedLinkStyle())")
        to.write(f"    builderState.style().setVisitedLink{property.codegen_properties.computed_style_name_for_methods}({initial_function}());")

    def _generate_visited_link_color_supporting_property_inherit_value_setter(self, to, property):
        to.write(f"if (builderState.applyPropertyToRegularStyle())")
        to.write(f"    builderState.style().{property.codegen_properties.computed_style_setter}(forwardInheritedValue(builderState.parentStyle().{property.codegen_properties.computed_style_getter}()));")
        to.write(f"if (builderState.applyPropertyToVisitedLinkStyle())")
        to.write(f"    builderState.style().setVisitedLink{property.codegen_properties.computed_style_name_for_methods}(forwardInheritedValue(builderState.parentStyle().{property.codegen_properties.computed_style_getter}()));")

    def _generate_visited_link_color_supporting_property_value_setter(self, to, property):
        to.write(f"if (builderState.applyPropertyToRegularStyle())")
        to.write(f"    builderState.style().{property.codegen_properties.computed_style_setter}({self._converted_value(property, ['ForVisitedLink::No'])});")
        to.write(f"if (builderState.applyPropertyToVisitedLinkStyle())")
        to.write(f"    builderState.style().setVisitedLink{property.codegen_properties.computed_style_name_for_methods}({self._converted_value(property, ['ForVisitedLink::Yes'])});")

    # CoordinatedValueList property setters.

    def _generate_coordinated_value_list_property_initial_value_setter(self, to, property):
        to.write(f"applyInitialCoordinatedValueListProperty<{property.id_or_cascade_alias_id}, &ComputedStyle::{property.method_name_for_ensure_coordinated_value_list}, {property.type_name_for_coordinated_value_list}>(builderState);")

    def _generate_coordinated_value_list_property_inherit_value_setter(self, to, property):
        to.write(f"applyInheritCoordinatedValueListProperty<{property.id_or_cascade_alias_id}, &ComputedStyle::{property.method_name_for_ensure_coordinated_value_list}, &ComputedStyle::{property.method_name_for_get_coordinated_value_list}, {property.type_name_for_coordinated_value_list}>(builderState);")

    def _generate_coordinated_value_list_property_value_setter(self, to, property):
        to.write(f"applyValueCoordinatedValueListProperty<{property.id_or_cascade_alias_id}, &ComputedStyle::{property.method_name_for_ensure_coordinated_value_list}, WebCore::{property.codegen_properties.coordinated_value_list_property_item_type}, {property.type_name_for_coordinated_value_list}>(builderState, value);")

    # Font property setters.

    def _generate_font_property_initial_value_setter(self, to, property):
        to.write(f"builderState.{property.codegen_properties.font_description_setter.replace('set', 'setFontDescription', 1)}(Style::ComputedStyle::{property.codegen_properties.computed_style_initial}());")

    def _generate_font_property_inherit_value_setter(self, to, property):
        to.write(f"auto inheritedValue = builderState.parentStyle().{property.codegen_properties.computed_style_getter}();")
        to.write(f"builderState.{property.codegen_properties.font_description_setter.replace('set', 'setFontDescription', 1)}(WTF::move(inheritedValue));")

    def _generate_font_property_value_setter(self, to, property, value):
        to.write(f"builderState.{property.codegen_properties.font_description_setter.replace('set', 'setFontDescription', 1)}({value});")

    # All other property setters.

    def _generate_property_initial_value_setter(self, to, property):
        to.write(f"builderState.style().{property.codegen_properties.computed_style_setter}(Style::ComputedStyle::{property.codegen_properties.computed_style_initial}());")

    def _generate_property_inherit_value_setter(self, to, property):
        to.write(f"builderState.style().{property.codegen_properties.computed_style_setter}(forwardInheritedValue(builderState.parentStyle().{property.codegen_properties.computed_style_getter}()));")

    def _generate_property_value_setter(self, to, property, value):
        to.write(f"builderState.style().{property.codegen_properties.computed_style_setter}({value});")

    # Property setter dispatch.

    def _generate_style_builder_generated_cpp_initial_value_setter(self, to, property):
        to.write(f"static void applyInitial{property.id_without_prefix}(BuilderState& builderState)")
        to.write(f"{{")

        with to.indent():
            if property.codegen_properties.visited_link_color_support:
                self._generate_visited_link_color_supporting_property_initial_value_setter(to, property)
            elif property.codegen_properties.coordinated_value_list_property:
                self._generate_coordinated_value_list_property_initial_value_setter(to, property)
            elif property.codegen_properties.font_property:
                self._generate_font_property_initial_value_setter(to, property)
            else:
                self._generate_property_initial_value_setter(to, property)

            if property.codegen_properties.computed_style_has_explicitly_set_policy:
                if property.codegen_properties.computed_style_has_explicitly_set_policy == "all-author-origin":
                    to.write(f"builderState.style().setHasExplicitlySet{property.codegen_properties.computed_style_name_for_methods}(builderState.isAuthorOrigin());")
                elif property.codegen_properties.computed_style_has_explicitly_set_policy == "all-border-radius":
                    to.write(f"builderState.style().setHasExplicitlySet{property.codegen_properties.computed_style_name_for_methods}(false);")

            if property.codegen_properties.fast_path_inherited:
                to.write(f"builderState.style().setDisallowsFastPathInheritance();")

        to.write(f"}}")

    def _generate_style_builder_generated_cpp_inherit_value_setter(self, to, property):
        to.write(f"static void applyInherit{property.id_without_prefix}(BuilderState& builderState)")
        to.write(f"{{")

        with to.indent():
            if property.codegen_properties.visited_link_color_support:
                self._generate_visited_link_color_supporting_property_inherit_value_setter(to, property)
            elif property.codegen_properties.coordinated_value_list_property:
                self._generate_coordinated_value_list_property_inherit_value_setter(to, property)
            elif property.codegen_properties.font_property:
                self._generate_font_property_inherit_value_setter(to, property)
            else:
                self._generate_property_inherit_value_setter(to, property)

            if property.codegen_properties.computed_style_has_explicitly_set_policy:
                if property.codegen_properties.computed_style_has_explicitly_set_policy == "all-author-origin":
                    to.write(f"builderState.style().setHasExplicitlySet{property.codegen_properties.computed_style_name_for_methods}(builderState.isAuthorOrigin());")
                elif property.codegen_properties.computed_style_has_explicitly_set_policy == "all-border-radius":
                    to.write(f"builderState.style().setHasExplicitlySet{property.codegen_properties.computed_style_name_for_methods}(builderState.parentStyle().hasExplicitlySet{property.codegen_properties.computed_style_name_for_methods}());")

            if property.codegen_properties.fast_path_inherited:
                to.write(f"builderState.style().setDisallowsFastPathInheritance();")

        to.write(f"}}")

    def _generate_style_builder_generated_cpp_value_setter(self, to, property):
        to.write(f"static void applyValue{property.id_without_prefix}(BuilderState& builderState, CSSValue& value)")
        to.write(f"{{")

        with to.indent():
            if property in self.style_properties.all_by_name["font"].codegen_properties.longhands and "Initial" not in property.codegen_properties.style_builder_custom and property.codegen_properties.style_builder_requires_system_font_shorthand_check:
                to.write(f"if (CSSPropertyParserHelpers::isSystemFontShorthand(value.valueID())) {{")
                with to.indent():
                    to.write(f"applyInitial{property.id_without_prefix}(builderState);")
                    to.write(f"return;")
                to.write(f"}}")

            if property.codegen_properties.visited_link_color_support:
                self._generate_visited_link_color_supporting_property_value_setter(to, property)
            elif property.codegen_properties.coordinated_value_list_property:
                self._generate_coordinated_value_list_property_value_setter(to, property)
            elif property.codegen_properties.font_property:
                self._generate_font_property_value_setter(to, property, self._converted_value(property))
            else:
                self._generate_property_value_setter(to, property, self._converted_value(property))

            if property.codegen_properties.computed_style_has_explicitly_set_policy:
                if property.codegen_properties.computed_style_has_explicitly_set_policy == "all-author-origin":
                    to.write(f"builderState.style().setHasExplicitlySet{property.codegen_properties.computed_style_name_for_methods}(builderState.isAuthorOrigin());")
                elif property.codegen_properties.computed_style_has_explicitly_set_policy == "all-border-radius":
                    to.write(f"builderState.style().setHasExplicitlySet{property.codegen_properties.computed_style_name_for_methods}(true);")
                elif property.codegen_properties.computed_style_has_explicitly_set_policy == "value-only":
                    to.write(f"builderState.style().setHasExplicitlySet{property.codegen_properties.computed_style_name_for_methods}(true);")

            if property.codegen_properties.fast_path_inherited:
                to.write(f"builderState.style().setDisallowsFastPathInheritance();")

        to.write(f"}}")

    def _generate_style_builder_generated_cpp_builder_functions_class(self, *, to):
        to.write(f"class BuilderFunctions {{")
        to.write(f"public:")

        with to.indent():
            for property in self.style_properties.all:
                if property.codegen_properties.longhands:
                    continue
                if property.codegen_properties.skip_style_builder:
                    continue

                if property.codegen_properties.is_logical:
                    raise Exception(f"Property '{property.name}' is logical but doesn't have skip-style-builder.")

                if "Initial" not in property.codegen_properties.style_builder_custom:
                    self._generate_style_builder_generated_cpp_initial_value_setter(to, property)
                if "Inherit" not in property.codegen_properties.style_builder_custom:
                    self._generate_style_builder_generated_cpp_inherit_value_setter(to, property)
                if "Value" not in property.codegen_properties.style_builder_custom:
                    self._generate_style_builder_generated_cpp_value_setter(to, property)

        to.write(f"}};")

    def _generate_style_builder_generated_cpp_builder_generated_apply(self, *, to):
        to.write_block("""
            void BuilderGenerated::applyProperty(CSSPropertyID id, BuilderState& builderState, CSSValue& value, ApplyValueType valueType)
            {
                switch (id) {
                case CSSPropertyID::CSSPropertyInvalid:
                    break;
                case CSSPropertyID::CSSPropertyCustom:
                    ASSERT_NOT_REACHED();
                    break;""")

        with to.indent():
            def scope_for_function(property, function):
                if function in property.codegen_properties.style_builder_custom:
                    return "BuilderCustom"
                return "BuilderFunctions"

            for property in self.properties_and_descriptors.all_unique:
                if not isinstance(property, StyleProperty):
                    to.write(f"case {property.id}:")
                    with to.indent():
                        to.write(f"break;")
                    continue

                to.write(f"case {property.id}:")

                with to.indent():
                    if property.codegen_properties.longhands:
                        to.write(f"ASSERT(isShorthand(id));")
                        to.write(f"ASSERT_NOT_REACHED();")
                    elif not property.codegen_properties.skip_style_builder:
                        apply_initial_arguments = ["builderState"]
                        apply_inherit_arguments = ["builderState"]
                        apply_value_arguments = ["builderState", "value"]

                        to.write(f"switch (valueType) {{")
                        to.write(f"case ApplyValueType::Initial:")
                        with to.indent():
                            to.write(f"{scope_for_function(property, 'Initial')}::applyInitial{property.id_without_prefix}({', '.join(apply_initial_arguments)});")
                            to.write(f"break;")
                        to.write(f"case ApplyValueType::Inherit:")
                        with to.indent():
                            to.write(f"{scope_for_function(property, 'Inherit')}::applyInherit{property.id_without_prefix}({', '.join(apply_inherit_arguments)});")
                            to.write(f"break;")
                        to.write("case ApplyValueType::Value:")
                        with to.indent():
                            to.write(f"{scope_for_function(property, 'Value')}::applyValue{property.id_without_prefix}({', '.join(apply_value_arguments)});")
                            to.write(f"break;")
                        to.write(f"}}")

                    to.write(f"break;")

            to.write(f"}}")
        to.write(f"}}")
        to.newline()

    def generate_style_builder_generated_cpp(self):
        with open('StyleBuilderGenerated.cpp', 'w') as output_file:
            writer = Writer(output_file)

            self.generation_context.generate_heading(
                to=writer
            )

            self.generation_context.generate_cpp_required_includes(
                to=writer,
                header="StyleBuilderGenerated.h"
            )

            self.generation_context.generate_includes(
                to=writer,
                headers=[
                    "CSSPrimitiveValueMappings.h",
                    "CSSProperty.h",
                    "RenderStyle+GettersInlines.h",
                    "RenderStyle+SettersInlines.h",
                    "StyleBuilderCustom.h",
                    "StyleBuilderState.h",
                    "StyleComputedStyle+InitialInlines.h",
                    "StylePropertyShorthand.h",
                ]
            )

            with self.generation_context.namespaces(["WebCore", "Style"], to=writer):
                self._generate_style_builder_generated_cpp_builder_functions_class(
                    to=writer
                )

                self._generate_style_builder_generated_cpp_builder_generated_apply(
                    to=writer
                )


# Generates `StyleExtractorGenerated.cpp`.
class GenerateStyleExtractorGenerated:
    def __init__(self, generation_context):
        self.generation_context = generation_context

    def generate(self):
        self.generate_style_extractor_generated_cpp()

    @property
    def properties_and_descriptors(self):
        return self.generation_context.properties_and_descriptors

    @property
    def style_properties(self):
        return self.generation_context.properties_and_descriptors.style_properties

    # MARK: - Helper generator functions for StyleExtractorGenerated.cpp

    @staticmethod
    def wrap_in_converter(property, value):
        return f"createCSSValue(extractorState.pool, extractorState.style, {value})"

    @staticmethod
    def wrap_in_serializer(property, value):
        return f"serializationForCSS(builder, context, extractorState.style, {value})"

    # Color property getters.

    def _generate_visited_link_color_supporting_property_value_getter(self, to, property):
        to.write(f"if (extractorState.allowVisitedStyle) {{")
        with to.indent():
            to.write(f"return extractorState.pool.createColorValue(extractorState.style.visitedDependent{property.codegen_properties.computed_style_name_for_methods}());")
        to.write(f"}}")
        self._generate_property_value_getter(to, property)

    def _generate_visited_link_color_supporting_property_value_serialization_getter(self, to, property):
        to.write(f"if (extractorState.allowVisitedStyle) {{")
        with to.indent():
            to.write(f"builder.append(WebCore::serializationForCSS(extractorState.style.visitedDependent{property.codegen_properties.computed_style_name_for_methods}()));")
            to.write(f"return;")
        to.write(f"}}")
        self._generate_property_value_serialization_getter(to, property)

    # Animation property getters.

    def _generate_coordinated_value_list_property_value_getter(self, to, property):
        to.write(f"auto mapper = [](auto& extractorState, const auto& value, const std::optional<{property.type_name_for_coordinated_value_list}::value_type>&, const auto&) -> Ref<CSSValue> {{")
        with to.indent():
            to.write(f"return {GenerateStyleExtractorGenerated.wrap_in_converter(property, 'value')};")
        to.write(f"}};")
        to.write(f"return extractCoordinatedValueListValue<{property.id_or_cascade_alias_id}>(extractorState, extractorState.style.computedStyle().{property.method_name_for_get_coordinated_value_list}(), mapper);")

    def _generate_coordinated_value_list_property_value_serialization_getter(self, to, property):
        to.write(f"auto mapper = [](auto& extractorState, auto& builder, const auto& context, const auto& value, const std::optional<{property.type_name_for_coordinated_value_list}::value_type>&, const auto&) {{")
        with to.indent():
            to.write(f"{GenerateStyleExtractorGenerated.wrap_in_serializer(property, 'value')};")
        to.write(f"}};")
        to.write(f"extractCoordinatedValueListSerialization<{property.id_or_cascade_alias_id}>(extractorState, builder, context, extractorState.style.computedStyle().{property.method_name_for_get_coordinated_value_list}(), mapper);")

    # All other property value getters.

    def _generate_property_value_getter(self, to, property):
        to.write(f"return {GenerateStyleExtractorGenerated.wrap_in_converter(property, f'extractorState.style.computedStyle().{property.codegen_properties.computed_style_getter}()')};")

    def _generate_property_value_serialization_getter(self, to, property):
        to.write(f"{GenerateStyleExtractorGenerated.wrap_in_serializer(property, f'extractorState.style.computedStyle().{property.codegen_properties.computed_style_getter}()')};")

    # Shorthand property value getter.

    def _generate_style_extractor_generated_cpp_shorthand_value_extractor(self, to, property):
        to.write(f"static RefPtr<CSSValue> extract{property.id_without_prefix}Shorthand(ExtractorState& extractorState)")
        to.write(f"{{")
        with to.indent():
            to.write(f"return extract{property.codegen_properties.shorthand_style_extractor_pattern}Shorthand(extractorState, {property.id_without_prefix_with_lowercase_first_letter}Shorthand());")
        to.write(f"}}")

    def _generate_style_extractor_generated_cpp_shorthand_value_serialization_extractor(self, to, property):
        to.write(f"static void extract{property.id_without_prefix}ShorthandSerialization(ExtractorState& extractorState, StringBuilder& builder, const CSS::SerializationContext& context)")
        to.write(f"{{")
        with to.indent():
            to.write(f"extract{property.codegen_properties.shorthand_style_extractor_pattern}ShorthandSerialization(extractorState, builder, context, {property.id_without_prefix_with_lowercase_first_letter}Shorthand());")
        to.write(f"}}")

    # Longhand property value getter.

    def _generate_style_extractor_generated_cpp_value_extractor(self, to, property):
        to.write(f"static RefPtr<CSSValue> extract{property.id_without_prefix}(ExtractorState& extractorState)")
        to.write(f"{{")

        with to.indent():
            if property.codegen_properties.visited_link_color_support:
                self._generate_visited_link_color_supporting_property_value_getter(to, property)
            elif property.codegen_properties.coordinated_value_list_property:
                self._generate_coordinated_value_list_property_value_getter(to, property)
            else:
                self._generate_property_value_getter(to, property)

        to.write(f"}}")

    def _generate_style_extractor_generated_cpp_value_serialization_extractor(self, to, property):
        to.write(f"static void extract{property.id_without_prefix}Serialization(ExtractorState& extractorState, StringBuilder& builder, const CSS::SerializationContext& context)")
        to.write(f"{{")

        with to.indent():
            if property.codegen_properties.visited_link_color_support:
                self._generate_visited_link_color_supporting_property_value_serialization_getter(to, property)
            elif property.codegen_properties.coordinated_value_list_property:
                self._generate_coordinated_value_list_property_value_serialization_getter(to, property)
            else:
                self._generate_property_value_serialization_getter(to, property)

        to.write(f"}}")

    def _generate_style_extractor_generated_cpp_extractor_functions_class(self, *, to):
        to.write(f"class ExtractorFunctions {{")
        to.write(f"public:")

        with to.indent():
            for property in self.properties_and_descriptors.all_unique:
                if not isinstance(property, StyleProperty):
                    continue
                if property.codegen_properties.internal_only:
                    continue
                if property.codegen_properties.skip_style_extractor:
                    continue
                if property.codegen_properties.style_extractor_custom:
                    continue
                if property.codegen_properties.is_logical:
                    continue
                if property.codegen_properties.longhands:
                    if not property.codegen_properties.shorthand_style_extractor_pattern:
                        continue
                    self._generate_style_extractor_generated_cpp_shorthand_value_extractor(to, property)
                    self._generate_style_extractor_generated_cpp_shorthand_value_serialization_extractor(to, property)
                else:
                    self._generate_style_extractor_generated_cpp_value_extractor(to, property)
                    self._generate_style_extractor_generated_cpp_value_serialization_extractor(to, property)

        to.write(f"}};")

    # Property getter dispatch.

    def _generate_style_extractor_generated_cpp_extractor_generated_extract_value(self, *, to):
        to.write_block("""
            RefPtr<CSSValue> ExtractorGenerated::extractValue(ExtractorState& extractorState, CSSPropertyID id)
            {
                switch (id) {
                case CSSPropertyID::CSSPropertyInvalid:
                    break;
                case CSSPropertyID::CSSPropertyCustom:
                    ASSERT_NOT_REACHED();
                    break;""")

        with to.indent():
            def scope_for_function(property):
                if property.codegen_properties.style_extractor_custom:
                    return "ExtractorCustom"
                if property.codegen_properties.longhands and not property.codegen_properties.shorthand_style_extractor_pattern:
                    return "ExtractorCustom"
                return "ExtractorFunctions"

            for property in self.properties_and_descriptors.all_unique:
                to.write(f"case {property.id}:")
                with to.indent():
                    if not isinstance(property, StyleProperty):
                        to.write(f"// Skipped - Descriptor-only property")
                        to.write(f"return nullptr;")
                    elif property.codegen_properties.internal_only:
                        to.write(f"// Skipped - Internal only")
                        to.write(f"return nullptr;")
                    elif property.codegen_properties.skip_style_extractor:
                        to.write(f"// Skipped - Not computable")
                        to.write(f"return nullptr;")
                    elif property.codegen_properties.is_logical and not property.codegen_properties.style_extractor_custom:
                        to.write(f"// Logical properties are handled by recursing using the direction resolved property.")
                        to.write(f"return extractValue(extractorState, CSSProperty::resolveDirectionAwareProperty(id, extractorState.style.writingMode()));")
                    elif property.codegen_properties.longhands:
                        to.write(f"ASSERT(isShorthand(id));")
                        to.write(f"return {scope_for_function(property)}::extract{property.id_without_prefix}Shorthand(extractorState);")
                    else:
                        to.write(f"return {scope_for_function(property)}::extract{property.id_without_prefix}(extractorState);")

            to.write(f"}}")
            to.write(f"ASSERT_NOT_REACHED();")
            to.write(f"return nullptr;")
        to.write(f"}}")
        to.newline()

    # Property serialization dispatch.

    def _generate_style_extractor_generated_cpp_extractor_generated_extract_serialization(self, *, to):
        to.write_block("""
            void ExtractorGenerated::extractValueSerialization(ExtractorState& extractorState, StringBuilder& builder, const CSS::SerializationContext& context, CSSPropertyID id)
            {
                switch (id) {
                case CSSPropertyID::CSSPropertyInvalid:
                    break;
                case CSSPropertyID::CSSPropertyCustom:
                    ASSERT_NOT_REACHED();
                    break;""")

        with to.indent():
            def scope_for_function(property):
                if property.codegen_properties.style_extractor_custom:
                    return "ExtractorCustom"
                if property.codegen_properties.longhands and not property.codegen_properties.shorthand_style_extractor_pattern:
                    return "ExtractorCustom"
                return "ExtractorFunctions"

            for property in self.properties_and_descriptors.all_unique:
                to.write(f"case {property.id}:")
                with to.indent():
                    if not isinstance(property, StyleProperty):
                        to.write(f"// Skipped - Descriptor-only property")
                        to.write(f"return;")
                    elif property.codegen_properties.internal_only:
                        to.write(f"// Skipped - Internal only")
                        to.write(f"return;")
                    elif property.codegen_properties.skip_style_extractor:
                        to.write(f"// Skipped - Not computable")
                        to.write(f"return;")
                    elif property.codegen_properties.is_logical and not property.codegen_properties.style_extractor_custom:
                        to.write(f"// Logical properties are handled by recursing using the direction resolved property.")
                        to.write(f"extractValueSerialization(extractorState, builder, context, CSSProperty::resolveDirectionAwareProperty(id, extractorState.style.writingMode()));")
                        to.write(f"return;")
                    elif property.codegen_properties.longhands:
                        to.write(f"ASSERT(isShorthand(id));")
                        to.write(f"{scope_for_function(property)}::extract{property.id_without_prefix}ShorthandSerialization(extractorState, builder, context);")
                        to.write(f"return;")
                    else:
                        to.write(f"{scope_for_function(property)}::extract{property.id_without_prefix}Serialization(extractorState, builder, context);")
                        to.write(f"return;")

            to.write(f"}}")
            to.write(f"ASSERT_NOT_REACHED();")
        to.write(f"}}")
        to.newline()

    def generate_style_extractor_generated_cpp(self):
        with open('StyleExtractorGenerated.cpp', 'w') as output_file:
            writer = Writer(output_file)

            self.generation_context.generate_heading(
                to=writer
            )

            self.generation_context.generate_cpp_required_includes(
                to=writer,
                header="StyleExtractorGenerated.h"
            )

            self.generation_context.generate_includes(
                to=writer,
                headers=[
                    "CSSPrimitiveValueMappings.h",
                    "CSSProperty.h",
                    "ColorSerialization.h",
                    "RenderStyle.h",
                    "StyleExtractorCustom.h",
                    "StyleExtractorState.h",
                    "StylePropertyShorthand.h",
                ]
            )

            with self.generation_context.namespaces(["WebCore", "Style"], to=writer):
                self._generate_style_extractor_generated_cpp_extractor_functions_class(
                    to=writer
                )

                self._generate_style_extractor_generated_cpp_extractor_generated_extract_value(
                    to=writer
                )

                self._generate_style_extractor_generated_cpp_extractor_generated_extract_serialization(
                    to=writer
                )


# Generates `StylePropertyShorthandFunctions.h` and `StylePropertyShorthandFunctions.cpp`.
class GenerateStylePropertyShorthandFunctions:
    def __init__(self, generation_context):
        self.generation_context = generation_context

    @property
    def style_properties(self):
        return self.generation_context.properties_and_descriptors.style_properties

    def generate(self):
        self.generate_style_property_shorthand_functions_h()
        self.generate_style_property_shorthand_functions_cpp()

    # MARK: - Helper generator functions for StylePropertyShorthandFunctions.h

    def _generate_style_property_shorthand_functions_declarations(self, *, to):
        # Skip non-shorthand properties (aka properties WITH longhands).
        for property in self.style_properties.all_shorthands:
            to.write(f"StylePropertyShorthand {property.id_without_prefix_with_lowercase_first_letter}Shorthand();")
        to.newline()

    def generate_style_property_shorthand_functions_h(self):
        with open('StylePropertyShorthandFunctions.h', 'w') as output_file:
            writer = Writer(output_file)

            self.generation_context.generate_heading(
                to=writer
            )

            self.generation_context.generate_required_header_pragma(
                to=writer
            )

            with self.generation_context.namespace("WebCore", to=writer):
                self.generation_context.generate_forward_declarations(
                    to=writer,
                    classes=["StylePropertyShorthand"]
                )

                self._generate_style_property_shorthand_functions_declarations(
                    to=writer
                )

    # MARK: - Helper generator functions for StylePropertyShorthandFunctions.cpp

    def _generate_style_property_shorthand_functions_accessors(self, *, to, longhand_to_shorthands, shorthand_to_longhand_count):
        for property in self.style_properties.all_shorthands:
            to.write(f"StylePropertyShorthand {property.id_without_prefix_with_lowercase_first_letter}Shorthand()")
            to.write(f"{{")
            with to.indent():
                to.write(f"static const CSSPropertyID {property.id_without_prefix_with_lowercase_first_letter}Properties[] = {{")

                with to.indent():
                    shorthand_to_longhand_count[property] = 0
                    for longhand in property.codegen_properties.longhands:
                        if longhand.name == "all":
                            for inner_property in self.style_properties.all_non_shorthands:
                                if inner_property.name == "direction" or inner_property.name == "unicode-bidi":
                                    continue
                                longhand_to_shorthands.setdefault(inner_property, [])
                                longhand_to_shorthands[inner_property].append(property)
                                shorthand_to_longhand_count[property] += 1
                                to.write(f"{inner_property.id},")
                        else:
                            longhand_to_shorthands.setdefault(longhand, [])
                            longhand_to_shorthands[longhand].append(property)
                            shorthand_to_longhand_count[property] += 1
                            to.write(f"{longhand.id},")

                to.write(f"}};")
                to.write(f"return StylePropertyShorthand({property.id}, std::span {{ {property.id_without_prefix_with_lowercase_first_letter}Properties }});")
            to.write(f"}}")
            to.newline()

    def _generate_style_property_shorthand_functions_matching_shorthands_for_longhand(self, *, to, longhand_to_shorthands, shorthand_to_longhand_count):
        to.write(f"StylePropertyShorthandVector matchingShorthandsForLonghand(CSSPropertyID id)")
        to.write(f"{{")
        with to.indent():
            to.write(f"switch (id) {{")

            vector_to_longhands = {}

            # https://drafts.csswg.org/cssom/#concept-shorthands-preferred-order
            def preferred_order_for_shorthands(x):
                return (-shorthand_to_longhand_count[x], x.name.startswith("-"), not x.name.startswith("-webkit-"), x.name)

            for longhand, shorthands in sorted(list(longhand_to_shorthands.items()), key=lambda item: item[0].name):
                shorthand_calls = [f"{p.id_without_prefix_with_lowercase_first_letter}Shorthand()" for p in sorted(shorthands, key=preferred_order_for_shorthands)]
                vector = f"StylePropertyShorthandVector{{{ ', '.join(shorthand_calls) }}}"
                vector_to_longhands.setdefault(vector, [])
                vector_to_longhands[vector].append(longhand)

            for vector, longhands in sorted(list(vector_to_longhands.items()), key=lambda item: item[0]):
                for longhand in longhands:
                    to.write(f"case {longhand.id}:")
                with to.indent():
                    to.write(f"return {vector};")

            to.write(f"default:")
            with to.indent():
                to.write(f"return {{ }};")
            to.write(f"}}")
        to.write(f"}}")
        to.newline()

    def generate_style_property_shorthand_functions_cpp(self):
        with open('StylePropertyShorthandFunctions.cpp', 'w') as output_file:
            writer = Writer(output_file)

            self.generation_context.generate_heading(
                to=writer
            )

            self.generation_context.generate_cpp_required_includes(
                to=writer,
                header="StylePropertyShorthandFunctions.h"
            )

            self.generation_context.generate_includes(
                to=writer,
                headers=[
                    "StylePropertyShorthand.h",
                ]
            )

            with self.generation_context.namespace("WebCore", to=writer):
                longhand_to_shorthands = {}
                shorthand_to_longhand_count = {}

                self._generate_style_property_shorthand_functions_accessors(
                    to=writer,
                    longhand_to_shorthands=longhand_to_shorthands,
                    shorthand_to_longhand_count=shorthand_to_longhand_count
                )

                self.generation_context.generate_property_id_switch_function(
                    to=writer,
                    signature="StylePropertyShorthand shorthandForProperty(CSSPropertyID id)",
                    iterable=self.style_properties.all_shorthands,
                    mapping=lambda p: f"return {p.id_without_prefix_with_lowercase_first_letter}Shorthand();",
                    default="return { };"
                )

                self._generate_style_property_shorthand_functions_matching_shorthands_for_longhand(
                    to=writer,
                    longhand_to_shorthands=longhand_to_shorthands,
                    shorthand_to_longhand_count=shorthand_to_longhand_count
                )


# Generates `CSSPropertyParsing.h` and `CSSPropertyParsing.cpp`.
class GenerateCSSPropertyParsing:
    def __init__(self, generation_context):
        self.generation_context = generation_context

        # Create a handler for each property and add it to the `property_consumers` map.
        self.property_consumers = {property: PropertyConsumer.make(property) for property in generation_context.properties_and_descriptors.all_properties_and_descriptors}
        self.shared_grammar_rule_consumers = {shared_grammar_rule: SharedGrammarRuleConsumer.make(shared_grammar_rule) for shared_grammar_rule in generation_context.shared_grammar_rules.all}

    def generate(self):
        self.generate_css_property_parsing_h()
        self.generate_css_property_parsing_cpp()

    @property
    def properties_and_descriptors(self):
        return self.generation_context.properties_and_descriptors

    @property
    def shared_grammar_rules(self):
        return self.generation_context.shared_grammar_rules

    @property
    def all_property_consumers(self):
        return (self.property_consumers[property] for property in self.properties_and_descriptors.all_properties_and_descriptors)

    @property
    def all_shared_grammar_rule_consumers(self):
        return (self.shared_grammar_rule_consumers[shared_grammar_rule] for shared_grammar_rule in self.shared_grammar_rules.all)

    @property
    def all_property_parsing_collections(self):
        ParsingCollection = collections.namedtuple('ParsingCollection', ['id', 'name', 'noun', 'supports_shorthands', 'consumers'])

        result = []
        for set in self.properties_and_descriptors.all_sets:
            result += [ParsingCollection(set.id, set.name, set.noun, set.supports_shorthands, list(self.property_consumers[property] for property in set.all))]
        return result

    @property
    def all_consumers_grouped_by_kind(self):
        ConsumerCollection = collections.namedtuple('ConsumerCollection', ['description', 'consumers'])

        return [ConsumerCollection(f'{parsing_collection.name} {parsing_collection.noun}', parsing_collection.consumers) for parsing_collection in self.all_property_parsing_collections] + [ConsumerCollection(f'shared', list(self.all_shared_grammar_rule_consumers))]

    def generate_css_property_parsing_h(self):
        with open('CSSPropertyParsing.h', 'w') as output_file:
            writer = Writer(output_file)

            self.generation_context.generate_heading(
                to=writer
            )

            self.generation_context.generate_required_header_pragma(
                to=writer
            )

            self.generation_context.generate_includes(
                to=writer,
                headers=[
                    "CSSPropertyNames.h",
                    "CSSValueKeywords.h",
                ]
            )

            with self.generation_context.namespace("WebCore", to=writer):
                self.generation_context.generate_forward_declarations(
                    to=writer,
                    classes=[
                        "CSSParserTokenRange",
                        "CSSValue",
                    ]
                )

                with self.generation_context.namespace("CSS", to=writer):
                    self.generation_context.generate_forward_declarations(
                        to=writer,
                        structs=[
                            "PropertyParserResult",
                            "PropertyParserState",
                        ]
                    )

                self._generate_css_property_parsing_h_property_parsing_declaration(
                    to=writer
                )

    def generate_css_property_parsing_cpp(self):
        with open('CSSPropertyParsing.cpp', 'w') as output_file:
            writer = Writer(output_file)

            self.generation_context.generate_heading(
                to=writer
            )

            self.generation_context.generate_cpp_required_includes(
                to=writer,
                header="CSSPropertyParsing.h"
            )

            self.generation_context.generate_includes(
                to=writer,
                headers=[
                    "CSSParserContext.h",
                    "CSSParserIdioms.h",
                    "CSSPropertyParser.h",
                    "CSSPropertyParserCustom.h",
                    "CSSPropertyParserState.h",
                    "DeprecatedGlobalSettings.h",
                ]
            )

            with self.generation_context.namespace("WebCore", to=writer):
                self.generation_context.generate_using_namespace_declarations(
                    to=writer,
                    namespaces=["CSSPropertyParserHelpers"]
                )

                self._generate_css_property_parsing_cpp_property_parsing_functions(
                    to=writer
                )

                for parsing_collection in self.all_property_parsing_collections:
                    self._generate_css_property_parsing_cpp_parse_longhand_property(
                        to=writer,
                        parsing_collection=parsing_collection
                    )
                    self._generate_css_property_parsing_cpp_parse_shorthand_property(
                        to=writer,
                        parsing_collection=parsing_collection
                    )

                    keyword_fast_path_eligible_property_consumers = [consumer for consumer in parsing_collection.consumers if consumer.keyword_fast_path_generator]

                    self._generate_css_property_parsing_cpp_is_keyword_valid_for_property(
                        to=writer,
                        parsing_collection=parsing_collection,
                        keyword_fast_path_eligible_property_consumers=keyword_fast_path_eligible_property_consumers
                    )

                    self._generate_css_property_parsing_cpp_is_keyword_fast_path_eligible_for_property(
                        to=writer,
                        parsing_collection=parsing_collection,
                        keyword_fast_path_eligible_property_consumers=keyword_fast_path_eligible_property_consumers
                    )

    # MARK: - Helper generator functions for CSSPropertyParsing.h

    def _generate_css_property_parsing_h_property_parsing_declaration(self, *, to):
        to.write(f"struct CSSPropertyParsing {{")

        with to.indent():
            for parsing_collection in self.all_property_parsing_collections:
                to.write(f"// Parse and return a single {'longhand ' if parsing_collection.supports_shorthands else ''}{parsing_collection.name} {parsing_collection.noun}.")
                to.write(f"static RefPtr<CSSValue> parse{parsing_collection.id}{'Longhand' if parsing_collection.supports_shorthands else ''}(CSSParserTokenRange&, CSSPropertyID, CSS::PropertyParserState&);")
                if parsing_collection.supports_shorthands:
                    to.write(f"// Parse a shorthand {parsing_collection.name} {parsing_collection.noun}, adding longhands to the provided result collection. Returns true on success, false on failure.")
                    to.write(f"static bool parse{parsing_collection.id}Shorthand(CSSParserTokenRange&, CSSPropertyID, CSS::PropertyParserState&, CSS::PropertyParserResult&);")
                to.write(f"// Fast path bare-keyword support.")
                to.write(f"static bool isKeywordValidFor{parsing_collection.id}(CSSPropertyID, CSSValueID, CSS::PropertyParserState&);")
                to.write(f"static bool isKeywordFastPathEligible{parsing_collection.id}(CSSPropertyID);")
                to.newline()

            to.write(f"// Direct consumers.")

            for description, consumers in self.all_consumers_grouped_by_kind:
                if any(consumer.is_exported for consumer in consumers):
                    to.newline()
                    to.write(f"// Exported {description} consumers.")
                    for consumer in (consumer for consumer in consumers if consumer.is_exported):
                        consumer.generate_export_declaration(to=to)

        to.write(f"}};")
        to.newline()

    # MARK: - Helper generator functions for CSSPropertyParsing.cpp

    def _generate_css_property_parsing_cpp_is_keyword_valid_for_property(self, *, to, parsing_collection, keyword_fast_path_eligible_property_consumers):
        if not keyword_fast_path_eligible_property_consumers:
            to.write(f"bool CSSPropertyParsing::isKeywordValidFor{parsing_collection.id}(CSSPropertyID, CSSValueID, CSS::PropertyParserState&)")
            to.write(f"{{")
            with to.indent():
                to.write(f"return false;")
            to.write(f"}}")
            to.newline()
            return

        requires_state = any(property_consumer.keyword_fast_path_generator.requires_state for property_consumer in keyword_fast_path_eligible_property_consumers)

        self.generation_context.generate_property_id_switch_function(
            to=to,
            signature=f"bool CSSPropertyParsing::isKeywordValidFor{parsing_collection.id}(CSSPropertyID id, CSSValueID keyword, CSS::PropertyParserState&{' state' if requires_state else ''})",
            iterable=keyword_fast_path_eligible_property_consumers,
            mapping=lambda property_consumer: f"return {property_consumer.keyword_fast_path_generator.generate_call_string(keyword_string='keyword', state_string='state')};",
            default="return false;",
            mapping_to_property=lambda property_consumer: property_consumer.property
        )

    def _generate_css_property_parsing_cpp_is_keyword_fast_path_eligible_for_property(self, *, to, parsing_collection, keyword_fast_path_eligible_property_consumers):
        if not keyword_fast_path_eligible_property_consumers:
            to.write(f"bool CSSPropertyParsing::isKeywordFastPathEligible{parsing_collection.id}(CSSPropertyID)")
            to.write(f"{{")
            with to.indent():
                to.write(f"return false;")
            to.write(f"}}")
            to.newline()
            return

        self.generation_context.generate_property_id_switch_function_bool(
            to=to,
            signature=f"bool CSSPropertyParsing::isKeywordFastPathEligible{parsing_collection.id}(CSSPropertyID id)",
            iterable=keyword_fast_path_eligible_property_consumers,
            mapping_to_property=lambda property_consumer: property_consumer.property
        )

    def _generate_css_property_parsing_cpp_property_parsing_functions(self, *, to):
        # First generate definitions for all the keyword-only fast path predicate functions.
        for property_consumer in self.all_property_consumers:
            keyword_fast_path_generator = property_consumer.keyword_fast_path_generator
            if not keyword_fast_path_generator:
                continue
            keyword_fast_path_generator.generate_definition(to=to)

        # Then all the non-exported consume functions (these will be static functions).
        for property_consumer in self.all_property_consumers:
            if not property_consumer.property.codegen_properties.parser_exported:
                property_consumer.generate_definition(to=to)

        # Then all the exported consume functions (these will be static members of the CSSPropertyParsing struct).
        for property_consumer in self.all_property_consumers:
            if property_consumer.property.codegen_properties.parser_exported:
                property_consumer.generate_definition(to=to)

        # And finally all the exported shared grammar rule consumers (these will be static members of the CSSPropertyParsing struct).
        for shared_grammar_rule_consumer in self.all_shared_grammar_rule_consumers:
            shared_grammar_rule_consumer.generate_definition(to=to)

    def _generate_css_property_parsing_cpp_parse_longhand_property(self, *, to, parsing_collection):
        to.write(f"RefPtr<CSSValue> CSSPropertyParsing::parse{parsing_collection.id}{'Longhand' if parsing_collection.supports_shorthands else ''}(CSSParserTokenRange& range, CSSPropertyID id, CSS::PropertyParserState& state)")

        to.write(f"{{")
        with to.indent():
            to.write(f"if (!isExposed(id, state.context.propertySettings) && !isInternal(id)) {{")
            with to.indent():
                to.write(f"// Allow internal properties as we use them to parse several internal-only-shorthands (e.g. background-repeat),")
                to.write(f"// and to handle certain DOM-exposed values (e.g. -webkit-font-size-delta from execCommand('FontSizeDelta')).")
                to.write(f"ASSERT_NOT_REACHED();")
                to.write(f"return {{ }};")
            to.write(f"}}")

            # Build up a list of pairs of (property, return-expression-to-use-for-property).

            PropertyReturnExpression = collections.namedtuple('PropertyReturnExpression', ['property', 'return_expression'])
            property_and_return_expressions = []

            for consumer in parsing_collection.consumers:
                return_expression = consumer.generate_call_string(
                    range_string="range",
                    state_string="state")

                if return_expression is None:
                    continue

                property_and_return_expressions.append(
                    PropertyReturnExpression(consumer.property, return_expression))

            # Take the list of pairs of (value, return-expression-to-use-for-value), and
            # group them by their 'return-expression' to avoid unnecessary duplication of
            # return statements.

            PropertiesReturnExpression = collections.namedtuple('PropertiesReturnExpression', ['properties', 'return_expression'])

            property_and_return_expressions_sorted_by_expression = sorted(property_and_return_expressions, key=lambda x: x.return_expression)
            property_and_return_expressions_grouped_by_expression = []
            for return_expression, group in itertools.groupby(property_and_return_expressions_sorted_by_expression, lambda x: x.return_expression):
                properties = [property_and_return_expression.property for property_and_return_expression in group]
                property_and_return_expressions_grouped_by_expression.append(PropertiesReturnExpression(properties, return_expression))

            def _sort_by_first_property(a, b):
                return StyleProperties._sort_by_descending_priority_and_name(a.properties[0], b.properties[0])

            to.write(f"switch (id) {{")
            for properties, return_expression in sorted(property_and_return_expressions_grouped_by_expression, key=functools.cmp_to_key(_sort_by_first_property)):
                for property in properties:
                    to.write(f"case {property.id}:")

                with to.indent():
                    to.write(f"return {return_expression};")

            to.write(f"default:")
            with to.indent():
                to.write(f"return {{ }};")
            to.write(f"}}")
        to.write(f"}}")
        to.newline()

    def _generate_css_property_parsing_cpp_parse_shorthand_property(self, *, to, parsing_collection):
        if not parsing_collection.supports_shorthands:
            return
        to.write(f"bool CSSPropertyParsing::parse{parsing_collection.id}Shorthand(CSSParserTokenRange& range, CSSPropertyID id, CSS::PropertyParserState& state, CSS::PropertyParserResult& result)")

        to.write(f"{{")
        with to.indent():
            to.write(f"ASSERT(isShorthand(id));")
            to.newline()

            to.write(f"switch (id) {{")

            for consumer in parsing_collection.consumers:
                if not consumer.property.codegen_properties.longhands:
                    continue
                if consumer.property.codegen_properties.skip_parser:
                    continue

                to.write(f"case {consumer.property.id}:")
                with to.indent():
                    if consumer.property.codegen_properties.settings_flag and not consumer.property.codegen_properties.internal_only:
                        to.write(f"if (!state.context.propertySettings.{consumer.property.codegen_properties.settings_flag}) {{")
                        with to.indent():
                            to.write(f"ASSERT_NOT_REACHED();")
                            to.write(f"return false;")
                        to.write(f"}}")

                    if consumer.property.codegen_properties.parser_function:
                        to.write(f"return CSS::PropertyParserCustom::{consumer.property.codegen_properties.parser_function}(range, state, {consumer.property.codegen_properties.parser_shorthand}(), result);")
                    elif consumer.property.codegen_properties.shorthand_parser_pattern:
                        to.write(f"return CSS::PropertyParserCustom::consume{consumer.property.codegen_properties.shorthand_parser_pattern}Shorthand(range, state, {consumer.property.codegen_properties.parser_shorthand}(), result);")
                    else:
                        raise Exception(f"Shorthand property '{consumer.property}' has unknown parsing method.")

            to.write(f"default:")
            with to.indent():
                to.write(f"return false;")
            to.write(f"}}")
        to.write(f"}}")
        to.newline()


# Generates `StyleInterpolationWrapperMap.h` and `StyleInterpolationWrapperMap.cpp`.
class GenerateStyleInterpolationWrapperMap:
    def __init__(self, generation_context):
        self.generation_context = generation_context

    def generate(self):
        self.generate_css_property_animation_wrapper_map_h()
        self.generate_css_property_animation_wrapper_map_cpp()

    @property
    def properties_and_descriptors(self):
        return self.generation_context.properties_and_descriptors

    @property
    def properties(self):
        return self.generation_context.properties_and_descriptors.style_properties

    def generate_css_property_animation_wrapper_map_h(self):
        with open('StyleInterpolationWrapperMap.h', 'w') as output_file:
            writer = Writer(output_file)

            self.generation_context.generate_heading(
                to=writer
            )

            self.generation_context.generate_required_header_pragma(
                to=writer
            )

            self.generation_context.generate_includes(
                to=writer,
                headers=[
                    "CSSPropertyNames.h",
                ],
                system_headers=[
                    "<array>",
                    "<wtf/NeverDestroyed.h>",
                ]
            )

            with self.generation_context.namespaces(["WebCore", "Style", "Interpolation"], to=writer):
                self.generation_context.generate_forward_declarations(
                    to=writer,
                    classes=[
                        "WrapperBase",
                    ]
                )

                self._generate_css_property_animation_wrapper_map_h_wrapper_map_declaration(
                    to=writer
                )

    def generate_css_property_animation_wrapper_map_cpp(self):
        with open('StyleInterpolationWrapperMap.cpp', 'w') as output_file:
            writer = Writer(output_file)

            self.generation_context.generate_heading(
                to=writer
            )

            self.generation_context.generate_cpp_required_includes(
                to=writer,
                header="StyleInterpolationWrapperMap.h"
            )

            self.generation_context.generate_includes(
                to=writer,
                headers=[
                    "StylePropertyShorthand.h",
                ]
            )

            writer.write("#define STYLE_INTERPOLATION_GENERATED_INCLUDE_TRAP 1")
            writer.write("#include \"StyleInterpolationWrappers.h\"")
            writer.write("#undef STYLE_INTERPOLATION_GENERATED_INCLUDE_TRAP")

            with self.generation_context.namespaces(["WebCore", "Style", "Interpolation"], to=writer):
                self._generate_css_property_animation_wrapper_map_cpp_constructor(
                    to=writer
                )

    # MARK: - Helper generator functions for StyleInterpolationWrapperMap.h

    def _generate_css_property_animation_wrapper_map_h_wrapper_map_declaration(self, *, to):
        to.write_block("""\
            class WrapperMap final {
            public:
                static WrapperMap& singleton()
                {
                    static NeverDestroyed<WrapperMap> map;
                    return map;
                }

                WrapperBase* wrapper(CSSPropertyID id)
                {
                    if (id >= cssPropertyIDEnumValueCount)
                        return nullptr;
                    return m_wrappers[id];
                }

            private:
                friend class WTF::NeverDestroyed<WrapperMap>;

                WrapperMap();
                ~WrapperMap() = delete;

                std::array<WrapperBase*, cssPropertyIDEnumValueCount> m_wrappers;
            };""")

        to.newline()

    # MARK: - Helper generator functions for StyleInterpolationWrapperMap.cpp

    def _generate_css_property_animation_wrapper_map_cpp_longhand_wrapper_construction(self, property):
        # Compute animation wrapper type.
        if property.codegen_properties.animation_wrapper is not None:
            property_wrapper_type = property.codegen_properties.animation_wrapper
        elif property.animation_type == 'by computed value' or property.animation_type == 'repeatable list' or property.animation_type == 'see prose':
            if property.codegen_properties.coordinated_value_list_property:
                property_wrapper_type = 'CoordinatedValueListPropertyStyleTypeWrapper'
            elif property.codegen_properties.visited_link_color_support:
                property_wrapper_type = 'VisitedAffectedStyleTypeWrapper'
            else:
                property_wrapper_type = 'StyleTypeWrapper'
        elif property.animation_type == 'discrete':
            if property.codegen_properties.coordinated_value_list_property:
                property_wrapper_type = 'DiscreteCoordinatedValueListPropertyWrapper'
            else:
                property_wrapper_type = 'DiscreteWrapper'
        else:
            raise Exception(f"'{property.name}' animation wrapper type is not defined")

        # Compute animation wrapper constructor parameters.
        if property.codegen_properties.animation_wrapper_requires_override_parameters is not None:
            property_wrapper_parameters = property.codegen_properties.animation_wrapper_requires_override_parameters
        else:
            # Add CSSPropertyID
            property_wrapper_parameters = [property.id]

            # Compute style class.
            style_type = "ComputedStyle"
            name_for_methods = property.codegen_properties.computed_style_name_for_methods
            getter = property.codegen_properties.computed_style_getter
            setter = property.codegen_properties.computed_style_setter

            if property.codegen_properties.coordinated_value_list_property and property_wrapper_type is not None:
                property_wrapper_parameters += [f"&{style_type}::{property.method_name_for_get_coordinated_value_list}", f"&{style_type}::{property.method_name_for_ensure_coordinated_value_list}", f"&{style_type}::{property.method_name_for_set_coordinated_value_list}", f"{property_wrapper_type}({property.id}, &{property.type_name_for_coordinated_value_list}::value_type::{property.codegen_properties.coordinated_value_list_property_getter}, &{property.type_name_for_coordinated_value_list}::value_type::{property.codegen_properties.coordinated_value_list_property_setter})"]
                property_wrapper_type = "CoordinatedValueListPropertyWrapper"
            else:
                # Add getter
                if property.codegen_properties.animation_wrapper_requires_getter is not None:
                    property_wrapper_parameters += [f"&{style_type}::{property.codegen_properties.animation_wrapper_requires_getter}"]
                else:
                    property_wrapper_parameters += [f"&{style_type}::{getter}"]

                # Add setter
                if property.codegen_properties.animation_wrapper_requires_setter is not None:
                    property_wrapper_parameters += [f"&{style_type}::{property.codegen_properties.animation_wrapper_requires_setter}"]
                else:
                    property_wrapper_parameters += [f"&{style_type}::{setter}"]

                # Add property type specific parameters
                if property.codegen_properties.visited_link_color_support:
                    property_wrapper_parameters += [f"&{style_type}::visitedLink{name_for_methods}", f"&{style_type}::setVisitedLink{name_for_methods}"]

                # Add additional specified parameters
                if property.codegen_properties.animation_wrapper_requires_additional_parameters is not None:
                    property_wrapper_parameters += property.codegen_properties.animation_wrapper_requires_additional_parameters

        return f"new {property_wrapper_type}({', '.join(property_wrapper_parameters)})"

    def _shorthand_contains_animatable_longhands(self, property, animatable_longhands):
        for longhand in property.codegen_properties.longhands:
            if longhand in animatable_longhands:
                return True
            if longhand.codegen_properties.is_logical:
                name = longhand.codegen_properties.logical_property_group.name
                physical_properties = self.properties_and_descriptors.style_properties.logical_property_groups[name]['physical']
                return any(p in animatable_longhands for p in physical_properties.values())
        return False

    def _generate_css_property_animation_wrapper_map_cpp_constructor(self, *, to):
        to.write_block("""\
            static WrapperBase* makeShorthandWrapper(CSSPropertyID id, const std::array<WrapperBase*, cssPropertyIDEnumValueCount>& wrappers)
            {
                auto shorthand = shorthandForProperty(id);
                ASSERT(shorthand.length());

                auto longhandWrappers = WTF::compactMap(shorthand, [&](auto longhand) -> std::optional<WrapperBase*> {
                    auto wrapper = wrappers[longhand];
                    if (!wrapper)
                        return std::nullopt;
                    return wrapper;
                });

                return new ShorthandWrapper(id, WTF::move(longhandWrappers));
            }
            """)

        to.write("WrapperMap::WrapperMap()")

        with to.indent():
            animatable_longhands = set()
            animatable_shorthands = set()

            to.write(": m_wrappers {")

            with to.indent():
                to.write(f"nullptr, // CSSPropertyID::CSSPropertyInvalid")
                to.write(f"nullptr, // CSSPropertyID::CSSPropertyCustom")

                for property in self.properties_and_descriptors.all_unique:
                    if not property.codegen_properties.longhands:
                        # Don't include descriptors.
                        if property in self.properties_and_descriptors.all_descriptor_only:
                            to.write(f"nullptr, // {property.id} - not animatable (descriptor only)")
                            continue
                        # Don't include logical properties.
                        if property.codegen_properties.is_logical:
                            to.write(f"nullptr, // {property.id} - logical, handled via resolution to physical")
                            continue
                        # Don't include not animatable properties.
                        if not property.is_animatable:
                            to.write(f"nullptr, // {property.id} - {property.animation_type}")
                            continue

                        construction = self._generate_css_property_animation_wrapper_map_cpp_longhand_wrapper_construction(property)
                        to.write(f"{construction}, // {property.id}")
                        animatable_longhands.add(property)
                    else:
                        if property.name == 'all' or self._shorthand_contains_animatable_longhands(property, animatable_longhands):
                            to.write(f"nullptr, // {property.id} - shorthand, will perform fix-up below")
                            animatable_shorthands.add(property)
                        else:
                            to.write(f"nullptr, // {property.id} - not animatable (shorthand, has no animatable longhands)")
            to.write("}")

        to.write("{")

        with to.indent():
            to.write("// Build animatable shorthand wrappers from longhand wrappers initialized above.")
            to.newline()
            for property in self.properties_and_descriptors.all_unique:
                if property in animatable_shorthands:
                    to.write(f"m_wrappers[{property.id}] = makeShorthandWrapper({property.id}, m_wrappers);")

        to.write("}")
        to.newline()


# Generates `StyleComputedStyleProperties.h`, `StyleComputedStyleProperties.cpp`, `StyleComputedStyleProperties+GettersInlines.h`, `StyleComputedStyleProperties+SettersInlines.h` and `StyleComputedStyleProperties+InitialInlines.h`.
class GenerateStyleComputedStyleProperties:
    def __init__(self, generation_context):
        self.generation_context = generation_context

    @property
    def properties_and_descriptors(self):
        return self.generation_context.properties_and_descriptors

    @property
    def style_properties(self):
        return self.generation_context.properties_and_descriptors.style_properties

    def generate(self):
        self.generate_style_computed_style_properties_h()
        self.generate_style_computed_style_properties_cpp()
        self.generate_style_computed_style_properties_getters_inlines_h()
        self.generate_style_computed_style_properties_setters_inlines_h()
        self.generate_style_computed_style_properties_initial_inlines_h()

    # Computes the expression of loads needed to get the member variable used to store the property.
    def _compute_get_expression(self, property, container_kind, container_path, storage_type, storage_name, storage_kind):
        # Compute getter expression, starting with the base set of loads to access the storage container.
        container = "->".join(container_path)

        if container_kind == 'data':
            expression = f"{container}->{storage_name}"
        elif container_kind == 'struct':
            expression = f"{container}.{storage_name}"
        elif container_kind == 'physical-group':
            expression = f"{container}.{Name(property.codegen_properties.logical_property_group.resolver).id_without_prefix_with_lowercase_first_letter}()"

        # If necessary, wrap the load in a cast or conversion.
        if storage_kind == 'enum':
            expression = f"static_cast<{storage_type}>({expression})"
        elif storage_kind == 'raw':
            expression = f"{storage_type}::fromRaw({expression})"
        return expression

    # Computes the expression of loads and assignments needed to set the member variable used to store the property.
    def _compute_set_expression(self, property, container_kind, container_path, storage_type, storage_name, storage_kind, argument_name):
        # Compute the right side of the assignment expression for the setter expression.
        if storage_kind == 'reference':
            rhs = f"WTF::move({argument_name})"
        elif storage_kind == 'enum':
            rhs = f"static_cast<unsigned>({argument_name})"
        elif storage_kind == 'raw':
            rhs = f"{argument_name}.toRaw()"
        else:
            rhs = f"{argument_name}"

        # Compute setter expression, starting with the base set of loads to access the storage container.
        container = ".access().".join(container_path)

        if container_kind == 'data':
            expression = f"{container}.access().{storage_name} = {rhs}"
        elif container_kind == 'struct':
            expression = f"{container}.{storage_name} = {rhs}"
        elif container_kind == 'physical-group':
            expression = f"{container}.set{Name(property.codegen_properties.logical_property_group.resolver).id_without_prefix}({rhs})"

        return expression

    # Computes the expression, if any, to call after setting the value to storage.
    def _compute_did_set_expression(self, property):
        if not property.codegen_properties.computed_style_setter_requires_did_set:
            return None
        return f"didSet{property.codegen_properties.computed_style_name_for_methods}()"

    # Computes the expression for the initial value function.
    def _compute_initial_expression(self, property):
        def pick_literal_expression(element):
            if property.codegen_properties.computed_style_storage_kind == 'enum':
                return element.cpp_enum_literal(property.codegen_properties.computed_style_type)
            return element.cpp_literal

        if len(property.initial.list) == 1:
            if isinstance(property.initial.list[0], SpecialLiteral):
                raise Exception(f"Special literals must have custom initial function implementation")
            return pick_literal_expression(property.initial.list[0])
        else:
            return "{ " + ", ".join(pick_literal_expression(element) for element in property.initial.list) + " }"

    # Generate StyleComputedStyleProperties.h

    def _generate_property_function_declarations(self, *, to):
        for property in self.style_properties.all:
            if property.codegen_properties.skip_computed_style:
                continue
            if property.codegen_properties.is_logical:
                continue
            if property.codegen_properties.longhands:
                continue
            if property.codegen_properties.cascade_alias:
                continue
            if property.codegen_properties.coordinated_value_list_property:
                continue

            to.write(f"// '{property}'")

            if not property.codegen_properties.skip_computed_style_getter:
                getter_name = property.codegen_properties.computed_style_getter
                getter_function_specifiers = property.getter_declaration_function_specifiers
                getter_return_type = property.getter_return_type

                self.generation_context.generate_function_declaration(
                    to=to,
                    function_name=getter_name,
                    function_specifiers=getter_function_specifiers,
                    return_type=getter_return_type,
                    function_qualifiers=['const']
                )

            if not property.codegen_properties.skip_computed_style_setter:
                setter_name = property.codegen_properties.computed_style_setter
                setter_function_specifiers = property.setter_declaration_function_specifiers
                setter_return_type = property.setter_return_type
                setter_argument_type = property.setter_argument_type

                self.generation_context.generate_function_declaration(
                    to=to,
                    function_name=setter_name,
                    function_specifiers=setter_function_specifiers,
                    return_type=setter_return_type,
                    argument_types=[setter_argument_type]
                )

                if property.codegen_properties.computed_style_setter_requires_did_set:
                    did_set_name = f"didSet{property.codegen_properties.computed_style_name_for_methods}"
                    did_set_function_specifiers = ['inline']
                    did_set_return_type = 'void'

                    self.generation_context.generate_function_declaration(
                        to=to,
                        function_name=did_set_name,
                        function_specifiers=did_set_function_specifiers,
                        return_type=did_set_return_type
                    )

            if not property.codegen_properties.skip_computed_style_initial:
                initial_name = property.codegen_properties.computed_style_initial
                initial_function_specifiers = property.initial_declaration_function_specifiers
                initial_return_type = property.initial_return_type

                self.generation_context.generate_function_declaration(
                    to=to,
                    function_name=initial_name,
                    function_specifiers=initial_function_specifiers,
                    return_type=initial_return_type
                )

            if property.codegen_properties.color_property:
                if property.codegen_properties.computed_style_visited_link_storage_path:
                    if not property.codegen_properties.visited_link_color_support:
                        raise Exception("Property {property} has computed_style_visited_link_storage_path but not visited_link_color_support")
                    getter_name = f"visitedLink{property.codegen_properties.computed_style_name_for_methods}"
                    setter_name = f"setVisitedLink{property.codegen_properties.computed_style_name_for_methods}"
                    getter_function_specifiers = ['inline']
                    getter_return_type = property.getter_return_type
                    setter_function_specifiers = ['inline']
                    setter_return_type = 'void'
                    setter_argument_type = property.setter_argument_type

                    self.generation_context.generate_function_declaration(
                        to=to,
                        function_name=getter_name,
                        function_specifiers=getter_function_specifiers,
                        return_type=getter_return_type,
                        function_qualifiers=['const']
                    )
                    self.generation_context.generate_function_declaration(
                        to=to,
                        function_name=setter_name,
                        function_specifiers=setter_function_specifiers,
                        return_type=setter_return_type,
                        argument_types=[setter_argument_type]
                    )

                self.generation_context.generate_function_declaration(
                    to=to,
                    function_name=f"{property.codegen_properties.computed_style_getter}Resolver",
                    function_specifiers=['inline'],
                    return_type="decltype(auto)",
                    function_qualifiers=['const']
                )

                if property.codegen_properties.computed_style_resolving_current_color_exported:
                    function_specifiers = ['WEBCORE_EXPORT']
                else:
                    function_specifiers = []

                self.generation_context.generate_function_declaration(
                    to=to,
                    function_name=f"{property.codegen_properties.computed_style_getter}ResolvingCurrentColor",
                    function_specifiers=function_specifiers,
                    return_type='WebCore::Color',
                    function_qualifiers=['const']
                )

                if property.codegen_properties.computed_style_resolving_current_color_applying_color_filter_exported:
                    function_specifiers = ['WEBCORE_EXPORT']
                else:
                    function_specifiers = []

                self.generation_context.generate_function_declaration(
                    to=to,
                    function_name=f"{property.codegen_properties.computed_style_getter}ResolvingCurrentColorApplyingColorFilter",
                    function_specifiers=function_specifiers,
                    return_type='WebCore::Color',
                    function_qualifiers=['const']
                )

                if property.codegen_properties.computed_style_visited_link_storage_path:
                    if property.codegen_properties.computed_style_visited_link_resolving_current_color_exported:
                        function_specifiers = ['WEBCORE_EXPORT']
                    else:
                        function_specifiers = []

                    self.generation_context.generate_function_declaration(
                        to=to,
                        function_name=f"visitedLink{property.codegen_properties.computed_style_name_for_methods}ResolvingCurrentColor",
                        function_specifiers=function_specifiers,
                        return_type='WebCore::Color',
                        function_qualifiers=['const']
                    )

                    if property.codegen_properties.computed_style_visited_link_resolving_current_color_applying_color_filter_exported:
                        function_specifiers = ['WEBCORE_EXPORT']
                    else:
                        function_specifiers = []

                    self.generation_context.generate_function_declaration(
                        to=to,
                        function_name=f"visitedLink{property.codegen_properties.computed_style_name_for_methods}ResolvingCurrentColorApplyingColorFilter",
                        function_specifiers=function_specifiers,
                        return_type='WebCore::Color',
                        function_qualifiers=['const']
                    )

                if property.codegen_properties.computed_style_visited_dependent_exported:
                    function_specifiers = ['WEBCORE_EXPORT']
                else:
                    function_specifiers = []

                self.generation_context.generate_function_declaration(
                    to=to,
                    function_name=f"visitedDependent{property.codegen_properties.computed_style_name_for_methods}",
                    function_specifiers=function_specifiers,
                    return_type='WebCore::Color',
                    argument_types=['OptionSet<PaintBehavior>'],
                    function_qualifiers=['const']
                )

                if property.codegen_properties.computed_style_visited_dependent_applying_color_filter_exported:
                    function_specifiers = ['WEBCORE_EXPORT']
                else:
                    function_specifiers = []

                self.generation_context.generate_function_declaration(
                    to=to,
                    function_name=f"visitedDependent{property.codegen_properties.computed_style_name_for_methods}ApplyingColorFilter",
                    function_specifiers=function_specifiers,
                    return_type='WebCore::Color',
                    argument_types=['OptionSet<PaintBehavior>'],
                    function_qualifiers=['const']
                )

            if property.codegen_properties.computed_style_has_explicitly_set_storage_path:
                getter_name = f"hasExplicitlySet{property.codegen_properties.computed_style_name_for_methods}"
                setter_name = f"setHasExplicitlySet{property.codegen_properties.computed_style_name_for_methods}"
                getter_function_specifiers = ['inline']
                getter_return_type = 'bool'
                setter_function_specifiers = ['inline']
                setter_return_type = f"void"
                setter_argument_type = 'bool'

                self.generation_context.generate_function_declaration(
                    to=to,
                    function_name=getter_name,
                    function_specifiers=getter_function_specifiers,
                    return_type=getter_return_type,
                    function_qualifiers=['const']
                )
                self.generation_context.generate_function_declaration(
                    to=to,
                    function_name=setter_name,
                    function_specifiers=setter_function_specifiers,
                    return_type=setter_return_type,
                    argument_types=[setter_argument_type]
                )
            to.newline()

    def _generate_logical_property_function_declarations(self, *, to):
        for property_group_name, property_group in self.style_properties.logical_property_groups.items():
            to.write(f"// Logical getters and setters for '{property_group_name}' properties of type '{property_group['kind']}'.")
            if property_group['kind'] == 'axis':
                horizontal_property = property_group['physical']['horizontal']
                vertical_property = property_group['physical']['vertical']

                # NOTE: Choice of which property we use here is arbitrary, they must always have the same types.
                getter_return_type = horizontal_property.getter_return_type
                setter_argument_type = horizontal_property.setter_argument_type

                for property in [horizontal_property, vertical_property]:
                    self.generation_context.generate_function_declaration(
                        to=to,
                        function_name=f"logical{property.id_without_prefix}",
                        function_specifiers=['inline'],
                        return_type=getter_return_type,
                        argument_types=['WritingMode'],
                        function_qualifiers=['const']
                    )
                for property in [horizontal_property, vertical_property]:
                    self.generation_context.generate_function_declaration(
                        to=to,
                        function_name=f"logical{property.id_without_prefix}",
                        function_specifiers=['inline'],
                        return_type=getter_return_type,
                        argument_types=[],
                        function_qualifiers=['const']
                    )
                for property in [horizontal_property, vertical_property]:
                    self.generation_context.generate_function_declaration(
                        to=to,
                        function_name=f"setLogical{property.id_without_prefix}",
                        function_specifiers=['inline'],
                        return_type='void',
                        argument_types=[setter_argument_type]
                    )
            elif property_group['kind'] == 'side':
                property = property_group['physical']['bottom']
                # NOTE: Choice of which property we use here is arbitrary, they must always have the same types.
                getter_return_type = property.getter_return_type
                setter_argument_type = property.setter_argument_type

                prefix = Name(property_group_name)

                for edge in ['Start', 'End', 'Before', 'After', 'LogicalLeft', 'LogicalRight']:
                    self.generation_context.generate_function_declaration(
                        to=to,
                        function_name=f"{prefix.id_without_prefix_with_lowercase_first_letter}{edge}",
                        function_specifiers=['inline'],
                        return_type=getter_return_type,
                        argument_types=['WritingMode'],
                        function_qualifiers=['const']
                    )
                for edge in ['Start', 'End', 'Before', 'After', 'LogicalLeft', 'LogicalRight']:
                    self.generation_context.generate_function_declaration(
                        to=to,
                        function_name=f"{prefix.id_without_prefix_with_lowercase_first_letter}{edge}",
                        function_specifiers=['inline'],
                        return_type=getter_return_type,
                        argument_types=[],
                        function_qualifiers=['const']
                    )
                for edge in ['Start', 'End', 'Before', 'After', 'LogicalLeft', 'LogicalRight']:
                    self.generation_context.generate_function_declaration(
                        to=to,
                        function_name=f"set{prefix.id_without_prefix}{edge}",
                        function_specifiers=['inline'],
                        return_type='void',
                        argument_types=[setter_argument_type]
                    )
            else:
                # FIXME: Add logical getters / setters for other group kinds (like 'corner') if it would be useful.
                to.write(f"// FIXME: Add support for logical getter/setters of kind '{property_group['kind']}'.")
            to.newline()

    def _generate_color_trait_declarations(self, *, to):
        for property in self.style_properties.all:
            if not property.codegen_properties.color_property:
                continue
            if property.codegen_properties.skip_computed_style:
                continue
            if property.codegen_properties.is_logical:
                continue
            if property.codegen_properties.longhands:
                continue
            if property.codegen_properties.cascade_alias:
                continue
            if property.codegen_properties.coordinated_value_list_property:
                continue

            to.write(f"template<> struct ColorPropertyTraits<PropertyNameConstant<{property.id_without_scope}>> {{")
            with to.indent():
                self.generation_context.generate_function_declaration(
                    to=to,
                    function_name='color',
                    function_specifiers=['static', 'inline'],
                    return_type='const Color&',
                    argument_types=['const ComputedStyleProperties&']
                )
                if property.codegen_properties.visited_link_color_support:
                    self.generation_context.generate_function_declaration(
                        to=to,
                        function_name='visitedLinkColor',
                        function_specifiers=['static', 'inline'],
                        return_type='const Color&',
                        argument_types=['const ComputedStyleProperties&']
                    )

                if property.codegen_properties.color_property_traits_requires_resolving_current_color:
                    self.generation_context.generate_function_declaration(
                        to=to,
                        function_name='colorResolvingCurrentColor',
                        function_specifiers=['static', 'inline'],
                        return_type='WebCore::Color',
                        argument_types=['const ComputedStyleProperties&']
                    )
                    if property.codegen_properties.visited_link_color_support:
                        self.generation_context.generate_function_declaration(
                            to=to,
                            function_name='visitedLinkColorResolvingCurrentColor',
                            function_specifiers=['static', 'inline'],
                            return_type='WebCore::Color',
                            argument_types=['const ComputedStyleProperties&']
                        )

                if property.codegen_properties.color_property_traits_requires_excludes_visited_link_color:
                    self.generation_context.generate_function_declaration(
                        to=to,
                        function_name='excludesVisitedLinkColor',
                        function_specifiers=['static', 'inline'],
                        return_type='bool',
                        argument_types=['const WebCore::Color&']
                    )

            to.write(f"}};")
            to.newline()

    def generate_style_computed_style_properties_h(self):
        with open('StyleComputedStyleProperties.h', 'w') as output_file:
            writer = Writer(output_file)

            self.generation_context.generate_heading(
                to=writer
            )

            self.generation_context.generate_required_header_pragma(
                to=writer
            )

            self.generation_context.generate_includes(
                to=writer,
                system_headers=[
                    "<WebCore/StyleComputedStyleBase.h>",
                ]
            )

            with self.generation_context.namespaces(["WebCore", "Style"], to=writer):
                writer.write(f"class ComputedStyleProperties : public ComputedStyleBase {{")
                writer.write(f"public:")

                with writer.indent():
                    self._generate_property_function_declarations(
                        to=writer
                    )
                    self._generate_logical_property_function_declarations(
                        to=writer
                    )

                writer.write(f"protected:")

                with writer.indent():
                    writer.write(f"ComputedStyleProperties(ComputedStyleProperties&&) = default;")
                    writer.write(f"ComputedStyleProperties& operator=(ComputedStyleProperties&&) = default;")
                    writer.newline()

                    writer.write(f"ComputedStyleProperties(CreateDefaultStyleTag tag) : ComputedStyleBase {{ tag }} {{ }}")
                    writer.write(f"ComputedStyleProperties(const ComputedStyleProperties& other, CloneTag tag) : ComputedStyleBase {{ other, tag }} {{ }}")
                    writer.newline()

                    writer.write(f"ComputedStyleProperties(ComputedStyleProperties& a, ComputedStyleProperties&& b) : ComputedStyleBase {{ a, WTF::move(b) }} {{ }}")

                writer.write(f"}};")
                writer.newline()

                self._generate_color_trait_declarations(
                    to=writer
                )

    # Generate StyleComputedStyleProperties.cpp

    def _generate_color_resolution_function_definitions(self, *, to):
        for property in self.style_properties.all:
            if property.codegen_properties.skip_computed_style:
                continue
            if property.codegen_properties.is_logical:
                continue
            if property.codegen_properties.longhands:
                continue
            if property.codegen_properties.cascade_alias:
                continue
            if property.codegen_properties.coordinated_value_list_property:
                continue
            if not property.codegen_properties.color_property:
                continue

            color_resolver = f"{property.codegen_properties.computed_style_getter}Resolver()"

            to.write(f"WebCore::Color ComputedStyleProperties::{property.codegen_properties.computed_style_getter}ResolvingCurrentColor() const")
            to.write(f"{{")
            with to.indent():
                to.write(f"return {color_resolver}.colorResolvingCurrentColor();")
            to.write(f"}}")
            to.newline()

            to.write(f"WebCore::Color ComputedStyleProperties::{property.codegen_properties.computed_style_getter}ResolvingCurrentColorApplyingColorFilter() const")
            to.write(f"{{")
            with to.indent():
                to.write(f"return {color_resolver}.colorResolvingCurrentColorApplyingColorFilter();")
            to.write(f"}}")
            to.newline()

            if property.codegen_properties.computed_style_visited_link_storage_path:
                to.write(f"WebCore::Color ComputedStyleProperties::visitedLink{property.codegen_properties.computed_style_name_for_methods}ResolvingCurrentColor() const")
                to.write(f"{{")
                with to.indent():
                    to.write(f"return {color_resolver}.visitedLinkColorResolvingCurrentColor();")
                to.write(f"}}")
                to.newline()

                to.write(f"WebCore::Color ComputedStyleProperties::visitedLink{property.codegen_properties.computed_style_name_for_methods}ResolvingCurrentColorApplyingColorFilter() const")
                to.write(f"{{")
                with to.indent():
                    to.write(f"return {color_resolver}.visitedLinkColorResolvingCurrentColorApplyingColorFilter();")
                to.write(f"}}")
                to.newline()

            to.write(f"WebCore::Color ComputedStyleProperties::visitedDependent{property.codegen_properties.computed_style_name_for_methods}(OptionSet<PaintBehavior> paintBehavior) const")
            to.write(f"{{")
            with to.indent():
                to.write(f"return {color_resolver}.visitedDependentColor(paintBehavior);")
            to.write(f"}}")
            to.newline()

            to.write(f"WebCore::Color ComputedStyleProperties::visitedDependent{property.codegen_properties.computed_style_name_for_methods}ApplyingColorFilter(OptionSet<PaintBehavior> paintBehavior) const")
            to.write(f"{{")
            with to.indent():
                to.write(f"return {color_resolver}.visitedDependentColorApplyingColorFilter(paintBehavior);")
            to.write(f"}}")
            to.newline()

    def generate_style_computed_style_properties_cpp(self):
        with open('StyleComputedStyleProperties.cpp', 'w') as output_file:
            writer = Writer(output_file)

            self.generation_context.generate_heading(
                to=writer
            )

            self.generation_context.generate_cpp_required_includes(
                to=writer,
                header="StyleComputedStyleProperties.h"
            )

            self.generation_context.generate_includes(
                to=writer,
                headers=[
                    "StyleComputedStyle+GettersInlines.h",
                ]
            )

            with self.generation_context.namespaces(["WebCore", "Style"], to=writer):
                self._generate_color_resolution_function_definitions(
                    to=writer
                )

    # Generate StyleComputedStyleProperties+GettersInlines.h

    def _generate_getters_inlines_property_function_definition(self, *, to, function_name, function_specifiers, return_type, get_expression):
        to.write(f"{''.join(map(lambda x: x + ' ', function_specifiers))}{return_type} ComputedStyleProperties::{function_name}() const")
        to.write(f"{{")
        with to.indent():
            to.write(f"return {get_expression};")
        to.write(f"}}")
        to.newline()

    def _generate_getters_inlines_property_function_definitions(self, *, to):
        for property in self.style_properties.all:
            if property.codegen_properties.skip_computed_style:
                continue
            if property.codegen_properties.is_logical:
                continue
            if property.codegen_properties.longhands:
                continue
            if property.codegen_properties.cascade_alias:
                continue
            if property.codegen_properties.coordinated_value_list_property:
                continue

            if not property.codegen_properties.computed_style_storage_path and not property.codegen_properties.skip_computed_style_getter:
                raise Exception(f"Missing ComputedStyle storage path for property {property.id}.")

            if not property.codegen_properties.computed_style_getter_custom and not property.codegen_properties.skip_computed_style_getter:
                function_name = property.codegen_properties.computed_style_getter
                function_specifiers = property.getter_definition_function_specifiers
                return_type = property.getter_return_type
                storage_type = property.codegen_properties.computed_style_type
                storage_name = property.codegen_properties.computed_style_storage_name
                storage_kind = property.codegen_properties.computed_style_storage_kind
                container_kind = property.codegen_properties.computed_style_storage_container
                container_path = property.codegen_properties.computed_style_storage_path

                self._generate_getters_inlines_property_function_definition(
                    to=to,
                    function_name=function_name,
                    function_specifiers=function_specifiers,
                    return_type=return_type,
                    get_expression=self._compute_get_expression(property, container_kind, container_path, storage_type, storage_name, storage_kind)
                )

            if property.codegen_properties.color_property:
                if property.codegen_properties.computed_style_visited_link_storage_path and not property.codegen_properties.skip_computed_style_getter:
                    function_name = f"visitedLink{property.codegen_properties.computed_style_name_for_methods}"
                    function_specifiers = ['inline']
                    return_type = property.getter_return_type
                    storage_type = property.codegen_properties.computed_style_type
                    storage_kind = property.codegen_properties.computed_style_storage_kind  # the storage kind for visited links are always the same as the principle value
                    storage_name = property.codegen_properties.computed_style_visited_link_storage_name
                    container_kind = property.codegen_properties.computed_style_visited_link_storage_container
                    container_path = property.codegen_properties.computed_style_visited_link_storage_path

                    self._generate_getters_inlines_property_function_definition(
                        to=to,
                        function_name=function_name,
                        function_specifiers=function_specifiers,
                        return_type=return_type,
                        get_expression=self._compute_get_expression(property, container_kind, container_path, storage_type, storage_name, storage_kind)
                    )

                color_resolver_getter_name = f"{property.codegen_properties.computed_style_getter}Resolver"
                color_resolver_getter_function_specifiers = ['inline']
                color_resolver_getter_return_type = 'decltype(auto)'
                color_resolver_getter_expression = f"ColorPropertyResolver<ColorPropertyTraits<PropertyNameConstant<{property.id_without_scope}>>> {{ *this }}"

                self._generate_getters_inlines_property_function_definition(
                    to=to,
                    function_name=color_resolver_getter_name,
                    function_specifiers=color_resolver_getter_function_specifiers,
                    return_type=color_resolver_getter_return_type,
                    get_expression=color_resolver_getter_expression
                )

            if property.codegen_properties.computed_style_has_explicitly_set_storage_path:
                function_name = f"hasExplicitlySet{property.codegen_properties.computed_style_name_for_methods}"
                function_specifiers = ['inline']
                return_type = 'bool'
                storage_type = 'bool'
                storage_kind = 'value'
                storage_name = property.codegen_properties.computed_style_has_explicitly_set_storage_name
                container_kind = property.codegen_properties.computed_style_has_explicitly_set_storage_container
                container_path = property.codegen_properties.computed_style_has_explicitly_set_storage_path

                self._generate_getters_inlines_property_function_definition(
                    to=to,
                    function_name=function_name,
                    function_specifiers=function_specifiers,
                    return_type=return_type,
                    get_expression=self._compute_get_expression(property, container_kind, container_path, storage_type, storage_name, storage_kind)
                )

    def _generate_getters_inlines_function_definition_logical_axis(self, *, to, axis, getter_return_type, horizontal, vertical):
        to.write(f"inline {getter_return_type} ComputedStyleProperties::logical{axis.id_without_prefix}(WritingMode writingMode) const")
        to.write(f"{{")
        with to.indent():
            to.write(f"return writingMode.isHorizontal() ? {horizontal.codegen_properties.computed_style_getter}() : {vertical.codegen_properties.computed_style_getter}();")
        to.write(f"}}")
        to.newline()

        to.write(f"inline {getter_return_type} ComputedStyleProperties::logical{axis.id_without_prefix}() const")
        to.write(f"{{")
        with to.indent():
            to.write(f"return logical{axis.id_without_prefix}(writingMode());")
        to.write(f"}}")
        to.newline()

    def _generate_getters_inlines_function_definition_logical_side_start_end(self, *, to, prefix, edge, getter_return_type, left, right, top, bottom):
        to.write(f"inline {getter_return_type} ComputedStyleProperties::{prefix.id_without_prefix_with_lowercase_first_letter}{edge}(WritingMode writingMode) const")
        to.write(f"{{")
        with to.indent():
            to.write(f"if (writingMode.isHorizontal())")
            with to.indent():
                to.write(f"return writingMode.isInlineLeftToRight() ? {left.codegen_properties.computed_style_getter}() : {right.codegen_properties.computed_style_getter}();")
            to.write(f"else")
            with to.indent():
                to.write(f"return writingMode.isInlineTopToBottom() ? {top.codegen_properties.computed_style_getter}() : {bottom.codegen_properties.computed_style_getter}();")
        to.write(f"}}")
        to.newline()

        to.write(f"inline {getter_return_type} ComputedStyleProperties::{prefix.id_without_prefix_with_lowercase_first_letter}{edge}() const")
        to.write(f"{{")
        with to.indent():
            to.write(f"return {prefix.id_without_prefix_with_lowercase_first_letter}{edge}(writingMode());")
        to.write(f"}}")
        to.newline()

    def _generate_getters_inlines_function_definition_logical_side_before_after(self, *, to, prefix, edge, getter_return_type, left, right, top, bottom):
        to.write(f"inline {getter_return_type} ComputedStyleProperties::{prefix.id_without_prefix_with_lowercase_first_letter}{edge}(WritingMode writingMode) const")
        to.write(f"{{")
        with to.indent():
            to.write(f"switch (writingMode.blockDirection()) {{")
            to.write(f"case FlowDirection::LeftToRight:")
            with to.indent():
                to.write(f"return {left.codegen_properties.computed_style_getter}();")
            to.write(f"case FlowDirection::RightToLeft:")
            with to.indent():
                to.write(f"return {right.codegen_properties.computed_style_getter}();")
            to.write(f"case FlowDirection::TopToBottom:")
            with to.indent():
                to.write(f"return {top.codegen_properties.computed_style_getter}();")
            to.write(f"case FlowDirection::BottomToTop:")
            with to.indent():
                to.write(f"return {bottom.codegen_properties.computed_style_getter}();")
            to.write(f"}}")
            to.write(f"ASSERT_NOT_REACHED();")
            to.write(f"return {bottom.codegen_properties.computed_style_getter}();")
        to.write(f"}}")
        to.newline()

        to.write(f"inline {getter_return_type} ComputedStyleProperties::{prefix.id_without_prefix_with_lowercase_first_letter}{edge}() const")
        to.write(f"{{")
        with to.indent():
            to.write(f"return {prefix.id_without_prefix_with_lowercase_first_letter}{edge}(writingMode());")
        to.write(f"}}")
        to.newline()

    def _generate_getters_inlines_function_definition_logical_side_left_right(self, *, to, prefix, edge, getter_return_type, left, right):
        to.write(f"inline {getter_return_type} ComputedStyleProperties::{prefix.id_without_prefix_with_lowercase_first_letter}{edge}(WritingMode writingMode) const")
        to.write(f"{{")
        with to.indent():
            to.write(f"if (writingMode.isHorizontal())")
            with to.indent():
                to.write(f"return {left.codegen_properties.computed_style_getter}();")
            to.write(f"else")
            with to.indent():
                to.write(f"return {right.codegen_properties.computed_style_getter}();")
        to.write(f"}}")
        to.newline()

        to.write(f"inline {getter_return_type} ComputedStyleProperties::{prefix.id_without_prefix_with_lowercase_first_letter}{edge}() const")
        to.write(f"{{")
        with to.indent():
            to.write(f"return {prefix.id_without_prefix_with_lowercase_first_letter}{edge}(writingMode());")
        to.write(f"}}")
        to.newline()

    def _generate_getters_inlines_logical_property_function_definitions(self, *, to):
        for property_group_name, property_group in self.style_properties.logical_property_groups.items():
            if property_group['kind'] == 'axis':
                horizontal = property_group['physical']['horizontal']
                vertical = property_group['physical']['vertical']

                # NOTE: Choice of which property we use here is arbitrary, they must always have the same types.
                getter_return_type = horizontal.getter_return_type

                self._generate_getters_inlines_function_definition_logical_axis(
                    to=to,
                    axis=horizontal,
                    getter_return_type=getter_return_type,
                    horizontal=horizontal,
                    vertical=vertical
                )
                self._generate_getters_inlines_function_definition_logical_axis(
                    to=to,
                    axis=vertical,
                    getter_return_type=getter_return_type,
                    horizontal=vertical,
                    vertical=horizontal
                )
            elif property_group['kind'] == 'side':
                left = property_group['physical']['left']
                right = property_group['physical']['right']
                bottom = property_group['physical']['bottom']
                top = property_group['physical']['top']

                # NOTE: Choice of which property we use here is arbitrary, they must always have the same types.
                getter_return_type = left.getter_return_type

                prefix = Name(property_group_name)

                self._generate_getters_inlines_function_definition_logical_side_start_end(
                    to=to,
                    prefix=prefix,
                    edge='Start',
                    getter_return_type=getter_return_type,
                    left=left,
                    right=right,
                    top=top,
                    bottom=bottom
                )
                self._generate_getters_inlines_function_definition_logical_side_start_end(
                    to=to,
                    prefix=prefix,
                    edge='End',
                    getter_return_type=getter_return_type,
                    left=right,
                    right=left,
                    top=bottom,
                    bottom=top
                )
                self._generate_getters_inlines_function_definition_logical_side_before_after(
                    to=to,
                    prefix=prefix,
                    edge='Before',
                    getter_return_type=getter_return_type,
                    left=left,
                    right=right,
                    top=top,
                    bottom=bottom
                )
                self._generate_getters_inlines_function_definition_logical_side_before_after(
                    to=to,
                    prefix=prefix,
                    edge='After',
                    getter_return_type=getter_return_type,
                    left=right,
                    right=left,
                    top=bottom,
                    bottom=top
                )
                self._generate_getters_inlines_function_definition_logical_side_left_right(
                    to=to,
                    prefix=prefix,
                    edge='LogicalLeft',
                    getter_return_type=getter_return_type,
                    left=left,
                    right=top
                )
                self._generate_getters_inlines_function_definition_logical_side_left_right(
                    to=to,
                    prefix=prefix,
                    edge='LogicalRight',
                    getter_return_type=getter_return_type,
                    left=right,
                    right=bottom
                )

    def _generate_getters_inlines_color_trait_definitions(self, *, to):
        for property in self.style_properties.all:
            if not property.codegen_properties.color_property:
                continue
            if property.codegen_properties.skip_computed_style:
                continue
            if property.codegen_properties.is_logical:
                continue
            if property.codegen_properties.longhands:
                continue
            if property.codegen_properties.cascade_alias:
                continue
            if property.codegen_properties.coordinated_value_list_property:
                continue

            if not property.codegen_properties.color_property_traits_color_custom:
                to.write(f"inline const Color& ColorPropertyTraits<PropertyNameConstant<{property.id_without_scope}>>::color(const ComputedStyleProperties& style)")
                to.write(f"{{")
                with to.indent():
                    to.write(f"return style.{property.codegen_properties.computed_style_getter}();")
                to.write(f"}}")
                to.newline()

            if property.codegen_properties.visited_link_color_support and not property.codegen_properties.color_property_traits_visited_link_color_custom:
                to.write(f"inline const Color& ColorPropertyTraits<PropertyNameConstant<{property.id_without_scope}>>::visitedLinkColor(const ComputedStyleProperties& style)")
                to.write(f"{{")
                with to.indent():
                    to.write(f"return style.visitedLink{property.codegen_properties.computed_style_name_for_methods}();")
                to.write(f"}}")
                to.newline()

    def generate_style_computed_style_properties_getters_inlines_h(self):
        with open('StyleComputedStyleProperties+GettersInlines.h', 'w') as output_file:
            writer = Writer(output_file)

            self.generation_context.generate_heading(
                to=writer
            )

            self.generation_context.generate_required_header_pragma(
                to=writer
            )

            writer.write("#ifndef COMPUTED_STYLE_PROPERTIES_GETTERS_INLINES_INCLUDE_TRAP")
            writer.write("#error \"Please do not include this file anywhere except from StyleComputedStyle+GettersInlines.h.\"")
            writer.write("#endif")
            writer.newline()

            self.generation_context.generate_includes(
                to=writer,
                system_headers=[
                    "<WebCore/StyleComputedStyleProperties+GettersCustomInlines.h>",
                ]
            )

            with self.generation_context.namespaces(["WebCore", "Style"], to=writer):
                self._generate_getters_inlines_property_function_definitions(
                    to=writer
                )
                self._generate_getters_inlines_logical_property_function_definitions(
                    to=writer
                )
                self._generate_getters_inlines_color_trait_definitions(
                    to=writer
                )

    # Generate StyleComputedStyleProperties+SettersInlines.h

    def _generate_setters_inlines_property_function_definition(self, *, to, function_name, function_specifiers, return_type, argument_type, argument_name, get_expression, set_expression, did_set_expression=None):
        to.write(f"{''.join(map(lambda x: x + ' ', function_specifiers))}{return_type} ComputedStyleProperties::{function_name}({argument_type} {argument_name})")
        to.write(f"{{")
        with to.indent():
            if return_type == 'void':
                to.write(f"if ({argument_name} != {get_expression}) {{")
                with to.indent():
                    to.write(f"{set_expression};")
                    if did_set_expression:
                        to.write(f"{did_set_expression};")
                to.write(f"}}")
            elif return_type == 'bool':
                to.write(f"if ({argument_name} != {get_expression}) {{")
                with to.indent():
                    to.write(f"{set_expression};")
                    if did_set_expression:
                        to.write(f"{did_set_expression};")
                    to.write(f"return true;")
                to.write(f"}}")
                to.write(f"return false;")
        to.write(f"}}")
        to.newline()

    def _generate_setters_inlines_property_function_definitions(self, *, to):
        for property in self.style_properties.all:
            if property.codegen_properties.skip_computed_style:
                continue
            if property.codegen_properties.skip_computed_style_setter:
                continue
            if property.codegen_properties.is_logical:
                continue
            if property.codegen_properties.longhands:
                continue
            if property.codegen_properties.cascade_alias:
                continue
            if property.codegen_properties.coordinated_value_list_property:
                continue

            if not property.codegen_properties.computed_style_storage_path:
                raise Exception(f"Missing ComputedStyle storage path for property {property.id}.")

            if not property.codegen_properties.computed_style_setter_custom:
                function_name = property.codegen_properties.computed_style_setter
                function_specifiers = property.setter_definition_function_specifiers
                return_type = "void"
                argument_type = property.setter_argument_type
                argument_name = "value"
                storage_type = property.codegen_properties.computed_style_type
                storage_kind = property.codegen_properties.computed_style_storage_kind
                storage_name = property.codegen_properties.computed_style_storage_name
                container_kind = property.codegen_properties.computed_style_storage_container
                container_path = property.codegen_properties.computed_style_storage_path

                self._generate_setters_inlines_property_function_definition(
                    to=to,
                    function_name=function_name,
                    function_specifiers=function_specifiers,
                    return_type=return_type,
                    argument_type=argument_type,
                    argument_name=argument_name,
                    get_expression=self._compute_get_expression(property, container_kind, container_path, storage_type, storage_name, storage_kind),
                    set_expression=self._compute_set_expression(property, container_kind, container_path, storage_type, storage_name, storage_kind, argument_name),
                    did_set_expression=self._compute_did_set_expression(property)
                )

            if property.codegen_properties.computed_style_visited_link_storage_path:
                function_name = f"setVisitedLink{property.codegen_properties.computed_style_name_for_methods}"
                function_specifiers = ['inline']
                return_type = "void"
                argument_type = property.setter_argument_type
                argument_name = "value"
                storage_type = property.codegen_properties.computed_style_type
                storage_kind = property.codegen_properties.computed_style_storage_kind  # the storage kind for visited links are always the same as the principle value
                storage_name = property.codegen_properties.computed_style_visited_link_storage_name
                container_kind = property.codegen_properties.computed_style_visited_link_storage_container
                container_path = property.codegen_properties.computed_style_visited_link_storage_path

                self._generate_setters_inlines_property_function_definition(
                    to=to,
                    function_name=function_name,
                    function_specifiers=function_specifiers,
                    return_type=return_type,
                    argument_type=argument_type,
                    argument_name=argument_name,
                    get_expression=self._compute_get_expression(property, container_kind, container_path, storage_type, storage_name, storage_kind),
                    set_expression=self._compute_set_expression(property, container_kind, container_path, storage_type, storage_name, storage_kind, argument_name)
                )

            if property.codegen_properties.computed_style_has_explicitly_set_storage_path:
                function_name = f"setHasExplicitlySet{property.codegen_properties.computed_style_name_for_methods}"
                function_specifiers = ['inline']
                return_type = "void"
                argument_type = 'bool'
                argument_name = f"value"
                storage_type = 'bool'
                storage_kind = 'value'
                storage_name = property.codegen_properties.computed_style_has_explicitly_set_storage_name
                container_kind = property.codegen_properties.computed_style_has_explicitly_set_storage_container
                container_path = property.codegen_properties.computed_style_has_explicitly_set_storage_path

                self._generate_setters_inlines_property_function_definition(
                    to=to,
                    function_name=function_name,
                    function_specifiers=function_specifiers,
                    return_type=return_type,
                    argument_type=argument_type,
                    argument_name=argument_name,
                    get_expression=self._compute_get_expression(property, container_kind, container_path, storage_type, storage_name, storage_kind),
                    set_expression=self._compute_set_expression(property, container_kind, container_path, storage_type, storage_name, storage_kind, argument_name)
                )

    def _generate_setters_inlines_function_definition_logical_axis(self, *, to, axis, setter_argument_type, horizontal, vertical):
        to.write(f"inline void ComputedStyleProperties::setLogical{axis.id_without_prefix}({setter_argument_type} value)")
        to.write(f"{{")
        with to.indent():
            to.write(f"if (writingMode().isHorizontal())")
            with to.indent():
                to.write(f"{horizontal.codegen_properties.computed_style_setter}(WTF::move(value));")
            to.write(f"else")
            with to.indent():
                to.write(f"{vertical.codegen_properties.computed_style_setter}(WTF::move(value));")
        to.write(f"}}")
        to.newline()

    def _generate_setters_inlines_function_definition_logical_side_start_end(self, *, to, prefix, edge, setter_argument_type, left, right, top, bottom):
        to.write(f"void ComputedStyleProperties::set{prefix.id_without_prefix}{edge}({setter_argument_type} value)")
        to.write(f"{{")
        with to.indent():
            to.write(f"if (writingMode().isHorizontal()) {{")
            with to.indent():
                to.write(f"if (writingMode().isInlineLeftToRight())")
                with to.indent():
                    to.write(f"{left.codegen_properties.computed_style_setter}(WTF::move(value));")
                to.write(f"else")
                with to.indent():
                    to.write(f"{right.codegen_properties.computed_style_setter}(WTF::move(value));")
            to.write(f"}} else {{")
            with to.indent():
                to.write(f"if (writingMode().isInlineTopToBottom())")
                with to.indent():
                    to.write(f"{top.codegen_properties.computed_style_setter}(WTF::move(value));")
                to.write(f"else")
                with to.indent():
                    to.write(f"{bottom.codegen_properties.computed_style_setter}(WTF::move(value));")
            to.write(f"}}")
        to.write(f"}}")
        to.newline()

    def _generate_setters_inlines_function_definition_logical_side_before_after(self, *, to, prefix, edge, setter_argument_type, left, right, top, bottom):
        to.write(f"void ComputedStyleProperties::set{prefix.id_without_prefix}{edge}({setter_argument_type} value)")
        to.write(f"{{")
        with to.indent():
            to.write(f"switch (writingMode().blockDirection()) {{")
            to.write(f"case FlowDirection::LeftToRight:")
            with to.indent():
                to.write(f"return {left.codegen_properties.computed_style_setter}(WTF::move(value));")
            to.write(f"case FlowDirection::RightToLeft:")
            with to.indent():
                to.write(f"return {right.codegen_properties.computed_style_setter}(WTF::move(value));")
            to.write(f"case FlowDirection::TopToBottom:")
            with to.indent():
                to.write(f"return {top.codegen_properties.computed_style_setter}(WTF::move(value));")
            to.write(f"case FlowDirection::BottomToTop:")
            with to.indent():
                to.write(f"return {bottom.codegen_properties.computed_style_setter}(WTF::move(value));")
            to.write(f"}}")
        to.write(f"}}")
        to.newline()

    def _generate_setters_inlines_function_definition_logical_side_left_right(self, *, to, prefix, edge, setter_argument_type, left, right):
        to.write(f"void ComputedStyleProperties::set{prefix.id_without_prefix}{edge}({setter_argument_type} value)")
        to.write(f"{{")
        with to.indent():
            to.write(f"if (writingMode().isHorizontal())")
            with to.indent():
                to.write(f"{left.codegen_properties.computed_style_setter}(WTF::move(value));")
            to.write(f"else")
            with to.indent():
                to.write(f"{right.codegen_properties.computed_style_setter}(WTF::move(value));")
        to.write(f"}}")
        to.newline()

    def _generate_setters_inlines_logical_property_function_definitions(self, *, to):
        for property_group_name, property_group in self.style_properties.logical_property_groups.items():
            if property_group['kind'] == 'axis':
                horizontal = property_group['physical']['horizontal']
                vertical = property_group['physical']['vertical']

                # NOTE: Choice of which property we use here is arbitrary, they must always have the same types.
                setter_argument_type = horizontal.setter_argument_type

                self._generate_setters_inlines_function_definition_logical_axis(
                    to=to,
                    axis=horizontal,
                    setter_argument_type=setter_argument_type,
                    horizontal=horizontal,
                    vertical=vertical
                )
                self._generate_setters_inlines_function_definition_logical_axis(
                    to=to,
                    axis=vertical,
                    setter_argument_type=setter_argument_type,
                    horizontal=vertical,
                    vertical=horizontal
                )

            elif property_group['kind'] == 'side':
                left = property_group['physical']['left']
                right = property_group['physical']['right']
                bottom = property_group['physical']['bottom']
                top = property_group['physical']['top']

                # NOTE: Choice of which property we use here is arbitrary, they must always have the same types.
                setter_argument_type = left.setter_argument_type

                prefix = Name(property_group_name)

                self._generate_setters_inlines_function_definition_logical_side_start_end(
                    to=to,
                    prefix=prefix,
                    edge='Start',
                    setter_argument_type=setter_argument_type,
                    left=left,
                    right=right,
                    top=top,
                    bottom=bottom
                )
                self._generate_setters_inlines_function_definition_logical_side_start_end(
                    to=to,
                    prefix=prefix,
                    edge='End',
                    setter_argument_type=setter_argument_type,
                    left=right,
                    right=left,
                    top=bottom,
                    bottom=top
                )
                self._generate_setters_inlines_function_definition_logical_side_before_after(
                    to=to,
                    prefix=prefix,
                    edge='Before',
                    setter_argument_type=setter_argument_type,
                    left=left,
                    right=right,
                    top=top,
                    bottom=bottom
                )
                self._generate_setters_inlines_function_definition_logical_side_before_after(
                    to=to,
                    prefix=prefix,
                    edge='After',
                    setter_argument_type=setter_argument_type,
                    left=right,
                    right=left,
                    top=bottom,
                    bottom=top
                )
                self._generate_setters_inlines_function_definition_logical_side_left_right(
                    to=to,
                    prefix=prefix,
                    edge='LogicalLeft',
                    setter_argument_type=setter_argument_type,
                    left=left,
                    right=top
                )
                self._generate_setters_inlines_function_definition_logical_side_left_right(
                    to=to,
                    prefix=prefix,
                    edge='LogicalRight',
                    setter_argument_type=setter_argument_type,
                    left=right,
                    right=bottom
                )

    def generate_style_computed_style_properties_setters_inlines_h(self):
        with open('StyleComputedStyleProperties+SettersInlines.h', 'w') as output_file:
            writer = Writer(output_file)

            self.generation_context.generate_heading(
                to=writer
            )

            self.generation_context.generate_required_header_pragma(
                to=writer
            )

            writer.write("#ifndef COMPUTED_STYLE_PROPERTIES_SETTERS_INLINES_INCLUDE_TRAP")
            writer.write("#error \"Please do not include this file anywhere except from StyleComputedStyle+SettersInlines.h.\"")
            writer.write("#endif")
            writer.newline()

            self.generation_context.generate_includes(
                to=writer,
                headers=[
                    "StyleComputedStyleProperties+SettersCustomInlines.h",
                ]
            )

            with self.generation_context.namespaces(["WebCore", "Style"], to=writer):
                self._generate_setters_inlines_property_function_definitions(
                    to=writer
                )
                self._generate_setters_inlines_logical_property_function_definitions(
                    to=writer
                )

    # Generate StyleComputedStyleProperties+InitialInlines.h

    def _generate_initial_inlines_function_definition(self, *, to, function_name, function_specifiers, return_type, initial_expression, requires_using_namespace_css_literals):
        to.write(f"{''.join(map(lambda x: x + ' ', function_specifiers))}{return_type} ComputedStyleProperties::{function_name}()")
        to.write(f"{{")
        with to.indent():
            if requires_using_namespace_css_literals:
                to.write(f"using namespace CSS::Literals;")
            to.write(f"return {initial_expression};")
        to.write(f"}}")
        to.newline()

    def _generate_initial_inlines_property_function_definitions(self, *, to):
        for property in self.style_properties.all:
            if property.codegen_properties.skip_computed_style:
                continue
            if property.codegen_properties.skip_computed_style_initial:
                continue
            if property.codegen_properties.is_logical:
                continue
            if property.codegen_properties.longhands:
                continue
            if property.codegen_properties.cascade_alias:
                continue
            if property.codegen_properties.coordinated_value_list_property:
                continue

            if not property.codegen_properties.computed_style_initial_custom:
                function_name = property.codegen_properties.computed_style_initial
                function_specifiers = property.initial_definition_function_specifiers
                return_type = property.initial_return_type
                requires_using_namespace_css_literals = property.initial.requires_using_namespace_css_literals

                self._generate_initial_inlines_function_definition(
                    to=to,
                    function_name=function_name,
                    function_specifiers=function_specifiers,
                    return_type=return_type,
                    initial_expression=self._compute_initial_expression(property),
                    requires_using_namespace_css_literals=requires_using_namespace_css_literals
                )

    def generate_style_computed_style_properties_initial_inlines_h(self):
        with open('StyleComputedStyleProperties+InitialInlines.h', 'w') as output_file:
            writer = Writer(output_file)

            self.generation_context.generate_heading(
                to=writer
            )

            self.generation_context.generate_required_header_pragma(
                to=writer
            )

            writer.write("#ifndef COMPUTED_STYLE_PROPERTIES_INITIAL_INLINES_INCLUDE_TRAP")
            writer.write("#error \"Please do not include this file anywhere except from StyleComputedStyle+InitialInlines.h.\"")
            writer.write("#endif")
            writer.newline()

            self.generation_context.generate_includes(
                to=writer,
                system_headers=[
                    "<WebCore/StyleComputedStyleProperties+InitialCustomInlines.h>",
                ]
            )

            with self.generation_context.namespaces(["WebCore", "Style"], to=writer):
                self._generate_initial_inlines_property_function_definitions(
                    to=writer
                )


# Generates `RenderStyleProperties.h`, `RenderStyleProperties.cpp`, `RenderStyleProperties+ConstructionInlines.h`, `RenderStyleProperties+GettersInlines.h` and `RenderStyleProperties+SettersInlines.h`.
class GenerateRenderStyleProperties:
    def __init__(self, generation_context):
        self.generation_context = generation_context

    @property
    def properties_and_descriptors(self):
        return self.generation_context.properties_and_descriptors

    @property
    def style_properties(self):
        return self.generation_context.properties_and_descriptors.style_properties

    def generate(self):
        self.generate_render_style_properties_h()
        self.generate_render_style_properties_cpp()
        self.generate_render_style_properties_construction_inlines_h()
        self.generate_render_style_properties_getters_inlines_h()
        self.generate_render_style_properties_setters_inlines_h()

    def _compute_forwarding_expression(self, storage_kind, argument_name):
        if storage_kind == 'reference':
            return f"WTF::move({argument_name})"
        else:
            return f"{argument_name}"

    # Computes the function specifiers, if any, of the getter's declaration.
    def _compute_getter_declaration_function_specifiers(self, property):
        function_specifiers = ['inline']
        if property.codegen_properties.computed_style_getter_constexpr:
            function_specifiers += ['constexpr']
        return function_specifiers

    # Computes the function specifiers, if any, of the getter's definition.
    def _compute_getter_definition_function_specifiers(self, property):
        function_specifiers = ['inline']
        if property.codegen_properties.computed_style_getter_constexpr:
            function_specifiers += ['constexpr']
        return function_specifiers

    # Computes the function specifiers, if any, of the setter's declaration.
    def _compute_setter_declaration_function_specifiers(self, property):
        function_specifiers = ['inline']
        if property.codegen_properties.computed_style_setter_constexpr:
            function_specifiers += ['constexpr']
        return function_specifiers

    # Computes the function specifiers, if any, of the setter's definition.
    def _compute_setter_definition_function_specifiers(self, property):
        function_specifiers = ['inline']
        if property.codegen_properties.computed_style_setter_constexpr:
            function_specifiers += ['constexpr']
        return function_specifiers

    # Generate RenderStyleProperties.h

    def _generate_property_function_declarations(self, *, to):
        for property in self.style_properties.all:
            if property.codegen_properties.skip_render_style:
                continue
            if property.codegen_properties.is_logical:
                continue
            if property.codegen_properties.longhands:
                continue
            if property.codegen_properties.cascade_alias:
                continue
            if property.codegen_properties.coordinated_value_list_property:
                continue

            to.write(f"// '{property}'")

            if not property.codegen_properties.skip_render_style_getter:
                getter_name = property.codegen_properties.computed_style_getter
                getter_function_specifiers = self._compute_getter_declaration_function_specifiers(property)
                getter_return_type = property.getter_return_type

                self.generation_context.generate_function_declaration(
                    to=to,
                    function_name=getter_name,
                    function_specifiers=getter_function_specifiers,
                    return_type=getter_return_type,
                    function_qualifiers=['const']
                )

            if not property.codegen_properties.skip_render_style_setter:
                setter_name = property.codegen_properties.computed_style_setter
                setter_function_specifiers = self._compute_setter_declaration_function_specifiers(property)
                setter_return_type = property.setter_return_type
                setter_argument_type = property.setter_argument_type

                self.generation_context.generate_function_declaration(
                    to=to,
                    function_name=setter_name,
                    function_specifiers=setter_function_specifiers,
                    return_type=setter_return_type,
                    argument_types=[setter_argument_type]
                )

            if property.codegen_properties.color_property:
                if property.codegen_properties.computed_style_visited_link_storage_path:
                    getter_name = f"visitedLink{property.codegen_properties.computed_style_name_for_methods}"
                    setter_name = f"setVisitedLink{property.codegen_properties.computed_style_name_for_methods}"
                    getter_function_specifiers = ['inline']
                    getter_return_type = property.getter_return_type
                    setter_function_specifiers = ['inline']
                    setter_return_type = f"void"
                    setter_argument_type = property.setter_argument_type

                    self.generation_context.generate_function_declaration(
                        to=to,
                        function_name=getter_name,
                        function_specifiers=getter_function_specifiers,
                        return_type=getter_return_type,
                        function_qualifiers=['const']
                    )
                    self.generation_context.generate_function_declaration(
                        to=to,
                        function_name=setter_name,
                        function_specifiers=setter_function_specifiers,
                        return_type=setter_return_type,
                        argument_types=[setter_argument_type]
                    )

                self.generation_context.generate_function_declaration(
                    to=to,
                    function_name=f"{property.codegen_properties.computed_style_getter}Resolver",
                    function_specifiers=['inline'],
                    return_type="decltype(auto)",
                    function_qualifiers=['const']
                )

                self.generation_context.generate_function_declaration(
                    to=to,
                    function_name=f"{property.codegen_properties.computed_style_getter}ResolvingCurrentColor",
                    function_specifiers=['inline'],
                    return_type='WebCore::Color',
                    function_qualifiers=['const']
                )
                self.generation_context.generate_function_declaration(
                    to=to,
                    function_name=f"{property.codegen_properties.computed_style_getter}ResolvingCurrentColorApplyingColorFilter",
                    function_specifiers=['inline'],
                    return_type='WebCore::Color',
                    function_qualifiers=['const']
                )

                if property.codegen_properties.computed_style_visited_link_storage_path:
                    self.generation_context.generate_function_declaration(
                        to=to,
                        function_name=f"visitedLink{property.codegen_properties.computed_style_name_for_methods}ResolvingCurrentColor",
                        function_specifiers=['inline'],
                        return_type='WebCore::Color',
                        function_qualifiers=['const']
                    )
                    self.generation_context.generate_function_declaration(
                        to=to,
                        function_name=f"visitedLink{property.codegen_properties.computed_style_name_for_methods}ResolvingCurrentColorApplyingColorFilter",
                        function_specifiers=['inline'],
                        return_type='WebCore::Color',
                        function_qualifiers=['const']
                    )

                self.generation_context.generate_function_declaration(
                    to=to,
                    function_name=f"visitedDependent{property.codegen_properties.computed_style_name_for_methods}",
                    function_specifiers=['inline'],
                    return_type='WebCore::Color',
                    argument_types=['OptionSet<PaintBehavior> = { }'],
                    function_qualifiers=['const']
                )
                self.generation_context.generate_function_declaration(
                    to=to,
                    function_name=f"visitedDependent{property.codegen_properties.computed_style_name_for_methods}ApplyingColorFilter",
                    function_specifiers=['inline'],
                    return_type='WebCore::Color',
                    argument_types=['OptionSet<PaintBehavior> = { }'],
                    function_qualifiers=['const']
                )

            if property.codegen_properties.computed_style_has_explicitly_set_storage_path:
                getter_name = f"hasExplicitlySet{property.codegen_properties.computed_style_name_for_methods}"
                setter_name = f"setHasExplicitlySet{property.codegen_properties.computed_style_name_for_methods}"
                getter_function_specifiers = ['inline']
                getter_return_type = 'bool'
                setter_function_specifiers = ['inline']
                setter_return_type = f"void"
                setter_argument_type = 'bool'

                self.generation_context.generate_function_declaration(
                    to=to,
                    function_name=getter_name,
                    function_specifiers=getter_function_specifiers,
                    return_type=getter_return_type,
                    function_qualifiers=['const']
                )
                self.generation_context.generate_function_declaration(
                    to=to,
                    function_name=setter_name,
                    function_specifiers=setter_function_specifiers,
                    return_type=setter_return_type,
                    argument_types=[setter_argument_type]
                )
            to.newline()

    def _generate_logical_property_function_declarations(self, *, to):
        for property_group_name, property_group in self.style_properties.logical_property_groups.items():
            to.write(f"// Logical getters and setters for '{property_group_name}' properties of type '{property_group['kind']}'.")
            if property_group['kind'] == 'axis':
                horizontal_property = property_group['physical']['horizontal']
                vertical_property = property_group['physical']['vertical']

                if horizontal_property.codegen_properties.skip_render_style:
                    continue

                # NOTE: Choice of which property we use here is arbitrary, they must always have the same types.
                getter_return_type = horizontal_property.getter_return_type
                setter_argument_type = horizontal_property.setter_argument_type

                if not horizontal_property.codegen_properties.skip_render_style_getter:
                    for property in [horizontal_property, vertical_property]:
                        to.write(f"inline {getter_return_type} logical{property.id_without_prefix}(WritingMode) const;")
                    for property in [horizontal_property, vertical_property]:
                        to.write(f"inline {getter_return_type} logical{property.id_without_prefix}() const;")
                if not horizontal_property.codegen_properties.skip_render_style_setter:
                    for property in [horizontal_property, vertical_property]:
                        to.write(f"inline void setLogical{property.id_without_prefix}({setter_argument_type});")
            elif property_group['kind'] == 'side':
                property = property_group['physical']['bottom']
                # NOTE: Choice of which property we use here is arbitrary, they must always have the same types.
                getter_return_type = property.getter_return_type
                setter_argument_type = property.setter_argument_type

                prefix = Name(property_group_name)

                if not property.codegen_properties.skip_render_style_getter:
                    for edge in ['Start', 'End', 'Before', 'After', 'LogicalLeft', 'LogicalRight']:
                        to.write(f"inline {getter_return_type} {prefix.id_without_prefix_with_lowercase_first_letter}{edge}(WritingMode) const;")
                    for edge in ['Start', 'End', 'Before', 'After', 'LogicalLeft', 'LogicalRight']:
                        to.write(f"inline {getter_return_type} {prefix.id_without_prefix_with_lowercase_first_letter}{edge}() const;")
                if not property.codegen_properties.skip_render_style_setter:
                    for edge in ['Start', 'End', 'Before', 'After', 'LogicalLeft', 'LogicalRight']:
                        to.write(f"inline void set{prefix.id_without_prefix}{edge}({setter_argument_type});")
            else:
                # FIXME: Add logical getters / setters for other group kinds (like 'corner') if it would be useful.
                to.write(f"// FIXME: Add support for logical getter/setters of kind '{property_group['kind']}'.")
            to.newline()

    def _generate_checked_ptr_delegation(self, *, to):
        to.write(f"// Delegation to `Style::ComputedStyle` for `CheckedPtr` support.")
        to.write(f"ALWAYS_INLINE uint32_t checkedPtrCount() const {{ return m_computedStyle.checkedPtrCount(); }}")
        to.write(f"ALWAYS_INLINE void incrementCheckedPtrCount() const {{ m_computedStyle.incrementCheckedPtrCount(); }}")
        to.write(f"ALWAYS_INLINE void decrementCheckedPtrCount() const {{ m_computedStyle.decrementCheckedPtrCount(); }}")
        to.write(f"ALWAYS_INLINE uint32_t checkedPtrCountWithoutThreadCheck() const {{ return m_computedStyle.checkedPtrCountWithoutThreadCheck(); }}")
        to.write(f"ALWAYS_INLINE void setDidBeginCheckedPtrDeletion() {{ m_computedStyle.setDidBeginCheckedPtrDeletion(); }}")
        to.newline()

    def generate_render_style_properties_h(self):
        with open('RenderStyleProperties.h', 'w') as output_file:
            writer = Writer(output_file)

            self.generation_context.generate_heading(
                to=writer
            )

            self.generation_context.generate_required_header_pragma(
                to=writer
            )

            self.generation_context.generate_includes(
                to=writer,
                system_headers=[
                    "<WebCore/StyleComputedStyle.h>",
                ]
            )

            with self.generation_context.namespace("WebCore", to=writer):
                writer.write(f"DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(RenderStyleProperties);")
                writer.write(f"class RenderStyleProperties {{")
                with writer.indent():
                    writer.write(f"WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(RenderStyleProperties, RenderStyleProperties);")
                    writer.write(f"WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RenderStyleProperties);")
                writer.write(f"public:")

                with writer.indent():
                    writer.write(f"~RenderStyleProperties() = default;")
                    writer.newline()

                    self._generate_checked_ptr_delegation(
                        to=writer
                    )

                    self._generate_property_function_declarations(
                        to=writer
                    )
                    self._generate_logical_property_function_declarations(
                        to=writer
                    )

                writer.write(f"protected:")

                with writer.indent():
                    writer.write(f"friend class RenderStyle;")
                    writer.write(f"friend class Style::DifferenceFunctions;")
                    writer.newline()

                    writer.write(f"enum CloneTag {{ Clone }};")
                    writer.write(f"enum CreateDefaultStyleTag {{ CreateDefaultStyle }};")
                    writer.newline()

                    writer.write(f"RenderStyleProperties(RenderStyleProperties&&);")
                    writer.write(f"RenderStyleProperties& operator=(RenderStyleProperties&&);")
                    writer.newline()

                    writer.write(f"RenderStyleProperties(CreateDefaultStyleTag);")
                    writer.write(f"RenderStyleProperties(const RenderStyleProperties&, CloneTag);")
                    writer.newline()

                    writer.write(f"RenderStyleProperties(RenderStyleProperties&, RenderStyleProperties&&);")
                    writer.newline()

                    writer.write(f"Style::ComputedStyle m_computedStyle;")
                writer.write(f"}};")
                writer.newline()

    # Generate RenderStyleProperties.cpp

    def generate_render_style_properties_cpp(self):
        with open('RenderStyleProperties.cpp', 'w') as output_file:
            writer = Writer(output_file)

            writer = Writer(output_file)

            self.generation_context.generate_heading(
                to=writer
            )

            self.generation_context.generate_cpp_required_includes(
                to=writer,
                header="RenderStyleProperties.h"
            )

            with self.generation_context.namespace("WebCore", to=writer):
                writer.write(f"DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(RenderStyleProperties);")
                writer.newline()

    # Generate RenderStyleProperties+ConstructionInlines.h

    def generate_render_style_properties_construction_inlines_h(self):
        with open('RenderStyleProperties+ConstructionInlines.h', 'w') as output_file:
            writer = Writer(output_file)

            self.generation_context.generate_heading(
                to=writer
            )

            self.generation_context.generate_required_header_pragma(
                to=writer
            )

            self.generation_context.generate_includes(
                to=writer,
                headers=[
                    "RenderStyleProperties.h",
                    "StyleComputedStyle+ConstructionInlines.h",
                ]
            )

            with self.generation_context.namespace("WebCore", to=writer):
                writer.write(f"inline RenderStyleProperties::RenderStyleProperties(RenderStyleProperties&&) = default;")
                writer.write(f"inline RenderStyleProperties& RenderStyleProperties::operator=(RenderStyleProperties&&) = default;")
                writer.newline()

                writer.write(f"inline RenderStyleProperties::RenderStyleProperties(CreateDefaultStyleTag)")
                with writer.indent():
                    writer.write(f": m_computedStyle(Style::ComputedStyle::CreateDefaultStyle)")
                writer.write(f"{{")
                writer.write(f"}}")
                writer.newline()

                writer.write(f"inline RenderStyleProperties::RenderStyleProperties(const RenderStyleProperties& other, CloneTag)")
                with writer.indent():
                    writer.write(f": m_computedStyle(other.m_computedStyle, Style::ComputedStyle::Clone)")
                writer.write(f"{{")
                writer.write(f"}}")
                writer.newline()

                writer.write(f"inline RenderStyleProperties::RenderStyleProperties(RenderStyleProperties& a, RenderStyleProperties&& b)")
                with writer.indent():
                    writer.write(f": m_computedStyle(a.m_computedStyle, WTF::move(b.m_computedStyle))")
                writer.write(f"{{")
                writer.write(f"}}")
                writer.newline()

    # Generate RenderStyleProperties+GettersInlines.h

    def _generate_getters_inlines_function_definition(self, *, to, function_name, function_specifiers, return_type, arguments=[]):
        to.write(f"{''.join(map(lambda x: x + ' ', function_specifiers))}{return_type} RenderStyleProperties::{function_name}({', '.join(map(lambda x: x['type'] + ' ' + x['name'], arguments))}) const")
        to.write(f"{{")
        with to.indent():
            to.write(f"return m_computedStyle.{function_name}({', '.join(map(lambda x: x['name'], arguments))});")
        to.write(f"}}")
        to.newline()

    def _generate_getters_inlines_property_function_definitions(self, *, to):
        for property in self.style_properties.all:
            if property.codegen_properties.skip_render_style:
                continue
            if property.codegen_properties.is_logical:
                continue
            if property.codegen_properties.longhands:
                continue
            if property.codegen_properties.cascade_alias:
                continue
            if property.codegen_properties.coordinated_value_list_property:
                continue

            if not property.codegen_properties.computed_style_storage_path and not property.codegen_properties.skip_render_style_getter:
                raise Exception(f"Missing ComputedStyle storage path for property {property.id}.")

            if not property.codegen_properties.skip_render_style_getter:
                self._generate_getters_inlines_function_definition(
                    to=to,
                    function_name=property.codegen_properties.computed_style_getter,
                    function_specifiers=self._compute_getter_definition_function_specifiers(property),
                    return_type=property.getter_return_type
                )

            if property.codegen_properties.color_property:
                if property.codegen_properties.computed_style_visited_link_storage_path:
                    self._generate_getters_inlines_function_definition(
                        to=to,
                        function_name=f"visitedLink{property.codegen_properties.computed_style_name_for_methods}",
                        function_specifiers=['inline'],
                        return_type=property.getter_return_type
                    )

                self._generate_getters_inlines_function_definition(
                    to=to,
                    function_name=f"{property.codegen_properties.computed_style_getter}Resolver",
                    function_specifiers=['inline'],
                    return_type="decltype(auto)"
                )

                self._generate_getters_inlines_function_definition(
                    to=to,
                    function_name=f"{property.codegen_properties.computed_style_getter}ResolvingCurrentColor",
                    function_specifiers=['inline'],
                    return_type='WebCore::Color'
                )
                self._generate_getters_inlines_function_definition(
                    to=to,
                    function_name=f"{property.codegen_properties.computed_style_getter}ResolvingCurrentColorApplyingColorFilter",
                    function_specifiers=['inline'],
                    return_type='WebCore::Color'
                )

                if property.codegen_properties.computed_style_visited_link_storage_path:
                    self._generate_getters_inlines_function_definition(
                        to=to,
                        function_name=f"visitedLink{property.codegen_properties.computed_style_name_for_methods}ResolvingCurrentColor",
                        function_specifiers=['inline'],
                        return_type='WebCore::Color'
                    )
                    self._generate_getters_inlines_function_definition(
                        to=to,
                        function_name=f"visitedLink{property.codegen_properties.computed_style_name_for_methods}ResolvingCurrentColorApplyingColorFilter",
                        function_specifiers=['inline'],
                        return_type='WebCore::Color'
                    )

                self._generate_getters_inlines_function_definition(
                    to=to,
                    function_name=f"visitedDependent{property.codegen_properties.computed_style_name_for_methods}",
                    function_specifiers=['inline'],
                    return_type='WebCore::Color',
                    arguments=[{'type': 'OptionSet<PaintBehavior>', 'name': 'paintBehavior'}]
                )
                self._generate_getters_inlines_function_definition(
                    to=to,
                    function_name=f"visitedDependent{property.codegen_properties.computed_style_name_for_methods}ApplyingColorFilter",
                    function_specifiers=['inline'],
                    return_type='WebCore::Color',
                    arguments=[{'type': 'OptionSet<PaintBehavior>', 'name': 'paintBehavior'}]
                )

            if property.codegen_properties.computed_style_has_explicitly_set_storage_path:
                self._generate_getters_inlines_function_definition(
                    to=to,
                    function_name=f"hasExplicitlySet{property.codegen_properties.computed_style_name_for_methods}",
                    function_specifiers=['inline'],
                    return_type='bool'
                )

    def _generate_getters_inlines_function_definition_taking_writing_mode(self, *, to, function_name, function_specifiers, return_type):
        to.write(f"{''.join(map(lambda x: x + ' ', function_specifiers))}{return_type} RenderStyleProperties::{function_name}(WritingMode writingMode) const")
        to.write(f"{{")
        with to.indent():
            to.write(f"return m_computedStyle.{function_name}(writingMode);")
        to.write(f"}}")
        to.newline()

    def _generate_getters_inlines_logical_property_function_definitions(self, *, to):
        for property_group_name, property_group in self.style_properties.logical_property_groups.items():
            if property_group['kind'] == 'axis':
                horizontal_property = property_group['physical']['horizontal']
                vertical_property = property_group['physical']['vertical']

                if horizontal_property.codegen_properties.skip_render_style or horizontal_property.codegen_properties.skip_render_style_getter:
                    continue

                # NOTE: Choice of which property we use here is arbitrary, they must always have the same types.
                function_specifiers = ['inline']
                return_type = horizontal_property.getter_return_type

                for property in [horizontal_property, vertical_property]:
                    self._generate_getters_inlines_function_definition_taking_writing_mode(
                        to=to,
                        function_name=f"logical{property.id_without_prefix}",
                        function_specifiers=function_specifiers,
                        return_type=return_type
                    )
                for property in [horizontal_property, vertical_property]:
                    self._generate_getters_inlines_function_definition(
                        to=to,
                        function_name=f"logical{property.id_without_prefix}",
                        function_specifiers=function_specifiers,
                        return_type=return_type
                    )
            elif property_group['kind'] == 'side':
                property = property_group['physical']['bottom']

                if property.codegen_properties.skip_render_style or property.codegen_properties.skip_render_style_getter:
                    continue

                # NOTE: Choice of which property we use here is arbitrary, they must always have the same types.
                function_specifiers = ['inline']
                return_type = property.getter_return_type

                prefix = Name(property_group_name)

                for edge in ['Start', 'End', 'Before', 'After', 'LogicalLeft', 'LogicalRight']:
                    self._generate_getters_inlines_function_definition_taking_writing_mode(
                        to=to,
                        function_name=f"{prefix.id_without_prefix_with_lowercase_first_letter}{edge}",
                        function_specifiers=function_specifiers,
                        return_type=return_type
                    )
                for edge in ['Start', 'End', 'Before', 'After', 'LogicalLeft', 'LogicalRight']:
                    self._generate_getters_inlines_function_definition(
                        to=to,
                        function_name=f"{prefix.id_without_prefix_with_lowercase_first_letter}{edge}",
                        function_specifiers=function_specifiers,
                        return_type=return_type
                    )

    def generate_render_style_properties_getters_inlines_h(self):
        with open('RenderStyleProperties+GettersInlines.h', 'w') as output_file:
            writer = Writer(output_file)

            self.generation_context.generate_heading(
                to=writer
            )

            self.generation_context.generate_required_header_pragma(
                to=writer
            )

            writer.write("#ifndef RENDER_STYLE_PROPERTIES_GETTERS_INLINES_INCLUDE_TRAP")
            writer.write("#error \"Please do not include this file anywhere except from RenderStyle+GettersInlines.h.\"")
            writer.write("#endif")
            writer.newline()

            self.generation_context.generate_includes(
                to=writer,
                system_headers=[
                    "<WebCore/StyleComputedStyle+GettersInlines.h>",
                ]
            )

            with self.generation_context.namespace("WebCore", to=writer):
                self._generate_getters_inlines_property_function_definitions(
                    to=writer
                )
                self._generate_getters_inlines_logical_property_function_definitions(
                    to=writer
                )

    # Generate RenderStyleProperties+SettersInlines.h

    def _generate_setters_inlines_function_definition(self, *, to, function_name, function_specifiers, return_type, argument_type, argument_name, forwarding_expression):
        to.write(f"{''.join(map(lambda x: x + ' ', function_specifiers))}{return_type} RenderStyleProperties::{function_name}({argument_type} {argument_name})")
        to.write(f"{{")
        with to.indent():
            if return_type == 'void':
                to.write(f"m_computedStyle.{function_name}({forwarding_expression});")
            else:
                to.write(f"return m_computedStyle.{function_name}({forwarding_expression});")
        to.write(f"}}")
        to.newline()

    def _generate_setters_inlines_property_function_definitions(self, *, to):
        for property in self.style_properties.all:
            if property.codegen_properties.skip_render_style:
                continue
            if property.codegen_properties.skip_render_style_setter:
                continue
            if property.codegen_properties.is_logical:
                continue
            if property.codegen_properties.longhands:
                continue
            if property.codegen_properties.cascade_alias:
                continue
            if property.codegen_properties.coordinated_value_list_property:
                continue

            if not property.codegen_properties.computed_style_storage_path:
                raise Exception(f"Missing ComputedStyle storage path for property {property.id}.")

            function_name = property.codegen_properties.computed_style_setter
            function_specifiers = self._compute_setter_definition_function_specifiers(property)
            return_type = property.setter_return_type
            argument_type = property.setter_argument_type
            argument_name = "value"
            storage_kind = property.codegen_properties.computed_style_storage_kind

            self._generate_setters_inlines_function_definition(
                to=to,
                function_name=function_name,
                function_specifiers=function_specifiers,
                return_type=return_type,
                argument_type=argument_type,
                argument_name=argument_name,
                forwarding_expression=self._compute_forwarding_expression(storage_kind, argument_name)
            )

            if property.codegen_properties.computed_style_visited_link_storage_path:
                function_name = f"setVisitedLink{property.codegen_properties.computed_style_name_for_methods}"
                function_specifiers = ['inline']
                return_type = "void"
                argument_type = property.setter_argument_type
                argument_name = "value"
                storage_kind = property.codegen_properties.computed_style_storage_kind  # the storage kind for visited links are always the same as the principle value

                self._generate_setters_inlines_function_definition(
                    to=to,
                    function_name=function_name,
                    function_specifiers=function_specifiers,
                    return_type=return_type,
                    argument_type=argument_type,
                    argument_name=argument_name,
                    forwarding_expression=self._compute_forwarding_expression(storage_kind, argument_name)
                )

            if property.codegen_properties.computed_style_has_explicitly_set_storage_path:
                function_name = f"setHasExplicitlySet{property.codegen_properties.computed_style_name_for_methods}"
                function_specifiers = ['inline']
                return_type = "void"
                argument_type = 'bool'
                argument_name = "value"
                storage_kind = 'value'

                self._generate_setters_inlines_function_definition(
                    to=to,
                    function_name=function_name,
                    function_specifiers=function_specifiers,
                    return_type=return_type,
                    argument_type=argument_type,
                    argument_name=argument_name,
                    forwarding_expression=self._compute_forwarding_expression(storage_kind, argument_name)
                )

    def _generate_setters_inlines_logical_property_function_definitions(self, *, to):
        for property_group_name, property_group in self.style_properties.logical_property_groups.items():
            if property_group['kind'] == 'axis':
                horizontal_property = property_group['physical']['horizontal']
                vertical_property = property_group['physical']['vertical']

                if horizontal_property.codegen_properties.skip_render_style or horizontal_property.codegen_properties.skip_render_style_setter:
                    continue

                # NOTE: Choice of which property we use here is arbitrary, they must always have the same types.
                function_specifiers = ['inline']
                argument_type = horizontal_property.setter_argument_type
                argument_name = "value"
                storage_kind = horizontal_property.codegen_properties.computed_style_storage_kind

                for property in [horizontal_property, vertical_property]:
                    self._generate_setters_inlines_function_definition(
                        to=to,
                        function_name=f"setLogical{property.id_without_prefix}",
                        function_specifiers=function_specifiers,
                        return_type="void",
                        argument_type=argument_type,
                        argument_name=argument_name,
                        forwarding_expression=self._compute_forwarding_expression(storage_kind, argument_name)
                    )
            elif property_group['kind'] == 'side':
                property = property_group['physical']['bottom']

                if property.codegen_properties.skip_render_style or property.codegen_properties.skip_render_style_getter:
                    continue

                # NOTE: Choice of which property we use here is arbitrary, they must always have the same types.
                function_specifiers = ['inline']
                argument_type = property.setter_argument_type
                argument_name = "value"
                storage_kind = property.codegen_properties.computed_style_storage_kind

                prefix = Name(property_group_name)

                for edge in ['Start', 'End', 'Before', 'After', 'LogicalLeft', 'LogicalRight']:
                    self._generate_setters_inlines_function_definition(
                        to=to,
                        function_name=f"set{prefix.id_without_prefix}{edge}",
                        function_specifiers=function_specifiers,
                        return_type="void",
                        argument_type=argument_type,
                        argument_name=argument_name,
                        forwarding_expression=self._compute_forwarding_expression(storage_kind, argument_name)
                    )

    def generate_render_style_properties_setters_inlines_h(self):
        with open('RenderStyleProperties+SettersInlines.h', 'w') as output_file:
            writer = Writer(output_file)

            self.generation_context.generate_heading(
                to=writer
            )

            self.generation_context.generate_required_header_pragma(
                to=writer
            )

            writer.write("#ifndef RENDER_STYLE_PROPERTIES_SETTERS_INLINES_INCLUDE_TRAP")
            writer.write("#error \"Please do not include this file anywhere except from RenderStyle+SettersInlines.h.\"")
            writer.write("#endif")
            writer.newline()

            self.generation_context.generate_includes(
                to=writer,
                headers=[
                    "StyleComputedStyle+SettersInlines.h",
                ]
            )

            with self.generation_context.namespace("WebCore", to=writer):
                self._generate_setters_inlines_property_function_definitions(
                    to=writer
                )
                self._generate_setters_inlines_logical_property_function_definitions(
                    to=writer
                )


# Generates `StyleChangedAnimatablePropertiesGenerated.cpp`.
class GenerateStyleChangedAnimatablePropertiesGenerated:
    def __init__(self, generation_context):
        self.generation_context = generation_context

    @property
    def properties_and_descriptors(self):
        return self.generation_context.properties_and_descriptors

    @property
    def style_properties(self):
        return self.generation_context.properties_and_descriptors.style_properties

    def generate(self):
        self.generate_style_changed_animatable_properties_generated_cpp()

    def _generate_conservatively_collect_changed_animatable_properties_opaque(self, *, to, tree_node, generated_children, function_name):
        assert(tree_node.kind == 'opaque')
        assert(not tree_node.children)

        to.write(f"static void {function_name}(const auto&, const auto&, CSSPropertiesBitSet& changingProperties)")
        to.write(f"{{")

        with to.indent():
            animatable_properties = set()
            non_animatable_properties = set()

            for property in itertools.chain(tree_node.properties, tree_node.visited_link_properties):
                if not property.is_animatable:
                    non_animatable_properties.add(property)
                else:
                    animatable_properties.add(property)

            for property in sorted(animatable_properties, key=lambda x: x.id):
                to.write(f"changingProperties.m_properties.set({property.id_without_scope});")

            if non_animatable_properties:
                to.newline()
                to.write("// Non-animatable properties:")
                for property in sorted(non_animatable_properties, key=lambda x: x.id):
                    to.write(f"// SKIPPED '{property}' - {property.animation_type}")

        to.write(f"}}")
        to.newline()

    def _generate_conservatively_collect_changed_animatable_properties_function(self, *, to, tree_node, generated_children, function_name):
        # Special case `opaque` nodes, as they can only have very specific kinds of children.
        if tree_node.kind == 'opaque':
            self._generate_conservatively_collect_changed_animatable_properties_opaque(
                to=to,
                tree_node=tree_node,
                generated_children=generated_children,
                function_name=function_name
            )
            return

        to.write(f"static void {function_name}(const auto& a, const auto& b, CSSPropertiesBitSet& changingProperties)")
        to.write(f"{{")

        with to.indent():
            # Partition children into 'data', 'opaque', and 'struct', based on the what kind of container the child is.
            # Each of these have a slightly different dispatch needs.
            data_children = []
            opaque_children = []
            struct_children = []

            for child in sorted(generated_children, key=lambda x: x.name):
                if child.kind == 'data':
                    data_children.append(child)
                elif child.kind == 'opaque':
                    opaque_children.append(child)
                else:
                    struct_children.append(child)

            animatable_properties = []
            non_animatable_properties = []

            for property in sorted(tree_node.properties, key=lambda x: x.id):
                if not property.is_animatable:
                    non_animatable_properties.append(property)
                else:
                    animatable_properties.append(property)

            animatable_visited_link_properties = []
            non_animatable_visited_link_properties = []

            for property in sorted(tree_node.visited_link_properties, key=lambda x: x.id):
                if not property.is_animatable:
                    non_animatable_visited_link_properties.append(property)
                else:
                    animatable_visited_link_properties.append(property)

            # Generate the "data" children, which require an additional check.
            for child in data_children:
                child_function_name = Name(f"{function_name}_{child.name}").id_without_prefix_with_lowercase_first_letter
                child_value_getter =  f"{child.name}"

                # For "data" nodes, first check if both point to the same object.
                to.write(f"if (a.{child_value_getter}.ptr() != b.{child_value_getter}.ptr())")
                with to.indent():
                    to.write(f"{child_function_name}(*a.{child_value_getter}, *b.{child_value_getter}, changingProperties);")

            if data_children and opaque_children:
                to.newline()

            # Generate the "opaque" children, which also require an additional check.
            for child in opaque_children:
                child_function_name = Name(f"{function_name}_{child.name}").id_without_prefix_with_lowercase_first_letter
                child_value_getter =  f"{child.name}"

                to.write(f"if (a.{child_value_getter} != b.{child_value_getter})")
                with to.indent():
                    to.write(f"{child_function_name}(a.{child_value_getter}, b.{child_value_getter}, changingProperties);")

            if (data_children or opaque_children) and struct_children:
                to.newline()

            # Generate the "struct" children, which get called without any check.
            for child in struct_children:
                child_function_name = Name(f"{function_name}_{child.name}").id_without_prefix_with_lowercase_first_letter
                child_value_getter =  f"{child.name}"

                to.write(f"{child_function_name}(a.{child_value_getter}, b.{child_value_getter}, changingProperties);")

            if (data_children or opaque_children or struct_children) and animatable_properties:
                to.newline()

            # Generate the animatable property children.
            for property in animatable_properties:
                if property.codegen_properties.computed_style_changed_for_animation_custom:
                    to.write(f"ChangedAnimatablePropertiesCustom::conservativelyCollectChangedAnimatablePropertiesFor{property.id_without_prefix}(a, b, changingProperties);")
                    continue

                if tree_node.kind == 'data':
                    expression = f"{property.codegen_properties.computed_style_storage_name}"
                elif tree_node.kind == 'struct':
                    expression = f"{property.codegen_properties.computed_style_storage_name}"
                elif tree_node.kind == 'physical-group':
                    expression = f"{Name(property.codegen_properties.logical_property_group.resolver).id_without_prefix_with_lowercase_first_letter}()"

                to.write(f"if (a.{expression} != b.{expression})")
                with to.indent():
                    to.write(f"changingProperties.m_properties.set({property.id_without_scope});")

            if (data_children or opaque_children or struct_children or animatable_properties) and animatable_visited_link_properties:
                to.newline()

            # Generate the animatable visited link property children.
            for property in animatable_visited_link_properties:
                if property.codegen_properties.computed_style_changed_for_animation_custom:
                    to.write(f"ChangedAnimatablePropertiesCustom::conservativelyCollectChangedAnimatablePropertiesForVisitedLink{property.id_without_prefix}(a, b, changingProperties);")
                    continue

                if tree_node.kind == 'data':
                    expression = f"{property.codegen_properties.computed_style_visited_link_storage_name}"
                elif tree_node.kind == 'struct':
                    expression = f"{property.codegen_properties.computed_style_visited_link_storage_name}"
                elif tree_node.kind == 'physical-group':
                    expression = f"{Name(property.codegen_properties.logical_property_group.resolver).id_without_prefix_with_lowercase_first_letter}()"

                to.write(f"if (a.{expression} != b.{expression})")
                with to.indent():
                    to.write(f"changingProperties.m_properties.set({property.id_without_scope});")

            # Print out comments detailing the non-animatable properties that were skipped.
            if non_animatable_properties:
                to.newline()
                to.write("// Non-animatable properties:")
                for property in non_animatable_properties:
                    to.write(f"// SKIPPED '{property}' - {property.animation_type}")

            # Print out comments detailing the non-animatable visited link properties that were skipped.
            if non_animatable_visited_link_properties:
                to.newline()
                to.write("// Non-animatable visited link properties:")
                for property in non_animatable_visited_link_properties:
                    to.write(f"// SKIPPED '{property}' - {property.animation_type}")

        to.write(f"}}")
        to.newline()

    def _generate_conservatively_collect_changed_animatable_properties_recursive(self, *, to, tree_node, function_name):
        # First, recursively generate the collection functions for all child nodes, assuring they are callable.
        generated_children = []

        for child in tree_node.children.values():
            child_function_name = Name(f"{function_name}_{child.name}").id_without_prefix_with_lowercase_first_letter

            did_generate = self._generate_conservatively_collect_changed_animatable_properties_recursive(
                to=to,
                tree_node=child,
                function_name=child_function_name
            )

            if did_generate:
                generated_children.append(child)

        # If we didn't generate any callable children, and we don't have any animatable properties, there is nothing to generate here.
        if not generated_children and not any(property.is_animatable for property in tree_node.properties) and not any(property.is_animatable for property in tree_node.visited_link_properties):
            return False

        # Second, generate the function for this tree node.
        self._generate_conservatively_collect_changed_animatable_properties_function(
            to=to,
            tree_node=tree_node,
            generated_children=generated_children,
            function_name=function_name
        )
        return True

    def _generate_conservatively_collect_changed_animatable_properties(self, *, to):
        to.write(f"class ChangedAnimatablePropertiesFunctions final {{")
        to.write(f"public:")

        with to.indent():
            function_name = 'collect'
            root = self.properties_and_descriptors.computed_style_storage_model

            self._generate_conservatively_collect_changed_animatable_properties_recursive(
                to=to,
                tree_node=root,
                function_name=function_name
            )

        to.write(f"}};")
        to.newline()

        to.write(f"void ChangedAnimatablePropertiesGenerated::conservativelyCollectChangedAnimatableProperties(const ComputedStyle& a, const ComputedStyle& b, CSSPropertiesBitSet& changingProperties)")
        to.write(f"{{")

        with to.indent():
            to.write(f"ChangedAnimatablePropertiesFunctions::collect(a, b, changingProperties);")

        to.write(f"}}")
        to.newline()

    def generate_style_changed_animatable_properties_generated_cpp(self):
        with open('StyleChangedAnimatablePropertiesGenerated.cpp', 'w') as output_file:
            writer = Writer(output_file)

            self.generation_context.generate_heading(
                to=writer
            )

            self.generation_context.generate_cpp_required_includes(
                to=writer,
                header="StyleChangedAnimatablePropertiesGenerated.h"
            )

            self.generation_context.generate_includes(
                to=writer,
                headers=[
                    "StyleChangedAnimatablePropertiesCustom.h",
                ]
            )

            with self.generation_context.namespaces(["WebCore", "Style"], to=writer):
                self._generate_conservatively_collect_changed_animatable_properties(to=writer)


# Helper class for representing a function parameter.
class FunctionParameter:
    def __init__(self, type, name):
        self.type = type
        self.name = name

    @property
    def declaration_string(self):
        return f"{self.type}"

    @property
    def definition_string(self):
        return f"{self.type} {self.name}"


# Helper class for representing a function signature.
class FunctionSignature:
    def __init__(self, *, result_type, scope, name, parameters):
        self.result_type = result_type
        self.scope = scope
        self.name = name
        self.parameters = parameters

    @property
    def _declaration_parameters_string(self):
        return ", ".join(parameter.declaration_string for parameter in self.parameters)

    @property
    def _definition_parameters_string(self):
        return ", ".join(parameter.definition_string for parameter in self.parameters)

    @property
    def _scope_string(self):
        return f"{self.scope}::" if self.scope else ""

    @property
    def declaration_string(self):
        return f"{self.result_type} {self.name}({self._declaration_parameters_string})"

    @property
    def definition_string(self):
        return f"{self.result_type} {self._scope_string}{self.name}({self._definition_parameters_string})"

    @property
    def reference_string(self):
        return f"{self._scope_string}{self.name}"

    def generate_call_string(self, parameters):
        return f"{self._scope_string}{self.name}({', '.join(parameters)})"


# The `TermGenerator` classes generate parser functions by providing
# generation of parsing text for a term or set of terms.
class TermGenerator(object):
    def make(term, keyword_fast_path_generator=None):
        if isinstance(term, MatchOneTerm):
            return TermGeneratorMatchOneTerm(term, keyword_fast_path_generator)
        elif isinstance(term, MatchOneOrMoreAnyOrderTerm):
            return TermGeneratorMatchOneOrMoreAnyOrderTerm(term)
        elif isinstance(term, MatchAllOrderedTerm):
            return TermGeneratorMatchAllOrderedTerm(term)
        elif isinstance(term, MatchAllAnyOrderTerm):
            return TermGeneratorMatchAllAnyOrderTerm(term)
        elif isinstance(term, OptionalTerm):
            return TermGeneratorOptionalTerm(term)
        elif isinstance(term, UnboundedRepetitionTerm):
            return TermGeneratorUnboundedRepetitionTerm(term)
        elif isinstance(term, BoundedRepetitionTerm):
            return TermGeneratorBoundedRepetitionTerm(term)
        elif isinstance(term, ReferenceTerm):
            return TermGeneratorReferenceTerm(term)
        elif isinstance(term, FunctionTerm):
            return TermGeneratorFunctionTerm(term)
        elif isinstance(term, LiteralTerm):
            return TermGeneratorLiteralTerm(term)
        elif isinstance(term, KeywordTerm):
            return TermGeneratorNonFastPathKeywordTerm([term])
        else:
            raise Exception(f"Unknown term type - {type(term)} - {term}")


# Generation support for a single `OptionalTerm`.
class TermGeneratorOptionalTerm(TermGenerator):
    def __init__(self, optional_term):
        self.term = optional_term
        self.subterm_generator = TermGenerator.make(optional_term.subterm, None)
        self.requires_state = self.subterm_generator.requires_state

    def __str__(self):
        return str(self.term)

    def __repr__(self):
        return self.__str__()

    @property
    def produces_group(self):
        return self.subterm_generator.produces_group

    def generate_conditional(self, *, to, range_string, state_string):
        self.subterm_generator.generate_conditional(to=to, range_string=range_string, state_string=state_string)

    def generate_unconditional(self, *, to, range_string, state_string):
        self.subterm_generator.generate_unconditional(to=to, range_string=range_string, state_string=state_string)


# Generation support for a single `FunctionTerm`.
class TermGeneratorFunctionTerm(TermGenerator):
    def __init__(self, term):
        self.term = term
        self.parameter_group_generator = TermGenerator.make(term.parameter_group_term)
        self.requires_state = self.parameter_group_generator.requires_state

    def __str__(self):
        return str(self.term)

    def __repr__(self):
        return self.__str__()

    @property
    def produces_group(self):
        return False

    def generate_conditional(self, *, to, range_string, state_string):
        to.write(f"// {str(self)}")
        self._generate_lambda(to=to)
        to.write(f"if (auto result = {self._generate_call_string(range_string=range_string, state_string=state_string)})")
        with to.indent():
            to.write(f"return result;")

    def generate_unconditional(self, *, to, range_string, state_string):
        to.write(f"// {str(self)}")
        self._generate_lambda(to=to)
        to.write(f"return {self._generate_call_string(range_string=range_string, state_string=state_string)};")

    def _generate_lambda(self, *, to):
        lambda_declaration_paramaters = ["CSSParserTokenRange& range"]
        if self.parameter_group_generator.requires_state:
            lambda_declaration_paramaters += ["CSS::PropertyParserState& state"]

        to.write(f"auto consume{self.term.name.id_without_prefix}Function = []({', '.join(lambda_declaration_paramaters)}) -> RefPtr<CSSValue> {{")
        with to.indent():
            if self.term.settings_flag:
                to.write(f"if (!state.context.{self.term.settings_flag})")
                with to.indent():
                    to.write(f"return {{ }};")

            inner_lambda_declaration_paramaters = ["CSSParserTokenRange& args"]
            inner_lambda_declaration_calling_parameters = ["args"]
            if self.parameter_group_generator.requires_state:
                inner_lambda_declaration_paramaters += ["CSS::PropertyParserState& state"]
                inner_lambda_declaration_calling_parameters += ["state"]

            to.write(f"auto consumeParameters = []({', '.join(inner_lambda_declaration_paramaters)}) -> std::optional<CSSValueListBuilder> {{")
            with to.indent():
                if self.parameter_group_generator.produces_group:
                    self.parameter_group_generator.generate_unconditional_into_builder(to=to, range_string="args", state_string="state")
                else:
                    to.write(f"auto consumeParameter = []({', '.join(inner_lambda_declaration_paramaters)}) -> RefPtr<CSSValue> {{")
                    with to.indent():
                        self.parameter_group_generator.generate_unconditional(to=to, range_string="args", state_string="state")
                    to.write(f"}};")
                    to.write(f"auto parameter = consumeParameter({', '.join(inner_lambda_declaration_calling_parameters)});")
                    to.write(f"if (!parameter)")
                    with to.indent():
                        if isinstance(self.parameter_group_generator, TermGeneratorOptionalTerm):
                            to.write(f"return CSSValueListBuilder {{ }};")
                        else:
                            to.write(f"return {{ }};")
                    to.write(f"return CSSValueListBuilder {{ parameter.releaseNonNull() }};")
            to.write(f"}};")

            to.write(f"if (range.peek().functionId() != {self.term.name.id})")
            with to.indent():
                to.write(f"return {{ }};")

            to.write(f"CSSParserTokenRange rangeCopy = range;")
            to.write(f"CSSParserTokenRange args = consumeFunction(rangeCopy);")

            to.write(f"auto result = consumeParameters({', '.join(inner_lambda_declaration_calling_parameters)});")
            to.write(f"if (!result)")
            with to.indent():
                to.write(f"return {{ }};")

            to.write(f"if (!args.atEnd())")
            with to.indent():
                to.write(f"return {{ }};")

            to.write(f"range = rangeCopy;")
            to.write(f"return CSSFunctionValue::create({self.term.name.id}, WTF::move(*result));")
        to.write(f"}};")

    def _generate_call_string(self, *, range_string, state_string):
        parameters = [range_string]
        if self.parameter_group_generator.requires_state:
            parameters += [state_string]
        return f"consume{self.term.name.id_without_prefix}Function({', '.join(parameters)})"


# Generation support for a single `LiteralTerm`.
class TermGeneratorLiteralTerm(TermGenerator):
    def __init__(self, term):
        self.term = term
        self.requires_state = self.term.requires_state

    def __str__(self):
        return str(self.term)

    def __repr__(self):
        return self.__str__()

    @property
    def produces_group(self):
        return False

    def generate_conditional(self, *, to, range_string, state_string):
        # FIXME: Implement generation.
        pass

    def generate_unconditional(self, *, to, range_string, state_string):
        # FIXME: Implement generation.
        pass


# Generation support for a single `UnboundedRepetitionTerm`.
class TermGeneratorUnboundedRepetitionTerm(TermGenerator):
    def __init__(self, term):
        self.term = term
        self.repeated_term_generator = TermGenerator.make(term.repeated_term, None)
        self.requires_state = self.repeated_term_generator.requires_state

    def __str__(self):
        return str(self.term)

    def __repr__(self):
        return self.__str__()

    @property
    def produces_group(self):
        return True

    def generate_conditional(self, *, to, range_string, state_string):
        to.write(f"// {str(self)}")
        self._generate_lambda(to=to)
        to.write(f"if (auto result = {self._generate_call_string(range_string=range_string, state_string=state_string)})")
        with to.indent():
            to.write(f"return result;")

    def generate_unconditional(self, *, to, range_string, state_string):
        to.write(f"// {str(self)}")
        self._generate_lambda(to=to)
        to.write(f"return {self._generate_call_string(range_string=range_string, state_string=state_string)};")

    def generate_unconditional_into_builder(self, *, to, range_string, state_string):
        to.write(f"// {str(self)}")
        self._generate_lambda_into_builder(to=to)
        to.write(f"return {self._generate_call_string(range_string=range_string, state_string=state_string)};")

    def _generate_consume_repeated_term_lambda(self, *, to):
        lambda_declaration_parameters = ["CSSParserTokenRange& range"]
        if self.repeated_term_generator.requires_state:
            lambda_declaration_parameters += ["CSS::PropertyParserState& state"]

        to.write(f"auto consumeRepeatedTerm = []({', '.join(lambda_declaration_parameters)}) -> RefPtr<CSSValue> {{")
        with to.indent():
            self.repeated_term_generator.generate_unconditional(to=to, range_string="range", state_string="state")
        to.write(f"}};")

    def _generate_lambda(self, *, to):
        lambda_declaration_parameters = ["CSSParserTokenRange& range"]
        if self.repeated_term_generator.requires_state:
            lambda_declaration_parameters += ["CSS::PropertyParserState& state"]

        to.write(f"auto consumeUnboundedRepetition = []({', '.join(lambda_declaration_parameters)}) -> RefPtr<CSSValue> {{")
        with to.indent():
            if self.term.settings_flag:
                to.write(f"if (!state.context.{self.term.settings_flag})")
                with to.indent():
                    to.write(f"return {{ }};")

            self._generate_consume_repeated_term_lambda(to=to)

            parameters = ["range", "consumeRepeatedTerm"]
            if self.repeated_term_generator.requires_state:
                parameters += ["state"]

            if self.term.min <= 1 and self.term.single_value_optimization:
                optimization = 'SingleValue'
            else:
                optimization = 'None'

            to.write(f"return consumeListSeparatedBy<'{self.term.separator}', ListBounds::minimumOf({self.term.min}), ListOptimization::{optimization}, {self.term.type}>({', '.join(parameters)});")
        to.write(f"}};")

    def _generate_lambda_into_builder(self, *, to):
        lambda_declaration_parameters = ["CSSParserTokenRange& range"]
        if self.repeated_term_generator.requires_state:
            lambda_declaration_parameters += ["CSS::PropertyParserState& state"]

        to.write(f"auto consumeUnboundedRepetition = []({', '.join(lambda_declaration_parameters)}) -> std::optional<CSSValueListBuilder> {{")
        with to.indent():
            self._generate_consume_repeated_term_lambda(to=to)

            parameters = ["range", "consumeRepeatedTerm"]
            if self.repeated_term_generator.requires_state:
                parameters += ["state"]

            to.write(f"return consumeListSeparatedByIntoBuilder<'{self.term.separator}', ListBounds::minimumOf({self.term.min})>({', '.join(parameters)});")
        to.write(f"}};")

    def _generate_call_string(self, *, range_string, state_string):
        parameters = [range_string]
        if self.repeated_term_generator.requires_state:
            parameters += [state_string]
        return f"consumeUnboundedRepetition({', '.join(parameters)})"


# Generation support for a single `BoundedRepetitionTerm`.
class TermGeneratorBoundedRepetitionTerm(TermGenerator):
    def __init__(self, term):
        self.term = term
        self.repeated_term_generator = TermGenerator.make(term.repeated_term, None)
        self.requires_state = self.repeated_term_generator.requires_state

    def __str__(self):
        return str(self.term)

    def __repr__(self):
        return self.__str__()

    @property
    def produces_group(self):
        return True

    def generate_conditional(self, *, to, range_string, state_string):
        to.write(f"// {str(self)}")
        self._generate_lambda(to=to)
        to.write(f"if (auto result = {self._generate_call_string(range_string=range_string, state_string=state_string)})")
        with to.indent():
            to.write(f"return result;")

    def generate_unconditional(self, *, to, range_string, state_string):
        to.write(f"// {str(self)}")
        self._generate_lambda(to=to)
        to.write(f"return {self._generate_call_string(range_string=range_string, state_string=state_string)};")

    def generate_unconditional_into_builder(self, *, to, range_string, state_string):
        to.write(f"// {str(self)}")
        self._generate_lambda_into_builder(to=to)
        to.write(f"return {self._generate_call_string(range_string=range_string, state_string=state_string)};")

    def _generate_consume_repeated_term_lambda(self, *, to):
        lambda_declaration_parameters = ["CSSParserTokenRange& range"]
        if self.repeated_term_generator.requires_state:
            lambda_declaration_parameters += ["CSS::PropertyParserState& state"]

        to.write(f"auto consumeRepeatedTerm = []({', '.join(lambda_declaration_parameters)}) -> RefPtr<CSSValue> {{")
        with to.indent():
            self.repeated_term_generator.generate_unconditional(to=to, range_string="range", state_string="state")
        to.write(f"}};")

    def _generate_lambda(self, *, to):
        lambda_declaration_parameters = ["CSSParserTokenRange& range"]
        if self.repeated_term_generator.requires_state:
            lambda_declaration_parameters += ["CSS::PropertyParserState& state"]

        to.write(f"auto consumeBoundedRepetition = []({', '.join(lambda_declaration_parameters)}) -> RefPtr<CSSValue> {{")
        with to.indent():
            if self.term.settings_flag:
                to.write(f"if (!state.context.{self.term.settings_flag})")
                with to.indent():
                    to.write(f"return {{ }};")

            self._generate_consume_repeated_term_lambda(to=to)

            if self.term.type != 'CSSValueList':
                inner_lambda_declaration_calling_parameters = ["rangeCopy"]
                if self.repeated_term_generator.requires_state:
                    inner_lambda_declaration_calling_parameters += ["state"]

                to.write(f"CSSParserTokenRange rangeCopy = range;")

                terms_to_return = []

                for i in range(0, self.term.min):
                    terms_to_return.append(f"term{i}.releaseNonNull()")
                    to.write(f"auto term{i} = consumeRepeatedTerm({', '.join(inner_lambda_declaration_calling_parameters)});")
                    to.write(f"if (!term{i})")
                    with to.indent():
                        to.write(f"return {{ }};")

                for i in range(self.term.min, self.term.max):
                    terms_to_return.append(f"term{i}.releaseNonNull()")
                    to.write(f"auto term{i} = consumeRepeatedTerm({', '.join(inner_lambda_declaration_calling_parameters)});")
                    to.write(f"if (!term{i}) {{")
                    with to.indent():
                        if self.term.default:
                            if self.term.default == 'previous':
                                to.write(f"term{i} = term{i - 1};")
                            else:
                                raise Exception(f"Unknown default value pragma {self.term.default}")

                            to.write(f"range = rangeCopy;")
                            to.write(f"return {self.term.type}::create({', '.join(terms_to_return)});")
                        else:
                            to.write(f"range = rangeCopy;")
                            if i - 1 == 0 and self.term.single_value_optimization:
                                to.write(f"return term{i - 1}.releaseNonNull(); // single item optimization")
                            else:
                                to.write(f"return {self.term.type}::create({', '.join(terms_to_return[:-1])});")
                    to.write(f"}}")

                to.write(f"range = rangeCopy;")
                to.write(f"return {self.term.type}::create({', '.join(terms_to_return)});")
            else:
                consumeListParameters = ["range", "consumeRepeatedTerm"]
                if self.repeated_term_generator.requires_state:
                    consumeListParameters += ["state"]

                if self.term.min <= 1 and self.term.single_value_optimization:
                    optimization = 'SingleValue'
                else:
                    optimization = 'None'

                to.write(f"return consumeListSeparatedBy<'{self.term.separator}', ListBounds {{ {self.term.min}, {self.term.max} }}, ListOptimization::{optimization}>({', '.join(consumeListParameters)});")
        to.write(f"}};")

    def _generate_lambda_into_builder(self, *, to):
        lambda_declaration_parameters = ["CSSParserTokenRange& range"]
        if self.repeated_term_generator.requires_state:
            lambda_declaration_parameters += ["CSS::PropertyParserState& state"]

        to.write(f"auto consumeBoundedRepetition = []({', '.join(lambda_declaration_parameters)}) -> std::optional<CSSValueListBuilder> {{")
        with to.indent():
            self._generate_consume_repeated_term_lambda(to=to)

            consumeListParameters = ["range", "consumeRepeatedTerm"]
            if self.repeated_term_generator.requires_state:
                consumeListParameters += ["state"]

            to.write(f"return consumeListSeparatedByIntoBuilder<'{self.term.separator}', ListBounds {{ {self.term.min}, {self.term.max} }}>({', '.join(consumeListParameters)});")
        to.write(f"}};")

    def _generate_call_string(self, *, range_string, state_string):
        parameters = [range_string]
        if self.repeated_term_generator.requires_state:
            parameters += [state_string]
        return f"consumeBoundedRepetition({', '.join(parameters)})"


# Generation support for a single `MatchOneTerm`.
class TermGeneratorMatchOneTerm(TermGenerator):
    def __init__(self, term, keyword_fast_path_generator=None):
        self.term = term
        self.keyword_fast_path_generator = keyword_fast_path_generator
        self.term_generators = TermGeneratorMatchOneTerm._build_term_generators(term, keyword_fast_path_generator)
        self.requires_state = term.settings_flag is not None or any(term_generator.requires_state for term_generator in self.term_generators)

    def __str__(self):
        return str(self.term)

    def __repr__(self):
        return self.__str__()

    @property
    def produces_group(self):
        return False

    @staticmethod
    def _build_term_generators(term, keyword_fast_path_generator):
        # Partition the sub-terms by type:
        fast_path_keyword_terms = []
        non_fast_path_keyword_terms = []
        reference_terms = []
        function_terms = []
        unbounded_repetition_terms = []
        bounded_repetition_terms = []
        match_one_or_more_any_order_terms = []
        match_all_ordered_terms = []
        match_all_any_order_terms = []

        for sub_term in term.subterms:
            if isinstance(sub_term, KeywordTerm):
                if keyword_fast_path_generator and sub_term.is_eligible_for_fast_path:
                    fast_path_keyword_terms.append(sub_term)
                else:
                    non_fast_path_keyword_terms.append(sub_term)
            elif isinstance(sub_term, ReferenceTerm):
                reference_terms.append(sub_term)
            elif isinstance(sub_term, FunctionTerm):
                function_terms.append(sub_term)
            elif isinstance(sub_term, UnboundedRepetitionTerm):
                unbounded_repetition_terms.append(sub_term)
            elif isinstance(sub_term, BoundedRepetitionTerm):
                bounded_repetition_terms.append(sub_term)
            elif isinstance(sub_term, BoundedRepetitionTerm):
                repetition_terms.append(sub_term)
            elif isinstance(sub_term, MatchOneOrMoreAnyOrderTerm):
                match_one_or_more_any_order_terms.append(sub_term)
            elif isinstance(sub_term, MatchAllOrderedTerm):
                match_all_ordered_terms.append(sub_term)
            elif isinstance(sub_term, MatchAllAnyOrderTerm):
                match_all_any_order_terms.append(sub_term)
            else:
                raise Exception(f"Unsupported term '{sub_term}' used inside MatchOneTerm '{term}'")

        # Build a list of generators for the terms, starting with all (if any) the keywords at once.
        term_generators = []

        if fast_path_keyword_terms:
            term_generators += [TermGeneratorFastPathKeywordTerms(keyword_fast_path_generator)]
        if non_fast_path_keyword_terms:
            term_generators += [TermGeneratorNonFastPathKeywordTerm(non_fast_path_keyword_terms)]
        if reference_terms:
            term_generators += [TermGenerator.make(sub_term) for sub_term in reference_terms]
        if function_terms:
            term_generators += [TermGenerator.make(sub_term) for sub_term in function_terms]
        if unbounded_repetition_terms:
            term_generators += [TermGenerator.make(sub_term) for sub_term in unbounded_repetition_terms]
        if bounded_repetition_terms:
            term_generators += [TermGenerator.make(sub_term) for sub_term in bounded_repetition_terms]
        if match_one_or_more_any_order_terms:
            term_generators += [TermGenerator.make(sub_term) for sub_term in match_one_or_more_any_order_terms]
        if match_all_ordered_terms:
            term_generators += [TermGenerator.make(sub_term) for sub_term in match_all_ordered_terms]
        if match_all_any_order_terms:
            term_generators += [TermGenerator.make(sub_term) for sub_term in match_all_any_order_terms]

        return term_generators

    def generate_conditional(self, *, to, range_string, state_string):
        if self.term.settings_flag:
            to.write(f"if (!{state_string}.context.{self.term.settings_flag})")
            with to.indent():
                to.write(f"return {{ }};")

        for term_generator in self.term_generators:
            term_generator.generate_conditional(to=to, range_string=range_string, state_string=state_string)

    def generate_unconditional(self, *, to, range_string, state_string):
        # Pop the last generator off, as that one will be the special, non-if case.
        *remaining_term_generators, last_term_generator = self.term_generators

        if self.term.settings_flag:
            to.write(f"if (!{state_string}.context.{self.term.settings_flag})")
            with to.indent():
                to.write(f"return {{ }};")

        # For any remaining generators, call the consume function and return the result if non-null.
        for term_generator in remaining_term_generators:
            term_generator.generate_conditional(to=to, range_string=range_string, state_string=state_string)

        # And finally call that last generator we popped of back.
        last_term_generator.generate_unconditional(to=to, range_string=range_string, state_string=state_string)


# Generation support for a single `MatchAllOrderedTerm`.
class TermGeneratorMatchAllOrderedTerm(TermGenerator):
    def __init__(self, term):
        self.term = term
        self.subterm_generators = [TermGenerator.make(subterm) for subterm in term.subterms]
        self.requires_state = any(subterm_generator.requires_state for subterm_generator in self.subterm_generators)
        self.number_of_terms = count_iterable(self.subterm_generators)
        self.number_of_optional_terms = count_iterable(filter(lambda x: isinstance(x, TermGeneratorOptionalTerm), self.subterm_generators))

    def __str__(self):
        return str(self.term)

    def __repr__(self):
        return self.__str__()

    @property
    def produces_group(self):
        return True

    def generate_conditional(self, *, to, range_string, state_string):
        to.write(f"// {str(self)}")
        self._generate_lambda(to=to)
        to.write(f"if (auto result = {self._generate_call_string(range_string=range_string, state_string=state_string)})")
        with to.indent():
            to.write(f"return result;")

    def generate_unconditional(self, *, to, range_string, state_string):
        to.write(f"// {str(self)}")
        self._generate_lambda(to=to)
        to.write(f"return {self._generate_call_string(range_string=range_string, state_string=state_string)};")

    def generate_unconditional_into_builder(self, *, to, range_string, state_string):
        to.write(f"// {str(self)}")
        self._generate_lambda_into_builder(to=to)
        to.write(f"return {self._generate_call_string(range_string=range_string, state_string=state_string)};")

    def _generate_consume_subterm_lambdas(self, *, to):
        for (i, subterm_generator) in enumerate(self.subterm_generators):
            inner_lambda_declaration_parameters = ["CSSParserTokenRange& range"]
            if subterm_generator.requires_state:
                inner_lambda_declaration_parameters += ["CSS::PropertyParserState& state"]

            to.write(f"auto consumeTerm{i} = []({', '.join(inner_lambda_declaration_parameters)}) -> RefPtr<CSSValue> {{")
            with to.indent():
                subterm_generator.generate_unconditional(to=to, range_string="range", state_string="state")
            to.write(f"}};")

    def _generate_lambda(self, *, to):
        lambda_declaration_parameters = ["CSSParserTokenRange& range"]
        if self.requires_state:
            lambda_declaration_parameters += ["CSS::PropertyParserState& state"]

        to.write(f"auto consumeMatchAllOrdered = []({', '.join(lambda_declaration_parameters)}) -> RefPtr<CSSValue> {{")
        with to.indent():
            if self.term.settings_flag:
                to.write(f"if (!state.context.{self.term.settings_flag})")
                with to.indent():
                    to.write(f"return {{ }};")

            self._generate_consume_subterm_lambdas(to=to)

            if self.term.type == 'CSSValueList':
                return_type_create = "CSSValueList::createSpaceSeparated"
            else:
                return_type_create = f"{self.term.type}::create"

            if self.number_of_optional_terms > 0:
                to.write(f"CSSValueListBuilder list;")

                for (i, subterm_generator) in enumerate(self.subterm_generators):
                    inner_lambda_call_parameters = ["range"]
                    if subterm_generator.requires_state:
                        inner_lambda_call_parameters += ["state"]

                    to.write(f"// {str(subterm_generator)}")
                    to.write(f"auto value{i} = consumeTerm{i}({', '.join(inner_lambda_call_parameters)});")
                    to.write(f"if (value{i})")
                    with to.indent():
                        to.write(f"list.append(value{i}.releaseNonNull());")

                    if not isinstance(subterm_generator, TermGeneratorOptionalTerm):
                        to.write(f"else")
                        with to.indent():
                            to.write(f"return {{ }};")

                if self.term.type == 'CSSValueList':
                    if self.number_of_terms - self.number_of_optional_terms <= 1 and self.term.single_value_optimization:
                        # Only attempt the single item optimization if there are enough optional terms that it
                        # can kick in and it hasn't been explicitly disabled via @(no-single-item-opt).
                        to.write(f"if (list.size() == 1)")
                        with to.indent():
                            to.write(f"return WTF::move(list[0]); // single item optimization")
                    to.write(f"return {return_type_create}(WTF::move(list));")
                else:
                    min_values = self.number_of_terms - self.number_of_optional_terms
                    max_values = self.number_of_terms

                    list_value_strings = []
                    for list_index in range(0, min_values - 1):
                        list_value_strings.append(f"WTF::move(list[{list_index}])")

                    for list_index in range(min_values - 1, max_values - 1):
                        list_value_strings.append(f"WTF::move(list[{list_index}])")

                        to.write(f"if (list.size() == {list_index + 1})")
                        with to.indent():
                            if list_index == 0 and self.term.single_value_optimization:
                                to.write(f"return WTF::move(list[0]); // single item optimization")
                            else:
                                to.write(f"return {return_type_create}({', '.join(list_value_strings)});")

                    list_value_strings.append(f"WTF::move(list[{max_values - 1}])")
                    to.write(f"return {return_type_create}({', '.join(list_value_strings)});")
            else:
                return_value_strings = []

                for (i, subterm_generator) in enumerate(self.subterm_generators):
                    inner_lambda_call_parameters = ["range"]
                    if subterm_generator.requires_state:
                        inner_lambda_call_parameters += ["state"]

                    to.write(f"// {str(subterm_generator)}")
                    to.write(f"auto value{i} = consumeTerm{i}({', '.join(inner_lambda_call_parameters)});")
                    to.write(f"if (!value{i})")
                    with to.indent():
                        to.write(f"return {{ }};")
                    return_value_strings.append(f"value{i}.releaseNonNull()")

                to.write(f"return {return_type_create}({', '.join(return_value_strings)});")
        to.write(f"}};")

    def _generate_lambda_into_builder(self, *, to):
        lambda_declaration_parameters = ["CSSParserTokenRange& range"]
        if self.requires_state:
            lambda_declaration_parameters += ["CSS::PropertyParserState& state"]

        to.write(f"auto consumeMatchAllOrdered = []({', '.join(lambda_declaration_parameters)}) -> std::optional<CSSValueListBuilder> {{")
        with to.indent():
            self._generate_consume_subterm_lambdas(to=to)

            to.write(f"CSSValueListBuilder list;")

            for (i, subterm_generator) in enumerate(self.subterm_generators):
                inner_lambda_call_parameters = ["range"]
                if subterm_generator.requires_state:
                    inner_lambda_call_parameters += ["state"]

                to.write(f"// {str(subterm_generator)}")
                to.write(f"auto value{i} = consumeTerm{i}({', '.join(inner_lambda_call_parameters)});")
                to.write(f"if (value{i})")
                with to.indent():
                    to.write(f"list.append(value{i}.releaseNonNull());")

                if not isinstance(subterm_generator, TermGeneratorOptionalTerm):
                    to.write(f"else")
                    with to.indent():
                        to.write(f"return {{ }};")

            to.write(f"return {{ WTF::move(list) }};")

        to.write(f"}};")

    def _generate_call_string(self, *, range_string, state_string):
        parameters = [range_string]
        if self.requires_state:
            parameters += [state_string]
        return f"consumeMatchAllOrdered({', '.join(parameters)})"


# Generation support for a single `MatchAllAnyOrderTerm`.
class TermGeneratorMatchAllAnyOrderTerm(TermGenerator):
    def __init__(self, term):
        self.term = term
        self.subterm_generators = [TermGenerator.make(subterm) for subterm in term.subterms]
        self.requires_state = any(subterm_generator.requires_state for subterm_generator in self.subterm_generators)
        self.number_of_terms = count_iterable(self.subterm_generators)
        self.number_of_optional_terms = count_iterable(filter(lambda x: isinstance(x, TermGeneratorOptionalTerm), self.subterm_generators))

    def __str__(self):
        return str(self.term)

    def __repr__(self):
        return self.__str__()

    @property
    def produces_group(self):
        return True

    def generate_conditional(self, *, to, range_string, state_string):
        to.write(f"// {str(self)}")
        self._generate_lambda(to=to)
        to.write(f"if (auto result = {self._generate_call_string(range_string=range_string, state_string=state_string)})")
        with to.indent():
            to.write(f"return result;")

    def generate_unconditional(self, *, to, range_string, state_string):
        to.write(f"// {str(self)}")
        self._generate_lambda(to=to)
        to.write(f"return {self._generate_call_string(range_string=range_string, state_string=state_string)};")

    def generate_unconditional_into_builder(self, *, to, range_string, state_string):
        to.write(f"// {str(self)}")
        self._generate_lambda_into_builder(to=to)
        to.write(f"return {self._generate_call_string(range_string=range_string, state_string=state_string)};")

    def _generate_consume_subterm_lambdas(self, *, to):
        try_consume_strings = []

        if self.term.preserve_order:
            to.write(f"CSSValueListBuilder list;")

        for (i, subterm_generator) in enumerate(self.subterm_generators):
            inner_lambda_declaration_parameters = ["CSSParserTokenRange& range"]
            if subterm_generator.requires_state:
                inner_lambda_declaration_parameters += ["CSS::PropertyParserState& state"]

            if self.term.preserve_order:
                to.write(f"bool consumedValue{i} = false; // {str(subterm_generator)}")
                lambda_capture_list_parameters = [f"&list", f"&consumedValue{i}"]
            else:
                to.write(f"RefPtr<CSSValue> value{i}; // {str(subterm_generator)}")
                lambda_capture_list_parameters = [f"&value{i}"]

            to.write(f"auto tryConsumeTerm{i} = [{', '.join(lambda_capture_list_parameters)}]({', '.join(inner_lambda_declaration_parameters)}) -> bool {{")
            with to.indent():
                to.write(f"auto consumeTerm{i} = []({', '.join(inner_lambda_declaration_parameters)}) -> RefPtr<CSSValue> {{")
                with to.indent():
                    subterm_generator.generate_unconditional(to=to, range_string="range", state_string="state")
                to.write(f"}};")

                inner_lambda_call_parameters = ["range"]
                if subterm_generator.requires_state:
                    inner_lambda_call_parameters += ["state"]

                try_consume_strings.append(f"tryConsumeTerm{i}({', '.join(inner_lambda_call_parameters)})")

                if self.term.preserve_order:
                    to.write(f"if (consumedValue{i})")
                    with to.indent():
                        to.write(f"return false;")

                    to.write(f"if (auto value = consumeTerm{i}({', '.join(inner_lambda_call_parameters)})) {{")
                    with to.indent():
                        to.write(f"list.append(value.releaseNonNull());")
                        to.write(f"consumedValue{i} = true;")
                        to.write(f"return true;")
                    to.write(f"}}")
                    to.write(f"return false;")
                else:
                    to.write(f"if (value{i})")
                    with to.indent():
                        to.write(f"return false;")

                    to.write(f"value{i} = consumeTerm{i}({', '.join(inner_lambda_call_parameters)});")
                    to.write(f"return !!value{i};")
            to.write(f"}};")

        to.write(f"for (size_t i = 0; i < {len(self.subterm_generators)} && !range.atEnd(); ++i) {{")
        with to.indent():
            to.write(f"if ({' || '.join(try_consume_strings)})")
            with to.indent():
                to.write(f"continue;")
            to.write(f"break;")
        to.write(f"}}")

    def _generate_lambda(self, *, to):
        lambda_declaration_parameters = ["CSSParserTokenRange& range"]
        if self.requires_state:
            lambda_declaration_parameters += ["CSS::PropertyParserState& state"]

        to.write(f"auto consumeMatchAllAnyOrder = []({', '.join(lambda_declaration_parameters)}) -> RefPtr<CSSValue> {{")
        with to.indent():
            if self.term.settings_flag:
                to.write(f"if (!state.context.{self.term.settings_flag})")
                with to.indent():
                    to.write(f"return {{ }};")

            self._generate_consume_subterm_lambdas(to=to)

            if self.number_of_optional_terms > 0 or self.term.preserve_order:
                if self.term.preserve_order:
                    for (i, subterm_generator) in enumerate(self.subterm_generators):
                        if not isinstance(subterm_generator, TermGeneratorOptionalTerm):
                            to.write(f"if (!consumedValue{i}) // {str(subterm_generator)}")
                            with to.indent():
                                to.write(f"return {{ }};")
                else:
                    to.write(f"CSSValueListBuilder list;")
                    for (i, subterm_generator) in enumerate(self.subterm_generators):
                        to.write(f"if (value{i}) // {str(subterm_generator)}")
                        with to.indent():
                            to.write(f"list.append(value{i}.releaseNonNull());")

                        if not isinstance(subterm_generator, TermGeneratorOptionalTerm):
                            to.write(f"else")
                            with to.indent():
                                to.write(f"return {{ }};")

                if self.term.type == 'CSSValueList':
                    if self.number_of_terms - self.number_of_optional_terms <= 1 and self.term.single_value_optimization:
                        # Only attempt the single item optimization if there are enough optional terms that it
                        # can kick in and it hasn't been explicitly disabled via @(no-single-item-opt).
                        to.write(f"if (list.size() == 1)")
                        with to.indent():
                            to.write(f"return WTF::move(list[0]); // single item optimization")
                    to.write(f"return CSSValueList::createSpaceSeparated(WTF::move(list));")
                else:
                    return_type_create = f"{self.term.type}::create"

                    min_values = self.number_of_terms - self.number_of_optional_terms
                    max_values = self.number_of_terms

                    list_value_strings = []
                    for list_index in range(0, min_values - 1):
                        list_value_strings.append(f"WTF::move(list[{list_index}])")

                    for list_index in range(min_values - 1, max_values - 1):
                        list_value_strings.append(f"WTF::move(list[{list_index}])")

                        to.write(f"if (list.size() == {list_index + 1})")
                        with to.indent():
                            if list_index == 0 and self.term.single_value_optimization:
                                to.write(f"return WTF::move(list[0]); // single item optimization")
                            else:
                                to.write(f"return {return_type_create}({', '.join(list_value_strings)});")

                    list_value_strings.append(f"WTF::move(list[{max_values - 1}])")
                    to.write(f"return {return_type_create}({', '.join(list_value_strings)});")
            else:
                if self.term.type == 'CSSValueList':
                    return_type_create = "CSSValueList::createSpaceSeparated"
                else:
                    return_type_create = f"{self.term.type}::create"

                return_value_strings = []

                for (i, subterm_generator) in enumerate(self.subterm_generators):
                    to.write(f"if (!value{i}) // {str(subterm_generator)}")
                    with to.indent():
                        to.write(f"return {{ }};")
                    return_value_strings.append(f"value{i}.releaseNonNull()")

                to.write(f"return {return_type_create}({', '.join(return_value_strings)});")
        to.write(f"}};")

    def _generate_lambda_into_builder(self, *, to):
        lambda_declaration_parameters = ["CSSParserTokenRange& range"]
        if self.requires_state:
            lambda_declaration_parameters += ["CSS::PropertyParserState& state"]

        to.write(f"auto consumeMatchAllAnyOrder = []({', '.join(lambda_declaration_parameters)}) -> std::optional<CSSValueListBuilder> {{")
        with to.indent():
            self._generate_consume_subterm_lambdas(to=to)

            if self.term.preserve_order:
                for (i, subterm_generator) in enumerate(self.subterm_generators):
                    if not isinstance(subterm_generator, TermGeneratorOptionalTerm):
                        to.write(f"if (!consumedValue{i}) // {str(subterm_generator)}")
                        with to.indent():
                            to.write(f"return {{ }};")
            else:
                to.write(f"CSSValueListBuilder list;")
                for (i, subterm_generator) in enumerate(self.subterm_generators):
                    to.write(f"if (value{i}) // {str(subterm_generator)}")
                    with to.indent():
                        to.write(f"list.append(value{i}.releaseNonNull());")

                    if not isinstance(subterm_generator, TermGeneratorOptionalTerm):
                        to.write(f"else")
                        with to.indent():
                            to.write(f"return {{ }};")

            to.write(f"return {{ WTF::move(list) }};")
        to.write(f"}};")

    def _generate_call_string(self, *, range_string, state_string):
        parameters = [range_string]
        if self.requires_state:
            parameters += [state_string]
        return f"consumeMatchAllAnyOrder({', '.join(parameters)})"


# Generation support for a single `MatchOneOrMoreAnyOrderTerm`.
class TermGeneratorMatchOneOrMoreAnyOrderTerm(TermGenerator):
    def __init__(self, term):
        self.term = term
        self.subterm_generators = [TermGenerator.make(subterm) for subterm in term.subterms]
        self.requires_state = any(subterm_generator.requires_state for subterm_generator in self.subterm_generators)

    def __str__(self):
        return str(self.term)

    def __repr__(self):
        return self.__str__()

    @property
    def produces_group(self):
        return True

    def generate_conditional(self, *, to, range_string, state_string):
        to.write(f"// {str(self)}")
        self._generate_lambda(to=to)
        to.write(f"if (auto result = {self._generate_call_string(range_string=range_string, state_string=state_string)})")
        with to.indent():
            to.write(f"return result;")

    def generate_unconditional(self, *, to, range_string, state_string):
        to.write(f"// {str(self)}")
        self._generate_lambda(to=to)
        to.write(f"return {self._generate_call_string(range_string=range_string, state_string=state_string)};")

    def generate_unconditional_into_builder(self, *, to, range_string, state_string):
        to.write(f"// {str(self)}")
        self._generate_lambda_into_builder(to=to)
        to.write(f"return {self._generate_call_string(range_string=range_string, state_string=state_string)};")

    def _generate_consume_subterm_lambdas(self, *, to):
        try_consume_strings = []

        if self.term.preserve_order:
            to.write(f"CSSValueListBuilder list;")

        for (i, subterm_generator) in enumerate(self.subterm_generators):
            inner_lambda_declaration_parameters = ["CSSParserTokenRange& range"]
            if subterm_generator.requires_state:
                inner_lambda_declaration_parameters += ["CSS::PropertyParserState& state"]

            if self.term.preserve_order:
                to.write(f"bool consumedValue{i} = false; // {str(subterm_generator)}")
                lambda_capture_list_parameters = [f"&list", f"&consumedValue{i}"]
            else:
                to.write(f"RefPtr<CSSValue> value{i}; // {str(subterm_generator)}")
                lambda_capture_list_parameters = [f"&value{i}"]

            to.write(f"auto tryConsumeTerm{i} = [{', '.join(lambda_capture_list_parameters)}]({', '.join(inner_lambda_declaration_parameters)}) -> bool {{")
            with to.indent():
                to.write(f"auto consumeTerm{i} = []({', '.join(inner_lambda_declaration_parameters)}) -> RefPtr<CSSValue> {{")
                with to.indent():
                    subterm_generator.generate_unconditional(to=to, range_string="range", state_string="state")
                to.write(f"}};")

                inner_lambda_call_parameters = ["range"]
                if subterm_generator.requires_state:
                    inner_lambda_call_parameters += ["state"]

                try_consume_strings.append(f"tryConsumeTerm{i}({', '.join(inner_lambda_call_parameters)})")

                if self.term.preserve_order:
                    to.write(f"if (consumedValue{i})")
                    with to.indent():
                        to.write(f"return false;")

                    to.write(f"if (auto value = consumeTerm{i}({', '.join(inner_lambda_call_parameters)})) {{")
                    with to.indent():
                        to.write(f"list.append(value.releaseNonNull());")
                        to.write(f"consumedValue{i} = true;")
                        to.write(f"return true;")
                    to.write(f"}}")
                    to.write(f"return false;")
                else:
                    to.write(f"if (value{i})")
                    with to.indent():
                        to.write(f"return false;")

                    to.write(f"value{i} = consumeTerm{i}({', '.join(inner_lambda_call_parameters)});")
                    to.write(f"return !!value{i};")
            to.write(f"}};")

        to.write(f"for (size_t i = 0; i < {len(self.subterm_generators)} && !range.atEnd(); ++i) {{")
        with to.indent():
            to.write(f"if ({' || '.join(try_consume_strings)})")
            with to.indent():
                to.write(f"continue;")
            to.write(f"break;")
        to.write(f"}}")

    def _generate_lambda(self, *, to):
        lambda_declaration_parameters = ["CSSParserTokenRange& range"]
        if self.requires_state:
            lambda_declaration_parameters += ["CSS::PropertyParserState& state"]

        to.write(f"auto consumeMatchOneOrMoreAnyOrder = []({', '.join(lambda_declaration_parameters)}) -> RefPtr<CSSValue> {{")
        with to.indent():
            if self.term.settings_flag:
                to.write(f"if (!state.context.{self.term.settings_flag})")
                with to.indent():
                    to.write(f"return {{ }};")

            self._generate_consume_subterm_lambdas(to=to)

            if not self.term.preserve_order:
                to.write(f"CSSValueListBuilder list;")
                for (i, subterm_generator) in enumerate(self.subterm_generators):
                    to.write(f"if (value{i}) // {str(subterm_generator)}")
                    with to.indent():
                        to.write(f"list.append(value{i}.releaseNonNull());")

            to.write(f"if (list.isEmpty())")
            with to.indent():
                to.write(f"return {{ }};")

            if self.term.type == 'CSSValueList':
                if self.term.single_value_optimization:
                    to.write(f"if (list.size() == 1)")
                    with to.indent():
                        to.write(f"return WTF::move(list[0]); // single item optimization")
                to.write(f"return CSSValueList::createSpaceSeparated(WTF::move(list));")
            else:
                return_type_create = f"{self.term.type}::create"

                min_values = 1
                max_values = len(self.subterm_generators)

                list_value_strings = []
                for list_index in range(0, min_values - 1):
                    list_value_strings.append(f"WTF::move(list[{list_index}])")

                for list_index in range(min_values - 1, max_values - 1):
                    list_value_strings.append(f"WTF::move(list[{list_index}])")

                    to.write(f"if (list.size() == {list_index + 1})")
                    with to.indent():
                        if list_index == 0 and self.term.single_value_optimization:
                            to.write(f"return WTF::move(list[0]); // single item optimization")
                        else:
                            to.write(f"return {return_type_create}({', '.join(list_value_strings)});")

                list_value_strings.append(f"WTF::move(list[{max_values - 1}])")
                to.write(f"return {return_type_create}({', '.join(list_value_strings)});")
        to.write(f"}};")

    def _generate_lambda_into_builder(self, *, to):
        lambda_declaration_parameters = ["CSSParserTokenRange& range"]
        if self.requires_state:
            lambda_declaration_parameters += ["CSS::PropertyParserState& state"]

        to.write(f"auto consumeMatchOneOrMoreAnyOrder = []({', '.join(lambda_declaration_parameters)}) -> std::optional<CSSValueListBuilder> {{")
        with to.indent():
            self._generate_consume_subterm_lambdas(to=to)

            if not self.term.preserve_order:
                to.write(f"CSSValueListBuilder list;")
                for (i, subterm_generator) in enumerate(self.subterm_generators):
                    to.write(f"if (value{i}) // {str(subterm_generator)}")
                    with to.indent():
                        to.write(f"list.append(value{i}.releaseNonNull());")

            to.write(f"if (list.isEmpty())")
            with to.indent():
                to.write(f"return {{ }};")
            to.write(f"return {{ WTF::move(list) }};")
        to.write(f"}};")

    def _generate_call_string(self, *, range_string, state_string):
        parameters = [range_string]
        if self.requires_state:
            parameters += [state_string]
        return f"consumeMatchOneOrMoreAnyOrder({', '.join(parameters)})"


# Generation support for a single `ReferenceTerm`.
class TermGeneratorReferenceTerm(TermGenerator):
    def __init__(self, term):
        self.term = term

    def __str__(self):
        return str(self.term)

    def __repr__(self):
        return self.__str__()

    @property
    def produces_group(self):
        return False

    def generate_conditional(self, *, to, range_string, state_string):
        to.write(f"// {str(self)}")
        if self.term.settings_flag is not None:
            self._generate_lambda(to=to)
        to.write(f"if (auto result = {self.generate_call_string(range_string=range_string, state_string=state_string)})")
        with to.indent():
            to.write(f"return result;")

    def generate_unconditional(self, *, to, range_string, state_string):
        to.write(f"// {str(self)}")
        if self.term.settings_flag is not None:
            self._generate_lambda(to=to)
        to.write(f"return {self.generate_call_string(range_string=range_string, state_string=state_string)};")

    def generate_call_string(self, *, range_string, state_string):
        if self.term.settings_flag is not None:
            return f"consume{self.term.name.id_without_prefix}Reference({range_string}, {state_string})"
        return self._generate_call_reference_string(range_string=range_string, state_string=state_string)

    def _generate_call_reference_string(self, *, range_string, state_string):
        if self.term.override_function:
            return f"{self.term.override_function}({range_string}, {state_string})"
        elif self.term.is_builtin:
            builtin = self.term.builtin
            if isinstance(builtin, BuiltinAngleConsumer):
                return f"CSSPrimitiveValueResolver<CSS::Angle<{builtin.value_range}>>::consumeAndResolve({range_string}, {state_string}, {{ .unitlessZeroAngle = {builtin.unitless_zero} }})"
            elif isinstance(builtin, BuiltinTimeConsumer):
                return f"CSSPrimitiveValueResolver<CSS::Time<{builtin.value_range}>>::consumeAndResolve({range_string}, {state_string})"
            elif isinstance(builtin, BuiltinLengthConsumer):
                return f"CSSPrimitiveValueResolver<CSS::Length<{builtin.value_range}>>::consumeAndResolve({range_string}, {state_string}, {{ .unitlessZeroLength = {builtin.unitless_zero} }})"
            elif isinstance(builtin, BuiltinLengthPercentageConsumer):
                return f"CSSPrimitiveValueResolver<CSS::LengthPercentage<{builtin.value_range}>>::consumeAndResolve({range_string}, {state_string}, {{ .anchorPolicy = {builtin.anchor}, .anchorSizePolicy = {builtin.anchor_size}, .unitlessZeroLength = {builtin.unitless_zero} }})"
            elif isinstance(builtin, BuiltinIntegerConsumer):
                return f"CSSPrimitiveValueResolver<CSS::Integer<{builtin.value_range}>>::consumeAndResolve({range_string}, {state_string})"
            elif isinstance(builtin, BuiltinNumberConsumer):
                return f"CSSPrimitiveValueResolver<CSS::Number<{builtin.value_range}>>::consumeAndResolve({range_string}, {state_string})"
            elif isinstance(builtin, BuiltinPercentageConsumer):
                return f"CSSPrimitiveValueResolver<CSS::Percentage<{builtin.value_range}>>::consumeAndResolve({range_string}, {state_string})"
            elif isinstance(builtin, BuiltinNumberOrPercentageResolvedToNumberConsumer):
                return f"consumePercentageDividedBy100OrNumber({range_string}, {state_string})"
            elif isinstance(builtin, BuiltinPositionConsumer):
                return f"consumePosition({range_string}, {state_string})"
            elif isinstance(builtin, BuiltinColorConsumer):
                if builtin.allowed_types:
                    return f"consumeColor({range_string}, {state_string}, {{ .allowedColorTypes = {{ {builtin.allowed_types} }} }})"
                return f"consumeColor({range_string}, {state_string}, {{ .allowedColorTypes = {{ }} }})"
            elif isinstance(builtin, BuiltinImageConsumer):
                if builtin.allowed_types:
                    return f"consumeImage({range_string}, {state_string}, {{ {builtin.allowed_types} }})"
                return f"consumeImage({range_string}, {state_string}, {{ }})"
            elif isinstance(builtin, BuiltinCustomIdentConsumer):
                if builtin.excluding:
                    return f"consumeCustomIdentExcluding({range_string}, {{ { ', '.join(ValueKeywordName(id).id for id in builtin.excluding)} }})"
                return f"consumeCustomIdent({range_string})"
            elif isinstance(builtin, BuiltinURLConsumer):
                if builtin.allowed_modifiers:
                    return f"consumeURL({range_string}, {state_string}, {{ {builtin.allowed_modifiers} }})"
                return f"consumeURL({range_string}, {state_string}, {{ }})"
            elif self.requires_state:
                return f"consume{self.term.name.id_without_prefix}({range_string}, {state_string})"
            else:
                return f"consume{self.term.name.id_without_prefix}({range_string})"
        else:
            return f"consume{self.term.name.id_without_prefix}({range_string}, {state_string})"

    def _generate_lambda(self, *, to):
        lambda_declaration_parameters = ["CSSParserTokenRange& range, CSS::PropertyParserState& state"]

        to.write(f"auto consume{self.term.name.id_without_prefix}Reference = []({', '.join(lambda_declaration_parameters)}) -> RefPtr<CSSValue> {{")
        with to.indent():
            if self.term.settings_flag:
                to.write(f"if (!state.context.{self.term.settings_flag})")
                with to.indent():
                    to.write(f"return {{ }};")
            to.write(f"return {self._generate_call_reference_string(range_string='range', state_string='state')};")
        to.write(f"}};")

    @property
    def requires_state(self):
        if self.term.override_function:
            return True
        elif self.term.is_builtin:
            builtin = self.term.builtin
            if isinstance(builtin, BuiltinAngleConsumer):
                return True
            elif isinstance(builtin, BuiltinTimeConsumer):
                return True
            elif isinstance(builtin, BuiltinLengthConsumer):
                return True
            elif isinstance(builtin, BuiltinLengthPercentageConsumer):
                return True
            elif isinstance(builtin, BuiltinIntegerConsumer):
                return True
            elif isinstance(builtin, BuiltinNumberConsumer):
                return True
            elif isinstance(builtin, BuiltinPercentageConsumer):
                return True
            elif isinstance(builtin, BuiltinNumberOrPercentageResolvedToNumberConsumer):
                return True
            elif isinstance(builtin, BuiltinPositionConsumer):
                return True
            elif isinstance(builtin, BuiltinColorConsumer):
                return True
            elif isinstance(builtin, BuiltinImageConsumer):
                return True
            elif isinstance(builtin, BuiltinResolutionConsumer):
                return True
            elif isinstance(builtin, BuiltinStringConsumer):
                return False
            elif isinstance(builtin, BuiltinCustomIdentConsumer):
                return False
            elif isinstance(builtin, BuiltinDashedIdentConsumer):
                return False
            elif isinstance(builtin, BuiltinURLConsumer):
                return True
            elif isinstance(builtin, BuiltinFeatureTagValueConsumer):
                return True
            elif isinstance(builtin, BuiltinVariationTagValueConsumer):
                return True
            elif isinstance(builtin, BuiltinUnicodeRangeTokenConsumer):
                return False
            else:
                raise Exception(f"Unknown builtin type used: {builtin.name.name}")
        else:
            return True


# Generation support for any keyword terms that are not fast-path eligible.
class TermGeneratorNonFastPathKeywordTerm(TermGenerator):
    def __init__(self, keyword_terms):
        self.keyword_terms = keyword_terms
        self.requires_state = any(keyword_term.requires_state for keyword_term in self.keyword_terms)

    def __str__(self):
        return ' | '.join(stringify_iterable(self.keyword_terms))

    def __repr__(self):
        return self.__str__()

    @property
    def produces_group(self):
        return False

    def generate_conditional(self, *, to, range_string, state_string):
        to.write(f"// {str(self)}")
        self._generate(to=to, range_string=range_string, state_string=state_string, default_string="break")

    def generate_unconditional(self, *, to, range_string, state_string):
        to.write(f"// {str(self)}")
        self._generate(to=to, range_string=range_string, state_string=state_string, default_string="return nullptr")

    def _generate(self, *, to, range_string, state_string, default_string):
        # Build up a list of pairs of (value, return-expression-to-use-for-value), taking
        # into account settings flags and mode checks for internal values. Leave the return
        # expression as an empty array for the default return expression "return true;".

        ReturnExpression = collections.namedtuple('ReturnExpression', ['conditions', 'return_value'])
        KeywordTermAndReturnExpression = collections.namedtuple('KeywordTermAndReturnExpression', ['keyword_term', 'return_expression'])
        keyword_term_and_return_expressions = []

        for keyword_term in self.keyword_terms:
            conditions = []
            if keyword_term.settings_flag:
                if keyword_term.settings_flag.startswith("DeprecatedGlobalSettings::"):
                    conditions.append(f"!{keyword_term.settings_flag}")
                else:
                    conditions.append(f"!{state_string}.context.{keyword_term.settings_flag}")
            if keyword_term.status == "internal":
                conditions.append(f"!isUASheetBehavior({state_string}.context.mode)")

            if keyword_term.aliased_to:
                return_value = keyword_term.aliased_to.id
            else:
                return_value = "keyword"

            keyword_term_and_return_expressions.append(KeywordTermAndReturnExpression(keyword_term, ReturnExpression(conditions, return_value)))

        # Take the list of pairs of (value, return-expression-to-use-for-value), and
        # group them by their 'return-expression' to avoid unnecessary duplication of
        # return statements.
        to.write(f"switch (auto keyword = {range_string}.peek().id(); keyword) {{")
        for return_expression, group in itertools.groupby(sorted(keyword_term_and_return_expressions, key=lambda x: x.return_expression), lambda x: x.return_expression):
            for keyword_term, _ in group:
                to.write(f"case {keyword_term.value.id}:")

            with to.indent():
                if return_expression.conditions:
                    to.write(f"if ({' || '.join(return_expression.conditions)})")
                    with to.indent():
                        to.write(f"{default_string};")

                to.write(f"{range_string}.consumeIncludingWhitespace();")
                to.write(f"return CSSPrimitiveValue::create({return_expression.return_value});")

        to.write(f"default:")
        with to.indent():
            to.write(f"{default_string};")

        to.write(f"}}")


# Generation support for a properties fast path eligible keyword terms.
class TermGeneratorFastPathKeywordTerms(TermGenerator):
    def __init__(self, keyword_fast_path_generator):
        self.keyword_fast_path_generator = keyword_fast_path_generator
        self.requires_state = keyword_fast_path_generator.requires_state

    def __str__(self):
        return ' | '.join(stringify_iterable(self.keyword_fast_path_generator.keyword_terms))

    def __repr__(self):
        return self.__str__()

    @property
    def produces_group(self):
        return False

    def generate_conditional(self, *, to, range_string, state_string):
        to.write(f"// {str(self)}")
        to.write(f"if (auto result = {self.generate_call_string(range_string=range_string, state_string=state_string)})")
        with to.indent():
            to.write(f"return result;")

    def generate_unconditional(self, *, to, range_string, state_string):
        to.write(f"// {str(self)}")
        to.write(f"return {self.generate_call_string(range_string=range_string, state_string=state_string)};")

    def generate_call_string(self, *, range_string, state_string):
        # For root keyword terms we can utilize the `keyword-only fast path` function.
        parameters = [range_string, self.keyword_fast_path_generator.generate_reference_string()]
        if self.requires_state:
            parameters.append(state_string)
        return f"consumeIdent({', '.join(parameters)})"


# Used by the `PropertyConsumer` classes to generate a `keyword-only fast path` function
# (e.g. `isKeywordValidFor*`) for use both in the keyword only fast path and in the main
# `parse` function.
class KeywordFastPathGenerator:
    def __init__(self, name, keyword_terms):
        self.keyword_terms = keyword_terms
        self.requires_state = any(keyword_term.requires_state for keyword_term in keyword_terms)
        self.signature = KeywordFastPathGenerator._build_signature(name, self.requires_state)

    @staticmethod
    def _build_parameters(requires_state):
        parameters = [FunctionParameter("CSSValueID", "keyword")]
        if requires_state:
            parameters += [FunctionParameter("CSS::PropertyParserState&", "state")]
        return parameters

    @staticmethod
    def _build_signature(name, requires_state):
        return FunctionSignature(
            result_type="bool",
            scope=None,
            name=name,
            parameters=KeywordFastPathGenerator._build_parameters(requires_state))

    def generate_reference_string(self):
        return self.signature.reference_string

    def generate_call_string(self, *, keyword_string, state_string):
        parameters = [keyword_string]
        if self.requires_state:
            parameters += [state_string]

        return self.signature.generate_call_string(parameters)

    def generate_definition(self, *, to):
        to.write(f"static {self.signature.definition_string}")
        to.write(f"{{")

        with to.indent():
            # Build up a list of pairs of (value, return-expression-to-use-for-value), taking
            # into account settings flags and mode checks for internal values. Leave the return
            # expression as an empty array for the default return expression "return true;".

            KeywordTermReturnExpression = collections.namedtuple('KeywordTermReturnExpression', ['keyword_term', 'return_expression'])
            keyword_term_and_return_expressions = []

            for keyword_term in self.keyword_terms:
                return_expression = []
                if keyword_term.settings_flag:
                    if keyword_term.settings_flag.startswith("DeprecatedGlobalSettings::"):
                        return_expression.append(keyword_term.settings_flag)
                    else:
                        return_expression.append(f"state.context.{keyword_term.settings_flag}")
                if keyword_term.status == "internal":
                    return_expression.append("isUASheetBehavior(state.context.mode)")

                keyword_term_and_return_expressions.append(KeywordTermReturnExpression(keyword_term, return_expression))

            # Take the list of pairs of (value, return-expression-to-use-for-value), and
            # group them by their 'return-expression' to avoid unnecessary duplication of
            # return statements.
            to.write(f"switch (keyword) {{")
            for return_expression, group in itertools.groupby(sorted(keyword_term_and_return_expressions, key=lambda x: x.return_expression), lambda x: x.return_expression):
                for keyword_term, _ in group:
                    to.write(f"case {keyword_term.value.id}:")
                with to.indent():
                    to.write(f"return {' && '.join(return_expression or ['true'])};")

            to.write(f"default:")
            with to.indent():
                to.write(f"return false;")

            to.write(f"}}")
        to.write(f"}}")
        to.newline()


# Each shared grammar rule has a corresponding `SharedGrammarRuleConsumer` which defines how
# that rules parsing is exposed and if the parsing function for the rule should be exported in
# the header for use in other areas of WebCore. Currently, all non-exported rules are 'skipped'
# here. Note, that does not mean the rule isn't used, as a reference of that rule by a property
# or another shared rule will still use the grammar, it will just be simplified into the parents
# grammar with no explicit function being emitted. That leaves only exported rules actually
# having functions emitted. The current set of kinds of `SharedGrammarRuleConsumer` are:
#
#   - `SkipSharedGrammarRuleConsumer`:
#        Used when the shared property rule is not needed by other parts of WebCore, and therefore
#        no explicit function needs to be emitted. Used for any shared rule that is not marked
#        as 'exported`.
#
#   - `GeneratedSharedGrammarRuleConsumer`:
#        Used for all exported rules. These generate a dedicated `consume` function which is exported
#        in `CSSPropertyParser` for use by other parts of WebCore.
#
# `SharedGrammarRuleConsumer` abstract interface:
#
#   def generate_export_declaration(self, *, to):
#   def generate_definition(self, *, to):
#   var is_exported
#
class SharedGrammarRuleConsumer(object):
    @staticmethod
    def make(shared_grammar_rule):
        if not shared_grammar_rule.exported:
            return SkipSharedGrammarRuleConsumer(shared_grammar_rule)
        return GeneratedSharedGrammarRuleConsumer(shared_grammar_rule)


class SkipSharedGrammarRuleConsumer(SharedGrammarRuleConsumer):
    def __init__(self, shared_grammar_rule):
        self.shared_grammar_rule = shared_grammar_rule

    def __str__(self):
        return "SkipSharedGrammarRuleConsumer"

    def __repr__(self):
        return self.__str__()

    @property
    def is_exported(self):
        return False

    def generate_export_declaration(self, *, to):
        pass

    def generate_definition(self, *, to):
        pass


class GeneratedSharedGrammarRuleConsumer(SharedGrammarRuleConsumer):
    def __init__(self, shared_grammar_rule):
        self.term_generator = TermGenerator.make(shared_grammar_rule.grammar.root_term)
        self.requires_state = self.term_generator.requires_state
        self.signature = GeneratedSharedGrammarRuleConsumer._build_signature(shared_grammar_rule, self.requires_state)

    def __str__(self):
        return "GeneratedSharedGrammarRuleConsumer"

    def __repr__(self):
        return self.__str__()

    @staticmethod
    def _build_parameters(requires_state):
        parameters = [FunctionParameter("CSSParserTokenRange&", "range")]
        if requires_state:
            parameters += [FunctionParameter("CSS::PropertyParserState&", "state")]
        return parameters

    @staticmethod
    def _build_signature(shared_grammar_rule, requires_state):
        return FunctionSignature(
            result_type="RefPtr<CSSValue>",
            scope="CSSPropertyParsing",
            name=f"consume{shared_grammar_rule.name_for_methods.id_without_prefix}",
            parameters=GeneratedSharedGrammarRuleConsumer._build_parameters(requires_state))

    @property
    def is_exported(self):
        return True

    def generate_export_declaration(self, *, to):
        to.write(f"static {self.signature.declaration_string};")

    def generate_definition(self, *, to):
        to.write(f"{self.signature.definition_string}")
        to.write(f"{{")
        with to.indent():
            self.term_generator.generate_unconditional(to=to, range_string='range', state_string='state')
        to.write(f"}}")
        to.newline()


# Each CSS property has a corresponding `PropertyConsumer` which defines how that property's
# parsing works, if the parsing function for the property should be exported in the header for
# use in other areas of WebCore, and what fast paths it exposes. The current set of kinds of
# `PropertyConsumer` are:
#
#   - `SkipPropertyConsumer`:
#        Used when the property is not eligible for parsing, and should be skipped. Used for
#        descriptor-only properties, shorthand properties, and properties marked 'skip-parser`.
#
#   - `CustomPropertyConsumer`:
#        Used when the property has been marked with `parser-function`. These property consumers never
#        generate a `consume` function of their own, and call the defined `consume` function declared
#        in `parser-function` directly from the main `parse` function.
#
#   - `FastPathKeywordOnlyPropertyConsumer`:
#        The only allowed values for this property are fast path eligible keyword values. These property
#        consumers always emit a `keyword-only fast path` function (e.g. `isKeywordValidFor*`) and the
#        main `parse` function uses that fast path function directly (e.g. `consumeIdent(range, isKeywordValidFor*)`
#        This allows us to avoid making a `consume` function for the property in all cases except for
#        when the property has been marked explicitly with `parser-exported`, in which case we do
#        generate a `consume` function to warp that invocation above.
#
#   - `DirectPropertyConsumer`:
#        Used when a property's only term is a single non-simplifiable reference term (e.g. [ <number> ]
#        or [ <color> ]. These property consumers call the referenced term directly from the main `parse`
#        function. This allows us to avoid making a `consume` function for the property in all cases
#        except for when the property has been marked explicitly with `parser-exported`, in which case
#        we do generate a `consume` function to warp that invocation above.
#
#   - `GeneratedPropertyConsumer`:
#        Used for all other properties. Requires that `parser-grammar` has been defined. These property
#        consumers use the provided parser grammar to generate a dedicated `consume` function which is
#        called from the main `parse` function. If the parser grammar allows for any keyword only valid
#        parses (e.g. for the grammar [ none | <image> ], "none" is a valid keyword only parse), these
#        property consumers will also emit a `keyword-only fast path` function (e.g. `isKeywordValidFor*`)
#        and ensure that it is called from the main `isKeywordValidForStyleProperty` function.
#
# `PropertyConsumer` abstract interface:
#
#   def generate_call_string(self, *, range_string, state_string):
#   def generate_export_declaration(self, *, to):
#   def generate_definition(self, *, to):
#   var is_exported
#   var keyword_fast_path_generator

class PropertyConsumer(object):
    @staticmethod
    def make(property):
        if property.codegen_properties.longhands or property.codegen_properties.skip_parser:
            return SkipPropertyConsumer(property)

        if property.codegen_properties.parser_function:
            return CustomPropertyConsumer(property)

        if property.codegen_properties.parser_grammar:
            if property.codegen_properties.parser_grammar.has_only_fast_path_keyword_terms:
                return FastPathKeywordOnlyPropertyConsumer(property)
            if isinstance(property.codegen_properties.parser_grammar.root_term, ReferenceTerm):
                return DirectPropertyConsumer(property)
            return GeneratedPropertyConsumer(property)

        raise Exception(f"Invalid property definition for '{property.id}'. Style properties must either specify values or a custom parser.")


# Property consumer used for properties that should not be parsed.
class SkipPropertyConsumer(PropertyConsumer):
    def __init__(self, property):
        self.property = property

    def __str__(self):
        return f"SkipPropertyConsumer for {self.property}"

    def __repr__(self):
        return self.__str__()

    def generate_call_string(self, *, range_string, state_string):
        return None

    def generate_export_declaration(self, *, to):
        pass

    def generate_definition(self, *, to):
        pass

    @property
    def is_exported(self):
        return False

    @property
    def keyword_fast_path_generator(self):
        return None


# Property consumer used for properties with `parser-function` defined.
class CustomPropertyConsumer(PropertyConsumer):
    def __init__(self, property):
        self.property = property

    def __str__(self):
        return f"CustomPropertyConsumer for {self.property}"

    def __repr__(self):
        return self.__str__()

    def generate_call_string(self, *, range_string, state_string):
        return f"{self.property.codegen_properties.parser_function}({range_string}, {state_string})"

    def generate_export_declaration(self, *, to):
        pass

    def generate_definition(self, *, to):
        pass

    @property
    def is_exported(self):
        return False

    @property
    def keyword_fast_path_generator(self):
        return None


# Property consumer used for properties with only fast-path eligible keyword terms in its grammar.
class FastPathKeywordOnlyPropertyConsumer(PropertyConsumer):
    def __init__(self, property):
        self.property = property
        self.keyword_fast_path_generator = KeywordFastPathGenerator(f"isKeywordValidFor{property.name_for_parsing_methods}", property.codegen_properties.parser_grammar.fast_path_keyword_terms_sorted_by_name)
        self.term_generator = TermGeneratorFastPathKeywordTerms(self.keyword_fast_path_generator)
        self.signature = FastPathKeywordOnlyPropertyConsumer._build_signature(property, self.keyword_fast_path_generator)

    def __str__(self):
        return f"FastPathKeywordOnlyPropertyConsumer for {self.property}"

    def __repr__(self):
        return self.__str__()

    @staticmethod
    def _build_scope(property):
        if property.codegen_properties.parser_exported:
            return "CSSPropertyParsing"
        return None

    @staticmethod
    def _build_parameters(keyword_fast_path_generator):
        parameters = [FunctionParameter("CSSParserTokenRange&", "range")]
        if keyword_fast_path_generator.requires_state:
            parameters += [FunctionParameter("CSS::PropertyParserState&", "state")]
        return parameters

    @staticmethod
    def _build_signature(property, keyword_fast_path_generator):
        return FunctionSignature(
            result_type="RefPtr<CSSValue>",
            scope=FastPathKeywordOnlyPropertyConsumer._build_scope(property),
            name=f"consume{property.name_for_parsing_methods}",
            parameters=FastPathKeywordOnlyPropertyConsumer._build_parameters(keyword_fast_path_generator))

    def generate_call_string(self, *, range_string, state_string):
        # NOTE: Even in the case that we generate a `consume` function, we don't generate a call to it,
        # but rather always directly use `consumeIdent` with the keyword-only fast path predicate.
        return self.term_generator.generate_call_string(range_string=range_string, state_string=state_string)

    # For "direct" and "fast-path keyword only" consumers, we only generate the property specific
    # definition if the property has been marked as exported.

    @property
    def is_exported(self):
        return self.property.codegen_properties.parser_exported

    def generate_export_declaration(self, *, to):
        if self.is_exported:
            to.write(f"static {self.signature.declaration_string};")

    def generate_definition(self, *, to):
        if self.is_exported:
            to.write(f"{self.signature.definition_string}")
            to.write(f"{{")
            with to.indent():
                to.write(f"return {self.generate_call_string(range_string='range', state_string='state')};")
            to.write(f"}}")
            to.newline()


# Property consumer for a property that has a `parser-grammar` that consists of only a single non-simplifiable
# reference term (e.g. [ <number> ] or [ <color> ])
class DirectPropertyConsumer(PropertyConsumer):
    def __init__(self, property):
        self.property = property
        self.term_generator = TermGeneratorReferenceTerm(self.property.codegen_properties.parser_grammar.root_term)
        self.signature = DirectPropertyConsumer._build_signature(self.property, self.term_generator)

    def __str__(self):
        return f"DirectPropertyConsumer for {self.property}"

    def __repr__(self):
        return self.__str__()

    @staticmethod
    def _build_scope(property):
        if property.codegen_properties.parser_exported:
            return "CSSPropertyParsing"
        return None

    @staticmethod
    def _build_parameters(term_generator):
        parameters = [FunctionParameter("CSSParserTokenRange&", "range")]
        if term_generator.requires_state:
            parameters += [FunctionParameter("CSS::PropertyParserState&", "state")]
        return parameters

    @staticmethod
    def _build_signature(property, term_generator):
        return FunctionSignature(
            result_type="RefPtr<CSSValue>",
            scope=DirectPropertyConsumer._build_scope(property),
            name=f"consume{property.name_for_parsing_methods}",
            parameters=DirectPropertyConsumer._build_parameters(term_generator))

    def generate_call_string(self, *, range_string, state_string):
        # NOTE: Even in the case that we generate a `consume` function for the case, we don't
        # generate a call to it, but rather always generate the consume function for the reference,
        # which is just as good and works in all cases.
        return self.term_generator.generate_call_string(range_string=range_string, state_string=state_string)

    # For "direct" and "fast-path keyword only" consumers, we only generate the property specific
    # definition if the property has been marked as exported.

    @property
    def is_exported(self):
        return self.property.codegen_properties.parser_exported

    def generate_export_declaration(self, *, to):
        if self.is_exported:
            to.write(f"static {self.signature.declaration_string};")

    def generate_definition(self, *, to):
        if self.is_exported:
            to.write(f"{self.signature.definition_string}")
            to.write(f"{{")
            with to.indent():
                self.term_generator.generate_unconditional(to=to, range_string='range', state_string='state')
            to.write(f"}}")
            to.newline()

    @property
    def keyword_fast_path_generator(self):
        return None


# Default property consumer. Uses `parser-grammar` to generate a `consume` function for the property.
class GeneratedPropertyConsumer(PropertyConsumer):
    def __init__(self, property):
        self.property = property
        self.keyword_fast_path_generator = GeneratedPropertyConsumer._build_keyword_fast_path_generator(property)
        self.term_generator = TermGenerator.make(property.codegen_properties.parser_grammar.root_term, self.keyword_fast_path_generator)
        self.requires_state = self.term_generator.requires_state
        self.signature = GeneratedPropertyConsumer._build_signature(property, self.requires_state)

    def __str__(self):
        return f"GeneratedPropertyConsumer for {self.property}"

    def __repr__(self):
        return self.__str__()

    @staticmethod
    def _build_scope(property):
        if property.codegen_properties.parser_exported:
            return "CSSPropertyParsing"
        return None

    @staticmethod
    def _build_parameters(property, requires_state):
        parameters = [FunctionParameter("CSSParserTokenRange&", "range")]
        if requires_state:
            parameters += [FunctionParameter("CSS::PropertyParserState&", "state")]
        return parameters

    @staticmethod
    def _build_signature(property, requires_state):
        return FunctionSignature(
            result_type="RefPtr<CSSValue>",
            scope=GeneratedPropertyConsumer._build_scope(property),
            name=f"consume{property.name_for_parsing_methods}",
            parameters=GeneratedPropertyConsumer._build_parameters(property, requires_state))

    @staticmethod
    def _build_keyword_fast_path_generator(property):
        if property.codegen_properties.parser_grammar.has_fast_path_keyword_terms:
            return KeywordFastPathGenerator(f"isKeywordValidFor{property.name_for_parsing_methods}", property.codegen_properties.parser_grammar.fast_path_keyword_terms_sorted_by_name)
        return None

    def generate_call_string(self, *, range_string, state_string):
        parameters = [range_string]
        if self.requires_state:
            parameters += [state_string]
        return self.signature.generate_call_string(parameters)

    @property
    def is_exported(self):
        return self.property.codegen_properties.parser_exported

    def generate_export_declaration(self, *, to):
        if self.is_exported:
            to.write(f"static {self.signature.declaration_string};")

    def generate_definition(self, *, to):
        if self.is_exported:
            to.write(f"{self.signature.definition_string}")
        else:
            to.write(f"static {self.signature.definition_string}")
        to.write(f"{{")
        with to.indent():
            self.term_generator.generate_unconditional(to=to, range_string='range', state_string='state')
        to.write(f"}}")
        to.newline()


class StringEqualingEnum(enum.Enum):
    def __eq__(self, b):
        if isinstance(b, str):
            return self.name == b
        else:
            return self.name == b.name

    def __hash__(self):
        return id(self.name)


class BNFToken(StringEqualingEnum):
    # Numbers.
    FLOAT   = re.compile(r'\-?\d+\.\d+')
    INT     = re.compile(r'\-?\d+')

    # Brackets.
    LPAREN  = re.compile(r'\(')
    RPAREN  = re.compile(r'\)')
    LBRACE  = re.compile(r'\{')
    RBRACE  = re.compile(r'\}')
    LSQUARE = re.compile(r'\[')
    RSQUARE = re.compile(r'\]')
    LTLT    = re.compile(r'<<')
    GTGT    = re.compile(r'>>')
    LT      = re.compile(r'<')
    GT      = re.compile(r'>')
    SQUOTE  = re.compile(r'\'')
    ATPAREN = re.compile(r'@\(')

    # Multipliers.
    HASH    = re.compile(r'#')
    PLUS    = re.compile(r'\+')
    STAR    = re.compile(r'\*')
    NOT     = re.compile(r'!')
    QMARK   = re.compile(r'\?')

    # Combinators.
    OROR    = re.compile(r'\|\|')
    OR      = re.compile(r'\|')
    ANDAND  = re.compile(r'&&')
    COMMA   = re.compile(r',')

    # Literals
    SLASH   = re.compile(r'/')
    EQUAL   = re.compile(r'=')

    # Identifiers.
    FUNC    = re.compile(r'[_a-zA-Z\-][_a-zA-Z0-9\-]*\(')
    ID      = re.compile(r'[_a-zA-Z\-][_a-zA-Z0-9\-]*')

    # Whitespace.
    WHITESPACE = re.compile(r'(\t|\n|\s|\r)+')


BNF_ILLEGAL_TOKEN = 'ILLEGAL'
BNF_EOF_TOKEN     = 'EOF'

BNFTokenInfo = collections.namedtuple("BNFTokens", ["name", "value"])


def BNFLexer(data):
    position = 0
    while position < len(data):
        for token_id in BNFToken:
            match = token_id.value.match(data, position)
            if match:
                position = match.end(0)
                if token_id == BNFToken.WHITESPACE:
                    # ignore whitespace
                    break
                yield BNFTokenInfo(token_id.name, match.group(0))
                break
        else:
            # in case pattern doesn't match send the character as illegal
            yield BNFTokenInfo(BNF_ILLEGAL_TOKEN, data[position])
            position += 1
    yield BNFTokenInfo(BNF_EOF_TOKEN, '\x00')


class BNFRepetitionModifier:
    class Kind(enum.Enum):
        EXACT       = '{A}'
        AT_LEAST    = '{A,}'
        BETWEEN     = '{A,B}'

    def __init__(self):
        self.kind = None
        self.min = None
        self.max = None

    def __str__(self):
        if self.kind is None:
            return "[UNSET RepetitionModifier]"
        elif self.kind == BNFRepetitionModifier.Kind.EXACT:
            return '{' + str(self.min) + '}'
        elif self.kind == BNFRepetitionModifier.Kind.AT_LEAST:
            return '{' + str(self.min) + ',}'
        elif self.kind == BNFRepetitionModifier.Kind.BETWEEN:
            return '{' + str(self.min) + ',' + str(self.max) + '}'
        raise Exception("Unknown repetition kind: {self.kind}")


# BNFAnnotations are introduced by trailing '@(foo=bar,baz bat)' and are an
# extension to the syntax used by CSS, added to allow passing additional
# metadata to the code generators.
class BNFAnnotation:
    class Directive:
        def __init__(self, name):
            self.name = name
            self.value = []

        def __str__(self):
            if self.value:
                return str(self.name) + '=' + ','.join(stringify_iterable(self.value))
            return str(self.name)

    def __init__(self):
        self.directives = []

    def __str__(self):
        return '@(' + ' '.join(stringify_iterable(self.directives)) + ')'

    def add_directive(self, directive):
        self.directives.append(directive)


# Node multipliers are introduced by trailing symbols like '#', '+', '*', and '{1,4}'.
# https://drafts.csswg.org/css-values-4/#component-multipliers
class BNFNodeMultiplier:
    class Kind(enum.Enum):
        ZERO_OR_ONE                     = '?'
        SPACE_SEPARATED_ZERO_OR_MORE    = '*'
        SPACE_SEPARATED_ONE_OR_MORE     = '+'
        SPACE_SEPARATED_EXACT           = '{A}'
        SPACE_SEPARATED_AT_LEAST        = '{A,}'
        SPACE_SEPARATED_BETWEEN         = '{A,B}'
        COMMA_SEPARATED_ONE_OR_MORE     = '#'
        COMMA_SEPARATED_EXACT           = '#{A}'
        COMMA_SEPARATED_AT_LEAST        = '#{A,}'
        COMMA_SEPARATED_BETWEEN         = '#{A,B}'

    def __init__(self):
        self.kind = None
        self.range = None
        self.annotation = None

    def __str__(self):
        if self.annotation:
            return self.stringified_without_annotation + str(self.annotation)
        return self.stringified_without_annotation

    @property
    def stringified_without_annotation(self):
        if self.kind == BNFNodeMultiplier.Kind.ZERO_OR_ONE:
            return '?'
        elif self.kind == BNFNodeMultiplier.Kind.SPACE_SEPARATED_ZERO_OR_MORE:
            return '*'
        elif self.kind == BNFNodeMultiplier.Kind.SPACE_SEPARATED_ONE_OR_MORE:
            return '+'
        elif self.kind == BNFNodeMultiplier.Kind.SPACE_SEPARATED_EXACT:
            return '{' + str(self.range.min) + '}'
        elif self.kind == BNFNodeMultiplier.Kind.SPACE_SEPARATED_AT_LEAST:
            return '{' + str(self.range.min) + ',}'
        elif self.kind == BNFNodeMultiplier.Kind.SPACE_SEPARATED_BETWEEN:
            return '{' + str(self.range.min) + ',' + str(self.range.max) + '}'
        elif self.kind == BNFNodeMultiplier.Kind.COMMA_SEPARATED_ONE_OR_MORE:
            return '#'
        elif self.kind == BNFNodeMultiplier.Kind.COMMA_SEPARATED_EXACT:
            return '#' + '{' + str(self.range.min) + '}'
        elif self.kind == BNFNodeMultiplier.Kind.COMMA_SEPARATED_AT_LEAST:
            return '#' + '{' + str(self.range.min) + ',}'
        elif self.kind == BNFNodeMultiplier.Kind.COMMA_SEPARATED_BETWEEN:
            return '#' + '{' + str(self.range.min) + ',' + str(self.range.max) + '}'
        return ''

    def add(self, multiplier):
        if self.annotation:
            raise Exception("Invalid to stack another multiplier on top of a multiplier that has already received an annotation.")

        if self.kind is None:
            if isinstance(multiplier, BNFRepetitionModifier):
                if multiplier.kind == BNFRepetitionModifier.Kind.EXACT:
                    self.kind = BNFNodeMultiplier.Kind.SPACE_SEPARATED_EXACT
                    self.range = multiplier
                elif multiplier.kind == BNFRepetitionModifier.Kind.AT_LEAST:
                    self.kind = BNFNodeMultiplier.Kind.SPACE_SEPARATED_AT_LEAST
                    self.range = multiplier
                elif multiplier.kind == BNFRepetitionModifier.Kind.BETWEEN:
                    self.kind = BNFNodeMultiplier.Kind.SPACE_SEPARATED_BETWEEN
                    self.range = multiplier
            else:
                self.kind = BNFNodeMultiplier.Kind(multiplier)
        elif self.kind == BNFNodeMultiplier.Kind.ZERO_OR_ONE:
            raise Exception("Invalid to stack another multiplier on top of '?'")
        elif self.kind == BNFNodeMultiplier.Kind.SPACE_SEPARATED_ZERO_OR_MORE:
            raise Exception("Invalid to stack another multiplier on top of '*'")
        elif self.kind == BNFNodeMultiplier.Kind.SPACE_SEPARATED_ONE_OR_MORE:
            raise Exception("Invalid to stack another multiplier on top of '+'")
        elif self.kind == BNFNodeMultiplier.Kind.SPACE_SEPARATED_EXACT:
            raise Exception("Invalid to stack another multiplier on top of a range.")
        elif self.kind == BNFNodeMultiplier.Kind.SPACE_SEPARATED_AT_LEAST:
            raise Exception("Invalid to stack another multiplier on top of a range.")
        elif self.kind == BNFNodeMultiplier.Kind.SPACE_SEPARATED_BETWEEN:
            raise Exception("Invalid to stack another multiplier on top of a range.")
        elif self.kind == BNFNodeMultiplier.Kind.COMMA_SEPARATED_ONE_OR_MORE:
            if isinstance(multiplier, BNFRepetitionModifier):
                if multiplier.kind == BNFRepetitionModifier.Kind.EXACT:
                    self.kind = BNFNodeMultiplier.Kind.COMMA_SEPARATED_EXACT
                    self.range = multiplier
                elif multiplier.kind == BNFRepetitionModifier.Kind.AT_LEAST:
                    self.kind = BNFNodeMultiplier.Kind.COMMA_SEPARATED_AT_LEAST
                    self.range = multiplier
                elif multiplier.kind == BNFRepetitionModifier.Kind.BETWEEN:
                    self.kind = BNFNodeMultiplier.Kind.COMMA_SEPARATED_BETWEEN
                    self.range = multiplier
            else:
                raise Exception("Invalid to stack a non-range multiplier on top of '#'.")
        elif self.kind == BNFNodeMultiplier.Kind.COMMA_SEPARATED_EXACT:
            raise Exception("Invalid to stack another multiplier on top of a comma modifier range multiplier.")
        elif self.kind == BNFNodeMultiplier.Kind.COMMA_SEPARATED_AT_LEAST:
            raise Exception("Invalid to stack another multiplier on top of a comma modifier range multiplier.")
        elif self.kind == BNFNodeMultiplier.Kind.COMMA_SEPARATED_BETWEEN:
            raise Exception("Invalid to stack another multiplier on top of a comma modifier range multiplier.")

    def add_annotation(self, annotation):
        if self.annotation:
            raise Exception("Invalid to add an annotation to a multiplier node that already has an annotation.")

        SUPPORTED_DIRECTIVES = {
            'no-single-item-opt': {
                BNFNodeMultiplier.Kind.SPACE_SEPARATED_ZERO_OR_MORE,
                BNFNodeMultiplier.Kind.SPACE_SEPARATED_ONE_OR_MORE,
                BNFNodeMultiplier.Kind.SPACE_SEPARATED_AT_LEAST,
                BNFNodeMultiplier.Kind.SPACE_SEPARATED_BETWEEN,
                BNFNodeMultiplier.Kind.COMMA_SEPARATED_ONE_OR_MORE,
                BNFNodeMultiplier.Kind.COMMA_SEPARATED_AT_LEAST,
                BNFNodeMultiplier.Kind.COMMA_SEPARATED_BETWEEN,
            },
            'type': {
                BNFNodeMultiplier.Kind.SPACE_SEPARATED_ZERO_OR_MORE,
                BNFNodeMultiplier.Kind.SPACE_SEPARATED_ONE_OR_MORE,
                BNFNodeMultiplier.Kind.SPACE_SEPARATED_BETWEEN,
                BNFNodeMultiplier.Kind.SPACE_SEPARATED_EXACT,
                BNFNodeMultiplier.Kind.COMMA_SEPARATED_ONE_OR_MORE,
                BNFNodeMultiplier.Kind.COMMA_SEPARATED_BETWEEN,
                BNFNodeMultiplier.Kind.COMMA_SEPARATED_EXACT,
            },
            'default': {
                BNFNodeMultiplier.Kind.SPACE_SEPARATED_BETWEEN,
                BNFNodeMultiplier.Kind.COMMA_SEPARATED_BETWEEN,
            },
            'settings-flag' : '*',
        }

        for directive in annotation.directives:
            if directive.name not in SUPPORTED_DIRECTIVES:
                raise Exception(f"Unknown annotation directive '{directive}' for multiplier '{self}'.")
            if SUPPORTED_DIRECTIVES[directive.name] != '*' and self.kind not in SUPPORTED_DIRECTIVES[directive.name]:
                raise Exception(f"Unsupported annotation directive '{directive}' for multiplier '{self}'.")

        self.annotation = annotation


# https://drafts.csswg.org/css-values-4/#component-combinators
class BNFGroupingNode:
    class Kind(enum.Enum):
        MATCH_ALL_ORDERED = ' '                # [ <length>    <integer>    <percentage> ]
        MATCH_ONE = '|'                        # [ <length>  | <integer>  | <percentage> ]
        MATCH_ALL_ANY_ORDER = '&&'             # [ <length> && <integer> && <percentage> ]
        MATCH_ONE_OR_MORE_ANY_ORDER = '||'     # [ <length> || <integer> || <percentage> ]

    def __init__(self, *, is_initial=False):
        self.kind = BNFGroupingNode.Kind.MATCH_ALL_ORDERED
        self.members = []
        self.multiplier = BNFNodeMultiplier()
        self.is_initial = is_initial
        self.annotation = None

    def __str__(self):
        return self.stringified_without_multipliers + str(self.multiplier)

    @property
    def stringified_without_multipliers(self):
        if self.is_initial:
            return self.stringified_without_brackets_or_multipliers
        return '[ ' + self.stringified_without_brackets_or_multipliers + ' ]'

    @property
    def stringified_without_brackets_or_multipliers(self):
        if self.kind != BNFGroupingNode.Kind.MATCH_ALL_ORDERED:
            join_string = ' ' + self.kind.value + ' '
        else:
            join_string = ' '

        return join_string.join(stringify_iterable(self.members))

    def add(self, member):
        self.members.append(member)

    def add_annotation(self, annotation):
        if self.annotation:
            raise Exception("Invalid to add an annotation to a grouping node that already has an annotation.")

        SUPPORTED_DIRECTIVES = {
            'no-single-item-opt': {
                BNFGroupingNode.Kind.MATCH_ALL_ORDERED,
                BNFGroupingNode.Kind.MATCH_ALL_ANY_ORDER,
                BNFGroupingNode.Kind.MATCH_ONE_OR_MORE_ANY_ORDER,
            },
            'preserve-order': {
                BNFGroupingNode.Kind.MATCH_ALL_ANY_ORDER,
                BNFGroupingNode.Kind.MATCH_ONE_OR_MORE_ANY_ORDER,
            },
            'type': {
                BNFGroupingNode.Kind.MATCH_ALL_ORDERED,
                BNFGroupingNode.Kind.MATCH_ALL_ANY_ORDER,
                BNFGroupingNode.Kind.MATCH_ONE_OR_MORE_ANY_ORDER,
            },
            'settings-flag' : '*',
        }

        for directive in annotation.directives:
            if directive.name not in SUPPORTED_DIRECTIVES:
                raise Exception(f"Unknown annotation directive '{directive}' for grouping '{self}'.")
            if SUPPORTED_DIRECTIVES[directive.name] != '*' and self.kind not in SUPPORTED_DIRECTIVES[directive.name]:
                raise Exception(f"Unsupported annotation directive '{directive}' for grouping '{self}'.")

        self.annotation = annotation


# https://drafts.csswg.org/css-values-4/#functional-notation
class BNFFunctionNode:
    def __init__(self, name):
        self.name = name
        self.parameter_group = BNFGroupingNode()
        self.multiplier = BNFNodeMultiplier()
        self.annotation = None

    def __str__(self):
        return self.stringified_without_multipliers + str(self.multiplier)

    @property
    def stringified_without_multipliers(self):
        return self.name + '(' + self.parameter_group.stringified_without_brackets_or_multipliers + ')'

    @property
    def kind(self):
        return self.parameter_group.kind

    @kind.setter
    def kind(self, kind):
        self.parameter_group.kind = kind

    def add(self, member):
        self.parameter_group.add(member)

    def add_annotation(self, annotation):
        if self.annotation:
            raise Exception("Invalid to add an annotation to a function node that already has an annotation.")

        SUPPORTED_DIRECTIVES = {
            'settings-flag'
        }

        for directive in annotation.directives:
            if directive.name not in SUPPORTED_DIRECTIVES:
                raise Exception(f"Unknown annotation directive '{directive}' for function node '{self}'.")

        self.annotation = annotation


class BNFReferenceNode:
    class StringAttribute:
        def __init__(self, name):
            self.name = name
            self.value = []

        def __str__(self):
            if self.value:
                return str(self.name) + '=' + str(self.value)
            return str(self.name)

    class RangeAttribute:
        def __init__(self):
            self.min = None
            self.max = None

        def __str__(self):
            return '[' + str(self.min) + ',' + str(self.max) + ']'

    def __init__(self, *, is_internal=False):
        self.name = None
        self.is_internal = is_internal
        self.is_function_reference = False
        self.attributes = []
        self.multiplier = BNFNodeMultiplier()
        self.annotation = None

    def __str__(self):
        return self.stringified_without_multipliers + str(self.multiplier)

    @property
    def stringified_without_multipliers(self):
        if self.is_internal:
            prefix = '<<'
            suffix = '>>'
        else:
            prefix = '<'
            suffix = '>'

        if self.is_function_reference:
            name = self.name + '()'
        else:
            name = self.name

        if self.attributes:
            return prefix + str(name) + ' ' + ' '.join(stringify_iterable(self.attributes)) + suffix
        return prefix + str(name) + suffix

    def add_attribute(self, attribute):
        self.attributes.append(attribute)

    def add_annotation(self, annotation):
        if self.annotation:
            raise Exception("Invalid to add an annotation to a reference node that already has an annotation.")

        SUPPORTED_DIRECTIVES = {
            'settings-flag'
        }

        for directive in annotation.directives:
            if directive.name not in SUPPORTED_DIRECTIVES:
                raise Exception(f"Unknown annotation directive '{directive}' for reference node '{self}'.")

        self.annotation = annotation


class BNFKeywordNode:
    def __init__(self, keyword):
        self.keyword = keyword
        self.multiplier = BNFNodeMultiplier()
        self.annotation = None

    def __str__(self):
        return self.stringified_without_multipliers + str(self.multiplier)

    @property
    def stringified_without_multipliers(self):
        return self.keyword

    def add_annotation(self, annotation):
        if self.annotation:
            raise Exception("Invalid to add an annotation to a keyword node that already has an annotation.")

        SUPPORTED_DIRECTIVES = {
            'aliased-to',
            'settings-flag',
        }

        for directive in annotation.directives:
            if directive.name not in SUPPORTED_DIRECTIVES:
                raise Exception(f"Unknown annotation directive '{directive}' for keyword '{self}'.")

        self.annotation = annotation


class BNFLiteralNode:
    def __init__(self, value=None):
        self.value = value
        self.multiplier = BNFNodeMultiplier()
        self.annotation = None

    def __str__(self):
        return self.stringified_without_multipliers + str(self.multiplier)

    @property
    def stringified_without_multipliers(self):
        return str(self.value)

    def add_annotation(self, annotation):
        if self.annotation:
            raise Exception("Invalid to add an annotation to a literal node that already has an annotation.")

        SUPPORTED_DIRECTIVES = {}

        for directive in annotation.directives:
            if directive.name not in SUPPORTED_DIRECTIVES:
                raise Exception(f"Unknown annotation directive '{directive}' for literal '{self}'.")

        self.annotation = annotation


class BNFParserState(enum.Enum):
    UNKNOWN_GROUPING_INITIAL = enum.auto()
    UNKNOWN_GROUPING_SEEN_TERM = enum.auto()
    KNOWN_ORDERED_GROUPING = enum.auto()
    KNOWN_COMBINATOR_GROUPING_TERM_REQUIRED = enum.auto()
    KNOWN_COMBINATOR_GROUPING_COMBINATOR_OR_CLOSE_REQUIRED = enum.auto()
    INTERNAL_REFERENCE_INITIAL = enum.auto()
    INTERNAL_REFERENCE_SEEN_ID = enum.auto()
    REFERENCE_INITIAL = enum.auto()
    REFERENCE_SEEN_QUOTE_OPEN = enum.auto()
    REFERENCE_SEEN_QUOTE_AND_ID = enum.auto()
    REFERENCE_SEEN_FUNCTION_OPEN = enum.auto()
    REFERENCE_SEEN_ID_OR_FUNCTION = enum.auto()
    REFERENCE_STRING_ATTRIBUTE_INITIAL = enum.auto()
    REFERENCE_STRING_ATTRIBUTE_SEEN_EQUAL = enum.auto()
    REFERENCE_STRING_ATTRIBUTE_SEEN_VALUE = enum.auto()
    REFERENCE_RANGE_ATTRIBUTE_INITIAL = enum.auto()
    REFERENCE_RANGE_ATTRIBUTE_SEEN_MIN = enum.auto()
    REFERENCE_RANGE_ATTRIBUTE_SEEN_MIN_AND_COMMA = enum.auto()
    REFERENCE_RANGE_ATTRIBUTE_SEEN_MAX = enum.auto()
    REPETITION_MODIFIER_INITIAL = enum.auto()
    REPETITION_MODIFIER_SEEN_MIN = enum.auto()
    REPETITION_MODIFIER_SEEN_MIN_AND_COMMA = enum.auto()
    REPETITION_MODIFIER_SEEN_MAX = enum.auto()
    QUOTED_LITERAL_INITIAL = enum.auto()
    QUOTED_LITERAL_SEEN_ID = enum.auto()
    ANNOTATION_INITIAL = enum.auto()
    ANNOTATION_SEEN_ID = enum.auto()
    ANNOTATION_SEEN_EQUAL_OR_COMMA = enum.auto()
    ANNOTATION_SEEN_VALUE = enum.auto()
    DONE = enum.auto()


BNFParserStateInfo = collections.namedtuple("BNFParserStates", ["state", "node", "node_owner"])


class BNFParser:
    COMBINATOR_FOR_TOKEN = {
        BNFToken.OR.name: BNFGroupingNode.Kind.MATCH_ONE,
        BNFToken.OROR.name: BNFGroupingNode.Kind.MATCH_ONE_OR_MORE_ANY_ORDER,
        BNFToken.ANDAND.name: BNFGroupingNode.Kind.MATCH_ALL_ANY_ORDER,
    }

    SIMPLE_MULTIPLIERS = {
        BNFToken.HASH.name,
        BNFToken.PLUS.name,
        BNFToken.STAR.name,
        BNFToken.NOT.name,
        BNFToken.QMARK.name,
    }

    SUPPORTED_UNQUOTED_LITERALS = {
        BNFToken.COMMA.name,
        BNFToken.SLASH.name,
    }

    DEBUG_PRINT_STATE = 0
    DEBUG_PRINT_TOKENS = 0

    def __init__(self, parsing_context, key_path, data):
        self.parsing_context = parsing_context
        self.key_path = key_path
        self.data = data
        self.root = BNFGroupingNode(is_initial=True)
        self.state_stack = []
        self.multiplier_target = None
        self.annotation_target = None
        self.enter_initial_grouping()

    def parse(self):
        PARSER_THUNKS = {
            BNFParserState.UNKNOWN_GROUPING_INITIAL: BNFParser.parse_UNKNOWN_GROUPING_INITIAL,
            BNFParserState.UNKNOWN_GROUPING_SEEN_TERM: BNFParser.parse_UNKNOWN_GROUPING_SEEN_TERM,
            BNFParserState.KNOWN_ORDERED_GROUPING: BNFParser.parse_KNOWN_ORDERED_GROUPING,
            BNFParserState.KNOWN_COMBINATOR_GROUPING_TERM_REQUIRED: BNFParser.parse_KNOWN_COMBINATOR_GROUPING_TERM_REQUIRED,
            BNFParserState.KNOWN_COMBINATOR_GROUPING_COMBINATOR_OR_CLOSE_REQUIRED: BNFParser.parse_KNOWN_COMBINATOR_GROUPING_COMBINATOR_OR_CLOSE_REQUIRED,
            BNFParserState.INTERNAL_REFERENCE_INITIAL: BNFParser.parse_INTERNAL_REFERENCE_INITIAL,
            BNFParserState.INTERNAL_REFERENCE_SEEN_ID: BNFParser.parse_INTERNAL_REFERENCE_SEEN_ID,
            BNFParserState.REFERENCE_INITIAL: BNFParser.parse_REFERENCE_INITIAL,
            BNFParserState.REFERENCE_SEEN_QUOTE_OPEN: BNFParser.parse_REFERENCE_SEEN_QUOTE_OPEN,
            BNFParserState.REFERENCE_SEEN_QUOTE_AND_ID: BNFParser.parse_REFERENCE_SEEN_QUOTE_AND_ID,
            BNFParserState.REFERENCE_SEEN_FUNCTION_OPEN: BNFParser.parse_REFERENCE_SEEN_FUNCTION_OPEN,
            BNFParserState.REFERENCE_SEEN_ID_OR_FUNCTION: BNFParser.parse_REFERENCE_SEEN_ID_OR_FUNCTION,
            BNFParserState.REFERENCE_STRING_ATTRIBUTE_INITIAL: BNFParser.parse_REFERENCE_STRING_ATTRIBUTE_INITIAL,
            BNFParserState.REFERENCE_STRING_ATTRIBUTE_SEEN_EQUAL: BNFParser.parse_REFERENCE_STRING_ATTRIBUTE_SEEN_EQUAL,
            BNFParserState.REFERENCE_STRING_ATTRIBUTE_SEEN_VALUE: BNFParser.parse_REFERENCE_STRING_ATTRIBUTE_SEEN_VALUE,
            BNFParserState.REFERENCE_RANGE_ATTRIBUTE_INITIAL: BNFParser.parse_REFERENCE_RANGE_ATTRIBUTE_INITIAL,
            BNFParserState.REFERENCE_RANGE_ATTRIBUTE_SEEN_MIN: BNFParser.parse_REFERENCE_RANGE_ATTRIBUTE_SEEN_MIN,
            BNFParserState.REFERENCE_RANGE_ATTRIBUTE_SEEN_MIN_AND_COMMA: BNFParser.parse_REFERENCE_RANGE_ATTRIBUTE_SEEN_MIN_AND_COMMA,
            BNFParserState.REFERENCE_RANGE_ATTRIBUTE_SEEN_MAX: BNFParser.parse_REFERENCE_RANGE_ATTRIBUTE_SEEN_MAX,
            BNFParserState.REPETITION_MODIFIER_INITIAL: BNFParser.parse_REPETITION_MODIFIER_INITIAL,
            BNFParserState.REPETITION_MODIFIER_SEEN_MIN: BNFParser.parse_REPETITION_MODIFIER_SEEN_MIN,
            BNFParserState.REPETITION_MODIFIER_SEEN_MIN_AND_COMMA: BNFParser.parse_REPETITION_MODIFIER_SEEN_MIN_AND_COMMA,
            BNFParserState.REPETITION_MODIFIER_SEEN_MAX: BNFParser.parse_REPETITION_MODIFIER_SEEN_MAX,
            BNFParserState.QUOTED_LITERAL_INITIAL: BNFParser.parse_QUOTED_LITERAL_INITIAL,
            BNFParserState.QUOTED_LITERAL_SEEN_ID: BNFParser.parse_QUOTED_LITERAL_SEEN_ID,
            BNFParserState.ANNOTATION_INITIAL: BNFParser.parse_ANNOTATION_INITIAL,
            BNFParserState.ANNOTATION_SEEN_ID: BNFParser.parse_ANNOTATION_SEEN_ID,
            BNFParserState.ANNOTATION_SEEN_EQUAL_OR_COMMA: BNFParser.parse_ANNOTATION_SEEN_EQUAL_OR_COMMA,
            BNFParserState.ANNOTATION_SEEN_VALUE: BNFParser.parse_ANNOTATION_SEEN_VALUE,
        }

        for token in BNFLexer(self.data):
            if token.name == BNF_ILLEGAL_TOKEN:
                raise Exception(f"Illegal token found while parsing grammar definition: {token}")

            state = self.state_stack[-1]

            if BNFParser.DEBUG_PRINT_STATE:
                print("STATE: " + state.state.name + " " + str(state.node))
            if BNFParser.DEBUG_PRINT_TOKENS:
                print("TOKEN: " + str(token))
            PARSER_THUNKS[state.state](self, token, state)

        if self.state_stack[-1].state != BNFParserState.DONE:
            raise Exception(f"Unexpected state '{state.state.name}' after processing all tokens")

        return self.root

    def transition_top(self, *, to):
        self.state_stack[-1] = BNFParserStateInfo(to, self.state_stack[-1].node, self.state_stack[-1].node_owner)

    def push(self, new_state, new_node, node_owner):
        self.state_stack.append(BNFParserStateInfo(new_state, new_node, node_owner))

    def pop(self):
        self.state_stack.pop()

    @property
    def top(self):
        return self.state_stack[-1]

    def unexpected(self, token, state):
        return Exception(f"Unexpected token '{token}' found while in state '{state.state.name}' while parsing '{self.data}'")

    # COMMON ACTIONS.

    # Root BNFGroupingNode. Syntactically isn't surrounded by square brackets.
    def enter_initial_grouping(self):
        self.push(BNFParserState.UNKNOWN_GROUPING_INITIAL, self.root, None)
        return self.top

    def exit_initial_grouping(self, token, state):
        if isinstance(state.node, BNFGroupingNode) and state.node.is_initial:
            self.transition_top(to=BNFParserState.DONE)
            return self.top
        raise self.unexpected(token, state)

    # Non-initial BNFGroupingNode. e.g. "[foo bar]", "[foo | bar]", etc.
    def enter_new_grouping(self, token, state):
        self.push(BNFParserState.UNKNOWN_GROUPING_INITIAL, BNFGroupingNode(), self.top.node)
        self.multiplier_target = None
        self.annotation_target = None
        return self.top

    def exit_grouping(self, token, state):
        if isinstance(state.node, BNFGroupingNode) and not state.node.is_initial:
            self.pop()
            state.node_owner.add(state.node)
            self.multiplier_target = state.node
            self.annotation_target = state.node
            return self.top
        raise self.unexpected(token, state)

    # BNFFunctionNode. e.g. "foo(<bar>)"
    def enter_new_function(self, token, state):
        self.push(BNFParserState.UNKNOWN_GROUPING_INITIAL, BNFFunctionNode(token.value[:-1]), self.top.node)
        self.multiplier_target = None
        self.annotation_target = None
        return self.top

    def exit_function(self, token, state):
        if isinstance(state.node, BNFFunctionNode):
            self.pop()
            state.node_owner.add(state.node)
            self.multiplier_target = state.node
            self.annotation_target = state.node
            return self.top
        raise self.unexpected(token, state)

    # Internal BNFReferenceNodes. e.g. "<<values>>"
    def enter_new_internal_reference(self, token, state):
        self.push(BNFParserState.INTERNAL_REFERENCE_INITIAL, BNFReferenceNode(is_internal=True), self.top.node)
        self.multiplier_target = None
        self.annotation_target = None
        return self.top

    def exit_internal_reference(self, token, state):
        if isinstance(state.node, BNFReferenceNode) and state.node.is_internal:
            self.pop()
            state.node_owner.add(state.node)
            self.multiplier_target = state.node
            self.annotation_target = state.node
            return self.top
        raise self.unexpected(token, state)

    # Non-internal BNFReferenceNodes. e.g. "<length>"
    def enter_new_reference(self, token, state):
        self.push(BNFParserState.REFERENCE_INITIAL, BNFReferenceNode(), self.top.node)
        self.multiplier_target = None
        self.annotation_target = None
        return self.top

    def exit_reference(self, token, state):
        if isinstance(state.node, BNFReferenceNode) and not state.node.is_internal:
            self.pop()
            state.node_owner.add(state.node)
            self.multiplier_target = state.node
            self.annotation_target = state.node
            return self.top
        raise self.unexpected(token, state)

    # BNFRepetitionModifier. e.g. {A,B}
    def enter_new_repetition_modifier(self, token, state):
        self.push(BNFParserState.REPETITION_MODIFIER_INITIAL, BNFRepetitionModifier(), self.multiplier_target)
        self.annotation_target = None
        return self.top

    def exit_repetition_modifier(self, token, state):
        if isinstance(state.node, BNFRepetitionModifier):
            self.pop()
            state.node_owner.multiplier.add(state.node)
            self.multiplier_target = None
            self.annotation_target = state.node_owner.multiplier
            return self.top
        raise self.unexpected(token, state)

    # BNFReferenceNode.StringAttribute. e.g. allows-quirks or excludes=auto,none
    def enter_new_string_attribute(self, token, state):
        self.push(BNFParserState.REFERENCE_STRING_ATTRIBUTE_INITIAL, BNFReferenceNode.StringAttribute(token.value), self.top.node)
        self.multiplier_target = None
        self.annotation_target = None
        return self.top

    def exit_string_attribute(self, token, state):
        if isinstance(state.node, BNFReferenceNode.StringAttribute):
            self.pop()
            state.node_owner.add_attribute(state.node)
            self.multiplier_target = None
            self.annotation_target = None  # FIXME: Consider adding support for annotations to attributes.
            return self.top
        raise self.unexpected(token, state)

    # BNFReferenceNode.RangeAttribute. e.g. [0,inf]
    def enter_new_range_attribute(self, token, state):
        self.multiplier_target = None
        self.annotation_target = None
        self.push(BNFParserState.REFERENCE_RANGE_ATTRIBUTE_INITIAL, BNFReferenceNode.RangeAttribute(), self.top.node)
        return self.top

    def exit_range_attribute(self, token, state):
        if isinstance(state.node, BNFReferenceNode.RangeAttribute):
            self.pop()
            state.node_owner.add_attribute(state.node)
            self.multiplier_target = None
            self.annotation_target = None  # FIXME: Consider adding support for annotations to attributes.
            return self.top
        raise self.unexpected(token, state)

    # BNFLiteralNode. e.g. '['
    def enter_new_quoted_literal(self, token, state):
        self.push(BNFParserState.QUOTED_LITERAL_INITIAL, BNFLiteralNode(), self.top.node)
        self.multiplier_target = None
        self.annotation_target = None
        return self.top

    def exit_quoted_literal(self, token, state):
        if isinstance(state.node, BNFLiteralNode):
            self.pop()
            state.node_owner.add(state.node)
            self.multiplier_target = state.node
            self.annotation_target = state.node
            return self.top
        raise self.unexpected(token, state)

    # BNFAnnotation. e.g. @(foo=bar,baz bat)
    def enter_new_annotation(self, token, state):
        self.push(BNFParserState.ANNOTATION_INITIAL, BNFAnnotation(), self.annotation_target)
        self.annotation_target = None
        # NOTE: self.multiplier_target is not cleared here, as you may have `<foo>@(bar){2}` where the {2} associates to <foo> not @(bar).
        return self.top

    def exit_annotation(self, token, state):
        if isinstance(state.node, BNFAnnotation):
            self.pop()
            state.node_owner.add_annotation(state.node)
            self.annotation_target = None
            return self.top
        raise self.unexpected(token, state)

    # BNFAnnotation.Directive. e.g. no-single-item-opt or settings-flag=cssFooEnabled
    def enter_new_directive(self, token, state):
        self.push(BNFParserState.ANNOTATION_SEEN_ID, BNFAnnotation.Directive(token.value), self.top.node)
        return self.top

    def exit_directive(self, token, state):
        if isinstance(state.node, BNFAnnotation.Directive):
            self.pop()
            state.node_owner.add_directive(state.node)
            return self.top
        raise self.unexpected(token, state)

    def process_keyword(self, token, state):
        keyword = BNFKeywordNode(token.value)
        state.node.add(keyword)
        self.multiplier_target = keyword
        self.annotation_target = keyword

    def process_unquoted_literal(self, token, state):
        literal = BNFLiteralNode(token.value)
        state.node.add(literal)
        self.multiplier_target = literal
        self.annotation_target = literal

    def process_simple_multiplier(self, token, state):
        self.multiplier_target.multiplier.add(token.value)
        self.annotation_target = self.multiplier_target.multiplier

    def process_combinator(self, token, state, known_kind):
        if known_kind and known_kind != BNFParser.COMBINATOR_FOR_TOKEN[token.name]:
            raise Exception(f"Unexpected token '{token}'. Did you mean '{state.node.kind.name}'?.")

        state.node.kind = BNFParser.COMBINATOR_FOR_TOKEN[token.name]
        self.multiplier_target = None
        self.annotation_target = None

    # MARK: Parsing Thunks.

    def parse_UNKNOWN_GROUPING_INITIAL(self, token, state):
        if token.name == BNFToken.LSQUARE:
            self.transition_top(to=BNFParserState.UNKNOWN_GROUPING_SEEN_TERM)
            self.enter_new_grouping(token, state)
            return

        if token.name == BNFToken.LTLT:
            self.transition_top(to=BNFParserState.UNKNOWN_GROUPING_SEEN_TERM)
            self.enter_new_internal_reference(token, state)
            return

        if token.name == BNFToken.LT:
            self.transition_top(to=BNFParserState.UNKNOWN_GROUPING_SEEN_TERM)
            self.enter_new_reference(token, state)
            return

        if token.name == BNFToken.FUNC:
            self.transition_top(to=BNFParserState.UNKNOWN_GROUPING_SEEN_TERM)
            self.enter_new_function(token, state)
            return

        if token.name == BNFToken.ID:
            self.transition_top(to=BNFParserState.UNKNOWN_GROUPING_SEEN_TERM)
            self.process_keyword(token, state)
            return

        if token.name == BNFToken.SQUOTE:
            self.transition_top(to=BNFParserState.UNKNOWN_GROUPING_SEEN_TERM)
            self.enter_new_quoted_literal(token, state)
            return

        if token.name in BNFParser.SUPPORTED_UNQUOTED_LITERALS:
            self.transition_top(to=BNFParserState.UNKNOWN_GROUPING_SEEN_TERM)
            self.process_unquoted_literal(token, state)
            return

        if token.name == BNFToken.RPAREN:
            self.transition_top(to=BNFParserState.UNKNOWN_GROUPING_SEEN_TERM)
            self.exit_function(token, state)
            return

        raise self.unexpected(token, state)

    def parse_UNKNOWN_GROUPING_SEEN_TERM(self, token, state):
        if token.name == BNFToken.RSQUARE:
            self.exit_grouping(token, state)
            return

        if token.name == BNF_EOF_TOKEN:
            self.exit_initial_grouping(token, state)
            return

        if token.name == BNFToken.RPAREN:
            self.exit_function(token, state)
            return

        if token.name == BNFToken.LSQUARE:
            self.transition_top(to=BNFParserState.KNOWN_ORDERED_GROUPING)
            self.enter_new_grouping(token, state)
            return

        if token.name == BNFToken.LTLT:
            self.transition_top(to=BNFParserState.KNOWN_ORDERED_GROUPING)
            self.enter_new_internal_reference(token, state)
            return

        if token.name == BNFToken.LT:
            self.transition_top(to=BNFParserState.KNOWN_ORDERED_GROUPING)
            self.enter_new_reference(token, state)
            return

        if token.name == BNFToken.FUNC:
            self.transition_top(to=BNFParserState.KNOWN_ORDERED_GROUPING)
            self.enter_new_function(token, state)
            return

        if token.name == BNFToken.ID:
            self.transition_top(to=BNFParserState.KNOWN_ORDERED_GROUPING)
            self.process_keyword(token, state)
            return

        if token.name == BNFToken.SQUOTE:
            self.transition_top(to=BNFParserState.KNOWN_ORDERED_GROUPING)
            self.enter_new_quoted_literal(token, state)
            return

        if token.name in BNFParser.SUPPORTED_UNQUOTED_LITERALS:
            self.transition_top(to=BNFParserState.KNOWN_ORDERED_GROUPING)
            self.process_unquoted_literal(token, state)
            return

        if token.name in BNFParser.COMBINATOR_FOR_TOKEN:
            self.transition_top(to=BNFParserState.KNOWN_COMBINATOR_GROUPING_TERM_REQUIRED)
            self.process_combinator(token, state, None)
            return

        if token.name in BNFParser.SIMPLE_MULTIPLIERS:
            self.process_simple_multiplier(token, state)
            return

        if token.name == BNFToken.ATPAREN:
            self.enter_new_annotation(token, state)
            return

        if token.name == BNFToken.LBRACE:
            self.enter_new_repetition_modifier(token, state)
            return

        raise self.unexpected(token, state)

    def parse_KNOWN_ORDERED_GROUPING(self, token, state):
        if token.name == BNFToken.RSQUARE:
            self.exit_grouping(token, state)
            return

        if token.name == BNF_EOF_TOKEN:
            self.exit_initial_grouping(token, state)
            return

        if token.name == BNFToken.RPAREN:
            self.exit_function(token, state)
            return

        if token.name == BNFToken.LSQUARE:
            self.enter_new_grouping(token, state)
            return

        if token.name == BNFToken.LTLT:
            self.enter_new_internal_reference(token, state)
            return

        if token.name == BNFToken.LT:
            self.enter_new_reference(token, state)
            return

        if token.name == BNFToken.ID:
            self.process_keyword(token, state)
            return

        if token.name == BNFToken.SQUOTE:
            self.enter_new_quoted_literal(token, state)
            return

        if token.name in BNFParser.SUPPORTED_UNQUOTED_LITERALS:
            self.process_unquoted_literal(token, state)
            return

        if token.name == BNFToken.FUNC:
            self.enter_new_function(token, state)
            return

        if token.name == BNFToken.LBRACE:
            self.enter_new_repetition_modifier(token, state)
            return

        if token.name in BNFParser.SIMPLE_MULTIPLIERS:
            self.process_simple_multiplier(token, state)
            return

        if token.name == BNFToken.ATPAREN:
            self.enter_new_annotation(token, state)
            return

        raise self.unexpected(token, state)

    def parse_KNOWN_COMBINATOR_GROUPING_TERM_REQUIRED(self, token, state):
        if token.name == BNFToken.LSQUARE:
            self.transition_top(to=BNFParserState.KNOWN_COMBINATOR_GROUPING_COMBINATOR_OR_CLOSE_REQUIRED)
            self.enter_new_grouping(token, state)
            return

        if token.name == BNFToken.LTLT:
            self.transition_top(to=BNFParserState.KNOWN_COMBINATOR_GROUPING_COMBINATOR_OR_CLOSE_REQUIRED)
            self.enter_new_internal_reference(token, state)
            return

        if token.name == BNFToken.LT:
            self.transition_top(to=BNFParserState.KNOWN_COMBINATOR_GROUPING_COMBINATOR_OR_CLOSE_REQUIRED)
            self.enter_new_reference(token, state)
            return

        if token.name == BNFToken.FUNC:
            self.transition_top(to=BNFParserState.KNOWN_COMBINATOR_GROUPING_COMBINATOR_OR_CLOSE_REQUIRED)
            self.enter_new_function(token, state)
            return

        if token.name == BNFToken.ID:
            self.transition_top(to=BNFParserState.KNOWN_COMBINATOR_GROUPING_COMBINATOR_OR_CLOSE_REQUIRED)
            self.process_keyword(token, state)
            return

        # FIXME: Does it make any sense to support literals here? e.g. [ <foo> && , ]

        if token.name == BNFToken.RSQUARE or token.name == BNFToken.FUNC or token.name == EOF:
            raise Exception(f"Unexpected token '{token}'. Groupings can't end in a combinator.")
        raise self.unexpected(token, state)

    def parse_KNOWN_COMBINATOR_GROUPING_COMBINATOR_OR_CLOSE_REQUIRED(self, token, state):
        if token.name == BNFToken.RSQUARE:
            self.exit_grouping(token, state)
            return

        if token.name == BNF_EOF_TOKEN:
            self.exit_initial_grouping(token, state)
            return

        if token.name == BNFToken.RPAREN:
            self.exit_function(token, state)
            return

        if token.name in BNFParser.COMBINATOR_FOR_TOKEN:
            self.transition_top(to=BNFParserState.KNOWN_COMBINATOR_GROUPING_TERM_REQUIRED)
            self.process_combinator(token, state, state.node.kind)
            return

        if token.name in BNFParser.SIMPLE_MULTIPLIERS:
            self.process_simple_multiplier(token, state)
            return

        if token.name == BNFToken.ATPAREN:
            self.enter_new_annotation(token, state)
            return

        if token.name == BNFToken.LBRACE:
            self.enter_new_repetition_modifier(token, state)
            return

        raise self.unexpected(token, state)

    def parse_REFERENCE_INITIAL(self, token, state):
        if token.name == BNFToken.FUNC:
            self.transition_top(to=BNFParserState.REFERENCE_SEEN_FUNCTION_OPEN)
            state.node.is_function_reference = True
            state.node.name = token.value[:-1]
            return

        if token.name == BNFToken.ID:
            self.transition_top(to=BNFParserState.REFERENCE_SEEN_ID_OR_FUNCTION)
            state.node.name = token.value
            return

        if token.name == BNFToken.SQUOTE:
            self.transition_top(to=BNFParserState.REFERENCE_SEEN_QUOTE_OPEN)
            return

        if token.name == BNFToken.ID:
            self.transition_top(to=BNFParserState.REFERENCE_SEEN_ID_OR_FUNCTION)
            state.node.name = token.value
            return

        raise self.unexpected(token, state)

    def parse_REFERENCE_SEEN_QUOTE_OPEN(self, token, state):
        if token.name == BNFToken.ID:
            self.transition_top(to=BNFParserState.REFERENCE_SEEN_QUOTE_AND_ID)
            state.node.name = "'" + token.value + "'"
            return

        raise self.unexpected(token, state)

    def parse_REFERENCE_SEEN_QUOTE_AND_ID(self, token, state):
        if token.name == BNFToken.SQUOTE:
            self.transition_top(to=BNFParserState.REFERENCE_SEEN_ID_OR_FUNCTION)
            return

        raise self.unexpected(token, state)

    def parse_REFERENCE_SEEN_FUNCTION_OPEN(self, token, state):
        if token.name == BNFToken.RPAREN:
            self.transition_top(to=BNFParserState.REFERENCE_SEEN_ID_OR_FUNCTION)
            return

        raise self.unexpected(token, state)

    def parse_REFERENCE_SEEN_ID_OR_FUNCTION(self, token, state):
        if token.name == BNFToken.ID:
            self.enter_new_string_attribute(token, state)
            return

        if token.name == BNFToken.LSQUARE:
            self.enter_new_range_attribute(token, state)
            return

        if token.name == BNFToken.GT:
            self.exit_reference(token, state)
            return

        raise self.unexpected(token, state)

    def parse_INTERNAL_REFERENCE_INITIAL(self, token, state):
        if token.name == BNFToken.ID:
            self.transition_top(to=BNFParserState.INTERNAL_REFERENCE_SEEN_ID)
            state.node.name = token.value
            return

        raise self.unexpected(token, state)

    def parse_INTERNAL_REFERENCE_SEEN_ID(self, token, state):
        if token.name == BNFToken.ID:
            self.enter_new_string_attribute(token, state)
            return

        if token.name == BNFToken.LSQUARE:
            self.enter_new_range_attribute(token, state)
            return

        if token.name == BNFToken.GTGT:
            self.exit_internal_reference(token, state)
            return

        raise self.unexpected(token, state)

    def parse_REFERENCE_STRING_ATTRIBUTE_INITIAL(self, token, state):
        if token.name == BNFToken.EQUAL:
            self.transition_top(to=BNFParserState.REFERENCE_STRING_ATTRIBUTE_SEEN_EQUAL)
            return

        state = self.exit_string_attribute(token, state)

        if token.name == BNFToken.ID:
            self.enter_new_string_attribute(token, state)
            return

        if token.name == BNFToken.LSQUARE:
            self.enter_new_range_attribute(token, state)
            return

        if token.name == BNFToken.GT:
            self.exit_reference(token, state)
            return

        if token.name == BNFToken.GTGT:
            self.exit_internal_reference(token, state)
            return

        raise self.unexpected(token, state)

    def parse_REFERENCE_STRING_ATTRIBUTE_SEEN_EQUAL(self, token, state):
        if token.name == BNFToken.ID:
            self.transition_top(to=BNFParserState.REFERENCE_STRING_ATTRIBUTE_SEEN_VALUE)
            state.node.value.append(token.value)
            return

        raise self.unexpected(token, state)

    def parse_REFERENCE_STRING_ATTRIBUTE_SEEN_VALUE(self, token, state):
        if token.name == BNFToken.COMMA:
            self.transition_top(to=BNFParserState.REFERENCE_STRING_ATTRIBUTE_SEEN_EQUAL)
            return

        state = self.exit_string_attribute(token, state)

        if token.name == BNFToken.ID:
            self.enter_new_string_attribute(token, state)
            return

        if token.name == BNFToken.LSQUARE:
            self.enter_new_range_attribute(token, state)
            return

        if token.name == BNFToken.GT:
            self.exit_reference(token, state)
            return

        if token.name == BNFToken.GTGT:
            self.exit_internal_reference(token, state)
            return

        raise self.unexpected(token, state)

    def parse_REFERENCE_RANGE_ATTRIBUTE_INITIAL(self, token, state):
        if token.name == BNFToken.INT or token.name == BNFToken.FLOAT or (token.name == BNFToken.ID and token.value == '-inf'):
            self.transition_top(to=BNFParserState.REFERENCE_RANGE_ATTRIBUTE_SEEN_MIN)
            state.node.min = token.value
            return

        raise self.unexpected(token, state)

    def parse_REFERENCE_RANGE_ATTRIBUTE_SEEN_MIN(self, token, state):
        if token.name == BNFToken.COMMA:
            self.transition_top(to=BNFParserState.REFERENCE_RANGE_ATTRIBUTE_SEEN_MIN_AND_COMMA)
            return

        raise self.unexpected(token, state)

    def parse_REFERENCE_RANGE_ATTRIBUTE_SEEN_MIN_AND_COMMA(self, token, state):
        if token.name == BNFToken.INT or token.name == BNFToken.FLOAT or (token.name == BNFToken.ID and token.value == 'inf'):
            self.transition_top(to=BNFParserState.REFERENCE_RANGE_ATTRIBUTE_SEEN_MAX)
            state.node.max = token.value
            return

        raise self.unexpected(token, state)

    def parse_REFERENCE_RANGE_ATTRIBUTE_SEEN_MAX(self, token, state):
        if token.name == BNFToken.RSQUARE:
            self.exit_range_attribute(token, state)
            return

        raise self.unexpected(token, state)

    def parse_REPETITION_MODIFIER_INITIAL(self, token, state):
        if token.name == BNFToken.INT:
            self.transition_top(to=BNFParserState.REPETITION_MODIFIER_SEEN_MIN)
            state.node.kind = BNFRepetitionModifier.Kind.EXACT
            state.node.min = int(token.value)
            return

        raise self.unexpected(token, state)

    def parse_REPETITION_MODIFIER_SEEN_MIN(self, token, state):
        if token.name == BNFToken.COMMA:
            self.transition_top(to=BNFParserState.REPETITION_MODIFIER_SEEN_MIN_AND_COMMA)
            state.node.kind = BNFRepetitionModifier.Kind.AT_LEAST
            return

        if token.name == BNFToken.RBRACE:
            self.exit_repetition_modifier(token, state)
            return

        raise self.unexpected(token, state)

    def parse_REPETITION_MODIFIER_SEEN_MIN_AND_COMMA(self, token, state):
        if token.name == BNFToken.INT:
            self.transition_top(to=BNFParserState.REPETITION_MODIFIER_SEEN_MAX)
            state.node.kind = BNFRepetitionModifier.Kind.BETWEEN
            state.node.max = int(token.value)
            return

        if token.name == BNFToken.RBRACE:
            self.exit_repetition_modifier(token, state)
            return

        raise self.unexpected(token, state)

    def parse_REPETITION_MODIFIER_SEEN_MAX(self, token, state):
        if token.name == BNFToken.RBRACE:
            self.exit_repetition_modifier(token, state)
            return

        raise self.unexpected(token, state)

    def parse_QUOTED_LITERAL_INITIAL(self, token, state):
        # Take the value regardless of token name.
        self.transition_top(to=BNFParserState.QUOTED_LITERAL_SEEN_ID)
        state.node.value = token.value

    def parse_QUOTED_LITERAL_SEEN_ID(self, token, state):
        if token.name == BNFToken.SQUOTE:
            self.exit_quoted_literal(token, state)
            return

        # Append the value regardless of the token value.
        state.node.value = state.node.value + token.value

    def parse_ANNOTATION_INITIAL(self, token, state):
        if token.name == BNFToken.ID:
            self.enter_new_directive(token, state)
            return

        raise self.unexpected(token, state)

    def parse_ANNOTATION_SEEN_ID(self, token, state):
        if token.name == BNFToken.EQUAL:
            self.transition_top(to=BNFParserState.ANNOTATION_SEEN_EQUAL_OR_COMMA)
            return

        state = self.exit_directive(token, state)

        if token.name == BNFToken.ID:
            self.enter_new_directive(token, state)
            return

        if token.name == BNFToken.RPAREN:
            self.exit_annotation(token, state)
            return

        raise self.unexpected(token, state)

    def parse_ANNOTATION_SEEN_EQUAL_OR_COMMA(self, token, state):
        if token.name == BNFToken.ID:
            state.node.value.append(token.value)
            self.transition_top(to=BNFParserState.ANNOTATION_SEEN_VALUE)
            return

        raise self.unexpected(token, state)

    def parse_ANNOTATION_SEEN_VALUE(self, token, state):
        if token.name == BNFToken.COMMA:
            self.transition_top(to=BNFParserState.ANNOTATION_SEEN_EQUAL_OR_COMMA)
            return

        state = self.exit_directive(token, state)

        if token.name == BNFToken.ID:
            self.enter_new_directive(token, state)
            return

        if token.name == BNFToken.RPAREN:
            self.exit_annotation(token, state)
            return

        raise self.unexpected(token, state)


def main():
    parser = argparse.ArgumentParser(description='Process CSS property definitions.')
    parser.add_argument('--properties', default="CSSProperties.json")
    parser.add_argument('--defines')
    parser.add_argument('--gperf-executable')
    parser.add_argument('-v', '--verbose', action='store_true')
    parser.add_argument('--dump-unused-grammars', action='store_true')
    parser.add_argument('--check-unused-grammars-values', action='store_true')
    args = parser.parse_args()

    with open(args.properties, "r", encoding="utf-8") as properties_file:
        properties_json = json.load(properties_file)

    parsing_context = ParsingContext(properties_json, defines_string=args.defines, parsing_for_codegen=True, check_unused_grammars_values=args.check_unused_grammars_values, verbose=args.verbose)
    parsing_context.parse_shared_grammar_rules()
    parsing_context.parse_properties_and_descriptors()

    if args.verbose:
        print(f"{len(parsing_context.parsed_shared_grammar_rules.rules)} shared grammar rules active for code generation")
        for set in parsing_context.parsed_properties_and_descriptors.all_sets:
            print(f"{len(set.all)} {set.name} {set.noun} active for code generation")
        print(f"{len(parsing_context.parsed_properties_and_descriptors.all_unique)} uniquely named properties and descriptors active for code generation")

    if args.dump_unused_grammars:
        for property in parsing_context.parsed_properties_and_descriptors.all_properties_and_descriptors:
            if property.codegen_properties.parser_grammar_unused:
                print(str(property).rjust(40, " ") + "  " + str(property.codegen_properties.parser_grammar_unused.root_term))
                print("           ".rjust(40, " ") + "  " + str(property.codegen_properties.parser_grammar_unused_reason))

    generation_context = GenerationContext(parsing_context.parsed_properties_and_descriptors, parsing_context.parsed_shared_grammar_rules, verbose=args.verbose, gperf_executable=args.gperf_executable)

    generators = [
        GenerateCSSPropertyInitialValues,
        GenerateCSSPropertyNames,
        GenerateCSSPropertyParsing,
        GenerateCSSStylePropertiesPropertyNames,
        GenerateStyleBuilderGenerated,
        GenerateStyleChangedAnimatablePropertiesGenerated,
        GenerateStyleComputedStyleProperties,
        GenerateStyleExtractorGenerated,
        GenerateStyleInterpolationWrapperMap,
        GenerateStylePropertyShorthandFunctions,
        GenerateRenderStyleProperties,
    ]

    for generator in generators:
        generator(generation_context).generate()


if __name__ == "__main__":
    main()
