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


class ParsingContext:
    def __init__(self, *, defines_string, parsing_for_codegen, verbose):
        if defines_string:
            self.conditionals = frozenset(defines_string.split(' '))
        else:
            self.conditionals = frozenset()
        self.parsing_for_codegen = parsing_for_codegen
        self.verbose = verbose

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


class Schema:
    class Entry:
        def __init__(self, key, *, allowed_types, default_value=None, required=False):
            if default_value and required:
                raise Exception(f"Invalid Schema.Entry for '{key}'. Cannot specify both 'default_value' and 'required'.")

            self.key = key
            self.allowed_types = allowed_types
            self.default_value = default_value
            self.required = required

    def __init__(self, *entries):
        self.entries = {entry.key: entry for entry in entries}

    def set_attributes_from_dictionary(self, dictionary, *, instance):
        for entry in self.entries.values():
            setattr(instance, entry.key.replace("-", "_"), dictionary.get(entry.key, entry.default_value))

    def validate_keys(self, dictionary, *, label):
        invalid_keys = list(filter(lambda key: key not in self.entries.keys(), dictionary.keys()))
        if len(invalid_keys) == 1:
            raise Exception(f"Invalid key for '{label}': {invalid_keys[0]}")
        if len(invalid_keys) > 1:
            raise Exception(f"Invalid keys for '{label}': {invalid_keys}")

    def validate_types(self, dictionary, *, label):
        for key, value in dictionary.items():
            if type(value) not in self.entries[key].allowed_types:
                raise Exception(f"Invalid type '{type(value)}' for key '{key}' in '{label}'. Expected type in set '{self.entries[key].allowed_types}'.")

    def validate_requirements(self, dictionary, *, label):
        for key, entry in self.entries.items():
            if entry.required and key not in dictionary:
                raise Exception(f"Required key '{key}' not found in '{label}'.")

    def validate_dictionary(self, dictionary, *, label):
        self.validate_keys(dictionary, label=label)
        self.validate_types(dictionary, label=label)
        self.validate_requirements(dictionary, label=label)


class Name(object):
    def __init__(self, name):
        self.name = name
        self.id_without_prefix = Name.convert_name_to_id(self.name)

    def __str__(self):
        return self.name

    def __repr__(self):
        return self.__str__()

    @staticmethod
    def convert_name_to_id(name):
        return re.sub(r'(^[^-])|-(.)', lambda m: (m[1] or m[2]).upper(), name)

    @property
    def id_without_prefix_with_lowercase_first_letter(self):
        return self.id_without_prefix[0].lower() + self.id_without_prefix[1:]


class PropertyName(Name):
    def __init__(self, name, *, name_for_methods):
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
        Status.schema.validate_dictionary(json_value, label=f"Status ({key_path}.status)")

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
        Specification.schema.validate_dictionary(json_value, label=f"Specification ({key_path}.specification)")
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
        Value.schema.validate_dictionary(json_value, label=f"Value ({key_path}.values)")

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
        LogicalPropertyGroup.schema.validate_dictionary(json_value, label=f"LogicalPropertyGroup ({key_path}.logical-property-group)")
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
            return Value(value=json_value)

        assert(type(json_value) is dict)
        Longhand.schema.validate_dictionary(json_value, label=f"Longhand ({key_path}.longhands)")

        if "enable-if" in json_value and not parsing_context.is_enabled(conditional=json_value["enable-if"]):
            if parsing_context.verbose:
                print(f"SKIPPED longhand {json_value['value']} in {key_path} due to failing to satisfy 'enable-if' condition, '{json_value['enable-if']}', with active macro set")
            return None

        return Longhand(**json_value)


class CodeGenProperties:
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
        Schema.Entry("descriptor-only", allowed_types=[bool], default_value=False),
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
        Schema.Entry("parser-requires-additional-parameters", allowed_types=[list], default_value=[]),
        Schema.Entry("parser-requires-context", allowed_types=[bool], default_value=False),
        Schema.Entry("parser-requires-context-mode", allowed_types=[bool], default_value=False),
        Schema.Entry("parser-requires-current-shorthand", allowed_types=[bool], default_value=False),
        Schema.Entry("parser-requires-current-property", allowed_types=[bool], default_value=False),
        Schema.Entry("parser-requires-quirks-mode", allowed_types=[bool], default_value=False),
        Schema.Entry("parser-requires-value-pool", allowed_types=[bool], default_value=False),
        Schema.Entry("partial-keyword-property", allowed_types=[bool], default_value=False),
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
        CodeGenProperties.schema.set_attributes_from_dictionary(dictionary, instance=self)
        self.property_name = property_name

    def __str__(self):
        return f"CodeGenProperties {vars(self)}"

    def __repr__(self):
        return self.__str__()

    @staticmethod
    def from_json(parsing_context, key_path, name, json_value):
        if type(json_value) is list:
            json_value = parsing_context.select_enabled_variant(json_value, label=f"{key_path}.codegen-properties")

        assert(type(json_value) is dict)
        CodeGenProperties.schema.validate_dictionary(json_value, label=f"CodeGenProperties ({key_path}.codegen-properties)")

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
            json_value["longhands"] = list(filter(lambda value: value is not None, map(lambda value: Longhand.from_json(parsing_context, f"{key_path}.codegen-properties", value), json_value["longhands"])))
            if not json_value["longhands"]:
                longhands = None

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

        return CodeGenProperties(property_name, **json_value)

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


class Property:
    schema = Schema(
        Schema.Entry("animatable", allowed_types=[bool], default_value=False),
        Schema.Entry("codegen-properties", allowed_types=[dict, list]),
        Schema.Entry("inherited", allowed_types=[bool], default_value=False),
        Schema.Entry("specification", allowed_types=[dict]),
        Schema.Entry("status", allowed_types=[dict, str]),
        Schema.Entry("values", allowed_types=[list]),
    )

    def __init__(self, **dictionary):
        Property.schema.set_attributes_from_dictionary(dictionary, instance=self)
        self.property_name = self.codegen_properties.property_name
        self.synonymous_properties = []
        self._values_sorted_by_name = None


    def __str__(self):
        return self.name

    def __repr__(self):
        return self.__str__()

    @staticmethod
    def from_json(parsing_context, key_path, name, json_value):
        assert(type(json_value) is dict)
        Property.schema.validate_dictionary(json_value, label=f"Property ({key_path}.{name})")

        codegen_properties = CodeGenProperties.from_json(parsing_context, f"{key_path}.{name}", name, json_value.get("codegen-properties", {}))
        json_value["codegen-properties"] = codegen_properties

        if "values" in json_value:
            json_value["values"] = list(filter(lambda value: value is not None, map(lambda value: Value.from_json(parsing_context, f"{key_path}.{name}", value), json_value["values"])))
        if "status" in json_value:
            json_value["status"] = Status.from_json(parsing_context, f"{key_path}.{name}", json_value["status"])
        if "specification" in json_value:
            json_value["specification"] = Specification.from_json(parsing_context, f"{key_path}.{name}", json_value["specification"])

        if parsing_context.parsing_for_codegen:
            if "codegen-properties" in json_value:
                if json_value["codegen-properties"].enable_if is not None and not parsing_context.is_enabled(conditional=json_value["codegen-properties"].enable_if):
                    if parsing_context.verbose:
                        print(f"SKIPPED {name} due to failing to satisfy 'enable-if' condition, '{json_value['codegen-properties'].enable_if}', with active macro set")
                    return None
                if json_value["codegen-properties"].skip_codegen is not None and json_value["codegen-properties"].skip_codegen:
                    if parsing_context.verbose:
                        print(f"SKIPPED {name} due to 'skip-codegen'")
                    return None

        return Property(**json_value)

    def perform_fixups_for_synonyms(self, all_properties):
        # If 'synonym' was specified, replace the name with references to the Property object, and vice-versa a back-reference on that Property object back to this.
        if self.codegen_properties.synonym:
            if self.codegen_properties.synonym not in all_properties.properties_by_name:
                raise Exception(f"Property {self.name} has an unknown synonym: {self.codegen_properties.synonym}.")

            original = all_properties.properties_by_name[self.codegen_properties.synonym]
            original.synonymous_properties.append(self)

            self.codegen_properties.synonym = original

    def perform_fixups_for_longhands(self, all_properties):
        # If 'longhands' was specified, replace the names with references to the Property objects.
        if self.codegen_properties.longhands:
            self.codegen_properties.longhands = [all_properties.properties_by_name[longhand.value] for longhand in self.codegen_properties.longhands]

    def perform_fixups_for_related_properties(self, all_properties):
        # If 'related-property' was specified, validate the relationship and replace the name with a reference to the Property object.
        if self.codegen_properties.related_property:
            if self.codegen_properties.related_property not in all_properties.properties_by_name:
                raise Exception(f"Property {self.name} has an unknown related property: {self.codegen_properties.related_property}.")

            related_property = all_properties.properties_by_name[self.codegen_properties.related_property]
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

    @property
    def values_sorted_by_name(self):
        if not self._values_sorted_by_name:
            self._values_sorted_by_name = sorted(self.values, key=functools.cmp_to_key(Properties._sort_with_prefixed_properties_last))
        return self._values_sorted_by_name

    @property
    def values_without_settings_flag(self):
        return (value for value in self.values_sorted_by_name if not value.settings_flag)

    @property
    def values_with_settings_flag(self):
        return (value for value in self.values_sorted_by_name if value.settings_flag)

    @property
    def a_value_requires_a_settings_flag_or_is_internal(self):
        for value in self.values:
            if value.settings_flag or value.status == "internal":
                return True
        return False

    @property
    def accepts_a_single_value_keyword(self):
        if self.codegen_properties.longhands:
            return False
        if self.codegen_properties.descriptor_only:
            return False
        if not self.values:
            return False
        if (self.codegen_properties.custom_parser or self.codegen_properties.parser_function) and not self.codegen_properties.partial_keyword_property:
            return False
        return True

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


