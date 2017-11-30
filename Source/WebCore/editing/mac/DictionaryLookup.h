/*
 * Copyright (C) 2014-2017 Apple Inc. All rights reserved.
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

#pragma once

#if PLATFORM(MAC)

#include "DictionaryPopupInfo.h"
#include <wtf/Function.h>

OBJC_CLASS NSString;
OBJC_CLASS NSView;
OBJC_CLASS PDFSelection;

// This file is included in Internals.cpp, so we can't use ObjC outright.
#if defined(__OBJC__)
#include <pal/spi/mac/NSImmediateActionGestureRecognizerSPI.h>
#define PlatformAnimationController id<NSImmediateActionAnimationController>
#else
#define PlatformAnimationController void*
#endif

namespace WebCore {

class HitTestResult;
class Range;
class VisibleSelection;

class DictionaryLookup {
public:
    WEBCORE_EXPORT static RefPtr<Range> rangeForSelection(const VisibleSelection&, NSDictionary **options);
    WEBCORE_EXPORT static RefPtr<Range> rangeAtHitTestResult(const HitTestResult&, NSDictionary **options);
    WEBCORE_EXPORT static NSString *stringForPDFSelection(PDFSelection *, NSDictionary **options);

    // FIXME: Should move/unify dictionaryPopupInfoForRange here too.

    WEBCORE_EXPORT static void showPopup(const DictionaryPopupInfo&, NSView *, const WTF::Function<void(TextIndicator&)>& textIndicatorInstallationCallback, const WTF::Function<FloatRect(FloatRect)>& rootViewToViewConversionCallback = nullptr);
    WEBCORE_EXPORT static void hidePopup();

    WEBCORE_EXPORT static PlatformAnimationController animationControllerForPopup(const DictionaryPopupInfo&, NSView *, const WTF::Function<void(TextIndicator&)>& textIndicatorInstallationCallback, const WTF::Function<FloatRect(FloatRect)>& rootViewToViewConversionCallback = nullptr);
};

} // namespace WebCore

#endif // PLATFORM(MAC)
