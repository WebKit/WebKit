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
        self.members_are_subclasses = False
        if attributes is not None:
            for attribute in attributes.split(', '):
                if '=' in attribute:
                    key, value = attribute.split('=')
                    if key == 'AdditionalEncoder':
                        self.encoders.append(value)
                    if key == 'Return' and value == 'Ref':
                        self.return_ref = True
                    if key == 'CreateUsing':
                        self.create_using = value
                    if key == 'LegacyPopulateFrom' and value == 'EmptyConstructor':
                        self.populate_from_empty_constructor = True
                    if key == 'Alias':
                        self.alias = value
                else:
                    if attribute == 'Nested':
                        self.nested = True
                    elif attribute == 'RefCounted':
                        self.return_ref = True
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


class EnumMember(object):
    def __init__(self, name, condition):
        self.name = name
        self.condition = condition

class ConditionalHeader(object):
    def __init__(self, header, condition):
        self.header = header
        self.condition = condition

    def __lt__(self, other):
        return self.header < other.header


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
    result.append('#include <wtf/ArgumentCoder.h>')
    result.append('#include <wtf/OptionSet.h>')
    result.append('#include <wtf/Ref.h>')
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


def check_type_members(type):
    result = []
    if type.parent_class is not None:
        result = result + check_type_members(type.parent_class)
    for member in type.members:
        if member.condition is not None:
            result.append('#if ' + member.condition)
        result.append('    static_assert(std::is_same_v<std::remove_cvref_t<decltype(instance.' + member.name + ')>, ' + member.type + '>);')
        if member.condition is not None:
            result.append('#endif')
    return result


def encode_type(type):
    result = []
    if type.parent_class is not None:
        result = result + encode_type(type.parent_class)
    for member in type.members:
        if member.condition is not None:
            result.append('#if ' + member.condition)
        if 'Nullable' in member.attributes:
            result.append('    encoder << !!instance.' + member.name + ';')
            result.append('    if (!!instance.' + member.name + ')')
            result.append('        encoder << instance.' + member.name + ';')
        elif member.is_subclass:
            result.append('    if (auto* subclass = dynamicDowncast<' + member.namespace + "::" + member.name + '>(instance)) {')
            result.append('        encoder << ' + type.subclass_enum_name() + "::" + member.name + ";")
            result.append('        encoder << *subclass;')
            result.append('    }')
        else:
            result.append('    encoder << instance.' + member.name + ('()' if type.serialize_with_function_calls else '') + ';')
            if 'ReturnEarlyIfTrue' in member.attributes:
                result.append('    if (instance.' + member.name + ')')
                result.append('        return;')
        if member.condition is not None:
            result.append('#endif')

    return result


