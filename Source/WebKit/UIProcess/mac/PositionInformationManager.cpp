/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "PositionInformationManager.h"

#include "WebPageProxy.h"
#include <wtf/RefPtr.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(PositionInformationManager);

PositionInformationManager::PositionInformationManager(WebPageProxy& page)
    : m_page(page)
{
}

PositionInformationManager::~PositionInformationManager()
{
    // Guarantee every still-queued callback runs exactly once, even when the manager is torn down
    // with requests in flight.
    invokeAndRemovePendingHandlers([](const InteractionInformationRequest&) {
        return true;
    }, std::nullopt);
}

void PositionInformationManager::doAfterUpdate(const InteractionInformationRequest& request, UpdateCompletionHandler&& callback)
{
    if (currentIsValid(request)) {
        callback(m_currentInformation);
        return;
    }

    m_pendingHandlers.constructAndAppend(std::in_place, request, WTF::move(callback));

    if (!hasValidOutstandingRequest(request))
        requestUpdate(request);
}

void PositionInformationManager::didChange(const InteractionInformationAtPosition& info)
{
    if (m_lastOutstandingRequest && info.request.isValidForRequest(*m_lastOutstandingRequest))
        m_lastOutstandingRequest = std::nullopt;

    auto newInfo = info;
    newInfo.mergeCompatibleOptionalInformation(m_currentInformation);

    m_currentInformation = WTF::move(newInfo);
    m_hasValidCurrentInformation = m_currentInformation.canBeValid;

    // Only the callbacks whose request the newly-arrived information satisfies should resolve now.
    invokeAndRemovePendingHandlers([&](const InteractionInformationRequest& request) {
        return currentIsValid(request);
    }, m_currentInformation);
}

void PositionInformationManager::invalidate()
{
    m_hasValidCurrentInformation = false;
    m_currentInformation = { };
}

void PositionInformationManager::reset()
{
    invalidate();
    m_lastOutstandingRequest = std::nullopt;
    invokeAndRemovePendingHandlers([](const InteractionInformationRequest&) {
        return true;
    }, std::nullopt);
}

void PositionInformationManager::abandonOutstandingRequest()
{
    auto abandonedRequest = std::exchange(m_lastOutstandingRequest, std::nullopt);
    if (!abandonedRequest)
        return;

    invokeAndRemovePendingHandlers([&](const InteractionInformationRequest& request) {
        return request.isValidForRequest(*abandonedRequest);
    }, std::nullopt);
}

void PositionInformationManager::requestUpdate(const InteractionInformationRequest& request)
{
    if (currentIsValid(request))
        return;

    RefPtr page = m_page.get();
    if (!page || !page->hasRunningProcess())
        return;

    m_lastOutstandingRequest = request;
    page->requestPositionInformation(request);
}

bool PositionInformationManager::currentIsValid(const InteractionInformationRequest& request) const
{
    return m_hasValidCurrentInformation && m_currentInformation.request.isValidForRequest(request);
}

bool PositionInformationManager::currentIsApproximatelyValid(const InteractionInformationRequest& request, int radius) const
{
    return m_hasValidCurrentInformation && m_currentInformation.request.isApproximatelyValidForRequest(request, radius);
}

bool PositionInformationManager::hasValidOutstandingRequest(const InteractionInformationRequest& request) const
{
    return m_lastOutstandingRequest.and_then([&request](const auto& outstandingRequest) -> std::optional<bool> {
        return outstandingRequest.isValidForRequest(request);
    }).value_or(false);
}

void PositionInformationManager::invokeAndRemovePendingHandlers(Function<bool(InteractionInformationRequest)>&& matches, const std::optional<InteractionInformationAtPosition>& information)
{
    ++m_callbackDepth;

    for (auto& slot : m_pendingHandlers) {
        if (!slot)
            continue;
        if (!matches(slot->request))
            continue;

        if (auto requestAndCallback = std::exchange(slot, std::nullopt); requestAndCallback->callback)
            requestAndCallback->callback(information);
    }

    if (--m_callbackDepth)
        return;

    m_pendingHandlers.removeAllMatching([](const auto& handler) {
        return !handler;
    });
}

} // namespace WebKit
