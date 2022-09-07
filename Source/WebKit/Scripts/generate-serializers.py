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
    def __init__(self, struct_or_class, namespace, name, members, condition, attributes):
        self.struct_or_class = struct_or_class
        self.namespace = namespace
        self.name = name
        self.members = members
        self.condition = condition
        self.encoders = ['Encoder']
        self.serialize_with_function_calls = False
        self.return_ref = False
        self.populate_from_empty_constructor = False
        if attributes is not None:
            for attribute in attributes.split(', '):
                key, value = attribute.split('=')
                if key == 'AdditionalEncoder':
                    self.encoders.append(value)
                if key == 'Return' and value == 'Ref':
                    self.return_ref = True
                if key == 'LegacyPopulateFrom' and value == 'EmptyConstructor':
                    self.populate_from_empty_constructor = True

    def namespace_and_name(self):
        if self.namespace is None:
            return self.name
        return self.namespace + '::' + self.name


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


def generate_header(serialized_types):
    result = []
    result.append(_license_header)
    result.append('#pragma once')
    result.append('')
    result.append('#include <wtf/ArgumentCoder.h>')
    result.append('#include <wtf/Ref.h>')
    result.append('')
    for type in serialized_types:
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
    for type in serialized_types:
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
    result.append('')
    result.append('} // namespace IPC\n')
    return '\n'.join(result)


def generate_cpp(serialized_types, headers):
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
                result.append('    result.' + member.name + ' = WTFMove(*' + member.name + ');')
            result.append('    return { WTFMove(result) };')
        else:
            if type.return_ref:
                result.append('    return { ' + type.namespace_and_name() + '::create(')
            else:
                result.append('    return { ' + type.namespace_and_name() + ' {')
            for i in range(len(type.members)):
                if type.members[i].condition is not None:
                    result.append('#if ' + type.members[i].condition)
                result.append('        WTFMove(*' + sanitize_string_for_variable_name(type.members[i].name) + ')' + ('' if i == len(type.members) - 1 else ','))
                if type.members[i].condition is not None:
                    result.append('#endif')
            if type.return_ref:
                result.append('    ) };')
            else:
                result.append('    } };')
        result.append('}')
        if type.condition is not None:
            result.append('')
            result.append('#endif')
    result.append('')
    result.append('} // namespace IPC\n')
    return '\n'.join(result)


def generate_serialized_type_info(serialized_types):
    result = []
    result.append(_license_header)
    result.append('#include "config.h"')
    result.append('#include "SerializedTypeInfo.h"')
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
    result.append('} // namespace WebKit')
    result.append('')
    result.append('#endif // ENABLE(IPC_TESTING_API)')
    result.append('')
    return '\n'.join(result)


def parse_serialized_types(file, file_name):
    serialized_types = []
    headers = []

    attributes = None
    namespace = None
    name = None
    members = []
    type_condition = None
    member_condition = None
    struct_or_class = None

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
            serialized_types.append(SerializedType(struct_or_class, namespace, name, members, type_condition, attributes))
            attributes = None
            namespace = None
            name = None
            members = []
            member_condition = None
            struct_or_class = None
            continue

        match = re.search(r'headers? requiring (.*): (.*)', line)
        if match:
            condition, header_names = match.groups()
            for header in header_names.split():
                headers.append(ConditionalHeader(header, condition))
            continue
        match = re.search(r'headers?: (.*)', line)
        if match:
            for header in match.group(1).split():
                headers.append(ConditionalHeader(header, None))
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

        match = re.search(r'\[(.*)\] (.*) ([^;]*)', line)
        if match:
            attribute, member_type, member_name = match.groups()
            members.append(MemberVariable(member_type, member_name, member_condition, [attribute]))
        else:
            match = re.search(r'(.*) ([^;]*)', line)
            if match:
                member_type, member_name = match.groups()
                members.append(MemberVariable(member_type, member_name, member_condition, []))
    if len(headers) == 0 and len(serialized_types) == 1:
        headers = [ConditionalHeader('"' + file_name[0:len(file_name) - len('.serialization.in')] + '.h"', None)]
    return [serialized_types, headers]


def main(argv):
    serialized_types = []
    headers = []
    for i in range(2, len(argv)):
        with open(argv[1] + argv[i]) as file:
            new_types, new_headers = parse_serialized_types(file, argv[i])
            for type in new_types:
                serialized_types.append(type)
            for header in new_headers:
                headers.append(header)
    headers.sort()

    with open('GeneratedSerializers.h', "w+") as header_output:
        header_output.write(generate_header(serialized_types))
    with open('GeneratedSerializers.cpp', "w+") as cpp_output:
        cpp_output.write(generate_cpp(serialized_types, headers))
    with open('SerializedTypeInfo.cpp', "w+") as cpp_output:
        cpp_output.write(generate_serialized_type_info(serialized_types))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
