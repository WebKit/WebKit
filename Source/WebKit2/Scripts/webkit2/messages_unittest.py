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

import unittest
from StringIO import StringIO

import messages

_messages_file_contents = """# Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "config.h"

#if ENABLE(WEBKIT2)

messages -> WebPage {
    LoadURL(WTF::String url)
#if ENABLE(TOUCH_EVENTS)
    TouchEvent(WebKit::WebTouchEvent event)
#endif
    DidReceivePolicyDecision(uint64_t frameID, uint64_t listenerID, uint32_t policyAction)
    Close()

    PreferencesDidChange(WebKit::WebPreferencesStore store)
    SendDoubleAndFloat(double d, float f)
    SendInts(Vector<uint64_t> ints, Vector<Vector<uint64_t> > intVectors)

    CreatePlugin(uint64_t pluginInstanceID, WebKit::Plugin::Parameters parameters) -> (bool result)
    RunJavaScriptAlert(uint64_t frameID, WTF::String message) -> ()
    GetPlugins(bool refresh) -> (Vector<WebCore::PluginInfo> plugins) DispatchOnConnectionQueue
    GetPluginProcessConnection(WTF::String pluginPath) -> (CoreIPC::Connection::Handle connectionHandle) Delayed

    TestMultipleAttributes() -> () DispatchOnConnectionQueue Delayed

#if PLATFORM(MAC)
    DidCreateWebProcessConnection(CoreIPC::MachPort connectionIdentifier)
#endif

#if PLATFORM(MAC)
    # Keyboard support
    InterpretKeyEvent(uint32_t type) -> (Vector<WebCore::KeypressCommand> commandName)
#endif
}

#endif
"""

_expected_results = {
    'name': 'WebPage',
    'condition': 'ENABLE(WEBKIT2)',
    'messages': (
        {
            'name': 'LoadURL',
            'parameters': (
                ('WTF::String', 'url'),
            ),
            'condition': None,
        },
        {
            'name': 'TouchEvent',
            'parameters': (
                ('WebKit::WebTouchEvent', 'event'),
            ),
            'condition': 'ENABLE(TOUCH_EVENTS)',
        },
        {
            'name': 'DidReceivePolicyDecision',
            'parameters': (
                ('uint64_t', 'frameID'),
                ('uint64_t', 'listenerID'),
                ('uint32_t', 'policyAction'),
            ),
            'condition': None,
        },
        {
            'name': 'Close',
            'parameters': (),
            'condition': None,
        },
        {
            'name': 'PreferencesDidChange',
            'parameters': (
                ('WebKit::WebPreferencesStore', 'store'),
            ),
            'condition': None,
        },
        {
            'name': 'SendDoubleAndFloat',
            'parameters': (
                ('double', 'd'),
                ('float', 'f'),
            ),
            'condition': None,
        },
        {
            'name': 'SendInts',
            'parameters': (
                ('Vector<uint64_t>', 'ints'),
                ('Vector<Vector<uint64_t> >', 'intVectors')
            ),
            'condition': None,
        },
        {
            'name': 'CreatePlugin',
            'parameters': (
                ('uint64_t', 'pluginInstanceID'),
                ('WebKit::Plugin::Parameters', 'parameters')
            ),
            'reply_parameters': (
                ('bool', 'result'),
            ),
            'condition': None,
        },
        {
            'name': 'RunJavaScriptAlert',
            'parameters': (
                ('uint64_t', 'frameID'),
                ('WTF::String', 'message')
            ),
            'reply_parameters': (),
            'condition': None,
        },
        {
            'name': 'GetPlugins',
            'parameters': (
                ('bool', 'refresh'),
            ),
            'reply_parameters': (
                ('Vector<WebCore::PluginInfo>', 'plugins'),
            ),
            'condition': None,
        },
        {
            'name': 'GetPluginProcessConnection',
            'parameters': (
                ('WTF::String', 'pluginPath'),
            ),
            'reply_parameters': (
                ('CoreIPC::Connection::Handle', 'connectionHandle'),
            ),
            'condition': None,
        },
        {
            'name': 'TestMultipleAttributes',
            'parameters': (
            ),
            'reply_parameters': (
            ),
            'condition': None,
        },
        {
            'name': 'DidCreateWebProcessConnection',
            'parameters': (
                ('CoreIPC::MachPort', 'connectionIdentifier'),
            ),
            'condition': 'PLATFORM(MAC)',
        },
        {
            'name': 'InterpretKeyEvent',
            'parameters': (
                ('uint32_t', 'type'),
            ),
            'reply_parameters': (
                ('Vector<WebCore::KeypressCommand>', 'commandName'),
            ),
            'condition': 'PLATFORM(MAC)',
        },
    ),
}


