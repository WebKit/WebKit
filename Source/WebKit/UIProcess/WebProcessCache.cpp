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

#include "Logging.h"
#include "WebProcessPool.h"
#include "WebProcessProxy.h"
#include <wtf/RAMSize.h>
#include <wtf/StdLibExtras.h>

namespace WebKit {

Seconds WebProcessCache::cachedProcessLifetime { 30_min };
Seconds WebProcessCache::clearingDelayAfterApplicationResignsActive { 5_min };

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
    if (!capacity())
        return false;

    if (MemoryPressureHandler::singleton().isUnderMemoryPressure()) {
        RELEASE_LOG(ProcessSwapping, "%p - WebProcessCache::canCacheProcess(): Not caching process %i because we are under memory pressure", this, process.processIdentifier());
        return false;
    }

    auto sessionID = process.websiteDataStore().sessionID();
    if (sessionID != PAL::SessionID::defaultSessionID() && !process.processPool().hasPagesUsingWebsiteDataStore(process.websiteDataStore())) {
        RELEASE_LOG(ProcessSwapping, "%p - WebProcessCache::canCacheProcess(): Not caching process %i because this session has been destroyed", this, process.processIdentifier());
        return false;
    }

    return true;
}

bool WebProcessCache::addProcessIfPossible(const String& registrableDomain, Ref<WebProcessProxy>&& process)
{
    ASSERT(!registrableDomain.isEmpty());
    ASSERT(!process->pageCount());
    ASSERT(!process->provisionalPageCount());
    ASSERT(!process->suspendedPageCount());

    if (!canCacheProcess(process))
        return false;

    uint64_t requestIdentifier = generateAddRequestIdentifier();
    m_pendingAddRequests.add(requestIdentifier, std::make_unique<CachedProcess>(process.copyRef()));

    RELEASE_LOG(ProcessSwapping, "%p - WebProcessCache::addProcessIfPossible(): Checking if process %i is responsive before caching it...", this, process->processIdentifier());
    process->isResponsive([this, processPool = makeRef(process->processPool()), requestIdentifier](bool isResponsive) {
        auto cachedProcess = m_pendingAddRequests.take(requestIdentifier);
        if (!cachedProcess)
            return;

        if (!isResponsive) {
            RELEASE_LOG_ERROR(ProcessSwapping, "%p - WebProcessCache::addProcessIfPossible(): Not caching process %i because it is not responsive", &processPool->webProcessCache(), cachedProcess->process().processIdentifier());
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

    if (!canCacheProcess(cachedProcess->process()))
        return false;

    auto registrableDomain = cachedProcess->process().registrableDomain();
    RELEASE_ASSERT(!registrableDomain.isEmpty());

    if (auto previousProcess = m_processesPerRegistrableDomain.take(registrableDomain))
        RELEASE_LOG(ProcessSwapping, "%p - WebProcessCache::addProcess(): Evicting process %i from WebProcess cache because a new process was added for the same domain", this, previousProcess->process().processIdentifier());

    while (m_processesPerRegistrableDomain.size() >= capacity()) {
        auto it = m_processesPerRegistrableDomain.random();
        RELEASE_LOG(ProcessSwapping, "%p - WebProcessCache::addProcess(): Evicting process %i from WebProcess cache because capacity was reached", this, it->value->process().processIdentifier());
        m_processesPerRegistrableDomain.remove(it);
    }

    RELEASE_LOG(ProcessSwapping, "%p - WebProcessCache::addProcess: Added process %i to WebProcess cache, cache size: [%u / %u]", this, cachedProcess->process().processIdentifier(), size() + 1, capacity());
    m_processesPerRegistrableDomain.add(registrableDomain, WTFMove(cachedProcess));

    return true;
}

RefPtr<WebProcessProxy> WebProcessCache::takeProcess(const String& registrableDomain, WebsiteDataStore& dataStore)
{
    auto it = m_processesPerRegistrableDomain.find(registrableDomain);
    if (it == m_processesPerRegistrableDomain.end())
        return nullptr;

    if (&it->value->process().websiteDataStore() != &dataStore)
        return nullptr;

    auto process = it->value->takeProcess();
    m_processesPerRegistrableDomain.remove(it);
    RELEASE_LOG(ProcessSwapping, "%p - WebProcessCache::takeProcess: Taking process %i from WebProcess cache, cache size: [%u / %u]", this, process->processIdentifier(), size(), capacity());

    ASSERT(!process->pageCount());
    ASSERT(!process->provisionalPageCount());
    ASSERT(!process->suspendedPageCount());

    return WTFMove(process);
}

void WebProcessCache::updateCapacity(WebProcessPool& processPool)
{
    if (!processPool.configuration().processSwapsOnNavigation() || !processPool.configuration().usesWebProcessCache()) {
        if (!processPool.configuration().processSwapsOnNavigation())
            RELEASE_LOG(ProcessSwapping, "%p - WebProcessCache::updateCapacity: Cache is disabled because process swap on navigation is disabled", this);
        else
            RELEASE_LOG(ProcessSwapping, "%p - WebProcessCache::updateCapacity: Cache is disabled by client", this);
        m_capacity = 0;
    } else {
        size_t memorySize = ramSize() / GB;
        if (memorySize < 3) {
            m_capacity = 0;
            RELEASE_LOG(ProcessSwapping, "%p - WebProcessCache::updateCapacity: Cache is disabled because device does not have enough RAM", this);
        } else {
            // Allow 4 processes in the cache per GB of RAM, up to 30 processes.
            m_capacity = std::min<unsigned>(memorySize * 4, 30);
            RELEASE_LOG(ProcessSwapping, "%p - WebProcessCache::updateCapacity: Cache has a capacity of %u processes", this, capacity());
        }
    }

    if (!m_capacity)
        clear();
}

void WebProcessCache::clear()
{
    if (m_pendingAddRequests.isEmpty() && m_processesPerRegistrableDomain.isEmpty())
        return;

    RELEASE_LOG(ProcessSwapping, "%p - WebProcessCache::clear() evicting %u processes", this, m_pendingAddRequests.size() + m_processesPerRegistrableDomain.size());
    m_pendingAddRequests.clear();
    m_processesPerRegistrableDomain.clear();
}

void WebProcessCache::clearAllProcessesForSession(PAL::SessionID sessionID)
{
    Vector<String> keysToRemove;
    for (auto& pair : m_processesPerRegistrableDomain) {
        if (pair.value->process().websiteDataStore().sessionID() == sessionID) {
            RELEASE_LOG(ProcessSwapping, "%p - WebProcessCache::clearAllProcessesForSession() evicting process %i because its session was destroyed", this, pair.value->process().processIdentifier());
            keysToRemove.append(pair.key);
        }
    }
    for (auto& key : keysToRemove)
        m_processesPerRegistrableDomain.remove(key);

    Vector<uint64_t> pendingRequestsToRemove;
    for (auto& pair : m_pendingAddRequests) {
        if (pair.value->process().websiteDataStore().sessionID() == sessionID) {
            RELEASE_LOG(ProcessSwapping, "%p - WebProcessCache::clearAllProcessesForSession() evicting process %i because its session was destroyed", this, pair.value->process().processIdentifier());
            pendingRequestsToRemove.append(pair.key);
        }
    }
    for (auto& key : pendingRequestsToRemove)
        m_pendingAddRequests.remove(key);
}

void WebProcessCache::setApplicationIsActive(bool isActive)
{
    RELEASE_LOG(ProcessSwapping, "%p - WebProcessCache::setApplicationIsActive(%d)", this, isActive);
    if (isActive)
        m_evictionTimer.stop();
    else if (!m_processesPerRegistrableDomain.isEmpty())
        m_evictionTimer.startOneShot(clearingDelayAfterApplicationResignsActive);
}

void WebProcessCache::removeProcess(WebProcessProxy& process, ShouldShutDownProcess shouldShutDownProcess)
{
    RELEASE_ASSERT(!process.registrableDomain().isEmpty());
    RELEASE_LOG(ProcessSwapping, "%p - WebProcessCache::evictProcess(): Evicting process %i from WebProcess cache because it expired", this, process.processIdentifier());

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
{
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

    m_process->setIsInProcessCache(false);
    m_process->shutDown();
}

Ref<WebProcessProxy> WebProcessCache::CachedProcess::takeProcess()
{
    ASSERT(m_process);
    m_evictionTimer.stop();
    m_process->setIsInProcessCache(false);
    return m_process.releaseNonNull();
}

void WebProcessCache::CachedProcess::evictionTimerFired()
{
    ASSERT(m_process);
    m_process->processPool().webProcessCache().removeProcess(*m_process, ShouldShutDownProcess::Yes);
}

#if !PLATFORM(COCOA)
void WebProcessCache::platformInitialize()
{
}
#endif

} // namespace WebKit
