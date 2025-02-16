#!/usr/bin/env python3
#
# Copyright (C) 2022-2024 Apple Inc. All rights reserved.
# Copyright (C) 2024 Sony Interactive Entertainment Inc.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import copy
import os
import re
import sys

# Supported type attributes:
#
# AdditionalEncoder - generate serializers for StreamConnectionEncoder in addition to IPC::Encoder.
# CreateUsing - use a custom function to call instead of the constructor or create.
# ConstructSubclass - use a subclass to construct the object. Do not include namespace.
# CustomHeader - don't include a header based on the struct/class name. Only needed for non-enum types.
# DisableMissingMemberCheck - do not check for attributes that are missed during serialization.
# Alias - this type is not a struct or class, but a typedef.
# Nested - this type is only serialized as a member of its parent, so work around the need for http://wg21.link/P0289 and don't forward declare it in the header.
# RefCounted - deserializer returns a std::optional<Ref<T>> instead of a std::optional<T>.
# LegacyPopulateFromEmptyConstructor - instead of calling a constructor with the members, call the empty constructor then insert the members one at a time.
# OptionSet - for enum classes, instead of only allowing deserialization of the exact values, allow deserialization of any bit combination of the values.
# RValue - serializer takes an rvalue reference, instead of an lvalue.
# WebKitPlatform - put serializer into a file built as part of WebKitPlatform
# CustomEncoder - Only generate the decoder, not the encoder.
# WebKitSecureCodingClass - For webkit_secure_coding declarations that need a custom way of establishing the Obj-C class to instantiate (e.g. softlinked frameworks)
# Wrapper - use a wrapper class to get members and to construct for an external type
#
# Supported member attributes:
#
# BitField - work around the need for http://wg21.link/P0572 and don't check that the serialization order matches the memory layout.
# EncodeRequestBody - Include the body of the WebCore::ResourceRequest when encoding (by default, it is omitted).
# Validator - additional C++ to validate the value when decoding
# NotSerialized - member is present in structure but intentionally not serialized.
# SecureCodingAllowed - ObjC classes to allow when decoding.
# OptionalTupleBits - This member stores bits of whether each following member is serialized. Attribute must be immediately before members with OptionalTupleBit.
# OptionalTupleBit - The name of the bit indicating whether this member is serialized.
# SupportWKKeyedCoder - For webkit_secure_coding types, in addition to the preferred property list code path, support SupportWKKeyedCoder
# Precondition - Used to fail early from a decoder, for example if a soft linked framework is not present to decode a member

class Template(object):
    def __init__(self, template_type, namespace, name, enum_storage=None):
        self.type = template_type
        self.namespace = namespace
        self.name = name
        self.enum_storage = enum_storage

    def forward_declaration(self):
        if self.enum_storage:
            return f'{self.type} {self.name} : {self.enum_storage};'
        return f'{self.type} {self.name};'

    def specialization(self):
        return f'{self.namespace}::{self.name}'


class SerializedType(object):
    def __init__(self, struct_or_class, cf_type, namespace, name, parent_class_name, members, dictionary_members, condition, attributes, templates, other_metadata=None):
        self.struct_or_class = struct_or_class
        self.cf_type = cf_type
        self.to_cf_method = None
        self.from_cf_method = None
        self.forward_declaration = None
        self.custom_secure_coding_class = None
        self.namespace = namespace
        self.name = name
        self.parent_class_name = parent_class_name
        self.parent_class = None
        self.members = members
        self.dictionary_members = dictionary_members
        self.alias = None
        self.condition = condition
        self.encoders = ['Encoder']
        self.return_ref = False
        self.construct_subclass = None
        self.create_using = False
        self.populate_from_empty_constructor = False
        self.nested = False
        self.rvalue = False
        self.webkit_platform = False
        self.members_are_subclasses = False
        self.custom_encoder = False
        self.support_wkkeyedcoder = False
        self.disableMissingMemberCheck = False
        self.debug_decoding_failure = False
        self.generic_wrapper = None
        if attributes is not None:
            for attribute in attributes.split(', '):
                if '=' in attribute:
                    key, value = attribute.split('=')
                    if key == 'AdditionalEncoder':
                        self.encoders.append(value)
                    if key == 'ConstructSubclass':
                        self.construct_subclass = value
                    if key == 'CreateUsing':
                        self.create_using = value
                    if key == 'Alias':
                        self.alias = value
                    if key == 'ToCFMethod':
                        self.to_cf_method = value
                    if key == 'FromCFMethod':
                        self.from_cf_method = value
                    if key == 'ForwardDeclaration':
                        self.forward_declaration = value
                    if key == 'WebKitSecureCodingClass':
                        self.custom_secure_coding_class = value
                    if key == 'Wrapper':
                        self.generic_wrapper = value
                else:
                    if attribute == 'Nested':
                        self.nested = True
                    elif attribute == 'RefCounted':
                        self.return_ref = True
                    elif attribute == 'DisableMissingMemberCheck':
                        self.disableMissingMemberCheck = True
                    elif attribute == 'RValue':
                        self.rvalue = True
                    elif attribute == 'WebKitPlatform':
                        self.webkit_platform = True
                    elif attribute == 'LegacyPopulateFromEmptyConstructor':
                        self.populate_from_empty_constructor = True
                    elif attribute == 'CustomEncoder':
                        self.custom_encoder = True
                    elif attribute == 'SupportWKKeyedCoder':
                        self.support_wkkeyedcoder = True
                    elif attribute == 'DebugDecodingFailure':
                        self.debug_decoding_failure = True
        self.templates = templates
        if other_metadata:
            if other_metadata == 'subclasses':
                self.members_are_subclasses = True
        if self.is_webkit_secure_coding_type():
            self.namespace = 'WebKit'
            self.webkit_platform = True

    def name_as_identifier(self):
        return re.sub(r'\W+', '_', self.name)

    def namespace_and_name(self):
        if self.cf_type is not None:
            return f'{self.cf_type}Ref'
        if self.namespace is None:
            return self.name
        return f'{self.namespace}::{self.cpp_struct_or_class_name()}'

    def namespace_if_not_wtf_and_name(self):
        if self.namespace == 'WTF':
            return self.name
        if self.namespace is None:
            return self.name
        return f'{self.namespace}::{self.cpp_struct_or_class_name()}'

    def namespace_and_name_for_construction(self, specialization):
        fulltype = None
        if self.construct_subclass:
            fulltype = f'{self.namespace}::{self.construct_subclass}'
        elif self.generic_wrapper is not None:
            fulltype = self.generic_wrapper
        else:
            fulltype = self.namespace_and_name()
        if specialization:
            fulltype = f'{fulltype}<{specialization}>'
        return fulltype

    def cf_wrapper_type(self):
        return f'{self.namespace}::{self.name}'

    def name_declaration_for_serialized_type_info(self):
        if self.cf_type is not None:
            return f'{self.cf_type}Ref'
        if self.namespace == 'WTF':
            if self.name != "UUID":
                return self.name
        return self.namespace_and_name()

    def subclass_enum_name(self):
        result = ""
        if self.namespace:
            result += self.namespace + "_"
        return f'{result}{self.name}_Subclass'

    def function_name_for_enum(self):
        return 'isValidEnum'

    def can_assert_member_order_is_correct(self):
        if self.disableMissingMemberCheck:
            return False
        for member in self.members:
            if '()' in member.name:
                return False
            if '.' in member.name:
                return False
        return True

    def members_for_serialized_type_info(self):
        return self.serialized_members()

    def serialized_members(self):
        return list(filter(lambda member: 'NotSerialized' not in member.attributes, self.members))

    def has_optional_tuple_bits(self):
        if len(self.members) == 0:
            return False
        for member in self.members:
            if 'OptionalTupleBits' in member.attributes:
                return True
        return False

    def should_skip_forward_declare(self):
        return self.nested or self.templates

    def cpp_type_from_struct_or_class(self):
        if self.is_webkit_secure_coding_type():
            return 'class'
        return self.struct_or_class

    def cpp_struct_or_class_name(self):
        if self.is_webkit_secure_coding_type():
            return f'CoreIPC{self.name}'
        return self.name

    def is_webkit_secure_coding_type(self):
        return self.struct_or_class == 'webkit_secure_coding'

    def wrapper_for_webkit_secure_coding_type(self):
        copied_type = copy.copy(self)
        copied_type.struct_or_class = 'class'
        copied_type.name = self.cpp_struct_or_class_name()
        copied_type.dictionary_members = None
        return copied_type


class SerializedEnum(object):
    def __init__(self, namespace, name, underlying_type, valid_values, condition, attributes):
        self.namespace = namespace
        self.name = name
        self.underlying_type = underlying_type
        self.valid_values = valid_values
        self.condition = condition
        self.attributes = attributes

    def namespace_and_name(self):
        if self.namespace is None:
            return self.name
        return f'{self.namespace}::{self.name}'

    def function_name(self):
        if self.is_option_set():
            return 'isValidOptionSet'
        return 'isValidEnum'

    def parameter(self):
        if self.is_option_set():
            return f'OptionSet<{self.namespace_and_name()}>'
        return self.underlying_type

    def is_option_set(self):
        return 'OptionSet' in self.attributes

    def is_nested(self):
        return 'Nested' in self.attributes

    def is_webkit_platform(self):
        return 'WebKitPlatform' in self.attributes


