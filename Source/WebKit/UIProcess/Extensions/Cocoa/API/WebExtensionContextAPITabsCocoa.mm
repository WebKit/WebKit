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
#import "WKWebViewPrivate.h"
#import "WebExtensionContextProxy.h"
#import "WebExtensionContextProxyMessages.h"
#import "WebExtensionScriptInjectionParameters.h"
#import "WebExtensionTabIdentifier.h"
#import "WebExtensionUtilities.h"
#import "WebExtensionWindowIdentifier.h"
#import "WebPageProxy.h"
#import "_WKWebExtensionControllerDelegatePrivate.h"
#import "_WKWebExtensionTabCreationOptionsInternal.h"
#import <WebCore/ImageBufferUtilitiesCG.h>

namespace WebKit {

using namespace WebExtensionDynamicScripts;

void WebExtensionContext::tabsCreate(WebPageProxyIdentifier webPageProxyIdentifier, const WebExtensionTabParameters& parameters, CompletionHandler<void(std::optional<WebExtensionTabParameters>, WebExtensionTab::Error)>&& completionHandler)
{
    ASSERT(!parameters.audible);
    ASSERT(!parameters.loading);
    ASSERT(!parameters.privateBrowsing);
    ASSERT(!parameters.readerModeAvailable);
    ASSERT(!parameters.size);
    ASSERT(!parameters.title);

    static NSString * const apiName = @"tabs.create()";

    auto delegate = extensionController()->delegate();
    if (![delegate respondsToSelector:@selector(webExtensionController:openNewTabWithOptions:forExtensionContext:completionHandler:)]) {
        completionHandler(std::nullopt, toErrorString(apiName, nil, @"it is not implemented"));
        return;
    }

    auto *creationOptions = [[_WKWebExtensionTabCreationOptions alloc] _init];
    creationOptions.shouldActivate = parameters.active.value_or(true);
    creationOptions.shouldSelect = creationOptions.shouldActivate ?: parameters.selected.value_or(false);
    creationOptions.shouldPin = parameters.pinned.value_or(false);
    creationOptions.shouldMute = parameters.muted.value_or(false);
    creationOptions.shouldShowReaderMode = parameters.showingReaderMode.value_or(false);

    RefPtr window = getWindow(parameters.windowIdentifier.value_or(WebExtensionWindowConstants::CurrentIdentifier), webPageProxyIdentifier);
    if (!window) {
        completionHandler(std::nullopt, toErrorString(apiName, nil, @"window not found"));
        return;
    }

    creationOptions.desiredWindow = window->delegate();
    creationOptions.desiredIndex = parameters.index.value_or(window->tabs().size());

    if (parameters.parentTabIdentifier) {
        RefPtr tab = getTab(parameters.parentTabIdentifier.value());
        if (!tab) {
            completionHandler(std::nullopt, toErrorString(apiName, nil, @"parent tab not found"));
            return;
        }

        creationOptions.desiredParentTab = tab->delegate();
    }

    if (parameters.url)
        creationOptions.desiredURL = parameters.url.value();

    [delegate webExtensionController:extensionController()->wrapper() openNewTabWithOptions:creationOptions forExtensionContext:wrapper() completionHandler:makeBlockPtr([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](id<_WKWebExtensionTab> newTab, NSError *error) mutable {
        if (error) {
            RELEASE_LOG_ERROR(Extensions, "Error for open new tab: %{private}@", error);
            completionHandler(std::nullopt, error.localizedDescription);
            return;
        }

        if (!newTab) {
            completionHandler(std::nullopt, std::nullopt);
            return;
        }

        THROW_UNLESS([newTab conformsToProtocol:@protocol(_WKWebExtensionTab)], @"Object returned by webExtensionController:openNewTabWithOptions:forExtensionContext:completionHandler: does not conform to the _WKWebExtensionTab protocol");

        completionHandler(getOrCreateTab(newTab)->parameters(), std::nullopt);
    }).get()];
}

void WebExtensionContext::tabsUpdate(WebExtensionTabIdentifier tabIdentifier, const WebExtensionTabParameters& parameters, CompletionHandler<void(std::optional<WebExtensionTabParameters>, WebExtensionTab::Error)>&& completionHandler)
{
    ASSERT(!parameters.audible);
    ASSERT(!parameters.index);
    ASSERT(!parameters.loading);
    ASSERT(!parameters.privateBrowsing);
    ASSERT(!parameters.readerModeAvailable);
    ASSERT(!parameters.showingReaderMode);
    ASSERT(!parameters.size);
    ASSERT(!parameters.title);
    ASSERT(!parameters.windowIdentifier);

    RefPtr tab = getTab(tabIdentifier);
    if (!tab) {
        completionHandler(std::nullopt, toErrorString(@"tabs.update()", nil, @"tab not found"));
        return;
    }

    auto updateActiveAndSelected = [&](CompletionHandler<void(WebExtensionTab::Error)>&& stepCompletionHandler) {
        if (parameters.active.value_or(false) && !tab->isActive()) {
            tab->activate(WTFMove(stepCompletionHandler));
            return;
        }

        if (!parameters.selected) {
            stepCompletionHandler(std::nullopt);
            return;
        }

        bool shouldSelect = parameters.selected.value();
        if (shouldSelect && !tab->isSelected()) {
            // If active is not explicitly set to false, activate the tab. This matches Firefox.
            if (parameters.active.value_or(true))
                tab->activate(WTFMove(stepCompletionHandler));
            else
                tab->select(WTFMove(stepCompletionHandler));
            return;
        }

        if (!shouldSelect && tab->isSelected()) {
            tab->deselect(WTFMove(stepCompletionHandler));
            return;
        }

        stepCompletionHandler(std::nullopt);
    };

    auto updateURL = [&](CompletionHandler<void(WebExtensionTab::Error)>&& stepCompletionHandler) {
        if (!parameters.url) {
            stepCompletionHandler(std::nullopt);
            return;
        }

        tab->loadURL(parameters.url.value(), WTFMove(stepCompletionHandler));
    };

    auto updatePinned = [&](CompletionHandler<void(WebExtensionTab::Error)>&& stepCompletionHandler) {
        if (!parameters.pinned || parameters.pinned.value() == tab->isPinned()) {
            stepCompletionHandler(std::nullopt);
            return;
        }

        if (parameters.pinned.value())
            tab->pin(WTFMove(stepCompletionHandler));
        else
            tab->unpin(WTFMove(stepCompletionHandler));
    };

    auto updateMuted = [&](CompletionHandler<void(WebExtensionTab::Error)>&& stepCompletionHandler) {
        if (!parameters.muted || parameters.muted.value() == tab->isMuted()) {
            stepCompletionHandler(std::nullopt);
            return;
        }

        if (parameters.muted.value())
            tab->mute(WTFMove(stepCompletionHandler));
        else
            tab->unmute(WTFMove(stepCompletionHandler));
    };

    auto updateParentTab = [&](CompletionHandler<void(WebExtensionTab::Error)>&& stepCompletionHandler) {
        auto currentParentTab = tab->parentTab();
        auto newParentTab = parameters.parentTabIdentifier ? getTab(parameters.parentTabIdentifier.value()) : nullptr;

        if (currentParentTab == newParentTab) {
            stepCompletionHandler(std::nullopt);
            return;
        }

        tab->setParentTab(newParentTab, WTFMove(stepCompletionHandler));
    };

    updateActiveAndSelected([&](WebExtensionTab::Error activeOrSelectedError) {
        if (activeOrSelectedError) {
            completionHandler(std::nullopt, activeOrSelectedError);
            return;
        }

        updateURL([&](WebExtensionTab::Error urlError) {
            if (urlError) {
                completionHandler(std::nullopt, urlError);
                return;
            }

            updatePinned([&](WebExtensionTab::Error pinnedError) {
                if (pinnedError) {
                    completionHandler(std::nullopt, pinnedError);
                    return;
                }

                updateMuted([&](WebExtensionTab::Error mutedError) {
                    if (mutedError) {
                        completionHandler(std::nullopt, mutedError);
                        return;
                    }

                    updateParentTab([&](WebExtensionTab::Error parentError) {
                        if (parentError) {
                            completionHandler(std::nullopt, parentError);
                            return;
                        }

                        completionHandler(tab->parameters(), std::nullopt);
                    });
                });
            });
        });
    });
}

void WebExtensionContext::tabsDuplicate(WebExtensionTabIdentifier tabIdentifier, const WebExtensionTabParameters& parameters, CompletionHandler<void(std::optional<WebExtensionTabParameters>, WebExtensionTab::Error)>&& completionHandler)
{
    RefPtr tab = getTab(tabIdentifier);
    if (!tab) {
        completionHandler(std::nullopt, toErrorString(@"tabs.duplicate()", nil, @"tab not found"));
        return;
    }

    tab->duplicate(parameters, [completionHandler = WTFMove(completionHandler)](RefPtr<WebExtensionTab> newTab, WebExtensionTab::Error error) mutable {
        if (error) {
            completionHandler(std::nullopt, error);
            return;
        }

        completionHandler(newTab->parameters(), std::nullopt);
    });
}

void WebExtensionContext::tabsGet(WebExtensionTabIdentifier tabIdentifier, CompletionHandler<void(std::optional<WebExtensionTabParameters>, WebExtensionTab::Error)>&& completionHandler)
{
    RefPtr tab = getTab(tabIdentifier);
    if (!tab) {
        completionHandler(std::nullopt, toErrorString(@"tabs.get()", nil, @"tab not found"));
        return;
    }

    completionHandler(tab->parameters(), std::nullopt);
}

void WebExtensionContext::tabsGetCurrent(WebPageProxyIdentifier webPageProxyIdentifier, CompletionHandler<void(std::optional<WebExtensionTabParameters>, WebExtensionTab::Error)>&& completionHandler)
{
    RefPtr tab = getCurrentTab(webPageProxyIdentifier);
    if (!tab) {
        // No error is reported when there is no tab found.
        completionHandler(std::nullopt, std::nullopt);
        return;
    }

    completionHandler(tab->parameters(), std::nullopt);
}

void WebExtensionContext::tabsQuery(WebPageProxyIdentifier webPageProxyIdentifier, const WebExtensionTabQueryParameters& queryParameters, CompletionHandler<void(Vector<WebExtensionTabParameters>, WebExtensionTab::Error)>&& completionHandler)
{
    Vector<WebExtensionTabParameters> result;

    for (auto& window : openWindows()) {
        if (!window->matches(queryParameters, webPageProxyIdentifier))
            continue;

        for (auto& tab : window->tabs()) {
            if (tab->matches(queryParameters, WebExtensionTab::AssumeWindowMatches::Yes, webPageProxyIdentifier))
                result.append(tab->parameters());
        }
    }

    completionHandler(WTFMove(result), std::nullopt);
}

void WebExtensionContext::tabsReload(WebPageProxyIdentifier webPageProxyIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, ReloadFromOrigin reloadFromOrigin, CompletionHandler<void(WebExtensionTab::Error)>&& completionHandler)
{
    RefPtr tab = getTab(webPageProxyIdentifier, tabIdentifier);
    if (!tab) {
        completionHandler(toErrorString(@"tabs.reload()", nil, @"tab not found"));
        return;
    }

    if (reloadFromOrigin == ReloadFromOrigin::Yes)
        tab->reloadFromOrigin(WTFMove(completionHandler));
    else
        tab->reload(WTFMove(completionHandler));
}

void WebExtensionContext::tabsGoBack(WebPageProxyIdentifier webPageProxyIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, CompletionHandler<void(WebExtensionTab::Error)>&& completionHandler)
{
    RefPtr tab = getTab(webPageProxyIdentifier, tabIdentifier);
    if (!tab) {
        completionHandler(toErrorString(@"tabs.goBack()", nil, @"tab not found"));
        return;
    }

    tab->goBack(WTFMove(completionHandler));
}

void WebExtensionContext::tabsGoForward(WebPageProxyIdentifier webPageProxyIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, CompletionHandler<void(WebExtensionTab::Error)>&& completionHandler)
{
    RefPtr tab = getTab(webPageProxyIdentifier, tabIdentifier);
    if (!tab) {
        completionHandler(toErrorString(@"tabs.goForward()", nil, @"tab not found"));
        return;
    }

    tab->goForward(WTFMove(completionHandler));
}

void WebExtensionContext::tabsDetectLanguage(WebPageProxyIdentifier webPageProxyIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, CompletionHandler<void(std::optional<String>, WebExtensionTab::Error)>&& completionHandler)
{
    RefPtr tab = getTab(webPageProxyIdentifier, tabIdentifier);
    if (!tab) {
        completionHandler(std::nullopt, toErrorString(@"tabs.detectLanguage()", nil, @"tab not found"));
        return;
    }

    tab->detectWebpageLocale([completionHandler = WTFMove(completionHandler)](NSLocale *locale, WebExtensionTab::Error error) mutable {
        if (error) {
            completionHandler(std::nullopt, error);
            return;
        }

        if (!locale) {
            completionHandler(std::nullopt, std::nullopt);
            return;
        }

        completionHandler(toWebAPI(locale), std::nullopt);
    });
}

static inline String toMIMEType(WebExtensionTab::ImageFormat format)
{
    switch (format) {
    case WebExtensionTab::ImageFormat::PNG:
        return "image/png"_s;
    case WebExtensionTab::ImageFormat::JPEG:
        return "image/jpeg"_s;
    }
}

void WebExtensionContext::tabsCaptureVisibleTab(WebPageProxyIdentifier webPageProxyIdentifier, std::optional<WebExtensionWindowIdentifier> windowIdentifier, WebExtensionTab::ImageFormat imageFormat, uint8_t imageQuality, CompletionHandler<void(std::optional<URL>, WebExtensionTab::Error)>&& completionHandler)
{
    RefPtr window = getWindow(windowIdentifier.value_or(WebExtensionWindowConstants::CurrentIdentifier), webPageProxyIdentifier);
    if (!window) {
        completionHandler({ }, toErrorString(@"tabs.captureVisibleTab()", nil, @"window not found"));
        return;
    }

    RefPtr activeTab = window->activeTab();
    if (!activeTab) {
        completionHandler({ }, toErrorString(@"tabs.captureVisibleTab()", nil, @"active tab not found"));
        return;
    }

    if (!activeTab->extensionHasPermission()) {
        completionHandler({ }, toErrorString(@"tabs.captureVisibleTab()", nil, @"either the 'activeTab' permission or granted host permissions for the current website are required"));
        return;
    }

    activeTab->captureVisibleWebpage([completionHandler = WTFMove(completionHandler), imageFormat, imageQuality](CocoaImage *image, WebExtensionTab::Error error) mutable {
        if (error) {
            completionHandler({ }, error);
            return;
        }

        if (!image) {
            completionHandler(std::nullopt, std::nullopt);
            return;
        }

#if USE(APPKIT)
        auto cgImage = [image CGImageForProposedRect:nil context:nil hints:nil];
#else
        auto cgImage = image.CGImage;
#endif
        if (!cgImage) {
            completionHandler(std::nullopt, std::nullopt);
            return;
        }

        completionHandler(URL { WebCore::dataURL(cgImage, toMIMEType(imageFormat), imageQuality) }, std::nullopt);
    });
}

void WebExtensionContext::tabsToggleReaderMode(WebPageProxyIdentifier webPageProxyIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, CompletionHandler<void(WebExtensionTab::Error)>&& completionHandler)
{
    RefPtr tab = getTab(webPageProxyIdentifier, tabIdentifier);
    if (!tab) {
        completionHandler(toErrorString(@"tabs.toggleReaderMode()", nil, @"tab not found"));
        return;
    }

    tab->toggleReaderMode(WTFMove(completionHandler));
}

void WebExtensionContext::tabsSendMessage(WebExtensionTabIdentifier tabIdentifier, const String& messageJSON, std::optional<WebExtensionFrameIdentifier> frameIdentifier, const WebExtensionMessageSenderParameters& senderParameters, CompletionHandler<void(std::optional<String> replyJSON, WebExtensionTab::Error)>&& completionHandler)
{
    RefPtr tab = getTab(tabIdentifier);
    if (!tab) {
        completionHandler(nullString(), toErrorString(@"tabs.sendMessage()", nil, @"tab not found"));
        return;
    }

    auto contentScriptProcesses = tab->processes(WebExtensionEventListenerType::RuntimeOnMessage, WebExtensionContentWorldType::ContentScript);
    if (contentScriptProcesses.isEmpty()) {
        completionHandler(std::nullopt, std::nullopt);
        return;
    }

    ASSERT(contentScriptProcesses.size() == 1);
    auto process = contentScriptProcesses.takeAny();

    process->sendWithAsyncReply(Messages::WebExtensionContextProxy::DispatchRuntimeMessageEvent(WebExtensionContentWorldType::ContentScript, messageJSON, frameIdentifier, senderParameters), [protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](std::optional<String> replyJSON) mutable {
        completionHandler(replyJSON, std::nullopt);
    }, identifier());
}

void WebExtensionContext::tabsConnect(WebExtensionTabIdentifier tabIdentifier, WebExtensionPortChannelIdentifier channelIdentifier, String name, std::optional<WebExtensionFrameIdentifier> frameIdentifier, const WebExtensionMessageSenderParameters& senderParameters, CompletionHandler<void(WebExtensionTab::Error)>&& completionHandler)
{
    // Add 1 for the starting port here so disconnect will balance with a decrement.
    constexpr auto sourceContentWorldType = WebExtensionContentWorldType::Main;
    addPorts(sourceContentWorldType, channelIdentifier, 1);

    RefPtr tab = getTab(tabIdentifier);
    if (!tab) {
        completionHandler(toErrorString(@"tabs.connect()", nil, @"tab not found"));
        return;
    }

    constexpr auto targetContentWorldType = WebExtensionContentWorldType::ContentScript;

    auto contentScriptProcesses = tab->processes(WebExtensionEventListenerType::RuntimeOnConnect, targetContentWorldType);
    if (contentScriptProcesses.isEmpty()) {
        completionHandler(toErrorString(@"tabs.connect()", nil, @"no runtime.onConnect listeners found"));
        return;
    }

    ASSERT(contentScriptProcesses.size() == 1);
    auto process = contentScriptProcesses.takeAny();

    process->sendWithAsyncReply(Messages::WebExtensionContextProxy::DispatchRuntimeConnectEvent(targetContentWorldType, channelIdentifier, name, frameIdentifier, senderParameters), [=, protectedThis = Ref { *this }](size_t firedEventCount) mutable {
        protectedThis->addPorts(targetContentWorldType, channelIdentifier, firedEventCount);
        protectedThis->fireQueuedPortMessageEventsIfNeeded(*process, targetContentWorldType, channelIdentifier);
        protectedThis->firePortDisconnectEventIfNeeded(sourceContentWorldType, targetContentWorldType, channelIdentifier);
        protectedThis->clearQueuedPortMessages(targetContentWorldType, channelIdentifier);
    }, identifier());

    completionHandler(std::nullopt);
}

void WebExtensionContext::tabsGetZoom(WebPageProxyIdentifier webPageProxyIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, CompletionHandler<void(std::optional<double>, WebExtensionTab::Error)>&& completionHandler)
{
    RefPtr tab = getTab(webPageProxyIdentifier, tabIdentifier);
    if (!tab) {
        completionHandler(std::nullopt, toErrorString(@"tabs.getZoom()", nil, @"tab not found"));
        return;
    }

    completionHandler(tab->zoomFactor(), std::nullopt);
}

void WebExtensionContext::tabsSetZoom(WebPageProxyIdentifier webPageProxyIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, double zoomFactor, CompletionHandler<void(WebExtensionTab::Error)>&& completionHandler)
{
    RefPtr tab = getTab(webPageProxyIdentifier, tabIdentifier);
    if (!tab) {
        completionHandler(toErrorString(@"tabs.setZoom()", nil, @"tab not found"));
        return;
    }

    tab->setZoomFactor(zoomFactor, WTFMove(completionHandler));
}

void WebExtensionContext::tabsRemove(Vector<WebExtensionTabIdentifier> tabIdentifiers, CompletionHandler<void(WebExtensionTab::Error)>&& completionHandler)
{
    auto tabs = tabIdentifiers.map([&](auto& tabIdentifier) -> RefPtr<WebExtensionTab> {
        RefPtr tab = getTab(tabIdentifier);
        if (!tab) {
            completionHandler(toErrorString(@"tabs.remove()", nil, @"tab '%llu' was not found", tabIdentifier.toUInt64()));
            return nullptr;
        }

        return tab;
    });

    if (tabs.contains(nullptr)) {
        // The completionHandler was called with an error in map() when returning nullptr.
        return;
    }

    size_t completed = 0;
    bool errorOccured = false;
    auto internalCompletionHandler = WTFMove(completionHandler);

    for (auto& tab : tabs) {
        if (errorOccured)
            break;

        tab->close([&](WebExtensionTab::Error error) mutable {
            if (errorOccured)
                return;

            if (error) {
                errorOccured = true;
                internalCompletionHandler(error);
                return;
            }

            if (++completed < tabs.size())
                return;

            internalCompletionHandler(std::nullopt);
        });
    }
}

void WebExtensionContext::tabsExecuteScript(WebPageProxyIdentifier webPageProxyIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, const WebExtensionScriptInjectionParameters& parameters, CompletionHandler<void(std::optional<InjectionResults>, WebExtensionTab::Error)>&& completionHandler)
{
    RefPtr tab = getTab(webPageProxyIdentifier, tabIdentifier);
    if (!tab) {
        completionHandler(std::nullopt, toErrorString(@"tabs.executeScript()", nil, @"tab not found"));
        return;
    }

    auto *webView = tab->mainWebView();
    if (!webView) {
        completionHandler(std::nullopt, toErrorString(@"tabs.executeScript()", nil, @"could not execute script in tab"));
        return;
    }

    if (!hasPermission(webView.URL, tab.get())) {
        completionHandler(std::nullopt, toErrorString(@"tabs.executeScript()", nil, @"this extension does not have access to this tab"));
        return;
    }

    std::optional<SourcePair> scriptData;
    if (parameters.code)
        scriptData = SourcePair { parameters.code.value(), std::nullopt };
    else {
        NSString *filePath = parameters.files.value().first();
        scriptData = sourcePairForResource(filePath, m_extension);
        if (!scriptData) {
            completionHandler(std::nullopt, toErrorString(@"tabs.executeScript()", nil, @"Invalid resource: %@", filePath));
            return;
        }
    }

    auto scriptPairs = getSourcePairsForParameters(parameters, m_extension);
    executeScript(scriptPairs, webView, *m_contentScriptWorld, tab.get(), parameters, *this, [completionHandler = WTFMove(completionHandler)](InjectionResultHolder& injectionResults) mutable {
        completionHandler(injectionResults.results, std::nullopt);
    });
}

void WebExtensionContext::tabsInsertCSS(WebPageProxyIdentifier webPageProxyIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, const WebExtensionScriptInjectionParameters& parameters, CompletionHandler<void(WebExtensionTab::Error)>&& completionHandler)
{
    RefPtr tab = getTab(webPageProxyIdentifier, tabIdentifier);
    if (!tab) {
        completionHandler(toErrorString(@"tabs.insertCSS()", nil, @"tab not found"));
        return;
    }

    auto *webView = tab->mainWebView();
    if (!webView) {
        completionHandler(toErrorString(@"tabs.insertCSS()", nil, @"could not inject stylesheet on this tab"));
        return;
    }

    if (!hasPermission(webView.URL, tab.get())) {
        completionHandler(toErrorString(@"tabs.insertCSS()", nil, @"this extension does not have access to this tab"));
        return;
    }

    // FIXME: <https://webkit.org/b/262491> There is currently no way to inject CSS in specific frames based on ID's. If 'frameIds' is passed, default to the main frame.
    auto injectedFrames = parameters.frameIDs ? WebCore::UserContentInjectedFrames::InjectInTopFrameOnly : WebCore::UserContentInjectedFrames::InjectInAllFrames;

    auto styleSheetPairs = getSourcePairsForParameters(parameters, m_extension);
    injectStyleSheets(styleSheetPairs, webView, *m_contentScriptWorld, injectedFrames, *this);

    completionHandler(std::nullopt);
}

void WebExtensionContext::tabsRemoveCSS(WebPageProxyIdentifier webPageProxyIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, const WebExtensionScriptInjectionParameters& parameters, CompletionHandler<void(WebExtensionTab::Error)>&& completionHandler)
{
    RefPtr tab = getTab(webPageProxyIdentifier, tabIdentifier);
    if (!tab) {
        completionHandler(toErrorString(@"tabs.removeCSS()", nil, @"tab not found"));
        return;
    }

    auto *webView = tab->mainWebView();
    if (!webView) {
        completionHandler(toErrorString(@"tabs.removeCSS()", nil, @"could not remove stylesheet on this tab"));
        return;
    }

    if (!hasPermission(webView.URL, tab.get())) {
        completionHandler(toErrorString(@"tabs.removeCSS()", nil, @"this extension does not have access to this tab"));
        return;
    }

    // FIXME: <https://webkit.org/b/262491> There is currently no way to inject CSS in specific frames based on ID's. If 'frameIds' is passed, default to the main frame.
    auto injectedFrames = parameters.frameIDs ? WebCore::UserContentInjectedFrames::InjectInTopFrameOnly : WebCore::UserContentInjectedFrames::InjectInAllFrames;

    auto styleSheetPairs = getSourcePairsForParameters(parameters, m_extension);
    removeStyleSheets(styleSheetPairs, webView, injectedFrames, *this);

    completionHandler(std::nullopt);
}

void WebExtensionContext::fireTabsCreatedEventIfNeeded(const WebExtensionTabParameters& parameters)
{
    constexpr auto type = WebExtensionEventListenerType::TabsOnCreated;
    wakeUpBackgroundContentIfNecessaryToFireEvents({ type }, [&] {
        sendToProcessesForEvent(type, Messages::WebExtensionContextProxy::DispatchTabsCreatedEvent(parameters));
    });
}

void WebExtensionContext::fireTabsUpdatedEventIfNeeded(const WebExtensionTabParameters& parameters, const WebExtensionTabParameters& changedParameters)
{
    constexpr auto type = WebExtensionEventListenerType::TabsOnUpdated;
    wakeUpBackgroundContentIfNecessaryToFireEvents({ type }, [&] {
        sendToProcessesForEvent(type, Messages::WebExtensionContextProxy::DispatchTabsUpdatedEvent(parameters, changedParameters));
    });
}

void WebExtensionContext::fireTabsReplacedEventIfNeeded(WebExtensionTabIdentifier replacedTabIdentifier, WebExtensionTabIdentifier newTabIdentifier)
{
    constexpr auto type = WebExtensionEventListenerType::TabsOnReplaced;
    wakeUpBackgroundContentIfNecessaryToFireEvents({ type }, [&] {
        sendToProcessesForEvent(type, Messages::WebExtensionContextProxy::DispatchTabsReplacedEvent(replacedTabIdentifier, newTabIdentifier));
    });
}

void WebExtensionContext::fireTabsDetachedEventIfNeeded(WebExtensionTabIdentifier tabIdentifier, WebExtensionWindowIdentifier oldWindowIdentifier, size_t oldIndex)
{
    constexpr auto type = WebExtensionEventListenerType::TabsOnDetached;
    wakeUpBackgroundContentIfNecessaryToFireEvents({ type }, [&] {
        sendToProcessesForEvent(type, Messages::WebExtensionContextProxy::DispatchTabsDetachedEvent(tabIdentifier, oldWindowIdentifier, oldIndex));
    });
}

void WebExtensionContext::fireTabsMovedEventIfNeeded(WebExtensionTabIdentifier tabIdentifier, WebExtensionWindowIdentifier windowIdentifier, size_t oldIndex, size_t newIndex)
{
    constexpr auto type = WebExtensionEventListenerType::TabsOnMoved;
    wakeUpBackgroundContentIfNecessaryToFireEvents({ type }, [&] {
        sendToProcessesForEvent(type, Messages::WebExtensionContextProxy::DispatchTabsMovedEvent(tabIdentifier, windowIdentifier, oldIndex, newIndex));
    });
}

void WebExtensionContext::fireTabsAttachedEventIfNeeded(WebExtensionTabIdentifier tabIdentifier, WebExtensionWindowIdentifier newWindowIdentifier, size_t newIndex)
{
    constexpr auto type = WebExtensionEventListenerType::TabsOnAttached;
    wakeUpBackgroundContentIfNecessaryToFireEvents({ type }, [&] {
        sendToProcessesForEvent(type, Messages::WebExtensionContextProxy::DispatchTabsAttachedEvent(tabIdentifier, newWindowIdentifier, newIndex));
    });
}

void WebExtensionContext::fireTabsActivatedEventIfNeeded(WebExtensionTabIdentifier previousActiveTabIdentifier, WebExtensionTabIdentifier newActiveTabIdentifier, WebExtensionWindowIdentifier windowIdentifier)
{
    constexpr auto type = WebExtensionEventListenerType::TabsOnActivated;
    wakeUpBackgroundContentIfNecessaryToFireEvents({ type }, [&] {
        sendToProcessesForEvent(type, Messages::WebExtensionContextProxy::DispatchTabsActivatedEvent(previousActiveTabIdentifier, newActiveTabIdentifier, windowIdentifier));
    });
}

void WebExtensionContext::fireTabsHighlightedEventIfNeeded(Vector<WebExtensionTabIdentifier> tabIdentifiers, WebExtensionWindowIdentifier windowIdentifier)
{
    constexpr auto type = WebExtensionEventListenerType::TabsOnHighlighted;
    wakeUpBackgroundContentIfNecessaryToFireEvents({ type }, [&] {
        sendToProcessesForEvent(type, Messages::WebExtensionContextProxy::DispatchTabsHighlightedEvent(tabIdentifiers, windowIdentifier));
    });
}

void WebExtensionContext::fireTabsRemovedEventIfNeeded(WebExtensionTabIdentifier tabIdentifier, WebExtensionWindowIdentifier windowIdentifier, WindowIsClosing windowIsClosing)
{
    constexpr auto type = WebExtensionEventListenerType::TabsOnRemoved;
    wakeUpBackgroundContentIfNecessaryToFireEvents({ type }, [&] {
        sendToProcessesForEvent(type, Messages::WebExtensionContextProxy::DispatchTabsRemovedEvent(tabIdentifier, windowIdentifier, windowIsClosing));
    });
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
