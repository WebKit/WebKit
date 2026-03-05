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

#import "APIContentRuleListStore.h"
#import "ScriptTrackingPrivacyFilter.h"
#import <wtf/CompletionHandler.h>
#import <wtf/ContinuousApproximateTime.h>
#import <wtf/Function.h>
#import <wtf/Ref.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>
#import <wtf/WeakHashSet.h>
#import <wtf/text/WTFString.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>

#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
#import <WebCore/LinkDecorationFilteringData.h>
#import <WebCore/OrganizationStorageAccessPromptQuirk.h>
#ifdef __OBJC__
#import <pal/spi/cocoa/WebPrivacySPI.h>
#endif
#endif

OBJC_CLASS WKWebPrivacyNotificationListener;
OBJC_CLASS NSURLSession;
OBJC_CLASS WKContentRuleList;

namespace WebCore {
class ResourceRequest;
enum class IsKnownCrossSiteTracker : bool;
};

namespace WebKit {

bool isTaintedScriptURLBlockable(const URL&);

#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)

enum class RestrictedOpenerType : uint8_t;

void configureForAdvancedPrivacyProtections(NSURLSession *);
bool isKnownTrackerAddressOrDomain(StringView host);
WebCore::IsKnownCrossSiteTracker isRequestToKnownCrossSiteTracker(const WebCore::ResourceRequest&);
bool isRequestBlockable(const WebCore::ResourceRequest&);
void requestLinkDecorationFilteringData(CompletionHandler<void(Vector<WebCore::LinkDecorationFilteringData>&&)>&&);

class ListDataObserver : public RefCountedAndCanMakeWeakPtr<ListDataObserver> {
public:
    static Ref<ListDataObserver> create(Function<void()>&& callback)
    {
        return adoptRef(*new ListDataObserver(WTF::move(callback)));
    }

    void invokeCallback() { m_callback(); }

private:
    explicit ListDataObserver(Function<void()>&& callback)
        : m_callback { WTF::move(callback) }
    {
    }

    Function<void()> m_callback;
};

class ListDataControllerBase : public RefCountedAndCanMakeWeakPtr<ListDataControllerBase> {
public:
    virtual ~ListDataControllerBase() = default;

    Ref<ListDataObserver> observeUpdates(Function<void()>&&);
    void initializeIfNeeded();

protected:
    virtual bool hasCachedListData() const = 0;
    virtual void didUpdateCachedListData() { }
    virtual void updateList(CompletionHandler<void()>&&) = 0;
    virtual unsigned resourceTypeValue() const = 0;

    RetainPtr<WKWebPrivacyNotificationListener> m_notificationListener;
    WeakHashSet<ListDataObserver> m_observers;
    bool m_wasInitialized { false };
};

template<typename DerivedType, typename BackingDataType>
class ListDataController : public ListDataControllerBase {
public:
    static DerivedType& sharedSingleton()
    {
        static MainRunLoopNeverDestroyed<DerivedType> sharedInstance;
        return sharedInstance.get();
    }

    void setCachedListDataForTesting(BackingDataType&& data)
    {
        m_wasInitialized = true;
        setCachedListData(WTF::move(data));
        // FIXME: This is a safer cpp false positive (rdar://161384112).
        SUPPRESS_FORWARD_DECL_ARG m_observers.forEach([](ListDataObserver& observer) {
            observer.invokeCallback();
        });
    }

    const BackingDataType& cachedListData() const LIFETIME_BOUND { return m_cachedListData; }

protected:
    friend class NeverDestroyed<DerivedType, MainRunLoopAccessTraits>;

    void setCachedListData(BackingDataType&& data)
    {
        m_cachedListData = WTF::move(data);
        didUpdateCachedListData();
    }

    bool hasCachedListData() const final { return !m_cachedListData.isEmpty(); }

    BackingDataType m_cachedListData;
};

class LinkDecorationFilteringController : public ListDataController<LinkDecorationFilteringController, Vector<WebCore::LinkDecorationFilteringData>> {
public:
    void updateList(CompletionHandler<void()>&&) final;

private:
    void didUpdateCachedListData() final { m_cachedListData.shrinkToFit(); }
    unsigned resourceTypeValue() const final;
};

class StorageAccessPromptQuirkController : public ListDataController<StorageAccessPromptQuirkController, Vector<WebCore::OrganizationStorageAccessPromptQuirk>> {
private:
    void updateList(CompletionHandler<void()>&&) final;
    void didUpdateCachedListData() final;
    unsigned resourceTypeValue() const final;
};

class StorageAccessUserAgentStringQuirkController : public ListDataController<StorageAccessUserAgentStringQuirkController, HashMap<WebCore::RegistrableDomain, String>> {
private:
    void updateList(CompletionHandler<void()>&&) final;
    unsigned resourceTypeValue() const final;
};

class ScriptTrackingPrivacyController : public ListDataController<ScriptTrackingPrivacyController, ScriptTrackingPrivacyRules> {
private:
    void updateList(CompletionHandler<void()>&&) final;
    void didUpdateCachedListData() final;
    unsigned resourceTypeValue() const final;
#ifdef __OBJC__
    // FIXME: Remove when WebPrivacyHelpersAdditions.mm no longer depends on it.
    WPResourceType NODELETE resourceType() const;
#endif
};

class RestrictedOpenerDomainsController {
public:
    static RestrictedOpenerDomainsController& singleton();

    RestrictedOpenerType lookup(const WebCore::RegistrableDomain&) const;

private:
    friend class NeverDestroyed<RestrictedOpenerDomainsController, MainRunLoopAccessTraits>;
    RestrictedOpenerDomainsController();
    void scheduleNextUpdate(ContinuousApproximateTime);
    void update();

    RetainPtr<WKWebPrivacyNotificationListener> m_notificationListener;
    HashMap<WebCore::RegistrableDomain, RestrictedOpenerType> m_restrictedOpenerTypes;
    ContinuousApproximateTime m_nextScheduledUpdateTime;
};

class ResourceMonitorURLsController {
public:
    static ResourceMonitorURLsController& singleton();

    void prepare(CompletionHandler<void(WKContentRuleList *, bool)>&&);
    void getSource(CompletionHandler<void(String&&)>&&);

    void setContentRuleListStore(API::ContentRuleListStore&);

private:
    friend class NeverDestroyed<ResourceMonitorURLsController, MainRunLoopAccessTraits>;
    ResourceMonitorURLsController() = default;

    RefPtr<API::ContentRuleListStore> m_contentRuleListStore;
};

#define HAVE_RESOURCE_MONITOR_URLS_GET_SOURCE 1

class ConsistentPrivacyQuirkController : public ListDataController<ConsistentPrivacyQuirkController, ScriptTrackingPrivacyRules> {
private:
    void updateList(CompletionHandler<void()>&&) final;
    void didUpdateCachedListData() final;
    unsigned resourceTypeValue() const final;
};

#endif // ENABLE(ADVANCED_PRIVACY_PROTECTIONS)

} // namespace WebKit
