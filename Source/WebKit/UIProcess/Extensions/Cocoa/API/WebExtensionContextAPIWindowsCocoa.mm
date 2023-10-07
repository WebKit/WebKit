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
#import "WebExtensionContextProxy.h"
#import "WebExtensionContextProxyMessages.h"
#import "WebExtensionUtilities.h"
#import "WebExtensionWindowIdentifier.h"
#import "_WKWebExtensionControllerDelegatePrivate.h"
#import "_WKWebExtensionWindowCreationOptionsInternal.h"
#import <wtf/BlockPtr.h>

namespace WebKit {

void WebExtensionContext::windowsCreate(const WebExtensionWindowParameters& creationParameters, CompletionHandler<void(std::optional<WebExtensionWindowParameters>, WebExtensionWindow::Error)>&& completionHandler)
{
    auto delegate = extensionController()->delegate();
    if (![delegate respondsToSelector:@selector(webExtensionController:openNewWindowWithOptions:forExtensionContext:completionHandler:)]) {
        completionHandler(std::nullopt, toErrorString(@"windows.create()", nil, @"it is not implemented"));
        return;
    }

    static constexpr CGFloat NaN = std::numeric_limits<CGFloat>::quiet_NaN();
    static constexpr CGRect CGRectNaN = { { NaN, NaN }, { NaN, NaN } };

    auto *creationOptions = [[_WKWebExtensionWindowCreationOptions alloc] _init];
    creationOptions.desiredWindowType = toAPI(creationParameters.type.value_or(WebExtensionWindow::Type::Normal));
    creationOptions.desiredWindowState = toAPI(creationParameters.state.value_or(WebExtensionWindow::State::Normal));
    creationOptions.shouldFocus = creationParameters.focused.value_or(true);
    creationOptions.shouldUsePrivateBrowsing = creationParameters.privateBrowsing.value_or(false);

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

        creationOptions.desiredFrame = desiredFrame;
    } else
        creationOptions.desiredFrame = CGRectNaN;

    NSMutableArray<NSURL *> *urls = [NSMutableArray array];
    NSMutableArray<id<_WKWebExtensionTab>> *tabs = [NSMutableArray array];

    if (creationParameters.tabs) {
        for (auto& tabParameters : creationParameters.tabs.value()) {
            if (tabParameters.identifier) {
                auto tab = getTab(tabParameters.identifier.value());
                if (!tab) {
                    completionHandler(std::nullopt, toErrorString(@"windows.create()", nil, @"tab '%llu' was not found", tabParameters.identifier.value().toUInt64()));
                    return;
                }

                [tabs addObject:tab->delegate()];
            } else if (tabParameters.url)
                [urls addObject:static_cast<NSURL *>(tabParameters.url.value())];
        }
    }

    creationOptions.desiredURLs = [urls copy];
    creationOptions.desiredTabs = [tabs copy];

    [delegate webExtensionController:extensionController()->wrapper() openNewWindowWithOptions:creationOptions forExtensionContext:wrapper() completionHandler:makeBlockPtr([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](id<_WKWebExtensionWindow> newWindow, NSError *error) mutable {
        if (error) {
            RELEASE_LOG_ERROR(Extensions, "Error for open new window: %{private}@", error);
            completionHandler(std::nullopt, error.localizedDescription);
            return;
        }

        if (!newWindow) {
            completionHandler(std::nullopt, std::nullopt);
            return;
        }

        THROW_UNLESS([newWindow conformsToProtocol:@protocol(_WKWebExtensionWindow)], @"Object returned by webExtensionController:openNewWindowWithOptions:forExtensionContext:completionHandler: does not conform to the _WKWebExtensionWindow protocol");

        auto window = getOrCreateWindow(newWindow);

        completionHandler(window->extensionHasAccess() ? std::optional(window->parameters()) : std::nullopt, std::nullopt);
    }).get()];
}

void WebExtensionContext::windowsGet(WebPageProxyIdentifier, WebExtensionWindowIdentifier windowIdentifier, OptionSet<WindowTypeFilter> filter, PopulateTabs populate, CompletionHandler<void(std::optional<WebExtensionWindowParameters>, WebExtensionWindow::Error)>&& completionHandler)
{
    static NSString * const apiName = @"windows.get()";

    auto window = getWindow(windowIdentifier);
    if (!window) {
        completionHandler(std::nullopt, toErrorString(apiName, nil, @"window not found"));
        return;
    }

    if (!window->matches(filter)) {
        completionHandler(std::nullopt, toErrorString(apiName, nil, @"window does not match requested 'windowTypes'"));
        return;
    }

    completionHandler(window->parameters(populate), std::nullopt);
}

