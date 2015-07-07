/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
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
#include "WebBackForwardListItem.h"

namespace WebKit {

static uint64_t highestUsedItemID = 0;

PassRefPtr<WebBackForwardListItem> WebBackForwardListItem::create(BackForwardListItemState backForwardListItemState, uint64_t pageID)
{
    return adoptRef(new WebBackForwardListItem(WTF::move(backForwardListItemState), pageID));
}

WebBackForwardListItem::WebBackForwardListItem(BackForwardListItemState backForwardListItemState, uint64_t pageID)
    : m_itemState(WTF::move(backForwardListItemState))
    , m_pageID(pageID)
{
    if (m_itemState.identifier > highestUsedItemID)
        highestUsedItemID = m_itemState.identifier;
}

WebBackForwardListItem::~WebBackForwardListItem()
{
}

uint64_t WebBackForwardListItem::highedUsedItemID()
{
    return highestUsedItemID;
}

} // namespace WebKit
