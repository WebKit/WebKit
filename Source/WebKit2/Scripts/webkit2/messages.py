# Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
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

import parser


DELAYED_ATTRIBUTE = 'Delayed'
DISPATCH_ON_CONNECTION_QUEUE_ATTRIBUTE = 'DispatchOnConnectionQueue'

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


def message_is_variadic(message):
    variadic_types = frozenset([
        'WebKit::InjectedBundleUserMessageEncoder',
        'WebKit::WebContextUserMessageEncoder',
    ])

    return len(message.parameters) and message.parameters[-1].type in variadic_types

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
    if message_is_variadic(message):
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
        if message.has_attribute(DELAYED_ATTRIBUTE):
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
        'WebCore::Animation',
        'WebCore::EditorCommandsForKeyEvent',
        'WebCore::CompositionUnderline',
        'WebCore::DragSession',
        'WebCore::FloatPoint3D',
        'WebCore::FileChooserSettings',
        'WebCore::GrammarDetail',
        'WebCore::IdentityTransformOperation',
        'WebCore::KeypressCommand',
        'WebCore::Length',
        'WebCore::MatrixTransformOperation',
        'WebCore::Matrix3DTransformOperation',
        'WebCore::NotificationContents',
        'WebCore::PerspectiveTransformOperation',
        'WebCore::PluginInfo',
        'WebCore::PrintInfo',
        'WebCore::RotateTransformOperation',
        'WebCore::ScaleTransformOperation',
        'WebCore::SkewTransformOperation',
        'WebCore::TimingFunction',
        'WebCore::TransformationMatrix',
        'WebCore::TransformOperation',
        'WebCore::TransformOperations',
        'WebCore::TranslateTransformOperation',
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
        if message.reply_parameters != None and message.has_attribute(DELAYED_ATTRIBUTE):
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
    receiver = parser.parse(file)
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


def async_case_statement(receiver, message, statement_before_return=None):
    dispatch_function = 'handleMessage'
    if message_is_variadic(message):
        dispatch_function += 'Variadic'

    result = []
    result.append('    case Messages::%s::%s:\n' % (receiver.name, message.id()))
    result.append('        CoreIPC::%s<Messages::%s::%s>(arguments, this, &%s);\n' % (dispatch_function, receiver.name, message.name, handler_function(receiver, message)))
    if statement_before_return:
        result.append('        %s\n' % statement_before_return)
    result.append('        return;\n')
    return surround_in_condition(''.join(result), message.condition)


def sync_case_statement(receiver, message):
    dispatch_function = 'handleMessage'
    if message.has_attribute(DELAYED_ATTRIBUTE):
        dispatch_function += 'Delayed'
    if message_is_variadic(message):
        dispatch_function += 'Variadic'

    result = []
    result.append('    case Messages::%s::%s:\n' % (receiver.name, message.id()))
    result.append('        CoreIPC::%s<Messages::%s::%s>(%sarguments, reply%s, this, &%s);\n' % (dispatch_function, receiver.name, message.name, 'connection, ' if message.has_attribute(DELAYED_ATTRIBUTE) else '', '' if message.has_attribute(DELAYED_ATTRIBUTE) else '.get()', handler_function(receiver, message)))
    result.append('        return;\n')

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
        'WTF::String': ['<wtf/text/WTFString.h>'],
        'WebCore::CompositionUnderline': ['<WebCore/Editor.h>'],
        'WebCore::GrammarDetail': ['<WebCore/TextCheckerClient.h>'],
        'WebCore::KeyframeValueList': ['<WebCore/GraphicsLayer.h>'],
        'WebCore::KeypressCommand': ['<WebCore/KeyboardEvent.h>'],
        'WebCore::FileChooserSettings': ['<WebCore/FileChooser.h>'],
        'WebCore::PluginInfo': ['<WebCore/PluginData.h>'],
        'WebCore::TextCheckingResult': ['<WebCore/TextCheckerClient.h>'],
        'WebKit::InjectedBundleUserMessageEncoder': [],
        'WebKit::WebContextUserMessageEncoder': [],
        'WebKit::WebGestureEvent': ['"WebEvent.h"'],
        'WebKit::WebLayerID': ['"WebLayerTreeInfo.h"'],
        'WebKit::WebLayerInfo': ['"WebLayerTreeInfo.h"'],
        'WebKit::WebKeyboardEvent': ['"WebEvent.h"'],
        'WebKit::WebMouseEvent': ['"WebEvent.h"'],
        'WebKit::WebTouchEvent': ['"WebEvent.h"'],
        'WebKit::WebWheelEvent': ['"WebEvent.h"'],
    }
    if type in special_cases:
        return special_cases[type]

    # We assume that we must include a header for a type iff it has a scope
    # resolution operator (::).
    split = type.split('::')
    if len(split) < 2:
        return []
    if split[0] == 'WebKit' or split[0] == 'CoreIPC':
        return ['"%s.h"' % split[1]]
    return ['<%s/%s.h>' % tuple(split)]