class MemberVariable(object):
    def __init__(self, type, name, condition, attributes, namespace=None, is_subclass=False):
        assert type == type.strip(), f"MemberVariable({type} {name}) has invalid type '{type}'"
        assert name == name.strip(), f"MemberVariable({type} {name}) has invalid name '{name}'"
        self.type = type
        self.name = name
        self.condition = condition
        self.attributes = attributes
        self.namespace = namespace
        self.is_subclass = is_subclass

    def optional_tuple_bit(self):
        for attribute in self.attributes:
            match = re.search(r'OptionalTupleBit=(.*)', attribute)
            if match:
                bit, = match.groups()
                return bit
        return None

    def optional_tuple_bits(self):
        for attribute in self.attributes:
            if attribute == 'OptionalTupleBits':
                return True
        return False

    def value_without_question_mark(self):
        value = self.name
        if self.value_is_optional():
            value = value[:-1]
        return value

    def ns_type_enum_value(self):
        value = self.value_without_question_mark()
        if value.startswith('Dictionary'):
            return 'Dictionary'
        if value.startswith('Array'):
            return 'Array'
        return value

    def array_contents(self):
        value = self.value_without_question_mark()
        if not value.startswith('Array'):
            return None
        match = re.search(r'(.*)<(.*)>', value)
        if match:
            array, contents = match.groups()
            if contents == 'String':
                return 'NSString'
            if contents == 'Data':
                return 'NSData'
            return contents
        return None

    def dictionary_contents(self):
        value = self.value_without_question_mark()
        if not value.startswith('Dictionary'):
            return None
        match = re.search(r'(.*)<(.*)>', value)
        if match:
            dictionary, contents = match.groups()
            match = re.search(r'(.*), (.*)', contents)
            if match:
                keys, values = match.groups()
                assert keys == 'String'
                return values
        return None

    def has_container_contents(self):
        return self.dictionary_contents() is not None or self.array_contents() is not None

    def ns_type(self):
        value = self.value_without_question_mark()
        if value == 'String':
            return 'NSString'
        if value.startswith('Dictionary'):
            return 'NSDictionary'
        if value == 'Data':
            return 'NSData'
        if value == 'Date':
            return 'NSDate'
        if value == 'Number':
            return 'NSNumber'
        if value == 'URL':
            return 'NSURL'
        if value == 'PersonNameComponents':
            return 'NSPersonNameComponents'
        if value.startswith('Array'):
            return 'NSArray'
        return value

    def ns_type_pointer(self):
        value = self.ns_type()
        if value == 'SecTrustRef':
            return value
        return f'{value} *'

    def type_check(self):
        value = self.ns_type()
        if value == 'SecTrustRef':
            return f'(m_{self.type} && CFGetTypeID((CFTypeRef)m_{self.type}.get()) == SecTrustGetTypeID())'
        return f'[m_{self.type} isKindOfClass:IPC::getClass<{value}>()]'

    def id_cast(self):
        value = self.ns_type()
        if value == 'SecTrustRef':
            return '(id)'
        return ''

    def dictionary_type(self):
        prefix = 'std::optional<' if self.value_is_optional() else ''
        suffix = '>' if self.value_is_optional() else ''
        if self.array_contents() is not None:
            return f'{prefix}Vector<RetainPtr<{self.array_contents()}>>{suffix}'
        if self.dictionary_contents() is not None:
            return f'{prefix}Vector<std::pair<String, RetainPtr<{self.dictionary_contents()}>>>{suffix}'
        return f'RetainPtr<{self.ns_type()}>'

    def value_is_optional(self):
        return self.name.endswith('?')


class EnumMember(object):
    def __init__(self, name, condition):
        self.name = name
        self.condition = condition


class ConditionalForwardDeclaration(object):
    def __init__(self, declaration, condition):
        self.declaration = declaration
        self.condition = condition

    def __lt__(self, other):
        if self.declaration != other.declaration:
            return self.declaration < other.declaration
        def condition_str(condition):
            return "" if condition is None else condition
        return condition_str(self.condition) < condition_str(other.condition)

    def __eq__(self, other):
        return other and self.declaration == other.declaration and self.condition == other.condition

    def __hash__(self):
        return hash((self.declaration, self.condition))


class ConditionalHeader(object):
    def __init__(self, header, condition, webkit_platform=False, secure_coding=False):
        self.header = header
        self.condition = condition
        self.webkit_platform = webkit_platform
        self.secure_coding = secure_coding

    def __lt__(self, other):
        if self.header != other.header:
            return self.header < other.header
        def condition_str(condition):
            return "" if condition is None else condition
        return condition_str(self.condition) < condition_str(other.condition)

    def __eq__(self, other):
        return other and self.header == other.header and self.condition == other.condition and self.webkit_platform == other.webkit_platform

    def __hash__(self):
        return hash((self.header, self.condition))


class UsingStatement(object):
    def __init__(self, name, alias_lines, condition):
        self.name = name
        self.alias_lines = alias_lines
        self.condition = condition


class ObjCWrappedType(object):
    def __init__(self, ns_type, wrapper, condition):
        self.ns_type = ns_type
        self.wrapper = wrapper
        self.condition = condition


def sanitize_string_for_variable_name(string):
    return string.replace('()', '').replace('.', '')

_license_header = """/*
 * Copyright (C) 2022-2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
"""


def one_argument_coder_declaration_cf(type):
    result = []
    result.append('')
    if type.condition is not None:
        result.append(f'#if {type.condition}')
    name_with_template = type.namespace_and_name()
    result.append(f'template<> struct ArgumentCoder<{name_with_template}> {{')
    for encoder in type.encoders:
        result.append(f'    static void encode({encoder}&, {name_with_template});')
    result.append('};')
    result.append(f'template<> struct ArgumentCoder<RetainPtr<{name_with_template}>> {{')
    for encoder in type.encoders:
        result.append(f'    static void encode({encoder}& encoder, const RetainPtr<{name_with_template}>& retainPtr)')
        result.append('    {')
        result.append(f'        ArgumentCoder<{name_with_template}>::encode(encoder, retainPtr.get());')
        result.append('    }')
    result.append(f'    static std::optional<RetainPtr<{name_with_template}>> decode(Decoder&);')
    result.append('};')
    if type.condition is not None:
        result.append('#endif')
    return result


def one_argument_coder_declaration(type, template_argument):
    if type.cf_type is not None:
        return one_argument_coder_declaration_cf(type)
    result = []
    result.append('')
    if type.condition is not None:
        result.append(f'#if {type.condition}')
    name_with_template = type.namespace_and_name()
    if template_argument is not None:
        name_with_template = f'{name_with_template}<{template_argument.namespace}::{template_argument.name}>'
    result.append(f'template<> struct ArgumentCoder<{name_with_template}> {{')
    for encoder in type.encoders:
        if type.rvalue:
            result.append(f'    static void encode({encoder}&, {name_with_template}&&);')
        else:
            result.append(f'    static void encode({encoder}&, const {name_with_template}&);')
    if type.return_ref:
        result.append(f'    static std::optional<Ref<{name_with_template}>> decode(Decoder&);')
    else:
        result.append(f'    static std::optional<{name_with_template}> decode(Decoder&);')
    result.append('};')
    if type.condition is not None:
        result.append('#endif')
    return result


def argument_coder_declarations(serialized_types, skip_nested):
    result = []
    for type in serialized_types:
        if type.nested == skip_nested:
            continue
        if type.templates:
            for template in type.templates:
                result.extend(one_argument_coder_declaration(type, template))
        else:
            result.extend(one_argument_coder_declaration(type, None))
    return result


def typenames(alias):
    return ', '.join(['typename' for x in range(alias.count(',') + 1)])


def remove_template_parameters(alias):
    match = re.search(r'(struct|class) (.*)<', alias)
    assert match
    return match.groups()[1]


def remove_alias_struct_or_class(alias):
    match = re.search(r'(struct|class) (.*)', alias)
    assert match
    return match.groups()[1].replace(',', ', ')


def alias_struct_or_class(alias):
    match = re.search(r'(struct|class) (.*)', alias)
    assert match
    return match.groups()[0]


def get_alias_namespace(alias):
    match = re.search(r'[ ,()<>]+([A-Za-z0-9_]+)::[A-Za-z0-9_]+', alias)
    if(match):
        return match.groups()[0]
    else:
        return None


def generate_forward_declarations(serialized_types, serialized_enums, additional_forward_declarations):
    result = []
    result.append('')
    serialized_enums_by_namespace = dict()
    serialized_types_by_namespace = dict()
    template_types_by_namespace = dict()
    for enum in serialized_enums:
        if enum.is_nested():
            continue
        if enum.namespace in serialized_enums_by_namespace:
            serialized_enums_by_namespace[enum.namespace] += [enum, ]
        else:
            serialized_enums_by_namespace[enum.namespace] = [enum, ]
    for type in serialized_types:
        for template in type.templates:
            if template.namespace in template_types_by_namespace:
                template_types_by_namespace[template.namespace] += [template, ]
            else:
                template_types_by_namespace[template.namespace] = [template, ]
        if type.should_skip_forward_declare():
            continue
        if type.alias is None and type.cf_type is not None:
            continue
        if type.namespace in serialized_types_by_namespace:
            serialized_types_by_namespace[type.namespace] += [type, ]
        else:
            serialized_types_by_namespace[type.namespace] = [type, ]
    all_namespaces = set(serialized_enums_by_namespace.keys())
    all_namespaces = all_namespaces.union(set(serialized_types_by_namespace.keys()))
    all_namespaces = all_namespaces.union(set(template_types_by_namespace.keys()))
    for namespace in sorted(all_namespaces, key=lambda x: (x is None, x)):
        if namespace is not None:
            result.append(f'namespace {namespace} {{')
        for enum in serialized_enums_by_namespace.get(namespace, []):
            if enum.condition is not None:
                result.append(f'#if {enum.condition}')
            result.append(f'enum class {enum.name} : {enum.underlying_type};')
            if enum.condition is not None:
                result.append('#endif')
        for type in serialized_types_by_namespace.get(namespace, []):
            if type.condition is not None:
                result.append(f'#if {type.condition}')
            if type.cf_type is None and type.alias is None:
                name = type.cpp_struct_or_class_name()
                less_than_index = name.find('<')
                if less_than_index == -1:
                    result.append(f'{type.cpp_type_from_struct_or_class()} {name};')
                else:
                    result.append(f'template<typename> {type.cpp_type_from_struct_or_class()} {name[:less_than_index]};')
            if type.condition is not None:
                result.append('#endif')
        for template in template_types_by_namespace.get(namespace, []):
            result.append(template.forward_declaration())
        if namespace is not None:
            result.append('}')
        result.append('')
    for declaration in additional_forward_declarations:
        if declaration.condition is not None:
            result.append(f'#if {declaration.condition}')
        result.append(declaration.declaration + ';')
        if declaration.condition is not None:
            result.append('#endif')
    for namespace in sorted(all_namespaces, key=lambda x: (x is None, x)):
        for type in serialized_types_by_namespace.get(namespace, []):
            if type.alias is not None:
                if type.condition is not None:
                    result.append(f'#if {type.condition}')
                if namespace is not None:
                    result.append(f'namespace {namespace} {{')

                if namespace is None or get_alias_namespace(type.alias) is None or get_alias_namespace(type.alias) == type.namespace:
                    result.append(f'template<{typenames(type.alias)}> {alias_struct_or_class(type.alias)} {remove_template_parameters(type.alias)};')
                result.append(f'using {type.name} = {remove_alias_struct_or_class(type.alias)};')

                if namespace is not None:
                    result.append('}')
                if type.condition is not None:
                    result.append('#endif')
    return result


