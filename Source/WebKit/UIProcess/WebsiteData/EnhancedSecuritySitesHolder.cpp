/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "EnhancedSecuritySitesHolder.h"

#include "EnhancedSecuritySitesPersistence.h"
#include <WebCore/SQLiteDatabase.h>
#include <wtf/CrossThreadCopier.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Seconds.h>
#include <wtf/StdLibExtras.h>

namespace WebKit {

WorkQueue& EnhancedSecuritySitesHolder::sharedWorkQueueSingleton()
{
    static NeverDestroyed<Ref<WorkQueue>> workQueue = WorkQueue::create("EnhancedSecuritySitesHolder Work Queue"_s);
    return workQueue.get();
}

Ref<EnhancedSecuritySitesHolder> EnhancedSecuritySitesHolder::create(const String& databaseDirectoryPath)
{
    return adoptRef(*new EnhancedSecuritySitesHolder(databaseDirectoryPath));
}

EnhancedSecuritySitesHolder::EnhancedSecuritySitesHolder(const String& databaseDirectoryPath)
{
    ASSERT(isMainRunLoop());

    sharedWorkQueueSingleton().dispatch([weakThis = ThreadSafeWeakPtr { *this }, path = crossThreadCopy(databaseDirectoryPath)] mutable {
        assertIsCurrent(sharedWorkQueueSingleton());

        if (RefPtr protectedThis = weakThis.get())
            protectedThis->m_enhancedSecurityPersistence = makeUnique<EnhancedSecuritySitesPersistence>(WTF::move(path));
    });
}

EnhancedSecuritySitesHolder::~EnhancedSecuritySitesHolder()
{
    ASSERT(isMainRunLoop());

    sharedWorkQueueSingleton().dispatch([container = WTF::move(m_enhancedSecurityPersistence)] { });
}

void EnhancedSecuritySitesHolder::fetchEnhancedSecurityOnlyDomains(CompletionHandler<void(HashSet<WebCore::RegistrableDomain>&&)>&& completionHandler)
{
    ASSERT(isMainRunLoop());

    sharedWorkQueueSingleton().dispatch([weakThis = ThreadSafeWeakPtr { *this }, completionHandler = WTF::move(completionHandler)] mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis) {
            callOnMainRunLoop([completionHandler = WTF::move(completionHandler)] mutable {
                completionHandler({ });
            });
            return;
        }

        assertIsCurrent(sharedWorkQueueSingleton());
        auto enhancedSecuritySites = protectedThis->m_enhancedSecurityPersistence->enhancedSecurityOnlyDomains();

        callOnMainRunLoop([enhancedSecuritySites = crossThreadCopy(WTF::move(enhancedSecuritySites)), completionHandler = WTF::move(completionHandler)] mutable {
            completionHandler(WTF::move(enhancedSecuritySites));
        });
    });
}

void EnhancedSecuritySitesHolder::fetchAllEnhancedSecuritySites(CompletionHandler<void(HashSet<WebCore::RegistrableDomain>&&)>&& completionHandler)
{
    ASSERT(isMainRunLoop());

    sharedWorkQueueSingleton().dispatch([weakThis = ThreadSafeWeakPtr { *this }, completionHandler = WTF::move(completionHandler)] mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis) {
            callOnMainRunLoop([completionHandler = WTF::move(completionHandler)] mutable {
                completionHandler({ });
            });
            return;
        }

        assertIsCurrent(sharedWorkQueueSingleton());
        auto enhancedSecuritySites = protectedThis->m_enhancedSecurityPersistence->allEnhancedSecuritySites();

        callOnMainRunLoop([enhancedSecuritySites = crossThreadCopy(WTF::move(enhancedSecuritySites)), completionHandler = WTF::move(completionHandler)] mutable {
            completionHandler(WTF::move(enhancedSecuritySites));
        });
    });
}

void EnhancedSecuritySitesHolder::trackEnhancedSecurityForDomain(WebCore::RegistrableDomain&& domain, EnhancedSecurity reason)
{
    ASSERT(isMainRunLoop());

    if (domain.isEmpty())
        return;

    sharedWorkQueueSingleton().dispatch([weakThis = ThreadSafeWeakPtr { *this }, domain = crossThreadCopy(WTF::move(domain)), reason]() mutable {
        assertIsCurrent(sharedWorkQueueSingleton());

        if (RefPtr protectedThis = weakThis.get())
            protectedThis->m_enhancedSecurityPersistence->trackEnhancedSecurityForDomain(WTF::move(domain), reason);
    });
}

void EnhancedSecuritySitesHolder::deleteSites(Vector<WebCore::RegistrableDomain>&& sites, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(isMainRunLoop());

    if (sites.isEmpty())
        return completionHandler();

    sharedWorkQueueSingleton().dispatch([weakThis = ThreadSafeWeakPtr { *this }, sites = crossThreadCopy(WTF::move(sites)), completionHandler = WTF::move(completionHandler)]() mutable {
        assertIsCurrent(sharedWorkQueueSingleton());

        if (RefPtr protectedThis = weakThis.get())
            protectedThis->m_enhancedSecurityPersistence->deleteSites(sites);

        callOnMainRunLoop(WTF::move(completionHandler));
    });
}

void EnhancedSecuritySitesHolder::deleteAllSites(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(isMainRunLoop());

    sharedWorkQueueSingleton().dispatch([weakThis = ThreadSafeWeakPtr { *this }, completionHandler = WTF::move(completionHandler)] mutable {
        assertIsCurrent(sharedWorkQueueSingleton());

        if (RefPtr protectedThis = weakThis.get())
            protectedThis->m_enhancedSecurityPersistence->deleteAllSites();

        callOnMainRunLoop(WTF::move(completionHandler));
    });
}

} // namespace WebKit
