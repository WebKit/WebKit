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
    def __init__(self, name, messages, condition):
        self.name = name
        self.messages = messages
        self.condition = condition

    def iterparameters(self):
        return (parameter for message in self.messages for parameter in message.parameters)

    @classmethod
    def parse(cls, file):
        destination = None
        messages = []
        condition = None
        master_condition = None
        for line in file:
            match = re.search(r'messages -> ([A-Za-z_0-9]+) {', line)
            if match:
                if condition:
                    master_condition = condition
                    condition = None
                destination = match.group(1)
                continue
            if line.startswith('#'):
                if line.startswith('#if '):
                    condition = line.rstrip()[4:]
                elif line.startswith('#endif'):
                    condition = None
                continue
            match = re.search(r'([A-Za-z_0-9]+)\((.*?)\)(?:(?:\s+->\s+)\((.*?)\)(?:\s+(delayed))?)?', line)
            if match:
                name, parameters_string, reply_parameters_string, delayed_string = match.groups()
                if parameters_string:
                    parameters = parse_parameter_string(parameters_string)
                else:
                    parameters = []

                delayed = delayed_string == 'delayed'

                if reply_parameters_string:
                    reply_parameters = parse_parameter_string(reply_parameters_string)
                elif reply_parameters_string == '':
                    reply_parameters = []
                else:
                    reply_parameters = None

                messages.append(Message(name, parameters, reply_parameters, delayed, condition))
        return MessageReceiver(destination, messages, master_condition)


class Message(object):
    def __init__(self, name, parameters, reply_parameters, delayed, condition):
        self.name = name
        self.parameters = parameters
        self.reply_parameters = reply_parameters
        if self.reply_parameters is not None:
            self.delayed = delayed
        self.condition = condition

    def id(self):
        return '%sID' % self.name


class Parameter(object):
    def __init__(self, type, name):
        self.type = type
        self.name = name


def parse_parameter_string(parameter_string):
    return [Parameter(*type_and_name.split(' ')) for type_and_name in parameter_string.split(', ')]


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
    # Don't use references for built-in types.
    builtin_types = frozenset([
        'bool',
        'float',
        'double',
        'uint8_t',
        'uint16_t',
        'uint32_t',
        'uint64_t',
    ])

    if type in builtin_types:
        return type

    return 'const %s&' % type


def reply_parameter_type(type):
    return '%s&' % type


def arguments_type(parameters, parameter_type_function):
    arguments_type = 'CoreIPC::Arguments%d' % len(parameters)
    if len(parameters):
        arguments_type = '%s<%s>' % (arguments_type, ', '.join(parameter_type_function(parameter.type) for parameter in parameters))
    return arguments_type


def base_class(message):
    return arguments_type(message.parameters, function_parameter_type)


def reply_type(message):
    return arguments_type(message.reply_parameters, reply_parameter_type)


def delayed_reply_type(message):
    return arguments_type(message.reply_parameters, function_parameter_type)


def message_to_struct_declaration(message):
    result = []
    function_parameters = [(function_parameter_type(x.type), x.name) for x in message.parameters]
    result.append('struct %s : %s' % (message.name, base_class(message)))
    result.append(' {\n')
    result.append('    static const Kind messageID = %s;\n' % message.id())
    if message.reply_parameters != None:
        result.append('    typedef %s Reply;\n' % reply_type(message))
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


def forward_declarations_and_headers(receiver):
    types_by_namespace = collections.defaultdict(set)

    headers = set([
        '"Arguments.h"',
        '"MessageID.h"',
    ])

    for parameter in receiver.iterparameters():
        type = parameter.type
        split = type.split('::')

        if len(split) == 2:
            namespace = split[0]
            inner_type = split[1]
            types_by_namespace[namespace].add(inner_type)
        elif len(split) > 2:
            # We probably have a nested struct, which means we can't forward declare it.
            # Include its header instead.
            headers.update(headers_for_type(type))

    forward_declarations = '\n'.join([forward_declarations_for_namespace(namespace, types) for (namespace, types) in sorted(types_by_namespace.iteritems())])
    headers = ['#include %s\n' % header for header in sorted(headers)]

    return (forward_declarations, headers)

def generate_messages_header(file):
    receiver = MessageReceiver.parse(file)
    header_guard = messages_header_filename(receiver).replace('.', '_')

    result = []

    result.append(_license_header)
    result.append('\n')

    result.append('#ifndef %s\n' % header_guard)
    result.append('#define %s\n\n' % header_guard)

    if receiver.condition:
        result.append('#if %s\n\n' % receiver.condition)

    forward_declarations, headers = forward_declarations_and_headers(receiver)

    result += headers
    result.append('\n')

    result.append(forward_declarations)
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

    if receiver.condition:
        result.append('\n#endif // %s\n' % receiver.condition)

    result.append('\n#endif // %s\n' % header_guard)

    return ''.join(result)


