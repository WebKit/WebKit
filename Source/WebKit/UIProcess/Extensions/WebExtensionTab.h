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

#pragma once

#if ENABLE(WK_WEB_EXTENSIONS)

#include "CocoaImage.h"
#include "WebExtensionError.h"
#include "WebExtensionEventListenerType.h"
#include "WebExtensionTabIdentifier.h"
#include "WebPageProxyIdentifier.h"
#include <wtf/Forward.h>
#include <wtf/Identified.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakObjCPtr.h>

OBJC_CLASS NSArray;
OBJC_CLASS NSLocale;
OBJC_CLASS WKWebView;
OBJC_PROTOCOL(WKWebExtensionTab);

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

class WebExtensionTab : public RefCounted<WebExtensionTab>, public CanMakeWeakPtr<WebExtensionTab>, public Identified<WebExtensionTabIdentifier> {
    WTF_MAKE_NONCOPYABLE(WebExtensionTab);
    WTF_MAKE_TZONE_ALLOCATED(WebExtensionTab);

public:
    template<typename... Args>
    static Ref<WebExtensionTab> create(Args&&... args)
    {
        return adoptRef(*new WebExtensionTab(std::forward<Args>(args)...));
    }

    explicit WebExtensionTab(const WebExtensionContext&, WKWebExtensionTab *);

    enum class ChangedProperties : uint16_t {
        Loading      = 1 << 1,
        Muted        = 1 << 2,
        Pinned       = 1 << 3,
        PlayingAudio = 1 << 4,
        ReaderMode   = 1 << 5,
        Size         = 1 << 6,
        Title        = 1 << 7,
        URL          = 1 << 8,
        ZoomFactor   = 1 << 9,
    };

    static constexpr OptionSet<ChangedProperties> allChangedProperties()
    {
        return {
            ChangedProperties::Loading,
            ChangedProperties::Muted,
            ChangedProperties::Pinned,
            ChangedProperties::PlayingAudio,
            ChangedProperties::ReaderMode,
            ChangedProperties::Size,
            ChangedProperties::Title,
            ChangedProperties::URL,
            ChangedProperties::ZoomFactor,
        };
    }

    using ImageFormat = WebExtensionTabImageFormat;

    enum class AssumeWindowMatches : bool { No, Yes };
    enum class ReloadFromOrigin : bool { No, Yes };

    using WebProcessProxySet = HashSet<Ref<WebProcessProxy>>;

    WebExtensionTabParameters parameters() const;
    WebExtensionTabParameters changedParameters(OptionSet<ChangedProperties> = { }) const;

    WebExtensionContext* extensionContext() const;

    bool operator==(const WebExtensionTab&) const;

    bool matches(const WebExtensionTabQueryParameters&, AssumeWindowMatches = AssumeWindowMatches::No, std::optional<WebPageProxyIdentifier> = std::nullopt) const;

    bool extensionHasAccess() const;
    bool extensionHasPermission() const;
    bool extensionHasTemporaryPermission() const;

    bool hasActiveUserGesture() const { return m_activeUserGesture; }
    void setActiveUserGesture(bool activeUserGesture) { m_activeUserGesture = activeUserGesture; }

    RefPtr<WebExtensionMatchPattern> temporaryPermissionMatchPattern() const { return m_temporaryPermissionMatchPattern; }
    void setTemporaryPermissionMatchPattern(RefPtr<WebExtensionMatchPattern>&& matchPattern) { m_temporaryPermissionMatchPattern = WTFMove(matchPattern); }

    OptionSet<ChangedProperties> changedProperties() const { return m_changedProperties; }
    void addChangedProperties(OptionSet<ChangedProperties> properties) { m_changedProperties.add(properties); }
    void clearChangedProperties() { m_changedProperties = { }; }

    RefPtr<WebExtensionWindow> window() const;
    size_t index() const;

    RefPtr<WebExtensionTab> parentTab() const;
    void setParentTab(RefPtr<WebExtensionTab>, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&&);

    WKWebView *webView() const;

    String title() const;

    bool isOpen() const;
    void didOpen() { ASSERT(!m_isOpen); m_isOpen = true; }
    void didClose() { ASSERT(m_isOpen); m_isOpen = false; }

    bool isPrivate() const;

    bool isPinned() const;
    void setPinned(bool, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&&);

    void pin(CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler) { setPinned(true, WTFMove(completionHandler)); }
    void unpin(CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler) { setPinned(false, WTFMove(completionHandler)); }

    bool isReaderModeAvailable() const;

