/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#include "BackForwardController.h"

#include "BackForwardClient.h"
#include "Page.h"
#include "ShouldTreatAsContinuingLoad.h"

namespace WebCore {

BackForwardController::BackForwardController(Page& page, Ref<BackForwardClient>&& client)
    : m_page(page)
    , m_client(WTFMove(client))
{
}

BackForwardController::~BackForwardController() = default;

bool BackForwardController::canGoBackOrForward(int distance) const
{
    if (!distance)
        return true;
    if (distance > 0 && distance <= forwardCount())
        return true;
    if (distance < 0 && -distance <= backCount())
        return true;
    return false;
}

void BackForwardController::goBackOrForward(int distance)
{
    if (!distance)
        return;

    HistoryItem* item = itemAtIndex(distance);
    if (!item) {
        if (distance > 0) {
            if (int forwardCount = this->forwardCount())
                item = itemAtIndex(forwardCount);
        } else {
            if (int backCount = this->backCount())
                item = itemAtIndex(-backCount);
        }
    }

    if (!item)
        return;

    m_page.goToItem(*item, FrameLoadType::IndexedBackForward, ShouldTreatAsContinuingLoad::No);
}

bool BackForwardController::goBack()
{
    HistoryItem* item = backItem();
    if (!item)
        return false;

    m_page.goToItem(*item, FrameLoadType::Back, ShouldTreatAsContinuingLoad::No);
    return true;
}

bool BackForwardController::goForward()
{
    HistoryItem* item = forwardItem();
    if (!item)
        return false;

    m_page.goToItem(*item, FrameLoadType::Forward, ShouldTreatAsContinuingLoad::No);
    return true;
}

void BackForwardController::addItem(Ref<HistoryItem>&& item)
{
    m_client->addItem(WTFMove(item));
}

void BackForwardController::setCurrentItem(HistoryItem& item)
{
    m_client->goToItem(item);
}

int BackForwardController::count() const
{
    return m_client->backListCount() + 1 + m_client->forwardListCount();
}

int BackForwardController::backCount() const
{
    return m_client->backListCount();
}

int BackForwardController::forwardCount() const
{
    return m_client->forwardListCount();
}

HistoryItem* BackForwardController::itemAtIndex(int i)
{
    return m_client->itemAtIndex(i);
}

void BackForwardController::close()
{
    m_client->close();
}

} // namespace WebCore