def decode_type(type):
    result = []
    if type.parent_class is not None:
        result = result + decode_type(type.parent_class)

    if type.members_are_subclasses:
        result.append('    std::optional<' + type.subclass_enum_name() + '> type;')
        result.append('    decoder >> type;')
        result.append('    if (!type)')
        result.append('        return std::nullopt;')
        result.append('')

    for member in type.members:
        if member.condition is not None:
            result.append('#if ' + member.condition)
        sanitized_variable_name = sanitize_string_for_variable_name(member.name)
        r = re.compile("SecureCodingAllowed=\\[(.*)\\]")
        decodable_classes = [r.match(m).groups()[0] for m in list(filter(r.match, member.attributes))]
        if len(decodable_classes) == 1:
            match = re.search("RetainPtr<(.*)>", member.type)
            assert match
            result.append('    auto ' + sanitized_variable_name + ' = IPC::decode<' + match.groups()[0] + '>(decoder, @[ ' + decodable_classes[0] + ' ]);')
            result.append('    if (!' + sanitized_variable_name + ')')
            result.append('        return std::nullopt;')
            if 'ReturnEarlyIfTrue' in member.attributes:
                result.append('    if (*' + sanitized_variable_name + ')')
                result.append('        return { ' + type.namespace_and_name() + ' { } };')
        elif member.is_subclass:
            result.append('    if (type == ' + type.subclass_enum_name() + "::" + member.name + ') {')
            typename = member.namespace + "::" + member.name
            result.append('        std::optional<Ref<' + typename + '>> result;')
            result.append('        decoder >> result;')
            result.append('        if (!result)')
            result.append('            return std::nullopt;')
            result.append('        return WTFMove(*result);')
            result.append('    }')
        else:
            assert len(decodable_classes) == 0
            r = re.compile(r"SoftLinkedClass='(.*)'")
            soft_linked_classes = [r.match(m).groups()[0] for m in list(filter(r.match, member.attributes))]
            if len(soft_linked_classes) == 1:
                match = re.search("RetainPtr<(.*)>", member.type)
                assert match
                indentation = 1
                if 'Nullable' in member.attributes:
                    indentation = 2
                    result.append('    auto has' + sanitized_variable_name + ' = decoder.decode<bool>();')
                    result.append('    if (!has' + sanitized_variable_name + ')')
                    result.append('        return std::nullopt;')
                    result.append('    std::optional<' + member.type + '> ' + sanitized_variable_name + ';')
                    result.append('    if (*has' + sanitized_variable_name + ') {')
                auto_specifier = '' if 'Nullable' in member.attributes else 'auto '
                result.append(indent(indentation) + auto_specifier + sanitized_variable_name + ' = IPC::decode<' + match.groups()[0] + '>(decoder, ' + soft_linked_classes[0] + ');')
                result.append(indent(indentation) + 'if (!' + sanitized_variable_name + ')')
                result.append(indent(indentation) + '    return std::nullopt;')
                if 'Nullable' in member.attributes:
                    result.append('    } else')
                    result.append('        ' + sanitized_variable_name + ' = std::optional<' + member.type + '> { ' + member.type + ' { } };')
                elif 'ReturnEarlyIfTrue' in member.attributes:
                    result.append('    if (*' + sanitized_variable_name + ')')
                    result.append('        return { ' + type.namespace_and_name() + ' { } };')
            elif 'Nullable' in member.attributes:
                result.append('    auto has' + sanitized_variable_name + ' = decoder.decode<bool>();')
                result.append('    if (!has' + sanitized_variable_name + ')')
                result.append('        return std::nullopt;')
                result.append('    std::optional<' + member.type + '> ' + sanitized_variable_name + ';')
                result.append('    if (*has' + sanitized_variable_name + ') {')
                # FIXME: This should be below
                result.append('        ' + sanitized_variable_name + ' = decoder.decode<' + member.type + '>();')
                result.append('        if (!' + sanitized_variable_name + ')')
                result.append('            return std::nullopt;')
                result.append('    } else')
                result.append('        ' + sanitized_variable_name + ' = std::optional<' + member.type + '> { ' + member.type + ' { } };')
            else:
                assert len(soft_linked_classes) == 0
                result.append('    auto ' + sanitized_variable_name + ' = decoder.decode<' + member.type + '>();')
                result.append('    if (!' + sanitized_variable_name + ')')
                result.append('        return std::nullopt;')
                if 'ReturnEarlyIfTrue' in member.attributes:
                    result.append('    if (*' + sanitized_variable_name + ')')
                    result.append('        return { ' + type.namespace_and_name() + ' { } };')
        for attribute in member.attributes:
            match = re.search(r'Validator=\'(.*)\'', attribute)
            if match:
                validator, = match.groups()
                result.append('')
                result.append('    if (!(' + validator + '))')
                result.append('        return std::nullopt;')
                continue
            else:
                match = re.search(r'Validator', attribute)
                assert not match
        if member.condition is not None:
            result.append('#endif')
        result.append('')
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
    for i in range(len(type.members)):
        member = type.members[i]
        if type.members[i].condition is not None:
            result.append('#if ' + member.condition)
        result.append(indent(indentation + 1) + 'WTFMove(*' + sanitize_string_for_variable_name(member.name) + ')' + ('' if i == len(type.members) - 1 else ','))
        if member.condition is not None:
            result.append('#endif')
    if type.create_using or type.return_ref:
        result.append(indent(indentation) + ')')
    else:
        result.append(indent(indentation) + '}')
    return result


