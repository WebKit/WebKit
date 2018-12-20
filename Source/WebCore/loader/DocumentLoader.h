/*
 * Copyright (C) 2006-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CachedRawResourceClient.h"
#include "CachedResourceHandle.h"
#include "ContentSecurityPolicyClient.h"
#include "DocumentIdentifier.h"
#include "DocumentWriter.h"
#include "FrameDestructionObserver.h"
#include "LinkIcon.h"
#include "LoadTiming.h"
#include "NavigationAction.h"
#include "ResourceError.h"
#include "ResourceLoaderOptions.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "SecurityPolicyViolationEvent.h"
#include "ServiceWorkerRegistrationData.h"
#include "StringWithDirection.h"
#include "StyleSheetContents.h"
#include "SubstituteData.h"
#include "Timer.h"
#include <wtf/HashSet.h>
#include <wtf/OptionSet.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

#if ENABLE(APPLICATION_MANIFEST)
#include "ApplicationManifest.h"
#endif

#if HAVE(RUNLOOP_TIMER)
#include <wtf/RunLoopTimer.h>
#endif

#if PLATFORM(COCOA)
#include <wtf/SchedulePair.h>
#endif

namespace WebCore {

class ApplicationCacheHost;
class ApplicationManifestLoader;
class Archive;
class ArchiveResource;
class ArchiveResourceCollection;
class CachedRawResource;
class CachedResourceLoader;
class ContentFilter;
class FormState;
class Frame;
class FrameLoader;
class HTTPHeaderField;
class IconLoader;
class Page;
class PreviewConverter;
class ResourceLoader;
class SharedBuffer;
class SWClientConnection;
class SubresourceLoader;
class SubstituteResource;

enum class ShouldContinue;

using ResourceLoaderMap = HashMap<unsigned long, RefPtr<ResourceLoader>>;

enum class AutoplayPolicy {
    Default, // Uses policies specified in document settings.
    Allow,
    AllowWithoutSound,
    Deny,
};

enum class AutoplayQuirk {
    SynthesizedPauseEvents = 1 << 0,
    InheritedUserGestures = 1 << 1,
    ArbitraryUserGestures = 1 << 2,
};

enum class PopUpPolicy {
    Default, // Uses policies specified in frame settings.
    Allow,
    Block,
};

class DocumentLoader
    : public RefCounted<DocumentLoader>
    , public FrameDestructionObserver
    , public ContentSecurityPolicyClient
    , private CachedRawResourceClient {
    WTF_MAKE_FAST_ALLOCATED;
    friend class ContentFilter;
public:
    static Ref<DocumentLoader> create(const ResourceRequest& request, const SubstituteData& data)
    {
        return adoptRef(*new DocumentLoader(request, data));
    }
    WEBCORE_EXPORT virtual ~DocumentLoader();

    void attachToFrame(Frame&);

    WEBCORE_EXPORT virtual void detachFromFrame();

    WEBCORE_EXPORT FrameLoader* frameLoader() const;
    WEBCORE_EXPORT SubresourceLoader* mainResourceLoader() const;
    WEBCORE_EXPORT RefPtr<SharedBuffer> mainResourceData() const;
    
    DocumentWriter& writer() const { return m_writer; }

    const ResourceRequest& originalRequest() const;
    const ResourceRequest& originalRequestCopy() const;

    const ResourceRequest& request() const;
    ResourceRequest& request();

    CachedResourceLoader& cachedResourceLoader() { return m_cachedResourceLoader; }

    const SubstituteData& substituteData() const { return m_substituteData; }

    const URL& url() const;
    const URL& unreachableURL() const;

    const URL& originalURL() const;
    const URL& responseURL() const;
    const String& responseMIMEType() const;
#if PLATFORM(IOS_FAMILY)
    // FIXME: This method seems to violate the encapsulation of this class.
    WEBCORE_EXPORT void setResponseMIMEType(const String&);
#endif
    const String& currentContentType() const;
    void replaceRequestURLForSameDocumentNavigation(const URL&);
    bool isStopping() const { return m_isStopping; }
    void stopLoading();
    void setCommitted(bool committed) { m_committed = committed; }
    bool isCommitted() const { return m_committed; }
    WEBCORE_EXPORT bool isLoading() const;

    const ResourceError& mainDocumentError() const { return m_mainDocumentError; }

    const ResourceResponse& response() const { return m_response; }

    // FIXME: This method seems to violate the encapsulation of this class.
    void setResponse(const ResourceResponse& response) { m_response = response; }

    bool isClientRedirect() const { return m_isClientRedirect; }
    void setIsClientRedirect(bool isClientRedirect) { m_isClientRedirect = isClientRedirect; }
    void dispatchOnloadEvents();
    bool wasOnloadDispatched() { return m_wasOnloadDispatched; }
    WEBCORE_EXPORT bool isLoadingInAPISense() const;
    WEBCORE_EXPORT void setTitle(const StringWithDirection&);
    const String& overrideEncoding() const { return m_overrideEncoding; }

#if PLATFORM(COCOA)
    void schedule(SchedulePair&);
    void unschedule(SchedulePair&);
#endif

#if ENABLE(WEB_ARCHIVE) || ENABLE(MHTML)
    void setArchive(Ref<Archive>&&);
    WEBCORE_EXPORT void addAllArchiveResources(Archive&);
    WEBCORE_EXPORT void addArchiveResource(Ref<ArchiveResource>&&);
    RefPtr<Archive> popArchiveForSubframe(const String& frameName, const URL&);
    WEBCORE_EXPORT SharedBuffer* parsedArchiveData() const;

    WEBCORE_EXPORT bool scheduleArchiveLoad(ResourceLoader&, const ResourceRequest&);
#endif

    void scheduleSubstituteResourceLoad(ResourceLoader&, SubstituteResource&);
    void scheduleCannotShowURLError(ResourceLoader&);

    // Return the ArchiveResource for the URL only when loading an Archive
    WEBCORE_EXPORT ArchiveResource* archiveResourceForURL(const URL&) const;

    WEBCORE_EXPORT RefPtr<ArchiveResource> mainResource() const;

    // Return an ArchiveResource for the URL, either creating from live data or
    // pulling from the ArchiveResourceCollection.
    WEBCORE_EXPORT RefPtr<ArchiveResource> subresource(const URL&) const;

    WEBCORE_EXPORT Vector<Ref<ArchiveResource>> subresources() const;

#ifndef NDEBUG
    bool isSubstituteLoadPending(ResourceLoader*) const;
#endif
    void cancelPendingSubstituteLoad(ResourceLoader*);   
    
    void addResponse(const ResourceResponse&);
    const Vector<ResourceResponse>& responses() const { return m_responses; }

    const NavigationAction& triggeringAction() const { return m_triggeringAction; }
    void setTriggeringAction(NavigationAction&&);
    void setOverrideEncoding(const String& encoding) { m_overrideEncoding = encoding; }
    void setLastCheckedRequest(ResourceRequest&& request) { m_lastCheckedRequest = WTFMove(request); }
    const ResourceRequest& lastCheckedRequest()  { return m_lastCheckedRequest; }

    void stopRecordingResponses();
    const StringWithDirection& title() const { return m_pageTitle; }

    WEBCORE_EXPORT URL urlForHistory() const;
    WEBCORE_EXPORT bool urlForHistoryReflectsFailure() const;

    // These accessors accommodate WebCore's somewhat fickle custom of creating history
    // items for redirects, but only sometimes. For "source" and "destination",
    // these accessors return the URL that would have been used if a history
    // item were created. This allows WebKit to link history items reflecting
    // redirects into a chain from start to finish.
    String clientRedirectSourceForHistory() const { return m_clientRedirectSourceForHistory; } // null if no client redirect occurred.
    String clientRedirectDestinationForHistory() const { return urlForHistory(); }
    void setClientRedirectSourceForHistory(const String& clientRedirectSourceForHistory) { m_clientRedirectSourceForHistory = clientRedirectSourceForHistory; }
    
    String serverRedirectSourceForHistory() const { return (urlForHistory() == url() || url() == WTF::blankURL()) ? String() : urlForHistory().string(); } // null if no server redirect occurred.
    String serverRedirectDestinationForHistory() const { return url(); }

    bool didCreateGlobalHistoryEntry() const { return m_didCreateGlobalHistoryEntry; }
    void setDidCreateGlobalHistoryEntry(bool didCreateGlobalHistoryEntry) { m_didCreateGlobalHistoryEntry = didCreateGlobalHistoryEntry; }

    bool subresourceLoadersArePageCacheAcceptable() const { return m_subresourceLoadersArePageCacheAcceptable; }

    void setDefersLoading(bool);
    void setMainResourceDataBufferingPolicy(DataBufferingPolicy);

    void startLoadingMainResource();
    WEBCORE_EXPORT void cancelMainResourceLoad(const ResourceError&);
    void willContinueMainResourceLoadAfterRedirect(const ResourceRequest&);

    bool isLoadingMainResource() const { return m_loadingMainResource; }
    bool isLoadingMultipartContent() const { return m_isLoadingMultipartContent; }

    void stopLoadingPlugIns();
    void stopLoadingSubresources();
    WEBCORE_EXPORT void stopLoadingAfterXFrameOptionsOrContentSecurityPolicyDenied(unsigned long identifier, const ResourceResponse&);

    bool userContentExtensionsEnabled() const { return m_userContentExtensionsEnabled; }
    void setUserContentExtensionsEnabled(bool enabled) { m_userContentExtensionsEnabled = enabled; }

    AutoplayPolicy autoplayPolicy() const { return m_autoplayPolicy; }
    void setAutoplayPolicy(AutoplayPolicy policy) { m_autoplayPolicy = policy; }

    void setCustomUserAgent(const String& customUserAgent) { m_customUserAgent = customUserAgent; }
    const String& customUserAgent() const { return m_customUserAgent; }
        
    void setCustomNavigatorPlatform(const String& customNavigatorPlatform) { m_customNavigatorPlatform = customNavigatorPlatform; }
    const String& customNavigatorPlatform() const { return m_customNavigatorPlatform; }

    OptionSet<AutoplayQuirk> allowedAutoplayQuirks() const { return m_allowedAutoplayQuirks; }
    void setAllowedAutoplayQuirks(OptionSet<AutoplayQuirk> allowedQuirks) { m_allowedAutoplayQuirks = allowedQuirks; }

    PopUpPolicy popUpPolicy() const { return m_popUpPolicy; }
    void setPopUpPolicy(PopUpPolicy popUpPolicy) { m_popUpPolicy = popUpPolicy; }

    void addSubresourceLoader(ResourceLoader*);
    void removeSubresourceLoader(LoadCompletionType, ResourceLoader*);
    void addPlugInStreamLoader(ResourceLoader&);
    void removePlugInStreamLoader(ResourceLoader&);

    void subresourceLoaderFinishedLoadingOnePart(ResourceLoader*);

    void setDeferMainResourceDataLoad(bool defer) { m_deferMainResourceDataLoad = defer; }
    
    void didTellClientAboutLoad(const String& url);
    bool haveToldClientAboutLoad(const String& url) { return m_resourcesClientKnowsAbout.contains(url); }
    void recordMemoryCacheLoadForFutureClientNotification(const ResourceRequest&);
    void takeMemoryCacheLoadsForClientNotification(Vector<ResourceRequest>& loads);

    LoadTiming& timing() { return m_loadTiming; }
    void resetTiming() { m_loadTiming = LoadTiming(); }

    // The WebKit layer calls this function when it's ready for the data to actually be added to the document.
    WEBCORE_EXPORT void commitData(const char* bytes, size_t length);

    ApplicationCacheHost& applicationCacheHost() const;
    ApplicationCacheHost* applicationCacheHostUnlessBeingDestroyed() const;

    void checkLoadComplete();

    // The URL of the document resulting from this DocumentLoader.
    URL documentURL() const;

#if USE(QUICK_LOOK)
    void setPreviewConverter(std::unique_ptr<PreviewConverter>&&);
    PreviewConverter* previewConverter() const;
#endif

#if ENABLE(CONTENT_EXTENSIONS)
    void addPendingContentExtensionSheet(const String& identifier, StyleSheetContents&);
    void addPendingContentExtensionDisplayNoneSelector(const String& identifier, const String& selector, uint32_t selectorID);
#endif

    void setShouldOpenExternalURLsPolicy(ShouldOpenExternalURLsPolicy shouldOpenExternalURLsPolicy) { m_shouldOpenExternalURLsPolicy = shouldOpenExternalURLsPolicy; }
    ShouldOpenExternalURLsPolicy shouldOpenExternalURLsPolicyToPropagate() const;

#if ENABLE(CONTENT_FILTERING)
    ContentFilter* contentFilter() const;
#endif

    bool isAlwaysOnLoggingAllowed() const;

    void startIconLoading();
    WEBCORE_EXPORT void didGetLoadDecisionForIcon(bool decision, uint64_t loadIdentifier, uint64_t newCallbackID);
    void finishedLoadingIcon(IconLoader&, SharedBuffer*);

    const Vector<LinkIcon>& linkIcons() const { return m_linkIcons; }

#if ENABLE(APPLICATION_MANIFEST)
    WEBCORE_EXPORT uint64_t loadApplicationManifest();
    void finishedLoadingApplicationManifest(ApplicationManifestLoader&);
#endif

    WEBCORE_EXPORT void setCustomHeaderFields(Vector<HTTPHeaderField>&& fields);
    const Vector<HTTPHeaderField>& customHeaderFields() { return m_customHeaderFields; }
    
protected:
    WEBCORE_EXPORT DocumentLoader(const ResourceRequest&, const SubstituteData&);

    WEBCORE_EXPORT virtual void attachToFrame();

    bool m_deferMainResourceDataLoad { true };

private:
    Document* document() const;

#if ENABLE(SERVICE_WORKER)
    void matchRegistration(const URL&, CompletionHandler<void(Optional<ServiceWorkerRegistrationData>&&)>&&);
#endif
    void registerTemporaryServiceWorkerClient(const URL&);
    void unregisterTemporaryServiceWorkerClient();

    void loadMainResource(ResourceRequest&&);

    void setRequest(const ResourceRequest&);

    void commitIfReady();
    void setMainDocumentError(const ResourceError&);
    void commitLoad(const char*, int);
    void clearMainResourceLoader();

    void setupForReplace();
    void maybeFinishLoadingMultipartContent();
    
    bool maybeCreateArchive();
#if ENABLE(WEB_ARCHIVE) || ENABLE(MHTML)
    void clearArchiveResources();
#endif

    void willSendRequest(ResourceRequest&&, const ResourceResponse&, CompletionHandler<void(ResourceRequest&&)>&&);
    void finishedLoading();
    void mainReceivedError(const ResourceError&);
    WEBCORE_EXPORT void redirectReceived(CachedResource&, ResourceRequest&&, const ResourceResponse&, CompletionHandler<void(ResourceRequest&&)>&&) override;
    WEBCORE_EXPORT void responseReceived(CachedResource&, const ResourceResponse&, CompletionHandler<void()>&&) override;
    WEBCORE_EXPORT void dataReceived(CachedResource&, const char* data, int length) override;
    WEBCORE_EXPORT void notifyFinished(CachedResource&) override;

    void responseReceived(const ResourceResponse&, CompletionHandler<void()>&&);
    void dataReceived(const char* data, int length);

    bool maybeLoadEmpty();

    bool isMultipartReplacingLoad() const;
    bool isPostOrRedirectAfterPost(const ResourceRequest&, const ResourceResponse&);

    bool tryLoadingRequestFromApplicationCache();
    bool tryLoadingSubstituteData();
    bool tryLoadingRedirectRequestFromApplicationCache(const ResourceRequest&);
#if ENABLE(SERVICE_WORKER)
    void restartLoadingDueToServiceWorkerRegistrationChange(ResourceRequest&&, Optional<ServiceWorkerRegistrationData>&&);
#endif
    void continueAfterContentPolicy(PolicyAction);

    void stopLoadingForPolicyChange();
    ResourceError interruptedForPolicyChangeError() const;

#if HAVE(RUNLOOP_TIMER)
    typedef RunLoopTimer<DocumentLoader> DocumentLoaderTimer;
#else
    typedef Timer DocumentLoaderTimer;
#endif
    void handleSubstituteDataLoadNow();
    void startDataLoadTimer();

    void deliverSubstituteResourcesAfterDelay();
    void substituteResourceDeliveryTimerFired();

    void clearMainResource();

    void cancelPolicyCheckIfNeeded();
    void becomeMainResourceClient();

    void notifyFinishedLoadingIcon(uint64_t callbackIdentifier, SharedBuffer*);

#if ENABLE(APPLICATION_MANIFEST)
    void notifyFinishedLoadingApplicationManifest(uint64_t callbackIdentifier, Optional<ApplicationManifest>);
#endif

    // ContentSecurityPolicyClient
    WEBCORE_EXPORT void addConsoleMessage(MessageSource, MessageLevel, const String&, unsigned long requestIdentifier) final;
    WEBCORE_EXPORT void sendCSPViolationReport(URL&&, Ref<FormData>&&) final;
    WEBCORE_EXPORT void enqueueSecurityPolicyViolationEvent(SecurityPolicyViolationEvent::Init&&) final;

    bool disallowWebArchive() const;

    Ref<CachedResourceLoader> m_cachedResourceLoader;

    CachedResourceHandle<CachedRawResource> m_mainResource;
    ResourceLoaderMap m_subresourceLoaders;
    ResourceLoaderMap m_multipartSubresourceLoaders;
    ResourceLoaderMap m_plugInStreamLoaders;
    
    mutable DocumentWriter m_writer;

    // A reference to actual request used to create the data source.
    // This should only be used by the resourceLoadDelegate's
    // identifierForInitialRequest:fromDatasource: method. It is
    // not guaranteed to remain unchanged, as requests are mutable.
    ResourceRequest m_originalRequest;   

    SubstituteData m_substituteData;

    // A copy of the original request used to create the data source.
    // We have to copy the request because requests are mutable.
    ResourceRequest m_originalRequestCopy;
    
    // The 'working' request. It may be mutated
    // several times from the original request to include additional
    // headers, cookie information, canonicalization and redirects.
    ResourceRequest m_request;

    ResourceResponse m_response;

    ResourceError m_mainDocumentError;    

    bool m_originalSubstituteDataWasValid;
    bool m_committed { false };
    bool m_isStopping { false };
    bool m_gotFirstByte { false };
    bool m_isClientRedirect { false };
    bool m_isLoadingMultipartContent { false };

    // FIXME: Document::m_processingLoadEvent and DocumentLoader::m_wasOnloadDispatched are roughly the same
    // and should be merged.
    bool m_wasOnloadDispatched { false };

    StringWithDirection m_pageTitle;

    String m_overrideEncoding;

    // The action that triggered loading - we keep this around for the
    // benefit of the various policy handlers.
    NavigationAction m_triggeringAction;

    // The last request that we checked click policy for - kept around
    // so we can avoid asking again needlessly.
    ResourceRequest m_lastCheckedRequest;

    // We retain all the received responses so we can play back the
    // WebResourceLoadDelegate messages if the item is loaded from the
    // page cache.
    Vector<ResourceResponse> m_responses;
    bool m_stopRecordingResponses { false };
    
    typedef HashMap<RefPtr<ResourceLoader>, RefPtr<SubstituteResource>> SubstituteResourceMap;
    SubstituteResourceMap m_pendingSubstituteResources;
    Timer m_substituteResourceDeliveryTimer;

    std::unique_ptr<ArchiveResourceCollection> m_archiveResourceCollection;
#if ENABLE(WEB_ARCHIVE) || ENABLE(MHTML)
    RefPtr<Archive> m_archive;
    RefPtr<SharedBuffer> m_parsedArchiveData;
#endif

    HashSet<String> m_resourcesClientKnowsAbout;
    Vector<ResourceRequest> m_resourcesLoadedFromMemoryCacheForClientNotification;
    
    String m_clientRedirectSourceForHistory;
    bool m_didCreateGlobalHistoryEntry { false };

    bool m_loadingMainResource { false };
    LoadTiming m_loadTiming;

    MonotonicTime m_timeOfLastDataReceived;
    unsigned long m_identifierForLoadWithoutResourceLoader { 0 };

    DocumentLoaderTimer m_dataLoadTimer;
    bool m_waitingForContentPolicy { false };
    bool m_waitingForNavigationPolicy { false };

    HashMap<uint64_t, LinkIcon> m_iconsPendingLoadDecision;
    HashMap<std::unique_ptr<IconLoader>, uint64_t> m_iconLoaders;
    Vector<LinkIcon> m_linkIcons;

#if ENABLE(APPLICATION_MANIFEST)
    HashMap<std::unique_ptr<ApplicationManifestLoader>, uint64_t> m_applicationManifestLoaders;
#endif

    Vector<HTTPHeaderField> m_customHeaderFields;
    
    bool m_subresourceLoadersArePageCacheAcceptable { false };
    ShouldOpenExternalURLsPolicy m_shouldOpenExternalURLsPolicy { ShouldOpenExternalURLsPolicy::ShouldNotAllow };

    std::unique_ptr<ApplicationCacheHost> m_applicationCacheHost;

#if ENABLE(CONTENT_FILTERING)
    std::unique_ptr<ContentFilter> m_contentFilter;
#endif

#if USE(QUICK_LOOK)
    std::unique_ptr<PreviewConverter> m_previewConverter;
#endif

#if ENABLE(CONTENT_EXTENSIONS)
    HashMap<String, RefPtr<StyleSheetContents>> m_pendingNamedContentExtensionStyleSheets;
    HashMap<String, Vector<std::pair<String, uint32_t>>> m_pendingContentExtensionDisplayNoneSelectors;
#endif
    String m_customUserAgent;
    String m_customNavigatorPlatform;
    bool m_userContentExtensionsEnabled { true };
    AutoplayPolicy m_autoplayPolicy { AutoplayPolicy::Default };
    OptionSet<AutoplayQuirk> m_allowedAutoplayQuirks;
    PopUpPolicy m_popUpPolicy { PopUpPolicy::Default };

#if ENABLE(SERVICE_WORKER)
    Optional<ServiceWorkerRegistrationData> m_serviceWorkerRegistrationData;
    struct TemporaryServiceWorkerClient {
        DocumentIdentifier documentIdentifier;
        Ref<SWClientConnection> serviceWorkerConnection;
    };
    Optional<TemporaryServiceWorkerClient> m_temporaryServiceWorkerClient;
#endif

#ifndef NDEBUG
    bool m_hasEverBeenAttached { false };
#endif
};

inline void DocumentLoader::recordMemoryCacheLoadForFutureClientNotification(const ResourceRequest& request)
{
    m_resourcesLoadedFromMemoryCacheForClientNotification.append(request);
}

inline void DocumentLoader::takeMemoryCacheLoadsForClientNotification(Vector<ResourceRequest>& loadsSet)
{
    loadsSet.swap(m_resourcesLoadedFromMemoryCacheForClientNotification);
    m_resourcesLoadedFromMemoryCacheForClientNotification.clear();
}

inline const ResourceRequest& DocumentLoader::originalRequest() const
{
    return m_originalRequest;
}

inline const ResourceRequest& DocumentLoader::originalRequestCopy() const
{
    return m_originalRequestCopy;
}

inline const ResourceRequest& DocumentLoader::request() const
{
    return m_request;
}

inline ResourceRequest& DocumentLoader::request()
{
    return m_request;
}

inline const URL& DocumentLoader::url() const
{
    return m_request.url();
}

inline const URL& DocumentLoader::originalURL() const
{
    return m_originalRequestCopy.url();
}

inline const URL& DocumentLoader::responseURL() const
{
    return m_response.url();
}

inline const String& DocumentLoader::responseMIMEType() const
{
    return m_response.mimeType();
}

inline const String& DocumentLoader::currentContentType() const
{
    return m_writer.mimeType();
}

inline const URL& DocumentLoader::unreachableURL() const
{
    return m_substituteData.failingURL();
}

inline ApplicationCacheHost& DocumentLoader::applicationCacheHost() const
{
    // For a short time while the document loader is being destroyed, m_applicationCacheHost is null.
    // It's not acceptable to call this function during that time.
    ASSERT(m_applicationCacheHost);
    return *m_applicationCacheHost;
}

inline ApplicationCacheHost* DocumentLoader::applicationCacheHostUnlessBeingDestroyed() const
{
    return m_applicationCacheHost.get();
}

#if ENABLE(CONTENT_FILTERING)

inline ContentFilter* DocumentLoader::contentFilter() const
{
    return m_contentFilter.get();
}

#endif

inline void DocumentLoader::didTellClientAboutLoad(const String& url)
{
#if !PLATFORM(COCOA)
    // Don't include data URLs here, as if a lot of data is loaded that way, we hold on to the (large) URL string for too long.
    if (protocolIs(url, "data"))
        return;
#endif
    if (!url.isEmpty())
        m_resourcesClientKnowsAbout.add(url);
}

}
