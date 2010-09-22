# Copyright (C) 2010 Apple Inc. All rights reserved.
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

import collections
import re


_license_header = """/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

class MessageReceiver(object):
    def __init__(self, name, messages):
        self.name = name
        self.messages = messages

    def iterparameters(self):
        return (parameter for message in self.messages for parameter in message.parameters)

    @classmethod
    def parse(cls, file):
        destination = None
        messages = []
        condition = None
        for line in file:
            match = re.search(r'messages -> ([A-Za-z_0-9]+) {', line)
            if match:
                destination = match.group(1)
                continue
            if line.startswith('#'):
                if line.startswith('#if '):
                    condition = line.rstrip()[4:]
                elif line.startswith('#endif'):
                    condition = None
                continue
            match = re.search(r'([A-Za-z_0-9]+)\((.*)\)', line)
            if match:
                name, parameters_string = match.groups()
                if parameters_string:
                    parameters = [Parameter(*type_and_name.split(' ')) for type_and_name in parameters_string.split(', ')]
                else:
                    parameters = []
                messages.append(Message(name, parameters, condition))
        return MessageReceiver(destination, messages)


class Message(object):
    def __init__(self, name, parameters, condition):
        self.name = name
        self.parameters = parameters
        self.condition = condition

    def id(self):
        return '%sID' % self.name


class Parameter(object):
    def __init__(self, type, name):
        self.type = type
        self.name = name


def messages_header_filename(receiver):
    return '%sMessages.h' % receiver.name


def surround_in_condition(string, condition):
    if not condition:
        return string
    return '#if %s\n%s#endif\n' % (condition, string)


def messages_to_kind_enum(messages):
    result = []
    result.append('enum Kind {\n')
    result += [surround_in_condition('    %s,\n' % message.id(), message.condition) for message in messages]
    result.append('};\n')
    return ''.join(result)


def function_parameter_type(type):
    # We assume that we must use a reference for a type iff it contains a scope
    # resolution operator (::).
    if re.search(r'::', type):
        return 'const %s&' % type
    return type


def base_class(message):
    base_class = 'CoreIPC::Arguments%d' % len(message.parameters)
    if len(message.parameters):
        base_class = '%s<%s>' % (base_class, ', '.join(function_parameter_type(parameter.type) for parameter in message.parameters))
    return base_class


def message_to_struct_declaration(message):
    result = []
    function_parameters = [(function_parameter_type(x.type), x.name) for x in message.parameters]
    result.append('struct %s : %s' % (message.name, base_class(message)))
    result.append(' {\n')
    result.append('    static const Kind messageID = %s;\n' % message.id())
    if len(function_parameters):
        result.append('    %s%s(%s)' % (len(function_parameters) == 1 and 'explicit ' or '', message.name, ', '.join([' '.join(x) for x in function_parameters])))
        result.append('\n        : %s(%s)\n' % (base_class(message), ', '.join([x[1] for x in function_parameters])))
        result.append('    {\n')
        result.append('    }\n')
    result.append('};\n')
    return surround_in_condition(''.join(result), message.condition)


def forward_declarations_for_namespace(namespace, types):
    result = []
    result.append('namespace %s {\n' % namespace)
    result += ['    class %s;\n' % x for x in types]
    result.append('}\n')
    return ''.join(result)


def forward_declarations(receiver):
    types_by_namespace = collections.defaultdict(set)
    for parameter in receiver.iterparameters():
        type = parameter.type
        split = type.split('::')
        if len(split) != 2:
            continue
        namespace = split[0]
        inner_type = split[1]
        types_by_namespace[namespace].add(inner_type)
    return '\n'.join([forward_declarations_for_namespace(namespace, types) for (namespace, types) in sorted(types_by_namespace.iteritems())])


def generate_messages_header(file):
    receiver = MessageReceiver.parse(file)
    header_guard = messages_header_filename(receiver).replace('.', '_')

    headers = set([
        '"Arguments.h"',
        '"MessageID.h"',
    ])

    result = []

    result.append(_license_header)
    result.append('\n')

    result.append('#ifndef %s\n' % header_guard)
    result.append('#define %s\n\n' % header_guard)

    result += ['#include %s\n' % header for header in sorted(headers)]
    result.append('\n')

    result.append(forward_declarations(receiver))
    result.append('\n')

    result.append('namespace Messages {\n\nnamespace %s {\n\n' % receiver.name)
    result.append(messages_to_kind_enum(receiver.messages))
    result.append('\n')
    result.append('\n'.join([message_to_struct_declaration(x) for x in receiver.messages]))
    result.append('\n} // namespace %s\n\n} // namespace Messages\n' % receiver.name)

    result.append('\nnamespace CoreIPC {\n\n')
    result.append('template<> struct MessageKindTraits<Messages::%s::Kind> {\n' % receiver.name)
    result.append('    static const MessageClass messageClass = MessageClass%s;\n' % receiver.name)
    result.append('};\n')
    result.append('\n} // namespace CoreIPC\n')

    result.append('\n#endif // %s\n' % header_guard)
    return ''.join(result)


def handler_function(receiver, message):
    return '%s::%s' % (receiver.name, message.name[0].lower() + message.name[1:])


def case_statement(receiver, message):
    result = []
    result.append('    case Messages::%s::%s:\n' % (receiver.name, message.id()))
    result.append('        CoreIPC::handleMessage<Messages::%s::%s>(arguments, this, &%s);\n' % (receiver.name, message.name, handler_function(receiver, message)))
    result.append('        return;\n')
    return surround_in_condition(''.join(result), message.condition)


def header_for_type(type):
    special_cases = {
        'WTF::String': '<wtf/text/WTFString.h>',
        'WebKit::WebKeyboardEvent': '"WebEvent.h"',
        'WebKit::WebMouseEvent': '"WebEvent.h"',
        'WebKit::WebWheelEvent': '"WebEvent.h"',
        'WebKit::WebTouchEvent': '"WebEvent.h"',
    }
    if type in special_cases:
        return special_cases[type]

    # We assume that we must include a header for a type iff it has a scope
    # resolution operator (::).
    split = type.split('::')
    if len(split) != 2:
        return None
    if split[0] == 'WebKit':
        return '"%s.h"' % split[1]
    return '<%s/%s.h>' % tuple(split)


def generate_message_handler(file):
    receiver = MessageReceiver.parse(file)
    headers = set([
        '"%s"' % messages_header_filename(receiver),
        '"HandleMessage.h"',
        '"ArgumentDecoder.h"',
    ])
    [headers.add(header) for header in [header_for_type(parameter.type) for parameter in receiver.iterparameters()] if header]

    result = []

    result.append(_license_header)
    result.append('\n')

    result.append('#include "%s.h"\n\n' % receiver.name)
    result += ['#include %s\n' % header for header in sorted(headers)]
    result.append('\n')

    result.append('namespace WebKit {\n\n')

    result.append('void %s::didReceive%sMessage(CoreIPC::Connection*, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments)\n' % (receiver.name, receiver.name))
    result.append('{\n')
    result.append('    switch (messageID.get<Messages::%s::Kind>()) {\n' % receiver.name)
    result += [case_statement(receiver, message) for message in receiver.messages]
    result.append('    }\n\n')
    result.append('    ASSERT_NOT_REACHED();\n')
    result.append('}\n')

    result.append('\n} // namespace WebKit\n')

    return ''.join(result)
