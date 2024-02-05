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

#pragma once

#if ENABLE(WK_WEB_EXTENSIONS)

#include "CocoaImage.h"
#include "WebExtensionEventListenerType.h"
#include "WebExtensionTabIdentifier.h"
#include "WebPageProxyIdentifier.h"
#include <wtf/Forward.h>
#include <wtf/WeakObjCPtr.h>

OBJC_CLASS NSArray;
OBJC_CLASS NSLocale;
OBJC_CLASS WKWebView;
OBJC_PROTOCOL(_WKWebExtensionTab);

namespace WebKit {

class WebExtensionContext;
class WebExtensionMatchPattern;
class WebExtensionWindow;
class WebProcessProxy;
struct WebExtensionTabParameters;
struct WebExtensionTabQueryParameters;

enum class WebExtensionTabImageFormat : uint8_t {
    PNG,
    JPEG,
};

class WebExtensionTab : public RefCounted<WebExtensionTab>, public CanMakeWeakPtr<WebExtensionTab> {
    WTF_MAKE_NONCOPYABLE(WebExtensionTab);
    WTF_MAKE_FAST_ALLOCATED;

public:
    template<typename... Args>
    static Ref<WebExtensionTab> create(Args&&... args)
    {
        return adoptRef(*new WebExtensionTab(std::forward<Args>(args)...));
    }

    explicit WebExtensionTab(const WebExtensionContext&, _WKWebExtensionTab *);

    enum class ChangedProperties : uint16_t {
        Audible    = 1 << 1,
        Loading    = 1 << 2,
        Muted      = 1 << 3,
        Pinned     = 1 << 4,
        ReaderMode = 1 << 5,
        Size       = 1 << 6,
        Title      = 1 << 7,
        URL        = 1 << 8,
        ZoomFactor = 1 << 9,
    };

    static constexpr OptionSet<ChangedProperties> allChangedProperties()
    {
        return {
            ChangedProperties::Audible,
            ChangedProperties::Loading,
            ChangedProperties::Muted,
            ChangedProperties::Pinned,
            ChangedProperties::ReaderMode,
            ChangedProperties::Size,
            ChangedProperties::Title,
            ChangedProperties::URL,
            ChangedProperties::ZoomFactor,
        };
    }

    using ImageFormat = WebExtensionTabImageFormat;

    enum class AssumeWindowMatches : bool { No, Yes };
    enum class MainWebViewOnly : bool { No, Yes };

    using Error = std::optional<String>;
    using WebProcessProxySet = HashSet<Ref<WebProcessProxy>>;

    WebExtensionTabIdentifier identifier() const { return m_identifier; }
    WebExtensionTabParameters parameters() const;
    WebExtensionTabParameters changedParameters(OptionSet<ChangedProperties> = { }) const;

    WebExtensionContext* extensionContext() const;

    bool operator==(const WebExtensionTab&) const;

    bool matches(const WebExtensionTabQueryParameters&, AssumeWindowMatches = AssumeWindowMatches::No, std::optional<WebPageProxyIdentifier> = std::nullopt) const;

    bool extensionHasAccess() const;
    bool extensionHasPermission() const;

    bool hasActiveUserGesture() { return m_activeUserGesture; }
    void setActiveUserGesture(bool activeUserGesture) { m_activeUserGesture = activeUserGesture; }

    RefPtr<WebExtensionMatchPattern> temporaryPermissionMatchPattern() { return m_temporaryPermissionMatchPattern; }
    void setTemporaryPermissionMatchPattern(RefPtr<WebExtensionMatchPattern>&& matchPattern) { m_temporaryPermissionMatchPattern = WTFMove(matchPattern); }

    OptionSet<ChangedProperties> changedProperties() const { return m_changedProperties; }
    void addChangedProperties(OptionSet<ChangedProperties> properties) { m_changedProperties.add(properties); }
    void clearChangedProperties() { m_changedProperties = { }; }

    RefPtr<WebExtensionWindow> window() const;
    size_t index() const;

    RefPtr<WebExtensionTab> parentTab() const;
    void setParentTab(RefPtr<WebExtensionTab>, CompletionHandler<void(Error)>&&);

    WKWebView *mainWebView() const;
    NSArray *webViews() const;

    String title() const;

    bool isOpen() const;
    void didOpen() { ASSERT(!m_isOpen); m_isOpen = true; }
    void didClose() { ASSERT(m_isOpen); m_isOpen = false; }

    bool isActive() const;
    bool isSelected() const;
    bool isPrivate() const;

    void pin(CompletionHandler<void(Error)>&&);
    void unpin(CompletionHandler<void(Error)>&&);

    bool isPinned() const;

    void toggleReaderMode(CompletionHandler<void(Error)>&&);

    bool isReaderModeAvailable() const;
    bool isShowingReaderMode() const;

    void mute(CompletionHandler<void(Error)>&&);
    void unmute(CompletionHandler<void(Error)>&&);

    bool isAudible() const;
    bool isMuted() const;

    CGSize size() const;