void WebExtensionContext::windowsGetLastFocused(OptionSet<WindowTypeFilter> filter, PopulateTabs populate, CompletionHandler<void(std::optional<WebExtensionWindowParameters>, WebExtensionWindow::Error)>&& completionHandler)
{
    static NSString * const apiName = @"windows.getLastFocused()";

    auto window = frontmostWindow();
    if (!window) {
        completionHandler(std::nullopt, toErrorString(apiName, nil, @"window not found"));
        return;
    }

    if (!window->matches(filter)) {
        completionHandler(std::nullopt, toErrorString(apiName, nil, @"window does not match requested 'windowTypes'"));
        return;
    }

    completionHandler(window->parameters(populate), std::nullopt);
}

void WebExtensionContext::windowsGetAll(OptionSet<WindowTypeFilter> filter, PopulateTabs populate, CompletionHandler<void(Vector<WebExtensionWindowParameters>, WebExtensionWindow::Error)>&& completionHandler)
{
    Vector<WebExtensionWindowParameters> result;
    for (auto& window : openWindows()) {
        if (!window->matches(filter))
            continue;

        result.append(window->parameters(populate));
    }

    completionHandler(result, std::nullopt);
}

void WebExtensionContext::windowsUpdate(WebExtensionWindowIdentifier windowIdentifier, WebExtensionWindowParameters updateParameters, CompletionHandler<void(std::optional<WebExtensionWindowParameters>, WebExtensionWindow::Error)>&& completionHandler)
{
    static NSString * const apiName = @"windows.update()";

    auto window = getWindow(windowIdentifier);
    if (!window) {
        completionHandler(std::nullopt, toErrorString(apiName, nil, @"window not found"));
        return;
    }

    auto updateState = [&](CompletionHandler<void(WebExtensionWindow::Error)>&& stepCompletionHandler) {
        if (!updateParameters.state || updateParameters.state == window->state()) {
            stepCompletionHandler(std::nullopt);
            return;
        }

        window->setState(updateParameters.state.value(), WTFMove(stepCompletionHandler));
    };

    auto updateFocus = [&](CompletionHandler<void(WebExtensionWindow::Error)>&& stepCompletionHandler) {
        if (!updateParameters.focused || !updateParameters.focused.value()) {
            stepCompletionHandler(std::nullopt);
            return;
        }

        window->focus(WTFMove(stepCompletionHandler));
    };

    auto updateFrame = [&](CompletionHandler<void(WebExtensionWindow::Error)>&& stepCompletionHandler) {
        if (!updateParameters.frame || window->state() != WebExtensionWindow::State::Normal) {
            stepCompletionHandler(std::nullopt);
            return;
        }

        CGRect currentFrame = window->frame();
        if (CGRectIsNull(currentFrame)) {
            stepCompletionHandler(toErrorString(apiName, nil, @"it is not implemented for 'top', 'left', 'width', and 'height'"));
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
        CGRect screenFrame = window->screenFrame();
        if (CGRectIsEmpty(screenFrame)) {
            stepCompletionHandler(toErrorString(apiName, nil, @"it is not implemented for 'top', 'left', 'width', and 'height'"));
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
            stepCompletionHandler(std::nullopt);
            return;
        }

        window->setFrame(desiredFrame, WTFMove(stepCompletionHandler));
    };

    // Frame can only be updated if state is Normal.
    if (updateParameters.frame && window->state() != WebExtensionWindow::State::Normal) {
        ASSERT(!updateParameters.state);
        updateParameters.state = WebExtensionWindow::State::Normal;
    }

    updateState([&](WebExtensionWindow::Error stateError) {
        if (stateError) {
            completionHandler(std::nullopt, stateError);
            return;
        }

        updateFocus([&](WebExtensionWindow::Error focusError) {
            if (focusError) {
                completionHandler(std::nullopt, focusError);
                return;
            }

            updateFrame([&](WebExtensionWindow::Error frameError) {
                if (frameError) {
                    completionHandler(std::nullopt, frameError);
                    return;
                }

                completionHandler(window->parameters(), std::nullopt);
            });
        });
    });
}

void WebExtensionContext::windowsRemove(WebExtensionWindowIdentifier windowIdentifier, CompletionHandler<void(WebExtensionWindow::Error)>&& completionHandler)
{
    auto window = getWindow(windowIdentifier);
    if (!window) {
        completionHandler(toErrorString(@"windows.remove()", nil, @"window not found"));
        return;
    }

    window->close(WTFMove(completionHandler));
}

void WebExtensionContext::fireWindowsEventIfNeeded(WebExtensionEventListenerType type, std::optional<WebExtensionWindowParameters> windowParameters)
{
    wakeUpBackgroundContentIfNecessaryToFireEvents({ type }, [&] {
        sendToProcessesForEvent(type, Messages::WebExtensionContextProxy::DispatchWindowsEvent(type, windowParameters));
    });
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