class MessagesTest(unittest.TestCase):
    def setUp(self):
        self.receiver = messages.MessageReceiver.parse(StringIO(_messages_file_contents))


class ParsingTest(MessagesTest):
    def check_message(self, message, expected_message):
        self.assertEquals(message.name, expected_message['name'])
        self.assertEquals(len(message.parameters), len(expected_message['parameters']))
        for index, parameter in enumerate(message.parameters):
            self.assertEquals(parameter.type, expected_message['parameters'][index][0])
            self.assertEquals(parameter.name, expected_message['parameters'][index][1])
        if message.reply_parameters != None:
            for index, parameter in enumerate(message.reply_parameters):
                self.assertEquals(parameter.type, expected_message['reply_parameters'][index][0])
                self.assertEquals(parameter.name, expected_message['reply_parameters'][index][1])
        else:
            self.assertFalse('reply_parameters' in expected_message)
        self.assertEquals(message.condition, expected_message['condition'])

    def test_receiver(self):
        """Receiver should be parsed as expected"""
        self.assertEquals(self.receiver.name, _expected_results['name'])
        self.assertEquals(self.receiver.condition, _expected_results['condition'])
        self.assertEquals(len(self.receiver.messages), len(_expected_results['messages']))
        for index, message in enumerate(self.receiver.messages):
            self.check_message(message, _expected_results['messages'][index])

