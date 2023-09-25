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

#import "WebExtensionContextProxyMessages.h"
#import "WebExtensionMessageSenderParameters.h"
#import "WebExtensionUtilities.h"
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
            protectedThis->fireQueuedPortMessageEventIfNeeded(process, targetContentWorldType, channelIdentifier);
            protectedThis->firePortDisconnectEventIfNeeded(sourceContentWorldType, targetContentWorldType, channelIdentifier);
            if (++handledCount >= totalExpected)
                protectedThis->clearQueuedPortMessages(targetContentWorldType, channelIdentifier);
        }, identifier());
    }

    completionHandler(std::nullopt);
}


} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
