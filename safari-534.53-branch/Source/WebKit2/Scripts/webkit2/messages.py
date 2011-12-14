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
import itertools
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
        return itertools.chain((parameter for message in self.messages for parameter in message.parameters),
            (reply_parameter for message in self.messages if message.reply_parameters for reply_parameter in message.reply_parameters))

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
            match = re.search(r'([A-Za-z_0-9]+)\((.*?)\)(?:(?:\s+->\s+)\((.*?)\))?(?:\s+(.*))?', line)
            if match:
                name, parameters_string, reply_parameters_string, attributes_string = match.groups()
                if parameters_string:
                    parameters = parse_parameter_string(parameters_string)
                else:
                    parameters = []

                for parameter in parameters:
                    parameter.condition = condition

                if attributes_string:
                    attributes = frozenset(attributes_string.split())
                    is_delayed = "Delayed" in attributes
                    dispatch_on_connection_queue = "DispatchOnConnectionQueue" in attributes
                else:
                    is_delayed = False
                    dispatch_on_connection_queue = False

                if reply_parameters_string:
                    reply_parameters = parse_parameter_string(reply_parameters_string)
                elif reply_parameters_string == '':
                    reply_parameters = []
                else:
                    reply_parameters = None

                messages.append(Message(name, parameters, reply_parameters, is_delayed, dispatch_on_connection_queue, condition))
        return MessageReceiver(destination, messages, master_condition)


class Message(object):
    def __init__(self, name, parameters, reply_parameters, is_delayed, dispatch_on_connection_queue, condition):
        self.name = name
        self.parameters = parameters
        self.reply_parameters = reply_parameters
        if self.reply_parameters is not None:
            self.is_delayed = is_delayed
        self.dispatch_on_connection_queue = dispatch_on_connection_queue
        self.condition = condition
        if len(self.parameters) != 0:
            self.is_variadic = parameter_type_is_variadic(self.parameters[-1].type)
        else:
            self.is_variadic = False

    def id(self):
        return '%sID' % self.name


class Parameter(object):
    def __init__(self, type, name, condition=None):
        self.type = type
        self.name = name
        self.condition = condition


def parse_parameter_string(parameter_string):
    return [Parameter(*type_and_name.rsplit(' ', 1)) for type_and_name in parameter_string.split(', ')]


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


def parameter_type_is_variadic(type):
    variadic_types = frozenset([
        'WebKit::InjectedBundleUserMessageEncoder',
        'WebKit::WebContextUserMessageEncoder',
    ])

    return type in variadic_types

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
        'int8_t',
        'int16_t',
        'int32_t',
        'int64_t',
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


def decode_type(message):
    if message.is_variadic:
        return arguments_type(message.parameters[:-1], reply_parameter_type)
    return base_class(message)


def delayed_reply_type(message):
    return arguments_type(message.reply_parameters, function_parameter_type)


def message_to_struct_declaration(message):
    result = []
    function_parameters = [(function_parameter_type(x.type), x.name) for x in message.parameters]
    result.append('struct %s : %s' % (message.name, base_class(message)))
    result.append(' {\n')
    result.append('    static const Kind messageID = %s;\n' % message.id())
    if message.reply_parameters != None:
        if message.is_delayed:
            send_parameters = [(function_parameter_type(x.type), x.name) for x in message.reply_parameters]
            result.append('    struct DelayedReply : public ThreadSafeRefCounted<DelayedReply> {\n')
            result.append('        DelayedReply(PassRefPtr<CoreIPC::Connection>, PassOwnPtr<CoreIPC::ArgumentEncoder>);\n')
            result.append('        ~DelayedReply();\n')
            result.append('\n')
            result.append('        bool send(%s);\n' % ', '.join([' '.join(x) for x in send_parameters]))
            result.append('\n')
            result.append('    private:\n')
            result.append('        RefPtr<CoreIPC::Connection> m_connection;\n')
            result.append('        OwnPtr<CoreIPC::ArgumentEncoder> m_arguments;\n')
            result.append('    };\n\n')

        result.append('    typedef %s Reply;\n' % reply_type(message))

    result.append('    typedef %s DecodeType;\n' % decode_type(message))
    if len(function_parameters):
        result.append('    %s%s(%s)' % (len(function_parameters) == 1 and 'explicit ' or '', message.name, ', '.join([' '.join(x) for x in function_parameters])))
        result.append('\n        : %s(%s)\n' % (base_class(message), ', '.join([x[1] for x in function_parameters])))
        result.append('    {\n')
        result.append('    }\n')
    result.append('};\n')
    return surround_in_condition(''.join(result), message.condition)


