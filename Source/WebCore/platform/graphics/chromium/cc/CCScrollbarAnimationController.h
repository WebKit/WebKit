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

#ifndef CCScrollbarAnimationController_h
#define CCScrollbarAnimationController_h

#include "FloatPoint.h"
#include "IntSize.h"
#include <wtf/PassOwnPtr.h>

namespace WebCore {

class CCLayerImpl;
class CCScrollbarLayerImpl;

// This abstract class represents the compositor-side analogy of ScrollbarAnimator.
// Individual platforms should subclass it to provide specialized implementation.
class CCScrollbarAnimationController {
public:
    // Implemented by subclass.
    static PassOwnPtr<CCScrollbarAnimationController> create(CCLayerImpl* scrollLayer);

    virtual ~CCScrollbarAnimationController();

    virtual bool animate(double monotonicTime) { return false; }
    void didPinchGestureBegin();
    void didPinchGestureUpdate();
    void didPinchGestureEnd();
    void updateScrollOffset(CCLayerImpl* scrollLayer);

    void setHorizontalScrollbarLayer(CCScrollbarLayerImpl* layer) { m_horizontalScrollbarLayer = layer; }
    CCScrollbarLayerImpl* horizontalScrollbarLayer() const { return m_horizontalScrollbarLayer; }

    void setVerticalScrollbarLayer(CCScrollbarLayerImpl* layer) { m_verticalScrollbarLayer = layer; }
    CCScrollbarLayerImpl* verticalScrollbarLayer() const { return m_verticalScrollbarLayer; }

    FloatPoint currentPos() const { return m_currentPos; }
    IntSize totalSize() const { return m_totalSize; }
    IntSize maximum() const { return m_maximum; }

    virtual void didPinchGestureBeginAtTime(double monotonicTime) { }
    virtual void didPinchGestureUpdateAtTime(double monotonicTime) { }
    virtual void didPinchGestureEndAtTime(double monotonicTime) { }
    virtual void updateScrollOffsetAtTime(CCLayerImpl* scrollLayer, double monotonicTime);

protected:
    explicit CCScrollbarAnimationController(CCLayerImpl* scrollLayer);

private:
    static IntSize getScrollLayerBounds(const CCLayerImpl*);

    // Beware of dangling pointer. Always update these during tree synchronization.
    CCScrollbarLayerImpl* m_horizontalScrollbarLayer;
    CCScrollbarLayerImpl* m_verticalScrollbarLayer;

    FloatPoint m_currentPos;
    IntSize m_totalSize;
    IntSize m_maximum;
};

} // namespace WebCore

#endif // CCScrollbarAnimationController_h
