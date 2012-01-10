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
import parser

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
    TestConnectionQueue(uint64_t pluginID) DispatchOnConnectionQueue

    TestParameterAttributes([AttributeOne AttributeTwo] uint64_t foo, double bar, [AttributeThree] double baz)

#if PLATFORM(MAC)
    DidCreateWebProcessConnection(CoreIPC::MachPort connectionIdentifier)
#endif

#if PLATFORM(MAC)
    # Keyboard support
    InterpretKeyEvent(uint32_t type) -> (Vector<WebCore::KeypressCommand> commandName)
#endif

#if ENABLE(DEPRECATED_FEATURE)
    DeprecatedOperation(CoreIPC::DummyType dummy)
#endif

#if ENABLE(EXPERIMENTAL_FEATURE)
    ExperimentalOperation(CoreIPC::DummyType dummy)
#endif
}

#endif
"""

_expected_results = {
    'name': 'WebPage',
    'conditions': ('ENABLE(WEBKIT2)'),
    'messages': (
        {
            'name': 'LoadURL',
            'parameters': (
                ('WTF::String', 'url'),
            ),
            'conditions': (None),
        },
        {
            'name': 'TouchEvent',
            'parameters': (
                ('WebKit::WebTouchEvent', 'event'),
            ),
            'conditions': ('ENABLE(TOUCH_EVENTS)'),
        },
        {
            'name': 'DidReceivePolicyDecision',
            'parameters': (
                ('uint64_t', 'frameID'),
                ('uint64_t', 'listenerID'),
                ('uint32_t', 'policyAction'),
            ),
            'conditions': (None),
        },
        {
            'name': 'Close',
            'parameters': (),
            'conditions': (None),
        },
        {
            'name': 'PreferencesDidChange',
            'parameters': (
                ('WebKit::WebPreferencesStore', 'store'),
            ),
            'conditions': (None),
        },
        {
            'name': 'SendDoubleAndFloat',
            'parameters': (
                ('double', 'd'),
                ('float', 'f'),
            ),
            'conditions': (None),
        },
        {
            'name': 'SendInts',
            'parameters': (
                ('Vector<uint64_t>', 'ints'),
                ('Vector<Vector<uint64_t> >', 'intVectors')
            ),
            'conditions': (None),
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
            'conditions': (None),
        },
        {
            'name': 'RunJavaScriptAlert',
            'parameters': (
                ('uint64_t', 'frameID'),
                ('WTF::String', 'message')
            ),
            'reply_parameters': (),
            'conditions': (None),
        },
        {
            'name': 'GetPlugins',
            'parameters': (
                ('bool', 'refresh'),
            ),
            'reply_parameters': (
                ('Vector<WebCore::PluginInfo>', 'plugins'),
            ),
            'conditions': (None),
        },
        {
            'name': 'GetPluginProcessConnection',
            'parameters': (
                ('WTF::String', 'pluginPath'),
            ),
            'reply_parameters': (
                ('CoreIPC::Connection::Handle', 'connectionHandle'),
            ),
            'conditions': (None),
        },
        {
            'name': 'TestMultipleAttributes',
            'parameters': (
            ),
            'reply_parameters': (
            ),
            'conditions': (None),
        },
        {
            'name': 'TestConnectionQueue',
            'parameters': (
                ('uint64_t', 'pluginID'),
            ),
            'conditions': (None),
        },
        {
            'name': 'TestParameterAttributes',
            'parameters': (
                ('uint64_t', 'foo', ('AttributeOne', 'AttributeTwo')),
                ('double', 'bar'),
                ('double', 'baz', ('AttributeThree',)),
            ),
            'conditions': (None),
        },
        {
            'name': 'DidCreateWebProcessConnection',
            'parameters': (
                ('CoreIPC::MachPort', 'connectionIdentifier'),
            ),
            'conditions': ('PLATFORM(MAC)'),
        },
        {
            'name': 'InterpretKeyEvent',
            'parameters': (
                ('uint32_t', 'type'),
            ),
            'reply_parameters': (
                ('Vector<WebCore::KeypressCommand>', 'commandName'),
            ),
            'conditions': ('PLATFORM(MAC)'),
        },
        {
            'name': 'DeprecatedOperation',
            'parameters': (
                ('CoreIPC::DummyType', 'dummy'),
            ),
            'conditions': ('ENABLE(DEPRECATED_FEATURE)'),
        },
        {
            'name': 'ExperimentalOperation',
            'parameters': (
                ('CoreIPC::DummyType', 'dummy'),
            ),
            'conditions': ('ENABLE(EXPERIMENTAL_FEATURE)'),
        }
    ),
}


class MessagesTest(unittest.TestCase):
    def setUp(self):
        self.receiver = parser.parse(StringIO(_messages_file_contents))


class ParsingTest(MessagesTest):
    def check_message(self, message, expected_message):
        self.assertEquals(message.name, expected_message['name'])
        self.assertEquals(len(message.parameters), len(expected_message['parameters']))
        for index, parameter in enumerate(message.parameters):
            expected_parameter = expected_message['parameters'][index]
            self.assertEquals(parameter.type, expected_parameter[0])
            self.assertEquals(parameter.name, expected_parameter[1])
            if len(expected_parameter) > 2:
                self.assertEquals(parameter.attributes, frozenset(expected_parameter[2]))
                for attribute in expected_parameter[2]:
                    self.assertTrue(parameter.has_attribute(attribute))
            else:
                self.assertEquals(parameter.attributes, frozenset())
        if message.reply_parameters != None:
            for index, parameter in enumerate(message.reply_parameters):
                self.assertEquals(parameter.type, expected_message['reply_parameters'][index][0])
                self.assertEquals(parameter.name, expected_message['reply_parameters'][index][1])
        else:
            self.assertFalse('reply_parameters' in expected_message)
        self.assertEquals(message.condition, expected_message['conditions'])

    def test_receiver(self):
        """Receiver should be parsed as expected"""
        self.assertEquals(self.receiver.name, _expected_results['name'])
        self.assertEquals(self.receiver.condition, _expected_results['conditions'])
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
    class DummyType;
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
    TestConnectionQueueID,
    TestParameterAttributesID,
#if PLATFORM(MAC)
    DidCreateWebProcessConnectionID,
#endif
#if PLATFORM(MAC)
    InterpretKeyEventID,
#endif
#if ENABLE(DEPRECATED_FEATURE)
    DeprecatedOperationID,
#endif
#if ENABLE(EXPERIMENTAL_FEATURE)
    ExperimentalOperationID,
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

struct TestConnectionQueue : CoreIPC::Arguments1<uint64_t> {
    static const Kind messageID = TestConnectionQueueID;
    typedef CoreIPC::Arguments1<uint64_t> DecodeType;
    explicit TestConnectionQueue(uint64_t pluginID)
        : CoreIPC::Arguments1<uint64_t>(pluginID)
    {
    }
};

struct TestParameterAttributes : CoreIPC::Arguments3<uint64_t, double, double> {
    static const Kind messageID = TestParameterAttributesID;
    typedef CoreIPC::Arguments3<uint64_t, double, double> DecodeType;
    TestParameterAttributes(uint64_t foo, double bar, double baz)
        : CoreIPC::Arguments3<uint64_t, double, double>(foo, bar, baz)
    {
    }
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

#if ENABLE(DEPRECATED_FEATURE)
struct DeprecatedOperation : CoreIPC::Arguments1<const CoreIPC::DummyType&> {
    static const Kind messageID = DeprecatedOperationID;
    typedef CoreIPC::Arguments1<const CoreIPC::DummyType&> DecodeType;
    explicit DeprecatedOperation(const CoreIPC::DummyType& dummy)
        : CoreIPC::Arguments1<const CoreIPC::DummyType&>(dummy)
    {
    }
};
#endif

#if ENABLE(EXPERIMENTAL_FEATURE)
struct ExperimentalOperation : CoreIPC::Arguments1<const CoreIPC::DummyType&> {
    static const Kind messageID = ExperimentalOperationID;
    typedef CoreIPC::Arguments1<const CoreIPC::DummyType&> DecodeType;
    explicit ExperimentalOperation(const CoreIPC::DummyType& dummy)
        : CoreIPC::Arguments1<const CoreIPC::DummyType&>(dummy)
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

#include "ArgumentCoders.h"
#include "ArgumentDecoder.h"
#include "Connection.h"
#if ENABLE(DEPRECATED_FEATURE) || ENABLE(EXPERIMENTAL_FEATURE)
#include "DummyType.h"
#endif
#include "HandleMessage.h"
#if PLATFORM(MAC)
#include "MachPort.h"
#endif
#include "Plugin.h"
#include "WebCoreArgumentCoders.h"
#if ENABLE(TOUCH_EVENTS)
#include "WebEvent.h"
#endif
#include "WebPageMessages.h"
#include "WebPreferencesStore.h"
#if PLATFORM(MAC)
#include <WebCore/KeyboardEvent.h>
#endif
#include <WebCore/PluginData.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

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

void WebPage::didReceiveWebPageMessageOnConnectionWorkQueue(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments, bool& didHandleMessage)
{
#if COMPILER(MSVC)
#pragma warning(push)
#pragma warning(disable: 4065)
#endif
    switch (messageID.get<Messages::WebPage::Kind>()) {
    case Messages::WebPage::TestConnectionQueueID:
        CoreIPC::handleMessageOnConnectionQueue<Messages::WebPage::TestConnectionQueue>(connection, arguments, this, &WebPage::testConnectionQueue);
        didHandleMessage = true;
        return;
    default:
        return;
    }
#if COMPILER(MSVC)
#pragma warning(pop)
#endif
}

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
    case Messages::WebPage::TestParameterAttributesID:
        CoreIPC::handleMessage<Messages::WebPage::TestParameterAttributes>(arguments, this, &WebPage::testParameterAttributes);
        return;
#if PLATFORM(MAC)
    case Messages::WebPage::DidCreateWebProcessConnectionID:
        CoreIPC::handleMessage<Messages::WebPage::DidCreateWebProcessConnection>(arguments, this, &WebPage::didCreateWebProcessConnection);
        return;
#endif
#if ENABLE(DEPRECATED_FEATURE)
    case Messages::WebPage::DeprecatedOperationID:
        CoreIPC::handleMessage<Messages::WebPage::DeprecatedOperation>(arguments, this, &WebPage::deprecatedOperation);
        return;
#endif
#if ENABLE(EXPERIMENTAL_FEATURE)
    case Messages::WebPage::ExperimentalOperationID:
        CoreIPC::handleMessage<Messages::WebPage::ExperimentalOperation>(arguments, this, &WebPage::experimentalOperation);
        return;
#endif
    default:
        break;
    }

    ASSERT_NOT_REACHED();
}

void WebPage::didReceiveSyncWebPageMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments, OwnPtr<CoreIPC::ArgumentEncoder>& reply)
{
    switch (messageID.get<Messages::WebPage::Kind>()) {
    case Messages::WebPage::CreatePluginID:
        CoreIPC::handleMessage<Messages::WebPage::CreatePlugin>(arguments, reply.get(), this, &WebPage::createPlugin);
        return;
    case Messages::WebPage::RunJavaScriptAlertID:
        CoreIPC::handleMessage<Messages::WebPage::RunJavaScriptAlert>(arguments, reply.get(), this, &WebPage::runJavaScriptAlert);
        return;
    case Messages::WebPage::GetPluginProcessConnectionID:
        CoreIPC::handleMessageDelayed<Messages::WebPage::GetPluginProcessConnection>(connection, arguments, reply, this, &WebPage::getPluginProcessConnection);
        return;
#if PLATFORM(MAC)
    case Messages::WebPage::InterpretKeyEventID:
        CoreIPC::handleMessage<Messages::WebPage::InterpretKeyEvent>(arguments, reply.get(), this, &WebPage::interpretKeyEvent);
        return;
#endif
    default:
        break;
    }

    ASSERT_NOT_REACHED();
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
