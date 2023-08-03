#!/usr/bin/env python3
#
# Copyright (C) 2022 Apple Inc. All rights reserved.
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

import re
import sys

# Supported type attributes:
#
# AdditionalEncoder - generate serializers for StreamConnectionEncoder in addition to IPC::Encoder.
# CreateUsing - use a custom function to call instead of the constructor or create.
# CustomHeader - don't include a header based on the struct/class name. Only needed for non-enum types.
# Alias - this type is not a struct or class, but a typedef.
# Nested - this type is only serialized as a member of its parent, so work around the need for http://wg21.link/P0289 and don't forward declare it in the header.
# RefCounted - deserializer returns a std::optional<Ref<T>> instead of a std::optional<T>.
# LegacyPopulateFromEmptyConstructor - instead of calling a constructor with the members, call the empty constructor then insert the members one at a time.
# OptionSet - for enum classes, instead of only allowing deserialization of the exact values, allow deserialization of any bit combination of the values.
# RValue - serializer takes an rvalue reference, instead of an lvalue.
# WebKitPlatform - put serializer into a file built as part of WebKitPlatform
# CustomEncoder - Only generate the decoder, not the encoder.
#
# Supported member attributes:
#
# BitField - work around the need for http://wg21.link/P0572 and don't check that the serialization order matches the memory layout.
# Validator - additional C++ to validate the value when decoding
# NotSerialized - member is present in structure but intentionally not serialized.
# SecureCodingAllowed - ObjC classes to allow when decoding.
# OptionalTupleBits - This member stores bits of whether each following member is serialized. Attribute must be on first member.
# OptionalTupleBit - The name of the bit indicating whether this member is serialized.

class SerializedType(object):
    def __init__(self, struct_or_class, namespace, name, parent_class_name, members, condition, attributes, other_metadata=None):
        self.struct_or_class = struct_or_class
        self.namespace = namespace
        self.name = name
        self.parent_class_name = parent_class_name
        self.parent_class = None
        self.members = members
        self.alias = None
        self.condition = condition
        self.encoders = ['Encoder']
        self.serialize_with_function_calls = False
        self.return_ref = False
        self.create_using = False
        self.populate_from_empty_constructor = False
        self.nested = False
        self.rvalue = False
        self.webkit_platform = False
        self.members_are_subclasses = False
        self.custom_encoder = False
        if attributes is not None:
            for attribute in attributes.split(', '):
                if '=' in attribute:
                    key, value = attribute.split('=')
                    if key == 'AdditionalEncoder':
                        self.encoders.append(value)
                    if key == 'CreateUsing':
                        self.create_using = value
                    if key == 'Alias':
                        self.alias = value
                else:
                    if attribute == 'Nested':
                        self.nested = True
                    elif attribute == 'RefCounted':
                        self.return_ref = True
                    elif attribute == 'RValue':
                        self.rvalue = True
                    elif attribute == 'WebKitPlatform':
                        self.webkit_platform = True
                    elif attribute == 'LegacyPopulateFromEmptyConstructor':
                        self.populate_from_empty_constructor = True
                    elif attribute == 'CustomEncoder':
                        self.custom_encoder = True
        if other_metadata:
            if other_metadata == 'subclasses':
                self.members_are_subclasses = True

    def namespace_and_name(self):
        if self.namespace is None:
            return self.name
        return self.namespace + '::' + self.name

    def namespace_unless_wtf_and_name(self):
        if self.namespace == 'WTF':
            return self.name
        return self.namespace_and_name()

    def subclass_enum_name(self):
        result = ""
        if self.namespace:
            result += self.namespace + "_"
        return result + self.name + "_Subclass"

    def function_name_for_enum(self):
        return 'isValidEnum'

    def can_assert_member_order_is_correct(self):
        for member in self.members:
            if '()' in member.name:
                return False
            if '.' in member.name:
                return False
        return True

    def serialized_members(self):
        return list(filter(lambda member: 'NotSerialized' not in member.attributes, self.members))

    def has_optional_tuple_bits(self):
        if len(self.members) == 0:
            return False
        return 'OptionalTupleBits' in self.members[0].attributes


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
        return self.namespace + '::' + self.name

    def function_name(self):
        if self.is_option_set():
            return 'isValidOptionSet'
        return 'isValidEnum'

    def additional_template_parameter(self):
        if self.is_option_set():
            return ''
        return ', void'

    def parameter(self):
        if self.is_option_set():
            return 'OptionSet<' + self.namespace_and_name() + '>'
        return self.underlying_type

    def is_option_set(self):
        return 'OptionSet' in self.attributes

    def is_nested(self):
        return 'Nested' in self.attributes


