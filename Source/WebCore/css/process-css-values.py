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
import re
import subprocess
import textwrap


class ParsingContext:
    def __init__(self, *, defines_string, parsing_for_codegen, verbose):
        if defines_string:
            self.conditionals = frozenset(defines_string.split(' '))
        else:
            self.conditionals = frozenset()
        self.parsing_for_codegen = parsing_for_codegen
        self.verbose = verbose

    def is_enabled(self, *, conditional):
        if "|" in conditional:
            return any([self.is_enabled(conditional=c) for c in conditional.split("|")])
        if "&" in conditional:
            return all([self.is_enabled(conditional=c) for c in conditional.split("&")])
        if conditional[0] == '!':
            return conditional[1:] not in self.conditionals
        return conditional in self.conditionals


class Value:
    def __init__(self, name, id_without_prefix, conditional):
        self.name = name
        self.id_without_prefix = id_without_prefix or Value.convert_name_to_id(name)
        self.conditional = conditional

    def __str__(self):
        if self.conditional:
            return f"Value [{self.name}, id={self.id_without_scope}, conditional={self.conditional}]"
        return f"Value [{self.name}, id={self.id_without_scope}]"

    def __repr__(self):
        return self.__str__()

    @staticmethod
    def convert_name_to_id(name):
        return re.sub(r'(^[^-])|-(.)', lambda m: (m[1] or m[2]).upper(), name)

    @property
    def name_lowercase(self):
        return self.name.lower()

    @property
    def id_without_scope(self):
        return f"CSSValue{self.id_without_prefix}"

    @property
    def id(self):
        return f"CSSValueID::CSSValue{self.id_without_prefix}"


def attribute_from_attribute_string(attribute, attribute_name, attribute_string, value):
    attribute_parts = attribute_string.partition("=")

    if attribute_parts[0] != attribute_name:
        return attribute

    if attribute:
        raise Exception(f"More than one '{attribute_name}' attribute specified for value '{value}'.")

    if not attribute_parts[2]:
        raise Exception(f"Empty '{attribute_name}' attribute specified for value '{value}'.")
    return attribute_parts[2]


def values_for_values_file(parsing_context, values_file):
    for line in values_file:
        # Remove any text after a "//" comment string is started.
        index = line.find("//")
        if index != -1:
            line = line[:index]

        # Remove any trailing whitespace.
        line = line.rstrip()

        # If the line is empty at this point, we can just skip it.
        if not line:
            continue

        # Parse the line into its constituent parts. A "value" and set of
        # attributes (currently only two attributes, "enable-if" and "id"
        # are supported).
        parts = line.split(" ")

        # The first part will always be the name.
        name = parts[0]

        # There may additionally be attributes of the form "foo=bar" after
        # the name.
        conditional = None
        id = None

        for attribute_string in parts[1:]:
            conditional = attribute_from_attribute_string(conditional, "enable-if", attribute_string, name)
            id = attribute_from_attribute_string(id, "id", attribute_string, name)

        if conditional and not parsing_context.is_enabled(conditional=conditional):
            if parsing_context.verbose:
                print(f"SKIPPED value {name} due to failing to satisfy 'enable-if' condition, '{conditional}', with active macro set")
            continue

        yield Value(name, id, conditional)


# GENERATION

