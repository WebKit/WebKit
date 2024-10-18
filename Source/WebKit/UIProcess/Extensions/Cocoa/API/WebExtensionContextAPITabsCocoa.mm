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
#import "WKWebExtensionControllerDelegatePrivate.h"
#import "WKWebExtensionTabConfigurationInternal.h"
#import "WKWebViewInternal.h"
#import "WKWebViewPrivate.h"
#import "WebExtensionContextProxy.h"
#import "WebExtensionContextProxyMessages.h"
#import "WebExtensionMessageSenderParameters.h"
#import "WebExtensionMessageTargetParameters.h"
#import "WebExtensionScriptInjectionParameters.h"
#import "WebExtensionTabIdentifier.h"
#import "WebExtensionUtilities.h"
#import "WebExtensionWindowIdentifier.h"
#import "WebPageProxy.h"
#import <WebCore/ImageBufferUtilitiesCG.h>
#import <wtf/CallbackAggregator.h>

namespace WebKit {

using namespace WebExtensionDynamicScripts;

void WebExtensionContext::tabsCreate(std::optional<WebPageProxyIdentifier> webPageProxyIdentifier, const WebExtensionTabParameters& parameters, CompletionHandler<void(Expected<std::optional<WebExtensionTabParameters>, WebExtensionError>&&)>&& completionHandler)
{
    ASSERT(!parameters.audible);
    ASSERT(!parameters.loading);
    ASSERT(!parameters.privateBrowsing);
    ASSERT(!parameters.readerModeAvailable);
    ASSERT(!parameters.size);
    ASSERT(!parameters.title);

    static NSString * const apiName = @"tabs.create()";

    auto delegate = extensionController()->delegate();
    if (![delegate respondsToSelector:@selector(webExtensionController:openNewTabUsingConfiguration:forExtensionContext:completionHandler:)]) {
        completionHandler(toWebExtensionError(apiName, nil, @"it is not implemented"));
        return;
    }

    auto *configuration = [[WKWebExtensionTabConfiguration alloc] _init];
    configuration.shouldBeActive = parameters.active.value_or(true);
    configuration.shouldAddToSelection = configuration.shouldBeActive ?: parameters.selected.value_or(false);
    configuration.shouldBePinned = parameters.pinned.value_or(false);
    configuration.shouldBeMuted = parameters.muted.value_or(false);
    configuration.shouldReaderModeBeActive = parameters.showingReaderMode.value_or(false);

    RefPtr window = getWindow(parameters.windowIdentifier.value_or(WebExtensionWindowConstants::CurrentIdentifier), webPageProxyIdentifier);
    if (parameters.windowIdentifier && !window) {
        completionHandler(toWebExtensionError(apiName, nil, @"window not found"));
        return;
    }

    configuration.window = window ? window->delegate() : nil;
    configuration.index = parameters.index.value_or(window ? window->tabs().size() : 0);

    if (parameters.parentTabIdentifier) {
        RefPtr tab = getTab(parameters.parentTabIdentifier.value());
        if (!tab) {
            completionHandler(toWebExtensionError(apiName, nil, @"parent tab not found"));
            return;
        }

        configuration.parentTab = tab->delegate();
    }

    if (parameters.url)
        configuration.url = parameters.url.value();

    [delegate webExtensionController:extensionController()->wrapper() openNewTabUsingConfiguration:configuration forExtensionContext:wrapper() completionHandler:makeBlockPtr([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](id<WKWebExtensionTab> newTab, NSError *error) mutable {
        if (error) {
            RELEASE_LOG_ERROR(Extensions, "Error for open new tab: %{public}@", privacyPreservingDescription(error));
            completionHandler(toWebExtensionError(apiName, nil, error.localizedDescription));
            return;
        }

        if (!newTab) {
            completionHandler({ });
            return;
        }

        completionHandler({ getOrCreateTab(newTab)->parameters() });
    }).get()];
}

void WebExtensionContext::tabsUpdate(WebPageProxyIdentifier webPageProxyIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, const WebExtensionTabParameters& parameters, CompletionHandler<void(Expected<std::optional<WebExtensionTabParameters>, WebExtensionError>&&)>&& completionHandler)
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

