/*
 * Copyright (C) 2010-2020 Apple Inc. All rights reserved.
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

#include <wtf/EnumTraits.h>

namespace IPC {

enum class ReceiverName : uint8_t {
    WebPage = 1
    , WebPage = 2
    , WebPage = 3
    , IPC = 4
    , AsyncReply = 5
    , Invalid = 6
};

enum class MessageName : uint16_t {
    WebPage_LoadURL = 1
#if ENABLE(TEST_FEATURE)
    , WebPage_TestAsyncMessage = 2
    , WebPage_TestAsyncMessageReply = 3
#endif
#if ENABLE(TEST_FEATURE)
    , WebPage_TestAsyncMessageWithNoArguments = 4
    , WebPage_TestAsyncMessageWithNoArgumentsReply = 5
#endif
#if ENABLE(TEST_FEATURE)
    , WebPage_TestAsyncMessageWithMultipleArguments = 6
    , WebPage_TestAsyncMessageWithMultipleArgumentsReply = 7
#endif
    , WebPage_TestSyncMessage = 8
    , WebPage_TestSynchronousMessage = 9
    , WebPage_LoadURL = 10
#if ENABLE(TOUCH_EVENTS)
    , WebPage_LoadSomething = 11
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION || SOME_OTHER_MESSAGE_CONDITION))
    , WebPage_TouchEvent = 12
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION && SOME_OTHER_MESSAGE_CONDITION))
    , WebPage_AddEvent = 13
#endif
#if ENABLE(TOUCH_EVENTS)
    , WebPage_LoadSomethingElse = 14
#endif
    , WebPage_DidReceivePolicyDecision = 15
    , WebPage_Close = 16
    , WebPage_PreferencesDidChange = 17
    , WebPage_SendDoubleAndFloat = 18
    , WebPage_SendInts = 19
    , WebPage_CreatePlugin = 20
    , WebPage_RunJavaScriptAlert = 21
    , WebPage_GetPlugins = 22
    , WebPage_GetPluginProcessConnection = 23
    , WebPage_TestMultipleAttributes = 24
    , WebPage_TestParameterAttributes = 25
    , WebPage_TemplateTest = 26
    , WebPage_SetVideoLayerID = 27
#if PLATFORM(MAC)
    , WebPage_DidCreateWebProcessConnection = 28
#endif
#if PLATFORM(MAC)
    , WebPage_InterpretKeyEvent = 29
#endif
#if ENABLE(DEPRECATED_FEATURE)
    , WebPage_DeprecatedOperation = 30
#endif
#if ENABLE(EXPERIMENTAL_FEATURE)
    , WebPage_ExperimentalOperation = 31
#endif
    , WebPage_LoadURL = 32
#if ENABLE(TOUCH_EVENTS)
    , WebPage_LoadSomething = 33
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION || SOME_OTHER_MESSAGE_CONDITION))
    , WebPage_TouchEvent = 34
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION && SOME_OTHER_MESSAGE_CONDITION))
    , WebPage_AddEvent = 35
#endif
#if ENABLE(TOUCH_EVENTS)
    , WebPage_LoadSomethingElse = 36
#endif
    , WebPage_DidReceivePolicyDecision = 37
    , WebPage_Close = 38
    , WebPage_PreferencesDidChange = 39
    , WebPage_SendDoubleAndFloat = 40
    , WebPage_SendInts = 41
    , WebPage_CreatePlugin = 42
    , WebPage_RunJavaScriptAlert = 43
    , WebPage_GetPlugins = 44
    , WebPage_GetPluginProcessConnection = 45
    , WebPage_TestMultipleAttributes = 46
    , WebPage_TestParameterAttributes = 47
    , WebPage_TemplateTest = 48
    , WebPage_SetVideoLayerID = 49
#if PLATFORM(MAC)
    , WebPage_DidCreateWebProcessConnection = 50
#endif
#if PLATFORM(MAC)
    , WebPage_InterpretKeyEvent = 51
#endif
#if ENABLE(DEPRECATED_FEATURE)
    , WebPage_DeprecatedOperation = 52
#endif
#if ENABLE(EXPERIMENTAL_FEATURE)
    , WebPage_ExperimentalOperation = 53
#endif
    , WrappedAsyncMessageForTesting = 54
    , SyncMessageReply = 55
    , InitializeConnection = 56
    , LegacySessionState = 57
};

ReceiverName receiverName(MessageName);
const char* description(MessageName);
bool isValidMessageName(MessageName);

} // namespace IPC

namespace WTF {

template<>
class HasCustomIsValidEnum<IPC::MessageName> : public std::true_type { };
template<typename E, typename T, typename = std::enable_if_t<std::is_same<E, IPC::MessageName>::value>>
bool isValidEnum(T messageName)
{
    static_assert(sizeof(T) == sizeof(IPC::MessageName), "isValidEnum<MessageName> should only be called with 16-bit types");
    return IPC::isValidMessageName(static_cast<IPC::MessageName>(messageName));
};

} // namespace WTF
