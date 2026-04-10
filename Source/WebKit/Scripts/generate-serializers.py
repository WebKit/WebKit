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

import argparse
import copy
import os
import re
import sys

from webkit.opaque_ipc_types import is_opaque_type, opaque_ipc_types
from webkit.serialization_parser import (
    Template, SerializedType, SerializedEnum, MemberVariable, EnumMember,
    ConditionalForwardDeclaration, ConditionalHeader, UsingStatement,
    ObjCWrappedType, ConditionStackEntry, generate_condition_expression,
    parse_serialized_types,
)


def _enforce_serialized_type_opaque_usage(self):
    for member in self.members:
        if is_opaque_type(member.type):
            namespace_and_name = self.namespace_and_name()
            if not opaque_ipc_types.structure_param_tracked(namespace_and_name, member.name, member.type):
                raise Exception(f"Justification needed in opaque_ipc_types.tracking.in: [] StructureParam {namespace_and_name}.{member.name} {member.type}")


SerializedType.enforce_opaque_ipc_types_usage = _enforce_serialized_type_opaque_usage


def _enforce_using_statement_opaque_usage(self):
    for alias_line in self.alias_lines:
        if alias_line.strip().startswith('#'):
            continue
        cleaned_line = alias_line.strip().rstrip(',;')
        if not cleaned_line or cleaned_line in ['Variant<', '>']:
            continue
        if is_opaque_type(cleaned_line):
            if not opaque_ipc_types.alias_param_tracked(self.name, cleaned_line):
                raise Exception(f"Justification needed in opaque_ipc_types.tracking.in: [] AliasParam {self.name} {cleaned_line}")


UsingStatement.enforce_opaque_ipc_types_usage = _enforce_using_statement_opaque_usage

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
        result.append(f'    static void encode({encoder}&, const RetainPtr<{name_with_template}>&);')
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


def argument_coder_declarations(serialized_types, skip_nested, webkit_platform):
    result = []
    for type in serialized_types:
        if type.nested == skip_nested:
            continue
        if (webkit_platform is not None and type.webkit_platform != webkit_platform):
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
    match = re.search(r'(struct|class) ([^<]*)<', alias)
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
    if (match):
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

    result.append('#if USE(CF)')
    result.append('#ifdef __swift__')
    result.append('#include <Security/SecTrust.h>')
    result.append('#else')
    result.append('typedef struct CF_BRIDGED_TYPE(id) __SecTrust *SecTrustRef;')
    result.append('#endif')
    result.append('#endif')

    result += generate_forward_declarations(serialized_types, serialized_enums, additional_forward_declarations)
    result.append('')
    result.append('namespace IPC {')
    result.append('')
    result.append('class Decoder;')
    result.append('class Encoder;')
    result.append('class StreamConnectionEncoder;')
    result = result + argument_coder_declarations(serialized_types, True, None)
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
        result.append('    static_assert(IsIncreasing < 0')
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
        result.append(f'    >);')
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
                result.append('        encoder << WTF::move(*subclass);')
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
            if not opaque_ipc_types.structure_webcontent_dispatchable(type.namespace_and_name(), member.name, member.type):
                result.append('    ASSERT(!isInWebProcess());')
            if type.rvalue and '()' not in member.name:
                if 'EncodeRequestBody' in member.attributes:
                    result.append(f'    RefPtr {member.name}Body = instance.{member.name}.httpBody();')
                result.append(f'    encoder << WTF::move(instance.{member.name});')
                if 'EncodeRequestBody' in member.attributes:
                    result.append(f'    encoder << IPC::FormDataReference {{ WTF::move({member.name}Body) }};')
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
    result.append('    auto isEngaged = decoder.template decode<bool>();')
    result.append('    if (!isEngaged)')
    result.append('        return std::nullopt;')
    result.append('    if (!*isEngaged)')
    result.append('        return { nullptr };')
    result.append(f'    auto result = decoder.decode<{type.cf_wrapper_type()}>();')
    result.append('    if (!decoder.isValid()) [[unlikely]]')
    result.append('        return std::nullopt;')
    if type.to_cf_method is not None:
        result.append(f'    return {type.to_cf_method};')
    else:
        result.append('    return result->toCF();')
    return result