    RefPtr tab = getTab(webPageProxyIdentifier, tabIdentifier, IncludeExtensionViews::Yes);
    if (!tab) {
        completionHandler(toWebExtensionError(@"tabs.update()", nil, @"tab not found"));
        return;
    }

    auto updateActiveAndSelected = [](WebExtensionTab& tab, const WebExtensionTabParameters& parameters, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& stepCompletionHandler) {
        if (parameters.active.value_or(false) && !tab.isActive()) {
            tab.activate(WTFMove(stepCompletionHandler));
            return;
        }

        if (!parameters.selected) {
            stepCompletionHandler({ });
            return;
        }

        bool shouldSelect = parameters.selected.value();
        if (shouldSelect && !tab.isSelected()) {
            // If active is not explicitly set to false, activate the tab. This matches Firefox.
            if (parameters.active.value_or(true))
                tab.activate(WTFMove(stepCompletionHandler));
            else
                tab.select(WTFMove(stepCompletionHandler));
            return;
        }

        if (!shouldSelect && tab.isSelected()) {
            tab.deselect(WTFMove(stepCompletionHandler));
            return;
        }

        stepCompletionHandler({ });
    };

    auto updateURL = [](WebExtensionTab& tab, const WebExtensionTabParameters& parameters, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& stepCompletionHandler) {
        if (!parameters.url) {
            stepCompletionHandler({ });
            return;
        }

        tab.loadURL(parameters.url.value(), WTFMove(stepCompletionHandler));
    };

    auto updatePinned = [](WebExtensionTab& tab, const WebExtensionTabParameters& parameters, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& stepCompletionHandler) {
        if (!parameters.pinned || parameters.pinned.value() == tab.isPinned()) {
            stepCompletionHandler({ });
            return;
        }

        if (parameters.pinned.value())
            tab.pin(WTFMove(stepCompletionHandler));
        else
            tab.unpin(WTFMove(stepCompletionHandler));
    };

    auto updateMuted = [](WebExtensionTab& tab, const WebExtensionTabParameters& parameters, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& stepCompletionHandler) {
        if (!parameters.muted || parameters.muted.value() == tab.isMuted()) {
            stepCompletionHandler({ });
            return;
        }

        if (parameters.muted.value())
            tab.mute(WTFMove(stepCompletionHandler));
        else
            tab.unmute(WTFMove(stepCompletionHandler));
    };

    auto updateParentTab = [this, protectedThis = Ref { *this }](WebExtensionTab& tab, const WebExtensionTabParameters& parameters, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& stepCompletionHandler) {
        auto currentParentTab = tab.parentTab();
        auto newParentTab = parameters.parentTabIdentifier ? getTab(parameters.parentTabIdentifier.value()) : nullptr;

        if (currentParentTab == newParentTab) {
            stepCompletionHandler({ });
            return;
        }

        tab.setParentTab(newParentTab, WTFMove(stepCompletionHandler));
    };

    updateActiveAndSelected(*tab, parameters, [tab = Ref { *tab }, parameters, updateURL = WTFMove(updateURL), updatePinned = WTFMove(updatePinned), updateMuted = WTFMove(updateMuted), updateParentTab = WTFMove(updateParentTab), completionHandler = WTFMove(completionHandler)](Expected<void, WebExtensionError>&& activeOrSelectedResult) mutable {
        if (!activeOrSelectedResult) {
            completionHandler(makeUnexpected(activeOrSelectedResult.error()));
            return;
        }

        updateURL(tab, parameters, [tab, parameters = WTFMove(parameters), updatePinned = WTFMove(updatePinned), updateMuted = WTFMove(updateMuted), updateParentTab = WTFMove(updateParentTab), completionHandler = WTFMove(completionHandler)](Expected<void, WebExtensionError>&& urlResult) mutable {
            if (!urlResult) {
                completionHandler(makeUnexpected(urlResult.error()));
                return;
            }

            updatePinned(tab, parameters, [tab, parameters = WTFMove(parameters), updateMuted = WTFMove(updateMuted), updateParentTab = WTFMove(updateParentTab), completionHandler = WTFMove(completionHandler)](Expected<void, WebExtensionError>&& pinnedResult) mutable {
                if (!pinnedResult) {
                    completionHandler(makeUnexpected(pinnedResult.error()));
                    return;
                }

                updateMuted(tab, parameters, [tab, parameters = WTFMove(parameters), updateParentTab = WTFMove(updateParentTab), completionHandler = WTFMove(completionHandler)](Expected<void, WebExtensionError>&& mutedResult) mutable {
                    if (!mutedResult) {
                        completionHandler(makeUnexpected(mutedResult.error()));
                        return;
                    }

                    updateParentTab(tab, parameters, [tab, completionHandler = WTFMove(completionHandler)](Expected<void, WebExtensionError>&& parentResult) mutable {
                        if (!parentResult) {
                            completionHandler(makeUnexpected(parentResult.error()));
                            return;
                        }

                        completionHandler({ tab->parameters() });
                    });
                });
            });
        });
    });
}

