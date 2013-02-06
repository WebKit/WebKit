/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "SchedulableLoader.h"

#if ENABLE(NETWORK_PROCESS)

namespace WebKit {

SchedulableLoader::SchedulableLoader(const NetworkResourceLoadParameters& parameters, NetworkConnectionToWebProcess* connection)
    : m_identifier(parameters.identifier())
    , m_webPageID(parameters.webPageID())
    , m_webFrameID(parameters.webFrameID())
    , m_request(parameters.request())
    , m_priority(parameters.priority())
    , m_contentSniffingPolicy(parameters.contentSniffingPolicy())
    , m_allowStoredCredentials(parameters.allowStoredCredentials())
    , m_inPrivateBrowsingMode(parameters.inPrivateBrowsingMode())
    , m_connection(connection)
    , m_shouldClearReferrerOnHTTPSToHTTPRedirect(parameters.shouldClearReferrerOnHTTPSToHTTPRedirect())
{
    for (size_t i = 0, count = parameters.requestBodySandboxExtensions().size(); i < count; ++i) {
        if (RefPtr<SandboxExtension> extension = SandboxExtension::create(parameters.requestBodySandboxExtensions()[i]))
            m_requestBodySandboxExtensions.append(extension);
    }
    m_resourceSandboxExtension = SandboxExtension::create(parameters.resourceSandboxExtension());
}

SchedulableLoader::~SchedulableLoader()
{
    ASSERT(!m_hostRecord);
}

void SchedulableLoader::connectionToWebProcessDidClose()
{
    m_connection = 0;

    // FIXME (NetworkProcess): Cancel the load. The request may be long-living, so we don't want it to linger around after all clients are gone.
}

void SchedulableLoader::consumeSandboxExtensions()
{
    for (size_t i = 0, count = m_requestBodySandboxExtensions.size(); i < count; ++i)
        m_requestBodySandboxExtensions[i]->consume();

    if (m_resourceSandboxExtension)
        m_resourceSandboxExtension->consume();
}

void SchedulableLoader::invalidateSandboxExtensions()
{
    for (size_t i = 0, count = m_requestBodySandboxExtensions.size(); i < count; ++i)
        m_requestBodySandboxExtensions[i]->invalidate();

    if (m_resourceSandboxExtension)
        m_resourceSandboxExtension->invalidate();
}

} // namespace WebKit

#endif // ENABLE(NETWORK_PROCESS)
