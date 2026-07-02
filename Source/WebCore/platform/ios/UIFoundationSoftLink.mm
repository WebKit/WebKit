/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#import "config.h"

#if PLATFORM(IOS_FAMILY)

#import <pal/spi/ios/UIKitSPI.h>
#import <wtf/SoftLinking.h>

// FIXME: Remove SOFT_LINK_PRIVATE_FRAMEWORK(UIFoundation) and move symbols from NSAttributedStringSPI.h to here.
SOFT_LINK_PRIVATE_FRAMEWORK_FOR_SOURCE(WebCore, UIFoundation)

SOFT_LINK_CLASS_FOR_SOURCE(WebCore, UIFoundation, NSColor)
SOFT_LINK_CLASS_FOR_SOURCE(WebCore, UIFoundation, NSTextAttachment)
SOFT_LINK_CLASS_FOR_SOURCE(WebCore, UIFoundation, NSMutableParagraphStyle)
SOFT_LINK_CLASS_FOR_SOURCE(WebCore, UIFoundation, NSTextList)
SOFT_LINK_CLASS_FOR_SOURCE(WebCore, UIFoundation, NSTextTableBlock)
SOFT_LINK_CLASS_FOR_SOURCE(WebCore, UIFoundation, NSTextTable)
SOFT_LINK_CLASS_FOR_SOURCE(WebCore, UIFoundation, NSTextTab)

#if ENABLE(MULTI_REPRESENTATION_HEIC)

SOFT_LINK_CLASS_FOR_SOURCE(WebCore, UIFoundation, NSAdaptiveImageGlyph)

SOFT_LINK_CONSTANT_FOR_SOURCE(WebCore, UIFoundation, NSAdaptiveImageGlyphAttributeName, NSString *)

#endif

#endif