    bool isReaderModeActive() const;
    void setReaderModeActive(bool, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&&);

    void toggleReaderMode(CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler) { setReaderModeActive(!isReaderModeActive(), WTFMove(completionHandler)); }

    bool isPlayingAudio() const;

    bool isMuted() const;
    void setMuted(bool, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&&);

    void mute(CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler) { setMuted(true, WTFMove(completionHandler)); }
    void unmute(CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler) { setMuted(false, WTFMove(completionHandler)); }

    CGSize size() const;

    double zoomFactor() const;
    void setZoomFactor(double, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&&);

    URL url() const;
    URL pendingURL() const;

    bool isLoadingComplete() const;

    void detectWebpageLocale(CompletionHandler<void(Expected<NSLocale *, WebExtensionError>&&)>&&);
    void captureVisibleWebpage(CompletionHandler<void(Expected<CocoaImage *, WebExtensionError>&&)>&&);

    void loadURL(URL, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&&);

    void reload(ReloadFromOrigin, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&&);

    void goBack(CompletionHandler<void(Expected<void, WebExtensionError>&&)>&&);
    void goForward(CompletionHandler<void(Expected<void, WebExtensionError>&&)>&&);

    bool isActive() const;
    void activate(CompletionHandler<void(Expected<void, WebExtensionError>&&)>&&);

    bool isSelected() const;
    void setSelected(bool, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&&);

    void select(CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler) { setSelected(true, WTFMove(completionHandler)); }
    void deselect(CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler) { setSelected(false, WTFMove(completionHandler)); }

    void duplicate(const WebExtensionTabParameters&, CompletionHandler<void(Expected<RefPtr<WebExtensionTab>, WebExtensionError>&&)>&&);

    void close(CompletionHandler<void(Expected<void, WebExtensionError>&&)>&&);

    bool shouldGrantPermissionsOnUserGesture() const;

    WebProcessProxySet processes(WebExtensionEventListenerType, WebExtensionContentWorldType) const;

#ifdef __OBJC__
    WKWebExtensionTab *delegate() const { return m_delegate.getAutoreleased(); }

    bool isValid() const { return m_extensionContext && m_delegate; }
#endif

private:
    WeakPtr<WebExtensionContext> m_extensionContext;
    WeakObjCPtr<WKWebExtensionTab> m_delegate;
    RefPtr<WebExtensionMatchPattern> m_temporaryPermissionMatchPattern;
    OptionSet<ChangedProperties> m_changedProperties;
    bool m_activeUserGesture : 1 { false };
    bool m_isOpen : 1 { false };
    mutable bool m_private : 1 { false };
    mutable bool m_cachedPrivate : 1 { false };
    bool m_respondsToWindow : 1 { false };
    bool m_respondsToIndex : 1 { false };
    bool m_respondsToParentTab : 1 { false };
    bool m_respondsToSetParentTab : 1 { false };
    bool m_respondsToWebView : 1 { false };
    bool m_respondsToTitle : 1 { false };
    bool m_respondsToIsPinned : 1 { false };
    bool m_respondsToSetPinned : 1 { false };
    bool m_respondsToIsReaderModeAvailable : 1 { false };
    bool m_respondsToIsReaderModeActive : 1 { false };
    bool m_respondsToSetReaderModeActive : 1 { false };
    bool m_respondsToIsPlayingAudio : 1 { false };
    bool m_respondsToIsMuted : 1 { false };
    bool m_respondsToSetMuted : 1 { false };
    bool m_respondsToSize : 1 { false };
    bool m_respondsToZoomFactor : 1 { false };
    bool m_respondsToSetZoomFactor : 1 { false };
    bool m_respondsToURL : 1 { false };
    bool m_respondsToPendingURL : 1 { false };
    bool m_respondsToIsLoadingComplete : 1 { false };
    bool m_respondsToDetectWebpageLocale : 1 { false };
    bool m_respondsToTakeSnapshot : 1 { false };
    bool m_respondsToLoadURL : 1 { false };
    bool m_respondsToReload : 1 { false };
    bool m_respondsToGoBack : 1 { false };
    bool m_respondsToGoForward : 1 { false };
    bool m_respondsToActivate : 1 { false };
    bool m_respondsToIsSelected : 1 { false };
    bool m_respondsToSetSelected : 1 { false };
    bool m_respondsToDuplicate : 1 { false };
    bool m_respondsToClose : 1 { false };
    bool m_respondsToShouldGrantTabPermissionsOnUserGesture : 1 { false };
};

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
