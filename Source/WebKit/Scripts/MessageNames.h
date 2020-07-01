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
    WebPage_AddEvent
    , WebPage_Close
    , WebPage_CreatePlugin
    , WebPage_DeprecatedOperation
    , WebPage_DidCreateWebProcessConnection
    , WebPage_DidReceivePolicyDecision
    , WebPage_ExperimentalOperation
    , WebPage_GetPluginProcessConnection
    , WebPage_GetPlugins
    , WebPage_InterpretKeyEvent
    , WebPage_LoadSomething
    , WebPage_LoadSomethingElse
    , WebPage_LoadURL
    , WebPage_PreferencesDidChange
    , WebPage_RunJavaScriptAlert
    , WebPage_SendDoubleAndFloat
    , WebPage_SendInts
    , WebPage_SetVideoLayerID
    , WebPage_TemplateTest
    , WebPage_TestAsyncMessage
    , WebPage_TestAsyncMessageReply
    , WebPage_TestAsyncMessageWithConnection
    , WebPage_TestAsyncMessageWithConnectionReply
    , WebPage_TestAsyncMessageWithMultipleArguments
    , WebPage_TestAsyncMessageWithMultipleArgumentsReply
    , WebPage_TestAsyncMessageWithNoArguments
    , WebPage_TestAsyncMessageWithNoArgumentsReply
    , WebPage_TestMultipleAttributes
    , WebPage_TestParameterAttributes
    , WebPage_TestSyncMessage
    , WebPage_TestSynchronousMessage
    , WebPage_TouchEvent
    , WrappedAsyncMessageForTesting
    , SyncMessageReply
    , InitializeConnection
    , LegacySessionState
    , Last = LegacySessionState
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
    static_assert(std::is_unsigned<T>::value, "isValidEnum<IPC::MessageName> should only be called with unsigned types");
    if (messageName > static_cast<std::underlying_type<IPC::MessageName>::type>(IPC::MessageName::Last))
        return false;
    return IPC::isValidMessageName(static_cast<E>(messageName));
};

} // namespace WTF
