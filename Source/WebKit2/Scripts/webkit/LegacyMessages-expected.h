/*
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

#if (ENABLE(WEBKIT2) && (NESTED_MASTER_CONDITION || MASTER_OR && MASTER_AND))

#include "Arguments.h"
#include "Connection.h"
#include "MessageEncoder.h"
#include "Plugin.h"
#include "StringReference.h"
#include <WebCore/GraphicsLayer.h>
#include <WebCore/KeyboardEvent.h>
#include <WebCore/PluginData.h>
#include <utility>
#include <wtf/HashMap.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace IPC {
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

static inline IPC::StringReference messageReceiverName()
{
    return IPC::StringReference("WebPage");
}

class LoadURL {
public:
    typedef std::tuple<String> DecodeType;

    static IPC::StringReference receiverName() { return messageReceiverName(); }
    static IPC::StringReference name() { return IPC::StringReference("LoadURL"); }
    static const bool isSync = false;

    explicit LoadURL(const String& url)
        : m_arguments(url)
    {
    }

    const std::tuple<const String&>& arguments() const
    {
        return m_arguments;
    }

private:
    std::tuple<const String&> m_arguments;
};

#if ENABLE(TOUCH_EVENTS)
class LoadSomething {
public:
    typedef std::tuple<String> DecodeType;

    static IPC::StringReference receiverName() { return messageReceiverName(); }
    static IPC::StringReference name() { return IPC::StringReference("LoadSomething"); }
    static const bool isSync = false;

    explicit LoadSomething(const String& url)
        : m_arguments(url)
    {
    }

    const std::tuple<const String&>& arguments() const
    {
        return m_arguments;
    }

private:
    std::tuple<const String&> m_arguments;
};
#endif

#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION || SOME_OTHER_MESSAGE_CONDITION))
class TouchEvent {
public:
    typedef std::tuple<WebKit::WebTouchEvent> DecodeType;

    static IPC::StringReference receiverName() { return messageReceiverName(); }
    static IPC::StringReference name() { return IPC::StringReference("TouchEvent"); }
    static const bool isSync = false;

    explicit TouchEvent(const WebKit::WebTouchEvent& event)
        : m_arguments(event)
    {
    }

    const std::tuple<const WebKit::WebTouchEvent&>& arguments() const
    {
        return m_arguments;
    }

private:
    std::tuple<const WebKit::WebTouchEvent&> m_arguments;
};
#endif

#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION && SOME_OTHER_MESSAGE_CONDITION))
class AddEvent {
public:
    typedef std::tuple<WebKit::WebTouchEvent> DecodeType;

    static IPC::StringReference receiverName() { return messageReceiverName(); }
    static IPC::StringReference name() { return IPC::StringReference("AddEvent"); }
    static const bool isSync = false;

    explicit AddEvent(const WebKit::WebTouchEvent& event)
        : m_arguments(event)
    {
    }

    const std::tuple<const WebKit::WebTouchEvent&>& arguments() const
    {
        return m_arguments;
    }

private:
    std::tuple<const WebKit::WebTouchEvent&> m_arguments;
};
#endif

#if ENABLE(TOUCH_EVENTS)
class LoadSomethingElse {
public:
    typedef std::tuple<String> DecodeType;

    static IPC::StringReference receiverName() { return messageReceiverName(); }
    static IPC::StringReference name() { return IPC::StringReference("LoadSomethingElse"); }
    static const bool isSync = false;

    explicit LoadSomethingElse(const String& url)
        : m_arguments(url)
    {
    }

    const std::tuple<const String&>& arguments() const
    {
        return m_arguments;
    }

private:
    std::tuple<const String&> m_arguments;
};
#endif

class DidReceivePolicyDecision {
public:
    typedef std::tuple<uint64_t, uint64_t, uint32_t> DecodeType;

    static IPC::StringReference receiverName() { return messageReceiverName(); }
    static IPC::StringReference name() { return IPC::StringReference("DidReceivePolicyDecision"); }
    static const bool isSync = false;

    DidReceivePolicyDecision(uint64_t frameID, uint64_t listenerID, uint32_t policyAction)
        : m_arguments(frameID, listenerID, policyAction)
    {
    }

    const std::tuple<uint64_t, uint64_t, uint32_t>& arguments() const
    {
        return m_arguments;
    }

private:
    std::tuple<uint64_t, uint64_t, uint32_t> m_arguments;
};

class Close {
public:
    typedef std::tuple<> DecodeType;

    static IPC::StringReference receiverName() { return messageReceiverName(); }
    static IPC::StringReference name() { return IPC::StringReference("Close"); }
    static const bool isSync = false;

    const std::tuple<>& arguments() const
    {
        return m_arguments;
    }

private:
    std::tuple<> m_arguments;
};

class PreferencesDidChange {
public:
    typedef std::tuple<WebKit::WebPreferencesStore> DecodeType;

    static IPC::StringReference receiverName() { return messageReceiverName(); }
    static IPC::StringReference name() { return IPC::StringReference("PreferencesDidChange"); }
    static const bool isSync = false;

    explicit PreferencesDidChange(const WebKit::WebPreferencesStore& store)
        : m_arguments(store)
    {
    }

    const std::tuple<const WebKit::WebPreferencesStore&>& arguments() const
    {
        return m_arguments;
    }

private:
    std::tuple<const WebKit::WebPreferencesStore&> m_arguments;
};

class SendDoubleAndFloat {
public:
    typedef std::tuple<double, float> DecodeType;

    static IPC::StringReference receiverName() { return messageReceiverName(); }
    static IPC::StringReference name() { return IPC::StringReference("SendDoubleAndFloat"); }
    static const bool isSync = false;

    SendDoubleAndFloat(double d, float f)
        : m_arguments(d, f)
    {
    }

    const std::tuple<double, float>& arguments() const
    {
        return m_arguments;
    }

private:
    std::tuple<double, float> m_arguments;
};

class SendInts {
public:
    typedef std::tuple<Vector<uint64_t>, Vector<Vector<uint64_t>>> DecodeType;

    static IPC::StringReference receiverName() { return messageReceiverName(); }
    static IPC::StringReference name() { return IPC::StringReference("SendInts"); }
    static const bool isSync = false;

    SendInts(const Vector<uint64_t>& ints, const Vector<Vector<uint64_t>>& intVectors)
        : m_arguments(ints, intVectors)
    {
    }

    const std::tuple<const Vector<uint64_t>&, const Vector<Vector<uint64_t>>&>& arguments() const
    {
        return m_arguments;
    }

private:
    std::tuple<const Vector<uint64_t>&, const Vector<Vector<uint64_t>>&> m_arguments;
};

class CreatePlugin {
public:
    typedef std::tuple<uint64_t, WebKit::Plugin::Parameters> DecodeType;

    static IPC::StringReference receiverName() { return messageReceiverName(); }
    static IPC::StringReference name() { return IPC::StringReference("CreatePlugin"); }
    static const bool isSync = true;

    typedef IPC::Arguments<bool&> Reply;
    CreatePlugin(uint64_t pluginInstanceID, const WebKit::Plugin::Parameters& parameters)
        : m_arguments(pluginInstanceID, parameters)
    {
    }

    const std::tuple<uint64_t, const WebKit::Plugin::Parameters&>& arguments() const
    {
        return m_arguments;
    }

private:
    std::tuple<uint64_t, const WebKit::Plugin::Parameters&> m_arguments;
};

class RunJavaScriptAlert {
public:
    typedef std::tuple<uint64_t, String> DecodeType;

    static IPC::StringReference receiverName() { return messageReceiverName(); }
    static IPC::StringReference name() { return IPC::StringReference("RunJavaScriptAlert"); }
    static const bool isSync = true;

    typedef IPC::Arguments<> Reply;
    RunJavaScriptAlert(uint64_t frameID, const String& message)
        : m_arguments(frameID, message)
    {
    }

    const std::tuple<uint64_t, const String&>& arguments() const
    {
        return m_arguments;
    }

private:
    std::tuple<uint64_t, const String&> m_arguments;
};

class GetPlugins {
public:
    typedef std::tuple<bool> DecodeType;

    static IPC::StringReference receiverName() { return messageReceiverName(); }
    static IPC::StringReference name() { return IPC::StringReference("GetPlugins"); }
    static const bool isSync = true;

    typedef IPC::Arguments<Vector<WebCore::PluginInfo>&> Reply;
    explicit GetPlugins(bool refresh)
        : m_arguments(refresh)
    {
    }

    const std::tuple<bool>& arguments() const
    {
        return m_arguments;
    }

private:
    std::tuple<bool> m_arguments;
};

class GetPluginProcessConnection {
public:
    typedef std::tuple<String> DecodeType;

    static IPC::StringReference receiverName() { return messageReceiverName(); }
    static IPC::StringReference name() { return IPC::StringReference("GetPluginProcessConnection"); }
    static const bool isSync = true;

    struct DelayedReply : public ThreadSafeRefCounted<DelayedReply> {
        DelayedReply(PassRefPtr<IPC::Connection>, std::unique_ptr<IPC::MessageEncoder>);
        ~DelayedReply();

        bool send(const IPC::Connection::Handle& connectionHandle);

    private:
        RefPtr<IPC::Connection> m_connection;
        std::unique_ptr<IPC::MessageEncoder> m_encoder;
    };

    typedef IPC::Arguments<IPC::Connection::Handle&> Reply;
    explicit GetPluginProcessConnection(const String& pluginPath)
        : m_arguments(pluginPath)
    {
    }

    const std::tuple<const String&>& arguments() const
    {
        return m_arguments;
    }

private:
    std::tuple<const String&> m_arguments;
};

class TestMultipleAttributes {
public:
    typedef std::tuple<> DecodeType;

    static IPC::StringReference receiverName() { return messageReceiverName(); }
    static IPC::StringReference name() { return IPC::StringReference("TestMultipleAttributes"); }
    static const bool isSync = true;

    struct DelayedReply : public ThreadSafeRefCounted<DelayedReply> {
        DelayedReply(PassRefPtr<IPC::Connection>, std::unique_ptr<IPC::MessageEncoder>);
        ~DelayedReply();

        bool send();

    private:
        RefPtr<IPC::Connection> m_connection;
        std::unique_ptr<IPC::MessageEncoder> m_encoder;
    };

    typedef IPC::Arguments<> Reply;
    const std::tuple<>& arguments() const
    {
        return m_arguments;
    }

private:
    std::tuple<> m_arguments;
};

class TestParameterAttributes {
public:
    typedef std::tuple<uint64_t, double, double> DecodeType;

    static IPC::StringReference receiverName() { return messageReceiverName(); }
    static IPC::StringReference name() { return IPC::StringReference("TestParameterAttributes"); }
    static const bool isSync = false;

    TestParameterAttributes(uint64_t foo, double bar, double baz)
        : m_arguments(foo, bar, baz)
    {
    }

    const std::tuple<uint64_t, double, double>& arguments() const
    {
        return m_arguments;
    }

private:
    std::tuple<uint64_t, double, double> m_arguments;
};

class TemplateTest {
public:
    typedef std::tuple<HashMap<String, std::pair<String, uint64_t>>> DecodeType;

    static IPC::StringReference receiverName() { return messageReceiverName(); }
    static IPC::StringReference name() { return IPC::StringReference("TemplateTest"); }
    static const bool isSync = false;

    explicit TemplateTest(const HashMap<String, std::pair<String, uint64_t>>& a)
        : m_arguments(a)
    {
    }

    const std::tuple<const HashMap<String, std::pair<String, uint64_t>>&>& arguments() const
    {
        return m_arguments;
    }

private:
    std::tuple<const HashMap<String, std::pair<String, uint64_t>>&> m_arguments;
};

class SetVideoLayerID {
public:
    typedef std::tuple<WebCore::GraphicsLayer::PlatformLayerID> DecodeType;

    static IPC::StringReference receiverName() { return messageReceiverName(); }
    static IPC::StringReference name() { return IPC::StringReference("SetVideoLayerID"); }
    static const bool isSync = false;

    explicit SetVideoLayerID(const WebCore::GraphicsLayer::PlatformLayerID& videoLayerID)
        : m_arguments(videoLayerID)
    {
    }

    const std::tuple<const WebCore::GraphicsLayer::PlatformLayerID&>& arguments() const
    {
        return m_arguments;
    }

private:
    std::tuple<const WebCore::GraphicsLayer::PlatformLayerID&> m_arguments;
};

#if PLATFORM(MAC)
class DidCreateWebProcessConnection {
public:
    typedef std::tuple<IPC::MachPort> DecodeType;

    static IPC::StringReference receiverName() { return messageReceiverName(); }
    static IPC::StringReference name() { return IPC::StringReference("DidCreateWebProcessConnection"); }
    static const bool isSync = false;

    explicit DidCreateWebProcessConnection(const IPC::MachPort& connectionIdentifier)
        : m_arguments(connectionIdentifier)
    {
    }

    const std::tuple<const IPC::MachPort&>& arguments() const
    {
        return m_arguments;
    }

private:
    std::tuple<const IPC::MachPort&> m_arguments;
};
#endif

#if PLATFORM(MAC)
class InterpretKeyEvent {
public:
    typedef std::tuple<uint32_t> DecodeType;

    static IPC::StringReference receiverName() { return messageReceiverName(); }
    static IPC::StringReference name() { return IPC::StringReference("InterpretKeyEvent"); }
    static const bool isSync = true;

    typedef IPC::Arguments<Vector<WebCore::KeypressCommand>&> Reply;
    explicit InterpretKeyEvent(uint32_t type)
        : m_arguments(type)
    {
    }

    const std::tuple<uint32_t>& arguments() const
    {
        return m_arguments;
    }

private:
    std::tuple<uint32_t> m_arguments;
};
#endif

#if ENABLE(DEPRECATED_FEATURE)
class DeprecatedOperation {
public:
    typedef std::tuple<IPC::DummyType> DecodeType;

    static IPC::StringReference receiverName() { return messageReceiverName(); }
    static IPC::StringReference name() { return IPC::StringReference("DeprecatedOperation"); }
    static const bool isSync = false;

    explicit DeprecatedOperation(const IPC::DummyType& dummy)
        : m_arguments(dummy)
    {
    }

    const std::tuple<const IPC::DummyType&>& arguments() const
    {
        return m_arguments;
    }

private:
    std::tuple<const IPC::DummyType&> m_arguments;
};
#endif

#if ENABLE(EXPERIMENTAL_FEATURE)
class ExperimentalOperation {
public:
    typedef std::tuple<IPC::DummyType> DecodeType;

    static IPC::StringReference receiverName() { return messageReceiverName(); }
    static IPC::StringReference name() { return IPC::StringReference("ExperimentalOperation"); }
    static const bool isSync = false;

    explicit ExperimentalOperation(const IPC::DummyType& dummy)
        : m_arguments(dummy)
    {
    }

    const std::tuple<const IPC::DummyType&>& arguments() const
    {
        return m_arguments;
    }

private:
    std::tuple<const IPC::DummyType&> m_arguments;
};
#endif

} // namespace WebPage
} // namespace Messages

#endif // (ENABLE(WEBKIT2) && (NESTED_MASTER_CONDITION || MASTER_OR && MASTER_AND))

#endif // WebPageMessages_h
