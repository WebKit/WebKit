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
#include "PageLoadState.h"

namespace WebKit {

PageLoadState::PageLoadState()
{
}

PageLoadState::~PageLoadState()
{
}

const String& PageLoadState::pendingAPIRequestURL() const
{
    return m_pendingAPIRequestURL;
}

void PageLoadState::setPendingAPIRequestURL(const String& pendingAPIRequestURL)
{
    m_pendingAPIRequestURL = pendingAPIRequestURL;
}

void PageLoadState::clearPendingAPIRequestURL()
{
    m_pendingAPIRequestURL = String();
}

void PageLoadState::didStartProvisionalLoad(const String& url, const String& unreachableURL)
{
    ASSERT(m_provisionalURL.isEmpty());

    m_provisionalURL = url;

    // FIXME: Do something with the unreachable URL.
}

void PageLoadState::didReceiveServerRedirectForProvisionalLoad(const String& url)
{
    m_provisionalURL = url;
}

void PageLoadState::didFailProvisionalLoad()
{
    m_provisionalURL = String();
}

void PageLoadState::didCommitLoad()
{
    ASSERT(!m_provisionalURL.isEmpty());

    m_url = m_provisionalURL;
    m_provisionalURL = String();
}

void PageLoadState::didFinishLoad()
{
    ASSERT(m_provisionalURL.isEmpty());
    ASSERT(!m_url.isEmpty());
}

void PageLoadState::didFailLoad()
{
    ASSERT(m_provisionalURL.isEmpty());
    ASSERT(!m_url.isEmpty());
}

void PageLoadState::didSameDocumentNavigation(const String& url)
{
    ASSERT(m_provisionalURL.isEmpty());
    ASSERT(!m_url.isEmpty());

    m_url = url;
}

} // namespace WebKit
