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
#import "WKWebExtensionWindowConfigurationInternal.h"
#import "WebExtensionContextProxy.h"
#import "WebExtensionContextProxyMessages.h"
#import "WebExtensionUtilities.h"
#import "WebExtensionWindowIdentifier.h"
#import <wtf/BlockPtr.h>

namespace WebKit {

void WebExtensionContext::windowsCreate(const WebExtensionWindowParameters& creationParameters, CompletionHandler<void(Expected<std::optional<WebExtensionWindowParameters>, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"windows.create()";

    if (!canOpenNewWindow()) {
        completionHandler(toWebExtensionError(apiName, nil, @"it is not implemented"));
        return;
    }

    static constexpr CGFloat NaN = std::numeric_limits<CGFloat>::quiet_NaN();
    static constexpr CGRect CGRectNaN = { { NaN, NaN }, { NaN, NaN } };

    auto *configuration = [[WKWebExtensionWindowConfiguration alloc] _init];
    configuration.windowType = toAPI(creationParameters.type.value_or(WebExtensionWindow::Type::Normal));
    configuration.windowState = toAPI(creationParameters.state.value_or(WebExtensionWindow::State::Normal));
    configuration.shouldBeFocused = creationParameters.focused.value_or(true);
    configuration.shouldBePrivate = creationParameters.privateBrowsing.value_or(false);

    if (creationParameters.frame) {
        CGRect desiredFrame = creationParameters.frame.value();

#if PLATFORM(MAC)
        // Window coordinates on macOS have the origin in the bottom-left corner.
        // Web Extensions have window coordinates in the top-left corner.
        CGRect screenFrame = NSScreen.mainScreen.frame;
        if (!std::isnan(desiredFrame.origin.x))
            desiredFrame.origin.x = screenFrame.origin.x + desiredFrame.origin.x;

        if (!std::isnan(desiredFrame.origin.y))
            desiredFrame.origin.y = screenFrame.size.height + screenFrame.origin.y - desiredFrame.size.height - desiredFrame.origin.y;
#endif

        configuration.frame = desiredFrame;
    } else
        configuration.frame = CGRectNaN;

    NSMutableArray<NSURL *> *urls = [NSMutableArray array];
    NSMutableArray<id<WKWebExtensionTab>> *tabs = [NSMutableArray array];

    if (creationParameters.tabs) {
        for (auto& tabParameters : creationParameters.tabs.value()) {
            if (tabParameters.identifier) {
                RefPtr tab = getTab(tabParameters.identifier.value());
                if (!tab) {
                    completionHandler(toWebExtensionError(apiName, nil, @"tab '%llu' was not found", tabParameters.identifier.value().toUInt64()));
                    return;
                }

                [tabs addObject:tab->delegate()];
            } else if (tabParameters.url)
                [urls addObject:static_cast<NSURL *>(tabParameters.url.value())];
        }
    }

    configuration.tabURLs = [urls copy];
    configuration.tabs = [tabs copy];

    RefPtr extensionController = this->extensionController();
    if (!extensionController) {
        completionHandler(toWebExtensionError(apiName, nil, @"No extensionController"));
        return;
    }
    [extensionController->delegate() webExtensionController:extensionController->wrapper() openNewWindowUsingConfiguration:configuration forExtensionContext:wrapper() completionHandler:makeBlockPtr([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](id<WKWebExtensionWindow> newWindow, NSError *error) mutable {
        if (error) {
            RELEASE_LOG_ERROR(Extensions, "Error for open new window: %{public}@", privacyPreservingDescription(error));
            completionHandler(toWebExtensionError(apiName, nil, error.localizedDescription));
            return;
        }

        if (!newWindow) {
            completionHandler({ });
            return;
        }

        RefPtr window = getOrCreateWindow(newWindow);
        completionHandler(window->extensionHasAccess() ? std::optional(window->parameters()) : std::nullopt);
    }).get()];
}

void WebExtensionContext::windowsGet(WebPageProxyIdentifier, WebExtensionWindowIdentifier windowIdentifier, OptionSet<WindowTypeFilter> filter, PopulateTabs populate, CompletionHandler<void(Expected<WebExtensionWindowParameters, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"windows.get()";

    RefPtr window = getWindow(windowIdentifier);
    if (!window) {
        completionHandler(toWebExtensionError(apiName, nil, @"window not found"));
        return;
    }

    if (!window->matches(filter)) {
        completionHandler(toWebExtensionError(apiName, nil, @"window does not match requested 'windowTypes'"));
        return;
    }

    URLVector tabURLs;
    if (populate == PopulateTabs::Yes) {
        for (Ref tab : window->tabs())
            tabURLs.append(tab->url());
    }

    requestPermissionToAccessURLs(tabURLs, nullptr, [window, populate, completionHandler = WTFMove(completionHandler)](auto&& requestedURLs, auto&& allowedURLs, auto expirationDate) mutable {
        completionHandler(window->parameters(populate));
    });
}

void WebExtensionContext::windowsGetLastFocused(OptionSet<WindowTypeFilter> filter, PopulateTabs populate, CompletionHandler<void(Expected<WebExtensionWindowParameters, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"windows.getLastFocused()";

    RefPtr window = frontmostWindow();
    if (!window) {
        completionHandler(toWebExtensionError(apiName, nil, @"window not found"));
        return;
    }

    if (!window->matches(filter)) {
        completionHandler(toWebExtensionError(apiName, nil, @"window does not match requested 'windowTypes'"));
        return;
    }

    URLVector tabURLs;
    if (populate == PopulateTabs::Yes) {
        for (Ref tab : window->tabs())
            tabURLs.append(tab->url());
    }

    requestPermissionToAccessURLs(tabURLs, nullptr, [window, populate, completionHandler = WTFMove(completionHandler)](auto&& requestedURLs, auto&& allowedURLs, auto expirationDate) mutable {
        completionHandler(window->parameters(populate));
    });
}

void WebExtensionContext::windowsGetAll(OptionSet<WindowTypeFilter> filter, PopulateTabs populate, CompletionHandler<void(Expected<Vector<WebExtensionWindowParameters>, WebExtensionError>&&)>&& completionHandler)
{
    URLVector tabURLs;
    WindowVector windows;
    for (Ref window : openWindows()) {
        if (!window->matches(filter))
            continue;

        windows.append(window);

        if (populate != PopulateTabs::Yes)
            continue;

        for (Ref tab : window->tabs())
            tabURLs.append(tab->url());
    }

    requestPermissionToAccessURLs(tabURLs, nullptr, [windows, populate, completionHandler = WTFMove(completionHandler)](auto&& requestedURLs, auto&& allowedURLs, auto expirationDate) mutable {
        // Get the parameters after permission has been granted, so it can include the URLs and titles if allowed.
        auto result = WTF::map(windows, [&](auto& window) {
            return window->parameters(populate);
        });

        completionHandler(WTFMove(result));
    });
}

void WebExtensionContext::windowsUpdate(WebExtensionWindowIdentifier windowIdentifier, WebExtensionWindowParameters updateParameters, CompletionHandler<void(Expected<WebExtensionWindowParameters, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"windows.update()";

    RefPtr window = getWindow(windowIdentifier);
    if (!window) {
        completionHandler(toWebExtensionError(apiName, nil, @"window not found"));
        return;
    }

    auto updateState = [](WebExtensionWindow& window, const WebExtensionWindowParameters& updateParameters, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& stepCompletionHandler) {
        if (!updateParameters.state || updateParameters.state == window.state()) {
            stepCompletionHandler({ });
            return;
        }

        window.setState(updateParameters.state.value(), WTFMove(stepCompletionHandler));
    };

    auto updateFocus = [](WebExtensionWindow& window, const WebExtensionWindowParameters& updateParameters, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& stepCompletionHandler) {
        if (!updateParameters.focused || !updateParameters.focused.value()) {
            stepCompletionHandler({ });
            return;
        }

        window.focus(WTFMove(stepCompletionHandler));
    };

    auto updateFrame = [](WebExtensionWindow& window, const WebExtensionWindowParameters& updateParameters, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& stepCompletionHandler) {
        if (!updateParameters.frame || window.state() != WebExtensionWindow::State::Normal) {
            stepCompletionHandler({ });
            return;
        }

        CGRect currentFrame = window.frame();
        if (CGRectIsNull(currentFrame)) {
            stepCompletionHandler(toWebExtensionError(apiName, nil, @"it is not implemented for 'top', 'left', 'width', and 'height'"));
            return;
        }

        CGRect desiredFrame = updateParameters.frame.value();

        if (std::isnan(desiredFrame.size.width))
            desiredFrame.size.width = currentFrame.size.width;

        if (std::isnan(desiredFrame.size.height))
            desiredFrame.size.height = currentFrame.size.height;

#if PLATFORM(MAC)
        // On macOS, window coordinates originate from the bottom-left of the main screen. When working with
        // multi-screen setups, the screen's frame defines this origin. However, Web Extensions expect window
        // coordinates with the origin in the top-left corner.
        CGRect screenFrame = window.screenFrame();
        if (CGRectIsEmpty(screenFrame)) {
            stepCompletionHandler(toWebExtensionError(apiName, nil, @"it is not implemented for 'top', 'left', 'width', and 'height'"));
            return;
        }

        if (std::isnan(desiredFrame.origin.x))
            desiredFrame.origin.x = currentFrame.origin.x;
        else
            desiredFrame.origin.x = screenFrame.origin.x + desiredFrame.origin.x;

        CGFloat screenTop = screenFrame.size.height + screenFrame.origin.y;
        if (std::isnan(desiredFrame.origin.y)) {
            // Calculate the current top to keep the top-left corner of the window at the same position if the height changed.
            CGFloat currentTop = screenTop - currentFrame.size.height - currentFrame.origin.y;
            desiredFrame.origin.y = screenTop - desiredFrame.size.height - currentTop;
        } else
            desiredFrame.origin.y = screenTop - desiredFrame.size.height - desiredFrame.origin.y;
#else
        if (std::isnan(desiredFrame.origin.x))
            desiredFrame.origin.x = currentFrame.origin.x;

        if (std::isnan(desiredFrame.origin.y))
            desiredFrame.origin.y = currentFrame.origin.y;
#endif

        if (CGRectEqualToRect(currentFrame, desiredFrame)) {
            stepCompletionHandler({ });
            return;
        }

        window.setFrame(desiredFrame, WTFMove(stepCompletionHandler));
    };

    // Frame can only be updated if state is Normal.
    if (updateParameters.frame && window->state() != WebExtensionWindow::State::Normal) {
        ASSERT(!updateParameters.state);
        updateParameters.state = WebExtensionWindow::State::Normal;
    }

    updateState(*window, updateParameters, [window = Ref { *window }, updateParameters = WTFMove(updateParameters), updateFocus = WTFMove(updateFocus), updateFrame = WTFMove(updateFrame), completionHandler = WTFMove(completionHandler)](Expected<void, WebExtensionError>&& stateResult) mutable {
        if (!stateResult) {
            completionHandler(makeUnexpected(stateResult.error()));
            return;
        }

        updateFocus(window, updateParameters, [window, updateParameters = WTFMove(updateParameters), updateFrame = WTFMove(updateFrame), completionHandler = WTFMove(completionHandler)](Expected<void, WebExtensionError>&& focusResult) mutable {
            if (!focusResult) {
                completionHandler(makeUnexpected(focusResult.error()));
                return;
            }

            updateFrame(window, updateParameters, [window, completionHandler = WTFMove(completionHandler)](Expected<void, WebExtensionError>&& frameResult) mutable {
                if (!frameResult) {
                    completionHandler(makeUnexpected(frameResult.error()));
                    return;
                }

                completionHandler(window->parameters());
            });
        });
    });
}

void WebExtensionContext::windowsRemove(WebExtensionWindowIdentifier windowIdentifier, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    RefPtr window = getWindow(windowIdentifier);
    if (!window) {
        completionHandler(toWebExtensionError(@"windows.remove()", nil, @"window not found"));
        return;
    }

    window->close(WTFMove(completionHandler));
}

void WebExtensionContext::fireWindowsEventIfNeeded(WebExtensionEventListenerType type, std::optional<WebExtensionWindowParameters> windowParameters)
{
    wakeUpBackgroundContentIfNecessaryToFireEvents({ type }, [=, this, protectedThis = Ref { *this }] {
        sendToProcessesForEvent(type, Messages::WebExtensionContextProxy::DispatchWindowsEvent(type, windowParameters));
    });
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