class Properties:
    schema = Schema(
        Schema.Entry("categories", allowed_types=[dict], required=True),
        Schema.Entry("instructions", allowed_types=[list], required=True),
        Schema.Entry("properties", allowed_types=[dict], required=True),
    )

    def __init__(self, *properties):
        self.properties = properties
        self.properties_by_name = {property.name: property for property in properties}
        self.logical_property_groups = {}
        self._all = None
        self._all_computed = None
        self._settings_flags = None

    def __str__(self):
        return "Properties"

    def __repr__(self):
        return self.__str__()

    @staticmethod
    def from_json(parsing_context, top_level_object):
        Properties.schema.validate_dictionary(top_level_object, label="top level object")

        properties = Properties(
            *list(
                filter(
                    lambda value: value is not None,
                    map(
                        lambda item: Property.from_json(parsing_context, "$properties", item[0], item[1]),
                        top_level_object["properties"].items()
                    )
                )
            )
        )

        # Now that all the properties have been parsed, go back through them and replace
        # any references to other properties that were by name (e.g. string) with a direct
        # reference to the property object.
        for property in properties.properties:
            property.perform_fixups(properties)

        return properties

    # Returns the set of all properties. Default decreasing priority and name sorting.
    @property
    def all(self):
        if not self._all:
            self._all = sorted(self.properties, key=functools.cmp_to_key(Properties._sort_by_descending_priority_and_name))
        return self._all

    # Returns the set of all properties that are included in computed styles. Sorted lexically by name with prefixed properties last.
    @property
    def all_computed(self):
        if not self._all_computed:
            self._all_computed = sorted([property for property in self.all if not property.is_skipped_from_computed_style], key=functools.cmp_to_key(Properties._sort_with_prefixed_properties_last))
        return self._all_computed

    # Returns a generator for the set of properties that are conditionally included depending on settings. Default decreasing priority and name sorting.
    @property
    def all_with_settings_flag(self):
        return (property for property in self.all if property.codegen_properties.settings_flag)

    # Returns a generator for the set of properties that are marked internal-only. Default decreasing priority and name sorting.
    @property
    def all_internal_only(self):
        return (property for property in self.all if property.codegen_properties.internal_only)

    # Returns a generator for the set properties that are NOT marked internal. Default decreasing priority and name sorting.
    @property
    def all_non_internal_only(self):
        return (property for property in self.all if not property.codegen_properties.internal_only)

    # Returns a generator for the set of properties that have an associate longhand, the so-called shorthands. Default decreasing priority and name sorting.
    @property
    def all_shorthands(self):
        return (property for property in self.all if property.codegen_properties.longhands)

    # Returns a generator for the set of properties that do not have an associate longhand. Default decreasing priority and name sorting.
    @property
    def all_non_shorthands(self):
        return (property for property in self.all if not property.codegen_properties.longhands)

    # Returns a generator for the set of properties that can accept a single value keyword. Default decreasing priority and name sorting.
    @property
    def all_accepting_a_single_value_keyword(self):
        return (property for property in self.all if property.accepts_a_single_value_keyword)

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

    # Returns the set of settings-flags used by any property. Uniqued and sorted lexically.
    @property
    def settings_flags(self):
        if not self._settings_flags:
            self._settings_flags = sorted(list(set([property.codegen_properties.settings_flag for property in self.properties if property.codegen_properties.settings_flag])))
        return self._settings_flags

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

        return Properties._sort_with_prefixed_properties_last(a, b)

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


# GENERATION