def generate_message_handler(file):
    receiver = parser.parse(file)
    headers = {
        '"%s"' % messages_header_filename(receiver): [None],
        '"HandleMessage.h"': [None],
        '"ArgumentDecoder.h"': [None],
    }

    type_conditions = {}
    for parameter in receiver.iterparameters():
        if not parameter.type in type_conditions:
            type_conditions[parameter.type] = []

        if not parameter.condition in type_conditions[parameter.type]:
            type_conditions[parameter.type].append(parameter.condition)

    for parameter in receiver.iterparameters():
        type = parameter.type
        conditions = type_conditions[type]

        argument_encoder_headers = argument_coder_headers_for_type(type)
        if argument_encoder_headers:
            for header in argument_encoder_headers:
                if header not in headers:
                    headers[header] = []
                headers[header].extend(conditions)

        type_headers = headers_for_type(type)
        for header in type_headers:
            if header not in headers:
                headers[header] = []
            headers[header].extend(conditions)

    for message in receiver.messages:
        if message.reply_parameters is not None:
            for reply_parameter in message.reply_parameters:
                type = reply_parameter.type
                argument_encoder_headers = argument_coder_headers_for_type(type)
                if argument_encoder_headers:
                    for header in argument_encoder_headers:
                        if header not in headers:
                            headers[header] = []
                        headers[header].append(message.condition)

                type_headers = headers_for_type(type)
                for header in type_headers:
                    if header not in headers:
                        headers[header] = []
                    headers[header].append(message.condition)


    result = []

    result.append(_license_header)
    result.append('#include "config.h"\n')
    result.append('\n')

    if receiver.condition:
        result.append('#if %s\n\n' % receiver.condition)

    result.append('#include "%s.h"\n\n' % receiver.name)
    for headercondition in sorted(headers):
        if headers[headercondition] and not None in headers[headercondition]:
            result.append('#if %s\n' % ' || '.join(set(headers[headercondition])))
            result += ['#include %s\n' % headercondition]
            result.append('#endif\n')
        else:
            result += ['#include %s\n' % headercondition]
    result.append('\n')

    sync_delayed_messages = []
    for message in receiver.messages:
        if message.reply_parameters != None and message.has_attribute(DELAYED_ATTRIBUTE):
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
            if message.has_attribute(DISPATCH_ON_CONNECTION_QUEUE_ATTRIBUTE):
                sync_dispatch_on_connection_queue_messages.append(message)
            else:
                sync_messages.append(message)
        else:
            if message.has_attribute(DISPATCH_ON_CONNECTION_QUEUE_ATTRIBUTE):
                async_dispatch_on_connection_queue_messages.append(message)
            else:
                async_messages.append(message)

    if async_dispatch_on_connection_queue_messages:
        result.append('void %s::didReceive%sMessageOnConnectionWorkQueue(CoreIPC::Connection*, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments, bool& didHandleMessage)\n' % (receiver.name, receiver.name))
        result.append('{\n')
        result.append('#if COMPILER(MSVC)\n')
        result.append('#pragma warning(push)\n')
        result.append('#pragma warning(disable: 4065)\n')
        result.append('#endif\n')
        result.append('    switch (messageID.get<Messages::%s::Kind>()) {\n' % receiver.name)
        result += [async_case_statement(receiver, message, 'didHandleMessage = true;') for message in async_dispatch_on_connection_queue_messages]
        result.append('    default:\n')
        result.append('        return;\n')
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
        result.append('void %s::didReceiveSync%sMessage(CoreIPC::Connection*%s, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments, OwnPtr<CoreIPC::ArgumentEncoder>& reply)\n' % (receiver.name, receiver.name, ' connection' if sync_delayed_messages else ''))
        result.append('{\n')
        result.append('    switch (messageID.get<Messages::%s::Kind>()) {\n' % receiver.name)
        result += [sync_case_statement(receiver, message) for message in sync_messages]
        result.append('    default:\n')
        result.append('        break;\n')
        result.append('    }\n\n')
        result.append('    ASSERT_NOT_REACHED();\n')
        result.append('}\n')

    result.append('\n} // namespace WebKit\n')

    if receiver.condition:
        result.append('\n#endif // %s\n' % receiver.condition)

    return ''.join(result)
