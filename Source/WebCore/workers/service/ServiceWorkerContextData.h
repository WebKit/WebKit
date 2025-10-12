/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include <WebCore/CertificateInfo.h>
#include <WebCore/ContentSecurityPolicyResponseHeaders.h>
#include <WebCore/CrossOriginEmbedderPolicy.h>
#include <WebCore/NavigationPreloadState.h>
#include <WebCore/ScriptBuffer.h>
#include <WebCore/ScriptExecutionContextIdentifier.h>
#include <WebCore/ServiceWorkerIdentifier.h>
#include <WebCore/ServiceWorkerImportedScript.h>
#include <WebCore/ServiceWorkerJobDataIdentifier.h>
#include <WebCore/ServiceWorkerRegistrationData.h>
#include <WebCore/WorkerType.h>
#include <wtf/RobinHoodHashMap.h>
#include <wtf/URLHash.h>

namespace WebCore {

enum class LastNavigationWasAppInitiated : bool;

struct ServiceWorkerContextData {
    std::optional<ServiceWorkerJobDataIdentifier> jobDataIdentifier;
    ServiceWorkerRegistrationData registration;
    ServiceWorkerIdentifier serviceWorkerIdentifier;
    ScriptBuffer script;
    CertificateInfo certificateInfo;
    ContentSecurityPolicyResponseHeaders contentSecurityPolicy;
    CrossOriginEmbedderPolicy crossOriginEmbedderPolicy;
    String referrerPolicy;
    URL scriptURL;
    WorkerType workerType;
    bool loadedFromDisk;
    std::optional<LastNavigationWasAppInitiated> lastNavigationWasAppInitiated;
    MemoryCompactRobinHoodHashMap<URL, ServiceWorkerImportedScript> scriptResourceMap;
    std::optional<ScriptExecutionContextIdentifier> serviceWorkerPageIdentifier;
    NavigationPreloadState navigationPreloadState;
    
    using ImportedScript = ServiceWorkerImportedScript;

    WEBCORE_EXPORT ServiceWorkerContextData isolatedCopy() const &;
    WEBCORE_EXPORT ServiceWorkerContextData isolatedCopy() &&;
};

} // namespace WebCore