class MemberVariable(object):
    def __init__(self, type, name, condition, attributes, namespace=None, is_subclass=False):
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


class EnumMember(object):
    def __init__(self, name, condition):
        self.name = name
        self.condition = condition

class ConditionalHeader(object):
    def __init__(self, header, condition, webkit_platform=False):
        self.header = header
        self.condition = condition
        self.webkit_platform = webkit_platform

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

def sanitize_string_for_variable_name(string):
    return string.replace('()', '').replace('.', '')

_license_header = """/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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


def argument_coder_declarations(serialized_types, skip_nested):
    result = []
    for type in serialized_types:
        if type.nested == skip_nested:
            continue
        result.append('')
        if type.condition is not None:
            result.append('#if ' + type.condition)
        result.append('template<> struct ArgumentCoder<' + type.namespace_and_name() + '> {')
        for encoder in type.encoders:
            if type.rvalue:
                result.append('    static void encode(' + encoder + '&, ' + type.namespace_and_name() + '&&);')
            else:
                result.append('    static void encode(' + encoder + '&, const ' + type.namespace_and_name() + '&);')
        if type.return_ref:
            result.append('    static std::optional<Ref<' + type.namespace_and_name() + '>> decode(Decoder&);')
        else:
            result.append('    static std::optional<' + type.namespace_and_name() + '> decode(Decoder&);')
        result.append('};')
        if type.condition is not None:
            result.append('#endif')
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


def generate_header(serialized_types, serialized_enums):
    result = []
    result.append(_license_header)
    result.append('#pragma once')
    result.append('')
    for header in ['<wtf/ArgumentCoder.h>', '<wtf/OptionSet.h>', '<wtf/Ref.h>']:
        result.append('#include ' + header)

    result.append('')
    for enum in serialized_enums:
        if enum.is_nested():
            continue
        if enum.condition is not None:
            result.append('#if ' + enum.condition)
        if enum.namespace is None:
            result.append('enum class ' + enum.name + ' : ' + enum.underlying_type + ';')
        else:
            result.append('namespace ' + enum.namespace + ' { enum class ' + enum.name + ' : ' + enum.underlying_type + '; }')
        if enum.condition is not None:
            result.append('#endif')
    for type in serialized_types:
        if type.nested:
            continue
        if type.condition is not None:
            result.append('#if ' + type.condition)
        if type.alias is not None:
            result.append('namespace ' + type.namespace + ' {')
            result.append('template<' + typenames(type.alias) + '> ' + alias_struct_or_class(type.alias) + ' ' + remove_template_parameters(type.alias) + ';')
            result.append('using ' + type.name + ' = ' + remove_alias_struct_or_class(type.alias) + ';')
            result.append('}')
        else:
            if type.namespace is None:
                result.append(type.struct_or_class + ' ' + type.name + ';')
            else:
                result.append('namespace ' + type.namespace + ' { ' + type.struct_or_class + ' ' + type.name + '; }')
        if type.condition is not None:
            result.append('#endif')
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
            result.append('#if ' + enum.condition)
        result.append('template<> bool ' + enum.function_name() + '<' + enum.namespace_and_name() + enum.additional_template_parameter() + '>(' + enum.parameter() + ');')
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
            result.append('#if ' + member.condition)
        result.append('    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.' + member.name + ')>, ' + member.type + '>);')
        if member.condition is not None:
            result.append('#endif')
    if type.can_assert_member_order_is_correct():
        # FIXME: Add this check for types with parent classes, too.
        if type.parent_class is None and not checking_parent_class:
            result.append('    struct ShouldBeSameSizeAs' + type.name + ' : public VirtualTableAndRefCountOverhead<std::is_polymorphic_v<' + type.namespace_and_name() + '>, ' + ('true' if type.return_ref else 'false') + '> {')
            for member in type.members:
                if member.condition is not None:
                    result.append('#if ' + member.condition)
                result.append('        ' + member.type + ' ' + member.name + (' : 1' if 'BitField' in member.attributes else '') + ';')
                if member.condition is not None:
                    result.append('#endif')
            result.append('    };')
            result.append('    static_assert(sizeof(ShouldBeSameSizeAs' + type.name + ') == sizeof(' + type.namespace_and_name() + '));')
        result.append('    static_assert(MembersInCorrectOrder < 0')
        for member in type.members:
            if 'BitField' in member.attributes:
                continue
            if member.condition is not None:
                result.append('#if ' + member.condition)
            result.append('        , offsetof(' + type.namespace_and_name() + ', ' + member.name + ')')
            if member.condition is not None:
                result.append('#endif')
        result.append('    >::value);')
    return result


def encode_optional_tuple(type):
    result = []
    serialized_members = type.serialized_members()
    result.append('    static_assert(static_cast<uint64_t>(' + serialized_members[1].optional_tuple_bit() + ') == 1);')
    result.append('    static_assert(BitsInIncreasingOrder<')
    for i in range(len(serialized_members)):
        member = serialized_members[i]
        bit = member.optional_tuple_bit()
        if bit is not None:
            if member.condition is not None:
                result.append('#if ' + member.condition)
            result.append('        ' + (', ' if i > 1 else '') + 'static_cast<uint64_t>(' + bit + ')')
            if member.condition is not None:
                result.append('#endif')
    result.append('    >::value);')

    for member in serialized_members:
        if member.condition is not None:
            result.append('#if ' + member.condition)
        if member.optional_tuple_bits():
            result.append('    encoder << instance.' + member.name + ';')
            bits_variable_name = member.name
        bit = member.optional_tuple_bit()
        if bit is not None:
            result.append('    if (instance.' + bits_variable_name + ' & ' + bit + ')')
            result.append('        encoder << instance.' + member.name + ';')
        if member.condition is not None:
            result.append('#endif')
    return result


def decode_optional_tuple(type):
    result = []
    bits_variable_name = None
    result.append('    ' + type.namespace_and_name() + ' result;')
    for member in type.serialized_members():
        if member.optional_tuple_bits():
            result.append('    auto bits = decoder.decode<' + member.type + '>();')
            result.append('    if (!bits)')
            result.append('        return std::nullopt;')
            result.append('    result.' + member.name + ' = *bits;')
        bit = member.optional_tuple_bit()
        if bit is not None:
            if member.condition is not None:
                result.append('#if ' + member.condition)
            result.append('    if (*bits & ' + bit + ') {')
            result.append('        if (auto deserialized = decoder.decode<' + member.type + '>())')
            result.append('            result.' + member.name + ' = WTFMove(*deserialized);')
            result.append('        else')
            result.append('            return std::nullopt;')
            result.append('    }')
            if member.condition is not None:
                result.append('#endif')
        result.append('')
    return result


def encode_type(type):
    result = []
    if type.parent_class is not None:
        result = result + encode_type(type.parent_class)
    for member in type.serialized_members():
        if member.condition is not None:
            result.append('#if ' + member.condition)
        if member.is_subclass:
            result.append('    if (auto* subclass = dynamicDowncast<' + member.namespace + "::" + member.name + '>(instance)) {')
            result.append('        encoder << ' + type.subclass_enum_name() + "::" + member.name + ";")
            if type.rvalue:
                result.append('        encoder << WTFMove(*subclass);')
            else:
                result.append('        encoder << *subclass;')
            result.append('        return;')
            result.append('    }')
        else:
            if type.rvalue and not type.serialize_with_function_calls:
                result.append('    encoder << WTFMove(instance.' + member.name + ('()' if type.serialize_with_function_calls else '') + ');')
            else:
                result.append('    encoder << instance.' + member.name + ('()' if type.serialize_with_function_calls else '') + ';')
        if member.condition is not None:
            result.append('#endif')

    return result


def decode_type(type):
    result = []
    if type.parent_class is not None:
        result = result + decode_type(type.parent_class)

    if type.members_are_subclasses:
        result.append('    auto type = decoder.decode<' + type.subclass_enum_name() + '>();')
        result.append('    if (UNLIKELY(!decoder.isValid()))')
        result.append('        return std::nullopt;')
        result.append('')

    for member in type.serialized_members():
        if member.condition is not None:
            result.append('#if ' + member.condition)
        sanitized_variable_name = sanitize_string_for_variable_name(member.name)
        r = re.compile("SecureCodingAllowed=\\[(.*)\\]")
        decodable_classes = [r.match(m).groups()[0] for m in list(filter(r.match, member.attributes))]
        if len(decodable_classes) == 1:
            match = re.search("RetainPtr<(.*)>", member.type)
            assert match
            result.append('    auto ' + sanitized_variable_name + ' = IPC::decode<' + match.groups()[0] + '>(decoder, @[ ' + decodable_classes[0] + ' ]);')
        elif member.is_subclass:
            result.append('    if (type == ' + type.subclass_enum_name() + "::" + member.name + ') {')
            typename = member.namespace + "::" + member.name
            result.append('        auto result = decoder.decode<Ref<' + typename + '>>();')
            result.append('        if (UNLIKELY(!decoder.isValid()))')
            result.append('            return std::nullopt;')
            result.append('        return WTFMove(*result);')
            result.append('    }')
        else:
            assert len(decodable_classes) == 0
            result.append('    auto ' + sanitized_variable_name + ' = decoder.decode<' + member.type + '>();')
        for attribute in member.attributes:
            match = re.search(r'Validator=\'(.*)\'', attribute)
            if match:
                validator, = match.groups()
                result.append('    if (UNLIKELY(!decoder.isValid()))')
                result.append('        return std::nullopt;')
                result.append('')
                result.append('    if (!(' + validator + '))')
                result.append('        return std::nullopt;')
                continue
            else:
                match = re.search(r'Validator', attribute)
                assert not match
        if member.condition is not None:
            result.append('#endif')
    return result


def indent(indentation):
    return '    ' * indentation


def construct_type(type, indentation):
    result = []
    if type.create_using:
        result.append(indent(indentation) + type.namespace_and_name() + '::' + type.create_using + '(')
    elif type.return_ref:
        result.append(indent(indentation) + type.namespace_and_name() + '::create(')
    else:
        result.append(indent(indentation) + type.namespace_and_name() + ' {')
    if type.parent_class is not None:
        result = result + construct_type(type.parent_class, indentation + 1)
        if len(type.members) != 0:
            result[-1] += ','
    serialized_members = type.serialized_members()
    for i in range(len(serialized_members)):
        member = serialized_members[i]
        if member.condition is not None:
            result.append('#if ' + member.condition)
        result.append(indent(indentation + 1) + 'WTFMove(*' + sanitize_string_for_variable_name(member.name) + ')' + ('' if i == len(serialized_members) - 1 else ','))
        if member.condition is not None:
            result.append('#endif')
    if type.create_using or type.return_ref:
        result.append(indent(indentation) + ')')
    else:
        result.append(indent(indentation) + '}')
    return result


def generate_impl(serialized_types, serialized_enums, headers, generating_webkit_platform_impl):
    serialized_types = resolve_inheritance(serialized_types)
    result = []
    result.append(_license_header)
    result.append('#include "config.h"')
    result.append('#include "GeneratedSerializers.h"')
    result.append('')
    for header in headers:
        if header.webkit_platform != generating_webkit_platform_impl:
            continue
        if header.condition is not None:
            result.append('#if ' + header.condition)
        result.append('#include ' + header.header)
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
    result.append('template<> struct VirtualTableAndRefCountOverhead<true, true> {')
    result.append('    virtual ~VirtualTableAndRefCountOverhead() { }')
    result.append('    unsigned refCount;')
    result.append('#if ASSERT_ENABLED')
    result.append('    bool m_isOwnedByMainThread;')
    result.append('    bool m_areThreadingChecksEnabled;')
    result.append('#endif')
    result.append('#if CHECK_REF_COUNTED_LIFECYCLE')
    result.append('    bool m_deletionHasBegun;')
    result.append('    bool m_adoptionIsRequired;')
    result.append('#endif')
    result.append('};')
    result.append('template<> struct VirtualTableAndRefCountOverhead<false, true> {')
    result.append('    unsigned refCount;')
    result.append('#if ASSERT_ENABLED')
    result.append('    bool m_isOwnedByMainThread;')
    result.append('    bool m_areThreadingChecksEnabled;')
    result.append('#endif')
    result.append('#if CHECK_REF_COUNTED_LIFECYCLE')
    result.append('    bool m_deletionHasBegun;')
    result.append('    bool m_adoptionIsRequired;')
    result.append('#endif')
    result.append('};')
    result.append('template<> struct VirtualTableAndRefCountOverhead<true, false> {')
    result.append('    virtual ~VirtualTableAndRefCountOverhead() { }')
    result.append('};')
    result.append('template<> struct VirtualTableAndRefCountOverhead<false, false> { };')
    result.append('')
    # GCC is less generous with its interpretation of "Use of the offsetof macro with a
    # type other than a standard-layout class is conditionally-supported".
    result.append('#if COMPILER(GCC)')
    result.append('IGNORE_WARNINGS_BEGIN("invalid-offsetof")')
    result.append('#endif')
    result.append('')
    result.append('namespace IPC {')
    result.append('')
    if not generating_webkit_platform_impl:
        result = result + argument_coder_declarations(serialized_types, False)
        result.append('')
    for type in serialized_types:
        if type.webkit_platform != generating_webkit_platform_impl:
            continue
        if type.condition is not None:
            result.append('#if ' + type.condition)

        if type.members_are_subclasses:
            result.append('enum class ' + type.subclass_enum_name() + " : IPC::EncodedVariantIndex {")
            for idx in range(0, len(type.members)):
                member = type.members[idx]
                if idx == len(type.members) - 1:
                    result.append('    ' + member.name)
                else:
                    result.append('    ' + member.name + ',')
            result.append('};')
            result.append('')
        for encoder in type.encoders:
            if type.custom_encoder:
                continue
            if type.rvalue:
                result.append('void ArgumentCoder<' + type.namespace_and_name() + '>::encode(' + encoder + '& encoder, ' + type.namespace_and_name() + '&& instance)')
            else:
                result.append('void ArgumentCoder<' + type.namespace_and_name() + '>::encode(' + encoder + '& encoder, const ' + type.namespace_and_name() + '& instance)')
            result.append('{')
            if not type.members_are_subclasses:
                result = result + check_type_members(type, False)
            if type.has_optional_tuple_bits():
                result = result + encode_optional_tuple(type)
            else:
                result = result + encode_type(type)
            if type.members_are_subclasses:
                result.append('    ASSERT_NOT_REACHED();')
            result.append('}')
            result.append('')
        if type.return_ref:
            result.append('std::optional<Ref<' + type.namespace_and_name() + '>> ArgumentCoder<' + type.namespace_and_name() + '>::decode(Decoder& decoder)')
        else:
            result.append('std::optional<' + type.namespace_and_name() + '> ArgumentCoder<' + type.namespace_and_name() + '>::decode(Decoder& decoder)')
        result.append('{')
        if type.has_optional_tuple_bits():
            result = result + decode_optional_tuple(type)
        else:
            result = result + decode_type(type)
        if not type.members_are_subclasses:
            if not type.has_optional_tuple_bits():
                result.append('    if (UNLIKELY(!decoder.isValid()))')
                result.append('        return std::nullopt;')
            if type.populate_from_empty_constructor:
                result.append('    ' + type.namespace_and_name() + ' result;')
                for member in type.serialized_members():
                    if member.condition is not None:
                        result.append('#if ' + member.condition)
                    result.append('    result.' + member.name + ' = WTFMove(*' + member.name + ');')
                    if member.condition is not None:
                        result.append('#endif')
                result.append('    return { WTFMove(result) };')
            elif type.has_optional_tuple_bits():
                result.append('    return { WTFMove(result) };')
            else:
                result.append('    return {')
                result = result + construct_type(type, 2)
                result.append('    };')
        else:
            result.append('    ASSERT_NOT_REACHED();')
            result.append('    return std::nullopt;')
        result.append('}')
        result.append('')
        if type.condition is not None:
            result.append('#endif')
            result.append('')
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
            result.append('#if ' + type.condition)
        result.append('template<> bool ' + type.function_name_for_enum() + '<IPC::' + type.subclass_enum_name() + ', void>(IPC::EncodedVariantIndex value)')
        result.append('{')
        result.append('    switch (static_cast<IPC::' + type.subclass_enum_name() + '>(value)) {')
        for member in type.members:
            if member.condition is not None:
                result.append('#if ' + member.condition)
            result.append('    case IPC::' + type.subclass_enum_name() + '::' + member.name + ':')
            if member.condition is not None:
                result.append('#endif')
        result.append('        return true;')
        result.append('    default:')
        result.append('        return false;')
        result.append('    }')
        result.append('}')
        if type.condition is not None:
            result.append('#endif')

    for enum in serialized_enums:
        if generating_webkit_platform_impl:
            continue
        if enum.underlying_type == 'bool':
            continue
        result.append('')
        if enum.condition is not None:
            result.append('#if ' + enum.condition)
        result.append('template<> bool ' + enum.function_name() + '<' + enum.namespace_and_name() + enum.additional_template_parameter() + '>(' + enum.parameter() + ' value)')
        result.append('{')
        if enum.is_option_set():
            result.append('    constexpr ' + enum.underlying_type + ' allValidBitsValue = 0')
            for i in range(0, len(enum.valid_values)):
                valid_value = enum.valid_values[i]
                if valid_value.condition is not None:
                    result.append('#if ' + valid_value.condition)
                result.append('        | static_cast<' + enum.underlying_type + '>(' + enum.namespace_and_name() + '::' + valid_value.name + ')')
                if valid_value.condition is not None:
                    result.append('#endif')
            result.append('        | 0;')
            result.append('    return (value.toRaw() | allValidBitsValue) == allValidBitsValue;')
        else:
            result.append('    switch (static_cast<' + enum.namespace_and_name() + '>(value)) {')
            for valid_value in enum.valid_values:
                if valid_value.condition is not None:
                    result.append('#if ' + valid_value.condition)
                result.append('    case ' + enum.namespace_and_name() + '::' + valid_value.name + ':')
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
    result.append('#if COMPILER(GCC)')
    result.append('IGNORE_WARNINGS_END')
    result.append('#endif')
    result.append('')
    return '\n'.join(result)


def generate_optional_tuple_type_info(type):
    result = []
    result.append('            {')
    result.append('                "OptionalTuple<"')
    serialized_members = type.serialized_members()
    for i in range(1, len(serialized_members)):
        member = serialized_members[i]
        if member.condition is not None:
            result.append('#if ' + member.condition)
        result.append('                    "' + (', ' if i > 1 else '') + member.type + '"')
        if member.condition is not None:
            result.append('#endif')
    result.append('                ">"_s,')
    result.append('                "optionalTuple"_s')
    result.append('            },')
    return result


def generate_serialized_type_info(serialized_types, serialized_enums, headers, typedefs):
    result = []
    result.append(_license_header)
    result.append('#include "config.h"')
    result.append('#include "SerializedTypeInfo.h"')
    result.append('')
    for header in headers:
        if header.condition is not None:
            result.append('#if ' + header.condition)
        result.append('#include ' + header.header)
        if header.condition is not None:
            result.append('#endif')
    result.append('')
    result.append('#if ENABLE(IPC_TESTING_API)')
    result.append('')
    result.append('namespace WebKit {')
    result.append('')
    result.append('Vector<SerializedTypeInfo> allSerializedTypes()')
    result.append('{')
    result.append('    return {')
    for type in serialized_types:
        result.append('        { "' + type.namespace_unless_wtf_and_name() + '"_s, {')
        if type.has_optional_tuple_bits():
            result = result + generate_optional_tuple_type_info(type)
            result.append('        } },')
            continue
        if type.members_are_subclasses:
            result.append('            { "std::variant<' + ', '.join([member.namespace + '::' + member.name for member in type.members]) + '>"_s, "subclasses"_s }')
            result.append('        } },')
            continue

        serialized_members = type.serialized_members()
        for member in serialized_members:
            if member.condition is not None:
                result.append('#if ' + member.condition)
            result.append('            {')
            result.append('                "' + member.type + '"_s,')
            result.append('                "' + member.name + '"_s')
            result.append('            },')
            if member.condition is not None:
                result.append('#endif')
        result.append('        } },')
    for typedef in typedefs:
        result.append('        { "' + typedef[0] + '"_s, {')
        result.append('            { "' + typedef[1] + '"_s, "alias"_s }')
        result.append('        } },')
    result.append('    };')
    result.append('}')
    result.append('')
    result.append('Vector<SerializedEnumInfo> allSerializedEnums()')
    result.append('{')
    result.append('    return {')
    for enum in serialized_enums:
        if enum.condition is not None:
            result.append('#if ' + enum.condition)
        result.append('        { "' + enum.namespace_and_name() + '"_s, sizeof(' + enum.namespace_and_name() + '), ' + ('true' if enum.is_option_set() else 'false') + ', {')
        if enum.underlying_type == 'bool':
            result.append('            0, 1')
        else:
            for valid_value in enum.valid_values:
                if valid_value.condition is not None:
                    result.append('#if ' + valid_value.condition)
                result.append('            static_cast<uint64_t>(' + enum.namespace_and_name() + '::' + valid_value.name + '),')
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


def parse_serialized_types(file, file_name):
    serialized_types = []
    serialized_enums = []
    typedefs = []
    headers = []

    attributes = None
    namespace = None
    name = None
    members = []
    type_condition = None
    member_condition = None
    struct_or_class = None
    underlying_type = None
    parent_class_name = None
    metadata = None

    for line in file:
        line = line.strip()
        if line.startswith('#'):
            if line.startswith('#if '):
                if name is None:
                    type_condition = line[4:]
                else:
                    member_condition = line[4:]
            elif line.startswith('#endif'):
                if name is None:
                    type_condition = None
                else:
                    member_condition = None
            continue
        if line.startswith('}'):
            if underlying_type is not None:
                serialized_enums.append(SerializedEnum(namespace, name, underlying_type, members, type_condition, attributes))
            else:
                type = SerializedType(struct_or_class, namespace, name, parent_class_name, members, type_condition, attributes, metadata)
                serialized_types.append(type)
                if namespace is not None and (attributes is None or ('CustomHeader' not in attributes and 'Nested' not in attributes)):
                    if namespace == 'WebKit':
                        headers.append(ConditionalHeader('"' + name + '.h"', type_condition))
                    elif namespace == 'WTF':
                        headers.append(ConditionalHeader('<wtf/' + name + '.h>', type_condition))
                    elif namespace == 'WebKit::WebGPU':
                        headers.append(ConditionalHeader('"WebGPU' + name + '.h"', type_condition))
                    else:
                        headers.append(ConditionalHeader('<' + namespace + '/' + name + '.h>', type_condition))
            attributes = None
            namespace = None
            name = None
            members = []
            member_condition = None
            struct_or_class = None
            underlying_type = None
            parent_class_name = None
            metadata = None
            continue

        match = re.search(r'^headers?: (.*)', line)
        if match:
            for header in match.group(1).split():
                headers.append(ConditionalHeader(header, type_condition))
            continue
        match = re.search(r'webkit_platform_headers?: (.*)', line)
        if match:
            for header in match.group(1).split():
                headers.append(ConditionalHeader(header, type_condition, True))
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
        match = re.search(r'\[(.*)\] (struct|class|alias) (.*) {', line)
        if match:
            attributes, struct_or_class, name = match.groups()
            continue
        match = re.search(r'(struct|class|alias) (.*) {', line)
        if match:
            struct_or_class, name = match.groups()
            continue
        match = re.search(r'using (.*) = (.*)', line)
        if match:
            typedefs.append(match.groups())
            continue

        if underlying_type is not None:
            members.append(EnumMember(line.strip(' ,'), member_condition))
            continue
        elif metadata == 'subclasses':
            match = re.search(r'(.*)::(.*)', line.strip(' ,'))
            if match:
                subclass_namespace, subclass_name = match.groups()
                subclass_member = MemberVariable("subclass", subclass_name, None, [], namespace=subclass_namespace, is_subclass=True)
                members.append(subclass_member)
            continue

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
            members.append(MemberVariable(member_type, member_name, member_condition, member_attributes))
        else:
            match = re.search(r'(.*) ([^;]*)', line)
            if match:
                member_type, member_name = match.groups()
                members.append(MemberVariable(member_type, member_name, member_condition, []))
    return [serialized_types, serialized_enums, headers, typedefs]


def main(argv):
    serialized_types = []
    serialized_enums = []
    typedefs = []
    headers = []
    header_set = set()
    file_extension = argv[1]
    for i in range(3, len(argv)):
        with open(argv[2] + argv[i]) as file:
            new_types, new_enums, new_headers, new_typedefs = parse_serialized_types(file, argv[i])
            for type in new_types:
                serialized_types.append(type)
            for enum in new_enums:
                serialized_enums.append(enum)
            for typedef in new_typedefs:
                typedefs.append(typedef)
            for header in new_headers:
                header_set.add(header)
    headers = sorted(header_set)

    with open('GeneratedSerializers.h', "w+") as output:
        output.write(generate_header(serialized_types, serialized_enums))
    with open('GeneratedSerializers.%s' % file_extension, "w+") as output:
        output.write(generate_impl(serialized_types, serialized_enums, headers, False))
    with open('WebKitPlatformGeneratedSerializers.%s' % file_extension, "w+") as output:
        output.write(generate_impl(serialized_types, serialized_enums, headers, True))
    with open('SerializedTypeInfo.%s' % file_extension, "w+") as output:
        output.write(generate_serialized_type_info(serialized_types, serialized_enums, headers, typedefs))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