def struct_or_class(namespace, type):
    structs = frozenset([
        'WebCore::EditorCommandsForKeyEvent',
        'WebCore::CompositionUnderline',
        'WebCore::GrammarDetail',
        'WebCore::KeypressCommand',
        'WebCore::PluginInfo',
        'WebCore::PrintInfo',
        'WebCore::ViewportArguments',
        'WebCore::WindowFeatures',
        'WebKit::AttributedString',
        'WebKit::ContextMenuState',
        'WebKit::DictionaryPopupInfo',
        'WebKit::DrawingAreaInfo',
        'WebKit::EditorState',
        'WebKit::PlatformPopupMenuData',
        'WebKit::PluginCreationParameters',
        'WebKit::PluginProcessCreationParameters',
        'WebKit::PrintInfo',
        'WebKit::SecurityOriginData',
        'WebKit::StatisticsData',
        'WebKit::TextCheckerState',
        'WebKit::WebNavigationDataStore',
        'WebKit::WebOpenPanelParameters::Data',
        'WebKit::WebPageCreationParameters',
        'WebKit::WebPreferencesStore',
        'WebKit::WebProcessCreationParameters',
        'WebKit::WindowGeometry',
    ])

    qualified_name = '%s::%s' % (namespace, type)
    if qualified_name in structs:
        return 'struct %s' % type

    return 'class %s' % type

def forward_declarations_for_namespace(namespace, types):
    result = []
    result.append('namespace %s {\n' % namespace)
    result += ['    %s;\n' % struct_or_class(namespace, x) for x in types]
    result.append('}\n')
    return ''.join(result)


def forward_declarations_and_headers(receiver):
    types_by_namespace = collections.defaultdict(set)

    headers = set([
        '"Arguments.h"',
        '"MessageID.h"',
    ])

    for message in receiver.messages:
        if message.reply_parameters != None and message.is_delayed:
            headers.add('<wtf/ThreadSafeRefCounted.h>')
            types_by_namespace['CoreIPC'].update(['ArgumentEncoder', 'Connection'])

    for parameter in receiver.iterparameters():
        type = parameter.type

        if type.find('<') != -1:
            # Don't forward declare class templates.
            headers.update(headers_for_type(type))
            continue

        split = type.split('::')

        if len(split) == 2:
            namespace = split[0]
            inner_type = split[1]
            types_by_namespace[namespace].add(inner_type)
        elif len(split) > 2:
            # We probably have a nested struct, which means we can't forward declare it.
            # Include its header instead.
            headers.update(headers_for_type(type))

    forward_declarations = '\n'.join([forward_declarations_for_namespace(namespace, types) for (namespace, types) in sorted(types_by_namespace.items())])
    headers = ['#include %s\n' % header for header in sorted(headers)]

    return (forward_declarations, headers)

def generate_messages_header(file):
    receiver = MessageReceiver.parse(file)
    header_guard = messages_header_filename(receiver).replace('.', '_')

    result = []

    result.append(_license_header)

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
    if message.name.find('URL') == 0:
        return '%s::%s' % (receiver.name, 'url' + message.name[3:])
    return '%s::%s' % (receiver.name, message.name[0].lower() + message.name[1:])


def async_case_statement(receiver, message, return_value=None):
    dispatch_function = 'handleMessage'
    if message.is_variadic:
        dispatch_function += 'Variadic'

    result = []
    result.append('    case Messages::%s::%s:\n' % (receiver.name, message.id()))
    result.append('        CoreIPC::%s<Messages::%s::%s>(arguments, this, &%s);\n' % (dispatch_function, receiver.name, message.name, handler_function(receiver, message)))
    if return_value:
        result.append('        return %s;\n' % return_value)
    else:
        result.append('        return;\n')
    return surround_in_condition(''.join(result), message.condition)


