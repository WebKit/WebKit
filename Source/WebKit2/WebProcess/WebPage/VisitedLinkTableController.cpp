/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "VisitedLinkTableController.h"

#include "VisitedLinkProviderMessages.h"
#include "VisitedLinkTableControllerMessages.h"
#include "WebProcess.h"
#include <WebCore/PageCache.h>
#include <wtf/NeverDestroyed.h>

using namespace WebCore;

namespace WebKit {

static HashMap<uint64_t, VisitedLinkTableController*>& visitedLinkTableControllers()
{
    static NeverDestroyed<HashMap<uint64_t, VisitedLinkTableController*>> visitedLinkTableControllers;

    return visitedLinkTableControllers;
}

PassRefPtr<VisitedLinkTableController> VisitedLinkTableController::getOrCreate(uint64_t identifier)
{
    auto& visitedLinkTableControllerPtr = visitedLinkTableControllers().add(identifier, nullptr).iterator->value;
    if (visitedLinkTableControllerPtr)
        return visitedLinkTableControllerPtr;

    RefPtr<VisitedLinkTableController> visitedLinkTableController = adoptRef(new VisitedLinkTableController(identifier));
    visitedLinkTableControllerPtr = visitedLinkTableController.get();

    return visitedLinkTableController.release();
}

VisitedLinkTableController::VisitedLinkTableController(uint64_t identifier)
    : m_identifier(identifier)
{
    WebProcess::shared().addMessageReceiver(Messages::VisitedLinkTableController::messageReceiverName(), m_identifier, *this);
}

VisitedLinkTableController::~VisitedLinkTableController()
{
    ASSERT(visitedLinkTableControllers().contains(m_identifier));

    WebProcess::shared().removeMessageReceiver(Messages::VisitedLinkTableController::messageReceiverName(), m_identifier);

    visitedLinkTableControllers().remove(m_identifier);
}

bool VisitedLinkTableController::isLinkVisited(Page&, LinkHash linkHash, const URL&, const AtomicString&)
{
    return m_visitedLinkTable.isLinkVisited(linkHash);
}

void VisitedLinkTableController::addVisitedLink(Page&, LinkHash linkHash)
{
    if (m_visitedLinkTable.isLinkVisited(linkHash))
        return;

    if (!WebProcess::shared().shouldTrackVisitedLinks())
        return;

    WebProcess::shared().parentProcessConnection()->send(Messages::VisitedLinkProvider::AddVisitedLinkHash(linkHash), m_identifier);
}

void VisitedLinkTableController::setVisitedLinkTable(const SharedMemory::Handle& handle)
{
    RefPtr<SharedMemory> sharedMemory = SharedMemory::create(handle, SharedMemory::ReadOnly);
    if (!sharedMemory)
        return;

    m_visitedLinkTable.setSharedMemory(sharedMemory.release());

    invalidateStylesForAllLinks();
    pageCache()->markPagesForVistedLinkStyleRecalc();
}

void VisitedLinkTableController::visitedLinkStateChanged(const Vector<WebCore::LinkHash>& linkHashes)
{
    for (auto linkHash : linkHashes)
        invalidateStylesForLink(linkHash);
    pageCache()->markPagesForVistedLinkStyleRecalc();
}

void VisitedLinkTableController::allVisitedLinkStateChanged()
{
    invalidateStylesForAllLinks();
    pageCache()->markPagesForVistedLinkStyleRecalc();
}

} // namespace WebKit