class GenerationContext:
    def __init__(self, properties, *, verbose, gperf_executable):
        self.properties = properties
        self.verbose = verbose
        self.gperf_executable = gperf_executable

    # Shared generation constants.

    number_of_predefined_properties = 2

    # Runs `gperf` on the output of the generated file CSSPropertyNames.gperf

    def run_gperf(self):
        gperf_command = self.gperf_executable or os.environ['GPERF']

        gperf_result_code = subprocess.call([gperf_command, '--key-positions=*', '-D', '-n', '-s', '2', 'CSSPropertyNames.gperf', '--output-file=CSSPropertyNames.cpp'])
        if gperf_result_code != 0:
            raise Exception(f"Error when generating CSSPropertyNames.cpp from CSSPropertyNames.gperf: {gperf_result_code}")

    # Shared generator templates.

    def _generate_property_id_switch_function(self, *, to, signature, properties, mapping, default, prologue=None, epilogue=None):
        to.write(f"{signature}\n")
        to.write(f"{{\n")

        if prologue:
            to.write(textwrap.indent(prologue, '    ') + "\n")

        to.write(f"    switch (id) {{\n")

        for property in properties:
            to.write(f"    case {property.id}:\n")
            to.write(f"        {mapping(property)}\n")

        to.write(f"    default:\n")
        to.write(f"        {default}\n")
        to.write(f"    }}\n")

        if epilogue:
            to.write(textwrap.indent(epilogue, '    ') + "\n")

        to.write(f"}}\n\n")

    def _generate_property_id_switch_function_bool(self, *, to, signature, properties):
        to.write(f"{signature}\n")
        to.write(f"{{\n")
        to.write(f"    switch (id) {{\n")

        for property in properties:
            to.write(f"    case {property.id}:\n")

        to.write(f"        return true;\n")
        to.write(f"    default:\n")
        to.write(f"        return false;\n")
        to.write(f"    }}\n")
        to.write(f"}}\n\n")

    # MARK: - Helper generator functions for CSSPropertyNames.h

    def _generate_css_property_names_gperf_heading(self, *, to):
        to.write(textwrap.dedent("""\
            %{
            // This file is automatically generated from CSSProperties.json by the process-css-properties script. Do not edit it.

            #include "config.h"
            #include "CSSPropertyNames.h"

            #include "CSSProperty.h"
            #include "Settings.h"
            #include <wtf/ASCIICType.h>
            #include <wtf/Hasher.h>
            #include <wtf/text/AtomString.h>
            #include <string.h>

            IGNORE_WARNINGS_BEGIN("implicit-fallthrough")

            // Older versions of gperf like to use the `register` keyword.
            #define register

            namespace WebCore {

            // Using std::numeric_limits<uint16_t>::max() here would be cleaner,
            // but is not possible due to missing constexpr support in MSVC 2013.
            static_assert(numCSSProperties + 1 <= 65535, "CSSPropertyID should fit into uint16_t.");

            """))

        all_computed_property_ids = (f"{property.id}," for property in self.properties.all_computed)
        to.write(f"const std::array<CSSPropertyID, {count_iterable(self.properties.all_computed)}> computedPropertyIDs {{")
        to.write("\n    ")
        to.write("\n    ".join(all_computed_property_ids))
        to.write("\n};\n\n")

        all_property_name_strings = quote_iterable(self.properties.all, "_s,")
        to.write(f"constexpr ASCIILiteral propertyNameStrings[numCSSProperties] = {{")
        to.write("\n    ")
        to.write("\n    ".join(all_property_name_strings))
        to.write("\n};\n\n")

        to.write("%}\n")

    def _generate_css_property_names_gperf_footing(self, *, to):
        to.write(textwrap.dedent("""\
            } // namespace WebCore

            IGNORE_WARNINGS_END
            """))

    def _generate_gperf_definition(self, *, to):
        to.write(textwrap.dedent("""\
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
            %%
            """))

        # Concatenates a list of 'propererty-name, property-id' strings with a second list of 'property-alias, property-id' strings.
        all_property_names_and_aliases_with_ids = itertools.chain(
              [f'{property.name}, {property.id}'                        for property in self.properties.all],
            *[[f'{alias}, {property.id}' for alias in property.aliases] for property in self.properties.all]
        )
        to.write("\n".join(all_property_names_and_aliases_with_ids))

        to.write("\n%%\n")

    def _generate_lookup_functions(self, *, to):
        to.write(textwrap.dedent("""
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

            """))

    def _generate_physical_logical_conversion_function(self, *, to, signature, source, destination, resolver_enum_prefix):
        source_as_id = PropertyName.convert_name_to_id(source)
        destination_as_id = PropertyName.convert_name_to_id(destination)

        to.write(f"{signature}\n")
        to.write(f"{{\n")
        to.write(f"    auto textflow = makeTextFlow(writingMode, direction);\n")
        to.write(f"    switch (id) {{\n")

        for group_name, property_group in sorted(self.properties.logical_property_groups.items(), key=lambda x: x[0]):
            kind = property_group["kind"]
            kind_as_id = PropertyName.convert_name_to_id(kind)

            destinations = LogicalPropertyGroup.logical_property_group_resolvers[destination][kind]
            properties = [property_group[destination][a_destination].id for a_destination in destinations]

            for resolver, property in sorted(property_group[source].items(), key=lambda x: x[0]):
                resolver_as_id = PropertyName.convert_name_to_id(resolver)
                resolver_enum = f"{resolver_enum_prefix}{kind_as_id}::{resolver_as_id}"

                to.write(f"    case {property.id}: {{\n")
                to.write(f"        static constexpr CSSPropertyID properties[{len(properties)}] = {{ {', '.join(properties)} }};\n")
                to.write(f"        return properties[static_cast<size_t>(map{source_as_id}{kind_as_id}To{destination_as_id}{kind_as_id}(textflow, {resolver_enum}))];\n")
                to.write(f"    }}\n")
        to.write(f"    default:\n")
        to.write(f"        return id;\n")
        to.write(f"    }}\n")
        to.write(f"}}\n\n")

    def _generate_is_exposed_functions(self, *, to):
        self._generate_property_id_switch_function(
            to=to,
            signature="static bool isExposedNotInvalidAndNotInternal(CSSPropertyID id, const CSSPropertySettings& settings)",
            properties=self.properties.all_with_settings_flag,
            mapping=lambda p: f"return settings.{p.codegen_properties.settings_flag};",
            default="return true;"
        )

        self._generate_property_id_switch_function(
            to=to,
            signature="static bool isExposedNotInvalidAndNotInternal(CSSPropertyID id, const Settings& settings)",
            properties=self.properties.all_with_settings_flag,
            mapping=lambda p: f"return settings.{p.codegen_properties.settings_flag}();",
            default="return true;"
        )

        to.write(textwrap.dedent("""\
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
        """))

    def _generate_is_inherited_property(self, *, to):
        to.write(f'constexpr bool isInheritedPropertyTable[numCSSProperties + {GenerationContext.number_of_predefined_properties}] = {{\n')
        to.write(f'    false, // CSSPropertyID::CSSPropertyInvalid\n')
        to.write(f'    true , // CSSPropertyID::CSSPropertyCustom\n')

        all_inherited_and_ids = (f'    {"true " if property.inherited else "false"}, // {property.id}' for property in self.properties.all)

        to.write(f'\n'.join(all_inherited_and_ids))
        to.write(f'\n}};\n\n')

        to.write(f"bool CSSProperty::isInheritedProperty(CSSPropertyID id)\n")
        to.write(f"{{\n")
        to.write(f"    ASSERT(id < firstCSSProperty + numCSSProperties);\n")
        to.write(f"    ASSERT(id != CSSPropertyID::CSSPropertyInvalid);\n")
        to.write(f"    return isInheritedPropertyTable[id];\n")
        to.write(f"}}\n\n")

    def _generate_are_in_same_logical_property_group_with_different_mappings_logic(self, *, to):
        to.write(f"bool CSSProperty::areInSameLogicalPropertyGroupWithDifferentMappingLogic(CSSPropertyID id1, CSSPropertyID id2)\n")
        to.write(f"{{\n")
        to.write(f"    switch (id1) {{\n")

        for group_name, property_group in sorted(self.properties.logical_property_groups.items(), key=lambda x: x[0]):
            logical = property_group["logical"]
            physical = property_group["physical"]
            for first in [logical, physical]:
                second = physical if first is logical else logical
                for resolver, property in sorted(first.items(), key=lambda x: x[1].name):
                    to.write(f"    case {property.id}:\n")

                to.write(f"        switch (id2) {{\n")
                for resolver, property in sorted(second.items(), key=lambda x: x[1].name):
                    to.write(f"        case {property.id}:\n")

                to.write(f"            return true;\n")
                to.write(f"        default:\n")
                to.write(f"            return false;\n")
                to.write(f"        }}\n")

        to.write(f"    default:\n")
        to.write(f"        return false;\n")
        to.write(f"    }}\n")
        to.write(f"}}\n\n")

    def _generate_css_property_settings_constructor(self, *, to):
        to.write(f"CSSPropertySettings::CSSPropertySettings(const Settings& settings)\n")
        to.write(f"    : ")

        settings_initializer_list = (f"{flag} {{ settings.{flag}() }}" for flag in self.properties.settings_flags)
        to.write(f"\n    , ".join(settings_initializer_list))

        to.write(f"\n{{\n")
        to.write(f"}}\n\n")

    def _generate_css_property_settings_operator_equal(self, *, to):
        to.write(f"bool operator==(const CSSPropertySettings& a, const CSSPropertySettings& b)\n")
        to.write(f"{{\n")

        to.write(f"    return ")
        settings_operator_equal_list = (f"a.{flag} == b.{flag}" for flag in self.properties.settings_flags)
        to.write(f"\n        && ".join(settings_operator_equal_list))

        to.write(f";\n")
        to.write(f"}}\n\n")

    def _generate_css_property_settings_hasher(self, *, to):
        to.write(f"void add(Hasher& hasher, const CSSPropertySettings& settings)\n")
        to.write(f"{{\n")
        to.write(f"    unsigned bits = ")

        settings_hasher_list = (f"settings.{flag} << {i}" for (i, flag) in enumerate(self.properties.settings_flags))
        to.write(f"\n        | ".join(settings_hasher_list))

        to.write(f";\n")
        to.write(f"    add(hasher, bits);\n")
        to.write(f"}}\n\n")

    def generate_css_property_names_gperf(self):
        with open('CSSPropertyNames.gperf', 'w') as output_file:
            self._generate_css_property_names_gperf_heading(
                to=output_file
            )

            self._generate_gperf_definition(
                to=output_file
            )

            self._generate_lookup_functions(
                to=output_file
            )

            self._generate_property_id_switch_function_bool(
                to=output_file,
                signature="bool isInternal(CSSPropertyID id)",
                properties=self.properties.all_internal_only
            )

            self._generate_is_exposed_functions(
                to=output_file
            )

            self._generate_is_inherited_property(
                to=output_file
            )

            self._generate_property_id_switch_function(
                to=output_file,
                signature="CSSPropertyID relatedProperty(CSSPropertyID id)",
                properties=(p for p in self.properties.all if p.codegen_properties.related_property),
                mapping=lambda p: f"return {p.codegen_properties.related_property.id};",
                default="return CSSPropertyID::CSSPropertyInvalid;"
            )

            self._generate_property_id_switch_function(
                to=output_file,
                signature="Vector<String> CSSProperty::aliasesForProperty(CSSPropertyID id)",
                properties=(p for p in self.properties.all if p.codegen_properties.aliases),
                mapping=lambda p: f"return {{ {', '.join(quote_iterable(p.codegen_properties.aliases, '_s'))} }};",
                default="return { };"
            )

            self._generate_property_id_switch_function_bool(
                to=output_file,
                signature="bool CSSProperty::isColorProperty(CSSPropertyID id)",
                properties=(p for p in self.properties.all if p.codegen_properties.color_property)
            )

            self._generate_property_id_switch_function(
                to=output_file,
                signature="UChar CSSProperty::listValuedPropertySeparator(CSSPropertyID id)",
                properties=(p for p in self.properties.all if p.codegen_properties.separator),
                mapping=lambda p: f"return '{ p.codegen_properties.separator[0] }';",
                default="break;",
                epilogue="return '\\0';"
            )

            self._generate_property_id_switch_function_bool(
                to=output_file,
                signature="bool CSSProperty::isDirectionAwareProperty(CSSPropertyID id)",
                properties=self.properties.all_direction_aware_properties
            )

            self._generate_property_id_switch_function_bool(
                to=output_file,
                signature="bool CSSProperty::isInLogicalPropertyGroup(CSSPropertyID id)",
                properties=self.properties.all_in_logical_property_group
            )

            self._generate_are_in_same_logical_property_group_with_different_mappings_logic(
                to=output_file
            )

            self._generate_physical_logical_conversion_function(
                to=output_file,
                signature="CSSPropertyID CSSProperty::resolveDirectionAwareProperty(CSSPropertyID id, TextDirection direction, WritingMode writingMode)",
                source="logical",
                destination="physical",
                resolver_enum_prefix="LogicalBox"
            )

            self._generate_physical_logical_conversion_function(
                to=output_file,
                signature="CSSPropertyID CSSProperty::unresolvePhysicalProperty(CSSPropertyID id, TextDirection direction, WritingMode writingMode)",
                source="physical",
                destination="logical",
                resolver_enum_prefix="Box"
            )

            self._generate_property_id_switch_function_bool(
                to=output_file,
                signature="bool CSSProperty::isDescriptorOnly(CSSPropertyID id)",
                properties=(p for p in self.properties.all if p.codegen_properties.descriptor_only)
            )

            self._generate_css_property_settings_constructor(
                to=output_file
            )

            self._generate_css_property_settings_operator_equal(
                to=output_file
            )

            self._generate_css_property_settings_hasher(
                to=output_file
            )

            self._generate_css_property_names_gperf_footing(
                to=output_file
            )

    # MARK: - Helper generator functions for CSSPropertyNames.h

    def _generate_css_property_names_h_heading(self, *, to):
        to.write(textwrap.dedent("""\
            // This file is automatically generated from CSSProperties.json by the process-css-properties script. Do not edit it.

            #pragma once

            #include <array>
            #include <wtf/HashFunctions.h>
            #include <wtf/HashTraits.h>

            namespace WebCore {

            class Settings;

            """))

    def _generate_css_property_names_h_property_constants(self, *, to):
        to.write(f"enum CSSPropertyID : uint16_t {{\n")
        to.write(f"    CSSPropertyInvalid = 0,\n")
        to.write(f"    CSSPropertyCustom = 1,\n")

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

        for property in self.properties.all:
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

            to.write(f"    {property.id_without_scope} = {count},\n")

            count += 1
            max_length = max(len(property.name), max_length)

        num = count - first

        to.write(f"}};\n\n")

        to.write(f"constexpr uint16_t firstCSSProperty = {first};\n")
        to.write(f"constexpr uint16_t numCSSProperties = {num};\n")
        to.write(f"constexpr unsigned maxCSSPropertyNameLength = {max_length};\n")
        to.write(f"constexpr auto firstTopPriorityProperty = {first_top_priority_property.id};\n")
        to.write(f"constexpr auto lastTopPriorityProperty = {last_top_priority_property.id};\n")
        to.write(f"constexpr auto firstHighPriorityProperty = {first_high_priority_property.id};\n")
        to.write(f"constexpr auto lastHighPriorityProperty = {last_high_priority_property.id};\n")
        to.write(f"constexpr auto firstLowPriorityProperty = {first_low_priority_property.id};\n")
        to.write(f"constexpr auto lastLowPriorityProperty = {last_low_priority_property.id};\n")
        to.write(f"constexpr auto firstDeferredProperty = {first_deferred_property.id};\n")
        to.write(f"constexpr auto lastDeferredProperty = {last_deferred_property.id};\n")
        to.write(f"constexpr auto firstShorthandProperty = {first_shorthand_property.id};\n")
        to.write(f"constexpr auto lastShorthandProperty = {last_shorthand_property.id};\n")
        to.write(f"constexpr uint16_t numCSSPropertyLonghands = firstShorthandProperty - firstCSSProperty;\n\n")

        to.write(f"extern const std::array<CSSPropertyID, {count_iterable(self.properties.all_computed)}> computedPropertyIDs;\n\n")

    def _generate_css_property_names_h_property_settings(self, *, to):
        to.write(f"struct CSSPropertySettings {{\n")
        to.write(f"    WTF_MAKE_STRUCT_FAST_ALLOCATED;\n")

        settings_variable_declarations = (f"    bool {flag} {{ false }};\n" for flag in self.properties.settings_flags)
        to.write("".join(settings_variable_declarations))

        to.write(f"    CSSPropertySettings() = default;\n")
        to.write(f"    explicit CSSPropertySettings(const Settings&);\n")
        to.write(f"}};\n\n")

        to.write(f"bool operator==(const CSSPropertySettings&, const CSSPropertySettings&);\n")
        to.write(f"inline bool operator!=(const CSSPropertySettings& a, const CSSPropertySettings& b) {{ return !(a == b); }}\n")
        to.write(f"void add(Hasher&, const CSSPropertySettings&);\n\n")

    def _generate_css_property_names_h_forward_declarations(self, *, to):
        to.write(textwrap.dedent("""\
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

            } // namespace WebCore
            """))

    def _generate_css_property_names_h_hash_traits(self, *, to):
        to.write(textwrap.dedent("""
            namespace WTF {

            template<> struct DefaultHash<WebCore::CSSPropertyID> : IntHash<unsigned> { };

            template<> struct HashTraits<WebCore::CSSPropertyID> : GenericHashTraits<WebCore::CSSPropertyID> {
                static const bool emptyValueIsZero = true;
                static void constructDeletedValue(WebCore::CSSPropertyID& slot) { slot = static_cast<WebCore::CSSPropertyID>(std::numeric_limits<uint16_t>::max()); }
                static bool isDeletedValue(WebCore::CSSPropertyID value) { return static_cast<uint16_t>(value) == std::numeric_limits<uint16_t>::max(); }
            };

            }
            """))

    def _generate_css_property_names_h_iterator_traits(self, *, to):
        to.write(textwrap.dedent("""
            namespace std {

            template<> struct iterator_traits<WebCore::AllCSSPropertiesRange::Iterator> { using value_type = WebCore::CSSPropertyID; };
            template<> struct iterator_traits<WebCore::AllLonghandCSSPropertiesRange::Iterator> { using value_type = WebCore::CSSPropertyID; };

            }
            """))

    def _generate_css_property_names_h_footing(self, *, to):
        to.write("\n")

    def generate_css_property_names_h(self):
        with open('CSSPropertyNames.h', 'w') as output_file:
            self._generate_css_property_names_h_heading(
                to=output_file
            )

            self._generate_css_property_names_h_property_constants(
                to=output_file
            )

            self._generate_css_property_names_h_property_settings(
                to=output_file
            )

            self._generate_css_property_names_h_forward_declarations(
                to=output_file
            )

            self._generate_css_property_names_h_hash_traits(
                to=output_file
            )

            self._generate_css_property_names_h_iterator_traits(
                to=output_file
            )

            self._generate_css_property_names_h_footing(
                to=output_file
            )

    # MARK: - Helper generator functions for CSSStyleDeclaration+PropertyNames.idl

    def _generate_css_style_declaration_property_names_idl_heading(self, *, to):
        to.write(textwrap.dedent("""\
            // This file is automatically generated from CSSProperties.json by the process-css-properties script. Do not edit it.

            typedef USVString CSSOMString;

            partial interface CSSStyleDeclaration {
            """))

    def _generate_css_style_declaration_property_names_idl_footing(self, *, to):
        to.write("};\n")

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
        to.write(textwrap.indent(comment, "    "))

        for name_or_alias, property in names_and_aliases_with_properties:
            if convert_to_idl_attribute:
                idl_attribute_name = GenerationContext._convert_css_property_to_idl_attribute(name_or_alias, lowercase_first=lowercase_first)
            else:
                idl_attribute_name = name_or_alias

            extended_attributes_values = [f"DelegateToSharedSyntheticAttribute=propertyValueFor{variant}IDLAttribute", "CallWith=PropertyName"]
            if property.codegen_properties.settings_flag:
                extended_attributes_values += [f"EnabledBySetting={property.codegen_properties.settings_flag}"]

            to.write(f"    [CEReactions, {', '.join(extended_attributes_values)}] attribute [LegacyNullToEmptyString] CSSOMString {idl_attribute_name};\n")

    def generate_css_style_declaration_property_names_idl(self):
        with open('CSSStyleDeclaration+PropertyNames.idl', 'w') as output_file:

            name_or_alias_to_property = {}
            for property in self.properties.all_non_internal_only:
                name_or_alias_to_property[property.name] = property
                for alias in property.aliases:
                    name_or_alias_to_property[alias] = property

            names_and_aliases_with_properties = sorted(list(name_or_alias_to_property.items()), key=lambda x: x[0])

            self._generate_css_style_declaration_property_names_idl_heading(
                to=output_file
            )

            self._generate_css_style_declaration_property_names_idl_section(
                to=output_file,
                comment=textwrap.dedent("""
                    // For each CSS property property that is a supported CSS property, the following
                    // partial interface applies where camel-cased attribute is obtained by running the
                    // CSS property to IDL attribute algorithm for property.
                    // Example: font-size -> element.style.fontSize
                    // Example: -webkit-transform -> element.style.WebkitTransform
                    // [CEReactions] attribute [LegacyNullToEmptyString] CSSOMString _camel_cased_attribute;
                    """),
                names_and_aliases_with_properties=names_and_aliases_with_properties,
                variant="CamelCased",
                convert_to_idl_attribute=True,
                lowercase_first=False
            )

            self._generate_css_style_declaration_property_names_idl_section(
                to=output_file,
                comment=textwrap.dedent("""
                    // For each CSS property property that is a supported CSS property and that begins
                    // with the string -webkit-, the following partial interface applies where webkit-cased
                    // attribute is obtained by running the CSS property to IDL attribute algorithm for
                    // property, with the lowercase first flag set.
                    // Example: -webkit-transform -> element.style.webkitTransform
                    // [CEReactions] attribute [LegacyNullToEmptyString] CSSOMString _webkit_cased_attribute;
                    """),
                names_and_aliases_with_properties=filter(lambda item: item[0].startswith("-webkit-"), names_and_aliases_with_properties),
                variant="WebKitCased",
                convert_to_idl_attribute=True,
                lowercase_first=True
            )

            self._generate_css_style_declaration_property_names_idl_section(
                to=output_file,
                comment=textwrap.dedent("""
                    // For each CSS property property that is a supported CSS property, except for
                    // properties that have no "-" (U+002D) in the property name, the following partial
                    // interface applies where dashed attribute is property.
                    // Example: font-size -> element.style['font-size']
                    // Example: -webkit-transform -> element.style.['-webkit-transform']
                    // [CEReactions] attribute [LegacyNullToEmptyString] CSSOMString _dashed_attribute;
                    """),
                names_and_aliases_with_properties=filter(lambda item: "-" in item[0], names_and_aliases_with_properties),
                variant="Dashed",
                convert_to_idl_attribute=False
            )

            self._generate_css_style_declaration_property_names_idl_section(
                to=output_file,
                comment=textwrap.dedent("""
                    // Non-standard. Special case properties starting with -epub- like is done for
                    // -webkit-, where attribute is obtained by running the CSS property to IDL attribute
                    // algorithm for property, with the lowercase first flag set.
                    // Example: -epub-caption-side -> element.style.epubCaptionSide
                    """),
                names_and_aliases_with_properties=filter(lambda item: item[0].startswith("-epub-"), names_and_aliases_with_properties),
                variant="EpubCased",
                convert_to_idl_attribute=True,
                lowercase_first=True
            )

            self._generate_css_style_declaration_property_names_idl_footing(
                to=output_file
            )

    # MARK: - Helper generator functions for StyleBuilderGenerated.cpp

    def _generate_style_builder_generated_cpp_heading(self, *, to):
        to.write(textwrap.dedent("""\
            // This file is automatically generated from CSSProperties.json by the process-css-properties script. Do not edit it.

            #include "config.h"
            #include "StyleBuilderGenerated.h"

            #include "CSSPrimitiveValueMappings.h"
            #include "CSSProperty.h"
            #include "RenderStyle.h"
            #include "StyleBuilderState.h"
            #include "StyleBuilderConverter.h"
            #include "StyleBuilderCustom.h"
            #include "StylePropertyShorthand.h"

            namespace WebCore {
            namespace Style {

            """))

    def _generate_style_builder_generated_cpp_footing(self, *, to):
        to.write(textwrap.dedent("""
            } // namespace Style
            } // namespace WebCore
            """))

    # Color property setters.

    def _generate_color_property_initial_value_setter(self, to, property):
        to.write(f"        if (builderState.applyPropertyToRegularStyle())\n")
        to.write(f"            builderState.style().{property.codegen_properties.setter}(RenderStyle::{property.codegen_properties.initial}());\n")
        to.write(f"        if (builderState.applyPropertyToVisitedLinkStyle())\n")
        to.write(f"            builderState.style().setVisitedLink{property.name_for_methods}(RenderStyle::{property.codegen_properties.initial}());\n")

    def _generate_color_property_inherit_value_setter(self, to, property):
        to.write(f"        if (builderState.applyPropertyToRegularStyle())\n")
        to.write(f"            builderState.style().{property.codegen_properties.setter}(builderState.parentStyle().{property.codegen_properties.getter}());\n")
        to.write(f"        if (builderState.applyPropertyToVisitedLinkStyle())\n")
        to.write(f"            builderState.style().setVisitedLink{property.name_for_methods}(builderState.parentStyle().{property.codegen_properties.getter}());\n")

    def _generate_color_property_value_setter(self, to, property, value):
        to.write(f"        if (builderState.applyPropertyToRegularStyle())\n")
        to.write(f"            builderState.style().{property.codegen_properties.setter}(builderState.colorFromPrimitiveValue({value}, ForVisitedLink::No));\n")
        to.write(f"        if (builderState.applyPropertyToVisitedLinkStyle())\n")
        to.write(f"            builderState.style().setVisitedLink{property.name_for_methods}(builderState.colorFromPrimitiveValue({value}, ForVisitedLink::Yes));\n")

    # Animation property setters.

    def _generate_animation_property_initial_value_setter(self, to, property):
        to.write(f"        auto& list = builderState.style().{property.method_name_for_ensure_animations_or_transitions}();\n")
        to.write(f"        if (list.isEmpty())\n")
        to.write(f"            list.append(Animation::create());\n")
        to.write(f"        list.animation(0).{property.codegen_properties.setter}(Animation::{property.codegen_properties.initial}());\n")
        to.write(f"        for (auto& animation : list)\n")
        to.write(f"            animation->clear{property.name_for_methods}();\n")

    def _generate_animation_property_inherit_value_setter(self, to, property):
        to.write(f"        auto& list = builderState.style().{property.method_name_for_ensure_animations_or_transitions}();\n")
        to.write(f"        auto* parentList = builderState.parentStyle().{property.method_name_for_animations_or_transitions}();\n")
        to.write(f"        size_t i = 0, parentSize = parentList ? parentList->size() : 0;\n")
        to.write(f"        for ( ; i < parentSize && parentList->animation(i).is{property.name_for_methods}Set(); ++i) {{\n")
        to.write(f"            if (list.size() <= i)\n")
        to.write(f"                list.append(Animation::create());\n")
        to.write(f"            list.animation(i).{property.codegen_properties.setter}(parentList->animation(i).{property.codegen_properties.getter}());\n")
        to.write(f"        }}\n\n")
        to.write(f"        // Reset any remaining animations to not have the property set.\n")
        to.write(f"        for ( ; i < list.size(); ++i)\n")
        to.write(f"            list.animation(i).clear{property.name_for_methods}();\n")

    def _generate_animation_property_value_setter(self, to, property):
        to.write(f"        auto& list = builderState.style().{property.method_name_for_ensure_animations_or_transitions}();\n")
        to.write(f"        size_t childIndex = 0;\n")
        to.write(f"        if (is<CSSValueList>(value)) {{\n")
        to.write(f"            // Walk each value and put it into an animation, creating new animations as needed.\n")
        to.write(f"            for (auto& currentValue : downcast<CSSValueList>(value)) {{\n")
        to.write(f"                if (childIndex <= list.size())\n")
        to.write(f"                    list.append(Animation::create());\n")
        to.write(f"                builderState.styleMap().mapAnimation{property.name_for_methods}(list.animation(childIndex), currentValue);\n")
        to.write(f"                ++childIndex;\n")
        to.write(f"            }}\n")
        to.write(f"        }} else {{\n")
        to.write(f"            if (list.isEmpty())\n")
        to.write(f"                list.append(Animation::create());\n")
        to.write(f"            builderState.styleMap().mapAnimation{property.name_for_methods}(list.animation(childIndex), value);\n")
        to.write(f"            childIndex = 1;\n")
        to.write(f"        }}\n")
        to.write(f"        for ( ; childIndex < list.size(); ++childIndex) {{\n")
        to.write(f"            // Reset all remaining animations to not have the property set.\n")
        to.write(f"            list.animation(childIndex).clear{property.name_for_methods}();\n")
        to.write(f"        }}\n")

    # Font property setters.

    def _generate_font_property_initial_value_setter(self, to, property):
        to.write(f"        auto fontDescription = builderState.fontDescription();\n")
        to.write(f"        fontDescription.{property.codegen_properties.setter}(FontCascadeDescription::{property.codegen_properties.initial}());\n")
        to.write(f"        builderState.setFontDescription(WTFMove(fontDescription));\n")

    def _generate_font_property_inherit_value_setter(self, to, property):
        to.write(f"        auto fontDescription = builderState.fontDescription();\n")
        to.write(f"        fontDescription.{property.codegen_properties.setter}(builderState.parentFontDescription().{property.codegen_properties.getter}());\n")
        to.write(f"        builderState.setFontDescription(WTFMove(fontDescription));\n")

    def _generate_font_property_value_setter(self, to, property, value):
        to.write(f"        auto fontDescription = builderState.fontDescription();\n")
        to.write(f"        fontDescription.{property.codegen_properties.setter}({value});\n")
        to.write(f"        builderState.setFontDescription(WTFMove(fontDescription));\n")

    # Fill Layer property setters.

    def _generate_fill_layer_property_initial_value_setter(self, to, property):
        initial = f"FillLayer::{property.codegen_properties.initial}({property.enum_name_for_layers_type})"
        to.write(f"        // Check for (single-layer) no-op before clearing anything.\n")
        to.write(f"        auto& layers = builderState.style().{property.method_name_for_layers}();\n")
        to.write(f"        if (!layers.next() && (!layers.is{property.name_for_methods}Set() || layers.{property.codegen_properties.getter}() == {initial}))\n")
        to.write(f"            return;\n\n")
        to.write(f"        auto* child = &builderState.style().{property.method_name_for_ensure_layers}();\n")
        to.write(f"        child->{property.codegen_properties.setter}({initial});\n")
        to.write(f"        for (child = child->next(); child; child = child->next())\n")
        to.write(f"            child->clear{property.name_for_methods}();\n")

    def _generate_fill_layer_property_inherit_value_setter(self, to, property):
        to.write(f"        // Check for no-op before copying anything.\n")
        to.write(f"        if (builderState.parentStyle().{property.method_name_for_layers}() == builderState.style().{property.method_name_for_layers}())\n")
        to.write(f"            return;\n\n")
        to.write(f"        auto* child = &builderState.style().{property.method_name_for_ensure_layers}();\n")
        to.write(f"        FillLayer* previousChild = nullptr;\n")
        to.write(f"        for (auto* parent = &builderState.parentStyle().{property.method_name_for_layers}(); parent && parent->is{property.name_for_methods}Set(); parent = parent->next()) {{\n")
        to.write(f"            if (!child) {{\n")
        to.write(f"                previousChild->setNext(FillLayer::create({property.enum_name_for_layers_type}));\n")
        to.write(f"                child = previousChild->next();\n")
        to.write(f"            }}\n")
        to.write(f"            child->{property.codegen_properties.setter}(parent->{property.codegen_properties.getter}());\n")
        to.write(f"            previousChild = child;\n")
        to.write(f"            child = previousChild->next();\n")
        to.write(f"        }}\n")
        to.write(f"        for (; child; child = child->next())\n")
        to.write(f"            child->clear{property.name_for_methods}();\n")

    def _generate_fill_layer_property_value_setter(self, to, property):
        to.write(f"        auto* child = &builderState.style().{property.method_name_for_ensure_layers}();\n")
        to.write(f"        FillLayer* previousChild = nullptr;\n")
        to.write(f"        if (is<CSSValueList>(value) && !is<CSSImageSetValue>(value)) {{\n")
        to.write(f"            // Walk each value and put it into a layer, creating new layers as needed.\n")
        to.write(f"            for (auto& item : downcast<CSSValueList>(value)) {{\n")
        to.write(f"                if (!child) {{\n")
        to.write(f"                    previousChild->setNext(FillLayer::create({property.enum_name_for_layers_type}));\n")
        to.write(f"                    child = previousChild->next();\n")
        to.write(f"                }}\n")
        to.write(f"                builderState.styleMap().mapFill{property.name_for_methods}(id, *child, item);\n")
        to.write(f"                previousChild = child;\n")
        to.write(f"                child = child->next();\n")
        to.write(f"            }}\n")
        to.write(f"        }} else {{\n")
        to.write(f"            builderState.styleMap().mapFill{property.name_for_methods}(id, *child, value);\n")
        to.write(f"            child = child->next();\n")
        to.write(f"        }}\n")
        to.write(f"        for (; child; child = child->next())\n")
        to.write(f"            child->clear{property.name_for_methods}();\n")

    # SVG property setters.

    def _generate_svg_property_initial_value_setter(self, to, property):
        to.write(f"        builderState.style().accessSVGStyle().{property.codegen_properties.setter}(SVGRenderStyle::{property.codegen_properties.initial}());\n")

    def _generate_svg_property_inherit_value_setter(self, to, property):
        to.write(f"        builderState.style().accessSVGStyle().{property.codegen_properties.setter}(forwardInheritedValue(builderState.parentStyle().svgStyle().{property.codegen_properties.getter}()));\n")

    def _generate_svg_property_value_setter(self, to, property, value):
        to.write(f"        builderState.style().accessSVGStyle().{property.codegen_properties.setter}({value});\n")

    # All other property setters.

    def _generate_property_initial_value_setter(self, to, property):
        to.write(f"        builderState.style().{property.codegen_properties.setter}(RenderStyle::{property.codegen_properties.initial}());\n")

    def _generate_property_inherit_value_setter(self, to, property):
        to.write(f"        builderState.style().{property.codegen_properties.setter}(forwardInheritedValue(builderState.parentStyle().{property.codegen_properties.getter}()));\n")

    def _generate_property_value_setter(self, to, property, value):
        to.write(f"        builderState.style().{property.codegen_properties.setter}({value});\n")

    # Property setter dispatch.

    def _generate_style_builder_generated_cpp_initial_value_setter(self, to, property):
        to.write(f"    static void applyInitial{property.id_without_prefix}(BuilderState& builderState)\n")
        to.write(f"    {{\n")

        if property.codegen_properties.auto_functions:
            to.write(f"        builderState.style().setHasAuto{property.name_for_methods}();\n")
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
            to.write(f"        builderState.style().setDisallowsFastPathInheritance();\n")

        to.write(f"    }}\n")

    def _generate_style_builder_generated_cpp_inherit_value_setter(self, to, property):
        to.write(f"    static void applyInherit{property.id_without_prefix}(BuilderState& builderState)\n")
        to.write(f"    {{\n")

        if property.codegen_properties.auto_functions:
            to.write(f"        if (builderState.parentStyle().hasAuto{property.name_for_methods}()) {{\n")
            to.write(f"            builderState.style().setHasAuto{property.name_for_methods}();\n")
            to.write(f"            return;\n")
            to.write(f"        }}\n")

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
            to.write(f"        builderState.style().setDisallowsFastPathInheritance();\n")

        to.write(f"    }}\n")

    def _generate_style_builder_generated_cpp_value_setter(self, to, property):
        if property.codegen_properties.fill_layer_property:
            to.write(f"    static void applyValue{property.id_without_prefix}(CSSPropertyID id, BuilderState& builderState, CSSValue& value)\n")
        else:
            to.write(f"    static void applyValue{property.id_without_prefix}(BuilderState& builderState, CSSValue& value)\n")
        to.write(f"    {{\n")

        def converted_value(property):
            if property.codegen_properties.converter:
                return f"BuilderConverter::convert{property.codegen_properties.converter}(builderState, value)"
            elif property.codegen_properties.conditional_converter:
                return f"WTFMove(convertedValue.value())"
            elif property.codegen_properties.color_property and not property.codegen_properties.visited_link_color_support:
                return f"builderState.colorFromPrimitiveValue(downcast<CSSPrimitiveValue>(value), ForVisitedLink::No)"
            else:
                return "downcast<CSSPrimitiveValue>(value)"

        if property in self.properties.properties_by_name["font"].codegen_properties.longhands and "Initial" not in property.codegen_properties.custom and not property.codegen_properties.converter:
            to.write(f"        if (is<CSSPrimitiveValue>(value) && CSSPropertyParserHelpers::isSystemFontShorthand(downcast<CSSPrimitiveValue>(value).valueID())) {{\n")
            to.write(f"            applyInitial{property.id_without_prefix}(builderState);\n")
            to.write(f"            return;\n")
            to.write(f"        }}\n")

        if property.codegen_properties.auto_functions:
            to.write(f"        if (downcast<CSSPrimitiveValue>(value).valueID() == CSSValueAuto) {{\n")
            to.write(f"            builderState.style().setHasAuto{property.name_for_methods}();\n")
            to.write(f"            return;\n")
            to.write(f"        }}\n")
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
                to.write(f"        auto convertedValue = BuilderConverter::convert{property.codegen_properties.conditional_converter}(builderState, value);\n")
                to.write(f"        if (convertedValue)\n    ")

            if property.codegen_properties.svg:
                self._generate_svg_property_value_setter(to, property, converted_value(property))
            else:
                self._generate_property_value_setter(to, property, converted_value(property))

        if property.codegen_properties.fast_path_inherited:
            to.write(f"        builderState.style().setDisallowsFastPathInheritance();\n")

        to.write(f"    }}\n")

    def _generate_style_builder_generated_cpp_builder_functions_class(self, *, to):
        to.write(f"class BuilderFunctions {{\n")
        to.write(f"public:\n")

        for property in self.properties.all:
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

        to.write(f"}};\n")

    def _generate_style_builder_generated_cpp_builder_generated_apply(self, *, to):
        to.write(textwrap.dedent("""
            void BuilderGenerated::applyProperty(CSSPropertyID id, BuilderState& builderState, CSSValue& value, bool isInitial, bool isInherit, const CSSRegisteredCustomProperty* registered)
            {
                switch (id) {
                case CSSPropertyID::CSSPropertyInvalid:
                    break;
                case CSSPropertyID::CSSPropertyCustom: {
                    auto& customProperty = downcast<CSSCustomPropertyValue>(value);
                    if (isInitial)
                        BuilderCustom::applyInitialCustomProperty(builderState, registered, customProperty.name());
                    else if (isInherit)
                        BuilderCustom::applyInheritCustomProperty(builderState, registered, customProperty.name());
                    else
                        BuilderCustom::applyValueCustomProperty(builderState, registered, customProperty);
                    break;
                }
            """))

        def scope_for_function(property, function):
            if function in property.codegen_properties.custom:
                return "BuilderCustom"
            return "BuilderFunctions"

        for property in self.properties.all:
            if property.codegen_properties.synonym:
                continue

            to.write(f"    case {property.id}:\n")

            for synonymous_property in property.synonymous_properties:
                to.write(f"    case {synonymous_property.id}:\n")

            if property.codegen_properties.longhands:
                to.write(f"        ASSERT(isShorthandCSSProperty(id));\n")
                to.write(f"        ASSERT_NOT_REACHED();\n")
            elif not property.codegen_properties.skip_builder:
                apply_initial_arguments = ["builderState"]
                apply_inherit_arguments = ["builderState"]
                apply_value_arguments = ["builderState", "value"]
                if property.codegen_properties.fill_layer_property:
                    apply_value_arguments.insert(0, "id")

                to.write(f"        if (isInitial)\n")
                to.write(f"            {scope_for_function(property, 'Initial')}::applyInitial{property.id_without_prefix}({', '.join(apply_initial_arguments)});\n")
                to.write(f"        else if (isInherit)\n")
                to.write(f"            {scope_for_function(property, 'Inherit')}::applyInherit{property.id_without_prefix}({', '.join(apply_inherit_arguments)});\n")
                to.write(f"        else\n")
                to.write(f"            {scope_for_function(property, 'Value')}::applyValue{property.id_without_prefix}({', '.join(apply_value_arguments)});\n")

            to.write(f"        break;\n")

        to.write(f"    }}\n")
        to.write(f"}}\n")

    def generate_style_builder_generated_cpp(self):
        with open('StyleBuilderGenerated.cpp', 'w') as output_file:
            self._generate_style_builder_generated_cpp_heading(
                to=output_file
            )

            self._generate_style_builder_generated_cpp_builder_functions_class(
                to=output_file
            )

            self._generate_style_builder_generated_cpp_builder_generated_apply(
                to=output_file
            )

            self._generate_style_builder_generated_cpp_footing(
                to=output_file
            )

    # MARK: - Helper generator functions for StylePropertyShorthandFunctions.h

    def _generate_style_property_shorthand_functions_h_heading(self, *, to):
        to.write(textwrap.dedent("""\
            // This file is automatically generated from CSSProperties.json by the process-css-properties script. Do not edit it.

            #pragma once

            namespace WebCore {

            class StylePropertyShorthand;

            """))

    def _generate_style_property_shorthand_functions_h_footing(self, *, to):
        to.write(textwrap.dedent("""
            } // namespace WebCore
            """))

    def _generate_style_property_shorthand_functions_declarations(self, *, to):
        # Skip non-shorthand properties (aka properties WITH longhands).
        for property in self.properties.all_shorthands:
            to.write(f"StylePropertyShorthand {property.id_without_prefix_with_lowercase_first_letter}Shorthand();\n")

    def generate_style_property_shorthand_functions_h(self):
        with open('StylePropertyShorthandFunctions.h', 'w') as output_file:
            self._generate_style_property_shorthand_functions_h_heading(
                to=output_file
            )
            self._generate_style_property_shorthand_functions_declarations(
                to=output_file
            )
            self._generate_style_property_shorthand_functions_h_footing(
                to=output_file
            )

    # MARK: - Helper generator functions for StylePropertyShorthandFunctions.cpp

    def _generate_style_property_shorthand_functions_cpp_heading(self, *, to):
        to.write(textwrap.dedent("""\
            // This file is automatically generated from CSSProperties.json by the process-css-properties script. Do not edit it."

            #include "config.h"
            #include "StylePropertyShorthandFunctions.h"

            #include "StylePropertyShorthand.h"

            namespace WebCore {

            """))

    def _generate_style_property_shorthand_functions_cpp_footing(self, *, to):
        to.write(textwrap.dedent("""
            } // namespace WebCore
            """))

    def _generate_style_property_shorthand_functions_accessors(self, *, to, longhand_to_shorthands, shorthand_to_longhand_count):
        for property in self.properties.all_shorthands:
            to.write(f"StylePropertyShorthand {property.id_without_prefix_with_lowercase_first_letter}Shorthand()\n")
            to.write(f"{{\n")
            to.write(f"    static const CSSPropertyID {property.id_without_prefix_with_lowercase_first_letter}Properties[] = {{\n")

            shorthand_to_longhand_count[property] = 0
            for longhand in property.codegen_properties.longhands:
                if longhand.name == "all":
                    for inner_property in self.properties.all_non_shorthands:
                        if inner_property.name == "direction" or inner_property.name == "unicode-bidi":
                            continue
                        longhand_to_shorthands.setdefault(inner_property, [])
                        longhand_to_shorthands[inner_property].append(property)
                        shorthand_to_longhand_count[property] += 1
                        to.write(f"        {inner_property.id},\n")
                else:
                    longhand_to_shorthands.setdefault(longhand, [])
                    longhand_to_shorthands[longhand].append(property)
                    shorthand_to_longhand_count[property] += 1
                    to.write(f"        {longhand.id},\n")


            to.write(f"    }};\n")
            to.write(f"    return StylePropertyShorthand({property.id}, {property.id_without_prefix_with_lowercase_first_letter}Properties);\n")
            to.write(f"}}\n\n")

    def _generate_style_property_shorthand_functions_matching_shorthands_for_longhand(self, *, to, longhand_to_shorthands, shorthand_to_longhand_count):
        to.write(f"StylePropertyShorthandVector matchingShorthandsForLonghand(CSSPropertyID id)\n")
        to.write(f"{{\n")
        to.write(f"    switch (id) {{\n")

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
                to.write(f"    case {longhand.id}:\n")
            to.write(f"        return {vector};\n")

        to.write(f"    default:\n")
        to.write(f"        return {{ }};\n")
        to.write(f"    }}\n")
        to.write(f"}}\n")

    def generate_style_property_shorthand_functions_cpp(self):
        with open('StylePropertyShorthandFunctions.cpp', 'w') as output_file:
            self._generate_style_property_shorthand_functions_cpp_heading(
                to=output_file
            )

            longhand_to_shorthands = {}
            shorthand_to_longhand_count = {}

            self._generate_style_property_shorthand_functions_accessors(
                to=output_file,
                longhand_to_shorthands=longhand_to_shorthands,
                shorthand_to_longhand_count=shorthand_to_longhand_count
            )

            self._generate_property_id_switch_function(
                to=output_file,
                signature="StylePropertyShorthand shorthandForProperty(CSSPropertyID id)",
                properties=self.properties.all_shorthands,
                mapping=lambda p: f"return {p.id_without_prefix_with_lowercase_first_letter}Shorthand();",
                default="return { };"
            )

            self._generate_style_property_shorthand_functions_matching_shorthands_for_longhand(
                to=output_file,
                longhand_to_shorthands=longhand_to_shorthands,
                shorthand_to_longhand_count=shorthand_to_longhand_count
            )

            self._generate_style_property_shorthand_functions_cpp_footing(
                to=output_file
            )

    # MARK: - Helper generator functions for CSSPropertyParsing.h

    def _generate_css_property_parsing_h_heading(self, *, to):
        to.write(textwrap.dedent("""\
            // This file is automatically generated from CSSProperties.json by the process-css-properties script. Do not edit it."

            #pragma once

            #include "CSSPropertyNames.h"
            #include "CSSValueKeywords.h"

            namespace WebCore {

            class CSSParserTokenRange;
            class CSSValue;
            struct CSSParserContext;
            """))

    def _generate_css_property_parsing_h_footing(self, *, to):
        to.write(textwrap.dedent("""
            } // namespace WebCore
            """))

    def _generate_css_property_parsing_h_property_parsing_declaration(self, *, to):
        to.write(textwrap.dedent("""
            struct CSSPropertyParsing {
                static RefPtr<CSSValue> parse(CSSParserTokenRange&, CSSPropertyID id, CSSPropertyID currentShorthand, const CSSParserContext&);

                // Returns true if the bare keyword value forms a valid construction when used with the
                // provided property.
                static bool isKeywordValidForProperty(CSSPropertyID, CSSValueID, const CSSParserContext&);

                // Returns true for properties that are valid to pass to `isKeywordValidForProperty`. This
                // corresponds to the set of properties where a bare keyword value is a valid construction.
                // NOTE: This will return true for properties that allow values that aren't keywords. All
                // that it validates is that the property can be valid with a keyword value. (For example,
                // 'list-style-type' supports a litany of keyword values, but also supports a string value.)
                static bool isKeywordProperty(CSSPropertyID);
            };
            """))

    def generate_css_property_parsing_h(self):
        with open('CSSPropertyParsing.h', 'w') as output_file:
            self._generate_css_property_parsing_h_heading(
                to=output_file
            )

            self._generate_css_property_parsing_h_property_parsing_declaration(
                to=output_file
            )

            self._generate_css_property_parsing_h_footing(
                to=output_file
            )

    # MARK: - Helper generator functions for CSSPropertyParsing.cpp

    def _generate_css_property_parsing_cpp_heading(self, *, to):
        to.write(textwrap.dedent("""\
            // This file is automatically generated from CSSProperties.json by the process-css-properties script. Do not edit it."

            #include "config.h"
            #include "CSSPropertyParsing.h"

            #include "CSSParserContext.h"
            #include "CSSParserIdioms.h"
            #include "CSSPropertyParser.h"
            #include "CSSPropertyParserWorkerSafe.h"
            #include "CSSValuePool.h"
            #include "DeprecatedGlobalSettings.h"

            namespace WebCore {

            using namespace CSSPropertyParserHelpers;
            using namespace CSSPropertyParserHelpersWorkerSafe;

            """))

    def _generate_css_property_parsing_cpp_footing(self, *, to):
        to.write(textwrap.dedent("""\
            } // namespace WebCore
            """))

    def _generate_css_property_parsing_cpp_is_keyword_valid_instance(self, *, to, property):
        # FIXME: Consider ordering case statements by the CSSValueID's numeric value.

        parameters = ["CSSValueID keyword"]
        if property.a_value_requires_a_settings_flag_or_is_internal:
            parameters.append("const CSSParserContext& context")

        to.write(f"\n")
        to.write(f"static bool isKeywordValidFor{property.id_without_prefix}({', '.join(parameters)})\n")
        to.write(f"{{\n")

        # Build up a list of pairs of (value, return-expression-to-use-for-value), taking
        # into account settings flags and mode checks for internal values. Leave the return
        # expression as an empty array for the default return expression "return true;".

        ValueReturnExpression = collections.namedtuple('ValueReturnExpression', ['value', 'return_expression'])
        value_and_return_expressions = []

        for value in property.values_sorted_by_name:
            return_expression = []
            if value.settings_flag:
                if value.settings_flag.startswith("DeprecatedGlobalSettings::"):
                    return_expression.append(value.settings_flag)
                else:
                    return_expression.append(f"context.{value.settings_flag}")
            if value.status == "internal":
                return_expression.append("isValueAllowedInMode(keyword, context.mode)")

            value_and_return_expressions.append(ValueReturnExpression(value, return_expression))

        # Take the list of pairs of (value, return-expression-to-use-for-value), and
        # group them by their 'return-expression' to avoid unnecessary duplication of
        # return statements.
        to.write(f"    switch (keyword) {{\n")
        for return_expression, group in itertools.groupby(sorted(value_and_return_expressions, key=lambda x: x.return_expression), lambda x: x.return_expression):
            for value, _ in group:
                to.write(f"    case {value.id}:\n")
            to.write(f"        return {' && '.join(return_expression or ['true'])};\n")

        to.write(f"    default:\n")
        to.write(f"        return false;\n")
        to.write(f"    }}\n")
        to.write(f"}}\n")

    def _generate_css_property_parsing_cpp_is_keyword_valid_aggregate(self, *, to):
        to.write(f"bool CSSPropertyParsing::isKeywordValidForProperty(CSSPropertyID id, CSSValueID keyword, const CSSParserContext& context)\n")
        to.write(f"{{\n")

        to.write(f"    switch (id) {{\n")

        for property in self.properties.all_accepting_a_single_value_keyword:
            to.write(f"    case {property.id}:\n")

            # Call implementation of `isKeywordValidFor...` generated in `_generate_css_property_parsing_cpp_is_keyword_valid_instance`.
            parameters = ["keyword"]
            if property.a_value_requires_a_settings_flag_or_is_internal:
                parameters.append("context")
            to.write(f"        return isKeywordValidFor{property.id_without_prefix}({', '.join(parameters)});\n")

        to.write(f"    default:\n")
        to.write(f"        return false;\n")
        to.write(f"    }}\n")
        to.write(f"}}\n\n")

    def _generate_css_property_parsing_cpp_property_parsing_functions(self, *, to):
        to.write(f"namespace {{\n")

        for property in self.properties.all_accepting_a_single_value_keyword:
            self._generate_css_property_parsing_cpp_is_keyword_valid_instance(
                to=to,
                property=property
            )

        to.write(f"\n}} // namespace (anonymous)\n\n")

    def _generate_css_property_parsing_cpp_parse(self, *, to):
        to.write(f"RefPtr<CSSValue> CSSPropertyParsing::parse(CSSParserTokenRange& range, CSSPropertyID id, CSSPropertyID currentShorthand, const CSSParserContext& context)\n")
        to.write(f"{{\n")
        to.write(f"    if (!isExposed(id, context.propertySettings) && !isInternal(id)) {{\n")
        to.write(f"        // Allow internal properties as we use them to parse several internal-only-shorthands (e.g. background-repeat),\n")
        to.write(f"        // and to handle certain DOM-exposed values (e.g. -webkit-font-size-delta from execCommand('FontSizeDelta')).\n")
        to.write(f"        ASSERT_NOT_REACHED();\n")
        to.write(f"        return nullptr;\n")
        to.write(f"    }}\n\n")

        # Build up a list of pairs of (property, return-expression-to-use-for-property).

        PropertyReturnExpression = collections.namedtuple('PropertyReturnExpression', ['property', 'return_expression'])
        property_and_return_expressions = []

        for property in self.properties.all:
            if property.codegen_properties.longhands:
                continue
            if property.codegen_properties.descriptor_only:
                continue
            if property.codegen_properties.skip_parser:
                continue

            if property.codegen_properties.custom_parser or property.codegen_properties.parser_function:
                parameters = []
                if property.codegen_properties.parser_requires_current_property:
                    parameters.append("id")
                parameters.append("range")
                if property.codegen_properties.parser_requires_current_shorthand:
                    parameters.append("currentShorthand")
                if property.codegen_properties.parser_requires_context:
                    parameters.append("context")
                if property.codegen_properties.parser_requires_context_mode:
                    parameters.append("context.mode")
                if property.codegen_properties.parser_requires_quirks_mode:
                    parameters.append("context.mode == HTMLQuirksMode")
                if property.codegen_properties.parser_requires_value_pool:
                    parameters.append("CSSValuePool::singleton()")
                parameters += property.codegen_properties.parser_requires_additional_parameters

                # If a "parser-function" has been specified, use that, otherwise assume the 'consume' function uses the property name.
                function = property.codegen_properties.parser_function or f"consume{property.id_without_prefix}"

                # Merge the scope, function and parameters to form the final invocation.
                return_expression = f"{function}({', '.join(parameters)})"
            elif property.accepts_a_single_value_keyword:
                parameters = ["range", f"isKeywordValidFor{property.id_without_prefix}"]
                if property.a_value_requires_a_settings_flag_or_is_internal:
                    parameters.append("context")

                return_expression = f"consumeIdent({', '.join(parameters)})"
            else:
                raise Exception(f"Invalid property definition for '{property.id}'. Style properties must either specify values or a custom parser.")

            property_and_return_expressions.append(PropertyReturnExpression(property, return_expression))

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
            return Properties._sort_by_descending_priority_and_name(a.properties[0], b.properties[0])

        to.write(f"    switch (id) {{\n")
        for properties, return_expression in sorted(property_and_return_expressions_grouped_by_expression, key=functools.cmp_to_key(_sort_by_first_property)):
            for property in properties:
                to.write(f"    case {property.id}:\n")
            to.write(f"        return {return_expression};\n")

        to.write(f"    default:\n")
        to.write(f"        return nullptr;\n")
        to.write(f"    }}\n")
        to.write(f"}}\n\n")

    def generate_css_property_parsing_cpp(self):
        with open('CSSPropertyParsing.cpp', 'w') as output_file:
            self._generate_css_property_parsing_cpp_heading(
                to=output_file
            )

            self._generate_css_property_parsing_cpp_property_parsing_functions(
                to=output_file
            )

            self._generate_css_property_parsing_cpp_parse(
                to=output_file
            )

            self._generate_css_property_parsing_cpp_is_keyword_valid_aggregate(
                to=output_file
            )

            self._generate_property_id_switch_function_bool(
                to=output_file,
                signature="bool CSSPropertyParsing::isKeywordProperty(CSSPropertyID id)",
                properties=self.properties.all_accepting_a_single_value_keyword,
            )

            self._generate_css_property_parsing_cpp_footing(
                to=output_file
            )