def sync_case_statement(receiver, message):
    dispatch_function = 'handleMessage'
    if message.is_delayed:
        dispatch_function += 'Delayed'
    if message.is_variadic:
        dispatch_function += 'Variadic'

    result = []
    result.append('    case Messages::%s::%s:\n' % (receiver.name, message.id()))
    result.append('        CoreIPC::%s<Messages::%s::%s>(%sarguments, reply, this, &%s);\n' % (dispatch_function, receiver.name, message.name, 'connection, ' if message.is_delayed else '', handler_function(receiver, message)))

    if message.is_delayed:
        result.append('        return CoreIPC::ManualReply;\n')
    else:
        result.append('        return CoreIPC::AutomaticReply;\n')

    return surround_in_condition(''.join(result), message.condition)


def argument_coder_headers_for_type(type):
    # Check for Vector.
    match = re.search(r'Vector<(.+)>', type)
    if match:
        element_type = match.groups()[0].strip()
        return ['"ArgumentCoders.h"'] + argument_coder_headers_for_type(element_type)

    special_cases = {
        'WTF::String': '"ArgumentCoders.h"',
        'WebKit::InjectedBundleUserMessageEncoder': '"InjectedBundleUserMessageCoders.h"',
        'WebKit::WebContextUserMessageEncoder': '"WebContextUserMessageCoders.h"',
    }

    if type in special_cases:
        return [special_cases[type]]

    split = type.split('::')
    if len(split) < 2:
        return []
    if split[0] == 'WebCore':
        return ['"WebCoreArgumentCoders.h"']

    return []


