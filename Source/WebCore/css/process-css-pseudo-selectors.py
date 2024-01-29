#!/usr/bin/env python3
#
# Copyright (C) 2024 Apple Inc. All rights reserved.
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
import itertools
import json
import os
import subprocess
import sys
import textwrap

# - MARK: Input file validation.

COMMON_KNOWN_KEY_TYPES = {
    'aliases': [list],
    'argument': [str],
    'comment': [str],
    'conditional': [str],
    'settings-flag': [str],
    'status': [str],
}

KNOWN_KEY_TYPES = {
    'pseudo-classes': COMMON_KNOWN_KEY_TYPES,
    'pseudo-elements': dict(COMMON_KNOWN_KEY_TYPES, **{
        'supports-single-colon-for-compatibility': [bool],
        'user-agent-part': [bool],
        'user-agent-part-string': [bool],
    })
}

# These count the number of -webkit- prefixes (excluding aliases).
# Do not increase these numbers and instead consider using:
# - a pseudo on standards track for things that are meant to be styled by arbitrary web content
# - an `-internal-` prefix for things only meant to be styled in the user agent stylesheet
# - an `-apple-` prefix only exposed to certain Apple clients (there is a settings-flag requirement for using this prefix)
WEBKIT_PREFIX_COUNTS_DO_NOT_INCREASE = {
    'pseudo-classes': 5,
    'pseudo-elements': 59,
}

class InputValidator:
    def __init__(self, input_data):
        self.check_field_documentation(input_data)
        self.validate_fields(input_data, 'pseudo-classes')
        self.validate_fields(input_data, 'pseudo-elements')
        self.check_webkit_prefix_count(input_data, 'pseudo-classes')
        self.check_webkit_prefix_count(input_data, 'pseudo-elements')

    def check_field_documentation(self, input_data):
        documentation = input_data['documentation']['fields']
        for pseudo_type in KNOWN_KEY_TYPES:
            for field in KNOWN_KEY_TYPES[pseudo_type]:
                if field not in documentation:
                    raise Exception(f'Missing documentation for field "{field}" found in "{pseudo_type}".')

    def validate_fields(self, input_data, pseudo_type):
        def is_known_prefix(name):
            if not name.startswith('-'):
                return True

            return name.startswith('-apple-') or name.startswith('-internal-') or name.startswith('-webkit-')

        for pseudo_name, pseudo_data in input_data[pseudo_type].items():
            # `-apple-` prefixed pseudos should be behind a flag that is not exposed to web content.
            if pseudo_name.startswith('-apple-') and 'settings-flag' not in pseudo_data:
                raise Exception(f'"{pseudo_name}" should have a "settings-flag" that is not exposed to web content.')

            # Check for unknown prefixes.
            if not is_known_prefix(pseudo_name):
                raise Exception(f'"{pseudo_name}" starts with an unknown prefix.')

            for key, value in pseudo_data.items():
                # Check for unknown fields.
                if key not in KNOWN_KEY_TYPES[pseudo_type]:
                    raise Exception(f'Unknown field "{key}" for {pseudo_type} found in "{pseudo_name}".')

                # Check if fields match expected types.
                expected_types = KNOWN_KEY_TYPES[pseudo_type][key]
                if type(value) not in expected_types:
                    raise Exception(f'Invalid value type {type(value)} for "{key}" in "{pseudo_name}". Expected type in set "{expected_types}".')

                if key == 'aliases':
                    # Check for unknown prefixes.
                    for alias in value:
                        if not is_known_prefix(alias):
                            raise Exception(f'"{alias}" starts with an unknown prefix.')

                    # Aliases are not currently supported for "supports-single-colon-for-compatibility": true.
                    if key_is_true(pseudo_data, 'supports-single-colon-for-compatibility'):
                        raise Exception('Setting "aliases" along with "supports-single-colon-for-compatibility": true is not supported.')

                # Validate "argument" field.
                if key == 'argument':
                    if value not in ['required', 'optional']:
                        raise Exception(f'Invalid "argument" field in "{pseudo_name}". The field should either be absent, "required" or "optional".')

                    if key_is_true(pseudo_data, 'user-agent-part'):
                        raise Exception('Setting "argument" along with "user-agent-part": true is not supported.')

                # Validate "supports-single-colon-for-compatiblity" field.
                if key == 'supports-single-colon-for-compatibility':
                    allowlist = ['after', 'before', 'first-letter', 'first-line']
                    if pseudo_name not in allowlist:
                        raise Exception(f'Unexpected "supports-single-colon-for-compatibility" field in "{pseudo_name}". This should never be set on new pseudo-elements.')

                # Validate "user-agent-part-string" field.
                if key == 'user-agent-part-string' and key_is_true(pseudo_data, 'user-agent-part'):
                    raise Exception('Setting "user-agent-part-string" along with "user-agent-part": true is not supported.')

    def check_webkit_prefix_count(self, input_data, pseudo_type):
        actual_count = len(list(filter(lambda name: name.startswith('-webkit-'), input_data[pseudo_type].keys())))
        expected_count = WEBKIT_PREFIX_COUNTS_DO_NOT_INCREASE[pseudo_type]

        too_many_webkit_prefixes_error_string = f"""Instead of adding a `-webkit-` prefix, please consider these alternatives:
- an `-internal-` prefix for things only meant to be styled in the user agent stylesheet
- an `-apple-` prefix only exposed to certain Apple clients (there is a settings-flag requirement for using this prefix)
- {pseudo_type} on standards track for things that are meant to be styled by arbitrary web content
"""

        count_decreased_error_string = f'The number of `-webkit-` prefixed {pseudo_type} has decreased to {actual_count} (yay!), please update the count in `process-css-pseudo-selectors.py`.'

        if actual_count > expected_count:
            raise Exception(too_many_webkit_prefixes_error_string)
        elif actual_count < expected_count:
            raise Exception(count_decreased_error_string)


