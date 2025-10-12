/*
 * Copyright (C) 2023-2024 Apple Inc. All rights reserved.
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
#import "FoundationSPI.h"
#import "WKWebExtensionControllerDelegatePrivate.h"
#import "WKWebExtensionMessagePortInternal.h"
#import "WKWebExtensionTabConfigurationInternal.h"
#import "WKWebViewInternal.h"
#import "WebExtensionContextProxyMessages.h"
#import "WebExtensionMessagePort.h"
#import "WebExtensionMessageSenderParameters.h"
#import "WebExtensionMessageTargetParameters.h"
#import "WebExtensionUtilities.h"
#import <wtf/BlockPtr.h>
#import <wtf/CallbackAggregator.h>
#import <wtf/darwin/DispatchExtras.h>

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
        completionHandler(toWebExtensionError(apiName, nullString(), @"no options page is specified in the manifest"));
        return;
    }

    RefPtr extensionController = this->extensionController();
    if (!extensionController) {
        completionHandler(toWebExtensionError(apiName, nullString(), @"the extension is not loaded"));
        return;
    }

    auto delegate = extensionController->delegate();

    bool respondsToOpenOptionsPage = [delegate respondsToSelector:@selector(webExtensionController:openOptionsPageForExtensionContext:completionHandler:)];
    bool respondsToOpenNewTab = [delegate respondsToSelector:@selector(webExtensionController:openNewTabUsingConfiguration:forExtensionContext:completionHandler:)];
    if (!respondsToOpenOptionsPage && !respondsToOpenNewTab) {
        completionHandler(toWebExtensionError(apiName, nullString(), @"it is not implemented"));
        return;
    }

    if (respondsToOpenOptionsPage) {
        [delegate webExtensionController:extensionController->wrapper() openOptionsPageForExtensionContext:wrapper() completionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler)](NSError *error) mutable {
            if (error) {
                RELEASE_LOG_ERROR(Extensions, "Error opening options page: %{public}@", privacyPreservingDescription(error));
                completionHandler(toWebExtensionError(apiName, nullString(), error.localizedDescription));
                return;
            }

            completionHandler({ });
        }).get()];

        return;
    }

    ASSERT(respondsToOpenNewTab);

    auto frontmostWindow = this->frontmostWindow();

    auto *configuration = [[WKWebExtensionTabConfiguration alloc] _init];
    configuration.shouldBeActive = YES;
    configuration.shouldAddToSelection = YES;
    configuration.window = frontmostWindow ? frontmostWindow->delegate() : nil;
    configuration.index = frontmostWindow ? frontmostWindow->tabs().size() : 0;
    configuration.url = optionsPageURL().createNSURL().get();

    [delegate webExtensionController:extensionController->wrapper() openNewTabUsingConfiguration:configuration forExtensionContext:wrapper() completionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler)](id<WKWebExtensionTab> newTab, NSError *error) mutable {
        if (error) {
            RELEASE_LOG_ERROR(Extensions, "Error opening options page in new tab: %{public}@", privacyPreservingDescription(error));
            completionHandler(toWebExtensionError(apiName, nullString(), error.localizedDescription));
            return;
        }

        if (!newTab) {
            completionHandler(toWebExtensionError(apiName, nullString(), @"the options page cound not be opened"));
            return;
        }

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
    else if (senderParameters.contentWorldType == WebExtensionContentWorldType::ContentScript) {
        RELEASE_LOG_ERROR(Extensions, "Tab not found for message for content script message");
        completionHandler(toWebExtensionError(apiName, nullString(), @"tab not found"));
        return;
    }

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
    else if (senderParameters.contentWorldType == WebExtensionContentWorldType::ContentScript) {
        RELEASE_LOG_ERROR(Extensions, "Tab not found for message for content script port");
        completionHandler(toWebExtensionError(apiName, nullString(), @"tab not found"));
        return;
    }

    constexpr auto eventType = WebExtensionEventListenerType::RuntimeOnConnect;

    wakeUpBackgroundContentIfNecessaryToFireEvents({ eventType }, [=, this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)]() mutable {
        auto mainWorldProcesses = processes(eventType, targetContentWorldType);
        if (mainWorldProcesses.isEmpty()) {
            completionHandler(toWebExtensionError(apiName, nullString(), @"no runtime.onConnect listeners found"));
            return;
        }

        size_t handledCount = 0;
        size_t totalExpected = mainWorldProcesses.size();

        for (auto& process : mainWorldProcesses) {
            process->sendWithAsyncReply(Messages::WebExtensionContextProxy::DispatchRuntimeConnectEvent(targetContentWorldType, channelIdentifier, name, std::nullopt, completeSenderParameters), [=, this, protectedThis = Ref { *this }, &handledCount](HashCountedSet<WebPageProxyIdentifier>&& addedPortCounts) mutable {
                // Flip target and source worlds since we're adding the opposite side of the port connection, sending from target back to source.
                addPorts(targetContentWorldType, sourceContentWorldType, channelIdentifier, WTFMove(addedPortCounts));

                fireQueuedPortMessageEventsIfNeeded(targetContentWorldType, channelIdentifier);
                fireQueuedPortMessageEventsIfNeeded(sourceContentWorldType, channelIdentifier);

                firePortDisconnectEventIfNeeded(sourceContentWorldType, targetContentWorldType, channelIdentifier);

                if (++handledCount < totalExpected)
                    return;

                clearQueuedPortMessages(targetContentWorldType, channelIdentifier);
                clearQueuedPortMessages(sourceContentWorldType, channelIdentifier);
            }, identifier());
        }

        completionHandler({ });
    });
}

void WebExtensionContext::sendNativeMessage(const String& applicationID, id message, CompletionHandler<void(Expected<RetainPtr<id>, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"runtime.sendNativeMessage()";

    RefPtr extensionController = this->extensionController();
    auto delegate = extensionController->delegate();
    if (![delegate respondsToSelector:@selector(webExtensionController:sendMessage:toApplicationWithIdentifier:forExtensionContext:replyHandler:)]) {
        // Fallback to sending the native message via NSExtension. This is only supported for
        // extensions loaded from an app extension bundle.

        // These counts are static since NSExtension has a per-app limit of 200.
        static constexpr size_t maximumActiveRequestCount = 150;
        static size_t activeRequestCount = 0;

        // This must stay in sync with SafariServices's SFExtensionMessageKey.
        static auto * const messageKey = @"message";

        auto *bundle = extension().bundle();
        if (!bundle) {
            completionHandler(toWebExtensionError(apiName, nullString(), @"native messaging is not supported"));
            return;
        }

        if (activeRequestCount >= maximumActiveRequestCount) {
            RELEASE_LOG_INFO(Extensions, "Dropping native message due to too many active native messages");
            completionHandler(toWebExtensionError(apiName, nullString(), @"there are too many active native message requests"));
            return;
        }

        // Make an NSExtension each time, since each instance has context specific blocks.
        NSError *error;
        auto *nativeExtension = [NSExtension extensionWithIdentifier:bundle.bundleIdentifier error:&error];
        if (!nativeExtension || error) {
            RELEASE_LOG_ERROR(Extensions, "Error creating NSExtension: %{public}@", privacyPreservingDescription(error));
            completionHandler(toWebExtensionError(apiName, nullString(), @"native messaging is not supported"));
            return;
        }

        Ref callbackAggregator = EagerCallbackAggregator<void(Expected<RetainPtr<id>, WebExtensionError>&&)>::create(WTFMove(completionHandler), { });

        nativeExtension.requestCancellationBlock = makeBlockPtr([callbackAggregator](id<NSCopying> requestIdentifier, NSError *error) {
            RELEASE_LOG_ERROR(Extensions, "NSExtension request with identifier %{public}@ was canceled: %{public}@", requestIdentifier, privacyPreservingDescription(error));

            dispatch_async(mainDispatchQueueSingleton(), makeBlockPtr([callbackAggregator] {
                --activeRequestCount;

                callbackAggregator.get()(toWebExtensionError(apiName, nullString(), @"the native extension canceled the request or encountered an error"));
            }).get());
        }).get();

        nativeExtension.requestInterruptionBlock = makeBlockPtr([callbackAggregator, nativeExtension = WeakObjCPtr { nativeExtension }](id<NSCopying> requestIdentifier) {
            RELEASE_LOG_ERROR(Extensions, "NSExtension request with identifier %{public}@ was interrupted", requestIdentifier);

            dispatch_async(mainDispatchQueueSingleton(), makeBlockPtr([callbackAggregator] {
                --activeRequestCount;

                callbackAggregator.get()(toWebExtensionError(apiName, nullString(), @"the native extension was interrupted or crashed"));
            }).get());

            // NSExtension does not release the interrupted request and assertions unless we cancel it. See: rdar://80093371
            [nativeExtension cancelExtensionRequestWithIdentifier:requestIdentifier];
        }).get();

        nativeExtension.requestCompletionBlock = ^(id<NSCopying> requestIdentifier, NSArray<NSExtensionItem *> *items) {
            id replyMessage = items.firstObject.userInfo[messageKey];

            dispatch_async(mainDispatchQueueSingleton(), makeBlockPtr([callbackAggregator, replyMessage = RetainPtr { replyMessage }] {
                --activeRequestCount;

                callbackAggregator.get()({ replyMessage.get() });
            }).get());
        };

        auto *messageItem = [[NSExtensionItem alloc] init];
        messageItem.userInfo = @{ messageKey: message };

        ++activeRequestCount;

        [nativeExtension beginExtensionRequestWithInputItems:@[ messageItem ] completion:makeBlockPtr([callbackAggregator, nativeExtension = RetainPtr { nativeExtension }](id<NSCopying> requestIdentifier, NSError *error) {
            if (!error)
                return;

            RELEASE_LOG_ERROR(Extensions, "NSExtension request with identifier %{public}@ failed: %{public}@", requestIdentifier, privacyPreservingDescription(error));

            dispatch_async(mainDispatchQueueSingleton(), makeBlockPtr([callbackAggregator, nativeExtension, requestIdentifier = RetainPtr { requestIdentifier }] {
                --activeRequestCount;

                callbackAggregator.get()(toWebExtensionError(apiName, nullString(), @"the native extension encountered an unknown error"));

                // NSExtension does not release the failed request and assertions unless we cancel it. See: rdar://80093371
                [nativeExtension cancelExtensionRequestWithIdentifier:requestIdentifier.get()];
            }).get());
        }).get()];

        return;
    }

    auto *applicationIdentifier = !applicationID.isNull() ? applicationID.createNSString().get() : nil;

    [delegate webExtensionController:extensionController->wrapper() sendMessage:message toApplicationWithIdentifier:applicationIdentifier forExtensionContext:wrapper() replyHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler)](id replyMessage, NSError *error) mutable {
        if (error) {
            completionHandler(toWebExtensionError(apiName, nullString(), error.localizedDescription));
            return;
        }

        if (replyMessage && !isValidJSONObject(replyMessage, JSONOptions::FragmentsAllowed)) {
            completionHandler(toWebExtensionError(apiName, nullString(), @"reply message was not JSON-serializable"));
            return;
        }

        completionHandler({ replyMessage });
    }).get()];
}

void WebExtensionContext::runtimeSendNativeMessage(const String& applicationID, const String& messageJSON, CompletionHandler<void(Expected<String, WebExtensionError>&&)>&& completionHandler)
{
    sendNativeMessage(applicationID, parseJSON(messageJSON.createNSString().get(), JSONOptions::FragmentsAllowed), [completionHandler = WTFMove(completionHandler)](auto&& result) mutable {
        if (!result) {
            completionHandler(makeUnexpected(result.error()));
            return;
        }

        completionHandler(String(encodeJSONString(result.value().get(), JSONOptions::FragmentsAllowed)));
    });
}

void WebExtensionContext::runtimeConnectNative(const String& applicationID, WebExtensionPortChannelIdentifier channelIdentifier, WebPageProxyIdentifier pageProxyIdentifier, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"runtime.connectNative()";

    constexpr auto sourceContentWorldType = WebExtensionContentWorldType::Main;
    constexpr auto targetContentWorldType = WebExtensionContentWorldType::Native;

    RefPtr extensionController = this->extensionController();
    if (!extensionController) {
        completionHandler({ });
        return;
    }

    // Add 1 for the starting port here so disconnect will balance with a decrement.
    addPorts(sourceContentWorldType, targetContentWorldType, channelIdentifier, { pageProxyIdentifier });

    Ref nativePort = WebExtensionMessagePort::create(*this, applicationID, channelIdentifier);

    auto delegate = extensionController->delegate();
    if (![delegate respondsToSelector:@selector(webExtensionController:connectUsingMessagePort:forExtensionContext:completionHandler:)]) {
        // Fallback to sending single native messages for each port message.
        // Weak reference the native port to prevent a reference cycle.

        nativePort->wrapper().messageHandler = makeBlockPtr([this, protectedThis = Ref { *this }, weakNativePort = WeakPtr { nativePort.get() }](id message, NSError *error) {
            RefPtr nativePort = weakNativePort.get();
            if (!nativePort)
                return;

            if (error) {
                nativePort->disconnect(toWebExtensionMessagePortError(error));
                return;
            }

            sendNativeMessage(nativePort->applicationIdentifier(), message, [weakNativePort](auto&& result) {
                RefPtr nativePort = weakNativePort.get();
                if (!nativePort)
                    return;

                if (!result) {
                    nativePort->disconnect();
                    return;
                }

                // Send the reply back to the port.
                nativePort->sendMessage(result.value().get());
            });
        }).get();

        addNativePort(nativePort);

        completionHandler({ });

        sendQueuedNativePortMessagesIfNeeded(channelIdentifier);
        firePortDisconnectEventIfNeeded(sourceContentWorldType, targetContentWorldType, channelIdentifier);
        clearQueuedPortMessages(targetContentWorldType, channelIdentifier);
        return;
    }

    [delegate webExtensionController:extensionController->wrapper() connectUsingMessagePort:nativePort->wrapper() forExtensionContext:wrapper() completionHandler:makeBlockPtr([=, this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)] (NSError *error) mutable {
        if (error) {
            completionHandler(toWebExtensionError(apiName, nullString(), error.localizedDescription));

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
    RefPtr extensionController = this->extensionController();
    if (!extensionController) {
        completionHandler({ });
        return;
    }

    RefPtr destinationExtension = extensionController->extensionContext(extensionID);
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
    auto validMatchPatterns = destinationExtension->protectedExtension()->externallyConnectableMatchPatterns();
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

    RefPtr extensionController = this->extensionController();
    if (!extensionController) {
        completionHandler({ });
        return;
    }

    RefPtr destinationExtension = extensionController->extensionContext(extensionID);
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
    auto validMatchPatterns = destinationExtension->protectedExtension()->externallyConnectableMatchPatterns();
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
            completionHandler(toWebExtensionError(apiName, nullString(), @"no runtime.onConnectExternal listeners found"));
            return;
        }

        size_t handledCount = 0;
        size_t totalExpected = mainWorldProcesses.size();

        for (auto& process : mainWorldProcesses) {
            process->sendWithAsyncReply(Messages::WebExtensionContextProxy::DispatchRuntimeConnectEvent(targetContentWorldType, channelIdentifier, name, std::nullopt, completeSenderParameters), [=, this, protectedThis = Ref { *this }, &handledCount](HashCountedSet<WebPageProxyIdentifier>&& addedPortCounts) mutable {
                // Flip target and source worlds since we're adding the opposite side of the port connection, sending from target back to source.
                addPorts(targetContentWorldType, sourceContentWorldType, channelIdentifier, WTFMove(addedPortCounts));

                fireQueuedPortMessageEventsIfNeeded(targetContentWorldType, channelIdentifier);
                fireQueuedPortMessageEventsIfNeeded(sourceContentWorldType, channelIdentifier);

                firePortDisconnectEventIfNeeded(sourceContentWorldType, targetContentWorldType, channelIdentifier);

                if (++handledCount < totalExpected)
                    return;

                clearQueuedPortMessages(targetContentWorldType, channelIdentifier);
                clearQueuedPortMessages(sourceContentWorldType, channelIdentifier);
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
