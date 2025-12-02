/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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

#pragma once

#if (ENABLE(WEBKIT2) && (NESTED_MASTER_CONDITION || MASTER_OR && MASTER_AND))

#include "ArgumentCoders.h"
#include "Connection.h"
#if PLATFORM(IOS_FAMILY)
#include "GestureTypes.h"
#endif
#include "MessageNames.h"
#include <WebCore/KeyboardEvent.h>
#include <WebCore/PlatformLayerIdentifier.h>
#include <WebCore/PluginData.h>
#include <utility>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/MachSendRight.h>
#include <wtf/OptionSet.h>
#include <wtf/RuntimeApplicationChecks.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace IPC {
class DummyType;
}

namespace WebKit {
class WebPreferencesStore;
class WebTouchEvent;
}

namespace Messages {
namespace TestWithLegacyReceiver {

static inline IPC::ReceiverName messageReceiverName()
{
    return IPC::ReceiverName::TestWithLegacyReceiver;
}

class LoadURL {
public:
    using Arguments = std::tuple<String>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithLegacyReceiver_LoadURL; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    explicit LoadURL(const String& url)
        : m_url(url)
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        encoder << m_url;
    }

private:
    const String& m_url;
};

#if ENABLE(TOUCH_EVENTS)
class LoadSomething {
public:
    using Arguments = std::tuple<String>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithLegacyReceiver_LoadSomething; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    explicit LoadSomething(const String& url)
        : m_url(url)
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        encoder << m_url;
    }

private:
    const String& m_url;
};
#endif

#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION || SOME_OTHER_MESSAGE_CONDITION))
class TouchEvent {
public:
    using Arguments = std::tuple<WebKit::WebTouchEvent>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithLegacyReceiver_TouchEvent; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    explicit TouchEvent(const WebKit::WebTouchEvent& event)
        : m_event(event)
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        SUPPRESS_FORWARD_DECL_ARG encoder << m_event;
    }

private:
    SUPPRESS_FORWARD_DECL_MEMBER const WebKit::WebTouchEvent& m_event;
};
#endif

#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION && SOME_OTHER_MESSAGE_CONDITION))
class AddEvent {
public:
    using Arguments = std::tuple<WebKit::WebTouchEvent>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithLegacyReceiver_AddEvent; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    explicit AddEvent(const WebKit::WebTouchEvent& event)
        : m_event(event)
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        SUPPRESS_FORWARD_DECL_ARG encoder << m_event;
    }

private:
    SUPPRESS_FORWARD_DECL_MEMBER const WebKit::WebTouchEvent& m_event;
};
#endif

#if ENABLE(TOUCH_EVENTS)
class LoadSomethingElse {
public:
    using Arguments = std::tuple<String>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithLegacyReceiver_LoadSomethingElse; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    explicit LoadSomethingElse(const String& url)
        : m_url(url)
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        encoder << m_url;
    }

private:
    const String& m_url;
};
#endif

class DidReceivePolicyDecision {
public:
    using Arguments = std::tuple<uint64_t, uint64_t, uint32_t>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithLegacyReceiver_DidReceivePolicyDecision; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    DidReceivePolicyDecision(uint64_t frameID, uint64_t listenerID, uint32_t policyAction)
        : m_frameID(frameID)
        , m_listenerID(listenerID)
        , m_policyAction(policyAction)
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        encoder << m_frameID;
        encoder << m_listenerID;
        encoder << m_policyAction;
    }

private:
    uint64_t m_frameID;
    uint64_t m_listenerID;
    uint32_t m_policyAction;
};

class Close {
public:
    using Arguments = std::tuple<>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithLegacyReceiver_Close; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    Close()
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
    }

private:
};

class PreferencesDidChange {
public:
    using Arguments = std::tuple<WebKit::WebPreferencesStore>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithLegacyReceiver_PreferencesDidChange; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    explicit PreferencesDidChange(const WebKit::WebPreferencesStore& store)
        : m_store(store)
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        SUPPRESS_FORWARD_DECL_ARG encoder << m_store;
    }

private:
    SUPPRESS_FORWARD_DECL_MEMBER const WebKit::WebPreferencesStore& m_store;
};

