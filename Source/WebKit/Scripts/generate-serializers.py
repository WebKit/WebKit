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
    def __init__(self, headers, struct_or_class, namespace, name, members, condition, encoders):
        self.headers = headers
        self.struct_or_class = struct_or_class
        self.namespace = namespace
        self.name = name
        self.members = members
        self.condition = condition
        self.encoders = encoders

    def namespace_and_name(self):
        return self.namespace + '::' + self.name

    def encoder_types(self):
        if self.encoders is not None:
            return self.encoders
        return ['Encoder']

class MemberVariable(object):
    def __init__(self, type, name, condition, attributes):
        self.type = type
        self.name = name
        self.condition = condition
        self.attributes = attributes


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
    result.append('#include "ArgumentCoders.h"')
    result.append('')
    for type in serialized_types:
        result.append('namespace ' + type.namespace + ' { ' + type.struct_or_class + ' ' + type.name + '; }')
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
        for encoder in type.encoder_types():
            result.append('    static void encode(' + encoder + '&, const ' + type.namespace_and_name() + '&);')
        result.append('    static std::optional<' + type.namespace_and_name() + '> decode(Decoder&);')
        result.append('};')
        if type.condition is not None:
            result.append('#endif // ' + type.condition)
    result.append('')
    result.append('} // namespace IPC\n')
    return '\n'.join(result)


def generate_cpp(serialized_types):
    result = []
    result.append(_license_header)
    result.append('#include "config.h"')
    result.append('#include "GeneratedSerializers.h"')
    result.append('')
    result.append('#include "StreamConnectionEncoder.h"')
    for type in serialized_types:
        if type.condition is not None:
            result.append('')
            result.append('#if ' + type.condition)
        for header in type.headers:
            result.append('#include ' + header)
        if type.condition is not None:
            result.append('#endif // ' + type.condition)
    result.append('')
    result.append('namespace IPC {')
    for type in serialized_types:
        result.append('')
        if type.condition is not None:
            result.append('#if ' + type.condition)
        for encoder in type.encoder_types():
            result.append('')
            result.append('void ArgumentCoder<' + type.namespace_and_name() + '>::encode(' + encoder + '& encoder, const ' + type.namespace_and_name() + '& instance)')
            result.append('{')
            for member in type.members:
                if member.condition is not None:
                    result.append('#if ' + member.condition)
                if 'NULLABLE' in member.attributes:
                    result.append('    encoder << !!instance.' + member.name + ';')
                    result.append('    if (!!instance.' + member.name + ')')
                    result.append('        encoder << instance.' + member.name + ';')
                else:
                    result.append('    encoder << instance.' + member.name + ';')
                if member.condition is not None:
                    result.append('#endif // ' + member.condition)
            result.append('}')
        result.append('')
        result.append('std::optional<' + type.namespace_and_name() + '> ArgumentCoder<' + type.namespace_and_name() + '>::decode(Decoder& decoder)')
        result.append('{')
        for member in type.members:
            if member.condition is not None:
                result.append('#if ' + member.condition)
            result.append('    std::optional<' + member.type + '> ' + member.name + ';')
            if 'NULLABLE' in member.attributes:
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
            else:
                result.append('    decoder >> ' + member.name + ';')
                result.append('    if (!' + member.name + ')')
                result.append('        return std::nullopt;')
            if member.condition is not None:
                result.append('#endif // ' + member.condition)
            result.append('')
        result.append('    return { {')
        first_member = True
        for i in range(len(type.members)):
            if type.members[i].condition is not None:
                result.append('#if ' + type.members[i].condition)
            result.append('        WTFMove(*' + type.members[i].name + ')' + ('' if i == len(type.members) - 1 else ','))
            if type.members[i].condition is not None:
                result.append('#endif // ' + type.members[i].condition)
        result.append('    } };')
        result.append('}')
        if type.condition is not None:
            result.append('')
            result.append('#endif // ' + type.condition)
    result.append('')
    result.append('} // namespace IPC\n')
    return '\n'.join(result)


def parse_serialized_type(file):
    namespace = None
    name = None
    headers = None
    members = []
    type_condition = None
    member_condition = None
    struct_or_class = None
    encoders = None
    for line in file:
        line = line.strip()
        if line.startswith('#'):
            if line.startswith('#if '):
                if name is None:
                    type_condition = line[4:]
                else:
                    member_condition = line[4:]
            elif line.startswith('#endif'):
                member_condition = None
            continue
        if line.startswith('}'):
            if headers is None:
                headers = ['"' + name + '.h"']
            headers.sort()
            return SerializedType(headers, struct_or_class, namespace, name, members, type_condition, encoders)
        match = re.search(r'headers: (.*)', line)
        if match:
            headers = match.group(1).split()
            continue
        match = re.search(r'encoders: (.*)', line)
        if match:
            encoders = match.group(1).split()
            continue
        match = re.search(r'(struct|class) (.*)::(.*) {', line)
        if match:
            struct_or_class, namespace, name = match.groups()
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
    return None


def main(argv):
    descriptions = []
    for i in range(2, len(argv)):
        with open(argv[1] + argv[i]) as file:
            descriptions.append(parse_serialized_type(file))

    with open('GeneratedSerializers.h', "w+") as header_output:
        header_output.write(generate_header(descriptions))
    with open('GeneratedSerializers.cpp', "w+") as cpp_output:
        cpp_output.write(generate_cpp(descriptions))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