def generate_header(serialized_types, serialized_enums, additional_forward_declarations):
    result = []
    result.append(_license_header)
    result.append('#pragma once')
    result.append('')
    for header in ['<wtf/ArgumentCoder.h>', '<wtf/OptionSet.h>', '<wtf/Ref.h>', '<wtf/RetainPtr.h>']:
        result.append(f'#include {header}')

    result += generate_forward_declarations(serialized_types, serialized_enums, additional_forward_declarations)
    result.append('')
    result.append('namespace IPC {')
    result.append('')
    result.append('class Decoder;')
    result.append('class Encoder;')
    result.append('class StreamConnectionEncoder;')
    result = result + argument_coder_declarations(serialized_types, True)
    result.append('')
    result.append('} // namespace IPC\n')
    result.append('')
    result.append('namespace WTF {')
    result.append('')
    for enum in serialized_enums:
        if enum.is_nested():
            continue
        if enum.underlying_type == 'bool':
            continue
        if enum.condition is not None:
            result.append(f'#if {enum.condition}')
        result.append(f'template<> bool {enum.function_name()}<{enum.namespace_and_name()}>({enum.parameter()});')
        if enum.condition is not None:
            result.append('#endif')
    result.append('')
    result.append('} // namespace WTF')
    result.append('')
    return '\n'.join(result)


def resolve_inheritance(serialized_types):
    result = []
    for serialized_type in serialized_types:
        if serialized_type.parent_class_name is not None:
            for possible_parent in serialized_types:
                if possible_parent.namespace_and_name() == serialized_type.parent_class_name:
                    serialized_type.parent_class = possible_parent
                    break
        result.append(serialized_type)
    return result


def check_type_members(type, checking_parent_class):
    result = []
    if type.parent_class is not None:
        result = check_type_members(type.parent_class, True)
    for member in type.members:
        if member.condition is not None:
            result.append(f'#if {member.condition}')
        result.append(f'    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.{member.name})>, {member.type}>);')
        if member.condition is not None:
            result.append('#endif')
    for member in type.dictionary_members:
        result.append(f'    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.m_{member.type})>, {member.dictionary_type()}>);')
    if type.can_assert_member_order_is_correct():
        # FIXME: Add this check for types with parent classes, too.
        if type.parent_class is None and not checking_parent_class:
            result.append(f'    struct ShouldBeSameSizeAs{type.name_as_identifier()} : public VirtualTableAndRefCountOverhead<std::is_polymorphic_v<{type.namespace_and_name()}>, {"true" if type.return_ref else "false"}> {{')
            for member in type.members:
                if member.condition is not None:
                    result.append(f'#if {member.condition}')
                result.append(f'        {member.type} {member.name}{" : 1" if "BitField" in member.attributes else ""};')
                if member.condition is not None:
                    result.append('#endif')
            for member in type.dictionary_members:
                result.append(f'        {member.dictionary_type()} {member.type};')
            result.append('    };')
            result.append(f'    static_assert(sizeof(ShouldBeSameSizeAs{type.name_as_identifier()}) == sizeof({type.namespace_and_name()}));')
        result.append('    static_assert(MembersInCorrectOrder < 0')
        for member in type.members:
            if 'BitField' in member.attributes:
                continue
            if member.condition is not None:
                result.append(f'#if {member.condition}')
            result.append(f'        , offsetof({type.namespace_and_name()}, {member.name})')
            if member.condition is not None:
                result.append('#endif')
        for member in type.dictionary_members:
            result.append(f'        , offsetof({type.namespace_and_name()}, m_{member.type})')
        result.append(f'    >::value);')
    if type.has_optional_tuple_bits():
        serialized_members = type.serialized_members()
        optional_tuple_state = None
        for i in range(len(serialized_members)):
            member = serialized_members[i]
            if member.optional_tuple_bits():
                result.append(f'    static_assert(static_cast<uint64_t>({serialized_members[i + 1].optional_tuple_bit()}) == 1);')
                result.append('    static_assert(BitsInIncreasingOrder<')
                optional_tuple_state = 'begin'
            elif member.optional_tuple_bit():
                if member.condition is not None:
                    result.append(f'#if {member.condition}')
                result.append(f'        {", " if optional_tuple_state == "middle" else ""}static_cast<uint64_t>({member.optional_tuple_bit()})')
                if member.condition is not None:
                    result.append('#endif')
                optional_tuple_state = 'middle'
            elif optional_tuple_state == 'middle':
                result.append('    >::value);')
                optional_tuple_state = None
        if optional_tuple_state == 'middle':
            result.append('    >::value);')
            optional_tuple_state = None
    result.append('')
    return result


def encode_cf_type(type):
    result = []
    if type.from_cf_method is not None:
        result.append(f'    encoder << {type.from_cf_method}(instance);')
    else:
        result.append(f'    encoder << {type.cf_wrapper_type()} {{ instance }};')
    return result


def encode_type(type):
    if type.cf_type is not None:
        return encode_cf_type(type)
    result = []
    if type.parent_class is not None:
        result = result + encode_type(type.parent_class)
    for member in type.serialized_members():
        if member.condition is not None:
            result.append(f'#if {member.condition}')
        if member.is_subclass:
            result.append(f'    if (auto* subclass = dynamicDowncast<{member.namespace}::{member.name}>(instance)) {{')
            result.append(f'        encoder << {type.subclass_enum_name()}::{member.name};')
            if type.rvalue:
                result.append('        encoder << WTFMove(*subclass);')
            else:
                result.append('        encoder << *subclass;')
            result.append('        return;')
            result.append('    }')
        elif member.optional_tuple_bits():
            result.append(f'    encoder << instance.{member.name};')
            bits_variable_name = member.name
        elif member.optional_tuple_bit() is not None:
            result.append(f'    if (instance.{bits_variable_name} & {member.optional_tuple_bit()})')
            result.append(f'        encoder << instance.{member.name};')
        else:
            if type.rvalue and '()' not in member.name:
                if 'EncodeRequestBody' in member.attributes:
                    result.append(f'    RefPtr {member.name}Body = instance.{member.name}.httpBody();')
                result.append(f'    encoder << WTFMove(instance.{member.name});')
                if 'EncodeRequestBody' in member.attributes:
                    result.append(f'    encoder << IPC::FormDataReference {{ WTFMove({member.name}Body) }};')
            else:
                result.append(f'    encoder << instance.{member.name};')
                if 'EncodeRequestBody' in member.attributes:
                    result.append(f'    encoder << IPC::FormDataReference {{ instance.{member.name}.httpBody() }};')
        if member.condition is not None:
            result.append('#endif')
    for member in type.dictionary_members:
        result.append(f'    encoder << instance.m_{member.type};')

    return result


def decode_cf_type(type):
    result = []
    result.append(f'    auto result = decoder.decode<{type.cf_wrapper_type()}>();')
    result.append('    if (UNLIKELY(!decoder.isValid()))')
    result.append('        return std::nullopt;')
    if type.to_cf_method is not None:
        result.append(f'    return {type.to_cf_method};')
    else:
        result.append('    return result->toCF();')
    return result