void WebExtensionContext::tabsDuplicate(WebExtensionTabIdentifier tabIdentifier, const WebExtensionTabParameters& parameters, CompletionHandler<void(Expected<std::optional<WebExtensionTabParameters>, WebExtensionError>&&)>&& completionHandler)
{
    RefPtr tab = getTab(tabIdentifier);
    if (!tab) {
        completionHandler(toWebExtensionError(@"tabs.duplicate()", nil, @"tab not found"));
        return;
    }

    tab->duplicate(parameters, [completionHandler = WTFMove(completionHandler)](Expected<RefPtr<WebExtensionTab>, WebExtensionError>&& result) mutable {
        if (!result) {
            completionHandler(makeUnexpected(result.error()));
            return;
        }

        RefPtr newTab = result.value();
        if (!newTab) {
            completionHandler({ });
            return;
        }

        completionHandler({ newTab->parameters() });
    });
}

void WebExtensionContext::tabsGet(WebExtensionTabIdentifier tabIdentifier, CompletionHandler<void(Expected<std::optional<WebExtensionTabParameters>, WebExtensionError>&&)>&& completionHandler)
{
    RefPtr tab = getTab(tabIdentifier);
    if (!tab) {
        completionHandler(toWebExtensionError(@"tabs.get()", nil, @"tab not found"));
        return;
    }

    requestPermissionToAccessURLs({ tab->url() }, tab, [tab, completionHandler = WTFMove(completionHandler)](auto&& requestedURLs, auto&& allowedURLs, auto expirationDate) mutable {
        completionHandler({ tab->parameters() });
    });
}

void WebExtensionContext::tabsGetCurrent(WebPageProxyIdentifier webPageProxyIdentifier, CompletionHandler<void(Expected<std::optional<WebExtensionTabParameters>, WebExtensionError>&&)>&& completionHandler)
{
    RefPtr tab = getCurrentTab(webPageProxyIdentifier);
    if (!tab) {
        // No error is reported when there is no tab found.
        completionHandler({ std::nullopt });
        return;
    }

    requestPermissionToAccessURLs({ tab->url() }, tab, [tab, completionHandler = WTFMove(completionHandler)](auto&& requestedURLs, auto&& allowedURLs, auto expirationDate) mutable {
        completionHandler({ tab->parameters() });
    });
}

void WebExtensionContext::tabsQuery(WebPageProxyIdentifier webPageProxyIdentifier, const WebExtensionTabQueryParameters& queryParameters, CompletionHandler<void(Expected<Vector<WebExtensionTabParameters>, WebExtensionError>&&)>&& completionHandler)
{
    TabVector matchedTabs;
    URLVector tabURLs;

    for (Ref window : openWindows()) {
        if (!window->matches(queryParameters, webPageProxyIdentifier))
            continue;

        for (Ref tab : window->tabs()) {
            if (tab->matches(queryParameters, WebExtensionTab::AssumeWindowMatches::Yes, webPageProxyIdentifier)) {
                matchedTabs.append(tab);
                tabURLs.append(tab->url());
            }
        }
    }

    requestPermissionToAccessURLs(tabURLs, nullptr, [matchedTabs = WTFMove(matchedTabs), completionHandler = WTFMove(completionHandler)](auto&& requestedURLs, auto&& allowedURLs, auto expirationDate) mutable {
        // Get the parameters after permission has been granted, so it can include the URL and title if allowed.
        auto result = WTF::map(matchedTabs, [&](auto& tab) {
            return tab->parameters();
        });

        completionHandler(WTFMove(result));
    });
}