class SendDoubleAndFloat {
public:
    using Arguments = std::tuple<double, float>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithLegacyReceiver_SendDoubleAndFloat; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    SendDoubleAndFloat(double d, float f)
        : m_d(d)
        , m_f(f)
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        encoder << m_d;
        encoder << m_f;
    }

private:
    double m_d;
    float m_f;
};

class SendInts {
public:
    using Arguments = std::tuple<Vector<uint64_t>, Vector<Vector<uint64_t>>>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithLegacyReceiver_SendInts; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    SendInts(const Vector<uint64_t>& ints, const Vector<Vector<uint64_t>>& intVectors)
        : m_ints(ints)
        , m_intVectors(intVectors)
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        SUPPRESS_FORWARD_DECL_ARG encoder << m_ints;
        SUPPRESS_FORWARD_DECL_ARG encoder << m_intVectors;
    }

private:
    SUPPRESS_FORWARD_DECL_MEMBER const Vector<uint64_t>& m_ints;
    SUPPRESS_FORWARD_DECL_MEMBER const Vector<Vector<uint64_t>>& m_intVectors;
};

class CreatePlugin {
public:
    using Arguments = std::tuple<uint64_t, WebKit::Plugin::Parameters>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithLegacyReceiver_CreatePlugin; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    static IPC::MessageName asyncMessageReplyName() { return IPC::MessageName::TestWithLegacyReceiver_CreatePluginReply; }
    static constexpr auto callbackThread = WTF::CompletionHandlerCallThread::ConstructionThread;
    using ReplyArguments = std::tuple<bool>;
    using Reply = CompletionHandler<void(bool)>;
    using Promise = WTF::NativePromise<bool, IPC::Error>;
    CreatePlugin(uint64_t pluginInstanceID, const WebKit::Plugin::Parameters& parameters)
        : m_pluginInstanceID(pluginInstanceID)
        , m_parameters(parameters)
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        encoder << m_pluginInstanceID;
        SUPPRESS_FORWARD_DECL_ARG encoder << m_parameters;
    }

private:
    uint64_t m_pluginInstanceID;
    SUPPRESS_FORWARD_DECL_MEMBER const WebKit::Plugin::Parameters& m_parameters;
};

class RunJavaScriptAlert {
public:
    using Arguments = std::tuple<uint64_t, String>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithLegacyReceiver_RunJavaScriptAlert; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    static IPC::MessageName asyncMessageReplyName() { return IPC::MessageName::TestWithLegacyReceiver_RunJavaScriptAlertReply; }
    static constexpr auto callbackThread = WTF::CompletionHandlerCallThread::ConstructionThread;
    using ReplyArguments = std::tuple<>;
    using Reply = CompletionHandler<void()>;
    using Promise = WTF::NativePromise<void, IPC::Error>;
    RunJavaScriptAlert(uint64_t frameID, const String& message)
        : m_frameID(frameID)
        , m_message(message)
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        encoder << m_frameID;
        encoder << m_message;
    }

private:
    uint64_t m_frameID;
    const String& m_message;
};

class GetPlugins {
public:
    using Arguments = std::tuple<bool>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithLegacyReceiver_GetPlugins; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    static IPC::MessageName asyncMessageReplyName() { return IPC::MessageName::TestWithLegacyReceiver_GetPluginsReply; }
    static constexpr auto callbackThread = WTF::CompletionHandlerCallThread::ConstructionThread;
    using ReplyArguments = std::tuple<Vector<WebCore::PluginInfo>>;
    using Reply = CompletionHandler<void(Vector<WebCore::PluginInfo>&&)>;
    using Promise = WTF::NativePromise<Vector<WebCore::PluginInfo>, IPC::Error>;
    explicit GetPlugins(bool refresh)
        : m_refresh(refresh)
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        encoder << m_refresh;
    }

private:
    bool m_refresh;
};

class GetPluginProcessConnection {
public:
    using Arguments = std::tuple<String>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithLegacyReceiver_GetPluginProcessConnection; }
    static constexpr bool isSync = true;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    static constexpr auto callbackThread = WTF::CompletionHandlerCallThread::ConstructionThread;
    using ReplyArguments = std::tuple<IPC::Connection::Handle>;
    using Reply = CompletionHandler<void(IPC::Connection::Handle&&)>;
    explicit GetPluginProcessConnection(const String& pluginPath)
        : m_pluginPath(pluginPath)
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        encoder << m_pluginPath;
    }

