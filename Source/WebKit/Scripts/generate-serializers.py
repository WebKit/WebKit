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
    def __init__(self, struct_or_class, namespace, name, parent_class, members, condition, attributes):
        self.struct_or_class = struct_or_class
        self.namespace = namespace
        self.name = name
        self.parent_class = parent_class
        self.members = members
        self.condition = condition
        self.encoders = ['Encoder']
        self.serialize_with_function_calls = False
        self.return_ref = False
        self.create_using = False
        self.populate_from_empty_constructor = False
        self.nested = False
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
                else:
                    if attribute == 'Nested':
                        self.nested = True

    def namespace_and_name(self):
        if self.namespace is None:
            return self.name
        return self.namespace + '::' + self.name


class SerializedEnum(object):
    def __init__(self, namespace, name, underlying_type, valid_values, condition, attributes):
        self.namespace = namespace
        self.name = name
        self.underlying_type = underlying_type
        self.valid_values = valid_values
        self.condition = condition
        self.attributes = attributes

    def namespace_and_name(self):
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
    def __init__(self, type, name, condition, attributes):
        self.type = type
        self.name = name
        self.condition = condition
        self.attributes = attributes

    def unique_ptr_type(self):
        match = re.search(r'std::unique_ptr<(.*)>', self.type)
        if match:
            return match.group(1)
        return None


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
        result.append('namespace ' + enum.namespace + ' { enum class ' + enum.name + ' : ' + enum.underlying_type + '; }')
        if enum.condition is not None:
            result.append('#endif')
    for type in serialized_types:
        if type.nested:
            continue
        if type.condition is not None:
            result.append('#if ' + type.condition)
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
        if serialized_type.parent_class is not None:
            for possible_parent in serialized_types:
                if possible_parent.namespace_and_name() == serialized_type.parent_class:
                    serialized_type.members = possible_parent.members + serialized_type.members
                    serialized_type.parent_class = len(possible_parent.members)
                    break
        result.append(serialized_type)
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
        for encoder in type.encoders:
            result.append('')
            result.append('void ArgumentCoder<' + type.namespace_and_name() + '>::encode(' + encoder + '& encoder, const ' + type.namespace_and_name() + '& instance)')
            result.append('{')
            for member in type.members:
                if member.condition is not None:
                    result.append('#if ' + member.condition)
                if 'Nullable' in member.attributes:
                    result.append('    encoder << !!instance.' + member.name + ';')
                    result.append('    if (!!instance.' + member.name + ')')
                    result.append('        encoder << instance.' + member.name + ';')
                elif member.unique_ptr_type() is not None:
                    result.append('    encoder << !!instance.' + member.name + ';')
                    result.append('    if (!!instance.' + member.name + ')')
                    result.append('        encoder << *instance.' + member.name + ';')
                else:
                    result.append('    encoder << instance.' + member.name + ('()' if type.serialize_with_function_calls else '') + ';')
                    if 'ReturnEarlyIfTrue' in member.attributes:
                        result.append('    if (instance.' + member.name + ')')
                        result.append('        return;')
                if member.condition is not None:
                    result.append('#endif')
            result.append('}')
        result.append('')
        if type.return_ref:
            result.append('std::optional<Ref<' + type.namespace_and_name() + '>> ArgumentCoder<' + type.namespace_and_name() + '>::decode(Decoder& decoder)')
        else:
            result.append('std::optional<' + type.namespace_and_name() + '> ArgumentCoder<' + type.namespace_and_name() + '>::decode(Decoder& decoder)')
        result.append('{')
        for member in type.members:
            if member.condition is not None:
                result.append('#if ' + member.condition)
            result.append('    std::optional<' + member.type + '> ' + sanitize_string_for_variable_name(member.name) + ';')
            if 'Nullable' in member.attributes:
                result.append('    std::optional<bool> has' + member.name + ';')
                result.append('    decoder >> has' + member.name + ';')
                result.append('    if (!has' + member.name + ')')
                result.append('        return std::nullopt;')
                result.append('    if (*has' + member.name + ') {')
                result.append('        decoder >> ' + member.name + ';')
                result.append('        if (!' + member.name + ')')
                result.append('            return std::nullopt;')
                result.append('    } else')
                result.append('        ' + member.name + ' = std::optional<' + member.type + '> { ' + member.type + ' { } };')
            elif member.unique_ptr_type() is not None:
                result.append('    std::optional<bool> has' + sanitize_string_for_variable_name(member.name) + ';')
                result.append('    decoder >> has' + sanitize_string_for_variable_name(member.name) + ';')
                result.append('    if (!has' + sanitize_string_for_variable_name(member.name) + ')')
                result.append('        return std::nullopt;')
                result.append('    if (*has' + sanitize_string_for_variable_name(member.name) + ') {')
                result.append('        std::optional<' + member.unique_ptr_type() + '> contents;')
                result.append('        decoder >> contents;')
                result.append('        if (!contents)')
                result.append('            return std::nullopt;')
                result.append('        ' + sanitize_string_for_variable_name(member.name) + '= makeUnique<' + member.unique_ptr_type() + '>(WTFMove(*contents));')
                result.append('    } else')
                result.append('        ' + sanitize_string_for_variable_name(member.name) + ' = std::optional<' + member.type + '> { ' + member.type + ' { } };')
            else:
                result.append('    decoder >> ' + sanitize_string_for_variable_name(member.name) + ';')
                result.append('    if (!' + sanitize_string_for_variable_name(member.name) + ')')
                result.append('        return std::nullopt;')
                if 'ReturnEarlyIfTrue' in member.attributes:
                    result.append('    if (*' + sanitize_string_for_variable_name(member.name) + ')')
                    result.append('        return { ' + type.namespace_and_name() + ' { } };')
            if member.condition is not None:
                result.append('#endif')
            result.append('')
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
            if type.return_ref:
                result.append('    return { ' + type.namespace_and_name() + '::create(')
            elif type.create_using:
                result.append('    return { ' + type.namespace_and_name() + '::' + type.create_using + '(')
            else:
                result.append('    return { ' + type.namespace_and_name() + ' {')
            if type.parent_class is not None:
                result.append('        {')
            for i in range(len(type.members)):
                member = type.members[i]
                if type.members[i].condition is not None:
                    result.append('#if ' + member.condition)
                additional_indentation = ('    ' if type.parent_class is not None and type.parent_class > i else '')
                result.append(additional_indentation + '        WTFMove(*' + sanitize_string_for_variable_name(member.name) + ')' + ('' if i == len(type.members) - 1 else ','))
                if member.condition is not None:
                    result.append('#endif')
                if type.parent_class == i + 1:
                    result.append('        },')
            if type.return_ref or type.create_using:
                result.append('    ) };')
            else:
                result.append('    } };')
        result.append('}')
        if type.condition is not None:
            result.append('')
            result.append('#endif')
    result.append('')
    result.append('} // namespace IPC')
    result.append('')
    result.append('namespace WTF {')
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