_expected_header = """/*
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

#ifndef WebPageMessages_h
#define WebPageMessages_h

#if ENABLE(WEBKIT2)

#include "Arguments.h"
#include "Connection.h"
#include "MessageID.h"
#include "Plugin.h"
#include <WebCore/KeyboardEvent.h>
#include <WebCore/PluginData.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Vector.h>

namespace CoreIPC {
    class ArgumentEncoder;
    class Connection;
    class MachPort;
}

namespace WTF {
    class String;
}

namespace WebKit {
    struct WebPreferencesStore;
    class WebTouchEvent;
}

namespace Messages {

namespace WebPage {

enum Kind {
    LoadURLID,
#if ENABLE(TOUCH_EVENTS)
    TouchEventID,
#endif
    DidReceivePolicyDecisionID,
    CloseID,
    PreferencesDidChangeID,
    SendDoubleAndFloatID,
    SendIntsID,
    CreatePluginID,
    RunJavaScriptAlertID,
    GetPluginsID,
    GetPluginProcessConnectionID,
    TestMultipleAttributesID,
#if PLATFORM(MAC)
    DidCreateWebProcessConnectionID,
#endif
#if PLATFORM(MAC)
    InterpretKeyEventID,
#endif
};

struct LoadURL : CoreIPC::Arguments1<const WTF::String&> {
    static const Kind messageID = LoadURLID;
    typedef CoreIPC::Arguments1<const WTF::String&> DecodeType;
    explicit LoadURL(const WTF::String& url)
        : CoreIPC::Arguments1<const WTF::String&>(url)
    {
    }
};

#if ENABLE(TOUCH_EVENTS)
struct TouchEvent : CoreIPC::Arguments1<const WebKit::WebTouchEvent&> {
    static const Kind messageID = TouchEventID;
    typedef CoreIPC::Arguments1<const WebKit::WebTouchEvent&> DecodeType;
    explicit TouchEvent(const WebKit::WebTouchEvent& event)
        : CoreIPC::Arguments1<const WebKit::WebTouchEvent&>(event)
    {
    }
};
#endif

struct DidReceivePolicyDecision : CoreIPC::Arguments3<uint64_t, uint64_t, uint32_t> {
    static const Kind messageID = DidReceivePolicyDecisionID;
    typedef CoreIPC::Arguments3<uint64_t, uint64_t, uint32_t> DecodeType;
    DidReceivePolicyDecision(uint64_t frameID, uint64_t listenerID, uint32_t policyAction)
        : CoreIPC::Arguments3<uint64_t, uint64_t, uint32_t>(frameID, listenerID, policyAction)
    {
    }
};

struct Close : CoreIPC::Arguments0 {
    static const Kind messageID = CloseID;
    typedef CoreIPC::Arguments0 DecodeType;
};

struct PreferencesDidChange : CoreIPC::Arguments1<const WebKit::WebPreferencesStore&> {
    static const Kind messageID = PreferencesDidChangeID;
    typedef CoreIPC::Arguments1<const WebKit::WebPreferencesStore&> DecodeType;
    explicit PreferencesDidChange(const WebKit::WebPreferencesStore& store)
        : CoreIPC::Arguments1<const WebKit::WebPreferencesStore&>(store)
    {
    }
};

struct SendDoubleAndFloat : CoreIPC::Arguments2<double, float> {
    static const Kind messageID = SendDoubleAndFloatID;
    typedef CoreIPC::Arguments2<double, float> DecodeType;
    SendDoubleAndFloat(double d, float f)
        : CoreIPC::Arguments2<double, float>(d, f)
    {
    }
};

struct SendInts : CoreIPC::Arguments2<const Vector<uint64_t>&, const Vector<Vector<uint64_t> >&> {
    static const Kind messageID = SendIntsID;
    typedef CoreIPC::Arguments2<const Vector<uint64_t>&, const Vector<Vector<uint64_t> >&> DecodeType;
    SendInts(const Vector<uint64_t>& ints, const Vector<Vector<uint64_t> >& intVectors)
        : CoreIPC::Arguments2<const Vector<uint64_t>&, const Vector<Vector<uint64_t> >&>(ints, intVectors)
    {
    }
};

struct CreatePlugin : CoreIPC::Arguments2<uint64_t, const WebKit::Plugin::Parameters&> {
    static const Kind messageID = CreatePluginID;
    typedef CoreIPC::Arguments1<bool&> Reply;
    typedef CoreIPC::Arguments2<uint64_t, const WebKit::Plugin::Parameters&> DecodeType;
    CreatePlugin(uint64_t pluginInstanceID, const WebKit::Plugin::Parameters& parameters)
        : CoreIPC::Arguments2<uint64_t, const WebKit::Plugin::Parameters&>(pluginInstanceID, parameters)
    {
    }
};

struct RunJavaScriptAlert : CoreIPC::Arguments2<uint64_t, const WTF::String&> {
    static const Kind messageID = RunJavaScriptAlertID;
    typedef CoreIPC::Arguments0 Reply;
    typedef CoreIPC::Arguments2<uint64_t, const WTF::String&> DecodeType;
    RunJavaScriptAlert(uint64_t frameID, const WTF::String& message)
        : CoreIPC::Arguments2<uint64_t, const WTF::String&>(frameID, message)
    {
    }
};

struct GetPlugins : CoreIPC::Arguments1<bool> {
    static const Kind messageID = GetPluginsID;
    typedef CoreIPC::Arguments1<Vector<WebCore::PluginInfo>&> Reply;
    typedef CoreIPC::Arguments1<bool> DecodeType;
    explicit GetPlugins(bool refresh)
        : CoreIPC::Arguments1<bool>(refresh)
    {
    }
};

struct GetPluginProcessConnection : CoreIPC::Arguments1<const WTF::String&> {
    static const Kind messageID = GetPluginProcessConnectionID;
    struct DelayedReply : public ThreadSafeRefCounted<DelayedReply> {
        DelayedReply(PassRefPtr<CoreIPC::Connection>, PassOwnPtr<CoreIPC::ArgumentEncoder>);
        ~DelayedReply();

        bool send(const CoreIPC::Connection::Handle& connectionHandle);

    private:
        RefPtr<CoreIPC::Connection> m_connection;
        OwnPtr<CoreIPC::ArgumentEncoder> m_arguments;
    };

    typedef CoreIPC::Arguments1<CoreIPC::Connection::Handle&> Reply;
    typedef CoreIPC::Arguments1<const WTF::String&> DecodeType;
    explicit GetPluginProcessConnection(const WTF::String& pluginPath)
        : CoreIPC::Arguments1<const WTF::String&>(pluginPath)
    {
    }
};

struct TestMultipleAttributes : CoreIPC::Arguments0 {
    static const Kind messageID = TestMultipleAttributesID;
    struct DelayedReply : public ThreadSafeRefCounted<DelayedReply> {
        DelayedReply(PassRefPtr<CoreIPC::Connection>, PassOwnPtr<CoreIPC::ArgumentEncoder>);
        ~DelayedReply();

        bool send();

    private:
        RefPtr<CoreIPC::Connection> m_connection;
        OwnPtr<CoreIPC::ArgumentEncoder> m_arguments;
    };

    typedef CoreIPC::Arguments0 Reply;
    typedef CoreIPC::Arguments0 DecodeType;
};

#if PLATFORM(MAC)
struct DidCreateWebProcessConnection : CoreIPC::Arguments1<const CoreIPC::MachPort&> {
    static const Kind messageID = DidCreateWebProcessConnectionID;
    typedef CoreIPC::Arguments1<const CoreIPC::MachPort&> DecodeType;
    explicit DidCreateWebProcessConnection(const CoreIPC::MachPort& connectionIdentifier)
        : CoreIPC::Arguments1<const CoreIPC::MachPort&>(connectionIdentifier)
    {
    }
};
#endif

#if PLATFORM(MAC)
struct InterpretKeyEvent : CoreIPC::Arguments1<uint32_t> {
    static const Kind messageID = InterpretKeyEventID;
    typedef CoreIPC::Arguments1<Vector<WebCore::KeypressCommand>&> Reply;
    typedef CoreIPC::Arguments1<uint32_t> DecodeType;
    explicit InterpretKeyEvent(uint32_t type)
        : CoreIPC::Arguments1<uint32_t>(type)
    {
    }
};
#endif

} // namespace WebPage

} // namespace Messages

namespace CoreIPC {

template<> struct MessageKindTraits<Messages::WebPage::Kind> {
    static const MessageClass messageClass = MessageClassWebPage;
};

} // namespace CoreIPC

#endif // ENABLE(WEBKIT2)

#endif // WebPageMessages_h
"""

