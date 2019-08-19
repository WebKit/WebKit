/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "DisplayListReplayer.h"

#include "DisplayListItems.h"
#include "GraphicsContext.h"
#include "Logging.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace DisplayList {

Replayer::Replayer(GraphicsContext& context, const DisplayList& displayList)
    : m_displayList(displayList)
    , m_context(context)
{
}

Replayer::~Replayer() = default;

std::unique_ptr<DisplayList> Replayer::replay(const FloatRect& initialClip, bool trackReplayList)
{
    LOG_WITH_STREAM(DisplayLists, stream << "\nReplaying with clip " << initialClip);
    UNUSED_PARAM(initialClip);

    std::unique_ptr<DisplayList> replayList;
    if (UNLIKELY(trackReplayList))
        replayList = makeUnique<DisplayList>();

    size_t numItems = m_displayList.itemCount();
    for (size_t i = 0; i < numItems; ++i) {
        auto& item = m_displayList.list()[i].get();

        if (!initialClip.isZero() && is<DrawingItem>(item)) {
            const DrawingItem& drawingItem = downcast<DrawingItem>(item);
            if (drawingItem.extentKnown() && !drawingItem.extent().intersects(initialClip)) {
                LOG_WITH_STREAM(DisplayLists, stream << "skipping " << i << " " << item);
                continue;
            }
        }

        LOG_WITH_STREAM(DisplayLists, stream << "applying " << i << " " << item);
        item.apply(m_context);

        if (UNLIKELY(trackReplayList))
            replayList->appendItem(const_cast<Item&>(item));
    }
    
    return replayList;
}

} // namespace DisplayList
} // namespace WebCore
