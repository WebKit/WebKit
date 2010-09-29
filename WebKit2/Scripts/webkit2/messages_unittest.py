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

#if ENABLE(WEBKIT2)

messages -> WebPage {
    LoadURL(WTF::String url)
#if ENABLE(TOUCH_EVENTS)
    TouchEvent(WebKit::WebTouchEvent event)
#endif
    DidReceivePolicyDecision(uint64_t frameID, uint64_t listenerID, uint32_t policyAction)
    Close()

    SendDoubleAndFloat(double d, float f)
    SendInts(Vector<uint64_t> ints, Vector<Vector<uint64_t> > intVectors)

    CreatePlugin(uint64_t pluginInstanceID, WebKit::Plugin::Parameters parameters) -> (bool result)
    RunJavaScriptAlert(uint64_t frameID, WTF::String message) -> ()
    GetPlugins(bool refresh) -> (Vector<WebCore::PluginInfo> plugins)
    GetPluginProcessConnection(WTF::String pluginPath) -> (CoreIPC::Connection::Handle connectionHandle) delayed

    DidCreateWebProcessConnection(CoreIPC::MachPort connectionIdentifier)
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
            'base_class': 'CoreIPC::Arguments1<const WTF::String&>',
        },
        {
            'name': 'TouchEvent',
            'parameters': (
                ('WebKit::WebTouchEvent', 'event'),
            ),
            'condition': 'ENABLE(TOUCH_EVENTS)',
            'base_class': 'CoreIPC::Arguments1<const WebKit::WebTouchEvent&>',
        },
        {
            'name': 'DidReceivePolicyDecision',
            'parameters': (
                ('uint64_t', 'frameID'),
                ('uint64_t', 'listenerID'),
                ('uint32_t', 'policyAction'),
            ),
            'condition': None,
            'base_class': 'CoreIPC::Arguments3<uint64_t, uint64_t, uint32_t>',
        },
        {
            'name': 'Close',
            'parameters': (),
            'condition': None,
            'base_class': 'CoreIPC::Arguments0',
        },
        {
            'name': 'SendDoubleAndFloat',
            'parameters': (
                ('double', 'd'),
                ('float', 'f'),
            ),
            'condition': None,
            'base_class': 'CoreIPC::Arguments2<double, float>',
        },
        {
            'name': 'SendInts',
            'parameters': (
                ('Vector<uint64_t>', 'ints'),
                ('Vector<Vector<uint64_t> >', 'intVectors')
            ),
            'condition': None,
            'base_class': 'CoreIPC::Arguments1<const Vector<uint64_t>&>',
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
            'base_class': 'CoreIPC::Arguments2<uint64_t, const WebKit::Plugin::Parameters&>',
            'reply_base_class': 'CoreIPC::Arguments1<bool>',
        },
        {
            'name': 'RunJavaScriptAlert',
            'parameters': (
                ('uint64_t', 'frameID'),
                ('WTF::String', 'message')
            ),
            'reply_parameters': (),
            'condition': None,
            'base_class': 'CoreIPC::Arguments2<uint64_t, const WTF::String&>',
            'reply_base_class': 'CoreIPC::Arguments0',
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
            'base_class': 'CoreIPC::Arguments1<bool>',
            'reply_base_class': 'CoreIPC::Arguments1<Vector<WebCore::PluginInfo>&>',
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
            'base_class': 'CoreIPC::Arguments1<const WTF::String&>',
            'reply_base_class': 'CoreIPC::Arguments1<CoreIPC::Connection::Handle&>',
            'delayed_reply_base_class': 'CoreIPC::Arguments1<const CoreIPC::Connection::Handle&>',
        },
        {
            'name': 'DidCreateWebProcessConnection',
            'parameters': (
                ('CoreIPC::MachPort', 'connectionIdentifier'),
            ),
            'condition': None,
            'base_class': 'CoreIPC::Arguments2<double, float>',
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
#include "MessageID.h"
#include "Plugin.h"

namespace CoreIPC {
    class MachPort;
}

namespace WTF {
    class String;
}

namespace WebKit {
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
    SendDoubleAndFloatID,
    SendIntsID,
    CreatePluginID,
    RunJavaScriptAlertID,
    GetPluginsID,
    GetPluginProcessConnectionID,
    DidCreateWebProcessConnectionID,
};

struct LoadURL : CoreIPC::Arguments1<const WTF::String&> {
    static const Kind messageID = LoadURLID;
    explicit LoadURL(const WTF::String& url)
        : CoreIPC::Arguments1<const WTF::String&>(url)
    {
    }
};

#if ENABLE(TOUCH_EVENTS)
struct TouchEvent : CoreIPC::Arguments1<const WebKit::WebTouchEvent&> {
    static const Kind messageID = TouchEventID;
    explicit TouchEvent(const WebKit::WebTouchEvent& event)
        : CoreIPC::Arguments1<const WebKit::WebTouchEvent&>(event)
    {
    }
};
#endif

struct DidReceivePolicyDecision : CoreIPC::Arguments3<uint64_t, uint64_t, uint32_t> {
    static const Kind messageID = DidReceivePolicyDecisionID;
    DidReceivePolicyDecision(uint64_t frameID, uint64_t listenerID, uint32_t policyAction)
        : CoreIPC::Arguments3<uint64_t, uint64_t, uint32_t>(frameID, listenerID, policyAction)
    {
    }
};

struct Close : CoreIPC::Arguments0 {
    static const Kind messageID = CloseID;
};

struct SendDoubleAndFloat : CoreIPC::Arguments2<double, float> {
    static const Kind messageID = SendDoubleAndFloatID;
    SendDoubleAndFloat(double d, float f)
        : CoreIPC::Arguments2<double, float>(d, f)
    {
    }
};

struct SendInts : CoreIPC::Arguments2<const Vector<uint64_t>&, const Vector<Vector<uint64_t> >&> {
    static const Kind messageID = SendIntsID;
    SendInts(const Vector<uint64_t>& ints, const Vector<Vector<uint64_t> >& intVectors)
        : CoreIPC::Arguments2<const Vector<uint64_t>&, const Vector<Vector<uint64_t> >&>(ints, intVectors)
    {
    }
};

struct CreatePlugin : CoreIPC::Arguments2<uint64_t, const WebKit::Plugin::Parameters&> {
    static const Kind messageID = CreatePluginID;
    typedef CoreIPC::Arguments1<bool&> Reply;
    CreatePlugin(uint64_t pluginInstanceID, const WebKit::Plugin::Parameters& parameters)
        : CoreIPC::Arguments2<uint64_t, const WebKit::Plugin::Parameters&>(pluginInstanceID, parameters)
    {
    }
};

struct RunJavaScriptAlert : CoreIPC::Arguments2<uint64_t, const WTF::String&> {
    static const Kind messageID = RunJavaScriptAlertID;
    typedef CoreIPC::Arguments0 Reply;
    RunJavaScriptAlert(uint64_t frameID, const WTF::String& message)
        : CoreIPC::Arguments2<uint64_t, const WTF::String&>(frameID, message)
    {
    }
};

struct GetPlugins : CoreIPC::Arguments1<bool> {
    static const Kind messageID = GetPluginsID;
    typedef CoreIPC::Arguments1<Vector<WebCore::PluginInfo>&> Reply;
    explicit GetPlugins(bool refresh)
        : CoreIPC::Arguments1<bool>(refresh)
    {
    }
};

struct GetPluginProcessConnection : CoreIPC::Arguments1<const WTF::String&> {
    static const Kind messageID = GetPluginProcessConnectionID;
    typedef CoreIPC::Arguments1<CoreIPC::Connection::Handle&> Reply;
    explicit GetPluginProcessConnection(const WTF::String& pluginPath)
        : CoreIPC::Arguments1<const WTF::String&>(pluginPath)
    {
    }
};

struct DidCreateWebProcessConnection : CoreIPC::Arguments1<const CoreIPC::MachPort&> {
    static const Kind messageID = DidCreateWebProcessConnectionID;
    explicit DidCreateWebProcessConnection(const CoreIPC::MachPort& connectionIdentifier)
        : CoreIPC::Arguments1<const CoreIPC::MachPort&>(connectionIdentifier)
    {
    }
};

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

#if ENABLE(WEBKIT2)

#include "WebPage.h"

#include "ArgumentCoders.h"
#include "ArgumentDecoder.h"
#include "HandleMessage.h"
#include "MachPort.h"
#include "Plugin.h"
#include "WebCoreArgumentCoders.h"
#include "WebEvent.h"
#include "WebPageMessages.h"

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
    case Messages::WebPage::SendDoubleAndFloatID:
        CoreIPC::handleMessage<Messages::WebPage::SendDoubleAndFloat>(arguments, this, &WebPage::sendDoubleAndFloat);
        return;
    case Messages::WebPage::SendIntsID:
        CoreIPC::handleMessage<Messages::WebPage::SendInts>(arguments, this, &WebPage::sendInts);
        return;
    case Messages::WebPage::DidCreateWebProcessConnectionID:
        CoreIPC::handleMessage<Messages::WebPage::DidCreateWebProcessConnection>(arguments, this, &WebPage::didCreateWebProcessConnection);
        return;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
}

CoreIPC::SyncReplyMode WebPage::didReceiveSyncWebPageMessage(CoreIPC::Connection*, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments, CoreIPC::ArgumentEncoder* reply)
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
        CoreIPC::handleMessage<Messages::WebPage::GetPluginProcessConnection>(arguments, reply, this, &WebPage::getPluginProcessConnection);
        return CoreIPC::AutomaticReply;
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
