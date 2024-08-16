/*
 * Copyright (C) 2015-2021 Apple Inc. All rights reserved.
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
#include "APINavigation.h"

#include "ProvisionalFrameProxy.h"
#include "WebBackForwardListItem.h"
#include <WebCore/RegistrableDomain.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#include <wtf/DebugUtilities.h>
#include <wtf/HexNumber.h>
#include <wtf/text/MakeString.h>

namespace API {
using namespace WebCore;
using namespace WebKit;

static constexpr Seconds navigationActivityTimeout { 30_s };

SubstituteData::SubstituteData(Vector<uint8_t>&& content, const ResourceResponse& response, WebCore::SubstituteData::SessionHistoryVisibility sessionHistoryVisibility)
    : SubstituteData(WTFMove(content), response.mimeType(), response.textEncodingName(), response.url().string(), nullptr, sessionHistoryVisibility)
{
}


Navigation::Navigation(WebCore::ProcessIdentifier processID)
    : m_navigationID(WebCore::NavigationIdentifier::generate())
    , m_processID(processID)
    , m_clientNavigationActivity(navigationActivityTimeout)
{
}

Navigation::Navigation(WebCore::ProcessIdentifier processID, RefPtr<WebBackForwardListItem>&& currentAndTargetItem)
    : m_navigationID(WebCore::NavigationIdentifier::generate())
    , m_processID(processID)
    , m_reloadItem(WTFMove(currentAndTargetItem))
    , m_clientNavigationActivity(navigationActivityTimeout)
{
}

Navigation::Navigation(WebCore::ProcessIdentifier processID, WebCore::ResourceRequest&& request, RefPtr<WebBackForwardListItem>&& fromItem)
    : m_navigationID(WebCore::NavigationIdentifier::generate())
    , m_processID(processID)
    , m_originalRequest(WTFMove(request))
    , m_currentRequest(m_originalRequest)
    , m_redirectChain { m_originalRequest.url() }
    , m_fromItem(WTFMove(fromItem))
    , m_clientNavigationActivity(navigationActivityTimeout)
{
}

Navigation::Navigation(WebCore::ProcessIdentifier processID, Ref<WebBackForwardListItem>&& targetItem, RefPtr<WebBackForwardListItem>&& fromItem, FrameLoadType backForwardFrameLoadType)
    : m_navigationID(WebCore::NavigationIdentifier::generate())
    , m_processID(processID)
    , m_originalRequest(targetItem->url())
    , m_currentRequest(m_originalRequest)
    , m_targetItem(WTFMove(targetItem))
    , m_fromItem(WTFMove(fromItem))
    , m_backForwardFrameLoadType(backForwardFrameLoadType)
    , m_clientNavigationActivity(navigationActivityTimeout)
{
}

Navigation::Navigation(WebCore::ProcessIdentifier processID, std::unique_ptr<SubstituteData>&& substituteData)
    : Navigation(processID)
{
    ASSERT(substituteData);
    m_substituteData = WTFMove(substituteData);
}

Navigation::Navigation(WebCore::ProcessIdentifier processID, WebCore::ResourceRequest&& simulatedRequest, std::unique_ptr<SubstituteData>&& substituteData, RefPtr<WebKit::WebBackForwardListItem>&& fromItem)
    : Navigation(processID, WTFMove(simulatedRequest), WTFMove(fromItem))
{
    ASSERT(substituteData);
    m_substituteData = WTFMove(substituteData);
}

Navigation::~Navigation()
{
}

void Navigation::setCurrentRequest(ResourceRequest&& request, ProcessIdentifier processIdentifier)
{
    m_currentRequest = WTFMove(request);
    m_currentRequestProcessIdentifier = processIdentifier;
}

void Navigation::appendRedirectionURL(const WTF::URL& url)
{
    if (m_redirectChain.isEmpty() || m_redirectChain.last() != url)
        m_redirectChain.append(url);
}

bool Navigation::currentRequestIsCrossSiteRedirect() const
{
    return currentRequestIsRedirect()
        && RegistrableDomain(m_lastNavigationAction.redirectResponse.url()) != RegistrableDomain(m_currentRequest.url());
}

#if !LOG_DISABLED

WTF::String Navigation::loggingString() const
{
    return makeString("Most recent URL: "_s, m_currentRequest.url().string(), " Back/forward list item URL: '"_s, m_targetItem ? m_targetItem->url() : WTF::String { }, "' (0x"_s, hex(reinterpret_cast<uintptr_t>(m_targetItem.get())), ')');
}

#endif

} // namespace API
