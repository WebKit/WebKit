/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "Plugin.h"
#include "TestWithLegacyReceiverMessagesReplies.h"
#include <WebCore/GraphicsLayer.h>
#include <WebCore/KeyboardEvent.h>
#include <WebCore/PluginData.h>
#include <utility>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/MachSendRight.h>
#include <wtf/OptionSet.h>
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
    using Arguments = std::tuple<const String&>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithLegacyReceiver_LoadURL; }
    static constexpr bool isSync = false;

    explicit LoadURL(const String& url)
        : m_arguments(url)
    {
    }

    const Arguments& arguments() const
    {
        return m_arguments;
    }

private:
    Arguments m_arguments;
};

#if ENABLE(TOUCH_EVENTS)
class LoadSomething {
public:
    using Arguments = std::tuple<const String&>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithLegacyReceiver_LoadSomething; }
    static constexpr bool isSync = false;

    explicit LoadSomething(const String& url)
        : m_arguments(url)
    {
    }

    const Arguments& arguments() const
    {
        return m_arguments;
    }

private:
    Arguments m_arguments;
};
#endif

#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION || SOME_OTHER_MESSAGE_CONDITION))
class TouchEvent {
public:
    using Arguments = std::tuple<const WebKit::WebTouchEvent&>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithLegacyReceiver_TouchEvent; }
    static constexpr bool isSync = false;

    explicit TouchEvent(const WebKit::WebTouchEvent& event)
        : m_arguments(event)
    {
    }

    const Arguments& arguments() const
    {
        return m_arguments;
    }

private:
    Arguments m_arguments;
};
#endif

#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION && SOME_OTHER_MESSAGE_CONDITION))
class AddEvent {
public:
    using Arguments = std::tuple<const WebKit::WebTouchEvent&>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithLegacyReceiver_AddEvent; }
    static constexpr bool isSync = false;

    explicit AddEvent(const WebKit::WebTouchEvent& event)
        : m_arguments(event)
    {
    }

    const Arguments& arguments() const
    {
        return m_arguments;
    }

private:
    Arguments m_arguments;
};
#endif

#if ENABLE(TOUCH_EVENTS)
class LoadSomethingElse {
public:
    using Arguments = std::tuple<const String&>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithLegacyReceiver_LoadSomethingElse; }
    static constexpr bool isSync = false;

    explicit LoadSomethingElse(const String& url)
        : m_arguments(url)
    {
    }

    const Arguments& arguments() const
    {
        return m_arguments;
    }

private:
    Arguments m_arguments;
};
#endif

class DidReceivePolicyDecision {
public:
    using Arguments = std::tuple<uint64_t, uint64_t, uint32_t>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithLegacyReceiver_DidReceivePolicyDecision; }
    static constexpr bool isSync = false;

    DidReceivePolicyDecision(uint64_t frameID, uint64_t listenerID, uint32_t policyAction)
        : m_arguments(frameID, listenerID, policyAction)
    {
    }

    const Arguments& arguments() const
    {
        return m_arguments;
    }

private:
    Arguments m_arguments;
};

class Close {
public:
    using Arguments = std::tuple<>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithLegacyReceiver_Close; }
    static constexpr bool isSync = false;

    const Arguments& arguments() const
    {
        return m_arguments;
    }

private:
    Arguments m_arguments;
};

class PreferencesDidChange {
public:
    using Arguments = std::tuple<const WebKit::WebPreferencesStore&>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithLegacyReceiver_PreferencesDidChange; }
    static constexpr bool isSync = false;

    explicit PreferencesDidChange(const WebKit::WebPreferencesStore& store)
        : m_arguments(store)
    {
    }

    const Arguments& arguments() const
    {
        return m_arguments;
    }

private:
    Arguments m_arguments;
};

class SendDoubleAndFloat {
public:
    using Arguments = std::tuple<double, float>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithLegacyReceiver_SendDoubleAndFloat; }
    static constexpr bool isSync = false;

    SendDoubleAndFloat(double d, float f)
        : m_arguments(d, f)
    {
    }

    const Arguments& arguments() const
    {
        return m_arguments;
    }

private:
    Arguments m_arguments;
};

