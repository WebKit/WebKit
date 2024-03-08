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
#import "WKWebViewInternal.h"
#import "WebExtensionContextProxyMessages.h"
#import "WebExtensionMessagePort.h"
#import "WebExtensionMessageSenderParameters.h"
#import "WebExtensionUtilities.h"
#import "WebPageProxy.h"
#import "_WKWebExtensionControllerDelegatePrivate.h"
#import "_WKWebExtensionTabCreationOptionsInternal.h"
#import <wtf/BlockPtr.h>
#import <wtf/CallbackAggregator.h>

namespace WebKit {

void WebExtensionContext::runtimeGetBackgroundPage(CompletionHandler<void(Expected<std::optional<WebCore::PageIdentifier>, WebExtensionError>&&)>&& completionHandler)
{
    wakeUpBackgroundContentIfNecessary([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)]() mutable {
        completionHandler(backgroundPageIdentifier());
    });
}

void WebExtensionContext::runtimeOpenOptionsPage(CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"runtime.openOptionsPage()";

    if (!optionsPageURL().isValid()) {
        completionHandler(toWebExtensionError(apiName, nil, @"no options page is specified in the manifest"));
        return;
    }

    auto delegate = extensionController()->delegate();

    bool respondsToOpenOptionsPage = [delegate respondsToSelector:@selector(webExtensionController:openOptionsPageForExtensionContext:completionHandler:)];
    bool respondsToOpenNewTab = [delegate respondsToSelector:@selector(webExtensionController:openNewTabWithOptions:forExtensionContext:completionHandler:)];
    if (!respondsToOpenOptionsPage && !respondsToOpenNewTab) {
        completionHandler(toWebExtensionError(apiName, nil, @"it is not implemented"));
        return;
    }

    if (respondsToOpenOptionsPage) {
        [delegate webExtensionController:extensionController()->wrapper() openOptionsPageForExtensionContext:wrapper() completionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler)](NSError *error) mutable {
            if (error) {
                RELEASE_LOG_ERROR(Extensions, "Error opening options page: %{public}@", privacyPreservingDescription(error));
                completionHandler(toWebExtensionError(apiName, nil, error.localizedDescription));
                return;
            }

            completionHandler({ });
        }).get()];

        return;
    }

    ASSERT(respondsToOpenNewTab);

    auto frontmostWindow = this->frontmostWindow();

    auto *creationOptions = [[_WKWebExtensionTabCreationOptions alloc] _init];
    creationOptions.shouldActivate = YES;
    creationOptions.shouldSelect = YES;
    creationOptions.desiredWindow = frontmostWindow ? frontmostWindow->delegate() : nil;
    creationOptions.desiredIndex = frontmostWindow ? frontmostWindow->tabs().size() : 0;
    creationOptions.desiredURL = optionsPageURL();

    [delegate webExtensionController:extensionController()->wrapper() openNewTabWithOptions:creationOptions forExtensionContext:wrapper() completionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler)](id<_WKWebExtensionTab> newTab, NSError *error) mutable {
        if (error) {
            RELEASE_LOG_ERROR(Extensions, "Error opening options page in new tab: %{public}@", privacyPreservingDescription(error));
            completionHandler(toWebExtensionError(apiName, nil, error.localizedDescription));
            return;
        }

        if (!newTab) {
            completionHandler(toWebExtensionError(apiName, nil, @"the options page cound not be opened"));
            return;
        }

        THROW_UNLESS([newTab conformsToProtocol:@protocol(_WKWebExtensionTab)], @"Object returned by webExtensionController:openNewTabWithOptions:forExtensionContext:completionHandler: does not conform to the _WKWebExtensionTab protocol");

        completionHandler({ });
    }).get()];
}

void WebExtensionContext::runtimeReload()
{
    reload();
}