def handler_function(receiver, message):
    return '%s::%s' % (receiver.name, message.name[0].lower() + message.name[1:])


def async_case_statement(receiver, message):
    result = []
    result.append('    case Messages::%s::%s:\n' % (receiver.name, message.id()))
    result.append('        CoreIPC::handleMessage<Messages::%s::%s>(arguments, this, &%s);\n' % (receiver.name, message.name, handler_function(receiver, message)))
    result.append('        return;\n')
    return surround_in_condition(''.join(result), message.condition)


def sync_case_statement(receiver, message):
    result = []
    result.append('    case Messages::%s::%s:\n' % (receiver.name, message.id()))
    result.append('        CoreIPC::handleMessage<Messages::%s::%s>(arguments, reply, this, &%s);\n' % (receiver.name, message.name, handler_function(receiver, message)))
    # FIXME: Handle delayed replies
    result.append('        return CoreIPC::AutomaticReply;\n')
    return surround_in_condition(''.join(result), message.condition)


def argument_coder_headers_for_type(type):
    special_cases = {
        'WTF::String': '"WebCoreArgumentCoders.h"',
    }

    if type in special_cases:
        return [special_cases[type]]

    return []


def headers_for_type(type):
    special_cases = {
        'WTF::String': '<wtf/text/WTFString.h>',
        'WebKit::WebKeyboardEvent': '"WebEvent.h"',
        'WebKit::WebMouseEvent': '"WebEvent.h"',
        'WebKit::WebWheelEvent': '"WebEvent.h"',
        'WebKit::WebTouchEvent': '"WebEvent.h"',
    }
    if type in special_cases:
        return [special_cases[type]]

    # We assume that we must include a header for a type iff it has a scope
    # resolution operator (::).
    split = type.split('::')
    if len(split) < 2:
        return []
    if split[0] == 'WebKit' or split[0] == 'CoreIPC':
        return ['"%s.h"' % split[1]]
    return ['<%s/%s.h>' % tuple(split)]


def generate_message_handler(file):
    receiver = MessageReceiver.parse(file)
    headers = set([
        '"%s"' % messages_header_filename(receiver),
        '"HandleMessage.h"',
        '"ArgumentDecoder.h"',
    ])

    for parameter in receiver.iterparameters():
        type = parameter.type
        argument_encoder_headers = argument_coder_headers_for_type(parameter.type)
        if argument_encoder_headers:
            headers.update(argument_encoder_headers)
            continue

        type_headers = headers_for_type(type)
        headers.update(type_headers)

    result = []

    result.append(_license_header)
    result.append('\n')

    if receiver.condition:
        result.append('#if %s\n\n' % receiver.condition)

    result.append('#include "%s.h"\n\n' % receiver.name)
    result += ['#include %s\n' % header for header in sorted(headers)]
    result.append('\n')

    result.append('namespace WebKit {\n\n')

    async_messages = []
    sync_messages = []
    for message in receiver.messages:
        if message.reply_parameters is not None:
            sync_messages.append(message)
        else:
            async_messages.append(message)

    if async_messages:
        result.append('void %s::didReceive%sMessage(CoreIPC::Connection*, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments)\n' % (receiver.name, receiver.name))
        result.append('{\n')
        result.append('    switch (messageID.get<Messages::%s::Kind>()) {\n' % receiver.name)
        result += [async_case_statement(receiver, message) for message in async_messages]
        result.append('    default:\n')
        result.append('        break;\n')
        result.append('    }\n\n')
        result.append('    ASSERT_NOT_REACHED();\n')
        result.append('}\n')

    if sync_messages:
        result.append('\n')
        result.append('CoreIPC::SyncReplyMode %s::didReceiveSync%sMessage(CoreIPC::Connection*, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments, CoreIPC::ArgumentEncoder* reply)\n' % (receiver.name, receiver.name))
        result.append('{\n')
        result.append('    switch (messageID.get<Messages::%s::Kind>()) {\n' % receiver.name)
        result += [sync_case_statement(receiver, message) for message in sync_messages]
        result.append('    default:\n')
        result.append('        break;\n')
        result.append('    }\n\n')
        result.append('    ASSERT_NOT_REACHED();\n')
        result.append('    return CoreIPC::AutomaticReply;\n')
        result.append('}\n')

    result.append('\n} // namespace WebKit\n')

    if receiver.condition:
        result.append('\n#endif // %s\n' % receiver.condition)

    return ''.join(result)
