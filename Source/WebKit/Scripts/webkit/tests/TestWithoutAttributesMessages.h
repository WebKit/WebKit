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
namespace TestWithoutAttributes {

static inline IPC::ReceiverName messageReceiverName()
{
    return IPC::ReceiverName::TestWithoutAttributes;
}

class LoadURL {
public:
    using Arguments = std::tuple<String>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithoutAttributes_LoadURL; }
    static constexpr bool isSync = false;

    explicit LoadURL(const String& url)
        : m_arguments(url)
    {
    }

    const auto& arguments() const
    {
        return m_arguments;
    }

private:
    std::tuple<const String&> m_arguments;
};

#if ENABLE(TOUCH_EVENTS)
class LoadSomething {
public:
    using Arguments = std::tuple<String>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithoutAttributes_LoadSomething; }
    static constexpr bool isSync = false;

    explicit LoadSomething(const String& url)
        : m_arguments(url)
    {
    }

    const auto& arguments() const
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
    using Arguments = std::tuple<WebKit::WebTouchEvent>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithoutAttributes_TouchEvent; }
    static constexpr bool isSync = false;

    explicit TouchEvent(const WebKit::WebTouchEvent& event)
        : m_arguments(event)
    {
    }

    const auto& arguments() const
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
    using Arguments = std::tuple<WebKit::WebTouchEvent>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithoutAttributes_AddEvent; }
    static constexpr bool isSync = false;

    explicit AddEvent(const WebKit::WebTouchEvent& event)
        : m_arguments(event)
    {
    }

    const auto& arguments() const
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
    using Arguments = std::tuple<String>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithoutAttributes_LoadSomethingElse; }
    static constexpr bool isSync = false;

    explicit LoadSomethingElse(const String& url)
        : m_arguments(url)
    {
    }

    const auto& arguments() const
    {
        return m_arguments;
    }

private:
    std::tuple<const String&> m_arguments;
};
#endif

class DidReceivePolicyDecision {
public:
    using Arguments = std::tuple<uint64_t, uint64_t, uint32_t>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithoutAttributes_DidReceivePolicyDecision; }
    static constexpr bool isSync = false;

    DidReceivePolicyDecision(uint64_t frameID, uint64_t listenerID, uint32_t policyAction)
        : m_arguments(frameID, listenerID, policyAction)
    {
    }

    const auto& arguments() const
    {
        return m_arguments;
    }

private:
    std::tuple<uint64_t, uint64_t, uint32_t> m_arguments;
};

class Close {
public:
    using Arguments = std::tuple<>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithoutAttributes_Close; }
    static constexpr bool isSync = false;

    const auto& arguments() const
    {
        return m_arguments;
    }

private:
    std::tuple<> m_arguments;
};

class PreferencesDidChange {
public:
    using Arguments = std::tuple<WebKit::WebPreferencesStore>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithoutAttributes_PreferencesDidChange; }
    static constexpr bool isSync = false;

    explicit PreferencesDidChange(const WebKit::WebPreferencesStore& store)
        : m_arguments(store)
    {
    }

    const auto& arguments() const
    {
        return m_arguments;
    }

private:
    std::tuple<const WebKit::WebPreferencesStore&> m_arguments;
};

class SendDoubleAndFloat {
public:
    using Arguments = std::tuple<double, float>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithoutAttributes_SendDoubleAndFloat; }
    static constexpr bool isSync = false;

    SendDoubleAndFloat(double d, float f)
        : m_arguments(d, f)
    {
    }

    const auto& arguments() const
    {
        return m_arguments;
    }

private:
    std::tuple<double, float> m_arguments;
};

class SendInts {
public:
    using Arguments = std::tuple<Vector<uint64_t>, Vector<Vector<uint64_t>>>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithoutAttributes_SendInts; }
    static constexpr bool isSync = false;

    SendInts(const Vector<uint64_t>& ints, const Vector<Vector<uint64_t>>& intVectors)
        : m_arguments(ints, intVectors)
    {
    }

    const auto& arguments() const
    {
        return m_arguments;
    }