def decode_type(type):
    if type.cf_type is not None:
        return decode_cf_type(type)

    result = []
    if type.parent_class is not None:
        result = result + decode_type(type.parent_class)

    if type.members_are_subclasses:
        result.append(f'    auto type = decoder.decode<{type.subclass_enum_name()}>();')
        result.append('    UNUSED_PARAM(type);')
        result.append('    if (UNLIKELY(!decoder.isValid()))')
        result.append('        return std::nullopt;')
        result.append('')

    if type.has_optional_tuple_bits() and type.populate_from_empty_constructor:
        result.append(f'    {type.namespace_and_name()} result;')

    for i in range(len(type.serialized_members())):
        member = type.serialized_members()[i]
        if member.condition is not None:
            result.append(f'#if {member.condition}')
        sanitized_variable_name = sanitize_string_for_variable_name(member.name)
        r = re.compile(r'SecureCodingAllowed=\[(.*)\]')
        decodable_classes = [r.match(m).groups()[0] for m in list(filter(r.match, member.attributes))]
        if len(decodable_classes) == 1:
            match = re.search("RetainPtr<(.*)>", member.type)
            assert match
            for attribute in member.attributes:
                precondition = re.search(r'Precondition=\'(.*)\'', attribute)
                if precondition:
                    condition, = precondition.groups()
                    result.append(f'    if (!({condition}))')
                    result.append('        return std::nullopt;')
                    break
                else:
                    condition = re.search(r'Precondition', attribute)
                    assert not condition
            result.append(f'    auto {sanitized_variable_name} = decoder.decodeWithAllowedClasses<{match.groups()[0]}>({{ {decodable_classes[0]} }});')
        elif member.is_subclass:
            result.append(f'    if (type == {type.subclass_enum_name()}::{member.name}) {{')
            typename = f'{member.namespace}::{member.name}'
            result.append(f'        auto result = decoder.decode<Ref<{typename}>>();')
            result.append('        if (UNLIKELY(!decoder.isValid()))')
            result.append('            return std::nullopt;')
            result.append('        return WTFMove(*result);')
            result.append('    }')
        elif member.optional_tuple_bits():
            bits_name = sanitized_variable_name
            result.append(f'    auto {bits_name} = decoder.decode<{member.type}>();')
            result.append(f'    if (!{bits_name})')
            result.append('        return std::nullopt;')
            if type.populate_from_empty_constructor:
                result.append(f'    result.{member.name} = *{bits_name};')
        elif member.optional_tuple_bit() is not None:
            if type.populate_from_empty_constructor:
                result.append(f'    if (*{bits_name} & {member.optional_tuple_bit()}) {{')
                result.append(f'        if (auto deserialized = decoder.decode<{member.type}>())')
                result.append(f'            result.{sanitized_variable_name} = WTFMove(*deserialized);')
                result.append('        else')
                result.append('            return std::nullopt;')
                result.append('    }')
            else:
                result.append('')
                result.append(f'    {member.type} {sanitized_variable_name} {{ }};')
                result.append(f'    if (*{bits_name} & {member.optional_tuple_bit()}) {{')
                result.append(f'        if (auto deserialized = decoder.decode<{member.type}>())')
                result.append(f'            {sanitized_variable_name} = WTFMove(*deserialized);')
                result.append('        else')
                result.append('            return std::nullopt;')
                result.append('    }')
        else:
            assert len(decodable_classes) == 0
            result.append(f'    auto {sanitized_variable_name} = decoder.decode<{member.type}>();')
            if 'EncodeRequestBody' in member.attributes:
                result.append(f'    if ({sanitized_variable_name}) {{')
                result.append(f'        if (auto {sanitized_variable_name}Body = decoder.decode<IPC::FormDataReference>())')
                result.append(f'            {sanitized_variable_name}->setHTTPBody({sanitized_variable_name}Body->takeData());')
                result.append('    }')
            if type.debug_decoding_failure:
                result.append(f'    if (UNLIKELY(!{sanitized_variable_name}))')
                result.append(f'        decoder.setIndexOfDecodingFailure({str(i)});')
        for attribute in member.attributes:
            match = re.search(r'Validator=\'(.*)\'', attribute)
            if match:
                validator, = match.groups()
                result.append('    if (UNLIKELY(!decoder.isValid()))')
                result.append('        return std::nullopt;')
                result.append('')
                result.append(f'    if (!({validator}))')
                result.append('        return std::nullopt;')
                continue
            else:
                match = re.search(r'Validator', attribute)
                assert not match
        if member.condition is not None:
            result.append('#endif')

    for member in type.dictionary_members:
        result.append(f'    auto {member.type} = decoder.decode<{member.dictionary_type()}>();')
        result.append(f'    if (!{member.type})')
        result.append('        return std::nullopt;')
        # FIXME: Add question marks to the serialization.in files and add these checks here:
        # if not member.value_is_optional() and member.array_contents() is None and member.dictionary_contents() is None:
        #    result.append('    if (!*{member.type})')
        #    result.append('        return std::nullopt;')
        result.append('')

    return result


def indent(indentation):
    return '    ' * indentation


def construct_type(type, specialization, indentation):
    result = []
    fulltype = type.namespace_and_name_for_construction(specialization)
    if type.create_using:
        result.append(f'{indent(indentation)}{fulltype}::{type.create_using}(')
    elif type.return_ref:
        result.append(f'{indent(indentation)}{fulltype}::create(')
    else:
        result.append(f'{indent(indentation)}{fulltype} {{')
    if type.parent_class is not None:
        result = result + construct_type(type.parent_class, specialization, indentation + 1)
        if len(type.members) != 0:
            result[-1] += ','
    serialized_members = type.serialized_members()
    for i in range(len(serialized_members)):
        member = serialized_members[i]
        if member.condition is not None:
            result.append(f'#if {member.condition}')
        result.append(f'{indent(indentation + 1)}WTFMove({"" if member.optional_tuple_bit() else "*"}{sanitize_string_for_variable_name(member.name)}){"" if i == len(serialized_members) - 1 else ","}')
        if member.condition is not None:
            result.append('#endif')
    for i in range(len(type.dictionary_members)):
        member = type.dictionary_members[i]
        result.append(f'{indent(indentation + 1)}WTFMove(*{member.type}){"," if i < len(type.dictionary_members) - 1 else ""}')
    if type.create_using or type.return_ref:
        result.append(indent(indentation) + ')')
    else:
        result.append(indent(indentation) + '}')
    return result


def generate_one_impl(type, template_argument):
    result = []
    name_with_template = type.namespace_and_name()
    if template_argument is not None:
        name_with_template = f'{name_with_template}<{template_argument.namespace}::{template_argument.name}>'
    if type.condition is not None:
        result.append(f'#if {type.condition}')

    if type.members_are_subclasses:
        result.append(f'enum class {type.subclass_enum_name()} : IPC::EncodedVariantIndex {{')
        for idx in range(0, len(type.members)):
            member = type.members[idx]
            if member.condition is not None:
                result.append(f'#if {member.condition}')
            if idx == 0:
                result.append(f'    {member.name}')
            else:
                result.append(f'    , {member.name}')
            if member.condition is not None:
                result.append('#endif')
        result.append('};')
        result.append('')
    for encoder in type.encoders:
        if type.custom_encoder:
            continue
        if type.members_are_subclasses:
            result.append('IGNORE_WARNINGS_BEGIN("missing-noreturn")')
        instanceArgName = 'instance' if type.generic_wrapper is None else 'passedInstance'
        if type.cf_type is not None:
            result.append(f'void ArgumentCoder<{name_with_template}>::encode({encoder}& encoder, {name_with_template} {instanceArgName})')
        elif type.rvalue:
            result.append(f'void ArgumentCoder<{name_with_template}>::encode({encoder}& encoder, {name_with_template}&& {instanceArgName})')
        else:
            result.append(f'void ArgumentCoder<{name_with_template}>::encode({encoder}& encoder, const {name_with_template}& {instanceArgName})')
        result.append('{')
        if type.generic_wrapper is not None:
            if type.rvalue:
                result.append(f'    auto instance = {type.generic_wrapper}(WTFMove({instanceArgName}));')
            else:
                result.append(f'    auto instance = {type.generic_wrapper}({instanceArgName});')
        if not type.members_are_subclasses and type.cf_type is None:
            result = result + check_type_members(type, False)
        result = result + encode_type(type)
        if type.members_are_subclasses:
            result.append('    ASSERT_NOT_REACHED();')
        result.append('}')
        if type.members_are_subclasses:
            result.append('IGNORE_WARNINGS_END')
        result.append('')
    if type.cf_type is not None:
        result.append(f'std::optional<RetainPtr<{name_with_template}>> ArgumentCoder<RetainPtr<{name_with_template}>>::decode(Decoder& decoder)')
    elif type.return_ref:
        result.append(f'std::optional<Ref<{name_with_template}>> ArgumentCoder<{name_with_template}>::decode(Decoder& decoder)')
    else:
        result.append(f'std::optional<{name_with_template}> ArgumentCoder<{name_with_template}>::decode(Decoder& decoder)')
    result.append('{')
    result = result + decode_type(type)
    if type.cf_type is None:
        if not type.members_are_subclasses:
            result.append('    if (UNLIKELY(!decoder.isValid()))')
            result.append('        return std::nullopt;')
            if type.populate_from_empty_constructor and not type.has_optional_tuple_bits():
                result.append(f'    {name_with_template} result;')
                for member in type.serialized_members():
                    if member.condition is not None:
                        result.append(f'#if {member.condition}')
                    result.append(f'    result.{member.name} = WTFMove(*{member.name});')
                    if member.condition is not None:
                        result.append('#endif')
                result.append('    return { WTFMove(result) };')
            elif type.has_optional_tuple_bits() and type.populate_from_empty_constructor:
                result.append('    return { WTFMove(result) };')
            else:
                result.append('    return {')
                if template_argument:
                    result = result + construct_type(type, template_argument.specialization(), 2)
                else:
                    result = result + construct_type(type, None, 2)
                result.append('    };')
        else:
            result.append('    ASSERT_NOT_REACHED();')
            result.append('    return std::nullopt;')
    result.append('}')
    result.append('')
    if type.condition is not None:
        result.append('#endif')
        result.append('')
    return result


