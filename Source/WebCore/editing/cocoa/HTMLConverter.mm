/*
 * Copyright (C) 2011-2024 Apple Inc. All rights reserved.
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
#import "HTMLConverter.h"

#import "ArchiveResource.h"
#import "BoundaryPointInlines.h"
#import "CSSColorValue.h"
#import "CSSComputedStyleDeclaration.h"
#import "CSSPrimitiveValue.h"
#import "CSSSerializationContext.h"
#import "CachedImage.h"
#import "CharacterData.h"
#import "ColorCocoa.h"
#import "ColorMac.h"
#import "CommonAtomStrings.h"
#import "ComposedTreeIterator.h"
#import "ContainerNodeInlines.h"
#import "Document.h"
#import "DocumentLoader.h"
#import "Editing.h"
#import "ElementChildIteratorInlines.h"
#import "ElementInlines.h"
#import "ElementRareData.h"
#import "ElementTraversal.h"
#import "File.h"
#import "FontCascade.h"
#import "FrameLoader.h"
#import "HTMLAttachmentElement.h"
#import "HTMLElement.h"
#import "HTMLFrameElement.h"
#import "HTMLIFrameElement.h"
#import "HTMLImageElement.h"
#import "HTMLInputElement.h"
#import "HTMLMetaElement.h"
#import "HTMLNames.h"
#import "HTMLOListElement.h"
#import "HTMLTableCellElement.h"
#import "HTMLTextAreaElement.h"
#import "ImageAdapter.h"
#import "LoaderNSURLExtras.h"
#import "LocalFrame.h"
#import "LocalizedStrings.h"
#import "NodeName.h"
#import "WebCoreTextAttachment.h"
#import "markup.h"
#import <objc/runtime.h>
#import <pal/spi/cocoa/NSAttributedStringSPI.h>
#import <wtf/ASCIICType.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/text/MakeString.h>
#import <wtf/text/ParsingUtilities.h>
#import <wtf/text/StringBuilder.h>
#import <wtf/text/StringToIntegerConversion.h>

#if ENABLE(DATA_DETECTION)
#import "DataDetection.h"
#endif

#if ENABLE(MULTI_REPRESENTATION_HEIC)
#import "PlatformNSAdaptiveImageGlyph.h"
#endif

#if PLATFORM(IOS_FAMILY)
#import "UIFoundationSoftLink.h"
#import "WAKAppKitStubs.h"
#import <pal/ios/UIKitSoftLink.h>
#import <pal/spi/ios/UIKitSPI.h>
#endif

using namespace WebCore;
using namespace HTMLNames;

#if !PLATFORM(IOS_FAMILY)

// Returns the font to be used if the NSFontAttributeName doesn't exist
NSFont *WebCore::WebDefaultFont()
{
    static NeverDestroyed defaultFont = [] {
        NSFont *font = [NSFont fontWithName:@"Helvetica" size:12];
        if (!font)
            font = [NSFont systemFontOfSize:12];
        return RetainPtr { font };
    }();
    return defaultFont.get().get();
}

#endif