def main():
    parser = argparse.ArgumentParser(description='Process CSS property definitions.')
    parser.add_argument('--properties', default="CSSProperties.json")
    parser.add_argument('--defines')
    parser.add_argument('--gperf-executable')
    parser.add_argument('-v', '--verbose', action='store_true')
    args = parser.parse_args()

    with open(args.properties, "r") as properties_file:
        properties_json = json.load(properties_file)

    parsing_context = ParsingContext(defines_string=args.defines, parsing_for_codegen=True, verbose=args.verbose)
    properties = Properties.from_json(parsing_context, properties_json)

    if args.verbose:
        print(f"{len(properties.properties)} properties active for code generation")

    generation_context = GenerationContext(properties, verbose=args.verbose, gperf_executable=args.gperf_executable)

    generation_context.generate_css_property_parsing_h()
    generation_context.generate_css_property_parsing_cpp()

    generation_context.generate_css_property_names_h()
    generation_context.generate_css_property_names_gperf()
    generation_context.run_gperf()

    generation_context.generate_css_style_declaration_property_names_idl()

    generation_context.generate_style_builder_generated_cpp()
    generation_context.generate_style_property_shorthand_functions_h()
    generation_context.generate_style_property_shorthand_functions_cpp()


if __name__ == "__main__":
    main()