def generate_impl(serialized_types, serialized_enums, headers, generating_webkit_platform_impl, objc_wrapped_types):
    result = []
    result.append(_license_header)
    result.append('#include "config.h"')
    result.append('#include "GeneratedSerializers.h"')
    result.append('#include "GeneratedWebKitSecureCoding.h"')
    result.append('')
    for header in headers:
        if header.webkit_platform != generating_webkit_platform_impl:
            continue
        if header.condition is not None:
            result.append(f'#if {header.condition}')
        result.append(f'#include {header.header}')
        if header.condition is not None:
            result.append('#endif')
    result.append('')
    result.append('template<size_t...> struct MembersInCorrectOrder;')
    result.append('template<size_t onlyOffset> struct MembersInCorrectOrder<onlyOffset> {')
    result.append('    static constexpr bool value = true;')
    result.append('};')
    result.append('template<size_t firstOffset, size_t secondOffset, size_t... remainingOffsets> struct MembersInCorrectOrder<firstOffset, secondOffset, remainingOffsets...> {')
    result.append('    static constexpr bool value = firstOffset > secondOffset ? false : MembersInCorrectOrder<secondOffset, remainingOffsets...>::value;')
    result.append('};')
    result.append('')
    result.append('template<uint64_t...> struct BitsInIncreasingOrder;')
    result.append('template<uint64_t onlyBit> struct BitsInIncreasingOrder<onlyBit> {')
    result.append('    static constexpr bool value = true;')
    result.append('};')
    result.append('template<uint64_t firstBit, uint64_t secondBit, uint64_t... remainingBits> struct BitsInIncreasingOrder<firstBit, secondBit, remainingBits...> {')
    result.append('    static constexpr bool value = firstBit == secondBit >> 1 && BitsInIncreasingOrder<secondBit, remainingBits...>::value;')
    result.append('};')
    result.append('')
    result.append('template<bool, bool> struct VirtualTableAndRefCountOverhead;')
    result.append('template<> struct VirtualTableAndRefCountOverhead<true, true> : public RefCounted<VirtualTableAndRefCountOverhead<true, true>> {')
    result.append('    virtual ~VirtualTableAndRefCountOverhead() { }')
    result.append('};')
    result.append('template<> struct VirtualTableAndRefCountOverhead<false, true> : public RefCounted<VirtualTableAndRefCountOverhead<false, true>> { };')
    result.append('template<> struct VirtualTableAndRefCountOverhead<true, false> {')
    result.append('    virtual ~VirtualTableAndRefCountOverhead() { }')
    result.append('};')
    result.append('template<> struct VirtualTableAndRefCountOverhead<false, false> { };')
    result.append('')
    # GCC and Clang>=18 are less generous with their interpretation of "Use of the offsetof macro
    # with a type other than a standard-layout class is conditionally-supported".
    result.append('IGNORE_WARNINGS_BEGIN("invalid-offsetof")')
    result.append('')
    result.append('namespace IPC {')
    result.append('')

    for type in objc_wrapped_types:
        if type.condition is not None:
            result.append(f'#if {type.condition}')
        result.append(f'template<> void encodeObjectDirectly<{type.ns_type}>(IPC::Encoder& encoder, {type.ns_type} *instance)')
        result.append('{')
        result.append(f'    encoder << (instance ? std::optional(WebKit::{type.wrapper}(instance)) : std::nullopt);')
        result.append('}')
        result.append('')
        result.append(f'template<> std::optional<RetainPtr<id>> decodeObjectDirectlyRequiringAllowedClasses<{type.ns_type}>(IPC::Decoder& decoder)')
        result.append('{')
        result.append(f'    auto result = decoder.decode<std::optional<WebKit::{type.wrapper}>>();')
        result.append('    if (!result)')
        result.append('        return std::nullopt;')
        result.append('    return *result ? (*result)->toID() : nullptr;')
        result.append('}')
        if type.condition is not None:
            result.append(f'#endif // {type.condition}')
        result.append('')

    if not generating_webkit_platform_impl:
        result = result + argument_coder_declarations(serialized_types, False)
        result.append('')
    for type in serialized_types:
        if type.webkit_platform != generating_webkit_platform_impl:
            continue
        if type.templates:
            for template in type.templates:
                result.extend(generate_one_impl(type, template))
        else:
            result.extend(generate_one_impl(type, None))
    result.append('} // namespace IPC')
    result.append('')
    result.append('namespace WTF {')
    for type in serialized_types:
        if generating_webkit_platform_impl:
            continue
        if not type.members_are_subclasses:
            continue
        result.append('')
        if type.condition is not None:
            result.append(f'#if {type.condition}')
        result.append(f'template<> bool {type.function_name_for_enum()}<IPC::{type.subclass_enum_name()}>(IPC::EncodedVariantIndex value)')
        result.append('{')
        result.append('IGNORE_WARNINGS_BEGIN("switch-unreachable")')
        result.append(f'    switch (static_cast<IPC::{type.subclass_enum_name()}>(value)) {{')
        for member in type.members:
            if member.condition is not None:
                result.append(f'#if {member.condition}')
            result.append(f'    case IPC::{type.subclass_enum_name()}::{member.name}:')
            if member.condition is not None:
                result.append('#endif')
        result.append('        return true;')
        result.append('    }')
        result.append('IGNORE_WARNINGS_END')
        result.append('    return false;')
        result.append('}')
        if type.condition is not None:
            result.append('#endif')

    for enum in serialized_enums:
        if enum.is_webkit_platform() != generating_webkit_platform_impl:
            continue
        result.append('')
        if enum.condition is not None:
            result.append(f'#if {enum.condition}')
        result.append(f'template<> bool {enum.function_name()}<{enum.namespace_and_name()}>({enum.parameter()} value)')
        result.append('{')
        if enum.is_option_set():
            result.append(f'    constexpr {enum.underlying_type} allValidBitsValue = 0')
            for i in range(0, len(enum.valid_values)):
                valid_value = enum.valid_values[i]
                if valid_value.condition is not None:
                    result.append(f'#if {valid_value.condition}')
                result.append(f'        | static_cast<{enum.underlying_type}>({enum.namespace_and_name()}::{valid_value.name})')
                if valid_value.condition is not None:
                    result.append('#endif')
            result.append('        | 0;')
            result.append('    return (value.toRaw() | allValidBitsValue) == allValidBitsValue;')
        else:
            if enum.underlying_type == 'bool':
                result.append('    switch (static_cast<uint8_t>(value)) {')
                result.append('    case 0:')
                result.append('    case 1:')
            else:
                result.append(f'    switch (static_cast<{enum.namespace_and_name()}>(value)) {{')
                for valid_value in enum.valid_values:
                    if valid_value.condition is not None:
                        result.append(f'#if {valid_value.condition}')
                    result.append(f'    case {enum.namespace_and_name()}::{valid_value.name}:')
                    if valid_value.condition is not None:
                        result.append('#endif')
            result.append('        return true;')
            result.append('    default:')
            result.append('        return false;')
            result.append('    }')
        result.append('}')
        if enum.condition is not None:
            result.append('#endif')
    result.append('')
    result.append('} // namespace WTF')
    result.append('')
    result.append('IGNORE_WARNINGS_END')
    result.append('')
    return '\n'.join(result)


def generate_optional_tuple_type_info(type):
    result = ['                "OptionalTuple<"']
    serialized_members = type.serialized_members()
    found_first_optional_tuple_bit_member = False
    for i in range(len(serialized_members)):
        member = serialized_members[i]
        if member.optional_tuple_bit():
            if member.condition is not None:
                result.append(f'#if {member.condition}')
            result.append(f'                    "{", " if found_first_optional_tuple_bit_member else ""}{member.name}"')
            found_first_optional_tuple_bit_member = True
            if member.condition is not None:
                result.append('#endif')
    result.append('                ">"_s')
    return result


def generate_one_serialized_type_info(type):
    result = []
    if type.condition is not None:
        result.append(f'#if {type.condition}')
    result.append(f'        {{ "{type.name_declaration_for_serialized_type_info()}"_s, {{')
    if type.members_are_subclasses:
        result.append('            { "std::variant<"')
        for i in range(len(type.members)):
            member = type.members[i]
            if member.condition is not None:
                result.append(f'#if {member.condition}')
            result.append(f'                "{"" if i == 0 else ", "}{member.namespace}::{member.name}"')
            if member.condition is not None:
                result.append('#endif')
        result.append('            ">"_s, "subclasses"_s }')
        result.append('        } },')
        if type.condition is not None:
            result.append(f'#endif // {type.condition}')
        return result

    if type.cf_type is not None:
        result.append(f'            {{ "{type.namespace_if_not_wtf_and_name()}"_s, "wrapper"_s }}')
        result.append('        } },')
        if type.condition is not None:
            result.append(f'#endif // {type.condition}')
        return result

    if type.is_webkit_secure_coding_type():
        for member in type.dictionary_members:
            if member.condition is not None:
                result.append(f'#if {member.condition}')
            result.append(f'            {{ "{member.dictionary_type()}"_s , "{member.type}"_s }},')
            if member.condition is not None:
                result.append(f'#endif // {member.condition}')
        result.append('        } },')
        result.append(f'        {{ "{type.name}"_s, {{')
        result.append(f'            {{ "{type.namespace_if_not_wtf_and_name()}"_s, "wrapper"_s }}')
        result.append('        } },')
        if type.condition is not None:
            result.append(f'#endif // {type.condition}')
        return result

    serialized_members = type.members_for_serialized_type_info()
    parent_class = type.parent_class
    while parent_class is not None:
        serialized_members = parent_class.members_for_serialized_type_info() + serialized_members
        parent_class = parent_class.parent_class

    optional_tuple_state = None
    for member in serialized_members:
        if member.condition is not None:
            result.append(f'#if {member.condition}')
        if member.optional_tuple_bits():
            result.append('            {')
            result.append('                "OptionalTuple<"')
            optional_tuple_state = 'begin'
        elif member.optional_tuple_bit():
            result.append(f'                    "{"" if optional_tuple_state == "begin" else ", "}{member.type}"')
            optional_tuple_state = 'middle'
        else:
            if optional_tuple_state == 'middle':
                result.append('                ">"_s,')
                result = result + generate_optional_tuple_type_info(type)
                result.append('            },')
                optional_tuple_state = None
            result.append('            {')
            result.append(f'                "{member.type}"_s,')
            result.append(f'                "{member.name}"_s')
            result.append('            },')
            optional_tuple_state = None
        if 'EncodeRequestBody' in member.attributes:
            result.append('            {')
            result.append('                "IPC::FormDataReference"_s,')
            result.append('                "requestBody"_s')
            result.append('            },')
        if member.condition is not None:
            result.append('#endif')
    if optional_tuple_state == 'middle':
        result.append('                ">"_s,')
        result = result + generate_optional_tuple_type_info(type)
        result.append('            },')
    result.append('        } },')
    if type.condition is not None:
        result.append(f'#endif // {type.condition}')
    return result