def generate_impl(serialized_types, serialized_enums, headers):
    serialized_types = resolve_inheritance(serialized_types)
    result = []
    result.append(_license_header)
    result.append('#include "config.h"')
    result.append('#include "GeneratedSerializers.h"')
    result.append('')
    for header in headers:
        if header.condition is not None:
            result.append('#if ' + header.condition)
        result.append('#include ' + header.header)
        if header.condition is not None:
            result.append('#endif')
    result.append('')
    result.append('namespace IPC {')
    result.append('')
    result = result + argument_coder_declarations(serialized_types, False)
    result.append('')
    for type in serialized_types:
        result.append('')
        if type.condition is not None:
            result.append('#if ' + type.condition)

        if type.members_are_subclasses:
            result.append('enum class ' + type.subclass_enum_name() + " : uint8_t {")
            for idx in range(0, len(type.members)):
                member = type.members[idx]
                if idx == len(type.members) - 1:
                    result.append('    ' + member.name)
                else:
                    result.append('    ' + member.name + ',')
            result.append('};')
        for encoder in type.encoders:
            result.append('')
            result.append('void ArgumentCoder<' + type.namespace_and_name() + '>::encode(' + encoder + '& encoder, const ' + type.namespace_and_name() + '& instance)')
            result.append('{')
            if not type.members_are_subclasses:
                result = result + check_type_members(type)
            result = result + encode_type(type)
            result.append('}')
        result.append('')
        if type.return_ref:
            result.append('std::optional<Ref<' + type.namespace_and_name() + '>> ArgumentCoder<' + type.namespace_and_name() + '>::decode(Decoder& decoder)')
        else:
            result.append('std::optional<' + type.namespace_and_name() + '> ArgumentCoder<' + type.namespace_and_name() + '>::decode(Decoder& decoder)')
        result.append('{')
        result = result + decode_type(type)
        if not type.members_are_subclasses:
            if type.populate_from_empty_constructor:
                result.append('    ' + type.namespace_and_name() + ' result;')
                for member in type.members:
                    if member.condition is not None:
                        result.append('#if ' + member.condition)
                    result.append('    result.' + member.name + ' = WTFMove(*' + member.name + ');')
                    if member.condition is not None:
                        result.append('#endif')
                result.append('    return { WTFMove(result) };')
            else:
                result.append('    return {')
                result = result + construct_type(type, 2)
                result.append('    };')
        else:
            result.append('    ASSERT_NOT_REACHED();')
            result.append('    return std::nullopt;')
        result.append('}')
        if type.condition is not None:
            result.append('')
            result.append('#endif')
    result.append('')
    result.append('} // namespace IPC')
    result.append('')
    result.append('namespace WTF {')
    for type in serialized_types:
        if not type.members_are_subclasses:
            continue
        result.append('')
        if type.condition is not None:
            result.append('#if ' + type.condition)
        result.append('template<> bool ' + type.function_name_for_enum() + '<IPC::' + type.subclass_enum_name() + ', void>(uint8_t value)')
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
        if enum.underlying_type == 'bool':
            continue
        result.append('')
        if enum.condition is not None:
            result.append('#if ' + enum.condition)
        result.append('template<> bool ' + enum.function_name() + '<' + enum.namespace_and_name() + enum.additional_template_parameter() + '>(' + enum.parameter() + ' value)')
        result.append('{')
        if enum.is_option_set():
            result.append('    constexpr ' + enum.underlying_type + ' allValidBitsValue =')
            for i in range(0, len(enum.valid_values)):
                valid_value = enum.valid_values[i]
                if valid_value.condition is not None:
                    result.append('#if ' + valid_value.condition)
                result.append('        ' + ('' if i == 0 else '| ') + 'static_cast<' + enum.underlying_type + '>(' + enum.namespace_and_name() + '::' + valid_value.name + ')' + (';' if i == len(enum.valid_values) - 1 else ''))
                if valid_value.condition is not None:
                    result.append('#endif')
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
    return '\n'.join(result)


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
        if type.members_are_subclasses:
            continue
        result.append('        { "' + type.namespace_unless_wtf_and_name() + '"_s, {')
        for i in range(len(type.members)):
            if i == 0:
                result.append('            {')
            result.append('                "' + type.members[i].type + '"_s,')
            result.append('                "' + type.members[i].name + '"_s')
            if i == len(type.members) - 1:
                result.append('            }')
            else:
                result.append('            }, {')
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
                serialized_types.append(SerializedType(struct_or_class, namespace, name, parent_class_name, members, type_condition, attributes, metadata))
                if namespace is not None and (attributes is None or 'CustomHeader' not in attributes and 'Nested' not in attributes):
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

        match = re.search(r'headers?: (.*)', line)
        if match:
            for header in match.group(1).split():
                headers.append(ConditionalHeader(header, type_condition))
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
            match = re.search(r"((, |^)+(SoftLinkedClass='.*?'))(, |$)?", member_attributes_s)
            if match:
                complete, _, soft_linked_class, _ = match.groups()
                member_attributes.append(soft_linked_class)
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
                headers.append(header)
    headers.sort()

    with open('GeneratedSerializers.h', "w+") as output:
        output.write(generate_header(serialized_types, serialized_enums))
    with open('GeneratedSerializers.%s' % file_extension, "w+") as output:
        output.write(generate_impl(serialized_types, serialized_enums, headers))
    with open('SerializedTypeInfo.%s' % file_extension, "w+") as output:
        output.write(generate_serialized_type_info(serialized_types, serialized_enums, headers, typedefs))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