# - MARK: Helpers.

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


def write_copyright_header(writer):
    writer.write_block("""/*
 * Copyright (C) 2014-2024 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */""")


def write_autogeneration_comment(writer):
    writer.newline()
    writer.write('// This file is automatically generated from CSSPseudoSelectors.json by the process-css-pseudo-selectors.py script, do not edit by hand.')


def write_pragma_once(writer):
    writer.newline()
    writer.write('#pragma once')


def open_namespace_webcore(writer):
    writer.newline()
    writer.write('namespace WebCore {')


def close_namespace_webcore(writer):
    writer.newline()
    writer.write('} // namespace WebCore')


def key_is_true(object, key):
    return object.get(key, False)


def expand_conditional(conditional):
    return conditional.replace('(', '_').replace(')', '')


def is_pseudo_selector_enabled(selector, webcore_defines):
    if 'conditional' not in selector:
        return True
    return expand_conditional(selector['conditional']) in webcore_defines


# - MARK: Formatters.

def format_name_for_enum_class(stringPseudoType):
    def format(substring):
        if substring == 'webkit':
            return 'WebKit'
        if substring == 'html':
            return 'HTML'
        return substring.capitalize()

    output = list(map(format, stringPseudoType.split('-')))
    return ''.join(output)


def format_name_for_function(userAgentPart):
    substrings = [substring for substring in userAgentPart.split('-') if substring != '']
    output = substrings[0]
    for substring in substrings[1:]:
        output += substring.capitalize()
    return output


# - MARK: Code generators.

GeneratorTypes = {
    'PSEUDO_CLASS_AND_COMPATIBILITY': 'pseudo_class_and_compatibility',
    'PSEUDO_ELEMENT': 'pseudo_element',
}


