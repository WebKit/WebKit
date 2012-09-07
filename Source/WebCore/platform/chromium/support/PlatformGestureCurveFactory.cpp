/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "PlatformGestureCurveFactory.h"

#include "FloatPoint.h"
#include "IntRect.h"
#include "TouchFlingPlatformGestureCurve.h"
#include "WebFlingAnimatorToGestureCurveAdapter.h"
#include "WebInputEvent.h"

namespace WebKit {

PlatformGestureCurveFactory* PlatformGestureCurveFactory::get()
{
    DEFINE_STATIC_LOCAL(PlatformGestureCurveFactory, factory, ());
    return &factory;
}

PassOwnPtr<WebCore::PlatformGestureCurve> PlatformGestureCurveFactory::createCurve(int deviceSource, const WebCore::FloatPoint& point, WebCore::IntPoint cumulativeScroll)
{
    OwnPtr<WebFlingAnimator> flingAnimator = m_mockFlingAnimator.release();
    if (!flingAnimator)
        flingAnimator = adoptPtr(Platform::current()->createFlingAnimator());

    if (flingAnimator)
        return WebFlingAnimatorToGestureCurveAdapter::create(point, WebCore::IntRect(), flingAnimator.release());

    if (deviceSource == WebGestureEvent::Touchscreen)
        return WebCore::TouchFlingPlatformGestureCurve::createForTouchScreen(point, cumulativeScroll);

    return WebCore::TouchFlingPlatformGestureCurve::createForTouchPad(point, cumulativeScroll);
}

void PlatformGestureCurveFactory::setWebFlingAnimatorForTest(PassOwnPtr<WebFlingAnimator> mockFlingAnimator)
{
    m_mockFlingAnimator = mockFlingAnimator;
}

} // namespace WebKit
