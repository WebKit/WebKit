#!/usr/bin/env python3
#
# Copyright (C) 2022 Apple Inc. All rights reserved.
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

def quote_iterable(iterable, suffix=""):
    return (f'"{x}"{suffix}' for x in iterable)


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
        self.entries = {entry.key: entry for entry in entries}

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
        return Name.special_case_name_to_id.get(name) or re.sub(r'(^[^-])|-(.)', lambda m: (m[1] or m[2]).upper(), name)

    @property
    def id_without_prefix_with_lowercase_first_letter(self):
        return self.id_without_prefix[0].lower() + self.id_without_prefix[1:]


class PropertyName(Name):
    def __init__(self, name, *, name_for_methods=None):
        super().__init__(name)
        self.name_for_methods = PropertyName._compute_name_for_methods(name_for_methods, self.id_without_prefix)

    def __str__(self):
        return self.name

    def __repr__(self):
        return self.__str__()

    @staticmethod
    def _compute_name_for_methods(name_for_methods, id_without_prefix):
        if name_for_methods:
            return name_for_methods
        return id_without_prefix.replace("Webkit",  "")

    @property
    def id_without_scope(self):
        return f"CSSProperty{self.id_without_prefix}"

    @property
    def id(self):
        return f"CSSPropertyID::CSSProperty{self.id_without_prefix}"


class ValueKeywordName(Name):
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