def should_decode_ref(member, serialized_types):
    for serialized_type in serialized_types:
        if serialized_type.namespace_and_name() == member.type:
            return serialized_type.members_are_subclasses
    return False


def decode_type(type, serialized_types):
    if type.cf_type is not None:
        return decode_cf_type(type)

    result = []
    if type.parent_class is not None:
        result = result + decode_type(type.parent_class, serialized_types)

    if type.members_are_subclasses:
        result.append(f'    auto type = decoder.decode<{type.subclass_enum_name()}>();')
        result.append('    UNUSED_PARAM(type);')
        result.append('    if (!decoder.isValid()) [[unlikely]]')
        result.append('        return std::nullopt;')
        result.append('')

    if type.has_optional_tuple_bits() and type.populate_from_empty_constructor:
        result.append(f'    {type.namespace_and_name()} result;')

    if type.debug_decoding_failure:
        result.append('    bool addedDecodingFailureIndex = false;')

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
            result.append(f'    auto {sanitized_variable_name} = decoder.decodeWithAllowedClasses<{member.type}>({{ {decodable_classes[0]} }});')
        elif member.is_subclass:
            result.append(f'    if (type == {type.subclass_enum_name()}::{member.name}) {{')
            typename = f'{member.namespace}::{member.name}'
            result.append(f'        auto result = decoder.decode<Ref<{typename}>>();')
            result.append('        if (!decoder.isValid()) [[unlikely]]')
            result.append('            return std::nullopt;')
            result.append('        return WTF::move(*result);')
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
                result.append(f'            result.{sanitized_variable_name} = WTF::move(*deserialized);')
                result.append('        else')
                result.append('            return std::nullopt;')
                result.append('    }')
            else:
                result.append('')
                result.append(f'    {member.type} {sanitized_variable_name} {{ }};')
                result.append(f'    if (*{bits_name} & {member.optional_tuple_bit()}) {{')
                result.append(f'        if (auto deserialized = decoder.decode<{member.type}>())')
                result.append(f'            {sanitized_variable_name} = WTF::move(*deserialized);')
                result.append('        else')
                result.append('            return std::nullopt;')
                result.append('    }')
        else:
            assert len(decodable_classes) == 0
            if should_decode_ref(member, serialized_types):
                result.append(f'    auto {sanitized_variable_name} = decoder.decode<Ref<{member.type}>>();')
            else:
                result.append(f'    auto {sanitized_variable_name} = decoder.decode<{member.type}>();')
            if 'EncodeRequestBody' in member.attributes:
                result.append(f'    if ({sanitized_variable_name}) {{')
                result.append(f'        if (auto {sanitized_variable_name}Body = decoder.decode<IPC::FormDataReference>())')
                result.append(f'            {sanitized_variable_name}->setHTTPBody({sanitized_variable_name}Body->takeData());')
                result.append('    }')
            if type.debug_decoding_failure:
                result.append(f'    if (!{sanitized_variable_name} && !addedDecodingFailureIndex) [[unlikely]] {{')
                result.append(f'        decoder.addIndexOfDecodingFailure({str(i)});')
                result.append('        addedDecodingFailureIndex = true;')
                result.append('    }')
        for attribute in member.attributes:
            match = re.search(r'Validator=\'(.*)\'', attribute)
            if match:
                validator, = match.groups()
                result.append('    if (!decoder.isValid()) [[unlikely]]')
                result.append('        return std::nullopt;')
                result.append('')
                result.append(f'    if (!({validator})) {{')
                result.append('#if ENABLE(IPC_TESTING_API)')
                result.append(f'        decoder.setErrorString("Validation failed: {validator}"_s);')
                result.append('#endif')
                result.append('        return std::nullopt;')
                result.append(f'    }}')
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
        result.append(f'{indent(indentation + 1)}WTF::move({"" if member.optional_tuple_bit() else "*"}{sanitize_string_for_variable_name(member.name)}){"" if i == len(serialized_members) - 1 else ","}')
        if member.condition is not None:
            result.append('#endif')
    for i in range(len(type.dictionary_members)):
        member = type.dictionary_members[i]
        result.append(f'{indent(indentation + 1)}WTF::move(*{member.type}){"," if i < len(type.dictionary_members) - 1 else ""}')
    if type.create_using or type.return_ref:
        result.append(indent(indentation) + ')')
    else:
        result.append(indent(indentation) + '}')
    return result


