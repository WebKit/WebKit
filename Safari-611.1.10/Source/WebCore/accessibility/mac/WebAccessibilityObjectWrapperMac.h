/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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

#import "WebAccessibilityObjectWrapperBase.h"

#if PLATFORM(MAC)

#import <pal/spi/mac/HIServicesSPI.h>

#ifndef NSAccessibilityPrimaryScreenHeightAttribute
#define NSAccessibilityPrimaryScreenHeightAttribute @"_AXPrimaryScreenHeight"
#endif

@interface WebAccessibilityObjectWrapper : WebAccessibilityObjectWrapperBase

// FIXME: Remove these methods since clients should not need to call them and hence should not be exposed in the public interface.
// Inside WebCore, use the WebCore homonymous declared below instead.
- (id)textMarkerRangeFromVisiblePositions:(const WebCore::VisiblePosition&)startPosition endPosition:(const WebCore::VisiblePosition&)endPosition;
- (id)textMarkerForVisiblePosition:(const WebCore::VisiblePosition&)visiblePos;
- (id)textMarkerForFirstPositionInTextControl:(WebCore::HTMLTextFormControlElement&)textControl;

// When a plugin uses a WebKit control to act as a surrogate view (e.g. PDF use WebKit to create text fields).
- (id)associatedPluginParent;

@end

#else

typedef const struct __AXTextMarker* AXTextMarkerRef;
typedef const struct __AXTextMarkerRange* AXTextMarkerRangeRef;

#endif // PLATFORM(MAC)

namespace WebCore {

class AccessibilityObject;
struct CharacterOffset;

// TextMarker and TextMarkerRange public funcstions.
// FIXME: TextMarker and TextMarkerRange should become classes on their own right, wrapping the system objects.

AccessibilityObject* accessibilityObjectForTextMarker(AXObjectCache*, AXTextMarkerRef);

// TextMarker <-> VisiblePosition conversion.
AXTextMarkerRef textMarkerForVisiblePosition(AXObjectCache*, const VisiblePosition&);
VisiblePosition visiblePositionForTextMarker(AXObjectCache*, AXTextMarkerRef);

// TextMarkerRange <-> VisiblePositionRange conversion.
AXTextMarkerRangeRef textMarkerRangeFromVisiblePositions(AXObjectCache*, const VisiblePosition&, const VisiblePosition&);
VisiblePositionRange visiblePositionRangeForTextMarkerRange(AXObjectCache*, AXTextMarkerRangeRef);

// TextMarker <-> CharacterOffset conversion.
AXTextMarkerRef textMarkerForCharacterOffset(AXObjectCache*, const CharacterOffset&);
CharacterOffset characterOffsetForTextMarker(AXObjectCache*, AXTextMarkerRef);

// TextMarkerRange <-> SimpleRange conversion.
AXTextMarkerRef startOrEndTextMarkerForRange(AXObjectCache*, const Optional<SimpleRange>&, bool isStart);
AXTextMarkerRangeRef textMarkerRangeFromRange(AXObjectCache*, const Optional<SimpleRange>&);
Optional<SimpleRange> rangeForTextMarkerRange(AXObjectCache*, AXTextMarkerRangeRef);

}
