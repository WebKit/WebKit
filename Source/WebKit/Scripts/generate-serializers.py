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
    def __init__(self, header, namespace, name, members, conditions):
        self.header = header
        self.namespace = namespace
        self.name = name
        self.members = members
        self.conditions = conditions

    def namespace_and_name(self):
        return self.namespace + '::' + self.name


class MemberVariable(object):
    def __init__(self, type, name):
        self.type = type
        self.name = name


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
        result.append('namespace ' + type.namespace + ' { struct ' + type.name + '; }')
    result.append('')
    result.append('namespace IPC {')
    result.append('')
    result.append('class Decoder;')
    result.append('class Encoder;')
    result.append('class StreamConnectionEncoder;')
    for type in serialized_types:
        result.append('')
        for condition in type.conditions:
            result.append('#if ' + condition)
        result.append('template<> struct ArgumentCoder<' + type.namespace_and_name() + '> {')
        for encoder in ['Encoder', 'StreamConnectionEncoder']:
            result.append('    static void encode(' + encoder + '&, const ' + type.namespace_and_name() + '&);')
        result.append('    static std::optional<' + type.namespace_and_name() + '> decode(Decoder&);')
        result.append('};')
        for condition in type.conditions:
            result.append('#endif // ' + condition)
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
        for condition in type.conditions:
            result.append('')
            result.append('#if ' + condition)
        result.append('#include ' + type.header)
        for condition in type.conditions:
            result.append('#endif // ' + condition)
    result.append('')
    result.append('namespace IPC {')
    for type in serialized_types:
        result.append('')
        for condition in type.conditions:
            result.append('#if ' + condition)
        for encoder in ['Encoder', 'StreamConnectionEncoder']:
            result.append('')
            result.append('void ArgumentCoder<' + type.namespace_and_name() + '>::encode(' + encoder + '& encoder, const ' + type.namespace_and_name() + '& instance)')
            result.append('{')
            for member in type.members:
                result.append('    encoder << instance.' + member.name + ';')
            result.append('}')
        result.append('')
        result.append('std::optional<' + type.namespace_and_name() + '> ArgumentCoder<' + type.namespace_and_name() + '>::decode(Decoder& decoder)')
        result.append('{')
        for member in type.members:
            result.append('    std::optional<' + member.type + '> ' + member.name + ';')
            result.append('    decoder >> ' + member.name + ';')
            result.append('    if (!' + member.name + ')')
            result.append('        return std::nullopt;')
            result.append('')
        result.append('    return { {')
        first_member = True
        for i in range(len(type.members)):
            result.append('        WTFMove(*' + type.members[i].name + ')' + ('' if i == len(type.members) - 1 else ','))
        result.append('    } };')
        result.append('}')
        for condition in type.conditions:
            result.append('')
            result.append('#endif // ' + condition)
    result.append('')
    result.append('} // namespace IPC\n')
    return '\n'.join(result)


def parse_serialized_type(file):
    namespace = None
    name = None
    header = None
    members = []
    conditions = []
    for line in file:
        line = line.strip()
        if line.startswith('#'):
            if line.startswith('#if '):
                conditions.append(line[4:])
            elif line.startswith('#endif') and conditions:
                conditions.pop()
            continue
        if line.startswith('}'):
            return SerializedType(header, namespace, name, members, conditions)
        match = re.search(r'header: (.*)', line)
        if match:
            header = match.group(1)
            continue
        match = re.search(r'struct (.*)::(.*) {', line)
        if match:
            namespace, name = match.groups()
            continue
        match = re.search(r'(.*) (.*)', line)
        if match:
            member_type, member_name = match.groups()
            members.append(MemberVariable(member_type, member_name))
    return None


def main(argv):
    descriptions = []
    for i in range(1, len(argv)):
        with open(argv[i]) as file:
            descriptions.append(parse_serialized_type(file))

    with open('GeneratedSerializers.h', "w+") as header_output:
        header_output.write(generate_header(descriptions))
    with open('GeneratedSerializers.cpp', "w+") as cpp_output:
        cpp_output.write(generate_cpp(descriptions))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