class SendInts {
public:
    using Arguments = std::tuple<const Vector<uint64_t>&, const Vector<Vector<uint64_t>>&>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithLegacyReceiver_SendInts; }
    static constexpr bool isSync = false;

    SendInts(const Vector<uint64_t>& ints, const Vector<Vector<uint64_t>>& intVectors)
        : m_arguments(ints, intVectors)
    {
    }

    const Arguments& arguments() const
    {
        return m_arguments;
    }

private:
    Arguments m_arguments;
};

class CreatePlugin {
public:
    using Arguments = std::tuple<uint64_t, const WebKit::Plugin::Parameters&>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithLegacyReceiver_CreatePlugin; }
    static constexpr bool isSync = false;

    static void callReply(IPC::Decoder&, CompletionHandler<void(bool&&)>&&);
    static void cancelReply(CompletionHandler<void(bool&&)>&&);
    static IPC::MessageName asyncMessageReplyName() { return IPC::MessageName::TestWithLegacyReceiver_CreatePluginReply; }
    using AsyncReply = CreatePluginAsyncReply;
    static constexpr auto callbackThread = WTF::CompletionHandlerCallThread::ConstructionThread;
    using ReplyArguments = std::tuple<bool>;
    CreatePlugin(uint64_t pluginInstanceID, const WebKit::Plugin::Parameters& parameters)
        : m_arguments(pluginInstanceID, parameters)
    {
    }

    const Arguments& arguments() const
    {
        return m_arguments;
    }

private:
    Arguments m_arguments;
};

class RunJavaScriptAlert {
public:
    using Arguments = std::tuple<uint64_t, const String&>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithLegacyReceiver_RunJavaScriptAlert; }
    static constexpr bool isSync = false;

    static void callReply(IPC::Decoder&, CompletionHandler<void()>&&);
    static void cancelReply(CompletionHandler<void()>&&);
    static IPC::MessageName asyncMessageReplyName() { return IPC::MessageName::TestWithLegacyReceiver_RunJavaScriptAlertReply; }
    using AsyncReply = RunJavaScriptAlertAsyncReply;
    static constexpr auto callbackThread = WTF::CompletionHandlerCallThread::ConstructionThread;
    using ReplyArguments = std::tuple<>;
    RunJavaScriptAlert(uint64_t frameID, const String& message)
        : m_arguments(frameID, message)
    {
    }

    const Arguments& arguments() const
    {
        return m_arguments;
    }

private:
    Arguments m_arguments;
};

class GetPlugins {
public:
    using Arguments = std::tuple<bool>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithLegacyReceiver_GetPlugins; }
    static constexpr bool isSync = false;

    static void callReply(IPC::Decoder&, CompletionHandler<void(Vector<WebCore::PluginInfo>&&)>&&);
    static void cancelReply(CompletionHandler<void(Vector<WebCore::PluginInfo>&&)>&&);
    static IPC::MessageName asyncMessageReplyName() { return IPC::MessageName::TestWithLegacyReceiver_GetPluginsReply; }
    using AsyncReply = GetPluginsAsyncReply;
    static constexpr auto callbackThread = WTF::CompletionHandlerCallThread::ConstructionThread;
    using ReplyArguments = std::tuple<Vector<WebCore::PluginInfo>>;
    explicit GetPlugins(bool refresh)
        : m_arguments(refresh)
    {
    }

    const Arguments& arguments() const
    {
        return m_arguments;
    }

private:
    Arguments m_arguments;
};

class GetPluginProcessConnection {
public:
    using Arguments = std::tuple<const String&>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithLegacyReceiver_GetPluginProcessConnection; }
    static constexpr bool isSync = true;

    using DelayedReply = GetPluginProcessConnectionDelayedReply;
    static constexpr auto callbackThread = WTF::CompletionHandlerCallThread::ConstructionThread;
    using ReplyArguments = std::tuple<IPC::Connection::Handle>;
    explicit GetPluginProcessConnection(const String& pluginPath)
        : m_arguments(pluginPath)
    {
    }

    const Arguments& arguments() const
    {
        return m_arguments;
    }

private:
    Arguments m_arguments;
};

class TestMultipleAttributes {
public:
    using Arguments = std::tuple<>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithLegacyReceiver_TestMultipleAttributes; }
    static constexpr bool isSync = true;

    using DelayedReply = TestMultipleAttributesDelayedReply;
    static constexpr auto callbackThread = WTF::CompletionHandlerCallThread::ConstructionThread;
    using ReplyArguments = std::tuple<>;
    const Arguments& arguments() const
    {
        return m_arguments;
    }