private:
    const String& m_pluginPath;
};

class TestMultipleAttributes {
public:
    using Arguments = std::tuple<>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithLegacyReceiver_TestMultipleAttributes; }
    static constexpr bool isSync = true;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    static constexpr auto callbackThread = WTF::CompletionHandlerCallThread::ConstructionThread;
    using ReplyArguments = std::tuple<>;
    using Reply = CompletionHandler<void()>;
    TestMultipleAttributes()
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
    }

private:
};

class TestParameterAttributes {
public:
    using Arguments = std::tuple<uint64_t, double, double>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithLegacyReceiver_TestParameterAttributes; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    TestParameterAttributes(uint64_t foo, double bar, double baz)
        : m_foo(foo)
        , m_bar(bar)
        , m_baz(baz)
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        encoder << m_foo;
        encoder << m_bar;
        encoder << m_baz;
    }

private:
    uint64_t m_foo;
    double m_bar;
    double m_baz;
};

class TemplateTest {
public:
    using Arguments = std::tuple<HashMap<String, std::pair<String, uint64_t>>>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithLegacyReceiver_TemplateTest; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    explicit TemplateTest(const HashMap<String, std::pair<String, uint64_t>>& a)
        : m_a(a)
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        SUPPRESS_FORWARD_DECL_ARG encoder << m_a;
    }

private:
    SUPPRESS_FORWARD_DECL_MEMBER const HashMap<String, std::pair<String, uint64_t>>& m_a;
};

class SetVideoLayerID {
public:
    using Arguments = std::tuple<WebCore::PlatformLayerIdentifier>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithLegacyReceiver_SetVideoLayerID; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    explicit SetVideoLayerID(const WebCore::PlatformLayerIdentifier& videoLayerID)
        : m_videoLayerID(videoLayerID)
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        encoder << m_videoLayerID;
    }

private:
    const WebCore::PlatformLayerIdentifier& m_videoLayerID;
};

class OpaqueTypeSecurityAssertion {
public:
    using Arguments = std::tuple<NotDispatchableFromWebContent>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithLegacyReceiver_OpaqueTypeSecurityAssertion; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    static IPC::MessageName asyncMessageReplyName() { return IPC::MessageName::TestWithLegacyReceiver_OpaqueTypeSecurityAssertionReply; }
    static constexpr auto callbackThread = WTF::CompletionHandlerCallThread::ConstructionThread;
    using ReplyArguments = std::tuple<NotDispatchableFromWebContent>;
    using Reply = CompletionHandler<void(NotDispatchableFromWebContent&&)>;
    using Promise = WTF::NativePromise<NotDispatchableFromWebContent, IPC::Error>;
    explicit OpaqueTypeSecurityAssertion(const NotDispatchableFromWebContent& ping)
        : m_ping(ping)
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        ASSERT(!isInWebProcess());
        SUPPRESS_FORWARD_DECL_ARG encoder << m_ping;
    }

private:
    SUPPRESS_FORWARD_DECL_MEMBER const NotDispatchableFromWebContent& m_ping;
};

#if PLATFORM(MAC)
class DidCreateWebProcessConnection {
public:
    using Arguments = std::tuple<MachSendRight, OptionSet<WebKit::SelectionFlags>>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithLegacyReceiver_DidCreateWebProcessConnection; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    DidCreateWebProcessConnection(MachSendRight&& connectionIdentifier, const OptionSet<WebKit::SelectionFlags>& flags)
        : m_connectionIdentifier(WTFMove(connectionIdentifier))
        , m_flags(flags)
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        encoder << WTFMove(m_connectionIdentifier);
        SUPPRESS_FORWARD_DECL_ARG encoder << m_flags;
    }

private:
    MachSendRight&& m_connectionIdentifier;
    SUPPRESS_FORWARD_DECL_MEMBER const OptionSet<WebKit::SelectionFlags>& m_flags;
};
#endif

