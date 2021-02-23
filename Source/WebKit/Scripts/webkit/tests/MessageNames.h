/*
 * Copyright (C) 2010-2021 Apple Inc. All rights reserved.
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
    TestWithIfMessage = 1
    , TestWithImageData = 2
    , TestWithLegacyReceiver = 3
    , TestWithSemaphore = 4
    , TestWithStream = 5
    , TestWithStreamBuffer = 6
    , TestWithSuperclass = 7
    , TestWithoutAttributes = 8
    , IPC = 9
    , AsyncReply = 10
    , Invalid = 11
};

enum class MessageName : uint16_t {
    TestWithIfMessage_LoadURL
    , TestWithImageData_ReceiveImageData
    , TestWithImageData_SendImageData
    , TestWithLegacyReceiver_AddEvent
    , TestWithLegacyReceiver_Close
    , TestWithLegacyReceiver_CreatePlugin
    , TestWithLegacyReceiver_DeprecatedOperation
    , TestWithLegacyReceiver_DidCreateWebProcessConnection
    , TestWithLegacyReceiver_DidReceivePolicyDecision
    , TestWithLegacyReceiver_ExperimentalOperation
    , TestWithLegacyReceiver_GetPlugins
    , TestWithLegacyReceiver_InterpretKeyEvent
    , TestWithLegacyReceiver_LoadSomething
    , TestWithLegacyReceiver_LoadSomethingElse
    , TestWithLegacyReceiver_LoadURL
    , TestWithLegacyReceiver_PreferencesDidChange
    , TestWithLegacyReceiver_RunJavaScriptAlert
    , TestWithLegacyReceiver_SendDoubleAndFloat
    , TestWithLegacyReceiver_SendInts
    , TestWithLegacyReceiver_SetVideoLayerID
    , TestWithLegacyReceiver_TemplateTest
    , TestWithLegacyReceiver_TestParameterAttributes
    , TestWithLegacyReceiver_TouchEvent
    , TestWithSemaphore_ReceiveSemaphore
    , TestWithSemaphore_SendSemaphore
    , TestWithStreamBuffer_SendStreamBuffer
    , TestWithStream_SendString
    , TestWithStream_SendStringSynchronized
    , TestWithSuperclass_LoadURL
    , TestWithSuperclass_TestAsyncMessage
    , TestWithSuperclass_TestAsyncMessageWithConnection
    , TestWithSuperclass_TestAsyncMessageWithMultipleArguments
    , TestWithSuperclass_TestAsyncMessageWithNoArguments
    , TestWithoutAttributes_AddEvent
    , TestWithoutAttributes_Close
    , TestWithoutAttributes_CreatePlugin
    , TestWithoutAttributes_DeprecatedOperation
    , TestWithoutAttributes_DidCreateWebProcessConnection
    , TestWithoutAttributes_DidReceivePolicyDecision
    , TestWithoutAttributes_ExperimentalOperation
    , TestWithoutAttributes_GetPlugins
    , TestWithoutAttributes_InterpretKeyEvent
    , TestWithoutAttributes_LoadSomething
    , TestWithoutAttributes_LoadSomethingElse
    , TestWithoutAttributes_LoadURL
    , TestWithoutAttributes_PreferencesDidChange
    , TestWithoutAttributes_RunJavaScriptAlert
    , TestWithoutAttributes_SendDoubleAndFloat
    , TestWithoutAttributes_SendInts
    , TestWithoutAttributes_SetVideoLayerID
    , TestWithoutAttributes_TemplateTest
    , TestWithoutAttributes_TestParameterAttributes
    , TestWithoutAttributes_TouchEvent
    , InitializeConnection
    , LegacySessionState
    , ProcessOutOfStreamMessage
    , SetStreamDestinationID
    , SyncMessageReply
    , TestWithSuperclass_TestAsyncMessageReply
    , TestWithSuperclass_TestAsyncMessageWithConnectionReply
    , TestWithSuperclass_TestAsyncMessageWithMultipleArgumentsReply
    , TestWithSuperclass_TestAsyncMessageWithNoArgumentsReply
    , TestWithLegacyReceiver_GetPluginProcessConnection
    , TestWithLegacyReceiver_TestMultipleAttributes
    , TestWithSuperclass_TestSyncMessage
    , TestWithSuperclass_TestSynchronousMessage
    , TestWithoutAttributes_GetPluginProcessConnection
    , TestWithoutAttributes_TestMultipleAttributes
    , WrappedAsyncMessageForTesting
    , Last = WrappedAsyncMessageForTesting
};

ReceiverName receiverName(MessageName);
const char* description(MessageName);
bool isValidMessageName(MessageName);
constexpr bool messageIsSync(MessageName name)
{
    return name >= MessageName::TestWithLegacyReceiver_GetPluginProcessConnection;
}

} // namespace IPC

namespace WTF {

template<>
struct EnumTraits<IPC::MessageName> {
    template<typename T>
    static std::enable_if_t<sizeof(T) == sizeof(IPC::MessageName) && std::is_unsigned_v<T>, bool> isValidEnum(T messageName)
    {
        if (messageName > WTF::enumToUnderlyingType(IPC::MessageName::Last))
            return false;
        return IPC::isValidMessageName(static_cast<IPC::MessageName>(messageName));
    }
};

} // namespace WTF
