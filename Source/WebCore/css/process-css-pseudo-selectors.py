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

import itertools
import json
import os
import sys
import subprocess
import textwrap

# - MARK: Input file validation.

COMMON_KNOWN_KEY_TYPES = {
    'aliases': [list],
    'argument': [str],
    'comment': [str],
    'condition': [str],
    'settings-flag': [str],
    'status': [str],
}

KNOWN_KEY_TYPES = {
    'pseudo-classes': COMMON_KNOWN_KEY_TYPES,
    'pseudo-elements': dict(COMMON_KNOWN_KEY_TYPES, **{
        'supports-single-colon-for-compatibility': [bool],
        'user-agent-part': [bool],
    })
}


class InputValidator:
    def __init__(self, input_data):
        self.check_field_documentation(input_data)
        self.validate_fields(input_data, 'pseudo-classes')
        self.validate_fields(input_data, 'pseudo-elements')

    def check_field_documentation(self, input_data):
        documentation = input_data['documentation']['fields']
        for pseudo_type in KNOWN_KEY_TYPES:
            for field in KNOWN_KEY_TYPES[pseudo_type]:
                if field not in documentation:
                    raise Exception('Missing documentation for field "{}" found in "{}".'.format(field, pseudo_type))

    def validate_fields(self, input_data, pseudo_type):
        for pseudo_name, pseudo_data in input_data[pseudo_type].items():
            for key, value in pseudo_data.items():
                # Check for unknown fields.
                if key not in KNOWN_KEY_TYPES[pseudo_type]:
                    raise Exception('Unknown field "{}" for {} found in "{}".'.format(key, pseudo_type, pseudo_name))

                # Check if fields match expected types.
                expected_types = KNOWN_KEY_TYPES[pseudo_type][key]
                if type(value) not in expected_types:
                    raise Exception('Invalid value type {} for "{}" in "{}". Expected type in set "{}".'.format(type(value), key, pseudo_name, expected_types))

                # Validate "argument" field.
                if key == 'argument':
                    if value not in ['required', 'optional']:
                        raise Exception('Invalid "argument" field in {}. The field should either be absent, "required" or "optional".'.format(pseudo_name))

                    if key_is_true(pseudo_data, 'user-agent-part') and pseudo_type == 'pseudo-elements':
                        raise Exception('Setting "argument" along with "user-agent-part": true is not supported.')


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
    writer.write_block("""
        /*
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
    return key in object and object[key]


def expand_ifdef_condition(condition):
    return condition.replace('(', '_').replace(')', '')


def is_pseudo_selector_enabled(selector, webcore_defines):
    if 'condition' not in selector:
        return True
    return expand_ifdef_condition(selector['condition']) in webcore_defines


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

            if 'aliases' in pseudo_class:
                for alias in pseudo_class['aliases']:
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

            if 'aliases' in pseudo_element:
                for alias in pseudo_element['aliases']:
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
        writer.write_block("""
            %struct-type
            %define initializer-suffix ,{0}
            %define class-name {1}
            %omit-struct-type
            %language=C++
            %readonly-tables
            %global-table
            %ignore-case
            %compare-strncmp
            %enum

            struct {2};
            """.format(
            '{std::nullopt,std::nullopt}' if generator_type == GeneratorTypes['PSEUDO_CLASS_AND_COMPATIBILITY'] else 'std::nullopt',
            'SelectorPseudoClassAndCompatibilityElementMapHash' if generator_type == GeneratorTypes['PSEUDO_CLASS_AND_COMPATIBILITY'] else 'SelectorPseudoElementMapHash',
            'SelectorPseudoClassOrCompatibilityPseudoElementEntry' if generator_type == GeneratorTypes['PSEUDO_CLASS_AND_COMPATIBILITY'] else 'SelectorPseudoTypeEntry',
        ))

        writer.write('%%')

        def prefix_value(prefix, value):
            if value == 'std::nullopt':
                return value
            return 'CSSSelector::{}::{}'.format(prefix, value)

        for key in self.mapping:
            value = self.mapping[key]
            if generator_type == GeneratorTypes['PSEUDO_CLASS_AND_COMPATIBILITY']:
                writer.write('"{}", {{{}, {}}}'.format(key, prefix_value('PseudoClass', value[0]), prefix_value('PseudoElement', value[1])))
            else:
                writer.write('"{}", {}'.format(key, prefix_value('PseudoElement', value)))

        writer.write('%%')

    def write_parsing_function_definitions_for_pseudo_class(self, writer):
        longest_keyword_length = len(max(self.mapping, key=len))
        writer.write_block("""
        static inline const SelectorPseudoClassOrCompatibilityPseudoElementEntry* parsePseudoClassAndCompatibilityElementString(const LChar* characters, unsigned length)
        {
            return SelectorPseudoClassAndCompatibilityElementMapHash::in_word_set(reinterpret_cast<const char*>(characters), length);
        }""")

        writer.write_block("""
        static inline const SelectorPseudoClassOrCompatibilityPseudoElementEntry* parsePseudoClassAndCompatibilityElementString(const UChar* characters, unsigned length)
        {{
            constexpr unsigned maxKeywordLength = {};
            LChar buffer[maxKeywordLength];
            if (length > maxKeywordLength)
                return nullptr;

            for (unsigned i = 0; i < length; ++i) {{
                UChar character = characters[i];
                if (!isLatin1(character))
                    return nullptr;

                buffer[i] = static_cast<LChar>(character);
            }}
            return parsePseudoClassAndCompatibilityElementString(buffer, length);
        }}""".format(longest_keyword_length))

        writer.write_block("""
        PseudoClassOrCompatibilityPseudoElement parsePseudoClassAndCompatibilityElementString(StringView pseudoTypeString)
        {
            const SelectorPseudoClassOrCompatibilityPseudoElementEntry* entry;
            if (pseudoTypeString.is8Bit())
                entry = parsePseudoClassAndCompatibilityElementString(pseudoTypeString.characters8(), pseudoTypeString.length());
            else
                entry = parsePseudoClassAndCompatibilityElementString(pseudoTypeString.characters16(), pseudoTypeString.length());

            if (entry)
                return entry->pseudoTypes;
            return { std::nullopt, std::nullopt };
        }""")

    def write_parsing_function_definitions_for_pseudo_element(self, writer):
        longest_keyword_length = len(max(self.mapping, key=len))
        writer.write_block("""
            static inline std::optional<CSSSelector::PseudoElement> parsePseudoElementString(const LChar* characters, unsigned length)
            {
                if (const SelectorPseudoTypeEntry* entry = SelectorPseudoElementMapHash::in_word_set(reinterpret_cast<const char*>(characters), length))
                    return entry->type;
                return std::nullopt;
            }""")

        writer.write_block("""
            static inline std::optional<CSSSelector::PseudoElement> parsePseudoElementString(const UChar* characters, unsigned length)
            {{
                constexpr unsigned maxKeywordLength = {};
                LChar buffer[maxKeywordLength];
                if (length > maxKeywordLength)
                    return std::nullopt;

                for (unsigned i = 0; i < length; ++i) {{
                    UChar character = characters[i];
                    if (!isLatin1(character))
                        return std::nullopt;

                    buffer[i] = static_cast<LChar>(character);
                }}
                return parsePseudoElementString(buffer, length);
            }}""".format(longest_keyword_length))

        writer.write_block("""
            std::optional<CSSSelector::PseudoElement> parsePseudoElementString(StringView pseudoTypeString)
            {
                if (pseudoTypeString.is8Bit())
                    return parsePseudoElementString(pseudoTypeString.characters8(), pseudoTypeString.length());
                return parsePseudoElementString(pseudoTypeString.characters16(), pseudoTypeString.length());
            }""")

    def write_end_ignore_warning(self, writer):
        writer.newline()
        writer.write('IGNORE_WARNINGS_END')


class GPerfGenerator:
    def __init__(self, data, webcore_defines):
        self.webcore_defines = webcore_defines
        self.data = data

        self.generate_pseudo_class_and_compatibility_gperf()
        self.generate_pseudo_element_gperf()
        self.generate_cpp_from_gperf()

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

    def generate_cpp_from_gperf(self):
        gperf_command = sys.argv[2]
        if 'GPERF' in os.environ:
            gperf_command = os.environ['GPERF']

        if subprocess.call([gperf_command, '--key-positions=*', '-m', '10', '-s', '2', 'SelectorPseudoClassAndCompatibilityElementMap.gperf', '--output-file=SelectorPseudoClassAndCompatibilityElementMap.cpp']) != 0:
            print("Error when generating SelectorPseudoClassAndCompatibilityElementMap.cpp from SelectorPseudoClassAndCompatibilityElementMap.gperf :(")
            sys.exit(1)

        if subprocess.call([gperf_command, '--key-positions=*', '-m', '10', '-s', '2', 'SelectorPseudoElementMap.gperf', '--output-file=SelectorPseudoElementMap.cpp']) != 0:
            print("Error when generating SelectorPseudoElementMap.cpp from SelectorPseudoElementMap.gperf :(")
            sys.exit(1)


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
            self.write_enum_class(writer, "PseudoElement", self.pseudo_enum_values(pseudo_elements) + [('UserAgentPart', None), ('UserAgentPartLegacyAlias', None)])
            close_namespace_webcore(writer)

    def pseudo_enum_values(self, values):
        enum_values = []
        for pseudo_name, pseudo_data in values.items():
            if key_is_true(pseudo_data, 'user-agent-part'):
                continue
            if 'condition' in pseudo_data:
                enum_values.append((format_name_for_enum_class(pseudo_name), pseudo_data['condition']))
            else:
                enum_values.append((format_name_for_enum_class(pseudo_name), None))
        return enum_values

    def write_enum_class(self, writer, type_name, values):
        writer.newline()
        writer.write('enum class CSSSelector{} : uint8_t {{'.format(type_name))
        for value in values:
            has_condition = value[1] is not None
            if has_condition:
                writer.write('#if {}'.format(value[1]))
            with writer.indent():
                writer.write("{},".format(value[0]))
            if has_condition:
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
            self.write_name_for_shadow_pseudo_element_legacy_alias(writer, pseudo_elements)
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
                conditions.append('{}()'.format(settings_flag))
            else:
                conditions.append('context.{}'.format(settings_flag))

        if is_internal:
            conditions.append('context.mode == UASheetMode')

        if len(conditions) == 1:
            return conditions[0]

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

            if 'condition' in pseudo_data:
                writer.write('#if {}'.format(pseudo_data['condition']))

            settings_flag = pseudo_data['settings-flag'] if 'settings-flag' in pseudo_data else None
            with writer.indent():
                writer.write('case PseudoClass::{}:'.format(format_name_for_enum_class(pseudo_name)))
                enablement_condition = self.format_enablement_condition(settings_flag, is_internal_pseudo)
                with writer.indent():
                    writer.write('return {};'.format(enablement_condition))

            if 'condition' in pseudo_data:
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

        shadow_map = {}
        shadow_alias_map = {}

        # Collect shadow pseudo elements and generate code for non-shadow pseudo elements.
        for pseudo_name, pseudo_data in values.items():
            is_internal_pseudo = pseudo_name.startswith('-internal-')
            if 'settings-flag' not in pseudo_data and not is_internal_pseudo:
                continue

            settings_flag = pseudo_data['settings-flag'] if 'settings-flag' in pseudo_data else None
            enablement_condition = self.format_enablement_condition(settings_flag, is_internal_pseudo)

            if key_is_true(pseudo_data, 'user-agent-part'):
                if 'condition' in pseudo_data:
                    shadow_map[pseudo_name] = (enablement_condition, pseudo_data['condition'])
                else:
                    shadow_map[pseudo_name] = (enablement_condition, None)

                if 'aliases' in pseudo_data:
                    for alias in pseudo_data['aliases']:
                        if 'condition' in pseudo_data:
                            shadow_alias_map[alias] = (enablement_condition, pseudo_data['condition'])
                        else:
                            shadow_alias_map[alias] = (enablement_condition, None)
                continue

            # Generate code for non-shadow pseudo elements.
            if 'condition' in pseudo_data:
                writer.write('#if {}'.format(pseudo_data['condition']))

            with writer.indent():
                writer.write('case PseudoElement::{}:'.format(format_name_for_enum_class(pseudo_name)))

                with writer.indent():
                    writer.write('return {};'.format(enablement_condition))

            if 'condition' in pseudo_data:
                writer.write('#endif')

        # Generate code for shadow pseudo elements.
        def write_condition_cases_for_shadow_elements(map):
            for pseudo_name, conditions in map.items():
                has_ifdef_condition = conditions[1] is not None
                enablement_condition = conditions[0]

                if has_ifdef_condition:
                    writer.write('#if {}'.format(conditions[1]))

                with writer.indent(), writer.indent():
                    if ' && ' in enablement_condition:
                        enablement_condition = '({})'.format(enablement_condition)

                    writer.write('if (!{} && equalLettersIgnoringASCIICase(name, "{}"_s))'.format(enablement_condition, pseudo_name))
                    with writer.indent():
                        writer.write('return false;')

                if has_ifdef_condition:
                    writer.write('#endif')

            with writer.indent(), writer.indent():
                writer.write('return true;')

        with writer.indent():
            writer.write('case PseudoElement::UserAgentPart:')
        write_condition_cases_for_shadow_elements(shadow_map)

        with writer.indent():
            writer.write('case PseudoElement::UserAgentPartLegacyAlias:')
        write_condition_cases_for_shadow_elements(shadow_alias_map)

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
            if 'condition' in pseudo_data:
                writer.write('#if {}'.format(pseudo_data['condition']))

            with writer.indent():
                writer.write('case PseudoClass::{}:'.format(format_name_for_enum_class(pseudo_name)))
                with writer.indent():
                    writer.write('return ":{}"_s;'.format(pseudo_name))

            if 'condition' in pseudo_data:
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

    def write_name_for_shadow_pseudo_element_legacy_alias(self, writer, pseudo_elements):
        writer.write_block("""
            inline const ASCIILiteral CSSSelector::nameForShadowPseudoElementLegacyAlias(StringView alias)
            {""")

        for pseudo_name, pseudo_data in pseudo_elements.items():
            if not key_is_true(pseudo_data, 'user-agent-part'):
                continue
            if 'aliases' not in pseudo_data:
                continue

            if 'condition' in pseudo_data:
                writer.write('#if {}'.format(pseudo_data['condition']))

            with writer.indent():
                for alias in pseudo_data['aliases']:
                    writer.write('if (equalLettersIgnoringASCIICase(alias, "{}"_s))'.format(alias))
                    with writer.indent():
                        writer.write('return "{}"_s;'.format(pseudo_name))

            if 'condition' in pseudo_data:
                writer.write('#endif')

        with writer.indent():
            writer.write('ASSERT_NOT_REACHED();')
            writer.write('return ""_s;')
        writer.write('}')

    def write_pseudo_argument_check_function(self, writer, function_name, enum_name, argument_types, pseudos):
        writer.write_block("""
            inline bool CSSSelector::{}({} type)
            {{
                switch (type) {{""".format(function_name, enum_name))

        for pseudo_name, pseudo_data in pseudos.items():
            if 'argument' not in pseudo_data or pseudo_data['argument'] not in argument_types:
                continue
            if 'condition' in pseudo_data:
                writer.write('#if {}'.format(pseudo_data['condition']))
            with writer.indent():
                writer.write('case {}::{}:'.format(enum_name, format_name_for_enum_class(pseudo_name)))
            if 'condition' in pseudo_data:
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


# - MARK: Script entry point.

def main():
    input_file = open(sys.argv[1], 'r')
    input_data = json.load(input_file)
    webcore_defines = [i.strip() for i in sys.argv[-1].split(' ')]

    InputValidator(input_data)
    GPerfGenerator(input_data, webcore_defines)
    CSSSelectorEnumGenerator(input_data)
    CSSSelectorInlinesGenerator(input_data)

main()