def output_sorted_headers(sorted_headers):
    result = []
    for header in sorted_headers:
        if header.condition is not None:
            result.append(f'#if {header.condition}')
        result.append(f'#include {header.header}')
        if header.condition is not None:
            result.append('#endif')
    return result


def generate_serialized_type_info(serialized_types, serialized_enums, headers, using_statements, objc_wrapped_types):
    result = []
    result.append(_license_header)
    result.append('#include "config.h"')
    result.append('#include "SerializedTypeInfo.h"')
    result.append('')
    header_set = set()
    for header in headers:
        header_set.add(header)
    header_set.add(ConditionalHeader('"GeneratedWebKitSecureCoding.h"', None))
    result.extend(output_sorted_headers(sorted(header_set)))

    result.append('')
    for using_statement in using_statements:
        if using_statement.condition is not None:
            result.append(f'#if {using_statement.condition}')
        result.append(f'static_assert(std::is_same_v<{using_statement.name},')
        for alias_line in using_statement.alias_lines:
            if '#' in alias_line:
                result.append(f'{alias_line.strip()}')
            else:
                result.append(f'    {alias_line}')
        result.append('>);')
        if using_statement.condition is not None:
            result.append('#endif')

    result.append('')
    result.append('#if ENABLE(IPC_TESTING_API)')
    result.append('')
    result.append('namespace WebKit {')
    result.append('')
    result.append('template<typename E> uint64_t enumValueForIPCTestAPI(E e)')
    result.append('{')
    result.append('    return static_cast<std::make_unsigned_t<std::underlying_type_t<E>>>(e);')
    result.append('}')
    result.append('')
    result.append('Vector<SerializedTypeInfo> allSerializedTypes()')
    result.append('{')
    result.append('    return {')
    for type in serialized_types:
        result.extend(generate_one_serialized_type_info(type))

    for type in objc_wrapped_types:
        if type.condition is not None:
            result.append(f'#if {type.condition}')
        result.append(f'        {{ "{type.ns_type}"_s, {{')
        result.append(f'            {{ "WebKit::{type.wrapper}"_s, "wrapper"_s }}')
        result.append('        } },')
        if type.condition is not None:
            result.append(f'#endif // {type.condition}')
    for using_statement in using_statements:
        if using_statement.condition is not None:
            result.append(f'#if {using_statement.condition}')
        result.append(f'        {{ "{using_statement.name}"_s, {{')
        result.append(f'        {{')
        for line_number in range(len(using_statement.alias_lines)):
            alias_line = using_statement.alias_lines[line_number]
            if '#' in alias_line:
                result.append(f'{alias_line.strip()}')
            else:
                underscore_s_after_last_line = '_s' if line_number is len(using_statement.alias_lines) - 1 else ''
                extra_space_after_comma = ' ' if alias_line.endswith(',') else ''
                result.append(f'            "{alias_line.strip()}{extra_space_after_comma}"{underscore_s_after_last_line}')
        result.append(f'            , "alias"_s }}')
        result.append('        } },')
        if using_statement.condition is not None:
            result.append('#endif')
    result.append('    };')
    result.append('}')
    result.append('')
    result.append('Vector<SerializedEnumInfo> allSerializedEnums()')
    result.append('{')
    result.append('    return {')
    for enum in serialized_enums:
        if enum.condition is not None:
            result.append(f'#if {enum.condition}')
        result.append(f'        {{ "{enum.namespace_and_name()}"_s, sizeof({enum.namespace_and_name()}), {"true" if enum.is_option_set() else "false"}, {{')
        if enum.underlying_type == 'bool':
            result.append('            0, 1')
        else:
            for valid_value in enum.valid_values:
                if valid_value.condition is not None:
                    result.append(f'#if {valid_value.condition}')
                result.append(f'            enumValueForIPCTestAPI({enum.namespace_and_name()}::{valid_value.name}),')
                if valid_value.condition is not None:
                    result.append('#endif')
        result.append('        } },')
        if enum.condition is not None:
            result.append('#endif')
    result.append('    };')
    result.append('}')
    result.append('')
    result.append('} // namespace WebKit')
    result.append('')
    result.append('#endif // ENABLE(IPC_TESTING_API)')
    result.append('')
    return '\n'.join(result)


class ConditionStackEntry(object):
    def __init__(self, expression):
        self._base_expression = expression
        self.should_negate = False

    @property
    def expression(self):
        return self._base_expression if not self.should_negate else f'!({self._base_expression})'


def generate_condition_expression(condition_stack):
    if not condition_stack:
        return None

    full_condition_expression = condition_stack[0].expression
    if len(condition_stack) == 1:
        return full_condition_expression

    for condition in condition_stack[1:]:
        condition_expression = condition.expression
        full_condition_expression = f'({full_condition_expression}) && ({condition_expression})'

    return full_condition_expression