class Status:
    schema = Schema(
        Schema.Entry("comment", allowed_types=[str]),
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
        Schema.Entry("comment", allowed_types=[str]),
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
        Schema.Entry("comment", allowed_types=[str]),
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
            # Order matches LogicalBoxAxis enum in Source/WebCore/platform/text/WritingMode.h.
            "axis": ["inline", "block"],
            # Order matches LogicalBoxSide enum in Source/WebCore/platform/text/WritingMode.h.
            "side": ["block-start", "inline-end", "block-end", "inline-start"],
            # Order matches LogicalBoxCorner enum in Source/WebCore/platform/text/WritingMode.h.
            "corner": ["start-start", "start-end", "end-start", "end-end"],
        },
        "physical": {
            # Order matches BoxAxis enum in Source/WebCore/platform/text/WritingMode.h.
            "axis": ["horizontal", "vertical"],
            # Order matches BoxSide enum in Source/WebCore/platform/text/WritingMode.h.
            "side": ["top", "right", "bottom", "left"],
            # Order matches BoxCorner enum in Source/WebCore/platform/text/WritingMode.h.
            "corner": ["top-left", "top-right", "bottom-right", "bottom-left"],
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
        Schema.Entry("aliases", allowed_types=[list], default_value=[]),
        Schema.Entry("auto-functions", allowed_types=[bool], default_value=False),
        Schema.Entry("color-property", allowed_types=[bool], default_value=False),
        Schema.Entry("comment", allowed_types=[str]),
        Schema.Entry("computable", allowed_types=[bool]),
        Schema.Entry("conditional-converter", allowed_types=[str]),
        Schema.Entry("converter", allowed_types=[str]),
        Schema.Entry("custom", allowed_types=[str]),
        Schema.Entry("custom-parser", allowed_types=[bool]),
        Schema.Entry("custom-parser-allows-number-or-integer-input", allowed_types=[bool], default_value=False),
        Schema.Entry("enable-if", allowed_types=[str]),
        Schema.Entry("fast-path-inherited", allowed_types=[bool], default_value=False),
        Schema.Entry("fill-layer-property", allowed_types=[bool], default_value=False),
        Schema.Entry("font-property", allowed_types=[bool], default_value=False),
        Schema.Entry("getter", allowed_types=[str]),
        Schema.Entry("high-priority", allowed_types=[bool], default_value=False),
        Schema.Entry("initial", allowed_types=[str]),
        Schema.Entry("internal-only", allowed_types=[bool], default_value=False),
        Schema.Entry("logical-property-group", allowed_types=[dict]),
        Schema.Entry("longhands", allowed_types=[list]),
        Schema.Entry("name-for-methods", allowed_types=[str]),
        Schema.Entry("parser-function", allowed_types=[str]),
        Schema.Entry("parser-exported", allowed_types=[bool]),
        Schema.Entry("parser-grammar", allowed_types=[str]),
        Schema.Entry("parser-grammar-comment", allowed_types=[str]),
        Schema.Entry("parser-requires-additional-parameters", allowed_types=[list], default_value=[]),
        Schema.Entry("parser-requires-context", allowed_types=[bool], default_value=False),
        Schema.Entry("parser-requires-context-mode", allowed_types=[bool], default_value=False),
        Schema.Entry("parser-requires-current-shorthand", allowed_types=[bool], default_value=False),
        Schema.Entry("parser-requires-current-property", allowed_types=[bool], default_value=False),
        Schema.Entry("parser-requires-quirks-mode", allowed_types=[bool], default_value=False),
        Schema.Entry("parser-requires-value-pool", allowed_types=[bool], default_value=False),
        Schema.Entry("related-property", allowed_types=[str]),
        Schema.Entry("separator", allowed_types=[str]),
        Schema.Entry("setter", allowed_types=[str]),
        Schema.Entry("settings-flag", allowed_types=[str]),
        Schema.Entry("sink-priority", allowed_types=[bool], default_value=False),
        Schema.Entry("skip-builder", allowed_types=[bool], default_value=False),
        Schema.Entry("skip-codegen", allowed_types=[bool], default_value=False),
        Schema.Entry("skip-parser", allowed_types=[bool], default_value=False),
        Schema.Entry("status", allowed_types=[str]),
        Schema.Entry("svg", allowed_types=[bool], default_value=False),
        Schema.Entry("synonym", allowed_types=[str]),
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
    def from_json(parsing_context, key_path, name, json_value):
        if type(json_value) is list:
            json_value = parsing_context.select_enabled_variant(json_value, label=f"{key_path}.codegen-properties")

        assert(type(json_value) is dict)
        StylePropertyCodeGenProperties.schema.validate_dictionary(parsing_context, f"{key_path}.codegen-properties", json_value, label=f"StylePropertyCodeGenProperties")

        property_name = PropertyName(name, name_for_methods=json_value.get("name-for-methods"))

        if "getter" not in json_value:
            json_value["getter"] = property_name.name_for_methods[0].lower() + property_name.name_for_methods[1:]

        if "setter" not in json_value:
            json_value["setter"] = f"set{property_name.name_for_methods}"

        if "initial" not in json_value:
            if "fill-layer-property" in json_value:
                json_value["initial"] = f"initialFill{property_name.name_for_methods}"
            else:
                json_value["initial"] = f"initial{property_name.name_for_methods}"

        if "custom" not in json_value:
            json_value["custom"] = ""
        elif json_value["custom"] == "All":
            json_value["custom"] = "Initial|Inherit|Value"
        json_value["custom"] = frozenset(json_value["custom"].split("|"))

        if "logical-property-group" in json_value:
            if json_value.get("longhands"):
                raise Exception(f"{key_path} is a shorthand, but belongs to a logical property group.")
            json_value["logical-property-group"] = LogicalPropertyGroup.from_json(parsing_context, f"{key_path}.codegen-properties", json_value["logical-property-group"])

        if "longhands" in json_value:
            json_value["longhands"] = list(compact_map(lambda value: Longhand.from_json(parsing_context, f"{key_path}.codegen-properties", value), json_value["longhands"]))
            if not json_value["longhands"]:
                del json_value["longhands"]

        if "computable" in json_value:
            if json_value["computable"]:
                if json_value.get("internal-only", False):
                    raise Exception(f"{key_path} can't be both internal-only and computable.")
        else:
            if json_value.get("internal-only", False):
                json_value["computable"] = False
            else:
                json_value["computable"] = True

        if json_value.get("top-priority", False):
            if json_value.get("comment") is None:
                raise Exception(f"{key_path} has top priority, but no comment to justify.")
            if json_value.get("longhands"):
                raise Exception(f"{key_path} is a shorthand, but has top priority.")
            if json_value.get("high-priority", False):
                raise Exception(f"{key_path} can't have conflicting top/high priority.")

        if json_value.get("high-priority", False):
            if json_value.get("longhands"):
                raise Exception(f"{key_path} is a shorthand, but has high priority.")

        if json_value.get("sink-priority", False):
            if json_value.get("longhands") is not None:
                raise Exception(f"{key_path} is a shorthand, but has sink priority.")

        if json_value.get("related-property"):
            if json_value.get("related-property") == name:
                raise Exception(f"{key_path} can't have itself as a related property.")
            if json_value.get("longhands"):
                raise Exception(f"{key_path} can't have both a related property and be a shorthand.")
            if json_value.get("high-priority", False):
                raise Exception(f"{key_path} can't have both a related property and be high priority.")

        if json_value.get("parser-grammar"):
            grammar = Grammar.from_string(parsing_context, f"{key_path}", name, json_value["parser-grammar"])
            grammar.perform_fixups(parsing_context.parsed_shared_grammar_rules)
            json_value["parser-grammar"] = grammar

        if json_value.get("parser-function"):
            for entry_name in ["skip-parser", "longhands", "custom-parser", "parser-grammar"]:
                if entry_name in json_value:
                    raise Exception(f"{key_path} can't have both 'parser-function' and '{entry_name}'.")

        if json_value.get("custom-parser"):
            for entry_name in ["skip-parser"]:
                if entry_name in json_value:
                    raise Exception(f"{key_path} can't have both 'custom-parser' and '{entry_name}'.")

            # Canonicalize to 'parser-function' so that the rest of the code only has to deal with one of them.
            json_value["parser-function"] = f"consume{property_name.id_without_prefix}"
            del json_value["custom-parser"]

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

    @property
    def is_deferred(self):
        return self.related_property or self.logical_property_group


class StyleProperty:
    schema = Schema(
        Schema.Entry("animatable", allowed_types=[bool], default_value=False),
        Schema.Entry("codegen-properties", allowed_types=[dict, list]),
        Schema.Entry("inherited", allowed_types=[bool], default_value=False),
        Schema.Entry("specification", allowed_types=[dict], convert_to=Specification),
        Schema.Entry("status", allowed_types=[dict, str], convert_to=Status),
        Schema.Entry("values", allowed_types=[list]),
    )

    def __init__(self, **dictionary):
        StyleProperty.schema.set_attributes_from_dictionary(dictionary, instance=self)
        self.property_name = self.codegen_properties.property_name
        self.synonymous_properties = []

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

        if "values" in json_value:
            values = list(filter(lambda value: value is not None, map(lambda value: Value.from_json(parsing_context, f"{key_path}.{name}", value), json_value["values"])))
            if codegen_properties.parser_grammar:
                codegen_properties.parser_grammar.perform_fixups_for_values_references(values)
            else:
                codegen_properties.parser_grammar = Grammar.from_values(parsing_context, key_path, name, values)
            json_value["values"] = values

        return StyleProperty(**json_value)

    def perform_fixups_for_synonyms(self, all_properties):
        # If 'synonym' was specified, replace the name with references to the Property object, and vice-versa a back-reference on that Property object back to this.
        if self.codegen_properties.synonym:
            if self.codegen_properties.synonym not in all_properties.all_by_name:
                raise Exception(f"Property {self.name} has an unknown synonym: {self.codegen_properties.synonym}.")

            original = all_properties.all_by_name[self.codegen_properties.synonym]
            original.synonymous_properties.append(self)

            self.codegen_properties.synonym = original

    def perform_fixups_for_longhands(self, all_properties):
        # If 'longhands' was specified, replace the names with references to the Property objects.
        if self.codegen_properties.longhands:
            self.codegen_properties.longhands = [all_properties.all_by_name[longhand.value] for longhand in self.codegen_properties.longhands]

    def perform_fixups_for_related_properties(self, all_properties):
        # If 'related-property' was specified, validate the relationship and replace the name with a reference to the Property object.
        if self.codegen_properties.related_property:
            if self.codegen_properties.related_property not in all_properties.all_by_name:
                raise Exception(f"Property {self.name} has an unknown related property: {self.codegen_properties.related_property}.")

            related_property = all_properties.all_by_name[self.codegen_properties.related_property]
            if type(related_property.codegen_properties.related_property) is str:
                if related_property.codegen_properties.related_property != self.name:
                    raise Exception(f"Property {self.name} has {related_property.name} as a related property, but it's not reciprocal.")
            else:
                if related_property.codegen_properties.related_property.name != self.name:
                    raise Exception(f"Property {self.name} has {related_property.name} as a related property, but it's not reciprocal.")
            self.codegen_properties.related_property = related_property

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
        self.perform_fixups_for_synonyms(all_properties)
        self.perform_fixups_for_longhands(all_properties)
        self.perform_fixups_for_related_properties(all_properties)
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

    # Used for parsing and consume methods. It is prefixed with a 'kind' for descriptors, and left unprefixed for style properties.
    # Examples:
    #       style property 'column-width' would generate a consume method called `consumeColumnWidth`
    #       @font-face descriptor 'font-display' would generate a consume method called `consumeFontFaceFontDisplay`
    @property
    def name_for_parsing_methods(self):
        return self.id_without_prefix

    @property
    def name_for_methods(self):
        return self.property_name.name_for_methods

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

        if not self.codegen_properties.computable:
            return True

        if self.codegen_properties.skip_builder and not self.codegen_properties.is_logical:
            return True

        if self.codegen_properties.longhands is not None:
            for longhand in self.codegen_properties.longhands:
                if not longhand.is_skipped_from_computed_style:
                    return True

        return False

    # Specialized properties to compute method names.

    @property
    def method_name_for_ensure_animations_or_transitions(self):
        if "animation-" in self.name:
            return "ensureAnimations"
        if "transition-" in self.name:
            return "ensureTransitions"
        raise Exception(f"Unrecognized animation or transition property name: '{self.name}")

    @property
    def method_name_for_animations_or_transitions(self):
        if "animation-" in self.name:
            return "animations"
        if "transition-" in self.name:
            return "transitions"
        raise Exception(f"Unrecognized animation or transition property name: '{self.name}")

    @property
    def method_name_for_ensure_layers(self):
        if "background-" in self.name:
            return "ensureBackgroundLayers"
        if "mask-" in self.name:
            return "ensureMaskLayers"
        raise Exception(f"Unrecognized FillLayer property name: '{self.name}")

    @property
    def method_name_for_layers(self):
        if "background-" in self.name:
            return "backgroundLayers"
        if "mask-" in self.name:
            return "maskLayers"
        raise Exception(f"Unrecognized FillLayer property name: '{self.name}")

    @property
    def enum_name_for_layers_type(self):
        if "background-" in self.name:
            return "FillLayerType::Background"
        if "mask-" in self.name:
            return "FillLayerType::Mask"
        raise Exception(f"Unrecognized FillLayer property name: '{self.name}")


class StyleProperties:
    def __init__(self, properties):
        self.properties = properties
        self.properties_by_name = {property.name: property for property in properties}
        self.logical_property_groups = {}
        self._all = None
        self._all_computed = None
        self._settings_flags = None

        self._perform_fixups()

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
    def supports_current_shorthand(self):
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

    # Returns a generator for the set of properties that are direction-aware (aka flow-sensative). Sorted first by property group name and then by property name.
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

        # Sort deferred longhands to the back, before shorthands.
        a_is_deferred = a.codegen_properties.is_deferred
        b_is_deferred = b.codegen_properties.is_deferred
        if a_is_deferred and not b_is_deferred:
            return 1
        if not a_is_deferred and b_is_deferred:
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
        Schema.Entry("aliases", allowed_types=[list], default_value=[]),
        Schema.Entry("comment", allowed_types=[str]),
        Schema.Entry("custom-parser-allows-number-or-integer-input", allowed_types=[bool], default_value=False),
        Schema.Entry("enable-if", allowed_types=[str]),
        Schema.Entry("internal-only", allowed_types=[bool], default_value=False),
        Schema.Entry("longhands", allowed_types=[list]),
        Schema.Entry("parser-function", allowed_types=[str]),
        Schema.Entry("parser-exported", allowed_types=[bool]),
        Schema.Entry("parser-grammar", allowed_types=[str]),
        Schema.Entry("parser-grammar-comment", allowed_types=[str]),
        Schema.Entry("parser-requires-additional-parameters", allowed_types=[list], default_value=[]),
        Schema.Entry("parser-requires-context", allowed_types=[bool], default_value=False),
        Schema.Entry("parser-requires-context-mode", allowed_types=[bool], default_value=False),
        Schema.Entry("parser-requires-current-shorthand", allowed_types=[bool], default_value=False),
        Schema.Entry("parser-requires-current-property", allowed_types=[bool], default_value=False),
        Schema.Entry("parser-requires-quirks-mode", allowed_types=[bool], default_value=False),
        Schema.Entry("parser-requires-value-pool", allowed_types=[bool], default_value=False),
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
        self.sink_priority = None
        self.is_deferred = None

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
            for entry_name in ["parser-function", "parser-requires-additional-parameters", "parser-requires-context", "parser-requires-context-mode", "parser-requires-current-shorthand", "parser-requires-current-property", "parser-requires-quirks-mode", "parser-requires-value-pool", "skip-parser", "longhands"]:
                if entry_name in json_value:
                    raise Exception(f"{key_path} can't have both 'parser-grammar' and '{entry_name}.")
            grammar = Grammar.from_string(parsing_context, f"{key_path}", name, json_value["parser-grammar"])
            grammar.perform_fixups(parsing_context.parsed_shared_grammar_rules)
            json_value["parser-grammar"] = grammar

        if json_value.get("parser-function"):
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
            values = list(filter(lambda value: value is not None, map(lambda value: Value.from_json(parsing_context, f"{key_path}.{name}", value), json_value["values"])))
            if codegen_properties.parser_grammar:
                codegen_properties.parser_grammar.perform_fixups_for_values_references(values)
            else:
                codegen_properties.parser_grammar = Grammar.from_values(parsing_context, key_path, name, values)
            json_value["values"] = values

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

    # Used for parsing and consume methods. It is prefixed with the rule type for descriptors, and left unprefixed for style properties.
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
        return DescriptorSet(name, list(compact_map(lambda item: Descriptor.from_json(parsing_context, key_path, item[0], item[1], name), json_value.items())))

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
    def supports_current_shorthand(self):
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


class PropertiesAndDescriptors:
    def __init__(self, style_properties, descriptors):
        self.style_properties = style_properties
        self.descriptors = descriptors
        self._all_grouped_by_name = None
        self._all_by_name = None
        self._all_unique = None
        self._settings_flags = None

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


# MARK: - Property Parsing

class Term:
    @staticmethod
    def wrap_with_multiplier(multiplier, term):
        if multiplier.kind == BNFNodeMultiplier.Kind.ZERO_OR_ONE:
            raise Exception("Unsupported multiplier '?'")
        elif multiplier.kind == BNFNodeMultiplier.Kind.SPACE_SEPARATED_ZERO_OR_MORE:
            raise Exception("Unsupported multiplier '*'")
        elif multiplier.kind == BNFNodeMultiplier.Kind.SPACE_SEPARATED_ONE_OR_MORE:
            raise Exception("Unsupported multiplier '+'")
        elif multiplier.kind == BNFNodeMultiplier.Kind.SPACE_SEPARATED_EXACT:
            raise Exception("Unsupported multiplier '{A}'")
        elif multiplier.kind == BNFNodeMultiplier.Kind.SPACE_SEPARATED_AT_LEAST:
            raise Exception("Unsupported multiplier '{A,}'")
        elif multiplier.kind == BNFNodeMultiplier.Kind.SPACE_SEPARATED_BETWEEN:
            raise Exception("Unsupported multiplier '{A,B}'")
        elif multiplier.kind == BNFNodeMultiplier.Kind.COMMA_SEPARATED_ONE_OR_MORE:
            return CommaSeparatedRepetitionTerm.wrapping_term(term)
        elif multiplier.kind == BNFNodeMultiplier.Kind.COMMA_SEPARATED_EXACT:
            raise Exception("Unsupported multiplier '#{A}'")
        elif multiplier.kind == BNFNodeMultiplier.Kind.COMMA_SEPARATED_AT_LEAST:
            raise Exception("Unsupported multiplier '#{A,}'")
        elif multiplier.kind == BNFNodeMultiplier.Kind.COMMA_SEPARATED_BETWEEN:
            raise Exception("Unsupported multiplier '#{A,B}'")

    @staticmethod
    def from_node(node):
        if isinstance(node, BNFGroupingNode):
            if node.kind == BNFGroupingNode.Kind.MATCH_ALL_ORDERED:
                if len(node.members) == 1:
                    term = Term.from_node(node.members[0])
                else:
                    raise Exception("Unsupported grouping. 'ordered' (e.g. [<length> <length>])")
            elif node.kind == BNFGroupingNode.Kind.MATCH_ONE:
                term = MatchOneTerm.from_node(node)
            elif node.kind == BNFGroupingNode.Kind.MATCH_ALL_ANY_ORDER:
                raise Exception("Unsupported grouping. 'all unordered' (e.g. [<length> && <length>])")
            elif node.kind == BNFGroupingNode.Kind.MATCH_ONE_OR_MORE_ANY_ORDER:
                raise Exception("Unsupported grouping. 'one or more unordered' (e.g. [<length> || <length>])")
            else:
                raise Exception(f"Unknown grouping kind '{node.kind}' in BNF parse tree node '{node}'")
        elif isinstance(node, BNFReferenceNode):
            term = ReferenceTerm.from_node(node)
        elif isinstance(node, BNFKeywordNode):
            term = KeywordTerm.from_node(node)
        else:
            raise Exception(f"Unknown node '{node}' in BNF parse tree")

        # If the node has an attached multiplier, wrap the node in
        # a term created from that multiplier.
        if node.multiplier.kind:
            term = Term.wrap_with_multiplier(node.multiplier, term)

        return term


class BuiltinSchema:
    class OptionalParameter:
        def __init__(self, name, values, default):
            self.name = name
            self.values = values
            self.default = default

    class RequiredParameter:
        def __init__(self, name, values):
            self.name = name
            self.values = values

    class Entry:
        def __init__(self, name, consume_function_name, *parameter_descriptors):
            self.name = Name(name)
            self.consume_function_name = consume_function_name

            # Mapping of descriptor name (e.g. 'value_range' or 'mode') to OptionalParameter descriptor.
            self.optionals = {}

            # Mapping of descriptor name (e.g. 'value_range' or 'mode') to RequiredParameter descriptor.
            self.requireds = {}

            # Mapping from all the potential values (e.g. 'svg', 'unitless-allowed') to the parameter descriptor (e.g. OptionalParameter/RequiredParameter instances).
            self.value_to_descriptor = {}

            for parameter_descriptor in parameter_descriptors:
                if isinstance(parameter_descriptor, BuiltinSchema.OptionalParameter):
                    self.optionals[parameter_descriptor.name] = parameter_descriptor
                    for value in parameter_descriptor.values.keys():
                        self.value_to_descriptor[value] = parameter_descriptor
                if isinstance(parameter_descriptor, BuiltinSchema.RequiredParameter):
                    self.requireds[parameter_descriptor.name] = parameter_descriptor
                    for value in parameter_descriptor.values.keys():
                        self.value_to_descriptor[value] = parameter_descriptor

            def builtin_schema_type_init(self, parameters):
                # Map from descriptor name (e.g. 'value_range' or 'mode') to mapped value (e.g. `ValueRange::NonNegative` or `HTMLStandardMode`) for all of the parameters.
                self.parameter_map = {}

                # Map from descriptor name (e.g. 'value_range' or 'mode') to parameter value (e.g. `[0,inf]` or `strict`) for all of the parameters.
                descriptors_used = {}

                # Example parameters is ['svg', 'unitless-allowed'].
                for parameter in parameters:
                    if parameter not in self.entry.value_to_descriptor:
                        raise Exception(f"Unknown parameter '{parameter}' passed to <{self.entry.name.name}>. Supported parameters are {', '.join(quote_iterable(self.entry.value_to_descriptor.keys()))}.")

                    descriptor = self.entry.value_to_descriptor[parameter]
                    if descriptor.name in descriptors_used:
                        raise Exception(f"More than one parameter of type '{descriptor.name}` passed to <{self.entry.name.name}>, pick one: {descriptors_used[descriptor.name]}, {parameter}.")
                    descriptors_used[descriptor.name] = parameter

                    self.parameter_map[descriptor.name] = descriptor.values[parameter]

                # Fill `results` with mappings from `names` (e.g. `value_range`) to mapped to value (e.g. `ValueRange::NonNegative`)
                self.results = {}
                for descriptor in self.entry.optionals.values():
                    self.results[descriptor.name] = self.parameter_map.get(descriptor.name, descriptor.default)
                for descriptor in self.entry.requireds.values():
                    if descriptor.name not in self.parameter_map:
                        raise Exception(f"Required parameter of type '{descriptor.name}` not passed to <{self.entry.name.name}>. Pick one of {', '.join(quote_iterable(descriptor.values.keys()))}.")
                    self.results[descriptor.name] = self.parameter_map.get(descriptor.name)

            def builtin_schema_type_parameter_string_getter(name, self):
                return self.results[name]

            # Dynamically generate a class that can handle validationg and generation.
            class_name = f"Builtin{self.name.id_without_prefix}Consumer"
            class_attributes = {
                "__init__": builtin_schema_type_init,
                "consume_function_name": self.consume_function_name,
                "entry": self,
            }

            for name in itertools.chain(self.optionals.keys(), self.requireds.keys()):
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
# They can either reference a rule from the grammer-rules set, in which case they will be replaced by
# the real term during fixup, or a builtin rule, in which case they will inform the generator to call
# out to a handwritten consumer. Example:
#
#   "<length unitless-allowed>"
#
class ReferenceTerm:
    builtins = BuiltinSchema(
        BuiltinSchema.Entry("angle", "consumeAngle",
            BuiltinSchema.OptionalParameter("mode", values={"svg": "SVGAttributeMode", "strict": "HTMLStandardMode"}, default=None),
            BuiltinSchema.OptionalParameter("unitless", values={"unitless-allowed": "UnitlessQuirk::Allow"}, default="UnitlessQuirk::Forbid"),
            BuiltinSchema.OptionalParameter("unitless-zero", values={"unitless-zero-allowed": "UnitlessZeroQuirk::Allow"}, default="UnitlessZeroQuirk::Forbid")),
        BuiltinSchema.Entry("length", "consumeLength",
            BuiltinSchema.OptionalParameter("value_range", values={"[0,inf]": "ValueRange::NonNegative"}, default="ValueRange::All"),
            BuiltinSchema.OptionalParameter("mode", values={"svg": "SVGAttributeMode", "strict": "HTMLStandardMode"}, default=None),
            BuiltinSchema.OptionalParameter("unitless", values={"unitless-allowed": "UnitlessQuirk::Allow"}, default="UnitlessQuirk::Forbid")),
        BuiltinSchema.Entry("length-percentage", "consumeLengthOrPercent",
            BuiltinSchema.OptionalParameter("value_range", values={"[0,inf]": "ValueRange::NonNegative"}, default="ValueRange::All"),
            BuiltinSchema.OptionalParameter("mode", values={"svg": "SVGAttributeMode", "strict": "HTMLStandardMode"}, default=None),
            BuiltinSchema.OptionalParameter("unitless", values={"unitless-allowed": "UnitlessQuirk::Allow"}, default="UnitlessQuirk::Forbid")),
        BuiltinSchema.Entry("time", "consumeTime",
            BuiltinSchema.OptionalParameter("value_range", values={"[0,inf]": "ValueRange::NonNegative"}, default="ValueRange::All"),
            BuiltinSchema.OptionalParameter("mode", values={"svg": "SVGAttributeMode", "strict": "HTMLStandardMode"}, default=None),
            BuiltinSchema.OptionalParameter("unitless", values={"unitless-allowed": "UnitlessQuirk::Allow"}, default="UnitlessQuirk::Forbid")),
        BuiltinSchema.Entry("integer", "consumeInteger",
            BuiltinSchema.OptionalParameter("value_range", values={"[0,inf]": "IntegerValueRange::NonNegative", "[1,inf]": "IntegerValueRange::Positive"}, default="IntegerValueRange::All")),
        BuiltinSchema.Entry("number", "consumeNumber",
            BuiltinSchema.OptionalParameter("value_range", values={"[0,inf]": "ValueRange::NonNegative"}, default="ValueRange::All")),
        BuiltinSchema.Entry("percentage", "consumePercent",
            BuiltinSchema.OptionalParameter("value_range", values={"[0,inf]": "ValueRange::NonNegative"}, default="ValueRange::All")),
        BuiltinSchema.Entry("position", "consumePosition",
            BuiltinSchema.OptionalParameter("unitless", values={"unitless-allowed": "UnitlessQuirk::Allow"}, default="UnitlessQuirk::Forbid")),
        BuiltinSchema.Entry("color", "consumeColor",
            BuiltinSchema.OptionalParameter("quirky_colors", values={"accept-quirky-colors-in-quirks-mode": True}, default=False)),
        BuiltinSchema.Entry("resolution", "consumeResolution"),
        BuiltinSchema.Entry("string", "consumeString"),
        BuiltinSchema.Entry("custom-ident", "consumeCustomIdent"),
        BuiltinSchema.Entry("dashed-ident", "consumeDashedIdent"),
        BuiltinSchema.Entry("url", "consumeURL"),
        BuiltinSchema.Entry("declaration-value", "consumeDeclarationValue")
    )

    def __init__(self, name, is_internal, parameters):
        # Store the first (and perhaps only) part as the reference's name (e.g. for <length-percentage [0,inf] unitless-allowed> store 'length-percentage').
        self.name = Name(name)

        # Store whether this is an 'internal' reference (e.g. as indicated by the double angle brackets <<values>>).
        self.is_internal = is_internal

        # Store any remaining parts as the parameters (e.g. for <length-percentage [0,inf] unitless-allowed> store ['[0,inf]', 'unitless-allowed']).
        self.parameters = parameters

        # Check name and parameters against the builtins schemas to verify if they are well formed.
        self.builtin = ReferenceTerm.builtins.validate_and_construct_if_builtin(self.name, self.parameters)

    def __str__(self):
        base = ' '.join([self.name.name] + self.parameters)
        if self.is_internal:
            return f"<<{base}>>"
        return f"<{base}>"

    def __repr__(self):
        return self.__str__()

    @staticmethod
    def from_node(node):
        assert(type(node) is BNFReferenceNode)
        return ReferenceTerm(node.name, node.is_internal, [str(attribute) for attribute in node.attributes])

    def perform_fixups(self, all_rules):
        # Replace a reference with the term it references if it can be found.
        value = str(self)
        if value in all_rules.rules_by_name:
            return all_rules.rules_by_name[value].grammar.root_term
        return self

    def perform_fixups_for_values_references(self, values):
        # NOTE: The actual name in the grammar is "<<values>>", which we store as is_internal + 'values'.
        if self.is_internal and self.name.name == "values":
            # FIXME: This should really return a "MatchOneTerm" if len(values) > 1 and not a list.
            return [value.keyword_term for value in values]
        return self

    @property
    def is_builtin(self):
        return self.builtin is not None


# KeywordTerm represents a direct keyword match. The syntax in the CSS specifications
# is a bare string. Example:
#
#   "auto"
#
class KeywordTerm:
    def __init__(self, value, *, aliased_to=None, comment=None, settings_flag=None, status=None):
        self.value = value
        self.aliased_to = aliased_to
        self.comment = comment
        self.settings_flag = settings_flag
        self.status = status

    def __str__(self):
        return f"'{self.value}'"

    def __repr__(self):
        return self.__str__()

    @staticmethod
    def from_node(node):
        assert(type(node) is BNFKeywordNode)
        return KeywordTerm(ValueKeywordName(node.keyword))

    def perform_fixups(self, all_rules):
        return self

    def perform_fixups_for_values_references(self, values):
        return self

    @property
    def requires_context(self):
        return self.settings_flag or self.status == "internal"

    @property
    def is_eligible_for_fast_path(self):
        # Keyword terms that are aliased as not eligable for the fast path as the fast
        # path can only support a basic predicate.
        return not self.aliased_to

    @property
    def name(self):
        return self.value.name


# MatchOneTerm represents a set of terms, only one of which can match. The
# syntax in the CSS specifications is a '|' between terms. Example:
#
#   "auto" | "reverse" | "<angle unitless-allowed unitless-zero-allowed>"
#
class MatchOneTerm:
    def __init__(self, terms):
        self.terms = terms

    def __str__(self):
        return f"[{' | '.join(map(lambda t: str(t), self.terms))}]"

    def __repr__(self):
        return self.__str__()

    @staticmethod
    def from_node(node):
        assert(type(node) is BNFGroupingNode)
        assert(node.kind is BNFGroupingNode.Kind.MATCH_ONE)

        return MatchOneTerm(list(compact_map(lambda member: Term.from_node(member), node.members)))

    @staticmethod
    def from_values(parsing_context, key_path, values):
        return MatchOneTerm(list(compact_map(lambda value: value.keyword_term, values)))

    def perform_fixups(self, all_rules):
        updated_terms = []
        for term in self.terms:
            updated_term = term.perform_fixups(all_rules)
            if isinstance(updated_term, MatchOneTerm):
                updated_terms += updated_term.terms
            else:
                updated_terms += [updated_term]

        self.terms = updated_terms

        if len(self.terms) == 1:
            return self.terms[0]
        return self

    def perform_fixups_for_values_references(self, values):
        self.terms = flatten([term.perform_fixups_for_values_references(values) for term in self.terms])
        return self

    @property
    def is_values_reference(self):
        return False

    @property
    def has_keyword_term(self):
        return any(isinstance(term, KeywordTerm) for term in self.terms)

    @property
    def has_only_keyword_terms(self):
        return all(isinstance(term, KeywordTerm) for term in self.terms)

    @property
    def keyword_terms(self):
        return (term for term in self.terms if isinstance(term, KeywordTerm))

    @property
    def fast_path_keyword_terms(self):
        return (term for term in self.keyword_terms if term.is_eligible_for_fast_path)

    @property
    def has_fast_path_keyword_terms(self):
        return any(term.is_eligible_for_fast_path for term in self.keyword_terms)

    @property
    def has_only_fast_path_keyword_terms(self):
        return all(isinstance(term, KeywordTerm) and term.is_eligible_for_fast_path for term in self.terms)


# CommaSeparatedRepetitionTerm represents matching a list of terms
# separated by commas. The syntax in the CSS specifications is a
# trailing '#'. Example:
#
#   "<length>#"
#
class CommaSeparatedRepetitionTerm:
    def __init__(self, repeated_term, single_value_optimization=True):
        self.repeated_term = repeated_term
        self.single_value_optimization = single_value_optimization

    def __str__(self):
        return f"[{str(self.repeated_term)}#]"

    def __repr__(self):
        return self.__str__()

    @staticmethod
    def wrapping_term(term):
        return CommaSeparatedRepetitionTerm(term)

    def perform_fixups(self, all_rules):
        self.repeated_term = self.repeated_term.perform_fixups(all_rules)
        return self

    def perform_fixups_for_values_references(self, values):
        self.repeated_term = self.repeated_term.perform_fixups_for_values_references(values)
        return self


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
    def from_values(parsing_context, key_path, name, values):
        return Grammar(name, MatchOneTerm.from_values(parsing_context, key_path, values))

    @staticmethod
    def from_string(parsing_context, key_path, name, string):
        assert(type(string) is str)
        return Grammar(name, Term.from_node(BNFParser(parsing_context, key_path, string).parse()))

    def perform_fixups(self, all_rules):
        self.root_term = self.root_term.perform_fixups(all_rules)

    def perform_fixups_for_values_references(self, values):
        self.root_term = self.root_term.perform_fixups_for_values_references(values)

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


# A shared grammar rule and metadata describing it. Part of the set of rules tracked by SharedGrammarRules.
class SharedGrammarRule:
    schema = Schema(
        Schema.Entry("aliased-to", allowed_types=[str], convert_to=ValueKeywordName),
        Schema.Entry("comment", allowed_types=[str]),
        Schema.Entry("exported", allowed_types=[bool], default_value=False),
        Schema.Entry("grammar", allowed_types=[str], required=True),
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

        grammar = Grammar.from_string(parsing_context, f"{key_path}.{name}", name, json_value["grammar"])

        if "aliased-to" in json_value:
            if not isinstance(grammar.root_term, KeywordTerm):
                raise Exception(f"Invalid use of 'aliased-to' found at '{key_path}'. 'aliased-to' can only be used with grammars that consist of a single keyword term.")
            grammar.root_term.aliased_to = json_value["aliased-to"]

        json_value["grammar"] = grammar

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

    def __init__(self, json_value, *, defines_string, parsing_for_codegen, verbose):
        ParsingContext.TopLevelObject.schema.validate_dictionary(self, "$", json_value, label="top level object")

        self.json_value = json_value
        self.conditionals = frozenset((defines_string or '').split(' '))
        self.parsing_for_codegen = parsing_for_codegen
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
        to.write("// This file is automatically generated from CSSProperties.json by the process-css-properties script. Do not edit it.")
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
        gperf_command = self.generation_context.gperf_executable or os.environ['GPERF']

        gperf_result_code = subprocess.call([gperf_command, '--key-positions=*', '-D', '-n', '-s', '2', 'CSSPropertyNames.gperf', '--output-file=CSSPropertyNames.cpp'])
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
                "CSSProperty.h",
                "Settings.h",
            ],
            system_headers=[
                "<string.h>",
                "<wtf/ASCIICType.h>",
                "<wtf/Hasher.h>",
                "<wtf/text/AtomString.h>",
            ]
        )

        to.write_block("""
            IGNORE_WARNINGS_BEGIN("implicit-fallthrough")

            // Older versions of gperf like to use the `register` keyword.
            #define register
            """)

        self.generation_context.generate_open_namespace(
            to=to,
            namespace="WebCore"
        )

        to.write_block("""\
            // Using std::numeric_limits<uint16_t>::max() here would be cleaner,
            // but is not possible due to missing constexpr support in MSVC 2013.
            static_assert(numCSSProperties + 1 <= 65535, "CSSPropertyID should fit into uint16_t.");
            """)

        all_computed_property_ids = (f"{property.id}," for property in self.properties_and_descriptors.style_properties.all_computed)
        to.write(f"const std::array<CSSPropertyID, {count_iterable(self.properties_and_descriptors.style_properties.all_computed)}> computedPropertyIDs {{")
        with to.indent():
            to.write_lines(all_computed_property_ids)
        to.write("};")
        to.newline()

        all_property_name_strings = quote_iterable((f"{property.name}" for property in self.properties_and_descriptors.all_unique), "_s,")
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
        # Concatenates a list of unique 'propererty-name, property-id' strings with a second list of all 'property-alias, property-id' strings.
        all_property_names_and_aliases_with_ids = itertools.chain(
              [f'{property.name}, {property.id}'                        for property in self.properties_and_descriptors.all_unique],
            *[[f'{alias}, {property.id}' for alias in property.aliases] for property in self.properties_and_descriptors.all_properties_and_descriptors]
        )

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
                LChar characters[maxCSSPropertyNameLength];
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
                unsigned length = nextCharacter - characters;
                return { characters, length };
            }

            """)

    def _generate_physical_logical_conversion_function(self, *, to, signature, source, destination, resolver_enum_prefix):
        source_as_id = PropertyName.convert_name_to_id(source)
        destination_as_id = PropertyName.convert_name_to_id(destination)

        to.write(f"{signature}")
        to.write(f"{{")
        with to.indent():
            to.write(f"auto textflow = makeTextFlow(writingMode, direction);")
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
                        to.write(f"return properties[static_cast<size_t>(map{source_as_id}{kind_as_id}To{destination_as_id}{kind_as_id}(textflow, {resolver_enum}))];")
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

        to.write(f"constexpr bool isInheritedPropertyTable[numCSSProperties + {GenerationContext.number_of_predefined_properties}] = {{")
        with to.indent():
            to.write(f"false, // CSSPropertyID::CSSPropertyInvalid")
            to.write(f"true , // CSSPropertyID::CSSPropertyCustom")
            to.write_lines(all_inherited_and_ids)
        to.write(f"}};")

        to.write_block("""
            bool CSSProperty::isInheritedProperty(CSSPropertyID id)
            {
                ASSERT(id < firstCSSProperty + numCSSProperties);
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
        first, *middle, last = (f"settings.{flag} << {i}" for (i, flag) in enumerate(self.properties_and_descriptors.settings_flags))

        to.write(f"void add(Hasher& hasher, const CSSPropertySettings& settings)")
        to.write(f"{{")
        with to.indent():
            to.write(f"unsigned bits = {first}")
            with to.indent():
                to.write_lines((f"| {expression}" for expression in middle))
                to.write(f"| {last};")

            to.write(f"add(hasher, bits);")
        to.write(f"}}")
        to.newline()

    def _term_matches_number_or_integer(self, term):
        if isinstance(term, ReferenceTerm):
            if term.name.name == "number" or term.name.name == "integer":
                return True
        elif isinstance(term, MatchOneTerm):
            for inner_term in term.terms:
                if self._term_matches_number_or_integer(inner_term):
                    return True
        elif isinstance(term, CommaSeparatedRepetitionTerm):
            return self._term_matches_number_or_integer(term.repeated_term)
        return False

    def _property_matches_number_or_integer(self, p):
        if p.codegen_properties.custom_parser_allows_number_or_integer_input:
            return True
        if not p.codegen_properties.parser_grammar:
            return False
        return self._term_matches_number_or_integer(p.codegen_properties.parser_grammar.root_term)

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
                signature="CSSPropertyID relatedProperty(CSSPropertyID id)",
                iterable=(p for p in self.properties_and_descriptors.style_properties.all if p.codegen_properties.related_property),
                mapping=lambda p: f"return {p.codegen_properties.related_property.id};",
                default="return CSSPropertyID::CSSPropertyInvalid;"
            )

            self.generation_context.generate_property_id_switch_function(
                to=writer,
                signature="Vector<String> CSSProperty::aliasesForProperty(CSSPropertyID id)",
                iterable=(p for p in self.properties_and_descriptors.style_properties.all if p.codegen_properties.aliases),
                mapping=lambda p: f"return {{ {', '.join(quote_iterable(p.codegen_properties.aliases, '_s'))} }};",
                default="return { };"
            )

            self.generation_context.generate_property_id_switch_function_bool(
                to=writer,
                signature="bool CSSProperty::isColorProperty(CSSPropertyID id)",
                iterable=(p for p in self.properties_and_descriptors.style_properties.all if p.codegen_properties.color_property)
            )

            self.generation_context.generate_property_id_switch_function(
                to=writer,
                signature="UChar CSSProperty::listValuedPropertySeparator(CSSPropertyID id)",
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
                signature="bool CSSProperty::isDirectionAwareProperty(CSSPropertyID id)",
                iterable=self.properties_and_descriptors.style_properties.all_direction_aware_properties
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
                signature="CSSPropertyID CSSProperty::resolveDirectionAwareProperty(CSSPropertyID id, TextDirection direction, WritingMode writingMode)",
                source="logical",
                destination="physical",
                resolver_enum_prefix="LogicalBox"
            )

            self._generate_physical_logical_conversion_function(
                to=writer,
                signature="CSSPropertyID CSSProperty::unresolvePhysicalProperty(CSSPropertyID id, TextDirection direction, WritingMode writingMode)",
                source="physical",
                destination="logical",
                resolver_enum_prefix="Box"
            )

            self.generation_context.generate_property_id_switch_function_bool(
                to=writer,
                signature="bool CSSProperty::isDescriptorOnly(CSSPropertyID id)",
                iterable=self.properties_and_descriptors.all_descriptor_only
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
            first_low_priority_property = None
            last_low_priority_property = None
            first_deferred_property = None
            last_deferred_property = None

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
                elif not property.codegen_properties.is_deferred:
                    if not first_low_priority_property:
                        first_low_priority_property = property
                    last_low_priority_property = property
                else:
                    if not first_deferred_property:
                        first_deferred_property = property
                    last_deferred_property = property

                to.write(f"{property.id_without_scope} = {count},")

                count += 1
                max_length = max(len(property.name), max_length)

            num = count - first

        to.write(f"}};")
        to.newline()

        to.write(f"constexpr uint16_t firstCSSProperty = {first};")
        to.write(f"constexpr uint16_t numCSSProperties = {num};")
        to.write(f"constexpr unsigned maxCSSPropertyNameLength = {max_length};")
        to.write(f"constexpr auto firstTopPriorityProperty = {first_top_priority_property.id};")
        to.write(f"constexpr auto lastTopPriorityProperty = {last_top_priority_property.id};")
        to.write(f"constexpr auto firstHighPriorityProperty = {first_high_priority_property.id};")
        to.write(f"constexpr auto lastHighPriorityProperty = {last_high_priority_property.id};")
        to.write(f"constexpr auto firstLowPriorityProperty = {first_low_priority_property.id};")
        to.write(f"constexpr auto lastLowPriorityProperty = {last_low_priority_property.id};")
        to.write(f"constexpr auto firstDeferredProperty = {first_deferred_property.id};")
        to.write(f"constexpr auto lastDeferredProperty = {last_deferred_property.id};")
        to.write(f"constexpr auto firstShorthandProperty = {first_shorthand_property.id};")
        to.write(f"constexpr auto lastShorthandProperty = {last_shorthand_property.id};")
        to.write(f"constexpr uint16_t numCSSPropertyLonghands = firstShorthandProperty - firstCSSProperty;")

        to.write(f"extern const std::array<CSSPropertyID, {count_iterable(self.properties_and_descriptors.style_properties.all_computed)}> computedPropertyIDs;")
        to.newline()

    def _generate_css_property_names_h_property_settings(self, *, to):
        settings_variable_declarations = (f"bool {flag} {{ false }};" for flag in self.properties_and_descriptors.settings_flags)

        to.write(f"struct CSSPropertySettings {{")
        with to.indent():
            to.write(f"WTF_MAKE_STRUCT_FAST_ALLOCATED;")
            to.newline()

            to.write_lines(settings_variable_declarations)
            to.newline()

            to.write(f"CSSPropertySettings() = default;")
            to.write(f"explicit CSSPropertySettings(const Settings&);")
        to.write(f"}};")
        to.newline()

        to.write(f"bool operator==(const CSSPropertySettings&, const CSSPropertySettings&);")
        to.write(f"inline bool operator!=(const CSSPropertySettings& a, const CSSPropertySettings& b) {{ return !(a == b); }}")
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

            CSSPropertyID relatedProperty(CSSPropertyID);

            template<CSSPropertyID first, CSSPropertyID last> struct CSSPropertiesRange {
                struct Iterator {
                    uint16_t index { static_cast<uint16_t>(first) };
                    constexpr CSSPropertyID operator*() const { return static_cast<CSSPropertyID>(index); }
                    constexpr Iterator& operator++() { ++index; return *this; }
                    constexpr bool operator==(std::nullptr_t) const { return index > static_cast<uint16_t>(last); }
                    constexpr bool operator!=(std::nullptr_t) const { return index <= static_cast<uint16_t>(last); }
                };
                static constexpr Iterator begin() { return { }; }
                static constexpr std::nullptr_t end() { return nullptr; }
                static constexpr uint16_t size() { return last - first + 1; }
            };
            using AllCSSPropertiesRange = CSSPropertiesRange<static_cast<CSSPropertyID>(firstCSSProperty), lastShorthandProperty>;
            using AllLonghandCSSPropertiesRange = CSSPropertiesRange<static_cast<CSSPropertyID>(firstCSSProperty), lastDeferredProperty>;
            constexpr AllCSSPropertiesRange allCSSProperties() { return { }; }
            constexpr AllLonghandCSSPropertiesRange allLonghandCSSProperties() { return { }; }

            constexpr bool isLonghand(CSSPropertyID property)
            {
                return static_cast<uint16_t>(property) >= firstCSSProperty && static_cast<uint16_t>(property) < static_cast<uint16_t>(firstShorthandProperty);
            }
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


# Generates `CSSStyleDeclaration+PropertyNames.idl`.
class GenerateCSSStyleDeclarationPropertyNames:
    def __init__(self, generation_context):
        self.generation_context = generation_context

    @property
    def properties_and_descriptors(self):
        return self.generation_context.properties_and_descriptors

    def generate(self):
        self.generate_css_style_declaration_property_names_idl()

    # MARK: - Helper generator functions for CSSStyleDeclaration+PropertyNames.idl

    def _generate_css_style_declaration_property_names_idl_typedefs(self, *, to):
        to.write_block("""\
            typedef USVString CSSOMString;
            """)

    def _generate_css_style_declaration_property_names_idl_open_interface(self, *, to):
        to.write("partial interface CSSStyleDeclaration {")

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
                idl_attribute_name = GenerateCSSStyleDeclarationPropertyNames._convert_css_property_to_idl_attribute(name_or_alias, lowercase_first=lowercase_first)
            else:
                idl_attribute_name = name_or_alias

            extended_attributes_values = [f"DelegateToSharedSyntheticAttribute=propertyValueFor{variant}IDLAttribute", "CallWith=PropertyName"]
            if property.codegen_properties.settings_flag:
                extended_attributes_values += [f"EnabledBySetting={property.codegen_properties.settings_flag}"]

            to.write(f"[CEReactions, {', '.join(extended_attributes_values)}] attribute [LegacyNullToEmptyString] CSSOMString {idl_attribute_name};")

    def generate_css_style_declaration_property_names_idl(self):
        with open('CSSStyleDeclaration+PropertyNames.idl', 'w') as output_file:
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

    # Color property setters.

    def _generate_color_property_initial_value_setter(self, to, property):
        to.write(f"if (builderState.applyPropertyToRegularStyle())")
        to.write(f"    builderState.style().{property.codegen_properties.setter}(RenderStyle::{property.codegen_properties.initial}());")
        to.write(f"if (builderState.applyPropertyToVisitedLinkStyle())")
        to.write(f"    builderState.style().setVisitedLink{property.name_for_methods}(RenderStyle::{property.codegen_properties.initial}());")

    def _generate_color_property_inherit_value_setter(self, to, property):
        to.write(f"if (builderState.applyPropertyToRegularStyle())")
        to.write(f"    builderState.style().{property.codegen_properties.setter}(builderState.parentStyle().{property.codegen_properties.getter}());")
        to.write(f"if (builderState.applyPropertyToVisitedLinkStyle())")
        to.write(f"    builderState.style().setVisitedLink{property.name_for_methods}(builderState.parentStyle().{property.codegen_properties.getter}());")

    def _generate_color_property_value_setter(self, to, property, value):
        to.write(f"if (builderState.applyPropertyToRegularStyle())")
        to.write(f"    builderState.style().{property.codegen_properties.setter}(builderState.colorFromPrimitiveValue({value}, ForVisitedLink::No));")
        to.write(f"if (builderState.applyPropertyToVisitedLinkStyle())")
        to.write(f"    builderState.style().setVisitedLink{property.name_for_methods}(builderState.colorFromPrimitiveValue({value}, ForVisitedLink::Yes));")

    # Animation property setters.

    def _generate_animation_property_initial_value_setter(self, to, property):
        to.write(f"auto& list = builderState.style().{property.method_name_for_ensure_animations_or_transitions}();")
        to.write(f"if (list.isEmpty())")
        to.write(f"    list.append(Animation::create());")
        to.write(f"list.animation(0).{property.codegen_properties.setter}(Animation::{property.codegen_properties.initial}());")
        to.write(f"for (auto& animation : list)")
        to.write(f"    animation->clear{property.name_for_methods}();")

    def _generate_animation_property_inherit_value_setter(self, to, property):
        to.write(f"auto& list = builderState.style().{property.method_name_for_ensure_animations_or_transitions}();")
        to.write(f"auto* parentList = builderState.parentStyle().{property.method_name_for_animations_or_transitions}();")
        to.write(f"size_t i = 0, parentSize = parentList ? parentList->size() : 0;")
        to.write(f"for ( ; i < parentSize && parentList->animation(i).is{property.name_for_methods}Set(); ++i) {{")
        to.write(f"    if (list.size() <= i)")
        to.write(f"        list.append(Animation::create());")
        to.write(f"    list.animation(i).{property.codegen_properties.setter}(parentList->animation(i).{property.codegen_properties.getter}());")
        to.write(f"}}")
        to.write(f"// Reset any remaining animations to not have the property set.")
        to.write(f"for ( ; i < list.size(); ++i)")
        to.write(f"    list.animation(i).clear{property.name_for_methods}();")

    def _generate_animation_property_value_setter(self, to, property):
        to.write(f"auto& list = builderState.style().{property.method_name_for_ensure_animations_or_transitions}();")
        to.write(f"size_t childIndex = 0;")
        to.write(f"if (is<CSSValueList>(value)) {{")
        to.write(f"    // Walk each value and put it into an animation, creating new animations as needed.")
        to.write(f"    for (auto& currentValue : downcast<CSSValueList>(value)) {{")
        to.write(f"        if (childIndex <= list.size())")
        to.write(f"            list.append(Animation::create());")
        to.write(f"        builderState.styleMap().mapAnimation{property.name_for_methods}(list.animation(childIndex), currentValue);")
        to.write(f"        ++childIndex;")
        to.write(f"    }}")
        to.write(f"}} else {{")
        to.write(f"    if (list.isEmpty())")
        to.write(f"        list.append(Animation::create());")
        to.write(f"    builderState.styleMap().mapAnimation{property.name_for_methods}(list.animation(childIndex), value);")
        to.write(f"    childIndex = 1;")
        to.write(f"}}")
        to.write(f"for ( ; childIndex < list.size(); ++childIndex) {{")
        to.write(f"    // Reset all remaining animations to not have the property set.")
        to.write(f"    list.animation(childIndex).clear{property.name_for_methods}();")
        to.write(f"}}")

    # Font property setters.

    def _generate_font_property_initial_value_setter(self, to, property):
        to.write(f"auto fontDescription = builderState.fontDescription();")
        to.write(f"fontDescription.{property.codegen_properties.setter}(FontCascadeDescription::{property.codegen_properties.initial}());")
        to.write(f"builderState.setFontDescription(WTFMove(fontDescription));")

    def _generate_font_property_inherit_value_setter(self, to, property):
        to.write(f"auto fontDescription = builderState.fontDescription();")
        to.write(f"fontDescription.{property.codegen_properties.setter}(builderState.parentFontDescription().{property.codegen_properties.getter}());")
        to.write(f"builderState.setFontDescription(WTFMove(fontDescription));")

    def _generate_font_property_value_setter(self, to, property, value):
        to.write(f"auto fontDescription = builderState.fontDescription();")
        to.write(f"fontDescription.{property.codegen_properties.setter}({value});")
        to.write(f"builderState.setFontDescription(WTFMove(fontDescription));")

    # Fill Layer property setters.

    def _generate_fill_layer_property_initial_value_setter(self, to, property):
        initial = f"FillLayer::{property.codegen_properties.initial}({property.enum_name_for_layers_type})"
        to.write(f"// Check for (single-layer) no-op before clearing anything.")
        to.write(f"auto& layers = builderState.style().{property.method_name_for_layers}();")
        to.write(f"if (!layers.next() && (!layers.is{property.name_for_methods}Set() || layers.{property.codegen_properties.getter}() == {initial}))")
        to.write(f"    return;")
        to.write(f"auto* child = &builderState.style().{property.method_name_for_ensure_layers}();")
        to.write(f"child->{property.codegen_properties.setter}({initial});")
        to.write(f"for (child = child->next(); child; child = child->next())")
        to.write(f"    child->clear{property.name_for_methods}();")

    def _generate_fill_layer_property_inherit_value_setter(self, to, property):
        to.write(f"// Check for no-op before copying anything.")
        to.write(f"if (builderState.parentStyle().{property.method_name_for_layers}() == builderState.style().{property.method_name_for_layers}())")
        to.write(f"    return;")
        to.write(f"auto* child = &builderState.style().{property.method_name_for_ensure_layers}();")
        to.write(f"FillLayer* previousChild = nullptr;")
        to.write(f"for (auto* parent = &builderState.parentStyle().{property.method_name_for_layers}(); parent && parent->is{property.name_for_methods}Set(); parent = parent->next()) {{")
        to.write(f"    if (!child) {{")
        to.write(f"        previousChild->setNext(FillLayer::create({property.enum_name_for_layers_type}));")
        to.write(f"        child = previousChild->next();")
        to.write(f"    }}")
        to.write(f"    child->{property.codegen_properties.setter}(parent->{property.codegen_properties.getter}());")
        to.write(f"    previousChild = child;")
        to.write(f"    child = previousChild->next();")
        to.write(f"}}")
        to.write(f"for (; child; child = child->next())")
        to.write(f"    child->clear{property.name_for_methods}();")

    def _generate_fill_layer_property_value_setter(self, to, property):
        to.write(f"auto* child = &builderState.style().{property.method_name_for_ensure_layers}();")
        to.write(f"FillLayer* previousChild = nullptr;")
        to.write(f"if (is<CSSValueList>(value) && !is<CSSImageSetValue>(value)) {{")
        to.write(f"    // Walk each value and put it into a layer, creating new layers as needed.")
        to.write(f"    for (auto& item : downcast<CSSValueList>(value)) {{")
        to.write(f"        if (!child) {{")
        to.write(f"            previousChild->setNext(FillLayer::create({property.enum_name_for_layers_type}));")
        to.write(f"            child = previousChild->next();")
        to.write(f"        }}")
        to.write(f"        builderState.styleMap().mapFill{property.name_for_methods}(id, *child, item);")
        to.write(f"        previousChild = child;")
        to.write(f"        child = child->next();")
        to.write(f"    }}")
        to.write(f"}} else {{")
        to.write(f"    builderState.styleMap().mapFill{property.name_for_methods}(id, *child, value);")
        to.write(f"    child = child->next();")
        to.write(f"}}")
        to.write(f"for (; child; child = child->next())")
        to.write(f"    child->clear{property.name_for_methods}();")

    # SVG property setters.

    def _generate_svg_property_initial_value_setter(self, to, property):
        to.write(f"builderState.style().accessSVGStyle().{property.codegen_properties.setter}(SVGRenderStyle::{property.codegen_properties.initial}());")

    def _generate_svg_property_inherit_value_setter(self, to, property):
        to.write(f"builderState.style().accessSVGStyle().{property.codegen_properties.setter}(forwardInheritedValue(builderState.parentStyle().svgStyle().{property.codegen_properties.getter}()));")

    def _generate_svg_property_value_setter(self, to, property, value):
        to.write(f"builderState.style().accessSVGStyle().{property.codegen_properties.setter}({value});")

    # All other property setters.

    def _generate_property_initial_value_setter(self, to, property):
        to.write(f"builderState.style().{property.codegen_properties.setter}(RenderStyle::{property.codegen_properties.initial}());")

    def _generate_property_inherit_value_setter(self, to, property):
        to.write(f"builderState.style().{property.codegen_properties.setter}(forwardInheritedValue(builderState.parentStyle().{property.codegen_properties.getter}()));")

    def _generate_property_value_setter(self, to, property, value):
        to.write(f"builderState.style().{property.codegen_properties.setter}({value});")

    # Property setter dispatch.

    def _generate_style_builder_generated_cpp_initial_value_setter(self, to, property):
        to.write(f"static void applyInitial{property.id_without_prefix}(BuilderState& builderState)")
        to.write(f"{{")

        with to.indent():
            if property.codegen_properties.auto_functions:
                to.write(f"builderState.style().setHasAuto{property.name_for_methods}();")
            elif property.codegen_properties.visited_link_color_support:
                self._generate_color_property_initial_value_setter(to, property)
            elif property.animatable:
                self._generate_animation_property_initial_value_setter(to, property)
            elif property.codegen_properties.font_property:
                self._generate_font_property_initial_value_setter(to, property)
            elif property.codegen_properties.fill_layer_property:
                self._generate_fill_layer_property_initial_value_setter(to, property)
            elif property.codegen_properties.svg:
                self._generate_svg_property_initial_value_setter(to, property)
            else:
                self._generate_property_initial_value_setter(to, property)

            if property.codegen_properties.fast_path_inherited:
                to.write(f"builderState.style().setDisallowsFastPathInheritance();")

        to.write(f"}}")

    def _generate_style_builder_generated_cpp_inherit_value_setter(self, to, property):
        to.write(f"static void applyInherit{property.id_without_prefix}(BuilderState& builderState)")
        to.write(f"{{")

        with to.indent():
            if property.codegen_properties.auto_functions:
                to.write(f"if (builderState.parentStyle().hasAuto{property.name_for_methods}()) {{")
                with to.indent():
                    to.write(f"builderState.style().setHasAuto{property.name_for_methods}();")
                    to.write(f"return;")
                to.write(f"}}")

                if property.codegen_properties.svg:
                    self._generate_svg_property_inherit_value_setter(to, property)
                else:
                    self._generate_property_inherit_value_setter(to, property)
            elif property.codegen_properties.visited_link_color_support:
                self._generate_color_property_inherit_value_setter(to, property)
            elif property.animatable:
                self._generate_animation_property_inherit_value_setter(to, property)
            elif property.codegen_properties.font_property:
                self._generate_font_property_inherit_value_setter(to, property)
            elif property.codegen_properties.fill_layer_property:
                self._generate_fill_layer_property_inherit_value_setter(to, property)
            elif property.codegen_properties.svg:
                self._generate_svg_property_inherit_value_setter(to, property)
            else:
                self._generate_property_inherit_value_setter(to, property)

            if property.codegen_properties.fast_path_inherited:
                to.write(f"builderState.style().setDisallowsFastPathInheritance();")

        to.write(f"}}")

    def _generate_style_builder_generated_cpp_value_setter(self, to, property):
        if property.codegen_properties.fill_layer_property:
            to.write(f"static void applyValue{property.id_without_prefix}(CSSPropertyID id, BuilderState& builderState, CSSValue& value)")
        else:
            to.write(f"static void applyValue{property.id_without_prefix}(BuilderState& builderState, CSSValue& value)")
        to.write(f"{{")

        with to.indent():
            def converted_value(property):
                if property.codegen_properties.converter:
                    return f"BuilderConverter::convert{property.codegen_properties.converter}(builderState, value)"
                elif property.codegen_properties.conditional_converter:
                    return f"WTFMove(convertedValue.value())"
                elif property.codegen_properties.color_property and not property.codegen_properties.visited_link_color_support:
                    return f"builderState.colorFromPrimitiveValue(downcast<CSSPrimitiveValue>(value), ForVisitedLink::No)"
                else:
                    return "downcast<CSSPrimitiveValue>(value)"

            if property in self.style_properties.all_by_name["font"].codegen_properties.longhands and "Initial" not in property.codegen_properties.custom and not property.codegen_properties.converter:
                to.write(f"if (is<CSSPrimitiveValue>(value) && CSSPropertyParserHelpers::isSystemFontShorthand(downcast<CSSPrimitiveValue>(value).valueID())) {{")
                with to.indent():
                    to.write(f"applyInitial{property.id_without_prefix}(builderState);")
                    to.write(f"return;")
                to.write(f"}}")

            if property.codegen_properties.auto_functions:
                to.write(f"if (downcast<CSSPrimitiveValue>(value).valueID() == CSSValueAuto) {{")
                with to.indent():
                    to.write(f"builderState.style().setHasAuto{property.name_for_methods}();")
                    to.write(f"return;")
                to.write(f"}}")
                if property.codegen_properties.svg:
                    self._generate_svg_property_value_setter(to, property, converted_value(property))
                else:
                    self._generate_property_value_setter(to, property, converted_value(property))
            elif property.codegen_properties.visited_link_color_support:
                self._generate_color_property_value_setter(to, property, converted_value(property))
            elif property.animatable:
                self._generate_animation_property_value_setter(to, property)
            elif property.codegen_properties.font_property:
                self._generate_font_property_value_setter(to, property, converted_value(property))
            elif property.codegen_properties.fill_layer_property:
                self._generate_fill_layer_property_value_setter(to, property)
            else:
                if property.codegen_properties.conditional_converter:
                    to.write(f"auto convertedValue = BuilderConverter::convert{property.codegen_properties.conditional_converter}(builderState, value);")
                    to.write(f"if (convertedValue)")
                    with to.indent():
                        if property.codegen_properties.svg:
                            self._generate_svg_property_value_setter(to, property, converted_value(property))
                        else:
                            self._generate_property_value_setter(to, property, converted_value(property))
                else:
                    if property.codegen_properties.svg:
                        self._generate_svg_property_value_setter(to, property, converted_value(property))
                    else:
                        self._generate_property_value_setter(to, property, converted_value(property))

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
                if property.codegen_properties.skip_builder:
                    continue
                if property.codegen_properties.synonym:
                    continue

                if property.codegen_properties.is_logical:
                    raise Exception(f"Property '{property.name}' is logical but doesn't have skip-builder.")

                if "Initial" not in property.codegen_properties.custom:
                    self._generate_style_builder_generated_cpp_initial_value_setter(to, property)
                if "Inherit" not in property.codegen_properties.custom:
                    self._generate_style_builder_generated_cpp_inherit_value_setter(to, property)
                if "Value" not in property.codegen_properties.custom:
                    self._generate_style_builder_generated_cpp_value_setter(to, property)

        to.write(f"}};")

    def _generate_style_builder_generated_cpp_builder_generated_apply(self, *, to):
        to.write_block("""
            void BuilderGenerated::applyProperty(CSSPropertyID id, BuilderState& builderState, CSSValue& value, bool isInitial, bool isInherit, const CSSRegisteredCustomProperty* registered)
            {
                switch (id) {
                case CSSPropertyID::CSSPropertyInvalid:
                    break;
                case CSSPropertyID::CSSPropertyCustom:
                    if (isInitial)
                        BuilderCustom::applyInitialCustomProperty(builderState, registered, downcast<CSSCustomPropertyValue>(value).name());
                    else if (isInherit)
                        BuilderCustom::applyInheritCustomProperty(builderState, registered, downcast<CSSCustomPropertyValue>(value).name());
                    else
                        BuilderCustom::applyValueCustomProperty(builderState, registered, downcast<CSSCustomPropertyValue>(value));
                    break;""")

        with to.indent():
            def scope_for_function(property, function):
                if function in property.codegen_properties.custom:
                    return "BuilderCustom"
                return "BuilderFunctions"

            for property in self.properties_and_descriptors.all_unique:
                if not isinstance(property, StyleProperty):
                    to.write(f"case {property.id}:")
                    with to.indent():
                        to.write(f"break;")
                    continue
                
                if property.codegen_properties.synonym:
                    continue

                to.write(f"case {property.id}:")

                for synonymous_property in property.synonymous_properties:
                    to.write(f"case {synonymous_property.id}:")

                with to.indent():
                    if property.codegen_properties.longhands:
                        to.write(f"ASSERT(isShorthandCSSProperty(id));")
                        to.write(f"ASSERT_NOT_REACHED();")
                    elif not property.codegen_properties.skip_builder:
                        apply_initial_arguments = ["builderState"]
                        apply_inherit_arguments = ["builderState"]
                        apply_value_arguments = ["builderState", "value"]
                        if property.codegen_properties.fill_layer_property:
                            apply_value_arguments.insert(0, "id")

                        to.write(f"if (isInitial)")
                        with to.indent():
                            to.write(f"{scope_for_function(property, 'Initial')}::applyInitial{property.id_without_prefix}({', '.join(apply_initial_arguments)});")
                        to.write(f"else if (isInherit)")
                        with to.indent():
                            to.write(f"{scope_for_function(property, 'Inherit')}::applyInherit{property.id_without_prefix}({', '.join(apply_inherit_arguments)});")
                        to.write(f"else")
                        with to.indent():
                            to.write(f"{scope_for_function(property, 'Value')}::applyValue{property.id_without_prefix}({', '.join(apply_value_arguments)});")

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
                    "RenderStyle.h",
                    "StyleBuilderConverter.h",
                    "StyleBuilderCustom.h",
                    "StyleBuilderState.h",
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
                to.write(f"return StylePropertyShorthand({property.id}, {property.id_without_prefix_with_lowercase_first_letter}Properties);")
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
        ParsingCollection = collections.namedtuple('ParsingCollection', ['id', 'name', 'noun', 'supports_current_shorthand', 'consumers'])

        result = []
        for set in self.properties_and_descriptors.all_sets:
            result += [ParsingCollection(set.id, set.name, set.noun, set.supports_current_shorthand, list(self.property_consumers[property] for property in set.all))]
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
                        "CSSValue"
                    ],
                    structs=[
                        "CSSParserContext"
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
                    "CSSPropertyParserWorkerSafe.h",
                    "CSSValuePool.h",
                    "DeprecatedGlobalSettings.h",
                ]
            )

            with self.generation_context.namespace("WebCore", to=writer):
                self.generation_context.generate_using_namespace_declarations(
                    to=writer,
                    namespaces=["CSSPropertyParserHelpers", "CSSPropertyParserHelpersWorkerSafe"]
                )

                self._generate_css_property_parsing_cpp_property_parsing_functions(
                    to=writer
                )

                for parsing_collection in self.all_property_parsing_collections:
                    self._generate_css_property_parsing_cpp_parse_property(
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
                to.write(f"// Parse and return a single longhand {parsing_collection.name} {parsing_collection.noun}.")
                if parsing_collection.supports_current_shorthand:
                    to.write(f"static RefPtr<CSSValue> parse{parsing_collection.id}(CSSParserTokenRange&, CSSPropertyID id, CSSPropertyID currentShorthand, const CSSParserContext&);")
                else:
                    to.write(f"static RefPtr<CSSValue> parse{parsing_collection.id}(CSSParserTokenRange&, CSSPropertyID id, const CSSParserContext&);")
                to.write(f"// Fast path bare-keyword support.")
                to.write(f"static bool isKeywordValidFor{parsing_collection.id}(CSSPropertyID, CSSValueID, const CSSParserContext&);")
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
            to.write(f"bool CSSPropertyParsing::isKeywordValidFor{parsing_collection.id}(CSSPropertyID, CSSValueID, const CSSParserContext&)")
            to.write(f"{{")
            with to.indent():
                to.write(f"return false;")
            to.write(f"}}")
            to.newline()
            return

        requires_context = any(propopery_consumer.keyword_fast_path_generator.requires_context for propopery_consumer in keyword_fast_path_eligible_property_consumers)

        self.generation_context.generate_property_id_switch_function(
            to=to,
            signature=f"bool CSSPropertyParsing::isKeywordValidFor{parsing_collection.id}(CSSPropertyID id, CSSValueID keyword, const CSSParserContext&{' context' if requires_context else ''})",
            iterable=keyword_fast_path_eligible_property_consumers,
            mapping=lambda property_consumer: f"return {property_consumer.keyword_fast_path_generator.generate_call_string(keyword_string='keyword', context_string='context')};",
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

    def _generate_css_property_parsing_cpp_parse_property(self, *, to, parsing_collection):
        if parsing_collection.supports_current_shorthand:
            to.write(f"RefPtr<CSSValue> CSSPropertyParsing::parse{parsing_collection.id}(CSSParserTokenRange& range, CSSPropertyID id, CSSPropertyID currentShorthand, const CSSParserContext& context)")
            current_shorthand_string = "currentShorthand"
        else:
            to.write(f"RefPtr<CSSValue> CSSPropertyParsing::parse{parsing_collection.id}(CSSParserTokenRange& range, CSSPropertyID id, const CSSParserContext& context)")
            current_shorthand_string = None

        to.write(f"{{")
        with to.indent():
            to.write(f"if (!isExposed(id, context.propertySettings) && !isInternal(id)) {{")
            with to.indent():
                to.write(f"// Allow internal properties as we use them to parse several internal-only-shorthands (e.g. background-repeat),")
                to.write(f"// and to handle certain DOM-exposed values (e.g. -webkit-font-size-delta from execCommand('FontSizeDelta')).")
                to.write(f"ASSERT_NOT_REACHED();")
                to.write(f"return nullptr;")
            to.write(f"}}")

            # Build up a list of pairs of (property, return-expression-to-use-for-property).

            PropertyReturnExpression = collections.namedtuple('PropertyReturnExpression', ['property', 'return_expression'])
            property_and_return_expressions = []

            for consumer in parsing_collection.consumers:
                return_expression = consumer.generate_call_string(
                    range_string="range",
                    id_string="id",
                    current_shorthand_string=current_shorthand_string,
                    context_string="context")

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
                to.write(f"return nullptr;")
            to.write(f"}}")
        to.write(f"}}")
        to.newline()


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


# The `TermGenerator` classes help generate parser functions by providing
# generation of parsing text for a term or set of terms.
class TermGenerator(object):
    def make(term, keyword_fast_path_generator=None):
        if isinstance(term, MatchOneTerm):
            return TermGeneratorMatchOneTerm(term, keyword_fast_path_generator)
        elif isinstance(term, CommaSeparatedRepetitionTerm):
            return TermGeneratorCommaSeparatedRepetitionTerm(term)
        elif isinstance(term, ReferenceTerm):
            return TermGeneratorReferenceTerm(term)
        else:
            raise Exception(f"Unknown term type - {type(term)} - {term}")


class TermGeneratorCommaSeparatedRepetitionTerm(TermGenerator):
    def __init__(self, term):
        self.term = term
        self.repeated_term_generator = TermGenerator.make(term.repeated_term, None)
        self.requires_context = self.repeated_term_generator.requires_context

    def generate_conditional(self, *, to, range_string, context_string):
        self._generate_lambda(to=to, range_string=range_string, context_string=context_string)
        to.write(f"if (auto result = {self._generate_call_string(range_string=range_string, context_string=context_string)})")
        with to.indent():
            to.write(f"return result;")

    def generate_unconditional(self, *, to, range_string, context_string):
        self._generate_lambda(to=to, range_string=range_string, context_string=context_string)
        to.write(f"return {self._generate_call_string(range_string=range_string, context_string=context_string)};")

    def _generate_lambda(self, *, to, range_string, context_string):
        lambda_declaration_paramaters = ["CSSParserTokenRange& range"]
        if self.repeated_term_generator.requires_context:
            lambda_declaration_paramaters += ["const CSSParserContext& context"]

        to.write(f"auto lambda = []({', '.join(lambda_declaration_paramaters)}) -> RefPtr<CSSValue> {{")
        with to.indent():
            self.repeated_term_generator.generate_unconditional(to=to, range_string="range", context_string="context")
        to.write(f"}};")

    def _generate_call_string(self, *, range_string, context_string):
        parameters = [range_string, "lambda"]
        if self.repeated_term_generator.requires_context:
            parameters += [context_string]

        with_or_without = 'With' if self.term.single_value_optimization else 'Without'
        return f"consumeCommaSeparatedList{with_or_without}SingleValueOptimization({', '.join(parameters)})"


class TermGeneratorMatchOneTerm(TermGenerator):
    def __init__(self, term, keyword_fast_path_generator=None):
        self.term = term
        self.keyword_fast_path_generator = keyword_fast_path_generator
        self.term_generators = TermGeneratorMatchOneTerm._build_term_generators(term, keyword_fast_path_generator)
        self.requires_context = any(term_generator.requires_context for term_generator in self.term_generators)

    @staticmethod
    def _build_term_generators(term, keyword_fast_path_generator):
        # Partition the sub-terms into keywords and references (and eventually more things):
        fast_path_keyword_terms = []
        non_fast_path_keyword_terms = []
        reference_terms = []
        repetition_terms = []

        for sub_term in term.terms:
            if isinstance(sub_term, KeywordTerm):
                if keyword_fast_path_generator and sub_term.is_eligible_for_fast_path:
                    fast_path_keyword_terms.append(sub_term)
                else:
                    non_fast_path_keyword_terms.append(sub_term)
            elif isinstance(sub_term, ReferenceTerm):
                reference_terms.append(sub_term)
            elif isinstance(sub_term, CommaSeparatedRepetitionTerm):
                repetition_terms.append(sub_term)
            else:
                raise Exception(f"Only KeywordTerm, ReferenceTerm and CommaSeparatedRepetitionTerm terms are supported inside MatchOneTerm at this time: '{term}' - {sub_term}")

        # Build a list of generators for the terms, starting with all (if any) the keywords at once.
        term_generators = []

        if fast_path_keyword_terms:
            term_generators += [TermGeneratorFastPathKeywordTerms(keyword_fast_path_generator)]
        if non_fast_path_keyword_terms:
            term_generators += [TermGeneratorNonFastPathKeywordTerm(non_fast_path_keyword_terms)]
        if reference_terms:
            term_generators += [TermGeneratorReferenceTerm(sub_term) for sub_term in reference_terms]
        if repetition_terms:
            term_generators += [TermGeneratorCommaSeparatedRepetitionTerm(sub_term) for sub_term in repetition_terms]
        return term_generators

    def generate_conditional(self, *, to, range_string, context_string):
        # For any remaining generators, call the consume function and return the result if non-null.
        for term_generator in self.term_generators:
            term_generator.generate_conditional(to=to, range_string=range_string, context_string=context_string)

    def generate_unconditional(self, *, to, range_string, context_string):
        # Pop the last generator off, as that one will be the special, non-if case.
        *remaining_term_generators, last_term_generator = self.term_generators

        # For any remaining generators, call the consume function and return the result if non-null.
        for term_generator in remaining_term_generators:
            term_generator.generate_conditional(to=to, range_string=range_string, context_string=context_string)

        # And finally call that last generator we popped of back.
        last_term_generator.generate_unconditional(to=to, range_string=range_string, context_string=context_string)


# Generation support for a single `ReferenceTerm`.
class TermGeneratorReferenceTerm(TermGenerator):
    def __init__(self, term):
        self.term = term

    def generate_conditional(self, *, to, range_string, context_string):
        to.write(f"if (auto result = {self.generate_call_string(range_string=range_string, context_string=context_string)})")
        with to.indent():
            to.write(f"return result;")

    def generate_unconditional(self, *, to, range_string, context_string):
        to.write(f"return {self.generate_call_string(range_string=range_string, context_string=context_string)};")

    def generate_call_string(self, *, range_string, context_string):
        if self.term.is_builtin:
            builtin = self.term.builtin
            if isinstance(builtin, BuiltinAngleConsumer):
                return f"{builtin.consume_function_name}({range_string}, {context_string}.mode, {builtin.unitless}, {builtin.unitless_zero})"
            elif isinstance(builtin, BuiltinTimeConsumer):
                return f"{builtin.consume_function_name}({range_string}, {context_string}.mode, {builtin.value_range}, {builtin.unitless})"
            elif isinstance(builtin, BuiltinLengthConsumer):
                return f"{builtin.consume_function_name}({range_string}, {builtin.mode or f'{context_string}.mode'}, {builtin.value_range}, {builtin.unitless})"
            elif isinstance(builtin, BuiltinLengthPercentageConsumer):
                return f"{builtin.consume_function_name}({range_string}, {builtin.mode or f'{context_string}.mode'}, {builtin.value_range}, {builtin.unitless})"
            elif isinstance(builtin, BuiltinIntegerConsumer):
                return f"{builtin.consume_function_name}({range_string}, {builtin.value_range})"
            elif isinstance(builtin, BuiltinNumberConsumer):
                return f"{builtin.consume_function_name}({range_string}, {builtin.value_range})"
            elif isinstance(builtin, BuiltinPercentageConsumer):
                return f"{builtin.consume_function_name}({range_string}, {builtin.value_range})"
            elif isinstance(builtin, BuiltinPositionConsumer):
                return f"{builtin.consume_function_name}({range_string}, {context_string}.mode, {builtin.unitless}, PositionSyntax::Position)"
            elif isinstance(builtin, BuiltinColorConsumer):
                if builtin.quirky_colors:
                    return f"{builtin.consume_function_name}({range_string}, {context_string}, {context_string}.mode == HTMLQuirksMode)"
                return f"{builtin.consume_function_name}({range_string}, {context_string})"
            elif isinstance(builtin, BuiltinResolutionConsumer):
                return f"{builtin.consume_function_name}({range_string})"
            elif isinstance(builtin, BuiltinStringConsumer):
                return f"{builtin.consume_function_name}({range_string})"
            elif isinstance(builtin, BuiltinCustomIdentConsumer):
                return f"{builtin.consume_function_name}({range_string})"
            elif isinstance(builtin, BuiltinDashedIdentConsumer):
                return f"{builtin.consume_function_name}({range_string})"
            elif isinstance(builtin, BuiltinURLConsumer):
                return f"{builtin.consume_function_name}({range_string})"
            elif isinstance(builtin, BuiltinDeclarationValueConsumer):
                return f"{builtin.consume_function_name}({range_string}, {context_string})"
            else:
                raise Exception(f"Unknown builtin type used: {builtin.name.name}")
        else:
            return f"consume{self.term.name.id_without_prefix}({range_string}, {context_string})"

    @property
    def requires_context(self):
        if self.term.is_builtin:
            builtin = self.term.builtin
            if isinstance(builtin, BuiltinAngleConsumer):
                return True
            elif isinstance(builtin, BuiltinTimeConsumer):
                return True
            elif isinstance(builtin, BuiltinLengthConsumer):
                return builtin.mode is None
            elif isinstance(builtin, BuiltinLengthPercentageConsumer):
                return builtin.mode is None
            elif isinstance(builtin, BuiltinIntegerConsumer):
                return False
            elif isinstance(builtin, BuiltinNumberConsumer):
                return False
            elif isinstance(builtin, BuiltinPercentageConsumer):
                return False
            elif isinstance(builtin, BuiltinPositionConsumer):
                return True
            elif isinstance(builtin, BuiltinColorConsumer):
                return True
            elif isinstance(builtin, BuiltinResolutionConsumer):
                return False
            elif isinstance(builtin, BuiltinStringConsumer):
                return False
            elif isinstance(builtin, BuiltinCustomIdentConsumer):
                return False
            elif isinstance(builtin, BuiltinDashedIdentConsumer):
                return False
            elif isinstance(builtin, BuiltinURLConsumer):
                return False
            elif isinstance(builtin, BuiltinDeclarationValueConsumer):
                return True
            else:
                raise Exception(f"Unknown builtin type used: {builtin.name.name}")
        else:
            return True


# Generation support for any keyword terms that are not fast-path eligible.
class TermGeneratorNonFastPathKeywordTerm(TermGenerator):
    def __init__(self, keyword_terms):
        self.keyword_terms = keyword_terms
        self.requires_context = any(keyword_term.requires_context for keyword_term in self.keyword_terms)

    def generate_conditional(self, *, to, range_string, context_string):
        self._generate(to=to, range_string=range_string, context_string=context_string, default_string="break")

    def generate_unconditional(self, *, to, range_string, context_string):
        self._generate(to=to, range_string=range_string, context_string=context_string, default_string="return nullptr")

    def _generate(self, *, to, range_string, context_string, default_string):
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
                    conditions.append(f"!{context_string}.{keyword_term.settings_flag}")
            if keyword_term.status == "internal":
                conditions.append(f"!isValueAllowedInMode(keyword, {context_string}.mode)")

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
                to.write(f"return CSSValuePool::singleton().createIdentifierValue({return_expression.return_value});")

        to.write(f"default:")
        with to.indent():
            to.write(f"{default_string};")

        to.write(f"}}")


# Generation support for a properties fast path eligable keyword terms.
class TermGeneratorFastPathKeywordTerms(TermGenerator):
    def __init__(self, keyword_fast_path_generator):
        self.keyword_fast_path_generator = keyword_fast_path_generator
        self.requires_context = keyword_fast_path_generator.requires_context

    def generate_conditional(self, *, to, range_string, context_string):
        to.write(f"if (auto result = {self.generate_call_string(range_string=range_string, context_string=context_string)})")
        with to.indent():
            to.write(f"return result;")

    def generate_unconditional(self, *, to, range_string, context_string):
        to.write(f"return {self.generate_call_string(range_string=range_string, context_string=context_string)};")

    def generate_call_string(self, *, range_string, context_string):
        # For root keyword terms we can utilize the `keyword-only fast path` function.
        parameters = [range_string, self.keyword_fast_path_generator.generate_reference_string()]
        if self.requires_context:
            parameters.append(context_string)
        return f"consumeIdent({', '.join(parameters)})"


# Used by the `PropertyConsumer` classes to generate a `keyword-only fast path` function
# (e.g. `isKeywordValidFor*`) for use both in the keyword only fast path and in the main
# `parse` function.
class KeywordFastPathGenerator:
    def __init__(self, name, keyword_terms):
        self.keyword_terms = keyword_terms
        self.requires_context = any(keyword_term.requires_context for keyword_term in keyword_terms)
        self.signature = KeywordFastPathGenerator._build_signature(name, self.requires_context)

    @staticmethod
    def _build_parameters(requires_context):
        parameters = [FunctionParameter("CSSValueID", "keyword")]
        if requires_context:
            parameters += [FunctionParameter("const CSSParserContext&", "context")]
        return parameters

    @staticmethod
    def _build_signature(name, requires_context):
        return FunctionSignature(
            result_type="bool",
            scope=None,
            name=name,
            parameters=KeywordFastPathGenerator._build_parameters(requires_context))

    def generate_reference_string(self):
        return self.signature.reference_string

    def generate_call_string(self, *, keyword_string, context_string):
        parameters = [keyword_string]
        if self.requires_context:
            parameters += [context_string]

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
                        return_expression.append(f"context.{keyword_term.settings_flag}")
                if keyword_term.status == "internal":
                    return_expression.append("isValueAllowedInMode(keyword, context.mode)")

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
        self.requires_context = self.term_generator.requires_context
        self.signature = GeneratedSharedGrammarRuleConsumer._build_signature(shared_grammar_rule, self.requires_context)

    def __str__(self):
        return "GeneratedSharedGrammarRuleConsumer"

    def __repr__(self):
        return self.__str__()

    @staticmethod
    def _build_parameters(requires_context):
        parameters = [FunctionParameter("CSSParserTokenRange&", "range")]
        if requires_context:
            parameters += [FunctionParameter("const CSSParserContext&", "context")]
        return parameters

    @staticmethod
    def _build_signature(shared_grammar_rule, requires_context):
        return FunctionSignature(
            result_type="RefPtr<CSSValue>",
            scope="CSSPropertyParsing",
            name=f"consume{shared_grammar_rule.name_for_methods.id_without_prefix}",
            parameters=GeneratedSharedGrammarRuleConsumer._build_parameters(requires_context))

    @property
    def is_exported(self):
        return True

    def generate_export_declaration(self, *, to):
        to.write(f"static {self.signature.declaration_string};")

    def generate_definition(self, *, to):
        to.write(f"{self.signature.definition_string}")
        to.write(f"{{")
        with to.indent():
            self.term_generator.generate_unconditional(to=to, range_string='range', context_string='context')
        to.write(f"}}")
        to.newline()


# Each CSS property has a corresponding `PropertyConsumer` which defines how that property's
# parsing works, if the parsing function for the property should be exported in the header for
# use in other areas of WebCore, and what fast paths it exposes. The current set of kinds of
# `PropertyConsumer` are:
#
#   - `SkipPropertyConsumer`:
#        Used when the property is not eligable for parsing, and should be skipped. Used for
#        descriptor-only properties, shorthand properties, and properties marked 'skip-parser`.
#
#   - `CustomPropertyConsumer`:
#        Used when the property has been marked with `custom-parser` or `parser-function`. These
#        property consumers never generate a `consume` function of their own, and call the defined
#        `consume` function (either based on the property name if `custom-parser` or the one declared
#        in `parser-function`) directly from the main `parse` function.
#
#   - `FastPathKeywordOnlyPropertyConsumer`:
#        The only allowed values for this property are fast path eligible keyword values. These property
#        consumers always emit a `keyword-only fast path` function (e.g. `isKeywordValidFor*`) and the
#        main `parse` function uses that fast path function directly (e.g. `consumeIdent(range, isKeywordValidFor*)`
#        This allows us to avoid making a `consume` function for the property in all cases except for
#        when the property has been marked explicity with `parser-exported`, in which case we do
#        generate a `consume` function to warp that invocation above.
#
#   - `DirectPropertyConsumer`:
#        Used when a property's only term is a single non-simplifiable reference term (e.g. [ <number> ]
#        or [ <color> ]. These property consumers call the referenced term directly from the main `parse`
#        function. This allows us to avoid making a `consume` function for the property in all cases
#        except for when the property has been marked explicity with `parser-exported`, in which case
#        we do generate a `consume` function to warp that invocation above.
#
#   - `GeneratedPropertyConsumer`:
#        Used for all other properties. Requires that `parser-grammar` has been defined. These property
#        consumers use the provided parser grammer to generate a dedicated `consume` function which is
#        called from the main `parse` function. If the parser grammar allows for any keyword only valid
#        parses (e.g. for the grammar [ none | <image> ], "none" is a valid keyword only parse), these
#        property consumers will also emit a `keyword-only fast path` function (e.g. `isKeywordValidFor*`)
#        and ensure that it is called from the main `isKeywordValidForStyleProperty` function.
#
# `PropertyConsumer` abstract interface:
#
#   def generate_call_string(self, *, range_string, id_string, current_shorthand_string, context_string):
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

    def generate_call_string(self, *, range_string, id_string, current_shorthand_string, context_string):
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


# Property consumer used for properties with `custom-parser` or `parser-function` defined.
class CustomPropertyConsumer(PropertyConsumer):
    def __init__(self, property):
        self.property = property

    def __str__(self):
        return f"CustomPropertyConsumer for {self.property}"

    def __repr__(self):
        return self.__str__()

    def generate_call_string(self, *, range_string, id_string, current_shorthand_string, context_string):
        parameters = []
        if self.property.codegen_properties.parser_requires_current_property:
            parameters.append(id_string)
        parameters.append(range_string)
        if self.property.codegen_properties.parser_requires_current_shorthand:
            parameters.append(current_shorthand_string)
        if self.property.codegen_properties.parser_requires_context:
            parameters.append(context_string)
        if self.property.codegen_properties.parser_requires_context_mode:
            parameters.append(f"{context_string}.mode")
        if self.property.codegen_properties.parser_requires_quirks_mode:
            parameters.append(f"{context_string}.mode == HTMLQuirksMode")
        if self.property.codegen_properties.parser_requires_value_pool:
            parameters.append("CSSValuePool::singleton()")
        parameters += self.property.codegen_properties.parser_requires_additional_parameters

        function = self.property.codegen_properties.parser_function

        # Merge the scope, function and parameters to form the final invocation.
        return f"{function}({', '.join(parameters)})"

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


# Property consumer used for properties with only fast-path eligable keyword terms in its grammar.
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
        if keyword_fast_path_generator.requires_context:
            parameters += [FunctionParameter("const CSSParserContext&", "context")]
        return parameters

    @staticmethod
    def _build_signature(property, keyword_fast_path_generator):
        return FunctionSignature(
            result_type="RefPtr<CSSValue>",
            scope=FastPathKeywordOnlyPropertyConsumer._build_scope(property),
            name=f"consume{property.name_for_parsing_methods}",
            parameters=FastPathKeywordOnlyPropertyConsumer._build_parameters(keyword_fast_path_generator))

    def generate_call_string(self, *, range_string, id_string=None, current_shorthand_string=None, context_string):
        # NOTE: Even in the case that we generate a `consume` function, we don't generate a call to it,
        # but rather always directly use `consumeIdent` with the keyword-only fast path predicate.
        return self.term_generator.generate_call_string(range_string=range_string, context_string=context_string)

    # For "direct" and "fast-path keyword only" consumers, we only generate the property specific
    # defintion if the property has been marked as exported.

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
                to.write(f"return {self.generate_call_string(range_string='range', context_string='context')};")
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
        if term_generator.requires_context:
            parameters += [FunctionParameter("const CSSParserContext&", "context")]
        return parameters

    @staticmethod
    def _build_signature(property, term_generator):
        return FunctionSignature(
            result_type="RefPtr<CSSValue>",
            scope=DirectPropertyConsumer._build_scope(property),
            name=f"consume{property.name_for_parsing_methods}",
            parameters=DirectPropertyConsumer._build_parameters(term_generator))

    def generate_call_string(self, *, range_string, id_string=None, current_shorthand_string=None, context_string):
        # NOTE: Even in the case that we generate a `consume` function for the case, we don't
        # generate a call to it, but rather always generate the consume function for the reference,
        # which is just as good and works in all cases.
        return self.term_generator.generate_call_string(range_string=range_string, context_string=context_string)

    # For "direct" and "fast-path keyword only" consumers, we only generate the property specific
    # defintion if the property has been marked as exported.

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
                self.term_generator.generate_unconditional(to=to, range_string='range', context_string='context')
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
        self.requires_context = self.term_generator.requires_context
        self.signature = GeneratedPropertyConsumer._build_signature(property, self.requires_context)

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
    def _build_parameters(property, requires_context):
        parameters = [FunctionParameter("CSSParserTokenRange&", "range")]
        if requires_context:
            parameters += [FunctionParameter("const CSSParserContext&", "context")]
        return parameters

    @staticmethod
    def _build_signature(property, requires_context):
        return FunctionSignature(
            result_type="RefPtr<CSSValue>",
            scope=GeneratedPropertyConsumer._build_scope(property),
            name=f"consume{property.name_for_parsing_methods}",
            parameters=GeneratedPropertyConsumer._build_parameters(property, requires_context))

    @staticmethod
    def _build_keyword_fast_path_generator(property):
        if property.codegen_properties.parser_grammar.has_fast_path_keyword_terms:
            return KeywordFastPathGenerator(f"isKeywordValidFor{property.name_for_parsing_methods}", property.codegen_properties.parser_grammar.fast_path_keyword_terms_sorted_by_name)
        return None

    def generate_call_string(self, *, range_string, id_string, current_shorthand_string, context_string):
        parameters = [range_string]
        if self.requires_context:
            parameters += [context_string]
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
            self.term_generator.generate_unconditional(to=to, range_string='range', context_string='context')
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
    FLOAT   = re.compile(r'\d+\.\d+')
    INT     = re.compile(r'\d+')

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

    # Identifiers.
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
            # in case pattern doesn't match send the charector as illegal
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
        if self.kind == BNFRepetitionModifier.Kind.EXACT:
            return '{' + self.min + '}'
        elif self.kind == BNFRepetitionModifier.Kind.AT_LEAST:
            return '{' + self.min + ',}'
        elif self.kind == BNFRepetitionModifier.Kind.BETWEEN:
            return '{' + self.min + ',' + self.max + '}'


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

    def __str__(self):
        if self.kind == BNFNodeMultiplier.Kind.ZERO_OR_ONE:
            return '?'
        elif self.kind == BNFNodeMultiplier.Kind.SPACE_SEPARATED_ZERO_OR_MORE:
            return '*'
        elif self.kind == BNFNodeMultiplier.Kind.SPACE_SEPARATED_ONE_OR_MORE:
            return '+'
        elif self.kind == BNFNodeMultiplier.Kind.SPACE_SEPARATED_EXACT:
            return '{' + self.range.min + '}'
        elif self.kind == BNFNodeMultiplier.Kind.SPACE_SEPARATED_AT_LEAST:
            return '{' + self.range.min + ',}'
        elif self.kind == BNFNodeMultiplier.Kind.SPACE_SEPARATED_BETWEEN:
            return '{' + self.range.min + ',' + self.range.max + '}'
        elif self.kind == BNFNodeMultiplier.Kind.COMMA_SEPARATED_ONE_OR_MORE:
            return '#'
        elif self.kind == BNFNodeMultiplier.Kind.COMMA_SEPARATED_EXACT:
            return '#' + '{' + self.range.min + '}'
        elif self.kind == BNFNodeMultiplier.Kind.COMMA_SEPARATED_AT_LEAST:
            return '#' + '{' + self.range.min + ',}'
        elif self.kind == BNFNodeMultiplier.Kind.COMMA_SEPARATED_BETWEEN:
            return '#' + '{' + self.range.min + ',' + self.range.max + '}'
        return ''

    def add(self, multiplier):
        if self.kind is None:
            if isinstance(multiplier, BNFRepetitionModifier):
                if multiplier.kind == BNFRepetitionModifier.Kind.EXACT:
                    self.kind = BNFNodeMultiplier.Kind.SPACE_SEPARATED_EXACT
                    self.range = mulitplier
                elif multiplier.kind == BNFRepetitionModifier.Kind.AT_LEAST:
                    self.kind = BNFNodeMultiplier.Kind.SPACE_SEPARATED_AT_LEAST
                    self.range = mulitplier
                elif multiplier.kind == BNFRepetitionModifier.Kind.BETWEEN:
                    self.kind = BNFNodeMultiplier.Kind.SPACE_SEPARATED_BETWEEN
                    self.range = mulitplier
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
                    self.range = mulitplier
                elif multiplier.kind == BNFRepetitionModifier.Kind.AT_LEAST:
                    self.kind = BNFNodeMultiplier.Kind.COMMA_SEPARATED_AT_LEAST
                    self.range = mulitplier
                elif multiplier.kind == BNFRepetitionModifier.Kind.BETWEEN:
                    self.kind = BNFNodeMultiplier.Kind.COMMA_SEPARATED_BETWEEN
                    self.range = mulitplier
            else:
                raise Exception("Invalid to stack a non-range multiplier on top of '#'.")
        elif self.kind == BNFNodeMultiplier.Kind.COMMA_SEPARATED_EXACT:
            raise Exception("Invalid to stack another multiplier on top of a comma modifier range multiplier.")
        elif self.kind == BNFNodeMultiplier.Kind.COMMA_SEPARATED_AT_LEAST:
            raise Exception("Invalid to stack another multiplier on top of a comma modifier range multiplier.")
        elif self.kind == BNFNodeMultiplier.Kind.COMMA_SEPARATED_BETWEEN:
            raise Exception("Invalid to stack another multiplier on top of a comma modifier range multiplier.")


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

    def __str__(self):
        return self.stringified_without_multipliers + str(self.multiplier)

    @property
    def stringified_without_multipliers(self):
        if self.kind != BNFGroupingNode.Kind.MATCH_ALL_ORDERED:
            join_string = ' ' + self.kind.name + ' '
        else:
            join_string = ' '

        members_string = join_string.join(str(member) for member in self.members)
        if self.is_initial:
            return members_string
        return '[ ' + members_string + ' ]'

    def add(self, member):
        self.members.append(member)

    @property
    def last(self):
        return self.members[-1]


class BNFReferenceNode:
    class RangeAttribute:
        def __init__(self):
            self.min = None
            self.max = None

        def __str__(self):
            return f"[{self.min},{self.max}]"

    def __init__(self, *, is_internal=False):
        self.name = None
        self.is_internal = is_internal
        self.attributes = []
        self.multiplier = BNFNodeMultiplier()

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

        if self.attributes:
            return prefix + str(self.name) + ' ' + ' '.join(str(attribute) for attribute in self.attributes) + suffix
        return prefix + str(self.name) + suffix

    def add_attribute(self, attribute):
        self.attributes.append(attribute)


class BNFKeywordNode:
    def __init__(self, keyword):
        self.keyword = keyword
        self.multiplier = BNFNodeMultiplier()

    def __str__(self):
        return self.stringified_without_multipliers + str(self.multiplier)

    @property
    def stringified_without_multipliers(self):
        return self.keyword


class BNFParserState(enum.Enum):
    UNKNOWN_GROUPING_INITIAL = enum.auto()
    UNKNOWN_GROUPING_SEEN_TERM = enum.auto()
    KNOWN_ORDERED_GROUPING = enum.auto()
    KNOWN_COMBINATOR_GROUPING_TERM_REQUIRED = enum.auto()
    KNOWN_COMBINATOR_GROUPING_COMBINATOR_OR_CLOSE_REQUIRED = enum.auto()
    INTERNAL_REFERENCE_INITIAL = enum.auto()
    INTERNAL_REFERENCE_SEEN_ID = enum.auto()
    REFERENCE_INITIAL = enum.auto()
    REFERENCE_SEEN_ID = enum.auto()
    REFERENCE_RANGE_INITIAL = enum.auto()
    REFERENCE_RANGE_SEEN_MIN = enum.auto()
    REFERENCE_RANGE_SEEN_MIN_AND_COMMA = enum.auto()
    REFERENCE_RANGE_SEEN_MAX = enum.auto()
    REPETITION_MODIFIER_INITIAL = enum.auto()
    REPETITION_MODIFIER_SEEN_MIN = enum.auto()
    REPETITION_MODIFIER_SEEN_MIN_AND_COMMA = enum.auto()
    REPETITION_MODIFIER_SEEN_MAX = enum.auto()
    DONE = enum.auto()


BNFParserStateInfo = collections.namedtuple("BNFParserStates", ["state", "node"])


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

    DEBUG_PRINT_STATE = 0
    DEBUG_PRINT_TOKENS = 0

    def __init__(self, parsing_context, key_path, data):
        self.parsing_context = parsing_context
        self.key_path = key_path
        self.data = data
        self.root = BNFGroupingNode(is_initial=True)
        self.state_stack = []
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
            BNFParserState.REFERENCE_SEEN_ID: BNFParser.parse_REFERENCE_SEEN_ID,
            BNFParserState.REFERENCE_RANGE_INITIAL: BNFParser.parse_REFERENCE_RANGE_INITIAL,
            BNFParserState.REFERENCE_RANGE_SEEN_MIN: BNFParser.parse_REFERENCE_RANGE_SEEN_MIN,
            BNFParserState.REFERENCE_RANGE_SEEN_MIN_AND_COMMA: BNFParser.parse_REFERENCE_RANGE_SEEN_MIN_AND_COMMA,
            BNFParserState.REFERENCE_RANGE_SEEN_MAX: BNFParser.parse_REFERENCE_RANGE_SEEN_MAX,
            BNFParserState.REPETITION_MODIFIER_INITIAL: BNFParser.parse_REPETITION_MODIFIER_INITIAL,
            BNFParserState.REPETITION_MODIFIER_SEEN_MIN: BNFParser.parse_REPETITION_MODIFIER_SEEN_MIN,
            BNFParserState.REPETITION_MODIFIER_SEEN_MIN_AND_COMMA: BNFParser.parse_REPETITION_MODIFIER_SEEN_MIN_AND_COMMA,
            BNFParserState.REPETITION_MODIFIER_SEEN_MAX: BNFParser.parse_REPETITION_MODIFIER_SEEN_MAX,
        }

        for token in BNFLexer(self.data):
            if token.name == BNF_ILLEGAL_TOKEN:
                raise Exception(f"Illegal token found while parsing grammar definition: {token}")

            state = self.state_stack[-1]

            if BNFParser.DEBUG_PRINT_STATE:
                print("STATE: " + state.state.name)
            if BNFParser.DEBUG_PRINT_TOKENS:
                print("TOKEN: " + str(token))
            PARSER_THUNKS[state.state](self, token, state)

        if self.state_stack[-1].state != BNFParserState.DONE:
            raise Exception(f"Unexpected state '{state.state.name}' after processing all tokens")

        return self.root

    def transition_top(self, *, to):
        self.state_stack[-1] = BNFParserStateInfo(to, self.state_stack[-1].node)

    def push(self, new_state, new_node):
        self.state_stack.append(BNFParserStateInfo(new_state, new_node))

    def pop(self):
        self.state_stack.pop()

    def unexpected(self, token, state):
        return Exception(f"Unexpected token '{token}' found while in state '{state.state.name}'")

    # COMMON ACTIONS.

    # Root BNFGroupingNode. Syntatically isn't surrounded by square brackets.
    def enter_initial_grouping(self):
        self.push(BNFParserState.UNKNOWN_GROUPING_INITIAL, self.root)

    def exit_initial_grouping(self, token, state):
        if state.node.is_initial:
            self.transition_top(to=BNFParserState.DONE)
            return
        raise self.unexpected(token, state)

    # Non-initial BNFGroupingNode. e.g. "[foo bar]", "[foo | bar]", etc.
    def enter_new_grouping(self, token, state):
        new_grouping = BNFGroupingNode()
        state.node.add(new_grouping)
        self.push(BNFParserState.UNKNOWN_GROUPING_INITIAL, new_grouping)

    def exit_grouping(self, token, state):
        if not state.node.is_initial:
            self.pop()
            return
        raise self.unexpected(token, state)

    # Internal BNFReferenceNodes. e.g. "<<values>>"
    def enter_new_internal_reference(self, token, state):
        new_reference = BNFReferenceNode(is_internal=True)
        state.node.add(new_reference)
        self.push(BNFParserState.INTERNAL_REFERENCE_INITIAL, new_reference)

    def exit_internal_reference(self, token, state):
        self.pop()

    # Non-internal BNFReferenceNodes. e.g. "<length>"
    def enter_new_reference(self, token, state):
        new_reference = BNFReferenceNode()
        state.node.add(new_reference)
        self.push(BNFParserState.REFERENCE_INITIAL, new_reference)

    def exit_reference(self, token, state):
        self.pop()

    # BNFRepetitionModifier. e.g. {A,B}
    def enter_new_repetition_modifier(self, token, state):
        new_repetition_modifier = BNFRepetitionModifier()
        state.node.last.multiplier.add(new_repetition_modifier)
        self.push(BNFParserState.REPETITION_MODIFIER_INITIAL, new_repetition_modifier)

    def exit_repetition_modifier(self, token, state):
        self.pop()

    # BNFReferenceNode.RangeAttribute. e.g. [0,inf]
    def enter_range_attribute(self, token, state):
        new_range_attribute = BNFReferenceNode.RangeAttribute()
        state.node.add_attribute(new_range_attribute)
        self.push(BNFParserState.REFERENCE_RANGE_INITIAL, new_range_attribute)

    def exit_range_attribute(self, token, state):
        self.pop()

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

        if token.name == BNFToken.ID:
            self.transition_top(to=BNFParserState.UNKNOWN_GROUPING_SEEN_TERM)
            state.node.add(BNFKeywordNode(token.value))
            return

        raise self.unexpected(token, state)

    def parse_UNKNOWN_GROUPING_SEEN_TERM(self, token, state):
        if token.name == BNFToken.RSQUARE:
            self.exit_grouping(token, state)
            return

        if token.name == BNF_EOF_TOKEN:
            self.exit_initial_grouping(token, state)
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

        if token.name == BNFToken.ID:
            self.transition_top(to=BNFParserState.KNOWN_ORDERED_GROUPING)
            state.node.add(BNFKeywordNode(token.value))
            return

        if token.name in BNFParser.COMBINATOR_FOR_TOKEN:
            self.transition_top(to=BNFParserState.KNOWN_COMBINATOR_GROUPING_TERM_REQUIRED)
            state.node.kind = BNFParser.COMBINATOR_FOR_TOKEN[token.name]
            return

        if token.name in BNFParser.SIMPLE_MULTIPLIERS:
            state.node.last.multiplier.add(token.value)
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
            state.node.add(BNFKeywordNode(token.value))
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

        if token.name == BNFToken.ID:
            self.transition_top(to=BNFParserState.KNOWN_COMBINATOR_GROUPING_COMBINATOR_OR_CLOSE_REQUIRED)
            state.node.add(BNFKeywordNode(token.value))
            return

        if token.name == BNFToken.RSQUARE or token.name == EOF:
            raise Exception(f"Unexpected token '{token}'. Groupings can't end in a combinator.")
        raise self.unexpected(token, state)

    def parse_KNOWN_COMBINATOR_GROUPING_COMBINATOR_OR_CLOSE_REQUIRED(self, token, state):
        if token.name == BNFToken.RSQUARE:
            self.exit_grouping(token, state)
            return

        if token.name == BNF_EOF_TOKEN:
            self.exit_initial_grouping(token, state)
            return

        if token.name in BNFParser.COMBINATOR_FOR_TOKEN:
            if state.node.kind == BNFParser.COMBINATOR_FOR_TOKEN[token.name]:
                self.transition_top(to=BNFParserState.KNOWN_COMBINATOR_GROUPING_TERM_REQUIRED)
                return
            raise Exception(f"Unexpected token '{token}'. Did you mean '{state.node.kind.name}'?.")

        if token.name in BNFParser.SIMPLE_MULTIPLIERS:
            state.node.last.multiplier.add(token.value)
            return

        if token.name == BNFToken.LBRACE:
            self.enter_new_repetition_modifier(token, state)
            return

        raise self.unexpected(token, state)

    def parse_REFERENCE_INITIAL(self, token, state):
        if token.name == BNFToken.LT:
            state.node.is_special = True
            return

        if token.name == BNFToken.ID:
            self.transition_top(to=BNFParserState.REFERENCE_SEEN_ID)
            state.node.name = token.value
            return

        raise self.unexpected(token, state)

    def parse_REFERENCE_SEEN_ID(self, token, state):
        if token.name == BNFToken.ID:
            state.node.add_attribute(token.value)
            # Remain in BNFParserState.REFERENCE_SEEN_ID.
            return

        if token.name == BNFToken.LSQUARE:
            self.enter_range_attribute(token, state)
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
            state.node.add_attribute(token.value)
            # Remain in BNFParserState.INTERNAL_REFERENCE_SEEN_ID.
            return

        if token.name == BNFToken.LSQUARE:
            self.enter_range_attribute(token, state)
            return

        if token.name == BNFToken.GTGT:
            self.exit_internal_reference(token, state)
            return

        raise self.unexpected(token, state)

    def parse_REFERENCE_RANGE_INITIAL(self, token, state):
        if token.name == BNFToken.INT or token.name == BNFToken.FLOAT or (token.name == BNFToken.ID and token.value == "inf"):
            self.transition_top(to=BNFParserState.REFERENCE_RANGE_SEEN_MIN)
            state.node.min = token.value
            return

        raise self.unexpected(token, state)

    def parse_REFERENCE_RANGE_SEEN_MIN(self, token, state):
        if token.name == BNFToken.COMMA:
            self.transition_top(to=BNFParserState.REFERENCE_RANGE_SEEN_MIN_AND_COMMA)
            return

        raise self.unexpected(token, state)

    def parse_REFERENCE_RANGE_SEEN_MIN_AND_COMMA(self, token, state):
        if token.name == BNFToken.INT or token.name == BNFToken.FLOAT or (token.name == BNFToken.ID and token.value == "inf"):
            self.transition_top(to=BNFParserState.REFERENCE_RANGE_SEEN_MAX)
            state.node.max = token.value
            return

        raise self.unexpected(token, state)

    def parse_REFERENCE_RANGE_SEEN_MAX(self, token, state):
        if token.name == BNFToken.RSQUARE:
            self.exit_range_attribute(token, state)
            return

        raise self.unexpected(token, state)

    def parse_REPETITION_MODIFIER_INITIAL(self, token, state):
        if token.name == BNFToken.INT:
            self.transition_top(to=BNFParserState.REPETITION_MODIFIER_SEEN_MIN)
            state.node.kind = BNFRepetitionModifier.Kind.EXACT
            state.node.min = token.value
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
            state.node.max = token.value
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


def main():
    parser = argparse.ArgumentParser(description='Process CSS property definitions.')
    parser.add_argument('--properties', default="CSSProperties.json")
    parser.add_argument('--defines')
    parser.add_argument('--gperf-executable')
    parser.add_argument('-v', '--verbose', action='store_true')
    args = parser.parse_args()

    with open(args.properties, "r") as properties_file:
        properties_json = json.load(properties_file)

    parsing_context = ParsingContext(properties_json, defines_string=args.defines, parsing_for_codegen=True, verbose=args.verbose)
    parsing_context.parse_shared_grammar_rules()
    parsing_context.parse_properties_and_descriptors()

    if args.verbose:
        print(f"{len(parsing_context.parsed_shared_grammar_rules.rules)} shared grammar rules active for code generation")
        for set in parsing_context.parsed_properties_and_descriptors.all_sets:
            print(f"{len(set.all)} {set.name} {set.noun} active for code generation")
        print(f"{len(parsing_context.parsed_properties_and_descriptors.all_unique)} uniquely named properties and descriptors active for code generation")

    generation_context = GenerationContext(parsing_context.parsed_properties_and_descriptors, parsing_context.parsed_shared_grammar_rules, verbose=args.verbose, gperf_executable=args.gperf_executable)

    generators = [
        GenerateCSSPropertyNames,
        GenerateCSSPropertyParsing,
        GenerateCSSStyleDeclarationPropertyNames,
        GenerateStyleBuilderGenerated,
        GenerateStylePropertyShorthandFunctions,
    ]

    for generator in generators:
        generator(generation_context).generate()


if __name__ == "__main__":
    main()