class GPerfMappingGenerator:
    def __init__(self, generator_type, data, webcore_defines):
        self.webcore_defines = webcore_defines
        self.data = data

        if generator_type == GeneratorTypes['PSEUDO_CLASS_AND_COMPATIBILITY']:
            self.initializer_suffix = '{std::nullopt,std::nullopt}'
        else:
            self.initializer_suffix = 'std::nullopt'

        self.map = {}

    def generate_pseudo_class_and_compatibility_element_map(self):
        pseudo_classes = self.data['pseudo-classes']
        pseudo_elements = self.data['pseudo-elements']

        # Process pseudo-classes.
        for pseudo_class_name, pseudo_class in pseudo_classes.items():
            if not is_pseudo_selector_enabled(pseudo_class, self.webcore_defines):
                continue

            self.map[pseudo_class_name] = (format_name_for_enum_class(pseudo_class_name), 'std::nullopt')

            for alias in pseudo_class.get('aliases', []):
                self.map[alias] = (format_name_for_enum_class(pseudo_class_name), 'std::nullopt')

        # Process compatibility pseudo-elements.
        for pseudo_element_name, pseudo_element in pseudo_elements.items():
            if not is_pseudo_selector_enabled(pseudo_element, self.webcore_defines):
                continue
            if not key_is_true(pseudo_element, 'supports-single-colon-for-compatibility'):
                continue

            self.map[pseudo_element_name] = ('std::nullopt', format_name_for_enum_class(pseudo_element_name))

        return self.map

    def generate_pseudo_element_map(self):
        pseudo_elements = self.data['pseudo-elements']

        for pseudo_element_name, pseudo_element in pseudo_elements.items():
            if not is_pseudo_selector_enabled(pseudo_element, self.webcore_defines):
                continue

            if key_is_true(pseudo_element, 'user-agent-part'):
                self.map[pseudo_element_name] = 'UserAgentPart'
            else:
                self.map[pseudo_element_name] = format_name_for_enum_class(pseudo_element_name)

            for alias in pseudo_element.get('aliases', []):
                if key_is_true(pseudo_element, 'user-agent-part'):
                    self.map[alias] = 'UserAgentPartLegacyAlias'
                else:
                    self.map[alias] = format_name_for_enum_class(pseudo_element_name)

        return self.map


