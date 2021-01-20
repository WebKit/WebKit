/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#if HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)

#include <CoreText/CoreText.h>
#include <MediaAccessibility/MediaAccessibility.h>
#include <wtf/SoftLinking.h>

#if COMPILER(MSVC)
// See https://msdn.microsoft.com/en-us/library/35bhkfb6.aspx
#pragma warning(disable: 4273)
#endif

SOFT_LINK_FRAMEWORK_FOR_SOURCE(WebCore, MediaAccessibility)

SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, MediaAccessibility, MACaptionAppearanceGetDisplayType, MACaptionAppearanceDisplayType, (MACaptionAppearanceDomain domain), (domain))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, MediaAccessibility, MACaptionAppearanceSetDisplayType, void, (MACaptionAppearanceDomain domain, MACaptionAppearanceDisplayType displayType), (domain, displayType))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, MediaAccessibility, MACaptionAppearanceCopyForegroundColor, CGColorRef, (MACaptionAppearanceDomain domain, MACaptionAppearanceBehavior *behavior), (domain, behavior))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, MediaAccessibility, MACaptionAppearanceCopyBackgroundColor, CGColorRef, (MACaptionAppearanceDomain domain, MACaptionAppearanceBehavior *behavior), (domain, behavior))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, MediaAccessibility, MACaptionAppearanceCopyWindowColor, CGColorRef, (MACaptionAppearanceDomain domain, MACaptionAppearanceBehavior *behavior), (domain, behavior))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, MediaAccessibility, MACaptionAppearanceGetForegroundOpacity, CGFloat, (MACaptionAppearanceDomain domain, MACaptionAppearanceBehavior *behavior), (domain, behavior))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, MediaAccessibility, MACaptionAppearanceGetBackgroundOpacity, CGFloat, (MACaptionAppearanceDomain domain, MACaptionAppearanceBehavior *behavior), (domain, behavior))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, MediaAccessibility, MACaptionAppearanceGetWindowOpacity, CGFloat, (MACaptionAppearanceDomain domain, MACaptionAppearanceBehavior *behavior), (domain, behavior))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, MediaAccessibility, MACaptionAppearanceGetWindowRoundedCornerRadius, CGFloat, (MACaptionAppearanceDomain domain, MACaptionAppearanceBehavior *behavior), (domain, behavior))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, MediaAccessibility, MACaptionAppearanceCopyFontDescriptorForStyle, CTFontDescriptorRef, (MACaptionAppearanceDomain domain,  MACaptionAppearanceBehavior *behavior, MACaptionAppearanceFontStyle fontStyle), (domain, behavior, fontStyle))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, MediaAccessibility, MACaptionAppearanceGetRelativeCharacterSize, CGFloat, (MACaptionAppearanceDomain domain, MACaptionAppearanceBehavior *behavior), (domain, behavior))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, MediaAccessibility, MACaptionAppearanceGetTextEdgeStyle, MACaptionAppearanceTextEdgeStyle, (MACaptionAppearanceDomain domain, MACaptionAppearanceBehavior *behavior), (domain, behavior))
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, MediaAccessibility, MACaptionAppearanceAddSelectedLanguage, bool, (MACaptionAppearanceDomain domain, CFStringRef language), (domain, language));
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, MediaAccessibility, MACaptionAppearanceCopySelectedLanguages, CFArrayRef, (MACaptionAppearanceDomain domain), (domain));
SOFT_LINK_FUNCTION_FOR_SOURCE(WebCore, MediaAccessibility, MACaptionAppearanceCopyPreferredCaptioningMediaCharacteristics,  CFArrayRef, (MACaptionAppearanceDomain domain), (domain));
SOFT_LINK_FUNCTION_MAY_FAIL_FOR_SOURCE(WebCore, MediaAccessibility, MAAudibleMediaCopyPreferredCharacteristics, CFArrayRef, (), ());
SOFT_LINK_FUNCTION_MAY_FAIL_FOR_SOURCE(WebCore, MediaAccessibility, MACaptionAppearanceCopyFontDescriptorWithStrokeForStyle, CTFontDescriptorRef, (MACaptionAppearanceDomain domain, MACaptionAppearanceBehavior *behavior, MACaptionAppearanceFontStyle fontStyle, CFStringRef trackLanguage, CGFloat darwingPointSize, CGFloat *strokeWidthPt), (domain, behavior, fontStyle, trackLanguage, darwingPointSize, strokeWidthPt));
SOFT_LINK_CONSTANT_FOR_SOURCE(WebCore, MediaAccessibility, kMAXCaptionAppearanceSettingsChangedNotification, CFStringRef)
SOFT_LINK_CONSTANT_MAY_FAIL_FOR_SOURCE(WebCore, MediaAccessibility, kMAAudibleMediaSettingsChangedNotification, CFStringRef)

#endif // HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)