void WebExtensionContext::runtimeSendMessage(const String& extensionID, const String& messageJSON, const WebExtensionMessageSenderParameters& senderParameters, CompletionHandler<void(Expected<String, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"runtime.sendMessage()";

    if (!extensionID.isEmpty() && uniqueIdentifier() != extensionID) {
        // FIXME: <https://webkit.org/b/id269299> Add support for externally_connectable:ids.
        completionHandler(toWebExtensionError(apiName, @"extensionID", @"cross-extension messaging is not supported"));
        return;
    }

    WebExtensionMessageSenderParameters completeSenderParameters = senderParameters;
    if (RefPtr tab = getTab(senderParameters.pageProxyIdentifier))
        completeSenderParameters.tabParameters = tab->parameters();

    constexpr auto targetContentWorldType = WebExtensionContentWorldType::Main;
    constexpr auto eventType = WebExtensionEventListenerType::RuntimeOnMessage;

    wakeUpBackgroundContentIfNecessaryToFireEvents({ eventType }, [=, this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)]() mutable {
        auto mainWorldProcesses = processes(eventType, targetContentWorldType);
        if (mainWorldProcesses.isEmpty()) {
            completionHandler({ });
            return;
        }

        auto callbackAggregator = EagerCallbackAggregator<void(Expected<String, WebExtensionError>)>::create(WTFMove(completionHandler), { });

        for (auto& process : mainWorldProcesses) {
            process->sendWithAsyncReply(Messages::WebExtensionContextProxy::DispatchRuntimeMessageEvent(targetContentWorldType, messageJSON, std::nullopt, completeSenderParameters), [callbackAggregator](String&& replyJSON) {
                // A null reply means no listeners replied. Don't call the callbackAggregator
                // to give other listeners in a different process a chance to reply.
                if (replyJSON.isNull())
                    return;

                callbackAggregator.get()(WTFMove(replyJSON));
            }, identifier());
        }
    });
}

void WebExtensionContext::runtimeConnect(const String& extensionID, WebExtensionPortChannelIdentifier channelIdentifier, const String& name, const WebExtensionMessageSenderParameters& senderParameters, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"runtime.connect()";

    const auto sourceContentWorldType = senderParameters.contentWorldType;
    constexpr auto targetContentWorldType = WebExtensionContentWorldType::Main;

    // Add 1 for the starting port here so disconnect will balance with a decrement.
    addPorts(sourceContentWorldType, targetContentWorldType, channelIdentifier, { senderParameters.pageProxyIdentifier });

    if (!extensionID.isEmpty() && uniqueIdentifier() != extensionID) {
        // FIXME: <https://webkit.org/b/id269299> Add support for externally_connectable:ids.
        completionHandler(toWebExtensionError(apiName, @"extensionID", @"cross-extension messaging is not supported"));
        return;
    }

    WebExtensionMessageSenderParameters completeSenderParameters = senderParameters;
    if (RefPtr tab = getTab(senderParameters.pageProxyIdentifier))
        completeSenderParameters.tabParameters = tab->parameters();

    constexpr auto eventType = WebExtensionEventListenerType::RuntimeOnConnect;

    wakeUpBackgroundContentIfNecessaryToFireEvents({ eventType }, [=, this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)]() mutable {
        auto mainWorldProcesses = processes(eventType, targetContentWorldType);
        if (mainWorldProcesses.isEmpty()) {
            completionHandler(toWebExtensionError(apiName, nil, @"no runtime.onConnect listeners found"));
            return;
        }

        size_t handledCount = 0;
        size_t totalExpected = mainWorldProcesses.size();

        for (auto& process : mainWorldProcesses) {
            process->sendWithAsyncReply(Messages::WebExtensionContextProxy::DispatchRuntimeConnectEvent(targetContentWorldType, channelIdentifier, name, std::nullopt, completeSenderParameters), [=, this, protectedThis = Ref { *this }, &handledCount](HashCountedSet<WebPageProxyIdentifier>&& addedPortCounts) mutable {
                addPorts(targetContentWorldType, sourceContentWorldType, channelIdentifier, WTFMove(addedPortCounts));
                fireQueuedPortMessageEventsIfNeeded(process, targetContentWorldType, channelIdentifier);
                firePortDisconnectEventIfNeeded(sourceContentWorldType, targetContentWorldType, channelIdentifier);
                if (++handledCount >= totalExpected)
                    clearQueuedPortMessages(targetContentWorldType, channelIdentifier);
            }, identifier());
        }

        completionHandler({ });
    });
}