def parse_serialized_types(file):
    serialized_types = []
    serialized_enums = []
    using_statements = []
    objc_wrapped_types = []
    additional_forward_declarations = []
    headers = []

    attributes = None
    namespace = None
    name = None
    members = []
    dictionary_members = []
    type_condition = None
    member_condition = None
    type_condition_stack = []
    member_condition_stack = []
    struct_or_class = None
    cf_type = None
    underlying_type = None
    parent_class_name = None
    metadata = None
    templates = []

    file_lines = []
    for line in file:
        file_lines.append(line.strip())

    for line_number in range(len(file_lines)):
        line = file_lines[line_number]
        if line.startswith('#'):
            if line == '#else':
                if name is None:
                    if type_condition_stack:
                        type_condition_stack[-1].should_negate = True
                else:
                    if member_condition_stack:
                        member_condition_stack[-1].should_negate = True
            elif line.startswith('#if '):
                condition_expression = line[4:]
                if name is None:
                    type_condition_stack.append(ConditionStackEntry(expression=condition_expression))
                else:
                    member_condition_stack.append(ConditionStackEntry(expression=condition_expression))
            elif line.startswith('#endif'):
                if name is None:
                    if type_condition_stack:
                        type_condition_stack.pop()
                else:
                    if member_condition_stack:
                        member_condition_stack.pop()
            type_condition = generate_condition_expression(type_condition_stack)
            member_condition = generate_condition_expression(member_condition_stack)
            continue
        if line.startswith('}'):
            if underlying_type is not None:
                serialized_enums.append(SerializedEnum(namespace, name, underlying_type, members, type_condition, attributes))
            else:
                type = SerializedType(struct_or_class, cf_type, namespace, name, parent_class_name, members, dictionary_members, type_condition, attributes, templates, metadata)
                serialized_types.append(type)
                if namespace is not None and (attributes is None or ('CustomHeader' not in attributes and 'Nested' not in attributes)):
                    if namespace == 'WebKit' or namespace == 'WebKit::WebPushD':
                        headers.append(ConditionalHeader(f'"{name}.h"', type_condition))
                    elif namespace == 'WTF':
                        headers.append(ConditionalHeader(f'<wtf/{name}.h>', type_condition))
                    elif namespace == 'WebKit::WebGPU':
                        headers.append(ConditionalHeader(f'"WebGPU{name}.h"', type_condition))
                    else:
                        headers.append(ConditionalHeader(f'<{namespace}/{name}.h>', type_condition))
            attributes = None
            namespace = None
            name = None
            members = []
            dictionary_members = []
            member_condition = None
            struct_or_class = None
            cf_type = None
            underlying_type = None
            parent_class_name = None
            metadata = None
            templates = []
            continue

        match = re.search(r'^headers?: (.*)', line)
        if match:
            for header in match.group(1).split():
                headers.append(ConditionalHeader(header, type_condition))
            continue
        match = re.search(r'^template: (enum class) (.*)::(.*) : (.*)', line)
        if match:
            template_type, template_namespace, template_name, enum_storage = match.groups()
            templates.append(Template(template_type, template_namespace, template_name, enum_storage))
            continue
        match = re.search(r'^template: (struct|class|enum class) (.*)::(.*)', line)
        if match:
            template_type, template_namespace, template_name = match.groups()
            templates.append(Template(template_type, template_namespace, template_name, None))
            continue
        match = re.search(r'webkit_platform_headers?: (.*)', line)
        if match:
            for header in match.group(1).split():
                headers.append(ConditionalHeader(header, type_condition, True))
            continue
        match = re.search(r'secure_coding_headers?: (.*)', line)
        if match:
            for header in match.group(1).split():
                headers.append(ConditionalHeader(header, type_condition, False, True))
            continue


        match = re.search(r'(.*)enum class (.*)::(.*) : (.*) {', line)
        if match:
            attributes, namespace, name, underlying_type = match.groups()
            assert underlying_type != 'bool'
            continue
        match = re.search(r'(.*)enum class (.*)::(.*) : bool', line)
        if match:
            serialized_enums.append(SerializedEnum(match.groups()[1], match.groups()[2], 'bool', [], type_condition, match.groups()[0]))
            continue
        match = re.search(r'(.*)enum class (.*) : (.*) {', line)
        if match:
            attributes, name, underlying_type = match.groups()
            assert underlying_type != 'bool'
            continue

        match = re.search(r'\[(.*)\] (struct|class|alias) (.*)::(.*) : (.*) {', line)
        if match:
            attributes, struct_or_class, namespace, name, parent_class_name = match.groups()
            continue
        match = re.search(r'(struct|class|alias) (.*)::(.*) : (.*) {', line)
        if match:
            struct_or_class, namespace, name, parent_class_name = match.groups()
            continue
        match = re.search(r'\[(.*)\] (struct|class|alias) (.*)::((?:.*)<(?:.*)>) {', line)
        if match:
            attributes, struct_or_class, namespace, name = match.groups()
            continue
        match = re.search(r'\[(.*)\] (struct|class|alias) (.*)::([^\s]*) {', line)
        if match:
            attributes, struct_or_class, namespace, name = match.groups()
            continue
        match = re.search(r'\[(.*)\] (struct|class) (.*)::(.*)\s+(.*) {', line)
        if match:
            attributes, struct_or_class, namespace, name, metadata = match.groups()
            continue
        match = re.search(r'(struct|class|alias) (.*)::(.*) {', line)
        if match:
            struct_or_class, namespace, name = match.groups()
            continue
        match = re.search(r'\[(.*)\] (struct|class|alias|webkit_secure_coding) (.*) {', line)
        if match:
            attributes, struct_or_class, name = match.groups()
            continue
        match = re.search(r'(struct|class|alias|webkit_secure_coding) (.*) {', line)
        if match:
            struct_or_class, name = match.groups()
            continue
        match = re.search(r'\[(.*)\] (.*)Ref wrapped by (.*)::(.*) {', line)
        if match:
            attributes, cf_type, namespace, name = match.groups()
            continue
        match = re.search(r'(.*)Ref wrapped by (.*)::(.*) {', line)
        if match:
            cf_type, namespace, name = match.groups()
            continue
        match = re.search(r'(.*) wrapped by (.*)', line)
        if match:
            objc_wrapped_type, objc_wrapper = match.groups()
            objc_wrapped_types.append(ObjCWrappedType(objc_wrapped_type, objc_wrapper, type_condition))
            continue
        match = re.search(r'additional_forward_declaration: (.*)', line)
        if match:
            declaration = match.groups()[0]
            additional_forward_declarations.append(ConditionalForwardDeclaration(declaration, type_condition))
            continue
        match = re.search(r'using (.*) = std::variant<$', line)
        if match:
            line_number = line_number + 1
            alias_lines = ['std::variant<']
            while not file_lines[line_number].startswith('>'):
                alias_lines.append('    ' + file_lines[line_number])
                line_number = line_number + 1
            alias_lines.append('>')
            using_statements.append(UsingStatement(match.groups()[0], alias_lines, type_condition))
            continue
        match = re.search(r'using (.*) = ([^;]*)', line)
        if match:
            using_statements.append(UsingStatement(match.groups()[0], [match.groups()[1]], type_condition))
            continue
        if underlying_type is not None:
            members.append(EnumMember(line.strip(' ,'), member_condition))
            continue
        elif metadata == 'subclasses':
            match = re.search(r'(.*)::(.*)', line.strip(' ,'))
            if match:
                subclass_namespace, subclass_name = match.groups()
                subclass_member = MemberVariable("subclass", subclass_name, member_condition, [], namespace=subclass_namespace, is_subclass=True)
                members.append(subclass_member)
            continue

        if struct_or_class == 'webkit_secure_coding':
            match = re.search(r'\[(.*)\] (.*): ([^;]*)', line)
        else:
            match = re.search(r'\[(.*)\] (.*) ([^;]*)', line)
        if match:
            member_attributes_s, member_type, member_name = match.groups()
            member_attributes = []
            match = re.search(r"((, |^)+(Validator='.*?'))(, |$)?", member_attributes_s)
            if match:
                complete, _, validator, _ = match.groups()
                member_attributes.append(validator)
                member_attributes_s = member_attributes_s.replace(complete, "")
            match = re.search(r"((, |^)+(SecureCodingAllowed=\[.*?\]))(, |$)?", member_attributes_s)
            if match:
                complete, _, allow_list, _ = match.groups()
                member_attributes.append(allow_list)
                member_attributes_s = member_attributes_s.replace(complete, "")
            member_attributes += [member_attribute.strip() for member_attribute in member_attributes_s.split(",")]
            if struct_or_class == 'webkit_secure_coding':
                dictionary_members.append(MemberVariable(member_type, member_name, member_condition, member_attributes))
            else:
                members.append(MemberVariable(member_type, member_name, member_condition, member_attributes))
        else:
            if struct_or_class == 'webkit_secure_coding':
                match = re.search(r'(.*): ([^;]*)', line)
            else:
                match = re.search(r'(.*) ([^;]*)', line)
            if match:
                member_type, member_name = match.groups()
                if struct_or_class == 'webkit_secure_coding':
                    dictionary_members.append(MemberVariable(member_type, member_name, member_condition, []))
                else:
                    members.append(MemberVariable(member_type, member_name, member_condition, []))
    return [serialized_types, serialized_enums, headers, using_statements, additional_forward_declarations, objc_wrapped_types]