void WebExtensionContext::tabsReload(WebPageProxyIdentifier webPageProxyIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, ReloadFromOrigin reloadFromOrigin, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    RefPtr tab = getTab(webPageProxyIdentifier, tabIdentifier, IncludeExtensionViews::Yes);
    if (!tab) {
        completionHandler(toWebExtensionError(@"tabs.reload()", nil, @"tab not found"));
        return;
    }

    tab->reload(reloadFromOrigin, WTFMove(completionHandler));
}

void WebExtensionContext::tabsGoBack(WebPageProxyIdentifier webPageProxyIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    RefPtr tab = getTab(webPageProxyIdentifier, tabIdentifier, IncludeExtensionViews::Yes);
    if (!tab) {
        completionHandler(toWebExtensionError(@"tabs.goBack()", nil, @"tab not found"));
        return;
    }

    tab->goBack(WTFMove(completionHandler));
}

void WebExtensionContext::tabsGoForward(WebPageProxyIdentifier webPageProxyIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    RefPtr tab = getTab(webPageProxyIdentifier, tabIdentifier, IncludeExtensionViews::Yes);
    if (!tab) {
        completionHandler(toWebExtensionError(@"tabs.goForward()", nil, @"tab not found"));
        return;
    }

    tab->goForward(WTFMove(completionHandler));
}

