/*
 * Copyright (C) 2019-2025 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WebProcessCache.h"

#include "APIPageConfiguration.h"
#include "EnhancedSecurity.h"
#include "LegacyGlobalSettings.h"
#include "Logging.h"
#include "ProcessThrottler.h"
#include "WebProcessPool.h"
#include <wtf/RAMSize.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>

#define WEBPROCESSCACHE_RELEASE_LOG(fmt, ...) RELEASE_LOG(ProcessSwapping, "%p - [PID=%d] WebProcessCache::" fmt, this, ##__VA_ARGS__)
#define WEBPROCESSCACHE_RELEASE_LOG_ERROR(fmt, ...) RELEASE_LOG_ERROR(ProcessSwapping, "%p - [PID=%d] WebProcessCache::" fmt, this, ##__VA_ARGS__)

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebProcessCache);
WTF_MAKE_TZONE_ALLOCATED_IMPL(WebProcessCache::CachedProcess);

#if PLATFORM(COCOA)
Seconds WebProcessCache::cachedProcessLifetime { 30_min };
Seconds WebProcessCache::clearingDelayAfterApplicationResignsActive { 5_min };
#else
Seconds WebProcessCache::cachedProcessLifetime { 5_min };
Seconds WebProcessCache::clearingDelayAfterApplicationResignsActive = cachedProcessLifetime;
#endif

int WebProcessCache::capacityOverride { -1 };
static Seconds cachedProcessSuspensionDelay { 30_s };

void WebProcessCache::setCachedProcessSuspensionDelayForTesting(Seconds delay)
{
    cachedProcessSuspensionDelay = delay;
}

void WebProcessCache::setCachedProcessLifetimeForTesting(Seconds lifetime)
{
    m_cachedProcessLifetime = lifetime;
}

static uint64_t generateAddRequestIdentifier()
{
    static uint64_t identifier = 0;
    return ++identifier;
}

WebProcessCache::WebProcessCache(WebProcessPool& processPool)
    : m_processPool(processPool)
    , m_evictionTimer(RunLoop::mainSingleton(), "WebProcessCache::EvictionTimer"_s, this, &WebProcessCache::clear)
    , m_cachedProcessLifetime(cachedProcessLifetime)
{
    updateCapacity(processPool);
    platformInitialize();
}

void WebProcessCache::ref() const
{
    m_processPool->ref();
}

void WebProcessCache::deref() const
{
    m_processPool->deref();
}

bool WebProcessCache::canCacheProcess(WebProcessProxy& process) const
{
    if (!capacity()) {
        WEBPROCESSCACHE_RELEASE_LOG("canCacheProcess: Not caching process because the cache has no capacity", process.processID());
        return false;
    }

    if (!process.isSharedProcess() && (!process.site() || process.site()->domain().isEmpty())) {
        WEBPROCESSCACHE_RELEASE_LOG("canCacheProcess: Not caching process because it does not have an associated registrable domain", process.processID());
        return false;
    }

    if (RefPtr websiteDataStore = process.websiteDataStore()) {
        // Network process might wait for this web process to exit before clearing data.
        if (websiteDataStore->isRemovingData()) {
            WEBPROCESSCACHE_RELEASE_LOG("canCacheProcess: Not caching process because its website data store is removing data", process.processID());
            return false;
        }
    }

    if (MemoryPressureHandler::singleton().isUnderMemoryPressure()) {
        WEBPROCESSCACHE_RELEASE_LOG("canCacheProcess: Not caching process because we are under memory pressure", process.processID());
        return false;
    }

    if (!process.websiteDataStore()) {
        WEBPROCESSCACHE_RELEASE_LOG("canCacheProcess: Not caching process because this session has been destroyed", process.processID());
        return false;
    }

    return true;
}

bool WebProcessCache::addProcessIfPossible(Ref<WebProcessProxy>&& process)
{
    ASSERT(!process->pageCount());
    ASSERT(!process->provisionalPageCount());
    ASSERT(!process->suspendedPageCount());
    ASSERT(!process->isRunningServiceWorkers());

    if (!canCacheProcess(process))
        return false;

    // CachedProcess can destroy the process pool (which owns the WebProcessCache), by making its reference weak in WebProcessProxy::setIsInProcessCache.
    uint64_t requestIdentifier = generateAddRequestIdentifier();
    m_pendingAddRequests.add(requestIdentifier, CachedProcess::create(process.copyRef(), m_cachedProcessLifetime));

    WEBPROCESSCACHE_RELEASE_LOG("addProcessIfPossible: Checking if process is responsive before caching it", process->processID());
    process->isResponsive([this, protectedThis = Ref { *this }, process, requestIdentifier](bool isResponsive) {
        auto cachedProcess = m_pendingAddRequests.take(requestIdentifier);
        if (!cachedProcess)
            return;

        if (!isResponsive) {
            WEBPROCESSCACHE_RELEASE_LOG_ERROR("addProcessIfPossible(): Not caching process because it is not responsive", cachedProcess->process().processID());
            return;
        }
        addProcess(cachedProcess.releaseNonNull());
    });
    return true;
}

bool WebProcessCache::addProcess(Ref<CachedProcess>&& cachedProcess)
{
    ASSERT(!cachedProcess->process().pageCount());
    ASSERT(!cachedProcess->process().provisionalPageCount());
    ASSERT(!cachedProcess->process().suspendedPageCount());
    ASSERT(!cachedProcess->process().isRunningServiceWorkers());

    Ref process = cachedProcess->process();
    if (!canCacheProcess(process))
        return false;

#if PLATFORM(COCOA) || PLATFORM(GTK) || PLATFORM(WPE)
        cachedProcess->startSuspensionTimer();
#endif

    if (process->isSharedProcess()) {
        auto site = process->sharedProcessMainFrameSite();
        ASSERT(site);
        if (auto previousProcess = m_sharedProcessesPerSite.take(*site))
            WEBPROCESSCACHE_RELEASE_LOG("addProcess: Evicting shared process from WebProcess cache because a new process was added for the same domain", previousProcess->process().processID());

        evictAtRandomIfNeeded();

        WEBPROCESSCACHE_RELEASE_LOG("addProcess: Added shared process to WebProcess cache (size=%u, capacity=%u) %" SENSITIVE_LOG_STRING, cachedProcess->process().processID(), size() + 1, capacity(), site->toString().utf8().data());
        m_sharedProcessesPerSite.add(*site, WTFMove(cachedProcess));

        return true;
    }

    RELEASE_ASSERT(process->site());
    RELEASE_ASSERT(!process->site()->isEmpty());
    auto site = *process->site();

    if (auto previousProcess = m_processesPerSite.take(site))
        WEBPROCESSCACHE_RELEASE_LOG("addProcess: Evicting process from WebProcess cache because a new process was added for the same domain", previousProcess->process().processID());

    evictAtRandomIfNeeded();

    WEBPROCESSCACHE_RELEASE_LOG("addProcess: Added process to WebProcess cache (size=%u, capacity=%u) %" SENSITIVE_LOG_STRING, cachedProcess->process().processID(), size() + 1, capacity(), site.toString().utf8().data());
    m_processesPerSite.add(site, WTFMove(cachedProcess));

    return true;
}

void WebProcessCache::evictAtRandomIfNeeded()
{
    while (size() >= capacity() && (!m_sharedProcessesPerSite.isEmpty() || !m_processesPerSite.isEmpty())) {
        if (!m_sharedProcessesPerSite.isEmpty()) {
            auto it = m_sharedProcessesPerSite.random();
            WEBPROCESSCACHE_RELEASE_LOG("addProcess: Evicting shared process from WebProcess cache because capacity was reached", it->value->process().processID());
            m_sharedProcessesPerSite.remove(it);
            if (size() < capacity())
                break;
        }
        if (!m_processesPerSite.isEmpty()) {
            auto it = m_processesPerSite.random();
            if (RefPtr sharedProcess = m_sharedProcessesPerSite.take(it->key)) {
                WEBPROCESSCACHE_RELEASE_LOG("addProcess: Evicting shared process from WebProcess cache because capacity was reached", sharedProcess->process().processID());
                if (size() < capacity())
                    break;
            }
            WEBPROCESSCACHE_RELEASE_LOG("addProcess: Evicting process from WebProcess cache because capacity was reached", it->value->process().processID());
            m_processesPerSite.remove(it);
        }
    }
}

RefPtr<WebProcessProxy> WebProcessCache::takeProcess(const WebCore::Site& site, WebsiteDataStore& dataStore, WebProcessProxy::LockdownMode lockdownMode, EnhancedSecurity enhancedSecurity, const API::PageConfiguration& pageConfiguration)
{
    auto it = m_processesPerSite.find(site);
    if (it == m_processesPerSite.end()) {
        WEBPROCESSCACHE_RELEASE_LOG("takeProcess: did not find %" SENSITIVE_LOG_STRING, 0, site.toString().utf8().data());
        return nullptr;
    }

    if (it->value->process().websiteDataStore() != &dataStore) {
        WEBPROCESSCACHE_RELEASE_LOG("takeProcess: cannot take process, datastore not identical", it->value->process().processID());
        return nullptr;
    }

    if (it->value->process().lockdownMode() != lockdownMode) {
        WEBPROCESSCACHE_RELEASE_LOG("takeProcess: cannot take process, lockdown mode not identical", it->value->process().processID());
        return nullptr;
    }

    if (it->value->process().enhancedSecurity() != enhancedSecurity) {
        WEBPROCESSCACHE_RELEASE_LOG("takeProcess: cannot take process, enhanced security not identical", it->value->process().processID());
        return nullptr;
    }

    if (!Ref { it->value->process() }->hasSameGPUAndNetworkProcessPreferencesAs(pageConfiguration)) {
        WEBPROCESSCACHE_RELEASE_LOG("takeProcess: cannot take process, preferences not identical", it->value->process().processID());
        return nullptr;
    }

    Ref process = m_processesPerSite.take(it)->takeProcess();
    WEBPROCESSCACHE_RELEASE_LOG("takeProcess: Taking process from WebProcess cache (size=%u, capacity=%u, processWasTerminated=%d) %" SENSITIVE_LOG_STRING, process->processID(), size(), capacity(), process->wasTerminated(), site.toString().utf8().data());

    ASSERT(!process->pageCount());
    ASSERT(!process->provisionalPageCount());
    ASSERT(!process->suspendedPageCount());

    if (process->wasTerminated()) {
        WEBPROCESSCACHE_RELEASE_LOG("takeProcess: cannot take process, was terminated", process->processID());
        return nullptr;
    }

    return process;
}

RefPtr<WebProcessProxy> WebProcessCache::takeSharedProcess(const WebCore::Site& mainFrameSite, WebsiteDataStore& dataStore, WebProcessProxy::LockdownMode lockdownMode, EnhancedSecurity enhancedSecurity, const API::PageConfiguration& pageConfiguration)
{
    auto it = m_sharedProcessesPerSite.find(mainFrameSite);
    if (it == m_sharedProcessesPerSite.end()) {
        WEBPROCESSCACHE_RELEASE_LOG("takeSharedProcess: did not find %" SENSITIVE_LOG_STRING, 0, mainFrameSite.toString().utf8().data());
        return nullptr;
    }

    if (it->value->process().websiteDataStore() != &dataStore) {
        WEBPROCESSCACHE_RELEASE_LOG("takeSharedProcess: cannot take process, datastore not identical", it->value->process().processID());
        return nullptr;
    }

    if (it->value->process().lockdownMode() != lockdownMode) {
        WEBPROCESSCACHE_RELEASE_LOG("takeSharedProcess: cannot take process, lockdown mode not identical", it->value->process().processID());
        return nullptr;
    }

    if (it->value->process().enhancedSecurity() != enhancedSecurity) {
        WEBPROCESSCACHE_RELEASE_LOG("takeSharedProcess: cannot take process, enhanced security not identical", it->value->process().processID());
        return nullptr;
    }

    if (!Ref { it->value->process() }->hasSameGPUAndNetworkProcessPreferencesAs(pageConfiguration)) {
        WEBPROCESSCACHE_RELEASE_LOG("takeSharedProcess: cannot take process, preferences not identical", it->value->process().processID());
        return nullptr;
    }

    Ref process = m_sharedProcessesPerSite.take(it)->takeProcess();
    WEBPROCESSCACHE_RELEASE_LOG("takeSharedProcess: Taking process from WebProcess cache (size=%u, capacity=%u, processWasTerminated=%d) %" SENSITIVE_LOG_STRING, process->processID(), size(), capacity(), process->wasTerminated(), mainFrameSite.toString().utf8().data());

    ASSERT(!process->pageCount());
    ASSERT(!process->provisionalPageCount());
    ASSERT(!process->suspendedPageCount());

    if (process->wasTerminated()) {
        WEBPROCESSCACHE_RELEASE_LOG("takeProcess: cannot take process, was terminated", process->processID());
        return nullptr;
    }

    return process;
}

void WebProcessCache::updateCapacity(WebProcessPool& processPool)
{
#if ENABLE(WEBPROCESS_CACHE)
    if (!processPool.configuration().processSwapsOnNavigation() || !processPool.configuration().usesWebProcessCache() || LegacyGlobalSettings::singleton().cacheModel() != CacheModel::PrimaryWebBrowser || processPool.configuration().usesSingleWebProcess()) {
        if (!processPool.configuration().processSwapsOnNavigation())
            WEBPROCESSCACHE_RELEASE_LOG("updateCapacity: Cache is disabled because process swap on navigation is disabled", 0);
        else if (!processPool.configuration().usesWebProcessCache())
            WEBPROCESSCACHE_RELEASE_LOG("updateCapacity: Cache is disabled by client", 0);
        else if (processPool.configuration().usesSingleWebProcess())
            WEBPROCESSCACHE_RELEASE_LOG("updateCapacity: Cache is disabled because process-per-tab was disabled", 0);
        else
            WEBPROCESSCACHE_RELEASE_LOG("updateCapacity: Cache is disabled because cache model is not PrimaryWebBrowser", 0);
        m_capacity = 0;
    } else if (capacityOverride >= 0) {
        m_capacity = capacityOverride;
    } else {
#if PLATFORM(IOS_FAMILY)
        constexpr unsigned maxProcesses = 10;
        size_t memorySize = WTF::ramSizeDisregardingJetsamLimit();
#else
        constexpr unsigned maxProcesses = 30;
        size_t memorySize = WTF::ramSize();
#endif
        WEBPROCESSCACHE_RELEASE_LOG("memory size %zu bytes", 0, memorySize);
        if (memorySize < 2 * GB) {
            m_capacity = 0;
            WEBPROCESSCACHE_RELEASE_LOG("updateCapacity: Cache is disabled because device does not have enough RAM", 0);
        } else {
            // Allow 4 processes in the cache per GB of RAM, up to maxProcesses.
            m_capacity = std::min<unsigned>(memorySize / (256 * MB), maxProcesses);
            WEBPROCESSCACHE_RELEASE_LOG("updateCapacity: Cache has a capacity of %u processes", 0, capacity());
        }
    }

    if (!m_capacity)
        clear();
#endif
}

void WebProcessCache::clear()
{
    if (m_pendingAddRequests.isEmpty() && m_processesPerSite.isEmpty() && m_sharedProcessesPerSite.isEmpty())
        return;

    WEBPROCESSCACHE_RELEASE_LOG("clear: Evicting %u processes", 0, m_pendingAddRequests.size() + m_processesPerSite.size());
    m_pendingAddRequests.clear();
    m_processesPerSite.clear();
    m_sharedProcessesPerSite.clear();
}

void WebProcessCache::clearAllProcessesForSession(PAL::SessionID sessionID)
{
    Vector<WebCore::Site> keysToRemove;
    for (auto& pair : m_processesPerSite) {
        RefPtr dataStore = pair.value->process().websiteDataStore();
        if (!dataStore || dataStore->sessionID() == sessionID) {
            WEBPROCESSCACHE_RELEASE_LOG("clearAllProcessesForSession: Evicting process because its session was destroyed", pair.value->process().processID());
            keysToRemove.append(pair.key);
        }
    }
    for (auto& key : keysToRemove)
        m_processesPerSite.remove(key);

    HashMap<WebCore::Site, Ref<CachedProcess>> sharedProcessesPerSite;
    for (auto& pair : m_sharedProcessesPerSite) {
        RefPtr dataStore = pair.value->process().websiteDataStore();
        if (!dataStore || dataStore->sessionID() == sessionID) {
            WEBPROCESSCACHE_RELEASE_LOG("clearAllProcessesForSession: Evicting shared process because its session was destroyed", pair.value->process().processID());
            continue;
        }
        sharedProcessesPerSite.add(pair.key, pair.value);
    }
    m_sharedProcessesPerSite = std::exchange(sharedProcessesPerSite, { });

    Vector<uint64_t> pendingRequestsToRemove;
    for (auto& pair : m_pendingAddRequests) {
        RefPtr dataStore = pair.value->process().websiteDataStore();
        if (!dataStore || dataStore->sessionID() == sessionID) {
            WEBPROCESSCACHE_RELEASE_LOG("clearAllProcessesForSession: Evicting process because its session was destroyed", pair.value->process().processID());
            pendingRequestsToRemove.append(pair.key);
        }
    }
    for (auto& key : pendingRequestsToRemove)
        m_pendingAddRequests.remove(key);
}

void WebProcessCache::setApplicationIsActive(bool isActive)
{
    WEBPROCESSCACHE_RELEASE_LOG("setApplicationIsActive: (isActive=%d)", 0, isActive);
    if (isActive)
        m_evictionTimer.stop();
    else if (size())
        m_evictionTimer.startOneShot(clearingDelayAfterApplicationResignsActive);
}

void WebProcessCache::removeProcess(WebProcessProxy& process, ShouldShutDownProcess shouldShutDownProcess)
{
    RELEASE_ASSERT(process.site() || process.isSharedProcess());
    WEBPROCESSCACHE_RELEASE_LOG("removeProcess: Evicting process from WebProcess cache because it expired", process.processID());

    RefPtr<CachedProcess> cachedProcess;

    if (auto expectedSite = process.site()) {
        auto it = m_processesPerSite.find(expectedSite.value());
        if (it != m_processesPerSite.end() && &it->value->process() == &process) {
            cachedProcess = WTFMove(it->value);
            m_processesPerSite.remove(it);
        }
    } else if (process.isSharedProcess()) {
        for (auto it = m_sharedProcessesPerSite.begin(); it != m_sharedProcessesPerSite.end(); ++it) {
            if (&it->value->process() == &process) {
                cachedProcess = WTFMove(it->value);
                m_sharedProcessesPerSite.remove(it);
                break;
            }
        }
    }

    if (!cachedProcess) {
        for (auto& pair : m_pendingAddRequests) {
            if (&pair.value->process() == &process) {
                cachedProcess = WTFMove(pair.value);
                m_pendingAddRequests.remove(pair.key);
                break;
            }
        }
    }

    ASSERT(cachedProcess);
    if (!cachedProcess)
        return;

    ASSERT(&cachedProcess->process() == &process);
    if (shouldShutDownProcess == ShouldShutDownProcess::No)
        cachedProcess->takeProcess();
}

Ref<WebProcessCache::CachedProcess> WebProcessCache::CachedProcess::create(Ref<WebProcessProxy>&& process, Seconds cachedProcessEvictionDelay)
{
    return adoptRef(*new WebProcessCache::CachedProcess(WTFMove(process), cachedProcessEvictionDelay));
}

WebProcessCache::CachedProcess::CachedProcess(Ref<WebProcessProxy>&& process, Seconds cachedProcessEvictionDelay)
    : m_process(WTFMove(process))
    , m_evictionTimer(RunLoop::mainSingleton(), "WebProcessCache::CachedProcess::EvictionTimer"_s, this, &CachedProcess::evictionTimerFired)
#if PLATFORM(COCOA) || PLATFORM(GTK) || PLATFORM(WPE)
    , m_suspensionTimer(RunLoop::mainSingleton(), "WebProcessCache::CachedProcess::SuspensionTimer"_s, this, &CachedProcess::suspensionTimerFired)
#endif
{
    RELEASE_ASSERT(!m_process->pageCount());
    RefPtr dataStore = m_process->websiteDataStore();
    RELEASE_ASSERT_WITH_MESSAGE(dataStore && !dataStore->processes().contains(*m_process), "Only processes with pages should be registered with the data store");
    protectedProcess()->setIsInProcessCache(true);
    m_evictionTimer.startOneShot(cachedProcessEvictionDelay);
}

WebProcessCache::CachedProcess::~CachedProcess()
{
    if (!m_process)
        return;

    ASSERT(!m_process->pageCount());
    ASSERT(!m_process->provisionalPageCount());
    ASSERT(!m_process->suspendedPageCount());

    RefPtr process = m_process;
#if PLATFORM(COCOA) || PLATFORM(GTK) || PLATFORM(WPE)
    if (isSuspended())
        process->platformResumeProcess();
#endif
    process->setIsInProcessCache(false, WebProcessProxy::WillShutDown::Yes);
    process->shutDown();
}

Ref<WebProcessProxy> WebProcessCache::CachedProcess::takeProcess()
{
    ASSERT(m_process);
    m_evictionTimer.stop();
    RefPtr process = m_process;
#if PLATFORM(COCOA) || PLATFORM(GTK) || PLATFORM(WPE)
    if (isSuspended())
        process->platformResumeProcess();
    else
        m_suspensionTimer.stop();

    // Dropping the background activity instantly might cause unnecessary process suspend/resume IPC
    // churn. This is because the background activity might be the last activity associated with the
    // process, so dropping it will cause a suspend IPC. Then we will almost always use the cached
    // process very soon after this call, causing a resume IPC.
    //
    // To avoid this, let the background activity live until the next runloop turn.
    if (m_backgroundActivity)
        RunLoop::currentSingleton().dispatch([backgroundActivity = WTFMove(m_backgroundActivity)]() { });
#endif
    process->setIsInProcessCache(false);
    return m_process.releaseNonNull();
}

void WebProcessCache::CachedProcess::evictionTimerFired()
{
    ASSERT(m_process);
    auto process = m_process.copyRef();
    process->protectedProcessPool()->webProcessCache().removeProcess(*process, ShouldShutDownProcess::Yes);
}

#if PLATFORM(COCOA) || PLATFORM(GTK) || PLATFORM(WPE)
void WebProcessCache::CachedProcess::startSuspensionTimer()
{
    ASSERT(m_process);

    // Allow the cached process to run for a while before dropping all assertions. This is useful
    // if the cached process will be reused fairly quickly after it goes into the cache, which
    // occurs in some benchmarks like PLT5.
    m_backgroundActivity = m_process->protectedThrottler()->backgroundActivity("Cached process near-suspended"_s);
    m_suspensionTimer.startOneShot(cachedProcessSuspensionDelay);
}

void WebProcessCache::CachedProcess::suspensionTimerFired()
{
    ASSERT(m_process);
    m_backgroundActivity = nullptr;
    protectedProcess()->platformSuspendProcess();
}
#endif

#if !PLATFORM(COCOA)
void WebProcessCache::platformInitialize()
{
}
#endif

} // namespace WebKit