void WebExtensionContext::runtimeSendNativeMessage(const String& applicationID, const String& messageJSON, CompletionHandler<void(Expected<String, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"runtime.sendNativeMessage()";

    id message = parseJSON(messageJSON, JSONOptions::FragmentsAllowed);

    auto delegate = extensionController()->delegate();
    if (![delegate respondsToSelector:@selector(webExtensionController:sendMessage:toApplicationIdentifier:forExtensionContext:replyHandler:)]) {
        // FIXME: <https://webkit.org/b/262081> Implement default native messaging with NSExtension.
        completionHandler(toWebExtensionError(apiName, nil, @"native messaging is not supported"));
        return;
    }

    auto *applicationIdentifier = !applicationID.isNull() ? (NSString *)applicationID : nil;

    [delegate webExtensionController:extensionController()->wrapper() sendMessage:message toApplicationIdentifier:applicationIdentifier forExtensionContext:wrapper() replyHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler)] (id replyMessage, NSError *error) mutable {
        if (error) {
            completionHandler(toWebExtensionError(apiName, nil, error.localizedDescription));
            return;
        }

        if (replyMessage)
            THROW_UNLESS(isValidJSONObject(replyMessage, JSONOptions::FragmentsAllowed), @"reply message is not JSON-serializable");

        completionHandler(String(encodeJSONString(replyMessage, JSONOptions::FragmentsAllowed)));
    }).get()];
}

void WebExtensionContext::runtimeConnectNative(const String& applicationID, WebExtensionPortChannelIdentifier channelIdentifier, WebPageProxyIdentifier pageProxyIdentifier, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"runtime.connectNative()";

    constexpr auto sourceContentWorldType = WebExtensionContentWorldType::Main;
    constexpr auto targetContentWorldType = WebExtensionContentWorldType::Native;

    // Add 1 for the starting port here so disconnect will balance with a decrement.
    addPorts(sourceContentWorldType, targetContentWorldType, channelIdentifier, { pageProxyIdentifier });

    auto nativePort = WebExtensionMessagePort::create(*this, applicationID, channelIdentifier);

    auto delegate = extensionController()->delegate();
    if (![delegate respondsToSelector:@selector(webExtensionController:connectUsingMessagePort:forExtensionContext:completionHandler:)]) {
        // FIXME: <https://webkit.org/b/262081> Implement default native messaging with NSExtension.
        completionHandler(toWebExtensionError(apiName, nil, @"native messaging is not supported"));
        return;
    }

    [delegate webExtensionController:extensionController()->wrapper() connectUsingMessagePort:nativePort->wrapper() forExtensionContext:wrapper() completionHandler:makeBlockPtr([=, this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)] (NSError *error) mutable {
        if (error) {
            completionHandler(toWebExtensionError(apiName, nil, error.localizedDescription));

            nativePort->disconnect(toWebExtensionMessagePortError(error));
            clearQueuedPortMessages(targetContentWorldType, channelIdentifier);
            return;
        }

        addNativePort(nativePort);

        completionHandler({ });

        sendQueuedNativePortMessagesIfNeeded(channelIdentifier);
        firePortDisconnectEventIfNeeded(sourceContentWorldType, targetContentWorldType, channelIdentifier);
        clearQueuedPortMessages(targetContentWorldType, channelIdentifier);
    }).get()];
}

void WebExtensionContext::runtimeWebPageSendMessage(const String& extensionID, const String& messageJSON, const WebExtensionMessageSenderParameters& senderParameters, CompletionHandler<void(Expected<String, WebExtensionError>&&)>&& completionHandler)
{
    RefPtr destinationExtension = extensionController()->extensionContext(extensionID);
    RefPtr tab = getTab(senderParameters.pageProxyIdentifier);
    if (!destinationExtension || !tab) {
        callAfterRandomDelay([completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler({ });
        });

        return;
    }

    WebExtensionMessageSenderParameters completeSenderParameters = senderParameters;
    completeSenderParameters.tabParameters = tab->parameters();

    auto url = completeSenderParameters.url;
    auto validMatchPatterns = destinationExtension->extension().externallyConnectableMatchPatterns();
    if (!hasPermission(url, tab.get()) || !WebExtensionMatchPattern::patternsMatchURL(validMatchPatterns, url)) {
        callAfterRandomDelay([completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler({ });
        });

        return;
    }

    constexpr auto eventType = WebExtensionEventListenerType::RuntimeOnMessageExternal;

    wakeUpBackgroundContentIfNecessaryToFireEvents({ eventType }, [=, this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)]() mutable {
        auto mainWorldProcesses = processes(eventType, WebExtensionContentWorldType::Main);
        if (mainWorldProcesses.isEmpty()) {
            completionHandler({ });
            return;
        }

        auto callbackAggregator = EagerCallbackAggregator<void(Expected<String, WebExtensionError>)>::create(WTFMove(completionHandler), { });

        for (auto& process : mainWorldProcesses) {
            process->sendWithAsyncReply(Messages::WebExtensionContextProxy::DispatchRuntimeMessageEvent(WebExtensionContentWorldType::Main, messageJSON, std::nullopt, completeSenderParameters), [callbackAggregator](String&& replyJSON) {
                // A null reply means no listeners replied. Don't call the callbackAggregator
                // to give other listeners in a different process a chance to reply.
                if (replyJSON.isNull())
                    return;

                callbackAggregator.get()(WTFMove(replyJSON));
            }, identifier());
        }
    });
}

