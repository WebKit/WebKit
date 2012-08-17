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

#include "config.h"

#include "CCScrollbarAnimationController.h"

#include "CCScrollbarLayerImpl.h"
#include <wtf/CurrentTime.h>

#if OS(ANDROID)
#include "CCScrollbarAnimationControllerLinearFade.h"
#endif

namespace WebCore {

#if OS(ANDROID)
PassOwnPtr<CCScrollbarAnimationController> CCScrollbarAnimationController::create(CCLayerImpl* scrollLayer)
{
    static const double fadeoutDelay = 0.3;
    static const double fadeoutLength = 0.3;
    return CCScrollbarAnimationControllerLinearFade::create(scrollLayer, fadeoutDelay, fadeoutLength);
}
#else
PassOwnPtr<CCScrollbarAnimationController> CCScrollbarAnimationController::create(CCLayerImpl* scrollLayer)
{
    return adoptPtr(new CCScrollbarAnimationController(scrollLayer));
}
#endif

CCScrollbarAnimationController::CCScrollbarAnimationController(CCLayerImpl* scrollLayer)
    : m_horizontalScrollbarLayer(0)
    , m_verticalScrollbarLayer(0)
{
    CCScrollbarAnimationController::updateScrollOffsetAtTime(scrollLayer, 0);
}

CCScrollbarAnimationController::~CCScrollbarAnimationController()
{
}

void CCScrollbarAnimationController::didPinchGestureBegin()
{
    didPinchGestureBeginAtTime(monotonicallyIncreasingTime());
}

void CCScrollbarAnimationController::didPinchGestureUpdate()
{
    didPinchGestureUpdateAtTime(monotonicallyIncreasingTime());
}

void CCScrollbarAnimationController::didPinchGestureEnd()
{
    didPinchGestureEndAtTime(monotonicallyIncreasingTime());
}

void CCScrollbarAnimationController::updateScrollOffset(CCLayerImpl* scrollLayer)
{
    updateScrollOffsetAtTime(scrollLayer, monotonicallyIncreasingTime());
}

IntSize CCScrollbarAnimationController::getScrollLayerBounds(const CCLayerImpl* scrollLayer)
{
    if (!scrollLayer->children().size())
        return IntSize();
    // Copy & paste from CCLayerTreeHostImpl...
    // FIXME: Hardcoding the first child here is weird. Think of
    // a cleaner way to get the contentBounds on the Impl side.
    return scrollLayer->children()[0]->bounds();
}

void CCScrollbarAnimationController::updateScrollOffsetAtTime(CCLayerImpl* scrollLayer, double)
{
    m_currentPos = scrollLayer->scrollPosition() + scrollLayer->scrollDelta();
    m_totalSize = getScrollLayerBounds(scrollLayer);
    m_maximum = scrollLayer->maxScrollPosition();

    if (m_horizontalScrollbarLayer) {
        m_horizontalScrollbarLayer->setCurrentPos(m_currentPos.x());
        m_horizontalScrollbarLayer->setTotalSize(m_totalSize.width());
        m_horizontalScrollbarLayer->setMaximum(m_maximum.width());
    }

    if (m_verticalScrollbarLayer) {
        m_verticalScrollbarLayer->setCurrentPos(m_currentPos.y());
        m_verticalScrollbarLayer->setTotalSize(m_totalSize.height());
        m_verticalScrollbarLayer->setMaximum(m_maximum.height());
    }
}

} // namespace WebCore
