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

#pragma once

#include "EnhancedSecurity.h"
#include <WebCore/RegistrableDomain.h>
#include <wtf/CompletionHandler.h>
#include <wtf/ContinuousApproximateTime.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/WorkQueue.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

class EnhancedSecuritySitesPersistence;

class EnhancedSecuritySitesHolder final : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<EnhancedSecuritySitesHolder, WTF::DestructionThread::MainRunLoop> {
public:
    static Ref<EnhancedSecuritySitesHolder> create(const String& databaseDirectoryPath);

    ~EnhancedSecuritySitesHolder();

    void fetchEnhancedSecurityOnlyDomains(CompletionHandler<void(HashSet<WebCore::RegistrableDomain>&&)>&&);
    void fetchAllEnhancedSecuritySites(CompletionHandler<void(HashSet<WebCore::RegistrableDomain>&&)>&&);

    void trackEnhancedSecurityForDomain(WebCore::RegistrableDomain&&, EnhancedSecurity);

    void deleteSites(Vector<WebCore::RegistrableDomain>&&, CompletionHandler<void()>&&);
    void deleteAllSites(CompletionHandler<void()>&&);

private:
    // Shared WorkQueue is used to prevent race condition when delete and create Persistence for same database file.
    static WorkQueue& sharedWorkQueueSingleton();

    explicit EnhancedSecuritySitesHolder(const String& databasePath);

    std::unique_ptr<EnhancedSecuritySitesPersistence> m_enhancedSecurityPersistence WTF_GUARDED_BY_CAPABILITY(sharedWorkQueueSingleton());
};

}