class GenerationContext:
    def __init__(self, values, *, verbose, gperf_executable):
        self.values = values
        self.verbose = verbose
        self.gperf_executable = gperf_executable

    # Shared generation constants.

    number_of_predefined_values = 1

    # Runs `gperf` on the output of the generated file CSSValuesNames.gperf

    def run_gperf(self):
        gperf_command = self.gperf_executable or os.environ['GPERF']

        gperf_result_code = subprocess.call([gperf_command, '--key-positions=*', '-D', '-n', '-s', '2', 'CSSValueKeywords.gperf', '--output-file=CSSValueKeywords.cpp'])
        if gperf_result_code != 0:
            raise Exception(f"Error when generating CSSValueKeywords.cpp from CSSValueKeywords.gperf: {gperf_result_code}")

    # Helper generator functions for CSSValueKeywords.gperf

    def _generate_css_value_keywords_gperf_heading(self, *, to):
        to.write(textwrap.dedent("""\
            %{
            // This file is automatically generated from CSSValueKeywords.in and SVGCSSValueKeywords.in by the process-css-values script. Do not edit it."

            #include "config.h"
            #include "CSSValueKeywords.h"

            #include <wtf/ASCIICType.h>
            #include <wtf/NeverDestroyed.h>
            #include <wtf/text/AtomString.h>
            #include <string.h>

            IGNORE_WARNINGS_BEGIN("implicit-fallthrough")

            // Older versions of gperf generate code using the `register` keyword.
            #define register

            namespace WebCore {

            %}
            """))

    def _generate_css_value_keywords_gperf_footing(self, *, to):
        to.write(textwrap.dedent("""\
            } // namespace WebCore

            IGNORE_WARNINGS_END
            """))

    def _generate_gperf_definition(self, *, to):
        to.write(textwrap.dedent("""\
            %struct-type
            struct CSSValueHashTableEntry {
                const char* name;
                uint16_t id;
            };
            %language=C++
            %readonly-tables
            %7bit
            %compare-strncmp
            %define class-name CSSValueKeywordsHash
            %enum
            %%
            """))

        for value in self.values:
            to.write(f"{value.name_lowercase}, {value.id_without_scope}\n")

        to.write("%%\n")

    def _generate_name_string_tables(self, *, to):
        to.write(f"constexpr ASCIILiteral valueList[] = {{\n")
        to.write(f"    \"\"_s,\n")

        for value in self.values:
            to.write(f"    \"{value.name}\"_s,\n")

        to.write(f"    ASCIILiteral()\n")
        to.write(f"}};\n")
        to.write(f"constexpr ASCIILiteral valueListForSerialization[] = {{\n")
        to.write(f"    \"\"_s,\n")

        for value in self.values:
            to.write(f"    \"{value.name_lowercase}\"_s,\n")

        to.write(f"    ASCIILiteral()\n")
        to.write(f"}};\n")

    def _generate_lookup_functions(self, *, to):
        to.write(textwrap.dedent("""
            CSSValueID findCSSValueKeyword(const char* characters, unsigned length)
            {
                auto* value = CSSValueKeywordsHash::in_word_set(characters, length);
                return value ? static_cast<CSSValueID>(value->id) : CSSValueInvalid;
            }

            ASCIILiteral nameLiteral(CSSValueID id)
            {
                if (static_cast<uint16_t>(id) >= numCSSValueKeywords)
                    return { };
                return valueList[id];
            }

            // When serializing a CSS keyword, it should be converted to ASCII lowercase.
            // https://drafts.csswg.org/cssom/#serialize-a-css-component-value
            ASCIILiteral nameLiteralForSerialization(CSSValueID id)
            {
                if (static_cast<uint16_t>(id) >= numCSSValueKeywords)
                    return { };
                return valueListForSerialization[id];
            }

            const AtomString& nameString(CSSValueID id)
            {
                if (static_cast<uint16_t>(id) >= numCSSValueKeywords)
                    return nullAtom();

                static NeverDestroyed<std::array<AtomString, numCSSValueKeywords>> strings;
                auto& string = strings.get()[id];
                if (string.isNull())
                    string = valueList[id];
                return string;
            }

            // When serializing a CSS keyword, it should be converted to ASCII lowercase.
            // https://drafts.csswg.org/cssom/#serialize-a-css-component-value
            const AtomString& nameStringForSerialization(CSSValueID id)
            {
                if (static_cast<uint16_t>(id) >= numCSSValueKeywords)
                    return nullAtom();

                static NeverDestroyed<std::array<AtomString, numCSSValueKeywords>> strings;
                auto& string = strings.get()[id];
                if (string.isNull())
                    string = valueListForSerialization[id];
                return string;
            }

            """))

    def generate_css_value_keywords_gperf(self):
        with open('CSSValueKeywords.gperf', 'w') as output_file:
            self._generate_css_value_keywords_gperf_heading(
                to=output_file
            )

            self._generate_gperf_definition(
                to=output_file
            )

            self._generate_name_string_tables(
                to=output_file
            )

            self._generate_lookup_functions(
                to=output_file
            )

            self._generate_css_value_keywords_gperf_footing(
                to=output_file
            )

    # Helper generator functions for CSSValueKeywords.h

    def _generate_css_value_keywords_h_heading(self, *, to):
        to.write(textwrap.dedent("""\
            // This file is automatically generated from CSSValueKeywords.in and SVGCSSValueKeywords.in by the process-css-values script. Do not edit it."

            #pragma once

            #include <array>
            #include <wtf/HashFunctions.h>
            #include <wtf/HashTraits.h>

            namespace WebCore {

            """))

    def _generate_css_value_keywords_h_property_constants(self, *, to):
        to.write(f"enum CSSValueID : uint16_t {{\n")
        to.write(f"    CSSValueInvalid = 0,\n")

        count = GenerationContext.number_of_predefined_values
        max_length = 0

        for value in self.values:
            to.write(f"    {value.id_without_scope} = {count},\n")

            count += 1
            max_length = max(len(value.name), max_length)

        last = count - 1

        to.write(f"}};\n\n")

        to.write(f"constexpr uint16_t numCSSValueKeywords = {count};\n")
        to.write(f"constexpr uint16_t lastCSSValueKeyword = {last};\n")
        to.write(f"constexpr unsigned maxCSSValueKeywordLength = {max_length};\n")

    def _generate_css_value_keywords_h_forward_declarations(self, *, to):
        to.write(textwrap.dedent("""\
            CSSValueID findCSSValueKeyword(const char* characters, unsigned length);
            ASCIILiteral nameLiteral(CSSValueID);
            ASCIILiteral nameLiteralForSerialization(CSSValueID); // Lowercase.
            const AtomString& nameString(CSSValueID);
            const AtomString& nameStringForSerialization(CSSValueID); // Lowercase.

            struct AllCSSValueKeywordsRange {
                struct Iterator {
                    uint16_t index { 0 };
                    constexpr CSSValueID operator*() const { return static_cast<CSSValueID>(index); }
                    constexpr Iterator& operator++() { ++index; return *this; }
                    constexpr bool operator==(std::nullptr_t) const { return index >= numCSSValueKeywords; }
                    constexpr bool operator!=(std::nullptr_t) const { return index < numCSSValueKeywords; }
                };
                static constexpr Iterator begin() { return { }; }
                static constexpr std::nullptr_t end() { return nullptr; }
                static constexpr uint16_t size() { return numCSSValueKeywords; }
            };
            constexpr AllCSSValueKeywordsRange allCSSValueKeywords() { return { }; }

            } // namespace WebCore
            """))

    def _generate_css_value_keywords_h_hash_traits(self, *, to):
        to.write(textwrap.dedent("""
            namespace WTF {

            template<> struct DefaultHash<WebCore::CSSValueID> : IntHash<unsigned> { };

            template<> struct HashTraits<WebCore::CSSValueID> : StrongEnumHashTraits<WebCore::CSSValueID> { };

            }
            """))

    def _generate_css_value_keywords_h_iterator_traits(self, *, to):
        to.write(textwrap.dedent("""
            namespace std {

            template<> struct iterator_traits<WebCore::AllCSSValueKeywordsRange::Iterator> { using value_type = WebCore::CSSValueID; };

            }
            """))

    def _generate_css_value_keywords_h_footing(self, *, to):
        to.write("\n")

    def generate_css_value_keywords_h(self):
        with open('CSSValueKeywords.h', 'w') as output_file:
            self._generate_css_value_keywords_h_heading(
                to=output_file
            )

            self._generate_css_value_keywords_h_property_constants(
                to=output_file
            )

            self._generate_css_value_keywords_h_forward_declarations(
                to=output_file
            )

            self._generate_css_value_keywords_h_hash_traits(
                to=output_file
            )

            self._generate_css_value_keywords_h_iterator_traits(
                to=output_file
            )

            self._generate_css_value_keywords_h_footing(
                to=output_file
            )


def main():
    parser = argparse.ArgumentParser(description='Process CSS property definitions.')
    parser.add_argument('--values', action='append')
    parser.add_argument('--defines')
    parser.add_argument('--gperf-executable')
    parser.add_argument('-v', '--verbose', action='store_true')
    args = parser.parse_args()

    if not args.values:
        args.values = ["CSSValueKeywords.in"]

    parsing_context = ParsingContext(defines_string=args.defines, parsing_for_codegen=True, verbose=args.verbose)

    parsed_values = []
    for values_file_path in args.values:
        with open(values_file_path, "r") as values_file:
            parsed_values += values_for_values_file(parsing_context, values_file)

    if args.verbose:
        print(f"{len(parsed_values)} values active for code generation")

    generation_context = GenerationContext(parsed_values, verbose=args.verbose, gperf_executable=args.gperf_executable)

    generation_context.generate_css_value_keywords_h()
    generation_context.generate_css_value_keywords_gperf()
    generation_context.run_gperf()


if __name__ == "__main__":
    main()
