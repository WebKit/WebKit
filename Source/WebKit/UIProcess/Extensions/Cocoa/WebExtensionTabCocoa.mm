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
    : m_extensionContext(context)
    , m_delegate(delegate)
    , m_respondsToWindow([delegate respondsToSelector:@selector(windowForWebExtensionContext:)])
    , m_respondsToParentTab([delegate respondsToSelector:@selector(parentTabForWebExtensionContext:)])
    , m_respondsToSetParentTab([delegate respondsToSelector:@selector(setParentTab:forWebExtensionContext:completionHandler:)])
    , m_respondsToMainWebView([delegate respondsToSelector:@selector(mainWebViewForWebExtensionContext:)])
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
    , m_respondsToShouldGrantTabPermissionsOnUserGesture([delegate respondsToSelector:@selector(shouldGrantTabPermissionsOnUserGestureForWebExtensionContext:)])
{
    ASSERT([delegate conformsToProtocol:@protocol(_WKWebExtensionTab)]);

    // Access to cache the result early, when the window is associated.
    isPrivate();
}

WebExtensionContext* WebExtensionTab::extensionContext() const
{
    return m_extensionContext.get();
}

bool WebExtensionTab::operator==(const WebExtensionTab& other) const
{
    return this == &other || (identifier() == other.identifier() && m_extensionContext == other.m_extensionContext && m_delegate.get() == other.m_delegate.get());
}