private:
    std::tuple<const Vector<uint64_t>&, const Vector<Vector<uint64_t>>&> m_arguments;
};

class CreatePlugin {
public:
    using Arguments = std::tuple<uint64_t, WebKit::Plugin::Parameters>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithoutAttributes_CreatePlugin; }
    static constexpr bool isSync = false;

    static IPC::MessageName asyncMessageReplyName() { return IPC::MessageName::TestWithoutAttributes_CreatePluginReply; }
    static constexpr auto callbackThread = WTF::CompletionHandlerCallThread::ConstructionThread;
    using ReplyArguments = std::tuple<bool>;
    CreatePlugin(uint64_t pluginInstanceID, const WebKit::Plugin::Parameters& parameters)
        : m_arguments(pluginInstanceID, parameters)
    {
    }

    const auto& arguments() const
    {
        return m_arguments;
    }

private:
    std::tuple<uint64_t, const WebKit::Plugin::Parameters&> m_arguments;
};

class RunJavaScriptAlert {
public:
    using Arguments = std::tuple<uint64_t, String>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithoutAttributes_RunJavaScriptAlert; }
    static constexpr bool isSync = false;

    static IPC::MessageName asyncMessageReplyName() { return IPC::MessageName::TestWithoutAttributes_RunJavaScriptAlertReply; }
    static constexpr auto callbackThread = WTF::CompletionHandlerCallThread::ConstructionThread;
    using ReplyArguments = std::tuple<>;
    RunJavaScriptAlert(uint64_t frameID, const String& message)
        : m_arguments(frameID, message)
    {
    }

    const auto& arguments() const
    {
        return m_arguments;
    }

private:
    std::tuple<uint64_t, const String&> m_arguments;
};

class GetPlugins {
public:
    using Arguments = std::tuple<bool>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithoutAttributes_GetPlugins; }
    static constexpr bool isSync = false;

    static IPC::MessageName asyncMessageReplyName() { return IPC::MessageName::TestWithoutAttributes_GetPluginsReply; }
    static constexpr auto callbackThread = WTF::CompletionHandlerCallThread::ConstructionThread;
    using ReplyArguments = std::tuple<Vector<WebCore::PluginInfo>>;
    explicit GetPlugins(bool refresh)
        : m_arguments(refresh)
    {
    }

    const auto& arguments() const
    {
        return m_arguments;
    }

private:
    std::tuple<bool> m_arguments;
};

class GetPluginProcessConnection {
public:
    using Arguments = std::tuple<String>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithoutAttributes_GetPluginProcessConnection; }
    static constexpr bool isSync = true;

    static constexpr auto callbackThread = WTF::CompletionHandlerCallThread::ConstructionThread;
    using ReplyArguments = std::tuple<IPC::Connection::Handle>;
    explicit GetPluginProcessConnection(const String& pluginPath)
        : m_arguments(pluginPath)
    {
    }

    const auto& arguments() const
    {
        return m_arguments;
    }

private:
    std::tuple<const String&> m_arguments;
};

class TestMultipleAttributes {
public:
    using Arguments = std::tuple<>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithoutAttributes_TestMultipleAttributes; }
    static constexpr bool isSync = true;

    static constexpr auto callbackThread = WTF::CompletionHandlerCallThread::ConstructionThread;
    using ReplyArguments = std::tuple<>;
    const auto& arguments() const
    {
        return m_arguments;
    }

private:
    std::tuple<> m_arguments;
};

class TestParameterAttributes {
public:
    using Arguments = std::tuple<uint64_t, double, double>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithoutAttributes_TestParameterAttributes; }
    static constexpr bool isSync = false;

    TestParameterAttributes(uint64_t foo, double bar, double baz)
        : m_arguments(foo, bar, baz)
    {
    }

    const auto& arguments() const
    {
        return m_arguments;
    }

