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

#include "WebAnimationCurveCommon.h"

#include "CCTimingFunction.h"

#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

namespace WebKit {

PassOwnPtr<WebCore::CCTimingFunction> createTimingFunction(WebAnimationCurve::TimingFunctionType type)
{
    switch (type) {
    case WebAnimationCurve::TimingFunctionTypeEase:
        return WebCore::CCEaseTimingFunction::create();
    case WebAnimationCurve::TimingFunctionTypeEaseIn:
        return WebCore::CCEaseInTimingFunction::create();
    case WebAnimationCurve::TimingFunctionTypeEaseOut:
        return WebCore::CCEaseOutTimingFunction::create();
    case WebAnimationCurve::TimingFunctionTypeEaseInOut:
        return WebCore::CCEaseInOutTimingFunction::create();
    case WebAnimationCurve::TimingFunctionTypeLinear:
        return nullptr;
    }
    return nullptr;
}

} // namespace WebKit