def headers_for_type(type):
    # Check for Vector.
    match = re.search(r'Vector<(.+)>', type)
    if match:
        element_type = match.groups()[0].strip()
        return ['<wtf/Vector.h>'] + headers_for_type(element_type)

    special_cases = {
        'WTF::String': '<wtf/text/WTFString.h>',
        'WebCore::CompositionUnderline': '<WebCore/Editor.h>',
        'WebCore::GrammarDetail': '<WebCore/TextCheckerClient.h>',
        'WebCore::KeypressCommand': '<WebCore/KeyboardEvent.h>',
        'WebCore::PluginInfo': '<WebCore/PluginData.h>',
        'WebCore::TextCheckingResult': '<WebCore/TextCheckerClient.h>',
        'WebKit::WebGestureEvent': '"WebEvent.h"',
        'WebKit::WebKeyboardEvent': '"WebEvent.h"',
        'WebKit::WebMouseEvent': '"WebEvent.h"',
        'WebKit::WebTouchEvent': '"WebEvent.h"',
        'WebKit::WebWheelEvent': '"WebEvent.h"',
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
    headers = {
        '"%s"' % messages_header_filename(receiver): None,
        '"HandleMessage.h"': None,
        '"ArgumentDecoder.h"': None,
    }

    type_conditions = {}
    for parameter in receiver.iterparameters():
        type = parameter.type
        condition = parameter.condition

        if type in type_conditions:
            if not type_conditions[type]:
                condition = type_conditions[type]
            else:
                if not condition:
                    type_conditions[type] = condition
        else:
            type_conditions[type] = condition

        argument_encoder_headers = argument_coder_headers_for_type(parameter.type)
        if argument_encoder_headers:
            for header in argument_encoder_headers:
                headers[header] = condition
            continue

        type_headers = headers_for_type(type)
        for header in type_headers:
            headers[header] = condition

    for message in receiver.messages:
        if message.reply_parameters is not None:
            for reply_parameter in message.reply_parameters:
                type = reply_parameter.type
                argument_encoder_headers = argument_coder_headers_for_type(type)
                if argument_encoder_headers:
                    for header in argument_encoder_headers:
                        headers[header] = message.condition
                    continue

                type_headers = headers_for_type(type)
                for header in type_headers:
                    headers[header] = message.condition

    result = []

    result.append(_license_header)
    result.append('#include "config.h"\n')
    result.append('\n')

    if receiver.condition:
        result.append('#if %s\n\n' % receiver.condition)

    result.append('#include "%s.h"\n\n' % receiver.name)
    for headercondition in sorted(headers):
        if headers[headercondition]:
            result.append('#if %s\n' % headers[headercondition])
            result += ['#include %s\n' % headercondition]
            result.append('#endif\n')
        else:
            result += ['#include %s\n' % headercondition]
    result.append('\n')

    sync_delayed_messages = []
    for message in receiver.messages:
        if message.reply_parameters != None and message.is_delayed:
            sync_delayed_messages.append(message)

    if sync_delayed_messages:
        result.append('namespace Messages {\n\nnamespace %s {\n\n' % receiver.name)

        for message in sync_delayed_messages:
            send_parameters = [(function_parameter_type(x.type), x.name) for x in message.reply_parameters]

            if message.condition:
                result.append('#if %s\n\n' % message.condition)
            
            result.append('%s::DelayedReply::DelayedReply(PassRefPtr<CoreIPC::Connection> connection, PassOwnPtr<CoreIPC::ArgumentEncoder> arguments)\n' % message.name)
            result.append('    : m_connection(connection)\n')
            result.append('    , m_arguments(arguments)\n')
            result.append('{\n')
            result.append('}\n')
            result.append('\n')
            result.append('%s::DelayedReply::~DelayedReply()\n' % message.name)
            result.append('{\n')
            result.append('    ASSERT(!m_connection);\n')
            result.append('}\n')
            result.append('\n')
            result.append('bool %s::DelayedReply::send(%s)\n' % (message.name, ', '.join([' '.join(x) for x in send_parameters])))
            result.append('{\n')
            result.append('    ASSERT(m_arguments);\n')
            result += ['    m_arguments->encode(%s);\n' % x.name for x in message.reply_parameters]
            result.append('    bool result = m_connection->sendSyncReply(m_arguments.release());\n')
            result.append('    m_connection = nullptr;\n')
            result.append('    return result;\n')
            result.append('}\n')
            result.append('\n')

            if message.condition:
                result.append('#endif\n\n')

        result.append('} // namespace %s\n\n} // namespace Messages\n\n' % receiver.name)

    result.append('namespace WebKit {\n\n')

    async_dispatch_on_connection_queue_messages = []
    sync_dispatch_on_connection_queue_messages = []
    async_messages = []
    sync_messages = []
    for message in receiver.messages:
        if message.reply_parameters is not None:
            if message.dispatch_on_connection_queue:
                sync_dispatch_on_connection_queue_messages.append(message)
            else:
                sync_messages.append(message)
        else:
            if message.dispatch_on_connection_queue:
                async_dispatch_on_connection_queue_messages.append(message)
            else:
                async_messages.append(message)

    if async_dispatch_on_connection_queue_messages:
        result.append('bool %s::willProcess%sMessageOnClientRunLoop(CoreIPC::Connection*, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments)\n' % (receiver.name, receiver.name))
        result.append('{\n')
        result.append('#if COMPILER(MSVC)\n')
        result.append('#pragma warning(push)\n')
        result.append('#pragma warning(disable: 4065)\n')
        result.append('#endif\n')
        result.append('    switch (messageID.get<Messages::%s::Kind>()) {\n' % receiver.name)
        result += [async_case_statement(receiver, message, 'false') for message in async_dispatch_on_connection_queue_messages]
        result.append('    default:\n')
        result.append('        return true;\n')
        result.append('    }\n')
        result.append('#if COMPILER(MSVC)\n')
        result.append('#pragma warning(pop)\n')
        result.append('#endif\n')
        result.append('}\n\n')

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
        result.append('CoreIPC::SyncReplyMode %s::didReceiveSync%sMessage(CoreIPC::Connection*%s, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments, CoreIPC::ArgumentEncoder* reply)\n' % (receiver.name, receiver.name, ' connection' if sync_delayed_messages else ''))
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
