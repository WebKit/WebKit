/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "LegacyGlobalSettings.h"
#include "Logging.h"
#include "WebProcessPool.h"
#include <wtf/RAMSize.h>
#include <wtf/StdLibExtras.h>

#define WEBPROCESSCACHE_RELEASE_LOG(fmt, ...) RELEASE_LOG(ProcessSwapping, "%p - [PID=%d] WebProcessCache::" fmt, this, ##__VA_ARGS__)
#define WEBPROCESSCACHE_RELEASE_LOG_ERROR(fmt, ...) RELEASE_LOG_ERROR(ProcessSwapping, "%p - [PID=%d] WebProcessCache::" fmt, this, ##__VA_ARGS__)

namespace WebKit {

Seconds WebProcessCache::cachedProcessLifetime { 30_min };
Seconds WebProcessCache::clearingDelayAfterApplicationResignsActive { 5_min };
static Seconds cachedProcessSuspensionDelay { 30_s };

void WebProcessCache::setCachedProcessSuspensionDelayForTesting(Seconds delay)
{
    cachedProcessSuspensionDelay = delay;
}

static uint64_t generateAddRequestIdentifier()
{
    static uint64_t identifier = 0;
    return ++identifier;
}

WebProcessCache::WebProcessCache(WebProcessPool& processPool)
    : m_evictionTimer(RunLoop::main(), this, &WebProcessCache::clear)
{
    updateCapacity(processPool);
    platformInitialize();
}

bool WebProcessCache::canCacheProcess(WebProcessProxy& process) const
{
    if (!capacity()) {
        WEBPROCESSCACHE_RELEASE_LOG("canCacheProcess: Not caching process because the cache has no capacity", process.processIdentifier());
        return false;
    }

    if (process.registrableDomain().isEmpty()) {
        WEBPROCESSCACHE_RELEASE_LOG("canCacheProcess: Not caching process because it does not have an associated registrable domain", process.processIdentifier());
        return false;
    }

    if (MemoryPressureHandler::singleton().isUnderMemoryPressure()) {
        WEBPROCESSCACHE_RELEASE_LOG("canCacheProcess: Not caching process because we are under memory pressure", process.processIdentifier());
        return false;
    }

    if (!process.websiteDataStore()) {
        WEBPROCESSCACHE_RELEASE_LOG("canCacheProcess: Not caching process because this session has been destroyed", process.processIdentifier());
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
    Ref protectedProcessPool = process->processPool();
    uint64_t requestIdentifier = generateAddRequestIdentifier();
    m_pendingAddRequests.add(requestIdentifier, makeUnique<CachedProcess>(process.copyRef()));

    WEBPROCESSCACHE_RELEASE_LOG("addProcessIfPossible: Checking if process is responsive before caching it", process->processIdentifier());
    process->isResponsive([this, processPool = WTFMove(protectedProcessPool), process, requestIdentifier](bool isResponsive) {
        auto cachedProcess = m_pendingAddRequests.take(requestIdentifier);
        if (!cachedProcess)
            return;

        if (!isResponsive) {
            WEBPROCESSCACHE_RELEASE_LOG_ERROR("addProcessIfPossible(): Not caching process because it is not responsive", cachedProcess->process().processIdentifier());
            return;
        }
        processPool->webProcessCache().addProcess(WTFMove(cachedProcess));
    });
    return true;
}

bool WebProcessCache::addProcess(std::unique_ptr<CachedProcess>&& cachedProcess)
{
    ASSERT(!cachedProcess->process().pageCount());
    ASSERT(!cachedProcess->process().provisionalPageCount());
    ASSERT(!cachedProcess->process().suspendedPageCount());
    ASSERT(!cachedProcess->process().isRunningServiceWorkers());

    if (!canCacheProcess(cachedProcess->process()))
        return false;

    auto registrableDomain = cachedProcess->process().registrableDomain();
    RELEASE_ASSERT(!registrableDomain.isEmpty());

    if (auto previousProcess = m_processesPerRegistrableDomain.take(registrableDomain))
        WEBPROCESSCACHE_RELEASE_LOG("addProcess: Evicting process from WebProcess cache because a new process was added for the same domain", previousProcess->process().processIdentifier());

    while (m_processesPerRegistrableDomain.size() >= capacity()) {
        auto it = m_processesPerRegistrableDomain.random();
        WEBPROCESSCACHE_RELEASE_LOG("addProcess: Evicting process from WebProcess cache because capacity was reached", it->value->process().processIdentifier());
        m_processesPerRegistrableDomain.remove(it);
    }

#if PLATFORM(MAC) || PLATFORM(GTK) || PLATFORM(WPE)
    cachedProcess->startSuspensionTimer();
#endif

    WEBPROCESSCACHE_RELEASE_LOG("addProcess: Added process to WebProcess cache (size=%u, capacity=%u)", cachedProcess->process().processIdentifier(), size() + 1, capacity());
    m_processesPerRegistrableDomain.add(registrableDomain, WTFMove(cachedProcess));

    return true;
}

RefPtr<WebProcessProxy> WebProcessCache::takeProcess(const WebCore::RegistrableDomain& registrableDomain, WebsiteDataStore& dataStore, WebProcessProxy::LockdownMode lockdownMode)
{
    auto it = m_processesPerRegistrableDomain.find(registrableDomain);
    if (it == m_processesPerRegistrableDomain.end())
        return nullptr;

    if (it->value->process().websiteDataStore() != &dataStore)
        return nullptr;

    if (it->value->process().lockdownMode() != lockdownMode)
        return nullptr;

    auto process = it->value->takeProcess();
    m_processesPerRegistrableDomain.remove(it);
    WEBPROCESSCACHE_RELEASE_LOG("takeProcess: Taking process from WebProcess cache (size=%u, capacity=%u, processWasTerminated=%d)", process->processIdentifier(), size(), capacity(), process->wasTerminated());

    ASSERT(!process->pageCount());
    ASSERT(!process->provisionalPageCount());
    ASSERT(!process->suspendedPageCount());

    if (process->wasTerminated())
        return nullptr;

    return process;
}

void WebProcessCache::updateCapacity(WebProcessPool& processPool)
{
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
    } else {
        size_t memorySize = ramSize() / GB;
        if (memorySize < 3) {
            m_capacity = 0;
            WEBPROCESSCACHE_RELEASE_LOG("updateCapacity: Cache is disabled because device does not have enough RAM", 0);
        } else {
            // Allow 4 processes in the cache per GB of RAM, up to 30 processes.
            m_capacity = std::min<unsigned>(memorySize * 4, 30);
            WEBPROCESSCACHE_RELEASE_LOG("updateCapacity: Cache has a capacity of %u processes", 0, capacity());
        }
    }

    if (!m_capacity)
        clear();
}

void WebProcessCache::clear()
{
    if (m_pendingAddRequests.isEmpty() && m_processesPerRegistrableDomain.isEmpty())
        return;

    WEBPROCESSCACHE_RELEASE_LOG("clear: Evicting %u processes", 0, m_pendingAddRequests.size() + m_processesPerRegistrableDomain.size());
    m_pendingAddRequests.clear();
    m_processesPerRegistrableDomain.clear();
}

void WebProcessCache::clearAllProcessesForSession(PAL::SessionID sessionID)
{
    Vector<WebCore::RegistrableDomain> keysToRemove;
    for (auto& pair : m_processesPerRegistrableDomain) {
        auto* dataStore = pair.value->process().websiteDataStore();
        if (!dataStore || dataStore->sessionID() == sessionID) {
            WEBPROCESSCACHE_RELEASE_LOG("clearAllProcessesForSession: Evicting process because its session was destroyed", pair.value->process().processIdentifier());
            keysToRemove.append(pair.key);
        }
    }
    for (auto& key : keysToRemove)
        m_processesPerRegistrableDomain.remove(key);

    Vector<uint64_t> pendingRequestsToRemove;
    for (auto& pair : m_pendingAddRequests) {
        auto* dataStore = pair.value->process().websiteDataStore();
        if (!dataStore || dataStore->sessionID() == sessionID) {
            WEBPROCESSCACHE_RELEASE_LOG("clearAllProcessesForSession: Evicting process because its session was destroyed", pair.value->process().processIdentifier());
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
    else if (!m_processesPerRegistrableDomain.isEmpty())
        m_evictionTimer.startOneShot(clearingDelayAfterApplicationResignsActive);
}

void WebProcessCache::removeProcess(WebProcessProxy& process, ShouldShutDownProcess shouldShutDownProcess)
{
    RELEASE_ASSERT(!process.registrableDomain().isEmpty());
    WEBPROCESSCACHE_RELEASE_LOG("removeProcess: Evicting process from WebProcess cache because it expired", process.processIdentifier());

    std::unique_ptr<CachedProcess> cachedProcess;
    auto it = m_processesPerRegistrableDomain.find(process.registrableDomain());
    if (it != m_processesPerRegistrableDomain.end() && &it->value->process() == &process) {
        cachedProcess = WTFMove(it->value);
        m_processesPerRegistrableDomain.remove(it);
    } else {
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

WebProcessCache::CachedProcess::CachedProcess(Ref<WebProcessProxy>&& process)
    : m_process(WTFMove(process))
    , m_evictionTimer(RunLoop::main(), this, &CachedProcess::evictionTimerFired)
#if PLATFORM(MAC) || PLATFORM(GTK) || PLATFORM(WPE)
    , m_suspensionTimer(RunLoop::main(), this, &CachedProcess::suspensionTimerFired)
#endif
{
    RELEASE_ASSERT(!m_process->pageCount());
    auto* dataStore = m_process->websiteDataStore();
    RELEASE_ASSERT_WITH_MESSAGE(dataStore && !dataStore->processes().contains(*m_process), "Only processes with pages should be registered with the data store");
    m_process->setIsInProcessCache(true);
    m_evictionTimer.startOneShot(cachedProcessLifetime);
}

WebProcessCache::CachedProcess::~CachedProcess()
{
    if (!m_process)
        return;

    ASSERT(!m_process->pageCount());
    ASSERT(!m_process->provisionalPageCount());
    ASSERT(!m_process->suspendedPageCount());

#if PLATFORM(MAC) || PLATFORM(GTK) || PLATFORM(WPE)
    if (isSuspended())
        m_process->platformResumeProcess();
#endif
    m_process->setIsInProcessCache(false, WebProcessProxy::WillShutDown::Yes);
    m_process->shutDown();
}

Ref<WebProcessProxy> WebProcessCache::CachedProcess::takeProcess()
{
    ASSERT(m_process);
    m_evictionTimer.stop();
#if PLATFORM(MAC) || PLATFORM(GTK) || PLATFORM(WPE)
    if (isSuspended())
        m_process->platformResumeProcess();
    else
        m_suspensionTimer.stop();
#endif
    m_process->setIsInProcessCache(false);
    return m_process.releaseNonNull();
}

void WebProcessCache::CachedProcess::evictionTimerFired()
{
    ASSERT(m_process);
    m_process->processPool().webProcessCache().removeProcess(*m_process, ShouldShutDownProcess::Yes);
}

#if PLATFORM(MAC) || PLATFORM(GTK) || PLATFORM(WPE)
void WebProcessCache::CachedProcess::startSuspensionTimer()
{
    m_suspensionTimer.startOneShot(cachedProcessSuspensionDelay);
}

void WebProcessCache::CachedProcess::suspensionTimerFired()
{
    ASSERT(m_process);
    m_process->platformSuspendProcess();
}
#endif

#if !PLATFORM(COCOA)
void WebProcessCache::platformInitialize()
{
}
#endif

} // namespace WebKit
