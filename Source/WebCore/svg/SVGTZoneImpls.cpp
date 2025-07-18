/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include "ElementInlines.h"
#include "SVGAnimatedPropertyAnimatorImpl.h"
#include "SVGAnimatedPropertyPairAnimatorImpl.h"
#include "SVGDecoratedProperty.h"
#include "SVGElementRareData.h"
#include "SVGImageClients.h"
#include "SVGPathByteStream.h"
#include "SVGPathConsumer.h"
#include "SVGPathSource.h"
#include "SVGPropertyAnimatorFactory.h"
#include "SVGStringList.h"
#include "SVGValuePropertyAnimatorImpl.h"

#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SVGAnimatedAngleAnimator);
WTF_MAKE_TZONE_ALLOCATED_IMPL(SVGAnimatedAngleOrientAnimator);
WTF_MAKE_TZONE_ALLOCATED_IMPL(SVGAnimatedBooleanAnimator);
WTF_MAKE_TZONE_ALLOCATED_IMPL(SVGAnimatedIntegerAnimator);
WTF_MAKE_TZONE_ALLOCATED_IMPL(SVGAnimatedIntegerPairAnimator);
WTF_MAKE_TZONE_ALLOCATED_IMPL(SVGAnimatedLengthAnimator);
WTF_MAKE_TZONE_ALLOCATED_IMPL(SVGAnimatedLengthListAnimator);
WTF_MAKE_TZONE_ALLOCATED_IMPL(SVGAnimatedNumberAnimator);
WTF_MAKE_TZONE_ALLOCATED_IMPL(SVGAnimatedNumberListAnimator);
WTF_MAKE_TZONE_ALLOCATED_IMPL(SVGAnimatedNumberPairAnimator);
WTF_MAKE_TZONE_ALLOCATED_IMPL(SVGAnimatedOrientTypeAnimator);
WTF_MAKE_TZONE_ALLOCATED_IMPL(SVGAnimatedPathSegListAnimator);
WTF_MAKE_TZONE_ALLOCATED_IMPL(SVGAnimatedPointListAnimator);
WTF_MAKE_TZONE_ALLOCATED_IMPL(SVGAnimatedPreserveAspectRatioAnimator);
WTF_MAKE_TZONE_ALLOCATED_IMPL(SVGAnimatedRectAnimator);
WTF_MAKE_TZONE_ALLOCATED_IMPL(SVGAnimatedStringAnimator);
WTF_MAKE_TZONE_ALLOCATED_IMPL(SVGAnimatedTransformListAnimator);

WTF_MAKE_TZONE_ALLOCATED_IMPL(SVGElementRareData);
WTF_MAKE_TZONE_ALLOCATED_IMPL(SVGImageChromeClient);
WTF_MAKE_TZONE_ALLOCATED_IMPL(SVGLengthAnimator);
WTF_MAKE_TZONE_ALLOCATED_IMPL(SVGPathByteStream);
WTF_MAKE_TZONE_ALLOCATED_IMPL(SVGPathConsumer);
WTF_MAKE_TZONE_ALLOCATED_IMPL(SVGPathSource);
WTF_MAKE_TZONE_ALLOCATED_IMPL(SVGPropertyAnimatorFactory);

} // namespace WebCore