def generate_one_impl(type, template_argument, serialized_types):
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
                result.append(f'    auto instance = {type.generic_wrapper}(WTF::move({instanceArgName}));')
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
        for encoder in type.encoders:
            result.append(f'void ArgumentCoder<RetainPtr<{name_with_template}>>::encode({encoder}& encoder, const RetainPtr<{name_with_template}>& retainPtr)')
            result.append('{')
            result.append('    if (!retainPtr) {')
            result.append('        encoder << false;')
            result.append('        return;')
            result.append('    }')
            result.append('    encoder << true;')
            result.append(f'    ArgumentCoder<{name_with_template}>::encode(encoder, retainPtr.get());')
            result.append('}')
            result.append('')
    if type.cf_type is not None:
        result.append(f'std::optional<RetainPtr<{name_with_template}>> ArgumentCoder<RetainPtr<{name_with_template}>>::decode(Decoder& decoder)')
    elif type.return_ref:
        result.append(f'std::optional<Ref<{name_with_template}>> ArgumentCoder<{name_with_template}>::decode(Decoder& decoder)')
    else:
        result.append(f'std::optional<{name_with_template}> ArgumentCoder<{name_with_template}>::decode(Decoder& decoder)')
    result.append('{')
    result = result + decode_type(type, serialized_types)
    if type.cf_type is None:
        if not type.members_are_subclasses:
            result.append('    if (!decoder.isValid()) [[unlikely]]')
            result.append('        return std::nullopt;')
            if type.populate_from_empty_constructor and not type.has_optional_tuple_bits():
                result.append(f'    {name_with_template} result;')
                for member in type.serialized_members():
                    if member.condition is not None:
                        result.append(f'#if {member.condition}')
                    result.append(f'    result.{member.name} = WTF::move(*{member.name});')
                    if member.condition is not None:
                        result.append('#endif')
                result.append('    return { WTF::move(result) };')
            elif type.has_optional_tuple_bits() and type.populate_from_empty_constructor:
                result.append('    return { WTF::move(result) };')
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
    result.append('#include <wtf/IsIncreasing.h>')
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

    result = result + argument_coder_declarations(serialized_types, False, generating_webkit_platform_impl)
    result.append('')

    for type in serialized_types:
        if type.webkit_platform != generating_webkit_platform_impl:
            continue
        if type.templates:
            for template in type.templates:
                result.extend(generate_one_impl(type, template, serialized_types))
        else:
            result.extend(generate_one_impl(type, None, serialized_types))
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
        result.append('            { "Variant<"')
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
        # Generate valueMap with both values and names
        if enum.underlying_type == 'bool':
            result.append('            { 0, "false"_s },')
            result.append('            { 1, "true"_s }')
        else:
            for valid_value in enum.valid_values:
                if valid_value.condition is not None:
                    result.append(f'#if {valid_value.condition}')
                result.append(f'            {{ enumValueForIPCTestAPI({enum.namespace_and_name()}::{valid_value.name}), "{valid_value.name}"_s }},')
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
    result.append('[[maybe_unused]] static RetainPtr<NSDictionary> dictionaryForWebKitSecureCodingType(id object)')
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
    # FIXME: isKindOfClass call can cause a static analysis false positive (https://github.com/llvm/llvm-project/issues/162979).
    result.append('        SUPPRESS_UNRETAINED_ARG if ([element isKindOfClass:retainPtr(IPC::getClass<T>()).get()])')
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
            result.append(f'    {":" if i == 0 else ","} m_{member.type}(WTF::move({member.type}))')
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
                        result.append(f'    m_{member.type} = optionalVectorFromDictionary<{member.dictionary_contents()}>(({member.ns_type_pointer()})retainPtr([dictionary objectForKey:@"{member.type}"]).get());')
                    if member.array_contents() is not None:
                        result.append(f'    m_{member.type} = optionalVectorFromArray<{member.array_contents()}>(({member.ns_type_pointer()})retainPtr([dictionary objectForKey:@"{member.type}"]).get());')
                else:
                    if member.dictionary_contents() is not None:
                        result.append(f'    m_{member.type} = vectorFromDictionary<{member.dictionary_contents()}>(({member.ns_type_pointer()})retainPtr([dictionary objectForKey:@"{member.type}"]).get());')
                    if member.array_contents() is not None:
                        result.append(f'    m_{member.type} = vectorFromArray<{member.array_contents()}>(({member.ns_type_pointer()})retainPtr([dictionary objectForKey:@"{member.type}"]).get());')
            else:
                result.append(f'    m_{member.type} = ({member.ns_type_pointer()})[dictionary objectForKey:@"{member.type}"];')
                # FIXME: isKindOfClass call from type_check() can cause a static analysis false positive (https://github.com/llvm/llvm-project/issues/162979).
                result.append(f'    SUPPRESS_UNRETAINED_ARG if (!{member.type_check()})')
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
        result.append(f'    friend struct IPC::ArgumentCoder<{type.cpp_struct_or_class_name()}>;')
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
    parser = argparse.ArgumentParser(description='Generate serializers from input files')
    parser.add_argument('file_extension', help='File extension for output files')
    parser.add_argument('input_files', nargs='+', help='Input files to process')
    parser.add_argument('--output-dir', help='Directory for output files')

    args = parser.parse_args(argv[1:])

    serialized_types = []
    serialized_enums = []
    using_statements = []
    objc_wrapped_types = []
    headers = []
    header_set = set()
    header_set.add(ConditionalHeader('"FormDataReference.h"', None))
    additional_forward_declarations_list = []
    file_extension = args.file_extension
    output_dir = args.output_dir

    input_files = args.input_files

    for input_file in input_files:
        with open(input_file) as file:
            new_types, new_enums, new_headers, new_using_statements, new_additional_forward_declarations, new_objc_wrapped_types = parse_serialized_types(file)
            for type in new_types:
                type.enforce_opaque_ipc_types_usage()
                serialized_types.append(type)
            for enum in new_enums:
                serialized_enums.append(enum)
            for using_statement in new_using_statements:
                using_statement.enforce_opaque_ipc_types_usage()
                using_statements.append(using_statement)
            for header in new_headers:
                header_set.add(header)
            for declaration in new_additional_forward_declarations:
                additional_forward_declarations_list.append(declaration)
            for objc_wrapped_type in new_objc_wrapped_types:
                objc_wrapped_types.append(objc_wrapped_type)
    headers = sorted(header_set)

    serialized_types = resolve_inheritance(serialized_types)

    if output_dir and not os.path.exists(output_dir):
        os.makedirs(output_dir)

    def output_path(filename):
        if output_dir:
            return os.path.join(output_dir, filename)
        return filename

    with open(output_path('GeneratedSerializers.h'), "w+") as output:
        output.write(generate_header(serialized_types, serialized_enums, additional_forward_declarations_list))
    with open(output_path('GeneratedSerializers.%s' % file_extension), "w+") as output:
        output.write(generate_impl(serialized_types, serialized_enums, headers, False, []))
    with open(output_path('WebKitPlatformGeneratedSerializers.%s' % file_extension), "w+") as output:
        output.write(generate_impl(serialized_types, serialized_enums, headers, True, objc_wrapped_types))
    with open(output_path('SerializedTypeInfo.%s' % file_extension), "w+") as output:
        output.write(generate_serialized_type_info(serialized_types, serialized_enums, headers, using_statements, objc_wrapped_types))
    with open(output_path('GeneratedWebKitSecureCoding.h'), "w+") as output:
        output.write(generate_webkit_secure_coding_header(serialized_types))
    with open(output_path('GeneratedWebKitSecureCoding.%s' % file_extension), "w+") as output:
        output.write(generate_webkit_secure_coding_impl(serialized_types, headers))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