void WebExtensionContext::runtimeWebPageConnect(const String& extensionID, WebExtensionPortChannelIdentifier channelIdentifier, const String& name, const WebExtensionMessageSenderParameters& senderParameters, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"runtime.connect()";

    constexpr auto sourceContentWorldType = WebExtensionContentWorldType::WebPage;
    constexpr auto targetContentWorldType = WebExtensionContentWorldType::Main;

    RefPtr destinationExtension = extensionController()->extensionContext(extensionID);
    RefPtr tab = getTab(senderParameters.pageProxyIdentifier);
    if (!destinationExtension || !tab) {
        callAfterRandomDelay([=, this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler({ });
            firePortDisconnectEventIfNeeded(sourceContentWorldType, targetContentWorldType, channelIdentifier);
            clearQueuedPortMessages(targetContentWorldType, channelIdentifier);
        });

        return;
    }

    WebExtensionMessageSenderParameters completeSenderParameters = senderParameters;
    completeSenderParameters.tabParameters = tab->parameters();

    auto url = completeSenderParameters.url;
    auto validMatchPatterns = destinationExtension->extension().externallyConnectableMatchPatterns();
    if (!hasPermission(url, tab.get()) || !WebExtensionMatchPattern::patternsMatchURL(validMatchPatterns, url)) {
        callAfterRandomDelay([=, this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler({ });
            firePortDisconnectEventIfNeeded(sourceContentWorldType, targetContentWorldType, channelIdentifier);
            clearQueuedPortMessages(targetContentWorldType, channelIdentifier);
        });

        return;
    }

    // Add 1 for the starting port here so disconnect will balance with a decrement.
    addPorts(sourceContentWorldType, targetContentWorldType, channelIdentifier, { senderParameters.pageProxyIdentifier });

    constexpr auto eventType = WebExtensionEventListenerType::RuntimeOnConnectExternal;

    wakeUpBackgroundContentIfNecessaryToFireEvents({ eventType }, [=, this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)]() mutable {
        auto mainWorldProcesses = processes(eventType, targetContentWorldType);
        if (mainWorldProcesses.isEmpty()) {
            completionHandler(toWebExtensionError(apiName, nil, @"no runtime.onConnectExternal listeners found"));
            return;
        }

        size_t handledCount = 0;
        size_t totalExpected = mainWorldProcesses.size();

        for (auto& process : mainWorldProcesses) {
            process->sendWithAsyncReply(Messages::WebExtensionContextProxy::DispatchRuntimeConnectEvent(targetContentWorldType, channelIdentifier, name, std::nullopt, completeSenderParameters), [=, this, protectedThis = Ref { *this }, &handledCount](HashCountedSet<WebPageProxyIdentifier>&& addedPortCounts) mutable {
                addPorts(targetContentWorldType, sourceContentWorldType, channelIdentifier, WTFMove(addedPortCounts));
                fireQueuedPortMessageEventsIfNeeded(process, targetContentWorldType, channelIdentifier);
                firePortDisconnectEventIfNeeded(sourceContentWorldType, targetContentWorldType, channelIdentifier);
                if (++handledCount >= totalExpected)
                    clearQueuedPortMessages(targetContentWorldType, channelIdentifier);
            }, identifier());
        }

        completionHandler({ });
    });
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