#if PLATFORM(MAC)
class InterpretKeyEvent {
public:
    using Arguments = std::tuple<uint32_t>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithLegacyReceiver_InterpretKeyEvent; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    static IPC::MessageName asyncMessageReplyName() { return IPC::MessageName::TestWithLegacyReceiver_InterpretKeyEventReply; }
    static constexpr auto callbackThread = WTF::CompletionHandlerCallThread::ConstructionThread;
    using ReplyArguments = std::tuple<Vector<WebCore::KeypressCommand>>;
    using Reply = CompletionHandler<void(Vector<WebCore::KeypressCommand>&&)>;
    using Promise = WTF::NativePromise<Vector<WebCore::KeypressCommand>, IPC::Error>;
    explicit InterpretKeyEvent(uint32_t type)
        : m_type(type)
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        encoder << m_type;
    }

private:
    uint32_t m_type;
};
#endif

#if ENABLE(DEPRECATED_FEATURE)
class DeprecatedOperation {
public:
    using Arguments = std::tuple<IPC::DummyType>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithLegacyReceiver_DeprecatedOperation; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    explicit DeprecatedOperation(const IPC::DummyType& dummy)
        : m_dummy(dummy)
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        SUPPRESS_FORWARD_DECL_ARG encoder << m_dummy;
    }

private:
    SUPPRESS_FORWARD_DECL_MEMBER const IPC::DummyType& m_dummy;
};
#endif

#if ENABLE(FEATURE_FOR_TESTING)
class ExperimentalOperation {
public:
    using Arguments = std::tuple<IPC::DummyType>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithLegacyReceiver_ExperimentalOperation; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    explicit ExperimentalOperation(const IPC::DummyType& dummy)
        : m_dummy(dummy)
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        SUPPRESS_FORWARD_DECL_ARG encoder << m_dummy;
    }

private:
    SUPPRESS_FORWARD_DECL_MEMBER const IPC::DummyType& m_dummy;
};
#endif

class CreatePluginReply {
public:
    using Arguments = std::tuple<bool>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithLegacyReceiver_CreatePluginReply; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    explicit CreatePluginReply(bool result)
        : m_result(result)
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        encoder << m_result;
    }

private:
    bool m_result;
};

class RunJavaScriptAlertReply {
public:
    using Arguments = std::tuple<>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithLegacyReceiver_RunJavaScriptAlertReply; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    RunJavaScriptAlertReply()
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
    }

private:
};

class GetPluginsReply {
public:
    using Arguments = std::tuple<Vector<WebCore::PluginInfo>>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithLegacyReceiver_GetPluginsReply; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    explicit GetPluginsReply(const Vector<WebCore::PluginInfo>& plugins)
        : m_plugins(plugins)
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        SUPPRESS_FORWARD_DECL_ARG encoder << m_plugins;
    }

private:
    SUPPRESS_FORWARD_DECL_MEMBER const Vector<WebCore::PluginInfo>& m_plugins;
};

class OpaqueTypeSecurityAssertionReply {
public:
    using Arguments = std::tuple<NotDispatchableFromWebContent>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithLegacyReceiver_OpaqueTypeSecurityAssertionReply; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    explicit OpaqueTypeSecurityAssertionReply(const NotDispatchableFromWebContent& pong)
        : m_pong(pong)
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        ASSERT(!isInWebProcess());
        SUPPRESS_FORWARD_DECL_ARG encoder << m_pong;
    }

private:
    SUPPRESS_FORWARD_DECL_MEMBER const NotDispatchableFromWebContent& m_pong;
};

#if PLATFORM(MAC)
class InterpretKeyEventReply {
public:
    using Arguments = std::tuple<Vector<WebCore::KeypressCommand>>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithLegacyReceiver_InterpretKeyEventReply; }
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr bool deferSendingIfSuspended = false;

    explicit InterpretKeyEventReply(const Vector<WebCore::KeypressCommand>& commandName)
        : m_commandName(commandName)
    {
    }

    template<typename Encoder>
    void encode(Encoder& encoder)
    {
        SUPPRESS_FORWARD_DECL_ARG encoder << m_commandName;
    }

private:
    SUPPRESS_FORWARD_DECL_MEMBER const Vector<WebCore::KeypressCommand>& m_commandName;
};
#endif

} // namespace TestWithLegacyReceiver
} // namespace Messages

#endif // (ENABLE(WEBKIT2) && (NESTED_MASTER_CONDITION || MASTER_OR && MASTER_AND))