private:
    std::tuple<uint64_t, double, double> m_arguments;
};

class TemplateTest {
public:
    using Arguments = std::tuple<HashMap<String, std::pair<String, uint64_t>>>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithoutAttributes_TemplateTest; }
    static constexpr bool isSync = false;

    explicit TemplateTest(const HashMap<String, std::pair<String, uint64_t>>& a)
        : m_arguments(a)
    {
    }

    const auto& arguments() const
    {
        return m_arguments;
    }

private:
    std::tuple<const HashMap<String, std::pair<String, uint64_t>>&> m_arguments;
};

class SetVideoLayerID {
public:
    using Arguments = std::tuple<WebCore::GraphicsLayer::PlatformLayerID>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithoutAttributes_SetVideoLayerID; }
    static constexpr bool isSync = false;

    explicit SetVideoLayerID(const WebCore::GraphicsLayer::PlatformLayerID& videoLayerID)
        : m_arguments(videoLayerID)
    {
    }

    const auto& arguments() const
    {
        return m_arguments;
    }

private:
    std::tuple<const WebCore::GraphicsLayer::PlatformLayerID&> m_arguments;
};

#if PLATFORM(MAC)
class DidCreateWebProcessConnection {
public:
    using Arguments = std::tuple<MachSendRight, OptionSet<WebKit::SelectionFlags>>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithoutAttributes_DidCreateWebProcessConnection; }
    static constexpr bool isSync = false;

    DidCreateWebProcessConnection(const MachSendRight& connectionIdentifier, const OptionSet<WebKit::SelectionFlags>& flags)
        : m_arguments(connectionIdentifier, flags)
    {
    }

    const auto& arguments() const
    {
        return m_arguments;
    }

private:
    std::tuple<const MachSendRight&, const OptionSet<WebKit::SelectionFlags>&> m_arguments;
};
#endif

#if PLATFORM(MAC)
class InterpretKeyEvent {
public:
    using Arguments = std::tuple<uint32_t>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithoutAttributes_InterpretKeyEvent; }
    static constexpr bool isSync = false;

    static IPC::MessageName asyncMessageReplyName() { return IPC::MessageName::TestWithoutAttributes_InterpretKeyEventReply; }
    static constexpr auto callbackThread = WTF::CompletionHandlerCallThread::ConstructionThread;
    using ReplyArguments = std::tuple<Vector<WebCore::KeypressCommand>>;
    explicit InterpretKeyEvent(uint32_t type)
        : m_arguments(type)
    {
    }

    const auto& arguments() const
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
    using Arguments = std::tuple<IPC::DummyType>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithoutAttributes_DeprecatedOperation; }
    static constexpr bool isSync = false;

    explicit DeprecatedOperation(const IPC::DummyType& dummy)
        : m_arguments(dummy)
    {
    }

    const auto& arguments() const
    {
        return m_arguments;
    }

private:
    std::tuple<const IPC::DummyType&> m_arguments;
};
#endif

#if ENABLE(FEATURE_FOR_TESTING)
class ExperimentalOperation {
public:
    using Arguments = std::tuple<IPC::DummyType>;

    static IPC::MessageName name() { return IPC::MessageName::TestWithoutAttributes_ExperimentalOperation; }
    static constexpr bool isSync = false;

    explicit ExperimentalOperation(const IPC::DummyType& dummy)
        : m_arguments(dummy)
    {
    }

    const auto& arguments() const
    {
        return m_arguments;
    }

private:
    std::tuple<const IPC::DummyType&> m_arguments;
};
#endif

} // namespace TestWithoutAttributes
} // namespace Messages

#endif // (ENABLE(WEBKIT2) && (NESTED_MASTER_CONDITION || MASTER_OR && MASTER_AND))
