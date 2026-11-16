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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebCodecsImageTrackList.h"

#if ENABLE(WEB_CODECS)

#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebCodecsImageTrackList);

Ref<WebCodecsImageTrackList> WebCodecsImageTrackList::create()
{
    return adoptRef(*new WebCodecsImageTrackList());
}

WebCodecsImageTrackList::WebCodecsImageTrackList()
    : m_readyPromise(makeUniqueRef<ReadyPromise>())
{
}

void WebCodecsImageTrackList::setTrackList(Vector<Ref<WebCodecsImageTrack>>&& list)
{
    if (list.isEmpty())
        return;
    m_list = WTF::move(list);
    m_selectedIndex = 0;
    m_readyPromise->resolve();
}

void WebCodecsImageTrackList::clearTrackList(const Exception& exception)
{
    // If readyPromise is not fulfilled, reject it with exception.
    if (!m_readyPromise->isFulfilled()) {
        m_readyPromise->reject(exception);
        return;
    }

    // Remove all entries from the list and set the selected index to -1.
    m_list.clear();
    m_selectedIndex = -1;
}

RefPtr<WebCodecsImageTrack> WebCodecsImageTrackList::selectedTrack() const
{
    if (m_selectedIndex < 0)
        return nullptr;
    return item(m_selectedIndex);
}

RefPtr<WebCodecsImageTrack> WebCodecsImageTrackList::item(unsigned index) const
{
    if (index >= m_list.size())
        return nullptr;
    return m_list[index].copyRef();
}

} // namespace WebCore

#endif // ENABLE(WEB_CODECS)
