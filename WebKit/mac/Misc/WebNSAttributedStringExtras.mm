/*
 * Copyright (C) 2005, 2007, 2008 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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

#import "WebNSAttributedStringExtras.h"

#import "DOMRangeInternal.h"
#import "WebDataSourcePrivate.h"
#import "WebFrame.h"
#import "WebFrameInternal.h"
#import "WebTypesInternal.h"
#import <WebCore/BlockExceptions.h>
#import <WebCore/ColorMac.h>
#import <WebCore/CSSHelper.h>
#import <WebCore/Document.h>
#import <WebCore/Element.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameLoader.h>
#import <WebCore/HTMLNames.h>
#import <WebCore/Image.h>
#import <WebCore/InlineTextBox.h>
#import <WebCore/Range.h>
#import <WebCore/RenderImage.h>
#import <WebCore/RenderListItem.h>
#import <WebCore/RenderObject.h>
#import <WebCore/RenderStyle.h>
#import <WebCore/RenderText.h>
#import <WebCore/SimpleFontData.h>
#import <WebCore/Text.h>
#import <WebCore/TextIterator.h>

using namespace WebCore;
using namespace HTMLNames;

struct ListItemInfo {
    unsigned start;
    unsigned end;
};

static NSFileWrapper *fileWrapperForElement(Element* e)
{
    NSFileWrapper *wrapper = nil;
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    
    const AtomicString& attr = e->getAttribute(srcAttr);
    if (!attr.isEmpty()) {
        NSURL *URL = e->document()->completeURL(attr);
        wrapper = [[kit(e->document()->frame()) _dataSource] _fileWrapperForURL:URL];
    }
    if (!wrapper) {
        RenderImage* renderer = static_cast<RenderImage*>(e->renderer());
        if (renderer->cachedImage() && !renderer->cachedImage()->errorOccurred()) {
            wrapper = [[NSFileWrapper alloc] initRegularFileWithContents:(NSData *)(renderer->cachedImage()->image()->getTIFFRepresentation())];
            [wrapper setPreferredFilename:@"image.tiff"];
            [wrapper autorelease];
        }
    }

    return wrapper;

    END_BLOCK_OBJC_EXCEPTIONS;

    return nil;
}

@implementation NSAttributedString (WebKitExtras)

- (NSAttributedString *)_web_attributedStringByStrippingAttachmentCharacters
{
    // This code was originally copied from NSTextView
    NSRange attachmentRange;
    NSString *originalString = [self string];
    static NSString *attachmentCharString = nil;
    
    if (!attachmentCharString) {
        unichar chars[2];
        if (!attachmentCharString) {
            chars[0] = NSAttachmentCharacter;
            chars[1] = 0;
            attachmentCharString = [[NSString alloc] initWithCharacters:chars length:1];
        }
    }
    
    attachmentRange = [originalString rangeOfString:attachmentCharString];
    if (attachmentRange.location != NSNotFound && attachmentRange.length > 0) {
        NSMutableAttributedString *newAttributedString = [[self mutableCopyWithZone:NULL] autorelease];
        
        while (attachmentRange.location != NSNotFound && attachmentRange.length > 0) {
            [newAttributedString replaceCharactersInRange:attachmentRange withString:@""];
            attachmentRange = [[newAttributedString string] rangeOfString:attachmentCharString];
        }
        return newAttributedString;
    }
    
    return self;
}

+ (NSAttributedString *)_web_attributedStringFromRange:(Range*)range
{
    NSMutableAttributedString *string = [[NSMutableAttributedString alloc] init];
    NSUInteger stringLength = 0;
    RetainPtr<NSMutableDictionary> attrs(AdoptNS, [[NSMutableDictionary alloc] init]);

    for (TextIterator it(range); !it.atEnd(); it.advance()) {
        RefPtr<Range> currentTextRange = it.range();
        ExceptionCode ec = 0;
        Node* startContainer = currentTextRange->startContainer(ec);
        Node* endContainer = currentTextRange->endContainer(ec);
        int startOffset = currentTextRange->startOffset(ec);
        int endOffset = currentTextRange->endOffset(ec);
        
        if (startContainer == endContainer && (startOffset == endOffset - 1)) {
            Node* node = startContainer->childNode(startOffset);
            if (node && node->hasTagName(imgTag)) {
                NSFileWrapper *fileWrapper = fileWrapperForElement(static_cast<Element*>(node));
                NSTextAttachment *attachment = [[NSTextAttachment alloc] initWithFileWrapper:fileWrapper];
                [string appendAttributedString:[NSAttributedString attributedStringWithAttachment:attachment]];
                [attachment release];
            }
        }

        int currentTextLength = it.length();
        if (!currentTextLength)
            continue;

        RenderObject* renderer = startContainer->renderer();
        ASSERT(renderer);
        if (!renderer)
            continue;
        RenderStyle* style = renderer->style();
        NSFont *font = style->font().primaryFont()->getNSFont();
        [attrs.get() setObject:font forKey:NSFontAttributeName];
        if (style->color().isValid())
            [attrs.get() setObject:nsColor(style->color()) forKey:NSForegroundColorAttributeName];
        else
            [attrs.get() removeObjectForKey:NSForegroundColorAttributeName];
        if (style->backgroundColor().isValid())
            [attrs.get() setObject:nsColor(style->backgroundColor()) forKey:NSBackgroundColorAttributeName];
        else
            [attrs.get() removeObjectForKey:NSBackgroundColorAttributeName];

        RetainPtr<NSString> substring(AdoptNS, [[NSString alloc] initWithCharactersNoCopy:const_cast<UChar*>(it.characters()) length:currentTextLength freeWhenDone:NO]);
        [string replaceCharactersInRange:NSMakeRange(stringLength, 0) withString:substring.get()];
        [string setAttributes:attrs.get() range:NSMakeRange(stringLength, currentTextLength)];
        stringLength += currentTextLength;
    }

    return [string autorelease];
}

@end
