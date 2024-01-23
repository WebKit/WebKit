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
#include "HistoryItem.h"
#include "Page.h"
#include "ShouldTreatAsContinuingLoad.h"

namespace WebCore {

BackForwardController::BackForwardController(Page& page, Ref<BackForwardClient>&& client)
    : m_page(page)
    , m_client(WTFMove(client))
{
}

BackForwardController::~BackForwardController() = default;

RefPtr<HistoryItem> BackForwardController::backItem()
{
    return itemAtIndex(-1);
}

RefPtr<HistoryItem> BackForwardController::currentItem()
{
    return itemAtIndex(0);
}

RefPtr<HistoryItem> BackForwardController::forwardItem()
{
    return itemAtIndex(1);
}

Ref<Page> BackForwardController::protectedPage() const
{
    return m_page.get();
}

Ref<BackForwardClient> BackForwardController::protectedClient() const
{
    return m_client;
}

bool BackForwardController::canGoBackOrForward(int distance) const
{
    if (!distance)
        return true;
    if (distance > 0 && static_cast<unsigned>(distance) <= forwardCount())
        return true;
    if (distance < 0 && static_cast<unsigned>(-distance) <= backCount())
        return true;
    return false;
}

void BackForwardController::goBackOrForward(int distance)
{
    if (!distance)
        return;

    RefPtr historyItem = itemAtIndex(distance);
    if (!historyItem) {
        if (distance > 0) {
            if (int forwardCount = this->forwardCount())
                historyItem = itemAtIndex(forwardCount);
        } else {
            if (int backCount = this->backCount())
                historyItem = itemAtIndex(-backCount);
        }
    }

    if (!historyItem)
        return;

    protectedPage()->goToItem(*historyItem, FrameLoadType::IndexedBackForward, ShouldTreatAsContinuingLoad::No);
}

bool BackForwardController::goBack()
{
    RefPtr historyItem = backItem();
    if (!historyItem)
        return false;

    protectedPage()->goToItem(*historyItem, FrameLoadType::Back, ShouldTreatAsContinuingLoad::No);
    return true;
}

bool BackForwardController::goForward()
{
    RefPtr historyItem = forwardItem();
    if (!historyItem)
        return false;

    protectedPage()->goToItem(*historyItem, FrameLoadType::Forward, ShouldTreatAsContinuingLoad::No);
    return true;
}

void BackForwardController::addItem(Ref<HistoryItem>&& item)
{
    protectedClient()->addItem(WTFMove(item));
}

void BackForwardController::setCurrentItem(HistoryItem& item)
{
    protectedClient()->goToItem(item);
}

bool BackForwardController::containsItem(const HistoryItem& item) const
{
    return protectedClient()->containsItem(item);
}

unsigned BackForwardController::count() const
{
    Ref client = m_client;
    return client->backListCount() + 1 + client->forwardListCount();
}

unsigned BackForwardController::backCount() const
{
    return protectedClient()->backListCount();
}

unsigned BackForwardController::forwardCount() const
{
    return protectedClient()->forwardListCount();
}

RefPtr<HistoryItem> BackForwardController::itemAtIndex(int i)
{
    return protectedClient()->itemAtIndex(i);
}

void BackForwardController::close()
{
    protectedClient()->close();
}

} // namespace WebCore
