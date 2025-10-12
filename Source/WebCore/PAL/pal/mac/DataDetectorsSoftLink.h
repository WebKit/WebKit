/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#pragma once

#if PLATFORM(MAC) && ENABLE(DATA_DETECTION)

#import <pal/spi/mac/DataDetectorsSPI.h>
#import <wtf/SoftLinking.h>

SOFT_LINK_FRAMEWORK_FOR_HEADER(PAL, DataDetectors)

#if HAVE(SECURE_ACTION_CONTEXT)
SOFT_LINK_CLASS_FOR_HEADER(PAL, DDSecureActionContext)
#else
SOFT_LINK_CLASS_FOR_HEADER(PAL, DDActionContext)
#endif

SOFT_LINK_CLASS_FOR_HEADER(PAL, DDActionsManager)

#if HAVE(DATA_DETECTORS_MAC_ACTION)
SOFT_LINK_CLASS_FOR_HEADER(PAL, DDMacAction)
#else
SOFT_LINK_CLASS_FOR_HEADER(PAL, DDAction)
#endif

SOFT_LINK_FUNCTION_FOR_HEADER(PAL, DataDetectors, DDHighlightCreateWithRectsInVisibleRectWithStyleScaleAndDirection, DDHighlightRef, (CFAllocatorRef allocator, CGRect* rects, CFIndex count, CGRect globalVisibleRect, DDHighlightStyle style, Boolean withButton, NSWritingDirection writingDirection, Boolean endsWithEOL, Boolean flipped, CGFloat scale), (allocator, rects, count, globalVisibleRect, style, withButton, writingDirection, endsWithEOL, flipped, scale))
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, DataDetectors, DDHighlightGetLayerWithContext, CGLayerRef, (DDHighlightRef highlight, CGContextRef context), (highlight, context))
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, DataDetectors, DDHighlightGetBoundingRect, CGRect, (DDHighlightRef highlight), (highlight))
SOFT_LINK_FUNCTION_FOR_HEADER(PAL, DataDetectors, DDHighlightPointIsOnHighlight, Boolean, (DDHighlightRef highlight, CGPoint point, Boolean* onButton), (highlight, point, onButton))

namespace PAL {

inline WKDDActionContext *allocWKDDActionContextInstance()
{
#if HAVE(SECURE_ACTION_CONTEXT)
    return allocDDSecureActionContextInstance();
#else
    return allocDDActionContextInstance();
#endif
}

#ifdef __OBJC__
inline Class getWKDDActionContextClassSingleton()
{
#if HAVE(SECURE_ACTION_CONTEXT)
    return getDDSecureActionContextClassSingleton();
#else
    return getDDActionContextClassSingleton();
#endif
}
#endif

}

#endif // PLATFORM(MAC) && ENABLE(DATA_DETECTION)