    double zoomFactor() const;
    void setZoomFactor(double, CompletionHandler<void(Error)>&&);

    URL url() const;
    URL pendingURL() const;

    bool isLoadingComplete() const;

    void detectWebpageLocale(CompletionHandler<void(NSLocale *, Error)>&&);
    void captureVisibleWebpage(CompletionHandler<void(CocoaImage *, Error)>&&);

    void loadURL(URL, CompletionHandler<void(Error)>&&);

    void reload(CompletionHandler<void(Error)>&&);
    void reloadFromOrigin(CompletionHandler<void(Error)>&&);

    void goBack(CompletionHandler<void(Error)>&&);
    void goForward(CompletionHandler<void(Error)>&&);

    void activate(CompletionHandler<void(Error)>&&);
    void select(CompletionHandler<void(Error)>&&);
    void deselect(CompletionHandler<void(Error)>&&);

    void duplicate(const WebExtensionTabParameters&, CompletionHandler<void(RefPtr<WebExtensionTab>, Error)>&&);

    void close(CompletionHandler<void(Error)>&&);

    bool shouldGrantTabPermissionsOnUserGesture() const;

    WebProcessProxySet processes(WebExtensionEventListenerType, WebExtensionContentWorldType, MainWebViewOnly = MainWebViewOnly::Yes) const;

#ifdef __OBJC__
    _WKWebExtensionTab *delegate() const { return m_delegate.getAutoreleased(); }

    bool isValid() const { return m_extensionContext && m_delegate; }
#endif

private:
    WebExtensionTabIdentifier m_identifier;
    WeakPtr<WebExtensionContext> m_extensionContext;
    WeakObjCPtr<_WKWebExtensionTab> m_delegate;
    RefPtr<WebExtensionMatchPattern> m_temporaryPermissionMatchPattern;
    OptionSet<ChangedProperties> m_changedProperties;
    bool m_activeUserGesture : 1 { false };
    bool m_isOpen : 1 { false };
    mutable bool m_private : 1 { false };
    mutable bool m_cachedPrivate : 1 { false };
    bool m_respondsToWindow : 1 { false };
    bool m_respondsToParentTab : 1 { false };
    bool m_respondsToSetParentTab : 1 { false };
    bool m_respondsToMainWebView : 1 { false };
    bool m_respondsToWebViews : 1 { false };
    bool m_respondsToTabTitle : 1 { false };
    bool m_respondsToIsSelected : 1 { false };
    bool m_respondsToIsPinned : 1 { false };
    bool m_respondsToPin : 1 { false };
    bool m_respondsToUnpin : 1 { false };
    bool m_respondsToIsReaderModeAvailable : 1 { false };
    bool m_respondsToIsShowingReaderMode : 1 { false };
    bool m_respondsToToggleReaderMode : 1 { false };
    bool m_respondsToIsAudible : 1 { false };
    bool m_respondsToIsMuted : 1 { false };
    bool m_respondsToMute : 1 { false };
    bool m_respondsToUnmute : 1 { false };
    bool m_respondsToSize : 1 { false };
    bool m_respondsToZoomFactor : 1 { false };
    bool m_respondsToSetZoomFactor : 1 { false };
    bool m_respondsToURL : 1 { false };
    bool m_respondsToPendingURL : 1 { false };
    bool m_respondsToIsLoadingComplete : 1 { false };
    bool m_respondsToDetectWebpageLocale : 1 { false };
    bool m_respondsToCaptureVisibleWebpage : 1 { false };
    bool m_respondsToLoadURL : 1 { false };
    bool m_respondsToReload : 1 { false };
    bool m_respondsToReloadFromOrigin : 1 { false };
    bool m_respondsToGoBack : 1 { false };
    bool m_respondsToGoForward : 1 { false };
    bool m_respondsToActivate : 1 { false };
    bool m_respondsToSelect : 1 { false };
    bool m_respondsToDeselect : 1 { false };
    bool m_respondsToDuplicate : 1 { false };
    bool m_respondsToClose : 1 { false };
    bool m_respondsToShouldGrantTabPermissionsOnUserGesture : 1 { false };
};

} // namespace WebKit

namespace WTF {

template<> struct EnumTraits<WebKit::WebExtensionTab::ChangedProperties> {
    using values = EnumValues<
        WebKit::WebExtensionTab::ChangedProperties,
        WebKit::WebExtensionTab::ChangedProperties::Audible,
        WebKit::WebExtensionTab::ChangedProperties::Loading,
        WebKit::WebExtensionTab::ChangedProperties::Muted,
        WebKit::WebExtensionTab::ChangedProperties::Pinned,
        WebKit::WebExtensionTab::ChangedProperties::ReaderMode,
        WebKit::WebExtensionTab::ChangedProperties::Size,
        WebKit::WebExtensionTab::ChangedProperties::Title,
        WebKit::WebExtensionTab::ChangedProperties::URL,
        WebKit::WebExtensionTab::ChangedProperties::ZoomFactor
    >;
};

} // namespace WTF

#endif // ENABLE(WK_WEB_EXTENSIONS)