class GPerfOutputGenerator:
    def __init__(self, generator_type, output_file_name, mapping):
        self.mapping = mapping

        with open(output_file_name, 'w') as output_file:
            writer = Writer(output_file)
            writer.write('%{')
            write_copyright_header(writer)
            write_autogeneration_comment(writer)
            self.write_includes(writer, generator_type)
            self.write_ignore_implicit_fallthrough(writer)
            self.write_define_register(writer)
            open_namespace_webcore(writer)
            self.write_entry_struct_definition(writer, generator_type)
            writer.newline()
            writer.write('%}')

            self.write_gperf_mapping(writer, generator_type)

            if generator_type == GeneratorTypes['PSEUDO_CLASS_AND_COMPATIBILITY']:
                self.write_parsing_function_definitions_for_pseudo_class(writer)
            else:
                self.write_parsing_function_definitions_for_pseudo_element(writer)

            close_namespace_webcore(writer)
            self.write_end_ignore_warning(writer)

    def write_includes(self, writer, generator_type):
        writer.newline()
        writer.write('#include "config.h"')
        writer.write('#include "SelectorPseudoTypeMap.h"')
        if generator_type == GeneratorTypes['PSEUDO_CLASS_AND_COMPATIBILITY']:
            writer.newline()
            writer.write('#include "MutableCSSSelector.h"')

    def write_ignore_implicit_fallthrough(self, writer):
        writer.newline()
        writer.write('IGNORE_WARNINGS_BEGIN("implicit-fallthrough")')

    def write_define_register(self, writer):
        writer.newline()
        writer.write('// Older versions of gperf use the `register` keyword.')
        writer.write('#define register')

    def write_entry_struct_definition(self, writer, generator_type):
        if generator_type == GeneratorTypes['PSEUDO_CLASS_AND_COMPATIBILITY']:
            writer.write_block("""
                struct SelectorPseudoClassOrCompatibilityPseudoElementEntry {
                    const char* name;
                    PseudoClassOrCompatibilityPseudoElement pseudoTypes;
                };""")
        else:
            writer.write_block("""
                struct SelectorPseudoTypeEntry {
                    const char* name;
                    std::optional<CSSSelector::PseudoElement> type;
                };""")

    def write_gperf_mapping(self, writer, generator_type):
        writer.write_block(f"""
            %struct-type
            %define initializer-suffix ,{'{std::nullopt,std::nullopt}' if generator_type == GeneratorTypes['PSEUDO_CLASS_AND_COMPATIBILITY'] else 'std::nullopt'}
            %define class-name {'SelectorPseudoClassAndCompatibilityElementMapHash' if generator_type == GeneratorTypes['PSEUDO_CLASS_AND_COMPATIBILITY'] else 'SelectorPseudoElementMapHash'}
            %omit-struct-type
            %language=C++
            %readonly-tables
            %global-table
            %ignore-case
            %compare-strncmp
            %enum

            struct {'SelectorPseudoClassOrCompatibilityPseudoElementEntry' if generator_type == GeneratorTypes['PSEUDO_CLASS_AND_COMPATIBILITY'] else 'SelectorPseudoTypeEntry'};
            """)

        writer.write('%%')

        def prefix_value(prefix, value):
            if value == 'std::nullopt':
                return value
            return f'CSSSelector::{prefix}::{value}'

        for key in self.mapping:
            value = self.mapping[key]
            if generator_type == GeneratorTypes['PSEUDO_CLASS_AND_COMPATIBILITY']:
                writer.write(f'"{key}", {{{prefix_value("PseudoClass", value[0])}, {prefix_value("PseudoElement", value[1])}}}')
            else:
                writer.write(f'"{key}", {prefix_value("PseudoElement", value)}')

        writer.write('%%')

    def write_parsing_function_definitions_for_pseudo_class(self, writer):
        longest_keyword_length = len(max(self.mapping, key=len))
        writer.write_block("""
        static inline const SelectorPseudoClassOrCompatibilityPseudoElementEntry* findPseudoClassAndCompatibilityElementName(const LChar* characters, unsigned length)
        {
            return SelectorPseudoClassAndCompatibilityElementMapHash::in_word_set(reinterpret_cast<const char*>(characters), length);
        }""")

        writer.write_block(f"""
        static inline const SelectorPseudoClassOrCompatibilityPseudoElementEntry* findPseudoClassAndCompatibilityElementName(const UChar* characters, unsigned length)
        {{
            constexpr unsigned maxKeywordLength = {longest_keyword_length};
            LChar buffer[maxKeywordLength];
            if (length > maxKeywordLength)
                return nullptr;

            for (unsigned i = 0; i < length; ++i) {{
                UChar character = characters[i];
                if (!isLatin1(character))
                    return nullptr;

                buffer[i] = static_cast<LChar>(character);
            }}
            return findPseudoClassAndCompatibilityElementName(buffer, length);
        }}""")

        writer.write_block("""
        PseudoClassOrCompatibilityPseudoElement findPseudoClassAndCompatibilityElementName(StringView name)
        {
            const SelectorPseudoClassOrCompatibilityPseudoElementEntry* entry;
            if (name.is8Bit())
                entry = findPseudoClassAndCompatibilityElementName(name.characters8(), name.length());
            else
                entry = findPseudoClassAndCompatibilityElementName(name.characters16(), name.length());

            if (entry)
                return entry->pseudoTypes;
            return { std::nullopt, std::nullopt };
        }""")

    def write_parsing_function_definitions_for_pseudo_element(self, writer):
        longest_keyword_length = len(max(self.mapping, key=len))
        writer.write_block("""
            static inline std::optional<CSSSelector::PseudoElement> findPseudoElementName(const LChar* characters, unsigned length)
            {
                if (const SelectorPseudoTypeEntry* entry = SelectorPseudoElementMapHash::in_word_set(reinterpret_cast<const char*>(characters), length))
                    return entry->type;
                return std::nullopt;
            }""")

        writer.write_block(f"""
            static inline std::optional<CSSSelector::PseudoElement> findPseudoElementName(const UChar* characters, unsigned length)
            {{
                constexpr unsigned maxKeywordLength = {longest_keyword_length};
                LChar buffer[maxKeywordLength];
                if (length > maxKeywordLength)
                    return std::nullopt;

                for (unsigned i = 0; i < length; ++i) {{
                    UChar character = characters[i];
                    if (!isLatin1(character))
                        return std::nullopt;

                    buffer[i] = static_cast<LChar>(character);
                }}
                return findPseudoElementName(buffer, length);
            }}""")

        writer.write_block("""
            std::optional<CSSSelector::PseudoElement> findPseudoElementName(StringView name)
            {
                if (name.is8Bit())
                    return findPseudoElementName(name.characters8(), name.length());
                return findPseudoElementName(name.characters16(), name.length());
            }""")

    def write_end_ignore_warning(self, writer):
        writer.newline()
        writer.write('IGNORE_WARNINGS_END')