_expected_receiver_implementation = """/*
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

#include "config.h"

#if ENABLE(WEBKIT2)

#include "WebPage.h"

#if PLATFORM(MAC)
#include "ArgumentCoders.h"
#endif
#include "ArgumentDecoder.h"
#include "Connection.h"
#include "HandleMessage.h"
#if PLATFORM(MAC)
#include "MachPort.h"
#endif
#include "Plugin.h"
#if PLATFORM(MAC)
#include "WebCoreArgumentCoders.h"
#endif
#if ENABLE(TOUCH_EVENTS)
#include "WebEvent.h"
#endif
#include "WebPageMessages.h"
#include "WebPreferencesStore.h"

namespace Messages {

namespace WebPage {

GetPluginProcessConnection::DelayedReply::DelayedReply(PassRefPtr<CoreIPC::Connection> connection, PassOwnPtr<CoreIPC::ArgumentEncoder> arguments)
    : m_connection(connection)
    , m_arguments(arguments)
{
}

GetPluginProcessConnection::DelayedReply::~DelayedReply()
{
    ASSERT(!m_connection);
}

bool GetPluginProcessConnection::DelayedReply::send(const CoreIPC::Connection::Handle& connectionHandle)
{
    ASSERT(m_arguments);
    m_arguments->encode(connectionHandle);
    bool result = m_connection->sendSyncReply(m_arguments.release());
    m_connection = nullptr;
    return result;
}

TestMultipleAttributes::DelayedReply::DelayedReply(PassRefPtr<CoreIPC::Connection> connection, PassOwnPtr<CoreIPC::ArgumentEncoder> arguments)
    : m_connection(connection)
    , m_arguments(arguments)
{
}

TestMultipleAttributes::DelayedReply::~DelayedReply()
{
    ASSERT(!m_connection);
}

bool TestMultipleAttributes::DelayedReply::send()
{
    ASSERT(m_arguments);
    bool result = m_connection->sendSyncReply(m_arguments.release());
    m_connection = nullptr;
    return result;
}

} // namespace WebPage

} // namespace Messages

namespace WebKit {

void WebPage::didReceiveWebPageMessage(CoreIPC::Connection*, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments)
{
    switch (messageID.get<Messages::WebPage::Kind>()) {
    case Messages::WebPage::LoadURLID:
        CoreIPC::handleMessage<Messages::WebPage::LoadURL>(arguments, this, &WebPage::loadURL);
        return;
#if ENABLE(TOUCH_EVENTS)
    case Messages::WebPage::TouchEventID:
        CoreIPC::handleMessage<Messages::WebPage::TouchEvent>(arguments, this, &WebPage::touchEvent);
        return;
#endif
    case Messages::WebPage::DidReceivePolicyDecisionID:
        CoreIPC::handleMessage<Messages::WebPage::DidReceivePolicyDecision>(arguments, this, &WebPage::didReceivePolicyDecision);
        return;
    case Messages::WebPage::CloseID:
        CoreIPC::handleMessage<Messages::WebPage::Close>(arguments, this, &WebPage::close);
        return;
    case Messages::WebPage::PreferencesDidChangeID:
        CoreIPC::handleMessage<Messages::WebPage::PreferencesDidChange>(arguments, this, &WebPage::preferencesDidChange);
        return;
    case Messages::WebPage::SendDoubleAndFloatID:
        CoreIPC::handleMessage<Messages::WebPage::SendDoubleAndFloat>(arguments, this, &WebPage::sendDoubleAndFloat);
        return;
    case Messages::WebPage::SendIntsID:
        CoreIPC::handleMessage<Messages::WebPage::SendInts>(arguments, this, &WebPage::sendInts);
        return;
#if PLATFORM(MAC)
    case Messages::WebPage::DidCreateWebProcessConnectionID:
        CoreIPC::handleMessage<Messages::WebPage::DidCreateWebProcessConnection>(arguments, this, &WebPage::didCreateWebProcessConnection);
        return;
#endif
    default:
        break;
    }

    ASSERT_NOT_REACHED();
}

CoreIPC::SyncReplyMode WebPage::didReceiveSyncWebPageMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments, CoreIPC::ArgumentEncoder* reply)
{
    switch (messageID.get<Messages::WebPage::Kind>()) {
    case Messages::WebPage::CreatePluginID:
        CoreIPC::handleMessage<Messages::WebPage::CreatePlugin>(arguments, reply, this, &WebPage::createPlugin);
        return CoreIPC::AutomaticReply;
    case Messages::WebPage::RunJavaScriptAlertID:
        CoreIPC::handleMessage<Messages::WebPage::RunJavaScriptAlert>(arguments, reply, this, &WebPage::runJavaScriptAlert);
        return CoreIPC::AutomaticReply;
    case Messages::WebPage::GetPluginsID:
        CoreIPC::handleMessage<Messages::WebPage::GetPlugins>(arguments, reply, this, &WebPage::getPlugins);
        return CoreIPC::AutomaticReply;
    case Messages::WebPage::GetPluginProcessConnectionID:
        CoreIPC::handleMessageDelayed<Messages::WebPage::GetPluginProcessConnection>(connection, arguments, reply, this, &WebPage::getPluginProcessConnection);
        return CoreIPC::ManualReply;
    case Messages::WebPage::TestMultipleAttributesID:
        CoreIPC::handleMessageDelayed<Messages::WebPage::TestMultipleAttributes>(connection, arguments, reply, this, &WebPage::testMultipleAttributes);
        return CoreIPC::ManualReply;
#if PLATFORM(MAC)
    case Messages::WebPage::InterpretKeyEventID:
        CoreIPC::handleMessage<Messages::WebPage::InterpretKeyEvent>(arguments, reply, this, &WebPage::interpretKeyEvent);
        return CoreIPC::AutomaticReply;
#endif
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return CoreIPC::AutomaticReply;
}

} // namespace WebKit

#endif // ENABLE(WEBKIT2)
"""


class GeneratedFileContentsTest(unittest.TestCase):
    def assertGeneratedFileContentsEqual(self, first, second):
        first_list = first.split('\n')
        second_list = second.split('\n')

        for index, first_line in enumerate(first_list):
            self.assertEquals(first_line, second_list[index])

        self.assertEquals(len(first_list), len(second_list))


class HeaderTest(GeneratedFileContentsTest):
    def test_header(self):
        file_contents = messages.generate_messages_header(StringIO(_messages_file_contents))
        self.assertGeneratedFileContentsEqual(file_contents, _expected_header)


class ReceiverImplementationTest(GeneratedFileContentsTest):
    def test_receiver_implementation(self):
        file_contents = messages.generate_message_handler(StringIO(_messages_file_contents))
        self.assertGeneratedFileContentsEqual(file_contents, _expected_receiver_implementation)


if __name__ == '__main__':
    unittest.main()
