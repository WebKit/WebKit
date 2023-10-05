/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "config.h"
#import "WebExtensionContext.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "CocoaHelpers.h"
#import "WebExtensionContextProxyMessages.h"
#import "WebExtensionMessagePort.h"
#import "WebExtensionMessageSenderParameters.h"
#import "WebExtensionUtilities.h"
#import "_WKWebExtensionControllerDelegatePrivate.h"
#import <wtf/BlockPtr.h>

namespace WebKit {

void WebExtensionContext::runtimeSendMessage(const String& extensionID, const String& messageJSON, const WebExtensionMessageSenderParameters& senderParameters, CompletionHandler<void(std::optional<String> replyJSON, std::optional<String> error)>&& completionHandler)
{
    if (!extensionID.isEmpty() && uniqueIdentifier() != extensionID) {
        completionHandler(std::nullopt, toErrorString(@"runtime.sendMessage()", @"extensionID", @"cross-extension messaging is not supported"));
        return;
    }

    WebExtensionMessageSenderParameters completeSenderParameters = senderParameters;
    if (auto tab = getTab(senderParameters.pageProxyIdentifier))
        completeSenderParameters.tabParameters = tab->parameters();

    auto mainWorldProcesses = processes(WebExtensionEventListenerType::RuntimeOnMessage, WebExtensionContentWorldType::Main);
    if (mainWorldProcesses.isEmpty()) {
        completionHandler(std::nullopt, std::nullopt);
        return;
    }

    bool sentReply = false;

    auto handleReply = [&sentReply, completionHandler = WTFMove(completionHandler), protectedThis = Ref { *this }](std::optional<String> replyJSON) mutable {
        if (sentReply)
            return;

        sentReply = true;
        completionHandler(replyJSON, std::nullopt);
    };

    for (auto& process : mainWorldProcesses)
        process->sendWithAsyncReply(Messages::WebExtensionContextProxy::DispatchRuntimeMessageEvent(WebExtensionContentWorldType::Main, messageJSON, std::nullopt, completeSenderParameters), handleReply, identifier());
}

void WebExtensionContext::runtimeConnect(const String& extensionID, WebExtensionPortChannelIdentifier channelIdentifier, const String& name, const WebExtensionMessageSenderParameters& senderParameters, CompletionHandler<void(WebExtensionTab::Error)>&& completionHandler)
{
    // Add 1 for the starting port here so disconnect will balance with a decrement.
    const auto sourceContentWorldType = senderParameters.contentWorldType;
    addPorts(sourceContentWorldType, channelIdentifier, 1);

    if (!extensionID.isEmpty() && uniqueIdentifier() != extensionID) {
        completionHandler(toErrorString(@"runtime.connect()", @"extensionID", @"cross-extension messaging is not supported"));
        return;
    }

    WebExtensionMessageSenderParameters completeSenderParameters = senderParameters;
    if (auto tab = getTab(senderParameters.pageProxyIdentifier))
        completeSenderParameters.tabParameters = tab->parameters();

    constexpr auto targetContentWorldType = WebExtensionContentWorldType::Main;

    auto mainWorldProcesses = processes(WebExtensionEventListenerType::RuntimeOnConnect, targetContentWorldType);
    if (mainWorldProcesses.isEmpty()) {
        completionHandler(toErrorString(@"runtime.connect()", nil, @"no runtime.onConnect listeners found"));
        return;
    }

    size_t handledCount = 0;
    size_t totalExpected = mainWorldProcesses.size();

    for (auto& process : mainWorldProcesses) {
        process->sendWithAsyncReply(Messages::WebExtensionContextProxy::DispatchRuntimeConnectEvent(targetContentWorldType, channelIdentifier, name, std::nullopt, completeSenderParameters), [=, &handledCount, protectedThis = Ref { *this }](size_t firedEventCount) mutable {
            protectedThis->addPorts(targetContentWorldType, channelIdentifier, firedEventCount);
            protectedThis->fireQueuedPortMessageEventsIfNeeded(process, targetContentWorldType, channelIdentifier);
            protectedThis->firePortDisconnectEventIfNeeded(sourceContentWorldType, targetContentWorldType, channelIdentifier);
            if (++handledCount >= totalExpected)
                protectedThis->clearQueuedPortMessages(targetContentWorldType, channelIdentifier);
        }, identifier());
    }

    completionHandler(std::nullopt);
}

void WebExtensionContext::runtimeSendNativeMessage(const String& applicationID, const String& messageJSON, CompletionHandler<void(std::optional<String> replyJSON, std::optional<String> error)>&& completionHandler)
{
    id message = parseJSON(messageJSON, { JSONOptions::FragmentsAllowed });

    auto delegate = extensionController()->delegate();
    if (![delegate respondsToSelector:@selector(webExtensionController:sendMessage:toApplicationIdentifier:forExtensionContext:replyHandler:)]) {
        // FIXME: <https://webkit.org/b/262081> Implement default native messaging with NSExtension.
        completionHandler(std::nullopt, toErrorString(@"runtime.sendNativeMessage()", nil, @"native messaging is not supported"));
        return;
    }

    auto *applicationIdentifier = !applicationID.isNull() ? (NSString *)applicationID : nil;

    [delegate webExtensionController:extensionController()->wrapper() sendMessage:message toApplicationIdentifier:applicationIdentifier forExtensionContext:wrapper() replyHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler)] (id replyMessage, NSError *error) mutable {
        if (error) {
            completionHandler(std::nullopt, error.localizedDescription);
            return;
        }

        if (replyMessage)
            THROW_UNLESS(isValidJSONObject(replyMessage, { JSONOptions::FragmentsAllowed }), @"Reply message is not JSON-serializable");

        completionHandler(encodeJSONString(replyMessage, { JSONOptions::FragmentsAllowed }), std::nullopt);
    }).get()];
}