class GPerfGenerator:
    def __init__(self, data, gperf_executable, webcore_defines):
        self.webcore_defines = webcore_defines
        self.data = data

        self.generate_pseudo_class_and_compatibility_gperf()
        self.generate_pseudo_element_gperf()
        self.generate_cpp_from_gperf(gperf_executable)

    def generate_pseudo_class_and_compatibility_gperf(self):
        mapping = GPerfMappingGenerator(
            GeneratorTypes['PSEUDO_CLASS_AND_COMPATIBILITY'],
            self.data,
            self.webcore_defines,
        ).generate_pseudo_class_and_compatibility_element_map()
        GPerfOutputGenerator(
            GeneratorTypes['PSEUDO_CLASS_AND_COMPATIBILITY'],
            'SelectorPseudoClassAndCompatibilityElementMap.gperf',
            mapping,
        )

    def generate_pseudo_element_gperf(self):
        mapping = GPerfMappingGenerator(
            GeneratorTypes['PSEUDO_ELEMENT'],
            self.data,
            self.webcore_defines,
        ).generate_pseudo_element_map()
        GPerfOutputGenerator(
            GeneratorTypes['PSEUDO_ELEMENT'],
            'SelectorPseudoElementMap.gperf',
            mapping,
        )

    def generate_cpp_from_gperf(self, gperf_executable):
        if 'GPERF' in os.environ:
            gperf_executable = os.environ['GPERF']

        if subprocess.call([gperf_executable, '--key-positions=*', '-m', '10', '-s', '2', 'SelectorPseudoClassAndCompatibilityElementMap.gperf', '--output-file=SelectorPseudoClassAndCompatibilityElementMap.cpp']) != 0:
            raise Exception("Error when generating SelectorPseudoClassAndCompatibilityElementMap.cpp from SelectorPseudoClassAndCompatibilityElementMap.gperf.")

        if subprocess.call([gperf_executable, '--key-positions=*', '-m', '10', '-s', '2', 'SelectorPseudoElementMap.gperf', '--output-file=SelectorPseudoElementMap.cpp']) != 0:
            raise Exception("Error when generating SelectorPseudoElementMap.cpp from SelectorPseudoElementMap.gperf.")


class CSSSelectorEnumGenerator:
    def __init__(self, data):
        self.data = data

        pseudo_classes = self.data['pseudo-classes']
        pseudo_elements = self.data['pseudo-elements']
        with open("CSSSelectorEnums.h", 'w') as output_file:
            writer = Writer(output_file)
            write_copyright_header(writer)
            write_autogeneration_comment(writer)
            write_pragma_once(writer)
            open_namespace_webcore(writer)
            self.write_enum_class(writer, "PseudoClass", self.pseudo_enum_values(pseudo_classes))
            self.write_enum_class(writer, "PseudoElement", self.pseudo_enum_values(pseudo_elements) + [('UserAgentPart', None), ('UserAgentPartLegacyAlias', None), ('WebKitUnknown', None)])
            close_namespace_webcore(writer)

    def pseudo_enum_values(self, values):
        enum_values = []
        for pseudo_name, pseudo_data in values.items():
            if key_is_true(pseudo_data, 'user-agent-part'):
                continue
            enum_values.append((format_name_for_enum_class(pseudo_name), pseudo_data.get('conditional', None)))
        return enum_values

    def write_enum_class(self, writer, type_name, values):
        writer.newline()
        writer.write(f'enum class CSSSelector{type_name} : uint8_t {{')
        for enum_value, conditional in values:
            if conditional:
                writer.write(f'#if {conditional}')
            with writer.indent():
                writer.write(f'{enum_value},')
            if conditional:
                writer.write('#endif')
        writer.write('};')


