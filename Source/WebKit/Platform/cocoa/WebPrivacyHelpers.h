/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#import <wtf/CompletionHandler.h>
#import <wtf/Function.h>
#import <wtf/Ref.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>
#import <wtf/WeakHashSet.h>
#import <wtf/text/WTFString.h>

#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
#import <WebCore/LinkDecorationFilteringData.h>
#import <WebCore/OrganizationStorageAccessPromptQuirk.h>
#endif

OBJC_CLASS WKWebPrivacyNotificationListener;
OBJC_CLASS NSURLSession;

namespace WebKit {

#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)

void configureForAdvancedPrivacyProtections(NSURLSession *);
void requestLinkDecorationFilteringData(CompletionHandler<void(Vector<WebCore::LinkDecorationFilteringData>&&)>&&);

class LinkDecorationFilteringDataObserver : public RefCounted<LinkDecorationFilteringDataObserver>, public CanMakeWeakPtr<LinkDecorationFilteringDataObserver> {
public:
    static Ref<LinkDecorationFilteringDataObserver> create(Function<void()>&& callback)
    {
        return adoptRef(*new LinkDecorationFilteringDataObserver(WTFMove(callback)));
    }

    ~LinkDecorationFilteringDataObserver() = default;

    void invokeCallback() { m_callback(); }

private:
    explicit LinkDecorationFilteringDataObserver(Function<void()>&& callback)
        : m_callback { WTFMove(callback) }
    {
    }

    Function<void()> m_callback;
};

class LinkDecorationFilteringController {
public:
    static LinkDecorationFilteringController& shared();

    const Vector<WebCore::LinkDecorationFilteringData>& cachedStrings() const { return m_cachedStrings; }
    void updateStrings(CompletionHandler<void()>&&);

    Ref<LinkDecorationFilteringDataObserver> observeUpdates(Function<void()>&&);

private:
    friend class NeverDestroyed<LinkDecorationFilteringController, MainThreadAccessTraits>;
    LinkDecorationFilteringController() = default;

    void setCachedStrings(Vector<WebCore::LinkDecorationFilteringData>&&);

    RetainPtr<WKWebPrivacyNotificationListener> m_notificationListener;
    Vector<WebCore::LinkDecorationFilteringData> m_cachedStrings;
    WeakHashSet<LinkDecorationFilteringDataObserver> m_observers;
};

class StorageAccessPromptQuirkObserver : public RefCounted<StorageAccessPromptQuirkObserver>, public CanMakeWeakPtr<StorageAccessPromptQuirkObserver> {
public:
    static Ref<StorageAccessPromptQuirkObserver> create(Function<void()>&& callback)
    {
        return adoptRef(*new StorageAccessPromptQuirkObserver(WTFMove(callback)));
    }

    ~StorageAccessPromptQuirkObserver() = default;

    void invokeCallback() { m_callback(); }

private:
    explicit StorageAccessPromptQuirkObserver(Function<void()>&& callback)
        : m_callback { WTFMove(callback) }
    {
    }

    Function<void()> m_callback;
};

class StorageAccessUserAgentStringQuirkObserver : public RefCounted<StorageAccessUserAgentStringQuirkObserver>, public CanMakeWeakPtr<StorageAccessUserAgentStringQuirkObserver> {
public:
    static Ref<StorageAccessUserAgentStringQuirkObserver> create(Function<void()>&& callback)
    {
        return adoptRef(*new StorageAccessUserAgentStringQuirkObserver(WTFMove(callback)));
    }

    ~StorageAccessUserAgentStringQuirkObserver() = default;

    void invokeCallback() { m_callback(); }

private:
    explicit StorageAccessUserAgentStringQuirkObserver(Function<void()>&& callback)
        : m_callback { WTFMove(callback) }
    {
    }

    Function<void()> m_callback;
};

class StorageAccessPromptQuirkController {
public:
    static StorageAccessPromptQuirkController& shared();

    const Vector<WebCore::OrganizationStorageAccessPromptQuirk>& cachedQuirks() const { return m_cachedQuirks; }
    void updateQuirks(CompletionHandler<void()>&&);
    void setCachedQuirksForTesting(Vector<WebCore::OrganizationStorageAccessPromptQuirk>&&);

    Ref<StorageAccessPromptQuirkObserver> observeUpdates(Function<void()>&&);

private:
    friend class NeverDestroyed<StorageAccessPromptQuirkController, MainThreadAccessTraits>;
    StorageAccessPromptQuirkController() = default;
    void setCachedQuirks(Vector<WebCore::OrganizationStorageAccessPromptQuirk>&&);

    RetainPtr<WKWebPrivacyNotificationListener> m_notificationListener;
    Vector<WebCore::OrganizationStorageAccessPromptQuirk> m_cachedQuirks;
    WeakHashSet<StorageAccessPromptQuirkObserver> m_observers;
};

class StorageAccessUserAgentStringQuirkController {
public:
    static StorageAccessUserAgentStringQuirkController& shared();

    const HashMap<WebCore::RegistrableDomain, String>& cachedQuirks() const { return m_cachedQuirks; }
    void updateQuirks(CompletionHandler<void()>&&);
    void setCachedQuirksForTesting(HashMap<WebCore::RegistrableDomain, String>&&);

    Ref<StorageAccessUserAgentStringQuirkObserver> observeUpdates(Function<void()>&&);

private:
    friend class NeverDestroyed<StorageAccessUserAgentStringQuirkController, MainThreadAccessTraits>;
    StorageAccessUserAgentStringQuirkController() = default;
    void setCachedQuirks(HashMap<WebCore::RegistrableDomain, String>&&);

    RetainPtr<WKWebPrivacyNotificationListener> m_notificationListener;
    HashMap<WebCore::RegistrableDomain, String> m_cachedQuirks;
    WeakHashSet<StorageAccessUserAgentStringQuirkObserver> m_observers;
};

void configureForAdvancedPrivacyProtections(NSURLSession *);

#endif // ENABLE(ADVANCED_PRIVACY_PROTECTIONS)

} // namespace WebKit