void WebExtensionContext::tabsDetectLanguage(WebPageProxyIdentifier webPageProxyIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, CompletionHandler<void(Expected<String, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"tabs.detectLanguage()";

    RefPtr tab = getTab(webPageProxyIdentifier, tabIdentifier, IncludeExtensionViews::Yes);
    if (!tab) {
        completionHandler(toWebExtensionError(apiName, nil, @"tab not found"));
        return;
    }

    requestPermissionToAccessURLs({ tab->url() }, tab, [tab, completionHandler = WTFMove(completionHandler)](auto&& requestedURLs, auto&& allowedURLs, auto expirationDate) mutable {
        if (!tab->extensionHasPermission()) {
            completionHandler(toWebExtensionError(apiName, nil, @"this extension does not have access to this tab"));
            return;
        }

        tab->detectWebpageLocale([completionHandler = WTFMove(completionHandler)](Expected<NSLocale *, WebExtensionError>&& result) mutable {
            if (!result) {
                completionHandler(makeUnexpected(result.error()));
                return;
            }

            completionHandler(String(toWebAPI(result.value())));
        });
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

void WebExtensionContext::tabsCaptureVisibleTab(WebPageProxyIdentifier webPageProxyIdentifier, std::optional<WebExtensionWindowIdentifier> windowIdentifier, WebExtensionTab::ImageFormat imageFormat, uint8_t imageQuality, CompletionHandler<void(Expected<URL, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"tabs.captureVisibleTab()";

    RefPtr window = getWindow(windowIdentifier.value_or(WebExtensionWindowConstants::CurrentIdentifier), webPageProxyIdentifier);
    if (!window) {
        completionHandler(toWebExtensionError(apiName, nil, @"window not found"));
        return;
    }

    RefPtr activeTab = window->activeTab();
    if (!activeTab) {
        completionHandler(toWebExtensionError(apiName, nil, @"active tab not found"));
        return;
    }

    requestPermissionToAccessURLs({ activeTab->url() }, activeTab, [activeTab, imageFormat, imageQuality, completionHandler = WTFMove(completionHandler)](auto&& requestedURLs, auto&& allowedURLs, auto expirationDate) mutable {
        if (!activeTab->extensionHasPermission()) {
            completionHandler(toWebExtensionError(apiName, nil, @"either the 'activeTab' permission or granted host permissions for the current website are required"));
            return;
        }

        activeTab->captureVisibleWebpage([completionHandler = WTFMove(completionHandler), imageFormat, imageQuality](Expected<CocoaImage *, WebExtensionError>&& result) mutable {
            if (!result) {
                completionHandler(makeUnexpected(result.error()));
                return;
            }

            auto *image = result.value();
            if (!image) {
                completionHandler({ });
                return;
            }

#if USE(APPKIT)
            auto cgImage = [image CGImageForProposedRect:nil context:nil hints:nil];
#else
            auto cgImage = image.CGImage;
#endif
            if (!cgImage) {
                completionHandler({ });
                return;
            }

            completionHandler(URL { WebCore::dataURL(cgImage, toMIMEType(imageFormat), imageQuality) });
        });
    });
}

void WebExtensionContext::tabsToggleReaderMode(WebPageProxyIdentifier webPageProxyIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    RefPtr tab = getTab(webPageProxyIdentifier, tabIdentifier, IncludeExtensionViews::Yes);
    if (!tab) {
        completionHandler(toWebExtensionError(@"tabs.toggleReaderMode()", nil, @"tab not found"));
        return;
    }

    tab->toggleReaderMode(WTFMove(completionHandler));
}

void WebExtensionContext::tabsSendMessage(WebExtensionTabIdentifier tabIdentifier, const String& messageJSON, const WebExtensionMessageTargetParameters& targetParameters, const WebExtensionMessageSenderParameters& senderParameters, CompletionHandler<void(Expected<String, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"tabs.sendMessage()";

    RefPtr tab = getTab(tabIdentifier);
    if (!tab) {
        completionHandler(toWebExtensionError(apiName, nil, @"tab not found"));
        return;
    }

    auto targetContentWorldType = isURLForAnyExtension(tab->url()) ? WebExtensionContentWorldType::Main : WebExtensionContentWorldType::ContentScript;

    auto processes = tab->processes(WebExtensionEventListenerType::RuntimeOnMessage, targetContentWorldType);
    if (processes.isEmpty()) {
        completionHandler({ });
        return;
    }

    ASSERT(processes.size() == 1);
    auto process = processes.takeAny();

    process->sendWithAsyncReply(Messages::WebExtensionContextProxy::DispatchRuntimeMessageEvent(targetContentWorldType, messageJSON, targetParameters, senderParameters), [protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](String&& replyJSON) mutable {
        completionHandler(WTFMove(replyJSON));
    }, identifier());
}

void WebExtensionContext::tabsConnect(WebExtensionTabIdentifier tabIdentifier, WebExtensionPortChannelIdentifier channelIdentifier, String name, const WebExtensionMessageTargetParameters& targetParameters, const WebExtensionMessageSenderParameters& senderParameters, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"tabs.connect()";

    RefPtr tab = getTab(tabIdentifier);
    if (!tab) {
        completionHandler(toWebExtensionError(apiName, nil, @"tab not found"));
        return;
    }

    constexpr auto sourceContentWorldType = WebExtensionContentWorldType::Main;
    auto targetContentWorldType = isURLForAnyExtension(tab->url()) ? WebExtensionContentWorldType::Main : WebExtensionContentWorldType::ContentScript;

    // Add 1 for the starting port here so disconnect will balance with a decrement.
    addPorts(sourceContentWorldType, targetContentWorldType, channelIdentifier, { senderParameters.pageProxyIdentifier });

    auto processes = tab->processes(WebExtensionEventListenerType::RuntimeOnConnect, targetContentWorldType);
    if (processes.isEmpty()) {
        completionHandler(toWebExtensionError(apiName, nil, @"no runtime.onConnect listeners found"));
        return;
    }

    ASSERT(processes.size() == 1);
    RefPtr process = processes.takeAny();

    process->sendWithAsyncReply(Messages::WebExtensionContextProxy::DispatchRuntimeConnectEvent(targetContentWorldType, channelIdentifier, name, targetParameters, senderParameters), [=, this, protectedThis = Ref { *this }](HashCountedSet<WebPageProxyIdentifier>&& addedPortCounts) mutable {
        // Flip target and source worlds since we're adding the opposite side of the port connection, sending from target back to source.
        addPorts(targetContentWorldType, sourceContentWorldType, channelIdentifier, WTFMove(addedPortCounts));

        fireQueuedPortMessageEventsIfNeeded(targetContentWorldType, channelIdentifier);
        fireQueuedPortMessageEventsIfNeeded(sourceContentWorldType, channelIdentifier);

        firePortDisconnectEventIfNeeded(sourceContentWorldType, targetContentWorldType, channelIdentifier);

        clearQueuedPortMessages(targetContentWorldType, channelIdentifier);
        clearQueuedPortMessages(sourceContentWorldType, channelIdentifier);
    }, identifier());

    completionHandler({ });
}

void WebExtensionContext::tabsGetZoom(WebPageProxyIdentifier webPageProxyIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, CompletionHandler<void(Expected<double, WebExtensionError>&&)>&& completionHandler)
{
    RefPtr tab = getTab(webPageProxyIdentifier, tabIdentifier, IncludeExtensionViews::Yes);
    if (!tab) {
        completionHandler(toWebExtensionError(@"tabs.getZoom()", nil, @"tab not found"));
        return;
    }

    completionHandler(tab->zoomFactor());
}

void WebExtensionContext::tabsSetZoom(WebPageProxyIdentifier webPageProxyIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, double zoomFactor, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    RefPtr tab = getTab(webPageProxyIdentifier, tabIdentifier, IncludeExtensionViews::Yes);
    if (!tab) {
        completionHandler(toWebExtensionError(@"tabs.setZoom()", nil, @"tab not found"));
        return;
    }

    tab->setZoomFactor(zoomFactor, WTFMove(completionHandler));
}

void WebExtensionContext::tabsRemove(Vector<WebExtensionTabIdentifier> tabIdentifiers, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    auto tabs = tabIdentifiers.map([&](auto& tabIdentifier) -> RefPtr<WebExtensionTab> {
        RefPtr tab = getTab(tabIdentifier);
        if (!tab) {
            completionHandler(toWebExtensionError(@"tabs.remove()", nil, @"tab '%llu' was not found", tabIdentifier.toUInt64()));
            return nullptr;
        }

        return tab;
    });

    if (tabs.contains(nullptr)) {
        // The completionHandler was called with an error in map() when returning nullptr.
        return;
    }

    Ref callbackAggregator = EagerCallbackAggregator<void(Expected<void, WebExtensionError>)>::create(WTFMove(completionHandler), { });

    for (RefPtr tab : tabs) {
        tab->close([callbackAggregator](Expected<void, WebExtensionError>&& result) mutable {
            if (!result)
                callbackAggregator.get()(makeUnexpected(result.error()));
        });
    }
}

void WebExtensionContext::tabsExecuteScript(WebPageProxyIdentifier webPageProxyIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, const WebExtensionScriptInjectionParameters& parameters, CompletionHandler<void(Expected<InjectionResults, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"tabs.executeScript()";

    RefPtr tab = getTab(webPageProxyIdentifier, tabIdentifier, IncludeExtensionViews::Yes);
    if (!tab) {
        completionHandler(toWebExtensionError(apiName, nil, @"tab not found"));
        return;
    }

    requestPermissionToAccessURLs({ tab->url() }, tab, [this, protectedThis = Ref { *this }, tab, parameters, completionHandler = WTFMove(completionHandler)](auto&& requestedURLs, auto&& allowedURLs, auto expirationDate) mutable {
        if (!tab->extensionHasPermission()) {
            completionHandler(toWebExtensionError(apiName, nil, @"this extension does not have access to this tab"));
            return;
        }

        auto *webView = tab->webView();
        if (!webView) {
            completionHandler(toWebExtensionError(apiName, nil, @"could not execute script in tab"));
            return;
        }

        std::optional<SourcePair> scriptData;
        if (parameters.code)
            scriptData = SourcePair { parameters.code.value(), URL { } };
        else {
            NSString *filePath = parameters.files.value().first();
            scriptData = sourcePairForResource(filePath, *this);
            if (!scriptData) {
                completionHandler(toWebExtensionError(apiName, nil, @"Invalid resource: %@", filePath));
                return;
            }
        }

        auto scriptPairs = getSourcePairsForParameters(parameters, *this);
        executeScript(scriptPairs, webView, *m_contentScriptWorld, *tab, parameters, *this, [completionHandler = WTFMove(completionHandler)](InjectionResults&& injectionResults) mutable {
            completionHandler(WTFMove(injectionResults));
        });
    });
}

void WebExtensionContext::tabsInsertCSS(WebPageProxyIdentifier webPageProxyIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, const WebExtensionScriptInjectionParameters& parameters, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"tabs.insertCSS()";

    RefPtr tab = getTab(webPageProxyIdentifier, tabIdentifier, IncludeExtensionViews::Yes);
    if (!tab) {
        completionHandler(toWebExtensionError(apiName, nil, @"tab not found"));
        return;
    }

    requestPermissionToAccessURLs({ tab->url() }, tab, [this, protectedThis = Ref { *this }, tab, parameters, completionHandler = WTFMove(completionHandler)](auto&& requestedURLs, auto&& allowedURLs, auto expirationDate) mutable {
        if (!tab->extensionHasPermission()) {
            completionHandler(toWebExtensionError(apiName, nil, @"this extension does not have access to this tab"));
            return;
        }

        auto *webView = tab->webView();
        if (!webView) {
            completionHandler(toWebExtensionError(apiName, nil, @"could not inject stylesheet on this tab"));
            return;
        }

        // FIXME: <https://webkit.org/b/262491> There is currently no way to inject CSS in specific frames based on ID's. If 'frameIds' is passed, default to the main frame.
        auto injectedFrames = parameters.frameIDs ? WebCore::UserContentInjectedFrames::InjectInTopFrameOnly : WebCore::UserContentInjectedFrames::InjectInAllFrames;

        auto styleSheetPairs = getSourcePairsForParameters(parameters, *this);
        injectStyleSheets(styleSheetPairs, webView, Ref { *m_contentScriptWorld }, parameters.styleLevel, injectedFrames, *this);

        completionHandler({ });
    });
}

void WebExtensionContext::tabsRemoveCSS(WebPageProxyIdentifier webPageProxyIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, const WebExtensionScriptInjectionParameters& parameters, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"tabs.removeCSS()";

    RefPtr tab = getTab(webPageProxyIdentifier, tabIdentifier, IncludeExtensionViews::Yes);
    if (!tab) {
        completionHandler(toWebExtensionError(apiName, nil, @"tab not found"));
        return;
    }

    auto *webView = tab->webView();
    if (!webView) {
        completionHandler(toWebExtensionError(apiName, nil, @"could not remove stylesheet on this tab"));
        return;
    }

    // Allow removing CSS without permission, since it is not sensitive and the extension might have had permission before
    // and permission has been revoked since it inserted CSS. This allows for the extension to clean up.

    // FIXME: <https://webkit.org/b/262491> There is currently no way to inject CSS in specific frames based on ID's. If 'frameIds' is passed, default to the main frame.
    auto injectedFrames = parameters.frameIDs ? WebCore::UserContentInjectedFrames::InjectInTopFrameOnly : WebCore::UserContentInjectedFrames::InjectInAllFrames;

    auto styleSheetPairs = getSourcePairsForParameters(parameters, *this);
    removeStyleSheets(styleSheetPairs, webView, injectedFrames, *this);

    completionHandler({ });
}

void WebExtensionContext::fireTabsCreatedEventIfNeeded(const WebExtensionTabParameters& parameters)
{
    constexpr auto type = WebExtensionEventListenerType::TabsOnCreated;
    wakeUpBackgroundContentIfNecessaryToFireEvents({ type }, [=, this, protectedThis = Ref { *this }] {
        sendToProcessesForEvent(type, Messages::WebExtensionContextProxy::DispatchTabsCreatedEvent(parameters));
    });
}

void WebExtensionContext::fireTabsUpdatedEventIfNeeded(const WebExtensionTabParameters& parameters, const WebExtensionTabParameters& changedParameters)
{
    constexpr auto type = WebExtensionEventListenerType::TabsOnUpdated;
    wakeUpBackgroundContentIfNecessaryToFireEvents({ type }, [=, this, protectedThis = Ref { *this }] {
        sendToProcessesForEvent(type, Messages::WebExtensionContextProxy::DispatchTabsUpdatedEvent(parameters, changedParameters));
    });
}

void WebExtensionContext::fireTabsReplacedEventIfNeeded(WebExtensionTabIdentifier replacedTabIdentifier, WebExtensionTabIdentifier newTabIdentifier)
{
    constexpr auto type = WebExtensionEventListenerType::TabsOnReplaced;
    wakeUpBackgroundContentIfNecessaryToFireEvents({ type }, [=, this, protectedThis = Ref { *this }] {
        sendToProcessesForEvent(type, Messages::WebExtensionContextProxy::DispatchTabsReplacedEvent(replacedTabIdentifier, newTabIdentifier));
    });
}

void WebExtensionContext::fireTabsDetachedEventIfNeeded(WebExtensionTabIdentifier tabIdentifier, WebExtensionWindowIdentifier oldWindowIdentifier, size_t oldIndex)
{
    constexpr auto type = WebExtensionEventListenerType::TabsOnDetached;
    wakeUpBackgroundContentIfNecessaryToFireEvents({ type }, [=, this, protectedThis = Ref { *this }] {
        sendToProcessesForEvent(type, Messages::WebExtensionContextProxy::DispatchTabsDetachedEvent(tabIdentifier, oldWindowIdentifier, oldIndex));
    });
}

void WebExtensionContext::fireTabsMovedEventIfNeeded(WebExtensionTabIdentifier tabIdentifier, WebExtensionWindowIdentifier windowIdentifier, size_t oldIndex, size_t newIndex)
{
    constexpr auto type = WebExtensionEventListenerType::TabsOnMoved;
    wakeUpBackgroundContentIfNecessaryToFireEvents({ type }, [=, this, protectedThis = Ref { *this }] {
        sendToProcessesForEvent(type, Messages::WebExtensionContextProxy::DispatchTabsMovedEvent(tabIdentifier, windowIdentifier, oldIndex, newIndex));
    });
}

void WebExtensionContext::fireTabsAttachedEventIfNeeded(WebExtensionTabIdentifier tabIdentifier, WebExtensionWindowIdentifier newWindowIdentifier, size_t newIndex)
{
    constexpr auto type = WebExtensionEventListenerType::TabsOnAttached;
    wakeUpBackgroundContentIfNecessaryToFireEvents({ type }, [=, this, protectedThis = Ref { *this }] {
        sendToProcessesForEvent(type, Messages::WebExtensionContextProxy::DispatchTabsAttachedEvent(tabIdentifier, newWindowIdentifier, newIndex));
    });
}

void WebExtensionContext::fireTabsActivatedEventIfNeeded(WebExtensionTabIdentifier previousActiveTabIdentifier, WebExtensionTabIdentifier newActiveTabIdentifier, WebExtensionWindowIdentifier windowIdentifier)
{
    constexpr auto type = WebExtensionEventListenerType::TabsOnActivated;
    wakeUpBackgroundContentIfNecessaryToFireEvents({ type }, [=, this, protectedThis = Ref { *this }] {
        sendToProcessesForEvent(type, Messages::WebExtensionContextProxy::DispatchTabsActivatedEvent(previousActiveTabIdentifier, newActiveTabIdentifier, windowIdentifier));
    });
}

void WebExtensionContext::fireTabsHighlightedEventIfNeeded(Vector<WebExtensionTabIdentifier> tabIdentifiers, WebExtensionWindowIdentifier windowIdentifier)
{
    constexpr auto type = WebExtensionEventListenerType::TabsOnHighlighted;
    wakeUpBackgroundContentIfNecessaryToFireEvents({ type }, [=, this, protectedThis = Ref { *this }] {
        sendToProcessesForEvent(type, Messages::WebExtensionContextProxy::DispatchTabsHighlightedEvent(tabIdentifiers, windowIdentifier));
    });
}

void WebExtensionContext::fireTabsRemovedEventIfNeeded(WebExtensionTabIdentifier tabIdentifier, WebExtensionWindowIdentifier windowIdentifier, WindowIsClosing windowIsClosing)
{
    constexpr auto type = WebExtensionEventListenerType::TabsOnRemoved;
    wakeUpBackgroundContentIfNecessaryToFireEvents({ type }, [=, this, protectedThis = Ref { *this }] {
        sendToProcessesForEvent(type, Messages::WebExtensionContextProxy::DispatchTabsRemovedEvent(tabIdentifier, windowIdentifier, windowIsClosing));
    });
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