private:
    Arguments m_arguments;
};

class TestParameterAttributes {
public:
    using Arguments = std::tuple<uint64_t, double, double>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithLegacyReceiver_TestParameterAttributes; }
    static constexpr bool isSync = false;

    TestParameterAttributes(uint64_t foo, double bar, double baz)
        : m_arguments(foo, bar, baz)
    {
    }

    const Arguments& arguments() const
    {
        return m_arguments;
    }

private:
    Arguments m_arguments;
};

class TemplateTest {
public:
    using Arguments = std::tuple<const HashMap<String, std::pair<String, uint64_t>>&>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithLegacyReceiver_TemplateTest; }
    static constexpr bool isSync = false;

    explicit TemplateTest(const HashMap<String, std::pair<String, uint64_t>>& a)
        : m_arguments(a)
    {
    }

    const Arguments& arguments() const
    {
        return m_arguments;
    }

private:
    Arguments m_arguments;
};

class SetVideoLayerID {
public:
    using Arguments = std::tuple<const WebCore::GraphicsLayer::PlatformLayerID&>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithLegacyReceiver_SetVideoLayerID; }
    static constexpr bool isSync = false;

    explicit SetVideoLayerID(const WebCore::GraphicsLayer::PlatformLayerID& videoLayerID)
        : m_arguments(videoLayerID)
    {
    }

    const Arguments& arguments() const
    {
        return m_arguments;
    }

private:
    Arguments m_arguments;
};

#if PLATFORM(MAC)
class DidCreateWebProcessConnection {
public:
    using Arguments = std::tuple<const MachSendRight&, const OptionSet<WebKit::SelectionFlags>&>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithLegacyReceiver_DidCreateWebProcessConnection; }
    static constexpr bool isSync = false;

    DidCreateWebProcessConnection(const MachSendRight& connectionIdentifier, const OptionSet<WebKit::SelectionFlags>& flags)
        : m_arguments(connectionIdentifier, flags)
    {
    }

    const Arguments& arguments() const
    {
        return m_arguments;
    }

private:
    Arguments m_arguments;
};
#endif

#if PLATFORM(MAC)
class InterpretKeyEvent {
public:
    using Arguments = std::tuple<uint32_t>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithLegacyReceiver_InterpretKeyEvent; }
    static constexpr bool isSync = false;

    static void callReply(IPC::Decoder&, CompletionHandler<void(Vector<WebCore::KeypressCommand>&&)>&&);
    static void cancelReply(CompletionHandler<void(Vector<WebCore::KeypressCommand>&&)>&&);
    static IPC::MessageName asyncMessageReplyName() { return IPC::MessageName::TestWithLegacyReceiver_InterpretKeyEventReply; }
    using AsyncReply = InterpretKeyEventAsyncReply;
    static constexpr auto callbackThread = WTF::CompletionHandlerCallThread::ConstructionThread;
    using ReplyArguments = std::tuple<Vector<WebCore::KeypressCommand>>;
    explicit InterpretKeyEvent(uint32_t type)
        : m_arguments(type)
    {
    }

    const Arguments& arguments() const
    {
        return m_arguments;
    }

private:
    Arguments m_arguments;
};
#endif

#if ENABLE(DEPRECATED_FEATURE)
class DeprecatedOperation {
public:
    using Arguments = std::tuple<const IPC::DummyType&>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithLegacyReceiver_DeprecatedOperation; }
    static constexpr bool isSync = false;

    explicit DeprecatedOperation(const IPC::DummyType& dummy)
        : m_arguments(dummy)
    {
    }

    const Arguments& arguments() const
    {
        return m_arguments;
    }

private:
    Arguments m_arguments;
};
#endif

#if ENABLE(FEATURE_FOR_TESTING)
class ExperimentalOperation {
public:
    using Arguments = std::tuple<const IPC::DummyType&>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithLegacyReceiver_ExperimentalOperation; }
    static constexpr bool isSync = false;

    explicit ExperimentalOperation(const IPC::DummyType& dummy)
        : m_arguments(dummy)
    {
    }

    const Arguments& arguments() const
    {
        return m_arguments;
    }

private:
    Arguments m_arguments;
};
#endif

} // namespace TestWithLegacyReceiver
} // namespace Messages

#endif // (ENABLE(WEBKIT2) && (NESTED_MASTER_CONDITION || MASTER_OR && MASTER_AND))