void WebExtensionContext::runtimeConnectNative(const String& applicationID, WebExtensionPortChannelIdentifier channelIdentifier, CompletionHandler<void(std::optional<String> error)>&& completionHandler)
{
    // Add 1 for the starting port here so disconnect will balance with a decrement.
    constexpr auto sourceContentWorldType = WebExtensionContentWorldType::Main;
    addPorts(sourceContentWorldType, channelIdentifier, 1);

    constexpr auto targetContentWorldType = WebExtensionContentWorldType::Native;
    auto nativePort = WebExtensionMessagePort::create(*this, applicationID, channelIdentifier);

    auto delegate = extensionController()->delegate();
    if (![delegate respondsToSelector:@selector(webExtensionController:connectUsingMessagePort:forExtensionContext:completionHandler:)]) {
        // FIXME: <https://webkit.org/b/262081> Implement default native messaging with NSExtension.
        completionHandler(toErrorString(@"runtime.connectNative()", nil, @"native messaging is not supported"));
        return;
    }

    [delegate webExtensionController:extensionController()->wrapper() connectUsingMessagePort:nativePort->wrapper() forExtensionContext:wrapper() completionHandler:makeBlockPtr([=, completionHandler = WTFMove(completionHandler), protectedThis = Ref { *this }] (NSError *error) mutable {
        if (error) {
            completionHandler(error.localizedDescription);
            nativePort->disconnect(toWebExtensionMessagePortError(error));
            protectedThis->clearQueuedPortMessages(targetContentWorldType, channelIdentifier);
            return;
        }

        protectedThis->addNativePort(nativePort);

        completionHandler(std::nullopt);

        protectedThis->sendQueuedNativePortMessagesIfNeeded(channelIdentifier);
        protectedThis->firePortDisconnectEventIfNeeded(sourceContentWorldType, targetContentWorldType, channelIdentifier);
        protectedThis->clearQueuedPortMessages(targetContentWorldType, channelIdentifier);
    }).get()];
}

void WebExtensionContext::fireRuntimeStartupEventIfNeeded()
{
    // The background content is assumed to be loaded for this event.

    RELEASE_LOG_DEBUG(Extensions, "Firing startup event");

    constexpr auto type = WebExtensionEventListenerType::RuntimeOnStartup;
    sendToProcessesForEvent(type, Messages::WebExtensionContextProxy::DispatchRuntimeStartupEvent());
}

void WebExtensionContext::fireRuntimeInstalledEventIfNeeded()
{
    ASSERT(m_installReason != InstallReason::None);

    // The background content is assumed to be loaded for this event.

    RELEASE_LOG_DEBUG(Extensions, "Firing installed event");

    constexpr auto type = WebExtensionEventListenerType::RuntimeOnInstalled;
    sendToProcessesForEvent(type, Messages::WebExtensionContextProxy::DispatchRuntimeInstalledEvent(m_installReason, m_previousVersion));
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