def generate_serialized_type_info(serialized_types, serialized_enums, headers):
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
        result.append('        { "' + type.namespace_and_name() + '"_s, {')
        for member in type.members:
            result.append('            "' + member.type + '"_s,')
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
    headers = []

    attributes = None
    namespace = None
    name = None
    members = []
    type_condition = None
    member_condition = None
    struct_or_class = None
    underlying_type = None
    parent_class = None
    file_extension = "cpp"

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
                serialized_types.append(SerializedType(struct_or_class, namespace, name, parent_class, members, type_condition, attributes))
                if namespace is not None and (attributes is None or 'CustomHeader' not in attributes and 'Nested' not in attributes):
                    if namespace == 'WebKit':
                        headers.append(ConditionalHeader('"' + name + '.h"', type_condition))
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
            parent_class = None
            continue

        match = re.search(r'headers?: (.*)', line)
        if match:
            for header in match.group(1).split():
                headers.append(ConditionalHeader(header, type_condition))
            continue
        match = re.search(r'file_extension?: (.*)', line)
        if match:
            file_extension = match.groups()
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

        match = re.search(r'\[(.*)\] (struct|class) (.*)::(.*) : (.*) {', line)
        if match:
            attributes, struct_or_class, namespace, name, parent_class = match.groups()
            continue
        match = re.search(r'(struct|class) (.*)::(.*) : (.*) {', line)
        if match:
            struct_or_class, namespace, name, parent_class = match.groups()
            continue
        match = re.search(r'\[(.*)\] (struct|class) (.*)::(.*) {', line)
        if match:
            attributes, struct_or_class, namespace, name = match.groups()
            continue
        match = re.search(r'(struct|class) (.*)::(.*) {', line)
        if match:
            struct_or_class, namespace, name = match.groups()
            continue
        match = re.search(r'\[(.*)\] (struct|class) (.*) {', line)
        if match:
            attributes, struct_or_class, name = match.groups()
            continue
        match = re.search(r'(struct|class) (.*) {', line)
        if match:
            struct_or_class, name = match.groups()
            continue

        if underlying_type is not None:
            members.append(EnumMember(line.strip(' ,'), member_condition))
            continue

        match = re.search(r'\[(.*)\] (.*) ([^;]*)', line)
        if match:
            attribute, member_type, member_name = match.groups()
            members.append(MemberVariable(member_type, member_name, member_condition, [attribute]))
        else:
            match = re.search(r'(.*) ([^;]*)', line)
            if match:
                member_type, member_name = match.groups()
                members.append(MemberVariable(member_type, member_name, member_condition, []))
    return [serialized_types, serialized_enums, headers, file_extension]


def main(argv):
    serialized_types = {}
    serialized_enums = {}
    headers = {}
    file_extensions = set()
    for i in range(2, len(argv)):
        with open(argv[1] + argv[i]) as file:
            new_types, new_enums, new_headers, file_extension = parse_serialized_types(file, argv[i])
            file_extensions.add(file_extension)
            for type in new_types:
                serialized_types.setdefault(file_extension, []).append(type)
            for enum in new_enums:
                serialized_enums.setdefault(file_extension, []).append(enum)
            for header in new_headers:
                headers.setdefault(file_extension, []).append(header)
    [v.sort() for v in headers.values()]

    with open('GeneratedSerializers.h', "w+") as header_output:
        header_output.write(generate_header(sum(serialized_types.values(), []), sum(serialized_enums.values(), [])))
    for file_extension in file_extensions:
        with open('GeneratedSerializers.%s' % file_extension, "w+") as output:
            output.write(generate_impl(serialized_types.get(file_extension, []), serialized_enums.get(file_extension, []), headers.get(file_extension, [])))
    with open('SerializedTypeInfo.cpp', "w+") as cpp_output:
        cpp_output.write(generate_serialized_type_info(sum(serialized_types.values(), []), serialized_enums.get("cpp", []), headers.get("cpp", [])))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
