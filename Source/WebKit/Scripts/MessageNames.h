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
#if ENABLE(TEST_FEATURE)
    , WebPage_TestAsyncMessageWithConnection = 8
    , WebPage_TestAsyncMessageWithConnectionReply = 9
#endif
    , WebPage_TestSyncMessage = 10
    , WebPage_TestSynchronousMessage = 11
    , WebPage_LoadURL = 12
#if ENABLE(TOUCH_EVENTS)
    , WebPage_LoadSomething = 13
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION || SOME_OTHER_MESSAGE_CONDITION))
    , WebPage_TouchEvent = 14
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION && SOME_OTHER_MESSAGE_CONDITION))
    , WebPage_AddEvent = 15
#endif
#if ENABLE(TOUCH_EVENTS)
    , WebPage_LoadSomethingElse = 16
#endif
    , WebPage_DidReceivePolicyDecision = 17
    , WebPage_Close = 18
    , WebPage_PreferencesDidChange = 19
    , WebPage_SendDoubleAndFloat = 20
    , WebPage_SendInts = 21
    , WebPage_CreatePlugin = 22
    , WebPage_RunJavaScriptAlert = 23
    , WebPage_GetPlugins = 24
    , WebPage_GetPluginProcessConnection = 25
    , WebPage_TestMultipleAttributes = 26
    , WebPage_TestParameterAttributes = 27
    , WebPage_TemplateTest = 28
    , WebPage_SetVideoLayerID = 29
#if PLATFORM(MAC)
    , WebPage_DidCreateWebProcessConnection = 30
#endif
#if PLATFORM(MAC)
    , WebPage_InterpretKeyEvent = 31
#endif
#if ENABLE(DEPRECATED_FEATURE)
    , WebPage_DeprecatedOperation = 32
#endif
#if ENABLE(EXPERIMENTAL_FEATURE)
    , WebPage_ExperimentalOperation = 33
#endif
    , WebPage_LoadURL = 34
#if ENABLE(TOUCH_EVENTS)
    , WebPage_LoadSomething = 35
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION || SOME_OTHER_MESSAGE_CONDITION))
    , WebPage_TouchEvent = 36
#endif
#if (ENABLE(TOUCH_EVENTS) && (NESTED_MESSAGE_CONDITION && SOME_OTHER_MESSAGE_CONDITION))
    , WebPage_AddEvent = 37
#endif
#if ENABLE(TOUCH_EVENTS)
    , WebPage_LoadSomethingElse = 38
#endif
    , WebPage_DidReceivePolicyDecision = 39
    , WebPage_Close = 40
    , WebPage_PreferencesDidChange = 41
    , WebPage_SendDoubleAndFloat = 42
    , WebPage_SendInts = 43
    , WebPage_CreatePlugin = 44
    , WebPage_RunJavaScriptAlert = 45
    , WebPage_GetPlugins = 46
    , WebPage_GetPluginProcessConnection = 47
    , WebPage_TestMultipleAttributes = 48
    , WebPage_TestParameterAttributes = 49
    , WebPage_TemplateTest = 50
    , WebPage_SetVideoLayerID = 51
#if PLATFORM(MAC)
    , WebPage_DidCreateWebProcessConnection = 52
#endif
#if PLATFORM(MAC)
    , WebPage_InterpretKeyEvent = 53
#endif
#if ENABLE(DEPRECATED_FEATURE)
    , WebPage_DeprecatedOperation = 54
#endif
#if ENABLE(EXPERIMENTAL_FEATURE)
    , WebPage_ExperimentalOperation = 55
#endif
    , WrappedAsyncMessageForTesting = 56
    , SyncMessageReply = 57
    , InitializeConnection = 58
    , LegacySessionState = 59
};

ReceiverName receiverName(MessageName);
const char* description(MessageName);
bool isValidMessageName(MessageName);

} // namespace IPC

namespace WTF {

template<>
class HasCustomIsValidEnum<IPC::MessageName> : public std::true_type { };
template<typename E, typename T, std::enable_if_t<std::is_same_v<E, IPC::MessageName>>* = nullptr>
bool isValidEnum(T messageName)
{
    static_assert(sizeof(T) == sizeof(E), "isValidEnum<IPC::MessageName> should only be called with 16-bit types");
    return IPC::isValidMessageName(static_cast<E>(messageName));
};

} // namespace WTF
