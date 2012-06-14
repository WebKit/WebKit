/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CCDebugRectHistory_h
#define CCDebugRectHistory_h

#if USE(ACCELERATED_COMPOSITING)

#include "FloatRect.h"
#include <wtf/Noncopyable.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class CCLayerImpl;
struct CCSettings;

// There are currently three types of debug rects:
//
// - Paint rects (update rects): regions of a layer that needed to be re-uploaded to the
//   texture resource; in most cases implying that WebCore had to repaint them, too.
//
// - Property-changed rects: enclosing bounds of layers that cause changes to the screen
//   even if the layer did not change internally. (For example, if the layer's opacity or
//   position changes.)
//
// - Surface damage rects: the aggregate damage on a target surface that is caused by all
//   layers and surfaces that contribute to it. This includes (1) paint rects, (2) property-
//   changed rects, and (3) newly exposed areas.
//
enum DebugRectType { PaintRectType, PropertyChangedRectType, SurfaceDamageRectType };

struct CCDebugRect {
    CCDebugRect(DebugRectType newType, FloatRect newRect)
            : type(newType)
            , rect(newRect) { }

    DebugRectType type;
    FloatRect rect;
};

// This class maintains a history of rects of various types that can be used
// for debugging purposes. The overhead of collecting rects is performed only if
// the appropriate CCSettings are enabled.
class CCDebugRectHistory {
    WTF_MAKE_NONCOPYABLE(CCDebugRectHistory);
public:
    static PassOwnPtr<CCDebugRectHistory> create()
    {
        return adoptPtr(new CCDebugRectHistory());
    }

    bool enabled(const CCSettings&);

    // Note: Saving debug rects must happen before layers' change tracking is reset.
    void saveDebugRectsForCurrentFrame(CCLayerImpl* rootLayer, const Vector<CCLayerImpl*>& renderSurfaceLayerList, const CCSettings&);

    const Vector<CCDebugRect>& debugRects() { return m_debugRects; }

private:
    CCDebugRectHistory();

    void savePaintRects(CCLayerImpl*);
    void savePropertyChangedRects(const Vector<CCLayerImpl*>& renderSurfaceLayerList);
    void saveSurfaceDamageRects(const Vector<CCLayerImpl* >& renderSurfaceLayerList);

    Vector<CCDebugRect> m_debugRects;
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)

#endif
