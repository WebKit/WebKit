/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef RenderedDocumentMarker_h
#define RenderedDocumentMarker_h

#include "DocumentMarker.h"
#include <wtf/Vector.h>

namespace WebCore {

class RenderedDocumentMarker : public DocumentMarker {
public:
    explicit RenderedDocumentMarker(const DocumentMarker& marker)
        : DocumentMarker(marker)
    {
    }

    bool contains(const LayoutPoint& point) const
    {
        for (const auto& rect : m_renderedRects) {
            if (rect.contains(point))
                return true;
        }
        return false;
    }

    void addRenderedRect(const LayoutRect& r) { m_renderedRects.append(r); }
    const Vector<LayoutRect, 1>& renderedRects() const { return m_renderedRects; }
    void invalidate(const LayoutRect& r)
    {
        for (const auto& rect : m_renderedRects) {
            if (rect.intersects(r)) {
                invalidate();
                return;
            }
        }
    }
    void invalidate() { m_renderedRects.clear(); }

private:
    Vector<LayoutRect, 1> m_renderedRects;
};


} // namespace

#endif
