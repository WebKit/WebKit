#!/usr/bin/env python
#
# Copyright (c) 2014 Apple Inc. All rights reserved.
# Copyright (c) 2014 University of Washington. All rights reserved.
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


import logging
import string
from string import Template

from generator import Generator, ucfirst
from generator_templates import GeneratorTemplates as Templates
from models import AliasedType, ArrayType, EnumType, ObjectType

log = logging.getLogger('global')


class ProtocolTypesImplementationGenerator(Generator):
    def __init__(self, model, input_filepath):
        Generator.__init__(self, model, input_filepath)

    def output_filename(self):
        return "Inspector%sProtocolTypes.cpp" % self.model().framework.setting('prefix')

    def generate_output(self):
        domains = self.domains_to_generate()
        self.calculate_types_requiring_shape_assertions(domains)

        secondary_headers = ['<wtf/text/CString.h>']

        header_args = {
            'primaryInclude': '"Inspector%sProtocolTypes.h"' % self.model().framework.setting('prefix'),
            'secondaryIncludes': "\n".join(['#include %s' % header for header in secondary_headers]),
            'inputFilename': self._input_filepath
        }

        sections = []
        sections.append(self.generate_license())
        sections.append(Template(Templates.CppImplementationPrelude).substitute(None, **header_args))
        sections.append('namespace Protocol {')
        sections.append(self._generate_enum_mapping())
        sections.append(self._generate_open_field_names())
        builder_sections = map(self._generate_builders_for_domain, domains)
        sections.extend(filter(lambda section: len(section) > 0, builder_sections))
        sections.append('} // namespace Protocol')
        sections.append(Template(Templates.CppImplementationPostlude).substitute(None, **header_args))

        return "\n\n".join(sections)

    # Private methods.

    def _generate_enum_mapping(self):
        framework_prefix = self.model().framework.setting('prefix')
        lines = []

        lines.append('static const char* const enum_constant_values[] = {')
        lines.extend(['    "%s",' % enum_value for enum_value in self.assigned_enum_values()])
        lines.append('};')
        lines.append('')
        lines.append('String get%sEnumConstantValue(int code) {' % framework_prefix)
        lines.append('    return enum_constant_values[code];')
        lines.append('}')

        return '\n'.join(lines)

    def _generate_open_field_names(self):
        lines = []
        for domain in self.domains_to_generate():
            for type_declaration in filter(lambda decl: Generator.type_has_open_fields(decl.type), domain.type_declarations):
                for type_member in sorted(type_declaration.type_members, key=lambda member: member.member_name):
                    field_name = '::'.join(['Inspector', 'Protocol', domain.domain_name, ucfirst(type_declaration.type_name), ucfirst(type_member.member_name)])
                    lines.append('const char* %s = "%s";' % (field_name, type_member.member_name))

        return '\n'.join(lines)

    def _generate_builders_for_domain(self, domain):
        sections = []
        declarations_to_generate = filter(lambda decl: self.type_needs_shape_assertions(decl.type), domain.type_declarations)

        for type_declaration in declarations_to_generate:
            for type_member in type_declaration.type_members:
                if isinstance(type_member.type, EnumType):
                    sections.append(self._generate_assertion_for_enum(type_member, type_declaration))

            if isinstance(type_declaration.type, ObjectType):
                sections.append(self._generate_assertion_for_object_declaration(type_declaration))
                if Generator.type_needs_runtime_casts(type_declaration.type):
                    sections.append(self._generate_runtime_cast_for_object_declaration(type_declaration))

        return '\n\n'.join(sections)

    def _generate_runtime_cast_for_object_declaration(self, object_declaration):
        args = {
            'objectType': Generator.protocol_type_string_for_type(object_declaration.type)
        }
        return Template(Templates.ProtocolObjectRuntimeCast).substitute(None, **args)

    def _generate_assertion_for_object_declaration(self, object_declaration):
        required_members = filter(lambda member: not member.is_optional, object_declaration.type_members)
        optional_members = filter(lambda member: member.is_optional, object_declaration.type_members)
        should_count_properties = not Generator.type_has_open_fields(object_declaration.type)
        lines = []

        lines.append('#if !ASSERT_DISABLED')
        lines.append('void BindingTraits<%s>::assertValueHasExpectedType(Inspector::InspectorValue* value)' % (Generator.protocol_type_string_for_type(object_declaration.type)))
        lines.append("""{
    RefPtr<InspectorObject> object;
    bool castSucceeded = value->asObject(object);
    ASSERT_UNUSED(castSucceeded, castSucceeded);""")
        for type_member in required_members:
            args = {
                'memberName': type_member.member_name,
                'assertMethod': Generator.assertion_method_for_type_member(type_member, object_declaration)
            }

            lines.append("""    {
        InspectorObject::iterator %(memberName)sPos = object->find(ASCIILiteral("%(memberName)s"));
        ASSERT(%(memberName)sPos != object->end());
        %(assertMethod)s(%(memberName)sPos->value.get());
    }""" % args)

        if should_count_properties:
            lines.append('')
            lines.append('    int foundPropertiesCount = %s;' % len(required_members))

        for type_member in optional_members:
            args = {
                'memberName': type_member.member_name,
                'assertMethod': Generator.assertion_method_for_type_member(type_member, object_declaration)
            }

            lines.append("""    {
        InspectorObject::iterator %(memberName)sPos = object->find(ASCIILiteral("%(memberName)s"));
        if (%(memberName)sPos != object->end()) {
            %(assertMethod)s(%(memberName)sPos->value.get());""" % args)

            if should_count_properties:
                lines.append('            ++foundPropertiesCount;')
            lines.append('        }')
            lines.append('    }')

        if should_count_properties:
            lines.append('    if (foundPropertiesCount != object->size())')
            lines.append('        FATAL("Unexpected properties in object: %s\\n", object->toJSONString().ascii().data());')
        lines.append('}')
        lines.append('#endif // !ASSERT_DISABLED')
        return '\n'.join(lines)

    def _generate_assertion_for_enum(self, enum_member, object_declaration):
        lines = []
        lines.append('#if !ASSERT_DISABLED')
        lines.append('void %s(Inspector::InspectorValue* value)' % Generator.assertion_method_for_type_member(enum_member, object_declaration))
        lines.append('{')
        lines.append('    String result;')
        lines.append('    bool castSucceeded = value->asString(result);')
        lines.append('    ASSERT(castSucceeded);')

        assert_condition = ' || '.join(['result == "%s"' % enum_value for enum_value in enum_member.type.enum_values()])
        lines.append('    ASSERT(%s);' % assert_condition)
        lines.append('}')
        lines.append('#endif // !ASSERT_DISABLED')

        return '\n'.join(lines)
