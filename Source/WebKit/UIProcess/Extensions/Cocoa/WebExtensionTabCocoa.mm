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
#import "WebExtensionTab.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "CocoaHelpers.h"
#import "Logging.h"
#import "WKSnapshotConfiguration.h"
#import "WKWebView.h"
#import "WKWebViewConfigurationPrivate.h"
#import "WKWebViewInternal.h"
#import "WKWebViewPrivate.h"
#import "WebExtensionContext.h"
#import "WebExtensionTabParameters.h"
#import "WebExtensionTabQueryParameters.h"
#import "WebExtensionUtilities.h"
#import "WebExtensionWindowIdentifier.h"
#import "WebPageProxy.h"
#import "_WKWebExtensionTab.h"
#import "_WKWebExtensionTabCreationOptionsInternal.h"
#import <wtf/BlockPtr.h>

namespace WebKit {

WebExtensionTab::WebExtensionTab(const WebExtensionContext& context, _WKWebExtensionTab *delegate)
    : m_identifier(WebExtensionTabIdentifier::generate())
    , m_extensionContext(context)
    , m_delegate(delegate)
    , m_respondsToWindow([delegate respondsToSelector:@selector(windowForWebExtensionContext:)])
    , m_respondsToParentTab([delegate respondsToSelector:@selector(parentTabForWebExtensionContext:)])
    , m_respondsToSetParentTab([delegate respondsToSelector:@selector(setParentTab:forWebExtensionContext:completionHandler:)])
    , m_respondsToMainWebView([delegate respondsToSelector:@selector(mainWebViewForWebExtensionContext:)])
    , m_respondsToWebViews([delegate respondsToSelector:@selector(webViewsForWebExtensionContext:)])
    , m_respondsToTabTitle([delegate respondsToSelector:@selector(tabTitleForWebExtensionContext:)])
    , m_respondsToIsSelected([delegate respondsToSelector:@selector(isSelectedForWebExtensionContext:)])
    , m_respondsToIsPinned([delegate respondsToSelector:@selector(isPinnedForWebExtensionContext:)])
    , m_respondsToPin([delegate respondsToSelector:@selector(pinForWebExtensionContext:completionHandler:)])
    , m_respondsToUnpin([delegate respondsToSelector:@selector(unpinForWebExtensionContext:completionHandler:)])
    , m_respondsToIsReaderModeAvailable([delegate respondsToSelector:@selector(isReaderModeAvailableForWebExtensionContext:)])
    , m_respondsToIsShowingReaderMode([delegate respondsToSelector:@selector(isShowingReaderModeForWebExtensionContext:)])
    , m_respondsToToggleReaderMode([delegate respondsToSelector:@selector(toggleReaderModeForWebExtensionContext:completionHandler:)])
    , m_respondsToIsAudible([delegate respondsToSelector:@selector(isAudibleForWebExtensionContext:)])
    , m_respondsToIsMuted([delegate respondsToSelector:@selector(isMutedForWebExtensionContext:)])
    , m_respondsToMute([delegate respondsToSelector:@selector(muteForWebExtensionContext:completionHandler:)])
    , m_respondsToUnmute([delegate respondsToSelector:@selector(unmuteForWebExtensionContext:completionHandler:)])
    , m_respondsToSize([delegate respondsToSelector:@selector(sizeForWebExtensionContext:)])
    , m_respondsToZoomFactor([delegate respondsToSelector:@selector(zoomFactorForWebExtensionContext:)])
    , m_respondsToSetZoomFactor([delegate respondsToSelector:@selector(setZoomFactor:forWebExtensionContext:completionHandler:)])
    , m_respondsToURL([delegate respondsToSelector:@selector(urlForWebExtensionContext:)])
    , m_respondsToPendingURL([delegate respondsToSelector:@selector(pendingURLForWebExtensionContext:)])
    , m_respondsToIsLoadingComplete([delegate respondsToSelector:@selector(isLoadingCompleteForWebExtensionContext:)])
    , m_respondsToDetectWebpageLocale([delegate respondsToSelector:@selector(detectWebpageLocaleForWebExtensionContext:completionHandler:)])
    , m_respondsToCaptureVisibleWebpage([delegate respondsToSelector:@selector(captureVisibleWebpageForWebExtensionContext:completionHandler:)])
    , m_respondsToLoadURL([delegate respondsToSelector:@selector(loadURL:forWebExtensionContext:completionHandler:)])
    , m_respondsToReload([delegate respondsToSelector:@selector(reloadForWebExtensionContext:completionHandler:)])
    , m_respondsToReloadFromOrigin([delegate respondsToSelector:@selector(reloadFromOriginForWebExtensionContext:completionHandler:)])
    , m_respondsToGoBack([delegate respondsToSelector:@selector(goBackForWebExtensionContext:completionHandler:)])
    , m_respondsToGoForward([delegate respondsToSelector:@selector(goForwardForWebExtensionContext:completionHandler:)])
    , m_respondsToActivate([delegate respondsToSelector:@selector(activateForWebExtensionContext:completionHandler:)])
    , m_respondsToSelect([delegate respondsToSelector:@selector(selectForWebExtensionContext:completionHandler:)])
    , m_respondsToDeselect([delegate respondsToSelector:@selector(deselectForWebExtensionContext:completionHandler:)])
    , m_respondsToDuplicate([delegate respondsToSelector:@selector(duplicateForWebExtensionContext:withOptions:completionHandler:)])
    , m_respondsToClose([delegate respondsToSelector:@selector(closeForWebExtensionContext:completionHandler:)])
{
    ASSERT([delegate conformsToProtocol:@protocol(_WKWebExtensionTab)]);
}

WebExtensionContext* WebExtensionTab::extensionContext() const
{
    return m_extensionContext.get();
}

bool WebExtensionTab::operator==(const WebExtensionTab& other) const
{
    return this == &other || (m_identifier == other.m_identifier && m_extensionContext == other.m_extensionContext && m_delegate.get() == other.m_delegate.get());
}

WebExtensionTabParameters WebExtensionTab::parameters() const
{
    bool hasAccess = extensionHasAccess();
    auto window = this->window();
    auto index = this->index();
    auto parentTab = this->parentTab();

    return {
        identifier(),

        hasAccess ? url() : URL { },
        hasAccess ? title() : nullString(),

        window ? std::optional(window->identifier()) : std::nullopt,

        index != notFound ? std::optional(index) : std::nullopt,
        size(),

        parentTab ? std::optional(parentTab->identifier()) : std::nullopt,

        isActive(),

        m_respondsToIsSelected ? std::optional(isSelected()) : std::nullopt,
        m_respondsToIsPinned ? std::optional(isPinned()) : std::nullopt,
        m_respondsToIsAudible ? std::optional(isAudible()) : std::nullopt,
        m_respondsToIsMuted ? std::optional(isMuted()) : std::nullopt,

        !isLoadingComplete(),
        isPrivate(),

        m_respondsToIsReaderModeAvailable ? std::optional(isReaderModeAvailable()) : std::nullopt,
        m_respondsToIsShowingReaderMode ? std::optional(isShowingReaderMode()) : std::nullopt
    };
}

WebExtensionTabParameters WebExtensionTab::changedParameters(OptionSet<ChangedProperties> changedProperties) const
{
    bool hasAccess = extensionHasAccess();

    return {
        std::nullopt, // identifier

        changedProperties.contains(ChangedProperties::URL) ? std::optional(hasAccess ? url() : URL { }) : std::nullopt,
        changedProperties.contains(ChangedProperties::Title) ? std::optional(hasAccess ? title() : nullString()) : std::nullopt,

        std::nullopt, // windowIdentifier
        std::nullopt, // index

        changedProperties.contains(ChangedProperties::Size) ? std::optional(size()) : std::nullopt,

        std::nullopt, // parentTabIdentifier
        std::nullopt, // active
        std::nullopt, // selected

        m_respondsToIsPinned && changedProperties.contains(ChangedProperties::Pinned) ? std::optional(isPinned()) : std::nullopt,
        m_respondsToIsAudible && changedProperties.contains(ChangedProperties::Audible) ? std::optional(isAudible()) : std::nullopt,
        m_respondsToIsMuted && changedProperties.contains(ChangedProperties::Muted) ? std::optional(isMuted()) : std::nullopt,

        changedProperties.contains(ChangedProperties::Loading) ? std::optional(!isLoadingComplete()) : std::nullopt,

        std::nullopt, // privateBrowsing

        m_respondsToIsReaderModeAvailable && changedProperties.contains(ChangedProperties::ReaderMode) ? std::optional(isReaderModeAvailable()) : std::nullopt,
        m_respondsToIsShowingReaderMode && changedProperties.contains(ChangedProperties::ReaderMode) ? std::optional(isShowingReaderMode()) : std::nullopt
    };
}

bool WebExtensionTab::matches(const WebExtensionTabQueryParameters& parameters, AssumeWindowMatches assumeWindowMatches, std::optional<WebPageProxyIdentifier> webPageProxyIdentifier) const
{
    if (assumeWindowMatches == AssumeWindowMatches::No && (parameters.windowIdentifier || parameters.windowType || parameters.frontmostWindow || parameters.currentWindow)) {
        auto window = this->window();
        if (!window)
            return false;

        if (!window->matches(parameters, webPageProxyIdentifier))
            return false;
    }

    if (auto index = this->index(); parameters.index && (index == notFound || index != parameters.index.value()))
        return false;

    bool isActive = this->isActive();
    if (parameters.active && isActive != parameters.active.value())
        return false;

    if (parameters.audible && isAudible() != parameters.audible.value())
        return false;

    if (parameters.hidden && isActive != !parameters.hidden.value())
        return false;

    if (parameters.loading && isLoadingComplete() != !parameters.loading.value())
        return false;

    if (parameters.muted && isMuted() != parameters.muted.value())
        return false;

    if (parameters.pinned && isPinned() != parameters.pinned.value())
        return false;

    if (parameters.selected && isSelected() != parameters.selected.value())
        return false;

    auto url = this->url();
    if ((parameters.urlPatterns || parameters.titlePattern) && !extensionHasAccess())
        return false;

    if (parameters.urlPatterns) {
        bool matchesAnyPattern = false;
        for (auto& patternString : parameters.urlPatterns.value()) {
            auto matchPattern = WebExtensionMatchPattern::create(patternString);
            if (!matchPattern)
                continue;

            if (matchPattern->matchesURL(url)) {
                matchesAnyPattern = true;
                break;
            }
        }

        if (!matchesAnyPattern)
            return false;
    }

    if (parameters.titlePattern) {
        NSPredicate *predicate = [NSPredicate predicateWithFormat:@"SELF LIKE %@", (NSString *)parameters.titlePattern.value()];
        if (![predicate evaluateWithObject:title()])
            return false;
    }

    return true;
}

bool WebExtensionTab::extensionHasAccess() const
{
    auto url = this->url();
    return extensionContext()->hasPermission(url, delegate());
}

RefPtr<WebExtensionWindow> WebExtensionTab::window(SkipContainsCheck skipCheck) const
{
    if (!isValid() || !m_respondsToWindow)
        return nullptr;

    auto window = [m_delegate windowForWebExtensionContext:m_extensionContext->wrapper()];
    if (!window)
        return nullptr;

    THROW_UNLESS([window conformsToProtocol:@protocol(_WKWebExtensionWindow)], @"Object returned by windowForWebExtensionContext: does not conform to the _WKWebExtensionWindow protocol");

    auto result = m_extensionContext->getOrCreateWindow(window);

    if (skipCheck == SkipContainsCheck::No)
        THROW_UNLESS(result->tabs().contains(*this), @"Window returned by windowForWebExtensionContext: does not contain the tab");

    return result;
}

size_t WebExtensionTab::index() const
{
    if (!isValid() || !m_respondsToWindow)
        return notFound;

    auto window = this->window();
    if (!window)
        return notFound;

    auto result = window->tabs().find(*this);
    THROW_UNLESS(result != notFound, @"Window returned by windowForWebExtensionContext: does not contain the tab");

    return result;
}

RefPtr<WebExtensionTab> WebExtensionTab::parentTab() const
{
    if (!isValid() || !m_respondsToParentTab)
        return nullptr;

    auto parentTab = [m_delegate parentTabForWebExtensionContext:m_extensionContext->wrapper()];
    if (!parentTab)
        return nullptr;

    THROW_UNLESS([parentTab conformsToProtocol:@protocol(_WKWebExtensionTab)], @"Object returned by parentTabForWebExtensionContext: does not conform to the _WKWebExtensionTab protocol");
    return m_extensionContext->getOrCreateTab(parentTab);
}

void WebExtensionTab::setParentTab(RefPtr<WebExtensionTab> parentTab, CompletionHandler<void(Error)>&& completionHandler)
{
    if (!isValid() || !m_respondsToSetParentTab) {
        completionHandler(toErrorString(@"tabs.update()", nil, @"it is not implemented for 'openerTabId'"));
        return;
    }

    [m_delegate setParentTab:(parentTab ? parentTab->delegate() : nil) forWebExtensionContext:m_extensionContext->wrapper() completionHandler:^(NSError *error) {
        if (error) {
            RELEASE_LOG_ERROR(Extensions, "Error for setPartentTab: %{private}@", error);
            completionHandler(error.localizedDescription);
            return;
        }

        completionHandler(std::nullopt);
    }];
}

WKWebView *WebExtensionTab::mainWebView() const
{
    if (!isValid() || !m_respondsToMainWebView)
        return nil;

    auto *mainWebView = [m_delegate mainWebViewForWebExtensionContext:m_extensionContext->wrapper()];
    if (!mainWebView)
        return nil;

    THROW_UNLESS([mainWebView isKindOfClass:WKWebView.class], @"Object returned by mainWebViewForWebExtensionContext: is not a WKWebView");
    THROW_UNLESS(mainWebView.configuration._webExtensionController == extensionContext()->extensionController()->wrapper(), @"WKWebView returned by mainWebViewForWebExtensionContext: is not configured with the same _WKWebExtensionController as extension context");

    if (m_respondsToWebViews) {
        auto *webViews = [m_delegate webViewsForWebExtensionContext:m_extensionContext->wrapper()];
        THROW_UNLESS([webViews isKindOfClass:NSArray.class], @"Object returned by webViewsForWebExtensionContext: is not an array");
        THROW_UNLESS([webViews containsObject:mainWebView], @"Array returned by webViewsForWebExtensionContext: does not contain the main web view");
    }

    return mainWebView;
}

NSArray *WebExtensionTab::webViews() const
{
    if (!isValid() || !m_respondsToWebViews || !m_respondsToMainWebView) {
        // This approach is nil-safe, unlike using @[ mainWebView() ].
        return [NSArray arrayWithObjects:mainWebView(), nil];
    }

    auto *webViews = [m_delegate webViewsForWebExtensionContext:m_extensionContext->wrapper()];
    THROW_UNLESS([webViews isKindOfClass:NSArray.class], @"Object returned by webViewsForWebExtensionContext: is not an array");

    for (WKWebView *webView in webViews) {
        THROW_UNLESS([webView isKindOfClass:WKWebView.class], @"Object in array returned by webViewsForWebExtensionContext: is not a WKWebView");
        THROW_UNLESS(webView.configuration._webExtensionController == extensionContext()->extensionController()->wrapper(), @"WKWebView returned by webViewsForWebExtensionContext: is not configured with the same _WKWebExtensionController as extension context");
    }

    if (auto *mainWebView = [m_delegate mainWebViewForWebExtensionContext:m_extensionContext->wrapper()])
        THROW_UNLESS([webViews containsObject:mainWebView], @"Array returned by webViewsForWebExtensionContext: does not contain the main web view");

    return webViews;
}

String WebExtensionTab::title() const
{
    if (!isValid() || !m_respondsToTabTitle)
        return mainWebView().title;

    auto *tabTitle = [m_delegate tabTitleForWebExtensionContext:m_extensionContext->wrapper()];
    THROW_UNLESS(!tabTitle || [tabTitle isKindOfClass:NSString.class], @"Object returned by tabTitleForWebExtensionContext: is not a string");

    return tabTitle;
}

bool WebExtensionTab::isActive() const
{
    if (!isValid())
        return false;

    auto window = this->window();
    return window ? window->activeTab() == this : false;
}

bool WebExtensionTab::isSelected() const
{
    if (!isValid() || !m_respondsToIsSelected)
        return isActive();

    return [m_delegate isSelectedForWebExtensionContext:m_extensionContext->wrapper()];
}

void WebExtensionTab::pin(CompletionHandler<void(Error)>&& completionHandler)
{
    if (!isValid() || !m_respondsToPin) {
        completionHandler(toErrorString(@"tabs.update()", nil, @"it is not implemented for 'pinned' set to `true`"));
        return;
    }

    [m_delegate pinForWebExtensionContext:m_extensionContext->wrapper() completionHandler:^(NSError *error) {
        if (error) {
            RELEASE_LOG_ERROR(Extensions, "Error for pin: %{private}@", error);
            completionHandler(error.localizedDescription);
            return;
        }

        completionHandler(std::nullopt);
    }];
}

void WebExtensionTab::unpin(CompletionHandler<void(Error)>&& completionHandler)
{
    if (!isValid() || !m_respondsToUnpin) {
        completionHandler(toErrorString(@"tabs.update()", nil, @"it is not implemented for 'pinned' set to `false`"));
        return;
    }

    [m_delegate unpinForWebExtensionContext:m_extensionContext->wrapper() completionHandler:^(NSError *error) {
        if (error) {
            RELEASE_LOG_ERROR(Extensions, "Error for unpin: %{private}@", error);
            completionHandler(error.localizedDescription);
            return;
        }

        completionHandler(std::nullopt);
    }];
}

bool WebExtensionTab::isPinned() const
{
    if (!isValid() || !m_respondsToIsPinned)
        return false;

    return [m_delegate isPinnedForWebExtensionContext:m_extensionContext->wrapper()];
}

bool WebExtensionTab::isPrivate() const
{
    if (!isValid())
        return false;

    auto window = this->window();
    return window ? window->isPrivate() : false;
}

void WebExtensionTab::toggleReaderMode(CompletionHandler<void(Error)>&& completionHandler)
{
    if (!isValid() || !m_respondsToToggleReaderMode) {
        completionHandler(toErrorString(@"tabs.toggleReaderMode()", nil, @"it is not implemented"));
        return;
    }

    [m_delegate toggleReaderModeForWebExtensionContext:m_extensionContext->wrapper() completionHandler:^(NSError *error) {
        if (error) {
            RELEASE_LOG_ERROR(Extensions, "Error for toggleReaderMode: %{private}@", error);
            completionHandler(error.localizedDescription);
            return;
        }

        completionHandler(std::nullopt);
    }];
}

bool WebExtensionTab::isReaderModeAvailable() const
{
    if (!isValid() || !m_respondsToIsReaderModeAvailable)
        return false;

    return [m_delegate isReaderModeAvailableForWebExtensionContext:m_extensionContext->wrapper()];
}

bool WebExtensionTab::isShowingReaderMode() const
{
    if (!isValid() || !m_respondsToIsShowingReaderMode)
        return false;

    return [m_delegate isShowingReaderModeForWebExtensionContext:m_extensionContext->wrapper()];
}

void WebExtensionTab::mute(CompletionHandler<void(Error)>&& completionHandler)
{
    if (!isValid() || !m_respondsToMute) {
        completionHandler(toErrorString(@"tabs.update()", nil, @"it is not implemented for 'muted' set to `true`"));
        return;
    }

    [m_delegate muteForWebExtensionContext:m_extensionContext->wrapper() completionHandler:^(NSError *error) {
        if (error) {
            RELEASE_LOG_ERROR(Extensions, "Error for mute: %{private}@", error);
            completionHandler(error.localizedDescription);
            return;
        }

        completionHandler(std::nullopt);
    }];
}

void WebExtensionTab::unmute(CompletionHandler<void(Error)>&& completionHandler)
{
    if (!isValid() || !m_respondsToUnmute) {
        completionHandler(toErrorString(@"tabs.update()", nil, @"it is not implemented for 'muted' set to `false`"));
        return;
    }

    [m_delegate unmuteForWebExtensionContext:m_extensionContext->wrapper() completionHandler:^(NSError *error) {
        if (error) {
            RELEASE_LOG_ERROR(Extensions, "Error for unmute: %{private}@", error);
            completionHandler(error.localizedDescription);
            return;
        }

        completionHandler(std::nullopt);
    }];
}

bool WebExtensionTab::isAudible() const
{
    if (!isValid() || !m_respondsToIsAudible)
        return false;

    return [m_delegate isAudibleForWebExtensionContext:m_extensionContext->wrapper()];
}

bool WebExtensionTab::isMuted() const
{
    if (!isValid() || !m_respondsToIsMuted)
        return false;

    return [m_delegate isMutedForWebExtensionContext:m_extensionContext->wrapper()];
}

CGSize WebExtensionTab::size() const
{
    if (!isValid() || !m_respondsToSize)
        return mainWebView().frame.size;

    return [m_delegate sizeForWebExtensionContext:m_extensionContext->wrapper()];
}

double WebExtensionTab::zoomFactor() const
{
    if (!isValid() || !m_respondsToZoomFactor)
        return mainWebView().pageZoom;

    return [m_delegate zoomFactorForWebExtensionContext:m_extensionContext->wrapper()];
}

void WebExtensionTab::setZoomFactor(double zoomFactor, CompletionHandler<void(Error)>&& completionHandler)
{
    if (!isValid() || !m_respondsToSetZoomFactor) {
        mainWebView().pageZoom = zoomFactor;
        completionHandler(std::nullopt);
        return;
    }

    [m_delegate setZoomFactor:zoomFactor forWebExtensionContext:m_extensionContext->wrapper() completionHandler:^(NSError *error) {
        if (error) {
            RELEASE_LOG_ERROR(Extensions, "Error for setZoomFactor: %{private}@", error);
            completionHandler(error.localizedDescription);
            return;
        }

        completionHandler(std::nullopt);
    }];
}

URL WebExtensionTab::url() const
{
    if (!isValid() || !m_respondsToURL)
        return mainWebView().URL;

    auto *url = [m_delegate urlForWebExtensionContext:m_extensionContext->wrapper()];
    THROW_UNLESS(!url || [url isKindOfClass:NSURL.class], @"Object returned by urlForWebExtensionContext: is not a URL");

    return url;
}

URL WebExtensionTab::pendingURL() const
{
    if (!isValid() || !m_respondsToPendingURL)
        return { };

    auto *pendingURL = [m_delegate pendingURLForWebExtensionContext:m_extensionContext->wrapper()];
    THROW_UNLESS(!pendingURL || [pendingURL isKindOfClass:NSURL.class], @"Object returned by pendingURLForWebExtensionContext: is not a URL");

    return pendingURL;
}

bool WebExtensionTab::isLoadingComplete() const
{
    if (!isValid() || !m_respondsToIsLoadingComplete)
        return !mainWebView().isLoading;

    return [m_delegate isLoadingCompleteForWebExtensionContext:m_extensionContext->wrapper()];
}

void WebExtensionTab::detectWebpageLocale(CompletionHandler<void(NSLocale *, Error)>&& completionHandler)
{
    if (!isValid() || !m_respondsToDetectWebpageLocale) {
        completionHandler(nil, toErrorString(@"tabs.detectLanguage()", nil, @"it is not implemented"));
        return;
    }

    [m_delegate detectWebpageLocaleForWebExtensionContext:m_extensionContext->wrapper() completionHandler:^(NSLocale *locale, NSError *error) {
        if (error) {
            RELEASE_LOG_ERROR(Extensions, "Error for detectWebpageLocale: %{private}@", error);
            completionHandler(nil, error.localizedDescription);
            return;
        }

        THROW_UNLESS(!locale || [locale isKindOfClass:NSLocale.class], @"Object passed to completionHandler of detectWebpageLocaleForWebExtensionContext:completionHandler: is not an NSLocale");
        completionHandler(locale, std::nullopt);
    }];
}

void WebExtensionTab::captureVisibleWebpage(CompletionHandler<void(CocoaImage *, Error)>&& completionHandler)
{
    auto internalCompletionHandler = makeBlockPtr([completionHandler = WTFMove(completionHandler)](CocoaImage *image, NSError *error) mutable {
        if (error) {
            RELEASE_LOG_ERROR(Extensions, "Error for captureVisibleWebpage: %{private}@", error);
            completionHandler(nil, error.localizedDescription);
            return;
        }

        THROW_UNLESS(!image || [image isKindOfClass:[CocoaImage class]], @"Object passed to completionHandler of captureVisibleWebpageForWebExtensionContext:completionHandler: is not an image");
        completionHandler(image, std::nullopt);
    });

    if (!isValid() || !m_respondsToCaptureVisibleWebpage) {
        auto *mainWebView = this->mainWebView();
        if (!mainWebView) {
            completionHandler(nil, toErrorString(@"tabs.captureVisibleTab()", nil, @"capture is unavailable for this tab"));
            return;
        }

        NSRect snapshotRect = mainWebView.bounds;
#if PLATFORM(MAC)
        snapshotRect.size.height -= mainWebView._topContentInset;
#endif

        WKSnapshotConfiguration *snapshotConfiguration = [[WKSnapshotConfiguration alloc] init];
        snapshotConfiguration.rect = snapshotRect;

        [mainWebView takeSnapshotWithConfiguration:snapshotConfiguration completionHandler:internalCompletionHandler.get()];
        return;
    }

    [m_delegate captureVisibleWebpageForWebExtensionContext:m_extensionContext->wrapper() completionHandler:internalCompletionHandler.get()];
}

void WebExtensionTab::loadURL(URL url, CompletionHandler<void(Error)>&& completionHandler)
{
    if (!isValid() || !m_respondsToLoadURL) {
        [mainWebView() loadRequest:[NSURLRequest requestWithURL:url]];
        completionHandler(std::nullopt);
        return;
    }

    [m_delegate loadURL:url forWebExtensionContext:m_extensionContext->wrapper() completionHandler:^(NSError *error) {
        if (error) {
            RELEASE_LOG_ERROR(Extensions, "Error for loadURL: %{private}@", error);
            completionHandler(error.localizedDescription);
            return;
        }

        completionHandler(std::nullopt);
    }];
}

void WebExtensionTab::reload(CompletionHandler<void(Error)>&& completionHandler)
{
    if (!isValid() || !m_respondsToReload) {
        [mainWebView() reload];
        completionHandler(std::nullopt);
        return;
    }

    [m_delegate reloadForWebExtensionContext:m_extensionContext->wrapper() completionHandler:^(NSError *error) {
        if (error) {
            RELEASE_LOG_ERROR(Extensions, "Error for reload: %{private}@", error);
            completionHandler(error.localizedDescription);
            return;
        }

        completionHandler(std::nullopt);
    }];
}

void WebExtensionTab::reloadFromOrigin(CompletionHandler<void(Error)>&& completionHandler)
{
    if (!isValid() || !m_respondsToReloadFromOrigin) {
        [mainWebView() reloadFromOrigin];
        completionHandler(std::nullopt);
        return;
    }

    [m_delegate reloadFromOriginForWebExtensionContext:m_extensionContext->wrapper() completionHandler:^(NSError *error) {
        if (error) {
            RELEASE_LOG_ERROR(Extensions, "Error for reloadFromOrigin: %{private}@", error);
            completionHandler(error.localizedDescription);
            return;
        }

        completionHandler(std::nullopt);
    }];
}

void WebExtensionTab::goBack(CompletionHandler<void(Error)>&& completionHandler)
{
    if (!isValid() || !m_respondsToGoBack) {
        [mainWebView() goBack];
        completionHandler(std::nullopt);
        return;
    }

    [m_delegate goBackForWebExtensionContext:m_extensionContext->wrapper() completionHandler:^(NSError *error) {
        if (error) {
            RELEASE_LOG_ERROR(Extensions, "Error for goBack: %{private}@", error);
            completionHandler(error.localizedDescription);
            return;
        }

        completionHandler(std::nullopt);
    }];
}

void WebExtensionTab::goForward(CompletionHandler<void(Error)>&& completionHandler)
{
    if (!isValid() || !m_respondsToGoForward) {
        [mainWebView() goForward];
        completionHandler(std::nullopt);
        return;
    }

    [m_delegate goForwardForWebExtensionContext:m_extensionContext->wrapper() completionHandler:^(NSError *error) {
        if (error) {
            RELEASE_LOG_ERROR(Extensions, "Error for goForward: %{private}@", error);
            completionHandler(error.localizedDescription);
            return;
        }

        completionHandler(std::nullopt);
    }];
}

void WebExtensionTab::activate(CompletionHandler<void(Error)>&& completionHandler)
{
    if (!isValid() || !m_respondsToActivate) {
        completionHandler(toErrorString(@"tabs.update()", nil, @"it is not implemented for 'active' set to `true`"));
        return;
    }

    [m_delegate activateForWebExtensionContext:m_extensionContext->wrapper() completionHandler:^(NSError *error) {
        if (error) {
            RELEASE_LOG_ERROR(Extensions, "Error for activate: %{private}@", error);
            completionHandler(error.localizedDescription);
            return;
        }

        completionHandler(std::nullopt);
    }];
}

void WebExtensionTab::select(CompletionHandler<void(Error)>&& completionHandler)
{
    if (!isValid() || !m_respondsToSelect) {
        completionHandler(toErrorString(@"tabs.update()", nil, @"it is not implemented for 'highlighted' or 'selected' set to `true`"));
        return;
    }

    [m_delegate selectForWebExtensionContext:m_extensionContext->wrapper() completionHandler:^(NSError *error) {
        if (error) {
            RELEASE_LOG_ERROR(Extensions, "Error for select: %{private}@", error);
            completionHandler(error.localizedDescription);
            return;
        }

        completionHandler(std::nullopt);
    }];
}

void WebExtensionTab::deselect(CompletionHandler<void(Error)>&& completionHandler)
{
    if (!isValid() || !m_respondsToDeselect) {
        completionHandler(toErrorString(@"tabs.update()", nil, @"it is not implemented for 'highlighted' or 'selected' set to `false`"));
        return;
    }

    [m_delegate deselectForWebExtensionContext:m_extensionContext->wrapper() completionHandler:^(NSError *error) {
        if (error) {
            RELEASE_LOG_ERROR(Extensions, "Error for deselect: %{private}@", error);
            completionHandler(error.localizedDescription);
            return;
        }

        completionHandler(std::nullopt);
    }];
}

void WebExtensionTab::duplicate(const WebExtensionTabParameters& parameters, CompletionHandler<void(RefPtr<WebExtensionTab>, Error)>&& completionHandler)
{
    ASSERT(!parameters.audible);
    ASSERT(!parameters.loading);
    ASSERT(!parameters.muted);
    ASSERT(!parameters.parentTabIdentifier);
    ASSERT(!parameters.pinned);
    ASSERT(!parameters.privateBrowsing);
    ASSERT(!parameters.readerModeAvailable);
    ASSERT(!parameters.showingReaderMode);
    ASSERT(!parameters.size);
    ASSERT(!parameters.title);
    ASSERT(!parameters.url);

    if (!isValid() || !m_respondsToDuplicate) {
        completionHandler(nullptr, toErrorString(@"tabs.duplicate()", nil, @"it is not implemented"));
        return;
    }

    auto window = parameters.windowIdentifier ? m_extensionContext->getWindow(parameters.windowIdentifier.value()) : this->window();

    size_t index = 0;
    if (parameters.index)
        index = parameters.index.value();
    else if (auto currentIndex = this->index(); currentIndex != notFound)
        index = currentIndex + 1;

    _WKWebExtensionTabCreationOptions *duplicateOptions = [[_WKWebExtensionTabCreationOptions alloc] _init];
    duplicateOptions.shouldActivate = parameters.active.value_or(true);
    duplicateOptions.shouldSelect = duplicateOptions.shouldActivate ?: parameters.selected.value_or(false);
    duplicateOptions.desiredWindow = window ? window->delegate() : nil;
    duplicateOptions.desiredIndex = index;

    [m_delegate duplicateForWebExtensionContext:m_extensionContext->wrapper() withOptions:duplicateOptions completionHandler:^(id<_WKWebExtensionTab> duplicatedTab, NSError *error) {
        if (error) {
            RELEASE_LOG_ERROR(Extensions, "Error for duplicate: %{private}@", error);
            completionHandler(nullptr, error.localizedDescription);
            return;
        }

        if (!duplicatedTab) {
            completionHandler(nullptr, std::nullopt);
            return;
        }

        THROW_UNLESS([duplicatedTab conformsToProtocol:@protocol(_WKWebExtensionTab)], @"Object passed to completionHandler of duplicateForWebExtensionContext:withOptions:completionHandler: does not conform to the _WKWebExtensionTab protocol");
        completionHandler(m_extensionContext->getOrCreateTab(duplicatedTab), std::nullopt);
    }];
}

void WebExtensionTab::close(CompletionHandler<void(Error)>&& completionHandler)
{
    if (!isValid() || !m_respondsToClose) {
        completionHandler(toErrorString(@"tabs.remove()", nil, @"it is not implemented"));
        return;
    }

    [m_delegate closeForWebExtensionContext:m_extensionContext->wrapper() completionHandler:^(NSError *error) {
        if (error) {
            RELEASE_LOG_ERROR(Extensions, "Error for tab close: %{private}@", error);
            completionHandler(error.localizedDescription);
            return;
        }

        completionHandler(std::nullopt);
    }];
}

WebExtensionTab::WebProcessProxySet WebExtensionTab::processes(WebExtensionEventListenerType type, MainWebViewOnly mainWebViewOnly) const
{
    if (!isValid())
        return { };

    auto *webViews = mainWebViewOnly == MainWebViewOnly::Yes ? [NSArray arrayWithObject:mainWebView()] : this->webViews();

    WebProcessProxySet result;
    for (WKWebView *webView in webViews) {
        if (!extensionContext()->pageListensForEvent(*webView._page, type))
            continue;

        Ref process = webView._page->process();
        if (process->canSendMessage())
            result.add(WTFMove(process));
    }

    return result;
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