def generate_webkit_secure_coding_impl(serialized_types, headers):
    result = []
    result.append(_license_header)
    result.append('#include "config.h"')
    result.append('#include "GeneratedWebKitSecureCoding.h"')
    result.append('')

    header_set = set()
    header_set.add(ConditionalHeader('"ArgumentCodersCocoa.h"', None))
    for header in headers:
        if header.secure_coding:
            header_set.add(header)
    result.extend(output_sorted_headers(sorted(header_set)))

    result.append('')
    result.append('namespace WebKit {')
    result.append('')
    result.append('static RetainPtr<NSDictionary> dictionaryForWebKitSecureCodingTypeFromWKKeyedCoder(id object)')
    result.append('{')
    result.append('    auto archiver = adoptNS([WKKeyedCoder new]);')
    result.append('    [object encodeWithCoder:archiver.get()];')
    result.append('    return [archiver accumulatedDictionary];')
    result.append('}')
    result.append('')
    result.append('static RetainPtr<NSDictionary> dictionaryForWebKitSecureCodingType(id object)')
    result.append('{')
    result.append('    if (WebKit::conformsToWebKitSecureCoding(object))')
    result.append('        return [object _webKitPropertyListData];')
    result.append('')
    result.append('    return dictionaryForWebKitSecureCodingTypeFromWKKeyedCoder(object);')
    result.append('}')
    result.append('')
    result.append('template<typename T> static RetainPtr<NSDictionary> dictionaryFromVector(const Vector<std::pair<String, RetainPtr<T>>>& vector)')
    result.append('{')
    result.append('    NSMutableDictionary *dictionary = [NSMutableDictionary dictionaryWithCapacity:vector.size()];')
    result.append('    for (auto& pair : vector)')
    result.append('        dictionary[pair.first] = pair.second;')
    result.append('    return dictionary;')
    result.append('}')
    result.append('')
    result.append('template<typename T> static RetainPtr<NSDictionary> dictionaryFromOptionalVector(const std::optional<Vector<std::pair<String, RetainPtr<T>>>>& vector)')
    result.append('{')
    result.append('    if (!vector)')
    result.append('        return nil;')
    result.append('    return dictionaryFromVector<T>(*vector);')
    result.append('}')
    result.append('')
    result.append('template<typename T> static Vector<std::pair<String, RetainPtr<T>>> vectorFromDictionary(NSDictionary *dictionary)')
    result.append('{')
    result.append('    if (![dictionary isKindOfClass:NSDictionary.class])')
    result.append('        return { };')
    result.append('    __block Vector<std::pair<String, RetainPtr<T>>> result;')
    result.append('    [dictionary enumerateKeysAndObjectsUsingBlock:^(id key, id value, BOOL*){')
    result.append('        if ([key isKindOfClass:NSString.class] && [value isKindOfClass:IPC::getClass<T>()])')
    result.append('            result.append((NSString *)key, (T)value);')
    result.append('    }];')
    result.append('    return result;')
    result.append('}')
    result.append('')
    result.append('template<typename T> static std::optional<Vector<std::pair<String, RetainPtr<T>>>> optionalVectorFromDictionary(NSDictionary *dictionary)')
    result.append('{')
    result.append('    if (![dictionary isKindOfClass:NSDictionary.class])')
    result.append('        return std::nullopt;')
    result.append('    return vectorFromDictionary<T>(dictionary);')
    result.append('}')
    result.append('')
    result.append('template<typename T> static RetainPtr<NSArray> arrayFromVector(const Vector<RetainPtr<T>>& vector)')
    result.append('{')
    result.append('    return createNSArray(vector, [] (auto& t) {')
    result.append('        return t.get();')
    result.append('    });')
    result.append('}')
    result.append('')
    result.append('template<typename T> static RetainPtr<NSArray> arrayFromOptionalVector(const std::optional<Vector<RetainPtr<T>>>& vector)')
    result.append('{')
    result.append('    if (!vector)')
    result.append('        return nil;')
    result.append('    return arrayFromVector<T>(*vector);')
    result.append('}')
    result.append('')
    result.append('template<typename T> static Vector<RetainPtr<T>> vectorFromArray(NSArray *array)')
    result.append('{')
    result.append('    if (![array isKindOfClass:NSArray.class])')
    result.append('        return { };')
    result.append('    Vector<RetainPtr<T>> result;')
    result.append('    for (id element in array) {')
    result.append('        if ([element isKindOfClass:IPC::getClass<T>()])')
    result.append('            result.append((T *)element);')
    result.append('    }')
    result.append('    return result;')
    result.append('}')
    result.append('')
    result.append('template<typename T> static std::optional<Vector<RetainPtr<T>>> optionalVectorFromArray(NSArray *array)')
    result.append('{')
    result.append('    if (![array isKindOfClass:NSArray.class])')
    result.append('        return std::nullopt;')
    result.append('    return vectorFromArray<T>(array);')
    result.append('}')
    result.append('')
    for type in serialized_types:
        if not type.is_webkit_secure_coding_type():
            continue
        if type.condition is not None:
            result.append(f'#if {type.condition}')

        result.append(f'{type.cpp_struct_or_class_name()}::{type.cpp_struct_or_class_name()}(')
        for i in range(len(type.dictionary_members)):
            member = type.dictionary_members[i]
            result.append(f'    {member.dictionary_type()}&& {member.type}{"," if i < len(type.dictionary_members) - 1 else ""}')
        result.append(')')
        for i in range(len(type.dictionary_members)):
            member = type.dictionary_members[i]
            result.append(f'    {":" if i == 0 else ","} m_{member.type}(WTFMove({member.type}))')
        result.append('{')
        result.append('}')
        result.append('')
        result.append(f'{type.cpp_struct_or_class_name()}::{type.cpp_struct_or_class_name()}({type.name} *object)')
        result.append('{')
        useWKKeyedCoderOnly = type.support_wkkeyedcoder and type.custom_secure_coding_class is None
        if useWKKeyedCoderOnly:
            result.append('    auto dictionary = dictionaryForWebKitSecureCodingTypeFromWKKeyedCoder(object);')
        else:
            result.append('    auto dictionary = dictionaryForWebKitSecureCodingType(object);')
        for member in type.dictionary_members:
            if member.has_container_contents():
                if member.value_is_optional():
                    if member.dictionary_contents() is not None:
                        result.append(f'    m_{member.type} = optionalVectorFromDictionary<{member.dictionary_contents()}>(({member.ns_type_pointer()})[dictionary objectForKey:@"{member.type}"]);')
                    if member.array_contents() is not None:
                        result.append(f'    m_{member.type} = optionalVectorFromArray<{member.array_contents()}>(({member.ns_type_pointer()})[dictionary objectForKey:@"{member.type}"]);')
                else:
                    if member.dictionary_contents() is not None:
                        result.append(f'    m_{member.type} = vectorFromDictionary<{member.dictionary_contents()}>(({member.ns_type_pointer()})[dictionary objectForKey:@"{member.type}"]);')
                    if member.array_contents() is not None:
                        result.append(f'    m_{member.type} = vectorFromArray<{member.array_contents()}>(({member.ns_type_pointer()})[dictionary objectForKey:@"{member.type}"]);')
            else:
                result.append(f'    m_{member.type} = ({member.ns_type_pointer()})[dictionary objectForKey:@"{member.type}"];')
                result.append(f'    if (!{member.type_check()})')
                result.append(f'        m_{member.type} = nullptr;')
                # FIXME: We ought to be able to ASSERT_NOT_REACHED() here once all the question marks are in the right places.
                result.append('')
        result.append('}')
        result.append('')
        result.append(f'RetainPtr<id> {type.cpp_struct_or_class_name()}::toID() const')
        result.append('{')
        result.append(f'    auto propertyList = [NSMutableDictionary dictionaryWithCapacity:{str(len(type.dictionary_members))}];')
        for member in type.dictionary_members:
            if not member.has_container_contents():
                result.append(f'    if (m_{member.type})')
                result.append(f'        propertyList[@"{member.type}"] = {member.id_cast()}m_{member.type}.get();')
        for member in type.dictionary_members:
            if member.value_is_optional():
                if member.dictionary_contents() is not None:
                    result.append(f'    if (auto dictionary = dictionaryFromOptionalVector(m_{member.type}))')
                    result.append(f'        propertyList[@"{member.type}"] = dictionary.get();')
                if member.array_contents() is not None:
                    result.append(f'    if (auto array = arrayFromOptionalVector(m_{member.type}))')
                    result.append(f'        propertyList[@"{member.type}"] = array.get();')
            else:
                if member.dictionary_contents() is not None:
                    result.append(f'    propertyList[@"{member.type}"] = dictionaryFromVector(m_{member.type}).get();')
                if member.array_contents() is not None:
                    result.append(f'    propertyList[@"{member.type}"] = arrayFromVector(m_{member.type}).get();')
        type_name = type.name
        if type.custom_secure_coding_class is not None:
            type_name = type.custom_secure_coding_class
        if not type.support_wkkeyedcoder:
            result.append(f'    RELEASE_ASSERT([{type_name} instancesRespondToSelector:@selector(_initWithWebKitPropertyListData:)]);')
        if not useWKKeyedCoderOnly:
            result.append(f'    if ([{type_name} instancesRespondToSelector:@selector(_initWithWebKitPropertyListData:)])')
            result.append(f'        return adoptNS([[{type_name} alloc] _initWithWebKitPropertyListData:propertyList]);')
        result.append('')
        result.append('    auto unarchiver = adoptNS([[WKKeyedCoder alloc] initWithDictionary:propertyList]);')
        result.append(f'    return adoptNS([[{type_name} alloc] initWithCoder:unarchiver.get()]);')
        result.append('}')

        if type.condition is not None:
            result.append(f'#endif // {type.condition}')
        result.append('')
    result.append('} // namespace WebKit')
    result.append('')
    return '\n'.join(result)


def generate_webkit_secure_coding_header(serialized_types):
    result = []
    result.append(_license_header)
    result.append('#pragma once')
    result.append('')
    result.append('#if PLATFORM(COCOA)')
    result.append('#include "CoreIPCTypes.h"')
    result.append('#include <wtf/cocoa/VectorCocoa.h>')
    result.append('')

    for type in serialized_types:
        if not type.is_webkit_secure_coding_type():
            continue
        if type.condition is not None:
            result.append(f'#if {type.condition}')
        result.append(f'OBJC_CLASS {type.name};')
        if type.condition is not None:
            result.append('#endif')

    result.append('')
    result.append('namespace WebKit {')
    for type in serialized_types:
        if not type.is_webkit_secure_coding_type():
            continue
        result.append('')
        if type.condition is not None:
            result.append(f'#if {type.condition}')
        result.append(f'class {type.cpp_struct_or_class_name()} {{')
        result.append('public:')
        result.append(f'    {type.cpp_struct_or_class_name()}({type.name} *);')
        result.append(f'    {type.cpp_struct_or_class_name()}(const RetainPtr<{type.name}>& object)')
        result.append(f'        : {type.cpp_struct_or_class_name()}(object.get()) {{ }}')
        result.append('')
        result.append('    RetainPtr<id> toID() const;')
        result.append('')
        result.append('private:')
        result.append(f'    friend struct IPC::ArgumentCoder<{type.cpp_struct_or_class_name()}, void>;')
        result.append('')
        result.append(f'    {type.cpp_struct_or_class_name()}(')
        for i in range(len(type.dictionary_members)):
            member = type.dictionary_members[i]
            result.append(f'        {member.dictionary_type()}&&{"," if i < len(type.dictionary_members) - 1 else ""}')
        result.append('    );')
        result.append('')
        for member in type.dictionary_members:
            result.append(f'    {member.dictionary_type()} m_{member.type};')
        result.append('};')
        if type.condition is not None:
            result.append('#endif')
    result.append('')
    result.append('} // namespace WebKit')
    result.append('')
    result.append('#endif // PLATFORM(COCOA)')
    result.append('')
    return '\n'.join(result)


def main(argv):
    serialized_types = []
    serialized_enums = []
    using_statements = []
    objc_wrapped_types = []
    headers = []
    header_set = set()
    header_set.add(ConditionalHeader('"FormDataReference.h"', None))
    additional_forward_declarations_list = []
    file_extension = argv[1]
    for i in range(2, len(argv)):
        with open(argv[i]) as file:
            new_types, new_enums, new_headers, new_using_statements, new_additional_forward_declarations, new_objc_wrapped_types = parse_serialized_types(file)
            for type in new_types:
                serialized_types.append(type)
            for enum in new_enums:
                serialized_enums.append(enum)
            for using_statement in new_using_statements:
                using_statements.append(using_statement)
            for header in new_headers:
                header_set.add(header)
            for declaration in new_additional_forward_declarations:
                additional_forward_declarations_list.append(declaration)
            for objc_wrapped_type in new_objc_wrapped_types:
                objc_wrapped_types.append(objc_wrapped_type)
    headers = sorted(header_set)

    serialized_types = resolve_inheritance(serialized_types)

    with open('GeneratedSerializers.h', "w+") as output:
        output.write(generate_header(serialized_types, serialized_enums, additional_forward_declarations_list))
    with open('GeneratedSerializers.%s' % file_extension, "w+") as output:
        output.write(generate_impl(serialized_types, serialized_enums, headers, False, []))
    with open('WebKitPlatformGeneratedSerializers.%s' % file_extension, "w+") as output:
        output.write(generate_impl(serialized_types, serialized_enums, headers, True, objc_wrapped_types))
    with open('SerializedTypeInfo.%s' % file_extension, "w+") as output:
        output.write(generate_serialized_type_info(serialized_types, serialized_enums, headers, using_statements, objc_wrapped_types))
    with open('GeneratedWebKitSecureCoding.h', "w+") as output:
        output.write(generate_webkit_secure_coding_header(serialized_types))
    with open('GeneratedWebKitSecureCoding.%s' % file_extension, "w+") as output:
        output.write(generate_webkit_secure_coding_impl(serialized_types, headers))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
