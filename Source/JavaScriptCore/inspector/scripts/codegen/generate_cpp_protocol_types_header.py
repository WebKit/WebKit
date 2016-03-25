#!/usr/bin/env python
#
# Copyright (c) 2014, 2016 Apple Inc. All rights reserved.
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
import re
import string
from string import Template

from cpp_generator import CppGenerator
from cpp_generator_templates import CppGeneratorTemplates as CppTemplates
from generator import Generator, ucfirst
from models import EnumType, ObjectType, PrimitiveType, AliasedType, ArrayType, Frameworks

log = logging.getLogger('global')


class CppProtocolTypesHeaderGenerator(CppGenerator):
    def __init__(self, model, input_filepath):
        CppGenerator.__init__(self, model, input_filepath)

    def output_filename(self):
        return "%sProtocolObjects.h" % self.protocol_name()

    def generate_output(self):
        domains = self.domains_to_generate()
        self.calculate_types_requiring_shape_assertions(domains)

        headers = set([
            '<inspector/InspectorProtocolTypes.h>',
            '<wtf/Assertions.h>',
        ])

        export_macro = self.model().framework.setting('export_macro', None)

        header_args = {
            'includes': '\n'.join(['#include ' + header for header in sorted(headers)]),
            'typedefs': '',
        }

        return_type = 'String'
        return_type_with_export_macro = [return_type]
        if export_macro is not None:
            return_type_with_export_macro[:0] = [export_macro]

        sections = []
        sections.append(self.generate_license())
        sections.append(Template(CppTemplates.HeaderPrelude).substitute(None, **header_args))
        sections.append('namespace Protocol {')
        sections.append(self._generate_forward_declarations(domains))
        sections.append(self._generate_typedefs(domains))
        sections.append('%s getEnumConstantValue(int code);' % ' '.join(return_type_with_export_macro))
        sections.append('\n'.join([
            'template<typename T> %s getEnumConstantValue(T enumValue)' % return_type,
            '{',
            '    return getEnumConstantValue(static_cast<int>(enumValue));',
            '}']))

        builder_sections = map(self._generate_builders_for_domain, domains)
        sections.extend(filter(lambda section: len(section) > 0, builder_sections))
        sections.append(self._generate_forward_declarations_for_binding_traits())
        sections.append('} // namespace Protocol')
        sections.append(Template(CppTemplates.HeaderPostlude).substitute(None, **header_args))
        return "\n\n".join(sections)

    # Private methods.

    # FIXME: move builders out of classes, uncomment forward declaration

    def _generate_forward_declarations(self, domains):
        sections = []

        for domain in domains:
            declaration_types = [decl.type for decl in domain.type_declarations]
            object_types = filter(lambda _type: isinstance(_type, ObjectType), declaration_types)
            enum_types = filter(lambda _type: isinstance(_type, EnumType), declaration_types)
            if len(object_types) + len(enum_types) == 0:
                continue

            domain_lines = []
            domain_lines.append('namespace %s {' % domain.domain_name)

            object_type_names = [_type.raw_name() for _type in object_types]
            enum_type_names = [_type.raw_name() for _type in enum_types]

            # Forward-declare all classes so the type builders won't break if rearranged.
            domain_lines.extend('class %s;' % name for name in sorted(object_type_names))
            domain_lines.extend('enum class %s;' % name for name in sorted(enum_type_names))
            domain_lines.append('} // %s' % domain.domain_name)
            sections.append(self.wrap_with_guard_for_domain(domain, '\n'.join(domain_lines)))

        if len(sections) == 0:
            return ''
        else:
            return """// Forward declarations.
%s
// End of forward declarations.
""" % '\n\n'.join(sections)

    def _generate_typedefs(self, domains):
        sections = map(self._generate_typedefs_for_domain, domains)
        sections = filter(lambda text: len(text) > 0, sections)

        if len(sections) == 0:
            return ''
        else:
            return """// Typedefs.
%s
// End of typedefs.""" % '\n\n'.join(sections)

    def _generate_typedefs_for_domain(self, domain):
        primitive_declarations = filter(lambda decl: isinstance(decl.type, AliasedType), domain.type_declarations)
        array_declarations = filter(lambda decl: isinstance(decl.type, ArrayType), domain.type_declarations)
        if len(primitive_declarations) == 0 and len(array_declarations) == 0:
            return ''

        sections = []
        for declaration in primitive_declarations:
            primitive_name = CppGenerator.cpp_name_for_primitive_type(declaration.type.aliased_type)
            typedef_lines = []
            if len(declaration.description) > 0:
                typedef_lines.append('/* %s */' % declaration.description)
            typedef_lines.append('typedef %s %s;' % (primitive_name, declaration.type_name))
            sections.append('\n'.join(typedef_lines))

        for declaration in array_declarations:
            element_type = CppGenerator.cpp_protocol_type_for_type(declaration.type.element_type)
            typedef_lines = []
            if len(declaration.description) > 0:
                typedef_lines.append('/* %s */' % declaration.description)
            typedef_lines.append('typedef Inspector::Protocol::Array<%s> %s;' % (element_type, declaration.type_name))
            sections.append('\n'.join(typedef_lines))

        lines = []
        lines.append('namespace %s {' % domain.domain_name)
        lines.append('\n'.join(sections))
        lines.append('} // %s' % domain.domain_name)
        return self.wrap_with_guard_for_domain(domain, '\n'.join(lines))

    def _generate_builders_for_domain(self, domain):
        sections = []

        for type_declaration in domain.type_declarations:
            if isinstance(type_declaration.type, EnumType):
                sections.append(self._generate_struct_for_enum_declaration(type_declaration))
            elif isinstance(type_declaration.type, ObjectType):
                sections.append(self._generate_class_for_object_declaration(type_declaration, domain))

        sections = filter(lambda section: len(section) > 0, sections)
        if len(sections) == 0:
            return ''

        lines = []
        lines.append('namespace %s {' % domain.domain_name)
        lines.append('\n'.join(sections))
        lines.append('} // %s' % domain.domain_name)
        return self.wrap_with_guard_for_domain(domain, '\n'.join(lines))

    def _generate_class_for_object_declaration(self, type_declaration, domain):
        if len(type_declaration.type_members) == 0:
            return ''

        enum_members = filter(lambda member: isinstance(member.type, EnumType) and member.type.is_anonymous, type_declaration.type_members)
        required_members = filter(lambda member: not member.is_optional, type_declaration.type_members)
        optional_members = filter(lambda member: member.is_optional, type_declaration.type_members)
        object_name = type_declaration.type_name

        lines = []
        if len(type_declaration.description) > 0:
            lines.append('/* %s */' % type_declaration.description)
        base_class = 'Inspector::InspectorObject'
        if not Generator.type_has_open_fields(type_declaration.type):
            base_class = base_class + 'Base'
        lines.append('class %s : public %s {' % (object_name, base_class))
        lines.append('public:')
        for enum_member in enum_members:
            lines.append('    // Named after property name \'%s\' while generating %s.' % (enum_member.member_name, object_name))
            lines.append(self._generate_struct_for_anonymous_enum_member(enum_member))
        lines.append(self._generate_builder_state_enum(type_declaration))

        constructor_example = []
        constructor_example.append('     * Ref<%s> result = %s::create()' % (object_name, object_name))
        for member in required_members:
            constructor_example.append('     *     .set%s(...)' % ucfirst(member.member_name))
        constructor_example.append('     *     .release()')

        builder_args = {
            'objectType': object_name,
            'constructorExample': '\n'.join(constructor_example) + ';',
        }

        lines.append(Template(CppTemplates.ProtocolObjectBuilderDeclarationPrelude).substitute(None, **builder_args))
        for type_member in required_members:
            lines.append(self._generate_builder_setter_for_member(type_member, domain))
        lines.append(Template(CppTemplates.ProtocolObjectBuilderDeclarationPostlude).substitute(None, **builder_args))
        for member in optional_members:
            lines.append(self._generate_unchecked_setter_for_member(member, domain))

        if Generator.type_has_open_fields(type_declaration.type):
            lines.append('')
            lines.append('    // Property names for type generated as open.')
            for type_member in type_declaration.type_members:
                export_macro = self.model().framework.setting('export_macro', None)
                lines.append('    %s static const char* %s;' % (export_macro, ucfirst(type_member.member_name)))

        lines.append('};')
        lines.append('')
        return '\n'.join(lines)

    def _generate_struct_for_enum_declaration(self, enum_declaration):
        lines = []
        lines.append('/* %s */' % enum_declaration.description)
        lines.extend(self._generate_struct_for_enum_type(enum_declaration.type_name, enum_declaration.type))
        return '\n'.join(lines)

    def _generate_struct_for_anonymous_enum_member(self, enum_member):
        def apply_indentation(line):
            if line.startswith(('#', '/*', '*/', '//')) or len(line) is 0:
                return line
            else:
                return '    ' + line

        indented_lines = map(apply_indentation, self._generate_struct_for_enum_type(enum_member.member_name, enum_member.type))
        return '\n'.join(indented_lines)

    def _generate_struct_for_enum_type(self, enum_name, enum_type):
        lines = []
        enum_name = ucfirst(enum_name)
        lines.append('enum class %s {' % enum_name)
        for enum_value in enum_type.enum_values():
            lines.append('    %s = %s,' % (Generator.stylized_name_for_enum_value(enum_value), self.encoding_for_enum_value(enum_value)))
        lines.append('}; // enum class %s' % enum_name)
        return lines  # The caller may want to adjust indentation, so don't join these lines.

    def _generate_builder_state_enum(self, type_declaration):
        lines = []
        required_members = filter(lambda member: not member.is_optional, type_declaration.type_members)
        enum_values = []

        lines.append('    enum {')
        lines.append('        NoFieldsSet = 0,')
        for i in range(len(required_members)):
            enum_value = "%sSet" % ucfirst(required_members[i].member_name)
            enum_values.append(enum_value)
            lines.append('        %s = 1 << %d,' % (enum_value, i))
        if len(enum_values) > 0:
            lines.append('        AllFieldsSet = (%s)' % ' | '.join(enum_values))
        else:
            lines.append('        AllFieldsSet = 0')
        lines.append('    };')
        lines.append('')
        return '\n'.join(lines)

    def _generate_builder_setter_for_member(self, type_member, domain):
        setter_args = {
            'camelName': ucfirst(type_member.member_name),
            'keyedSet': CppGenerator.cpp_setter_method_for_type(type_member.type),
            'name': type_member.member_name,
            'parameterType': CppGenerator.cpp_type_for_type_member(type_member)
        }

        lines = []
        lines.append('')
        lines.append('        Builder<STATE | %(camelName)sSet>& set%(camelName)s(%(parameterType)s value)' % setter_args)
        lines.append('        {')
        lines.append('            COMPILE_ASSERT(!(STATE & %(camelName)sSet), property_%(name)s_already_set);' % setter_args)

        if isinstance(type_member.type, EnumType):
            lines.append('            m_result->%(keyedSet)s(ASCIILiteral("%(name)s"), Inspector::Protocol::getEnumConstantValue(static_cast<int>(value)));' % setter_args)
        else:
            lines.append('            m_result->%(keyedSet)s(ASCIILiteral("%(name)s"), value);' % setter_args)
        lines.append('            return castState<%(camelName)sSet>();' % setter_args)
        lines.append('        }')
        return '\n'.join(lines)

    def _generate_unchecked_setter_for_member(self, type_member, domain):
        setter_args = {
            'camelName': ucfirst(type_member.member_name),
            'keyedSet': CppGenerator.cpp_setter_method_for_type(type_member.type),
            'name': type_member.member_name,
            'parameterType': CppGenerator.cpp_type_for_type_member(type_member)
        }

        lines = []
        lines.append('')
        lines.append('    void set%(camelName)s(%(parameterType)s value)' % setter_args)
        lines.append('    {')
        if isinstance(type_member.type, EnumType):
            lines.append('        InspectorObjectBase::%(keyedSet)s(ASCIILiteral("%(name)s"), Inspector::Protocol::getEnumConstantValue(static_cast<int>(value)));' % setter_args)
        elif CppGenerator.should_use_references_for_type(type_member.type):
            lines.append('        InspectorObjectBase::%(keyedSet)s(ASCIILiteral("%(name)s"), WTFMove(value));' % setter_args)
        else:
            lines.append('        InspectorObjectBase::%(keyedSet)s(ASCIILiteral("%(name)s"), value);' % setter_args)
        lines.append('    }')
        return '\n'.join(lines)

    def _generate_forward_declarations_for_binding_traits(self):
        # A list of (builder_type, needs_runtime_cast)
        type_arguments = []

        for domain in self.domains_to_generate():
            declarations_to_generate = filter(lambda decl: self.type_needs_shape_assertions(decl.type), domain.type_declarations)

            for type_declaration in declarations_to_generate:
                for type_member in type_declaration.type_members:
                    if isinstance(type_member.type, EnumType):
                        type_arguments.append((CppGenerator.cpp_protocol_type_for_type_member(type_member, type_declaration), False))

                if isinstance(type_declaration.type, ObjectType):
                    type_arguments.append((CppGenerator.cpp_protocol_type_for_type(type_declaration.type), Generator.type_needs_runtime_casts(type_declaration.type)))

        struct_keywords = ['struct']
        function_keywords = ['static void']
        export_macro = self.model().framework.setting('export_macro', None)
        if export_macro is not None:
            struct_keywords.append(export_macro)
            #function_keywords[1:1] = [export_macro]

        lines = []
        for argument in type_arguments:
            lines.append('template<> %s BindingTraits<%s> {' % (' '.join(struct_keywords), argument[0]))
            if argument[1]:
                lines.append('static RefPtr<%s> runtimeCast(RefPtr<Inspector::InspectorValue>&& value);' % argument[0])
            lines.append('#if !ASSERT_DISABLED')
            lines.append('%s assertValueHasExpectedType(Inspector::InspectorValue*);' % ' '.join(function_keywords))
            lines.append('#endif // !ASSERT_DISABLED')
            lines.append('};')
        return '\n'.join(lines)
