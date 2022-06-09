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

#include "WebBackForwardListItem.h"
#include "WebNavigationState.h"
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#include <wtf/DebugUtilities.h>
#include <wtf/HexNumber.h>

namespace API {
using namespace WebCore;
using namespace WebKit;

static constexpr Seconds navigationActivityTimeout { 30_s };

SubstituteData::SubstituteData(Vector<uint8_t>&& content, const ResourceResponse& response, WebCore::SubstituteData::SessionHistoryVisibility sessionHistoryVisibility)
    : SubstituteData(WTFMove(content), response.mimeType(), response.textEncodingName(), response.url().string(), nullptr, sessionHistoryVisibility)
{
}


Navigation::Navigation(WebNavigationState& state)
    : m_navigationID(state.generateNavigationID())
    , m_clientNavigationActivity(navigationActivityTimeout)
{
}

Navigation::Navigation(WebNavigationState& state, WebBackForwardListItem* currentAndTargetItem)
    : m_navigationID(state.generateNavigationID())
    , m_reloadItem(currentAndTargetItem)
    , m_clientNavigationActivity(navigationActivityTimeout)
{
}

Navigation::Navigation(WebNavigationState& state, WebCore::ResourceRequest&& request, WebBackForwardListItem* fromItem)
    : m_navigationID(state.generateNavigationID())
    , m_originalRequest(WTFMove(request))
    , m_currentRequest(m_originalRequest)
    , m_fromItem(fromItem)
    , m_clientNavigationActivity(navigationActivityTimeout)
{
    m_redirectChain.append(m_originalRequest.url());
}

Navigation::Navigation(WebNavigationState& state, WebBackForwardListItem& targetItem, WebBackForwardListItem* fromItem, FrameLoadType backForwardFrameLoadType)
    : m_navigationID(state.generateNavigationID())
    , m_originalRequest(targetItem.url())
    , m_currentRequest(m_originalRequest)
    , m_targetItem(&targetItem)
    , m_fromItem(fromItem)
    , m_backForwardFrameLoadType(backForwardFrameLoadType)
    , m_clientNavigationActivity(navigationActivityTimeout)
{
}

Navigation::Navigation(WebKit::WebNavigationState& state, std::unique_ptr<SubstituteData>&& substituteData)
    : Navigation(state)
{
    ASSERT(substituteData);
    m_substituteData = WTFMove(substituteData);
}

Navigation::Navigation(WebKit::WebNavigationState& state, WebCore::ResourceRequest&& simulatedRequest, std::unique_ptr<SubstituteData>&& substituteData, WebKit::WebBackForwardListItem* fromItem)
    : Navigation(state, WTFMove(simulatedRequest), fromItem)
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

#if !LOG_DISABLED

const char* Navigation::loggingString() const
{
    return debugString("Most recent URL: ", m_currentRequest.url().string(), " Back/forward list item URL: '", m_targetItem ? m_targetItem->url() : WTF::String { }, "' (0x", hex(reinterpret_cast<uintptr_t>(m_targetItem.get())), ')');
}

#endif

} // namespace API
