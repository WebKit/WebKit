 /*
 * Copyright (C) 2014 Igalia S.L.
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER “AS IS” AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#pragma once

#include "RenderSelectionInfo.h"

#if ENABLE(SERVICE_CONTROLS)
#include "SelectionRectGatherer.h"
#endif

namespace WebCore {

struct OldSelectionData;
    
class SelectionRangeData {
public:
    SelectionRangeData(RenderView&);
    
    class Context {
    public:
        Context() = default;
        Context(RenderObject* start, RenderObject* end, unsigned startOffset, unsigned endOffset)
            : m_start(makeWeakPtr(start))
            , m_end(makeWeakPtr(end))
            , m_startPosition(startOffset)
            , m_endPosition(endOffset)
        {
        }

        RenderObject* start() const { return m_start.get(); }
        RenderObject* end() const { return m_end.get(); }
        Optional<unsigned> startPosition() const { return m_startPosition; }
        Optional<unsigned> endPosition() const { return m_endPosition; }

        bool operator==(const Context& other) const
        {
            return m_start == other.m_start && m_end == other.m_end && m_startPosition == other.m_startPosition && m_endPosition == other.m_endPosition;
        }

    private:
        WeakPtr<RenderObject> m_start;
        WeakPtr<RenderObject> m_end;
        Optional<unsigned> m_startPosition;
        Optional<unsigned> m_endPosition;
    };

    enum class RepaintMode { NewXOROld, NewMinusOld, Nothing };
    void set(const Context&, RepaintMode = RepaintMode::NewXOROld);
    const Context& get() const { return m_selectionContext; }

    RenderObject* start() const { return m_selectionContext.start(); }
    RenderObject* end() const { return m_selectionContext.end(); }
    unsigned startPosition() const { ASSERT(m_selectionContext.startPosition()); return m_selectionContext.startPosition().value(); }
    unsigned endPosition() const { ASSERT(m_selectionContext.endPosition()); return m_selectionContext.endPosition().value(); }

    void clear();
    IntRect bounds() const { return collectBounds(ClipToVisibleContent::No); }
    IntRect boundsClippedToVisibleContent() const { return collectBounds(ClipToVisibleContent::Yes); }
    void repaint() const;

private:
    enum class ClipToVisibleContent { Yes, No };
    IntRect collectBounds(ClipToVisibleContent) const;
    void apply(const Context&, RepaintMode);

    const RenderView& m_renderView;
#if ENABLE(SERVICE_CONTROLS)
    SelectionRectGatherer m_selectionRectGatherer;
#endif
    Context m_selectionContext;
    bool m_selectionWasCaret { false };
};

} // namespace WebCore