class CSSSelectorInlinesGenerator:
    def __init__(self, data):
        self.data = data

        pseudo_classes = self.data['pseudo-classes']
        pseudo_elements = self.data['pseudo-elements']
        with open("CSSSelectorInlines.h", 'w') as output_file:
            writer = Writer(output_file)
            write_copyright_header(writer)
            write_autogeneration_comment(writer)
            write_pragma_once(writer)
            self.write_includes(writer)
            open_namespace_webcore(writer)
            self.write_is_pseudo_class_enabled(writer, pseudo_classes)
            self.write_is_pseudo_element_enabled(writer, pseudo_elements)
            self.write_selector_text_for_pseudo_class(writer, pseudo_classes)
            self.write_name_for_user_agent_part_legacy_alias(writer, pseudo_elements)
            self.write_pseudo_argument_check_function(writer, 'pseudoClassRequiresArgument', 'PseudoClass', ['required'], pseudo_classes)
            self.write_pseudo_argument_check_function(writer, 'pseudoElementRequiresArgument', 'PseudoElement', ['required'], pseudo_elements)
            self.write_pseudo_argument_check_function(writer, 'pseudoClassMayHaveArgument', 'PseudoClass', ['required', 'optional'], pseudo_classes)
            self.write_pseudo_argument_check_function(writer, 'pseudoElementMayHaveArgument', 'PseudoElement', ['required', 'optional'], pseudo_elements)
            close_namespace_webcore(writer)

    def write_includes(self, writer):
        writer.newline()
        writer.write('#include "CSSSelector.h"')
        writer.write('#include "CSSSelectorParserContext.h"')
        writer.write('#include "DeprecatedGlobalSettings.h"')
        writer.write('#include <wtf/text/StringView.h>')

    def format_enablement_condition(self, settings_flag, is_internal):
        conditions = []
        if settings_flag is not None:
            if settings_flag.startswith('DeprecatedGlobalSettings::'):
                conditions.append(f'{settings_flag}()')
            else:
                conditions.append(f'context.{settings_flag}')

        if is_internal:
            conditions.append('isUASheetBehavior(context.mode)')

        return ' && '.join(conditions)

    def write_is_pseudo_class_enabled(self, writer, values):
        writer.write_block("""
            inline bool CSSSelector::isPseudoClassEnabled(PseudoClass type, const CSSSelectorParserContext& context)
            {
                switch (type) {""")

        for pseudo_name, pseudo_data in values.items():
            is_internal_pseudo = pseudo_name.startswith('-internal-')
            if 'settings-flag' not in pseudo_data and not is_internal_pseudo:
                continue

            if 'conditional' in pseudo_data:
                writer.write(f'#if {pseudo_data["conditional"]}')

            settings_flag = pseudo_data.get('settings-flag', None)
            with writer.indent():
                writer.write(f'case PseudoClass::{format_name_for_enum_class(pseudo_name)}:')
                enablement_condition = self.format_enablement_condition(settings_flag, is_internal_pseudo)
                with writer.indent():
                    writer.write(f'return {enablement_condition};')

            if 'conditional' in pseudo_data:
                writer.write('#endif')

        with writer.indent():
            writer.write('default:')
            with writer.indent():
                writer.write('return true;')

            # End switch statement.
            writer.write('}')
        # End function.
        writer.write('}')

    def write_is_pseudo_element_enabled(self, writer, values):
        writer.write_block("""
            inline bool CSSSelector::isPseudoElementEnabled(PseudoElement type, StringView name, const CSSSelectorParserContext& context)
            {
                switch (type) {""")

        user_agent_part_map = {}
        user_agent_part_alias_map = {}

        # Collect user agent parts and generate code for other pseudo-elements.
        for pseudo_name, pseudo_data in values.items():
            is_internal_pseudo = pseudo_name.startswith('-internal-')
            if 'settings-flag' not in pseudo_data and not is_internal_pseudo:
                continue

            settings_flag = pseudo_data.get('settings-flag', None)
            enablement_condition = self.format_enablement_condition(settings_flag, is_internal_pseudo)

            if key_is_true(pseudo_data, 'user-agent-part'):
                user_agent_part_map[pseudo_name] = (enablement_condition, pseudo_data.get('conditional', None))

                for alias in pseudo_data.get('aliases', []):
                    user_agent_part_alias_map[alias] = (enablement_condition, pseudo_data.get('conditional', None))
                continue

            # Generate code for pseudo-elements that are not user agent parts.
            if 'conditional' in pseudo_data:
                writer.write(f'#if {pseudo_data["conditional"]}')

            with writer.indent():
                writer.write(f'case PseudoElement::{format_name_for_enum_class(pseudo_name)}:')

                with writer.indent():
                    writer.write(f'return {enablement_condition};')

            if 'conditional' in pseudo_data:
                writer.write('#endif')

        # Generate code for user agent parts.
        def write_cases_for_user_agent_parts(map):
            for pseudo_name, (enablement_condition, conditional) in map.items():
                if conditional:
                    writer.write(f'#if {conditional}')

                with writer.indent(), writer.indent():
                    if ' && ' in enablement_condition:
                        enablement_condition = f'({enablement_condition})'

                    writer.write(f'if (!{enablement_condition} && equalLettersIgnoringASCIICase(name, "{pseudo_name}"_s))')
                    with writer.indent():
                        writer.write('return false;')

                if conditional:
                    writer.write('#endif')

            with writer.indent(), writer.indent():
                writer.write('return true;')

        with writer.indent():
            writer.write('case PseudoElement::UserAgentPart:')
        write_cases_for_user_agent_parts(user_agent_part_map)

        with writer.indent():
            writer.write('case PseudoElement::UserAgentPartLegacyAlias:')
        write_cases_for_user_agent_parts(user_agent_part_alias_map)

        with writer.indent():
            writer.write('default:')
            with writer.indent():
                writer.write('return true;')

            # End switch statement.
            writer.write('}')
        # End function.
        writer.write('}')

    def write_selector_text_for_pseudo_class(self, writer, pseudo_classes):
        writer.write_block("""
            inline const ASCIILiteral CSSSelector::selectorTextForPseudoClass(PseudoClass type)
            {
                switch (type) {""")

        for pseudo_name, pseudo_data in pseudo_classes.items():
            if 'conditional' in pseudo_data:
                writer.write(f'#if {pseudo_data["conditional"]}')

            with writer.indent():
                writer.write(f'case PseudoClass::{format_name_for_enum_class(pseudo_name)}:')
                with writer.indent():
                    writer.write(f'return ":{pseudo_name}"_s;')

            if 'conditional' in pseudo_data:
                writer.write('#endif')

        with writer.indent():
            writer.write('default:')
            with writer.indent():
                writer.write('ASSERT_NOT_REACHED();')
                writer.write('return ""_s;')

            # End switch statement.
            writer.write('}')
        # End function.
        writer.write('}')

    def write_name_for_user_agent_part_legacy_alias(self, writer, pseudo_elements):
        writer.write_block("""
            inline const ASCIILiteral CSSSelector::nameForUserAgentPartLegacyAlias(StringView alias)
            {""")

        for pseudo_name, pseudo_data in pseudo_elements.items():
            if not key_is_true(pseudo_data, 'user-agent-part'):
                continue
            if 'aliases' not in pseudo_data:
                continue

            if 'conditional' in pseudo_data:
                writer.write(f'#if {pseudo_data["conditional"]}')

            with writer.indent():
                for alias in pseudo_data['aliases']:
                    writer.write(f'if (equalLettersIgnoringASCIICase(alias, "{alias}"_s))')
                    with writer.indent():
                        writer.write(f'return "{pseudo_name}"_s;')

            if 'conditional' in pseudo_data:
                writer.write('#endif')

        with writer.indent():
            writer.write('ASSERT_NOT_REACHED();')
            writer.write('return ""_s;')
        writer.write('}')

    def write_pseudo_argument_check_function(self, writer, function_name, enum_name, argument_types, pseudos):
        writer.write_block(f"""
            inline bool CSSSelector::{function_name}({enum_name} type)
            {{
                switch (type) {{""")

        for pseudo_name, pseudo_data in pseudos.items():
            if 'argument' not in pseudo_data or pseudo_data['argument'] not in argument_types:
                continue
            if 'conditional' in pseudo_data:
                writer.write(f'#if {pseudo_data["conditional"]}')
            with writer.indent():
                writer.write(f'case {enum_name}::{format_name_for_enum_class(pseudo_name)}:')
            if 'conditional' in pseudo_data:
                writer.write('#endif')

        with writer.indent(), writer.indent():
            writer.write('return true;')

        with writer.indent():
            writer.write('default:')
            with writer.indent():
                writer.write('return false;')
            # End switch.
            writer.write('}')
        # End function.
        writer.write('}')


