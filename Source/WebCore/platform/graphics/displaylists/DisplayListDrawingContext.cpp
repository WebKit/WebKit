/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "DisplayListDrawingContext.h"

#include "AffineTransform.h"
#include "DisplayListRecorderImpl.h"
#include "DisplayListReplayer.h"

namespace WebCore {
namespace DisplayList {

DrawingContext::DrawingContext(const FloatSize& logicalSize, const AffineTransform& initialCTM)
    : m_context(m_displayList, GraphicsContextState(), FloatRect({ }, logicalSize), initialCTM)
{
}

void DrawingContext::setTracksDisplayListReplay(bool tracksDisplayListReplay)
{
    m_tracksDisplayListReplay = tracksDisplayListReplay;
    m_replayedDisplayList.reset();
}

void DrawingContext::replayDisplayList(GraphicsContext& destContext)
{
    if (m_displayList.isEmpty())
        return;

    Replayer replayer(destContext, m_displayList);
    if (m_tracksDisplayListReplay)
        m_replayedDisplayList = replayer.replay({ }, m_tracksDisplayListReplay).trackedDisplayList;
    else
        replayer.replay();
    m_displayList.clear();
}

} // DisplayList
} // WebCore