WebExtensionTabParameters WebExtensionTab::parameters() const
{
    bool hasPermission = extensionHasPermission();
    RefPtr window = this->window();
    auto index = this->index();
    RefPtr parentTab = this->parentTab();

    return {
        identifier(),

        hasPermission ? url() : URL { },
        hasPermission ? title() : nullString(),

        window ? window->identifier() : WebExtensionWindowConstants::NoneIdentifier,
        index,

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
    if (changedProperties.isEmpty())
        changedProperties = this->changedProperties();

    bool hasPermission = extensionHasPermission();

    return {
        std::nullopt, // identifier

        changedProperties.contains(ChangedProperties::URL) ? std::optional(hasPermission ? url() : URL { }) : std::nullopt,
        changedProperties.contains(ChangedProperties::Title) ? std::optional(hasPermission ? title() : nullString()) : std::nullopt,

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
    if (!extensionHasAccess())
        return false;

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
    if ((parameters.urlPatterns || parameters.titlePattern) && !extensionHasPermission())
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
        if (!WebCore::matchesWildcardPattern(parameters.titlePattern.value(), title()))
            return false;
    }

    return true;
}

bool WebExtensionTab::extensionHasAccess() const
{
    bool isPrivate = this->isPrivate();
    return !isPrivate || (isPrivate && extensionContext()->hasAccessInPrivateBrowsing());
}

bool WebExtensionTab::extensionHasPermission() const
{
    ASSERT(extensionHasAccess());

    return extensionContext()->hasPermission(url(), const_cast<WebExtensionTab*>(this));
}

bool WebExtensionTab::extensionHasTemporaryPermission() const
{
    ASSERT(extensionHasAccess());
    RefPtr temporaryPattern = temporaryPermissionMatchPattern();
    return temporaryPattern && temporaryPattern->matchesURL(url());
}

RefPtr<WebExtensionWindow> WebExtensionTab::window() const
{
    if (!isValid() || !m_respondsToWindow)
        return nullptr;

    auto window = [m_delegate windowForWebExtensionContext:m_extensionContext->wrapper()];
    if (!window)
        return nullptr;

    THROW_UNLESS([window conformsToProtocol:@protocol(_WKWebExtensionWindow)], @"Object returned by windowForWebExtensionContext: does not conform to the _WKWebExtensionWindow protocol");

    return m_extensionContext->getOrCreateWindow(window);
}

size_t WebExtensionTab::index() const
{
    if (!isValid() || !m_respondsToWindow)
        return notFound;

    RefPtr window = this->window();
    if (!window)
        return notFound;

    return window->tabs().find(*this);
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

void WebExtensionTab::setParentTab(RefPtr<WebExtensionTab> parentTab, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"tabs.update()";

    if (!isValid() || !m_respondsToSetParentTab) {
        completionHandler(toWebExtensionError(apiName, nil, @"it is not implemented for 'openerTabId'"));
        return;
    }

    [m_delegate setParentTab:(parentTab ? parentTab->delegate() : nil) forWebExtensionContext:m_extensionContext->wrapper() completionHandler:makeBlockPtr([protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](NSError *error) mutable {
        if (error) {
            RELEASE_LOG_ERROR(Extensions, "Error for setParentTab: %{public}@", privacyPreservingDescription(error));
            completionHandler(toWebExtensionError(apiName, nil, error.localizedDescription));
            return;
        }

        completionHandler({ });
    }).get()];
}

WKWebView *WebExtensionTab::mainWebView() const
{
    if (!isValid() || !m_respondsToMainWebView)
        return nil;

    auto *mainWebView = [m_delegate mainWebViewForWebExtensionContext:m_extensionContext->wrapper()];
    if (!mainWebView)
        return nil;

    THROW_UNLESS([mainWebView isKindOfClass:WKWebView.class], @"Object returned by mainWebViewForWebExtensionContext: is not a WKWebView");

    auto *configuration = mainWebView.configuration;
    if (!configuration._webExtensionController || configuration._webExtensionController != extensionContext()->extensionController()->wrapper()) {
        RELEASE_LOG_ERROR_IF(!configuration._webExtensionController, Extensions, "%{public}@ returned by mainWebViewForWebExtensionContext: is not configured with a _WKWebExtensionController", mainWebView);
        RELEASE_LOG_ERROR_IF(configuration._webExtensionController && configuration._webExtensionController != extensionContext()->extensionController()->wrapper(), Extensions, "%{public}@ returned by mainWebViewForWebExtensionContext: is not configured with the same _WKWebExtensionController as extension context; %{public}@ != %{public}@", mainWebView, configuration._webExtensionController, extensionContext()->extensionController()->wrapper());
        ASSERT_NOT_REACHED();
        return nil;
    }

    return mainWebView;
}

String WebExtensionTab::title() const
{
    if (!isValid() || !m_respondsToTabTitle)
        return mainWebView().title;

    auto *tabTitle = [m_delegate tabTitleForWebExtensionContext:m_extensionContext->wrapper()];
    THROW_UNLESS(!tabTitle || [tabTitle isKindOfClass:NSString.class], @"Object returned by tabTitleForWebExtensionContext: is not a string");

    return tabTitle;
}

bool WebExtensionTab::isOpen() const
{
    return m_isOpen && isValid();
}

bool WebExtensionTab::isActive() const
{
    if (!isValid())
        return false;

    RefPtr window = this->window();
    return window ? window->activeTab() == this : false;
}

bool WebExtensionTab::isSelected() const
{
    if (!isValid() || !m_respondsToIsSelected)
        return isActive();

    return [m_delegate isSelectedForWebExtensionContext:m_extensionContext->wrapper()];
}

void WebExtensionTab::pin(CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"tabs.update()";

    if (!isValid() || !m_respondsToPin) {
        completionHandler(toWebExtensionError(apiName, nil, @"it is not implemented for 'pinned' set to `true`"));
        return;
    }

    [m_delegate pinForWebExtensionContext:m_extensionContext->wrapper() completionHandler:makeBlockPtr([protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](NSError *error) mutable {
        if (error) {
            RELEASE_LOG_ERROR(Extensions, "Error for pin: %{public}@", privacyPreservingDescription(error));
            completionHandler(toWebExtensionError(apiName, nil, error.localizedDescription));
            return;
        }

        completionHandler({ });
    }).get()];
}

void WebExtensionTab::unpin(CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"tabs.update()";

    if (!isValid() || !m_respondsToUnpin) {
        completionHandler(toWebExtensionError(apiName, nil, @"it is not implemented for 'pinned' set to `false`"));
        return;
    }

    [m_delegate unpinForWebExtensionContext:m_extensionContext->wrapper() completionHandler:makeBlockPtr([protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](NSError *error) mutable {
        if (error) {
            RELEASE_LOG_ERROR(Extensions, "Error for unpin: %{public}@", privacyPreservingDescription(error));
            completionHandler(toWebExtensionError(apiName, nil, error.localizedDescription));
            return;
        }

        completionHandler({ });
    }).get()];
}

bool WebExtensionTab::isPinned() const
{
    if (!isValid() || !m_respondsToIsPinned)
        return false;

    return [m_delegate isPinnedForWebExtensionContext:m_extensionContext->wrapper()];
}

bool WebExtensionTab::isPrivate() const
{
    if (m_cachedPrivate)
        return m_private;

    if (!isValid())
        return false;

    RefPtr window = this->window();
    if (!window)
        return false;

    // Private can't change after the fact, so cache it for quick access and to ensure it does not change.
    m_private = window->isPrivate();
    m_cachedPrivate = true;

    return m_private;
}

void WebExtensionTab::toggleReaderMode(CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"tabs.toggleReaderMode()";

    if (!isValid() || !m_respondsToToggleReaderMode) {
        completionHandler(toWebExtensionError(apiName, nil, @"it is not implemented"));
        return;
    }

    [m_delegate toggleReaderModeForWebExtensionContext:m_extensionContext->wrapper() completionHandler:makeBlockPtr([protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](NSError *error) mutable {
        if (error) {
            RELEASE_LOG_ERROR(Extensions, "Error for toggleReaderMode: %{public}@", privacyPreservingDescription(error));
            completionHandler(toWebExtensionError(apiName, nil, error.localizedDescription));
            return;
        }

        completionHandler({ });
    }).get()];
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

void WebExtensionTab::mute(CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"tabs.update()";

    if (!isValid() || !m_respondsToMute) {
        completionHandler(toWebExtensionError(apiName, nil, @"it is not implemented for 'muted' set to `true`"));
        return;
    }

    [m_delegate muteForWebExtensionContext:m_extensionContext->wrapper() completionHandler:makeBlockPtr([protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](NSError *error) mutable {
        if (error) {
            RELEASE_LOG_ERROR(Extensions, "Error for mute: %{public}@", privacyPreservingDescription(error));
            completionHandler(toWebExtensionError(apiName, nil, error.localizedDescription));
            return;
        }

        completionHandler({ });
    }).get()];
}

void WebExtensionTab::unmute(CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"tabs.update()";

    if (!isValid() || !m_respondsToUnmute) {
        completionHandler(toWebExtensionError(apiName, nil, @"it is not implemented for 'muted' set to `false`"));
        return;
    }

    [m_delegate unmuteForWebExtensionContext:m_extensionContext->wrapper() completionHandler:makeBlockPtr([protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](NSError *error) mutable {
        if (error) {
            RELEASE_LOG_ERROR(Extensions, "Error for unmute: %{public}@", privacyPreservingDescription(error));
            completionHandler(toWebExtensionError(apiName, nil, error.localizedDescription));
            return;
        }

        completionHandler({ });
    }).get()];
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

void WebExtensionTab::setZoomFactor(double zoomFactor, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"tabs.setZoom()";

    if (!isValid() || !m_respondsToSetZoomFactor) {
        mainWebView().pageZoom = zoomFactor;
        completionHandler({ });
        return;
    }

    [m_delegate setZoomFactor:zoomFactor forWebExtensionContext:m_extensionContext->wrapper() completionHandler:makeBlockPtr([protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](NSError *error) mutable {
        if (error) {
            RELEASE_LOG_ERROR(Extensions, "Error for setZoomFactor: %{public}@", privacyPreservingDescription(error));
            completionHandler(toWebExtensionError(apiName, nil, error.localizedDescription));
            return;
        }

        completionHandler({ });
    }).get()];
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

void WebExtensionTab::detectWebpageLocale(CompletionHandler<void(Expected<NSLocale *, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"tabs.detectLanguage()";

    if (!isValid() || !m_respondsToDetectWebpageLocale) {
        completionHandler(toWebExtensionError(apiName, nil, @"it is not implemented"));
        return;
    }

    [m_delegate detectWebpageLocaleForWebExtensionContext:m_extensionContext->wrapper() completionHandler:makeBlockPtr([protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](NSLocale *locale, NSError *error) mutable {
        if (error) {
            RELEASE_LOG_ERROR(Extensions, "Error for detectWebpageLocale: %{public}@", privacyPreservingDescription(error));
            completionHandler(toWebExtensionError(apiName, nil, error.localizedDescription));
            return;
        }

        THROW_UNLESS(!locale || [locale isKindOfClass:NSLocale.class], @"Object passed to completionHandler of detectWebpageLocaleForWebExtensionContext:completionHandler: is not an NSLocale");
        completionHandler(locale);
    }).get()];
}

void WebExtensionTab::captureVisibleWebpage(CompletionHandler<void(Expected<CocoaImage *, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"tabs.captureVisibleTab()";

    bool delegateIsUnavailable = !isValid() || !m_respondsToCaptureVisibleWebpage;
    WKWebView *mainWebView = delegateIsUnavailable ? this->mainWebView() : nil;

    if (delegateIsUnavailable && !mainWebView) {
        completionHandler(toWebExtensionError(apiName, nil, @"capture is unavailable for this tab"));
        return;
    }

    auto internalCompletionHandler = makeBlockPtr([completionHandler = WTFMove(completionHandler)](CocoaImage *image, NSError *error) mutable {
        if (error) {
            RELEASE_LOG_ERROR(Extensions, "Error for captureVisibleWebpage: %{public}@", privacyPreservingDescription(error));
            completionHandler(toWebExtensionError(apiName, nil, error.localizedDescription));
            return;
        }

        THROW_UNLESS(!image || [image isKindOfClass:[CocoaImage class]], @"Object passed to completionHandler of captureVisibleWebpageForWebExtensionContext:completionHandler: is not an image");
        completionHandler(image);
    });

    if (delegateIsUnavailable) {
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

void WebExtensionTab::loadURL(URL url, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"tabs.update()";

    if (!isValid() || !m_respondsToLoadURL) {
        [mainWebView() loadRequest:[NSURLRequest requestWithURL:url]];
        completionHandler({ });
        return;
    }

    [m_delegate loadURL:url forWebExtensionContext:m_extensionContext->wrapper() completionHandler:makeBlockPtr([protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](NSError *error) mutable {
        if (error) {
            RELEASE_LOG_ERROR(Extensions, "Error for loadURL: %{public}@", privacyPreservingDescription(error));
            completionHandler(toWebExtensionError(apiName, nil, error.localizedDescription));
            return;
        }

        completionHandler({ });
    }).get()];
}

void WebExtensionTab::reload(CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"tabs.reload()";

    if (!isValid() || !m_respondsToReload) {
        [mainWebView() reload];
        completionHandler({ });
        return;
    }

    [m_delegate reloadForWebExtensionContext:m_extensionContext->wrapper() completionHandler:makeBlockPtr([protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](NSError *error) mutable {
        if (error) {
            RELEASE_LOG_ERROR(Extensions, "Error for reload: %{public}@", privacyPreservingDescription(error));
            completionHandler(toWebExtensionError(apiName, nil, error.localizedDescription));
            return;
        }

        completionHandler({ });
    }).get()];
}

void WebExtensionTab::reloadFromOrigin(CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"tabs.reload()";

    if (!isValid() || !m_respondsToReloadFromOrigin) {
        [mainWebView() reloadFromOrigin];
        completionHandler({ });
        return;
    }

    [m_delegate reloadFromOriginForWebExtensionContext:m_extensionContext->wrapper() completionHandler:makeBlockPtr([protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](NSError *error) mutable {
        if (error) {
            RELEASE_LOG_ERROR(Extensions, "Error for reloadFromOrigin: %{public}@", privacyPreservingDescription(error));
            completionHandler(toWebExtensionError(apiName, nil, error.localizedDescription));
            return;
        }

        completionHandler({ });
    }).get()];
}

void WebExtensionTab::goBack(CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"tabs.goBack()";

    if (!isValid() || !m_respondsToGoBack) {
        [mainWebView() goBack];
        completionHandler({ });
        return;
    }

    [m_delegate goBackForWebExtensionContext:m_extensionContext->wrapper() completionHandler:makeBlockPtr([protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](NSError *error) mutable {
        if (error) {
            RELEASE_LOG_ERROR(Extensions, "Error for goBack: %{public}@", privacyPreservingDescription(error));
            completionHandler(toWebExtensionError(apiName, nil, error.localizedDescription));
            return;
        }

        completionHandler({ });
    }).get()];
}

void WebExtensionTab::goForward(CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"tabs.goForward()";

    if (!isValid() || !m_respondsToGoForward) {
        [mainWebView() goForward];
        completionHandler({ });
        return;
    }

    [m_delegate goForwardForWebExtensionContext:m_extensionContext->wrapper() completionHandler:makeBlockPtr([protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](NSError *error) mutable {
        if (error) {
            RELEASE_LOG_ERROR(Extensions, "Error for goForward: %{public}@", privacyPreservingDescription(error));
            completionHandler(toWebExtensionError(apiName, nil, error.localizedDescription));
            return;
        }

        completionHandler({ });
    }).get()];
}

void WebExtensionTab::activate(CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"tabs.update()";

    if (!isValid() || !m_respondsToActivate) {
        completionHandler(toWebExtensionError(apiName, nil, @"it is not implemented for 'active' set to `true`"));
        return;
    }

    [m_delegate activateForWebExtensionContext:m_extensionContext->wrapper() completionHandler:makeBlockPtr([protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](NSError *error) mutable {
        if (error) {
            RELEASE_LOG_ERROR(Extensions, "Error for activate: %{public}@", privacyPreservingDescription(error));
            completionHandler(toWebExtensionError(apiName, nil, error.localizedDescription));
            return;
        }

        completionHandler({ });
    }).get()];
}

void WebExtensionTab::select(CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"tabs.update()";

    if (!isValid() || !m_respondsToSelect) {
        completionHandler(toWebExtensionError(apiName, nil, @"it is not implemented for 'highlighted' or 'selected' set to `true`"));
        return;
    }

    [m_delegate selectForWebExtensionContext:m_extensionContext->wrapper() completionHandler:makeBlockPtr([protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](NSError *error) mutable {
        if (error) {
            RELEASE_LOG_ERROR(Extensions, "Error for select: %{public}@", privacyPreservingDescription(error));
            completionHandler(toWebExtensionError(apiName, nil, error.localizedDescription));
            return;
        }

        completionHandler({ });
    }).get()];
}

void WebExtensionTab::deselect(CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"tabs.update()";

    if (!isValid() || !m_respondsToDeselect) {
        completionHandler(toWebExtensionError(apiName, nil, @"it is not implemented for 'highlighted' or 'selected' set to `false`"));
        return;
    }

    [m_delegate deselectForWebExtensionContext:m_extensionContext->wrapper() completionHandler:makeBlockPtr([protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](NSError *error) mutable {
        if (error) {
            RELEASE_LOG_ERROR(Extensions, "Error for deselect: %{public}@", privacyPreservingDescription(error));
            completionHandler(toWebExtensionError(apiName, nil, error.localizedDescription));
            return;
        }

        completionHandler({ });
    }).get()];
}

void WebExtensionTab::duplicate(const WebExtensionTabParameters& parameters, CompletionHandler<void(Expected<RefPtr<WebExtensionTab>, WebExtensionError>&&)>&& completionHandler)
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

    static NSString * const apiName = @"tabs.duplicate()";

    if (!isValid() || !m_respondsToDuplicate) {
        completionHandler(toWebExtensionError(apiName, nil, @"it is not implemented"));
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

    [m_delegate duplicateForWebExtensionContext:m_extensionContext->wrapper() withOptions:duplicateOptions completionHandler:makeBlockPtr([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](id<_WKWebExtensionTab> duplicatedTab, NSError *error) mutable {
        if (error) {
            RELEASE_LOG_ERROR(Extensions, "Error for duplicate: %{public}@", privacyPreservingDescription(error));
            completionHandler(toWebExtensionError(apiName, nil, error.localizedDescription));
            return;
        }

        if (!duplicatedTab) {
            completionHandler({ });
            return;
        }

        THROW_UNLESS([duplicatedTab conformsToProtocol:@protocol(_WKWebExtensionTab)], @"Object passed to completionHandler of duplicateForWebExtensionContext:withOptions:completionHandler: does not conform to the _WKWebExtensionTab protocol");
        completionHandler(RefPtr { m_extensionContext->getOrCreateTab(duplicatedTab).ptr() });
    }).get()];
}

void WebExtensionTab::close(CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"tabs.remove()";

    if (!isValid() || !m_respondsToClose) {
        completionHandler(toWebExtensionError(apiName, nil, @"it is not implemented"));
        return;
    }

    [m_delegate closeForWebExtensionContext:m_extensionContext->wrapper() completionHandler:makeBlockPtr([protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](NSError *error) mutable {
        if (error) {
            RELEASE_LOG_ERROR(Extensions, "Error for tab close: %{public}@", privacyPreservingDescription(error));
            completionHandler(toWebExtensionError(apiName, nil, error.localizedDescription));
            return;
        }

        completionHandler({ });
    }).get()];
}

bool WebExtensionTab::shouldGrantTabPermissionsOnUserGesture() const
{
    if (!isValid() || !m_respondsToShouldGrantTabPermissionsOnUserGesture)
        return true;

    return [m_delegate shouldGrantTabPermissionsOnUserGestureForWebExtensionContext:m_extensionContext->wrapper()];
}

WebExtensionTab::WebProcessProxySet WebExtensionTab::processes(WebExtensionEventListenerType type, WebExtensionContentWorldType contentWorldType) const
{
    if (!isValid())
        return { };

    auto *webView = mainWebView();
    if (!webView)
        return { };

    if (!extensionContext()->pageListensForEvent(*webView._page, type, contentWorldType))
        return { };

    Ref process = webView._page->legacyMainFrameProcess();
    if (!process->canSendMessage())
        return { };

    return { WTFMove(process) };
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
