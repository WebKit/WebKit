/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WKPageLoadTypes_h
#define WKPageLoadTypes_h

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    kWKFrameNavigationTypeLinkClicked = 0,
    kWKFrameNavigationTypeFormSubmitted = 1,
    kWKFrameNavigationTypeBackForward = 2,
    kWKFrameNavigationTypeReload = 3,
    kWKFrameNavigationTypeFormResubmitted = 4,
    kWKFrameNavigationTypeOther = 5
};
typedef uint32_t WKFrameNavigationType;

enum {
    kWKSameDocumentNavigationAnchorNavigation,
    kWKSameDocumentNavigationSessionStatePush,
    kWKSameDocumentNavigationSessionStateReplace,
    kWKSameDocumentNavigationSessionStatePop
};
typedef uint32_t WKSameDocumentNavigationType;

enum {
    kWKDidFirstLayout = 1 << 0,
    kWKDidFirstVisuallyNonEmptyLayout = 1 << 1,
    kWKDidHitRelevantRepaintedObjectsAreaThreshold = 1 << 2,
    kWKReserved = 1 << 3, // Note that the fourth member of this enum is actually private and defined in WKPageLoadTypesPrivate.h
    kWKDidFirstLayoutAfterSuppressedIncrementalRendering = 1 << 4,
    kWKDidFirstPaintAfterSuppressedIncrementalRendering = 1 << 5,
    kWKDidRenderSignificantAmountOfText = 1 << 7,
    kWKDidFirstMeaningfulPaint = 1 << 8
};
typedef uint32_t WKLayoutMilestones;

#ifdef __cplusplus
}
#endif

#endif /* WKPageLoadTypes_h */