class UserAgentPartsGenerator():
    def __init__(self, data):
        user_agent_parts = self.filter_data(data)

        with open('UserAgentParts.h', 'w') as output_file:
            writer = Writer(output_file)
            write_copyright_header(writer)
            write_autogeneration_comment(writer)
            write_pragma_once(writer)
            self.write_h_includes(writer)
            self.write_open_namespace(writer)
            self.write_h_functions(writer, user_agent_parts)
            self.write_close_namespace(writer)

        with open('UserAgentParts.cpp', 'w') as output_file:
            writer = Writer(output_file)
            write_copyright_header(writer)
            write_autogeneration_comment(writer)
            self.write_cpp_includes(writer)
            self.write_open_namespace(writer)
            self.write_cpp_functions(writer, user_agent_parts)
            self.write_close_namespace(writer)

    def filter_data(self, data):
        parts = {}
        for part_name, part_data in data.items():
            if 'user-agent-part' in part_data or 'user-agent-part-string' in part_data:
                parts[part_name] = part_data
        return parts

    def write_h_includes(self, writer):
        writer.newline()
        writer.write('#include <wtf/Forward.h>')

    def write_open_namespace(self, writer):
        writer.newline()
        writer.write('namespace WebCore::UserAgentParts {')
        writer.newline()

    def write_h_functions(self, writer, user_agent_parts):
        for part_name, part_data in user_agent_parts.items():
            if 'conditional' in part_data:
                writer.write(f'#if {part_data["conditional"]}')
            writer.write(f'const AtomString& {format_name_for_function(part_name)}();')
            if 'conditional' in part_data:
                writer.write('#endif')
        writer.newline()

    def write_close_namespace(self, writer):
        writer.write('} // WebCore::UserAgentParts')

    def write_cpp_includes(self, writer):
        writer.newline()
        writer.write('#include "config.h"')
        writer.write('#include "UserAgentParts.h"')
        writer.newline()
        writer.write('#include <wtf/NeverDestroyed.h>')
        writer.write('#include <wtf/text/AtomString.h>')

    def write_cpp_functions(self, writer, user_agent_parts):
        for part_name, part_data in user_agent_parts.items():
            part_function_name = format_name_for_function(part_name)
            if 'conditional' in part_data:
                writer.write(f'#if {part_data["conditional"]}')
            writer.write(f'const AtomString& {part_function_name}()')
            writer.write('{')
            with writer.indent():
                writer.write(f'static MainThreadNeverDestroyed<const AtomString> {part_function_name}("{part_name}"_s);')
                writer.write(f'return {part_function_name};')
            writer.write('}')
            if 'conditional' in part_data:
                writer.write('#endif')
            writer.newline()


# - MARK: Script entry point.

def main():
    parser = argparse.ArgumentParser(description='Process CSS pseudo-class & pseudo-element definitions.')
    parser.add_argument('--selectors', default='CSSPseudoSelectors.json')
    parser.add_argument('--gperf-executable')
    parser.add_argument('--defines')
    args = parser.parse_args()

    input_file = open(args.selectors, 'r', encoding='utf-8')
    input_data = json.load(input_file)
    webcore_defines = [i.strip() for i in args.defines.split(' ')]

    InputValidator(input_data)
    GPerfGenerator(input_data, args.gperf_executable, webcore_defines)
    CSSSelectorEnumGenerator(input_data)
    CSSSelectorInlinesGenerator(input_data)
    UserAgentPartsGenerator(input_data['pseudo-elements'])


if __name__ == "__main__":
    main()
