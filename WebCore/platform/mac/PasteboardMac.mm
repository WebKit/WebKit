/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "config.h"
#import "Pasteboard.h"

#import "DOMRangeInternal.h"
#import "DocumentFragment.h"
#import "Editor.h"
#import "WebNSAttributedStringExtras.h"
#import "markup.h"

using namespace WebCore;

NSString *WebArchivePboardType          = @"Apple Web Archive pasteboard type";
NSString *WebSmartPastePboardType       = @"NeXT smart paste pasteboard type";
NSString *WebURLNamePboardType          = nil;
NSString *WebURLPboardType              = nil;
NSString *WebURLsWithTitlesPboardType   = @"WebURLsWithTitlesPboardType";

@interface NSAttributedString (AppKitSecretsIKnowAbout)
- (id)_initWithDOMRange:(DOMRange *)domRange;
@end

Pasteboard* Pasteboard::s_generalPasteboard = 0;

Pasteboard* Pasteboard::generalPasteboard() 
{
    if (!s_generalPasteboard)
        s_generalPasteboard = new Pasteboard([NSPasteboard generalPasteboard]);
    return s_generalPasteboard;
}

Pasteboard::~Pasteboard()
{
    delete s_generalPasteboard;
    s_generalPasteboard = 0;
}

Pasteboard::Pasteboard(NSPasteboard* pboard)
: m_pasteboard(pboard)
{ }

void Pasteboard::clearTypes()
{
    [m_pasteboard declareTypes:[NSArray array] owner:nil];
}

void Pasteboard::writeSelection(PassRefPtr<Range> selectedRange, bool canSmartCopyOrDelete, Frame* frame)
{
    NSAttributedString *attributedString = [[[NSAttributedString alloc] _initWithDOMRange:[DOMRange _rangeWith:selectedRange.get()]] autorelease];
    NSArray* types = selectionPasteboardTypes(canSmartCopyOrDelete, [attributedString containsAttachments]);

    [m_pasteboard declareTypes:types owner:nil];

    // Put HTML on the pasteboard.
    if ([types containsObject:WebArchivePboardType]) {
        [m_pasteboard setData:frame->editor()->client()->dataForArchivedSelection(frame) forType:WebArchivePboardType];
    }

    // Put the attributed string on the pasteboard (RTF/RTFD format).
    if ([types containsObject:NSRTFDPboardType]) {
        NSData *RTFDData = [attributedString RTFDFromRange:NSMakeRange(0, [attributedString length]) documentAttributes:nil];
        [m_pasteboard setData:RTFDData forType:NSRTFDPboardType];
    }        
    if ([types containsObject:NSRTFPboardType]) {
        if ([attributedString containsAttachments]) {
            attributedString = [attributedString _web_attributedStringByStrippingAttachmentCharacters];
        }
        NSData *RTFData = [attributedString RTFFromRange:NSMakeRange(0, [attributedString length]) documentAttributes:nil];
        [m_pasteboard setData:RTFData forType:NSRTFPboardType];
    }
    
    // Put plain string on the pasteboard.
    if ([types containsObject:NSStringPboardType]) {
        // Map &nbsp; to a plain old space because this is better for source code, other browsers do it,
        // and because HTML forces you to do this any time you want two spaces in a row.
        String text = frame->selectedText();
        text.replace('\\', frame->backslashAsCurrencySymbol());
        NSMutableString *s = [[[(NSString*)text copy] autorelease] mutableCopy];
        
        const unichar NonBreakingSpaceCharacter = 0xA0;
        NSString *NonBreakingSpaceString = [NSString stringWithCharacters:&NonBreakingSpaceCharacter length:1];
        [s replaceOccurrencesOfString:NonBreakingSpaceString withString:@" " options:0 range:NSMakeRange(0, [s length])];
        [m_pasteboard setString:s forType:NSStringPboardType];
        [s release];
    }
    
    if ([types containsObject:WebSmartPastePboardType]) {
        [m_pasteboard setData:nil forType:WebSmartPastePboardType];
    }
}

NSArray* Pasteboard::selectionPasteboardTypes(bool canSmartCopyOrDelete, bool selectionContainsAttachments)
{
    if (selectionContainsAttachments) {
        if (canSmartCopyOrDelete)
            return [NSArray arrayWithObjects:WebSmartPastePboardType, WebArchivePboardType, NSRTFDPboardType, NSRTFPboardType, NSStringPboardType, nil];
        else
            return [NSArray arrayWithObjects:WebArchivePboardType, NSRTFDPboardType, NSRTFPboardType, NSStringPboardType, nil];
    } else { // Don't write RTFD to the pasteboard when the copied attributed string has no attachments.
        if (canSmartCopyOrDelete)
            return [NSArray arrayWithObjects:WebSmartPastePboardType, WebArchivePboardType, NSRTFPboardType, NSStringPboardType, nil];
        else
            return [NSArray arrayWithObjects:WebArchivePboardType, NSRTFPboardType, NSStringPboardType, nil];
    }
}

bool Pasteboard::canSmartReplace()
{
    return [[m_pasteboard types] containsObject:WebSmartPastePboardType];
}

String Pasteboard::plainText(Frame* frame)
{
    NSArray *types = [m_pasteboard types];
    
    if ([types containsObject:NSStringPboardType])
        return [m_pasteboard stringForType:NSStringPboardType];
    
    NSAttributedString *attributedString = nil;
    NSString *string;

    if ([types containsObject:NSRTFDPboardType])
        attributedString = [[NSAttributedString alloc] initWithRTFD:[m_pasteboard dataForType:NSRTFDPboardType] documentAttributes:NULL];
    if (attributedString == nil && [types containsObject:NSRTFPboardType])
        attributedString = [[NSAttributedString alloc] initWithRTF:[m_pasteboard dataForType:NSRTFPboardType] documentAttributes:NULL];
    if (attributedString != nil) {
        string = [[attributedString string] copy];
        [attributedString release];
        return [string autorelease];
    }
    
    if ([types containsObject:NSFilenamesPboardType]) {
        string = [[m_pasteboard propertyListForType:NSFilenamesPboardType] componentsJoinedByString:@"\n"];
        if (string != nil)
            return string;
    }
    
    
    NSURL *URL;
    
    if ((URL = [NSURL URLFromPasteboard:m_pasteboard])) {
        // FIXME: using the editorClient to call into webkit, for now, since 
        // calling [URL _web_userVisibleString] from WebCore involves migrating a sizable web of 
        // helper code that should either be done in a separate patch or figured out in another way.
        string = frame->editor()->client()->_web_userVisibleString(URL);
        if ([string length] > 0)
            return string;
    }

    
    return String(); 
}

PassRefPtr<DocumentFragment> Pasteboard::documentFragment(Frame* frame, PassRefPtr<Range> context, bool allowPlainText, bool& chosePlainText)
{
    NSArray *types = [m_pasteboard types];
    chosePlainText = false;

    if ([types containsObject:NSHTMLPboardType]) {
        NSString *HTMLString = [m_pasteboard stringForType:NSHTMLPboardType];
        // This is a hack to make Microsoft's HTML pasteboard data work. See 3778785.
        if ([HTMLString hasPrefix:@"Version:"]) {
            NSRange range = [HTMLString rangeOfString:@"<html" options:NSCaseInsensitiveSearch];
            if (range.location != NSNotFound) {
                HTMLString = [HTMLString substringFromIndex:range.location];
            }
        }
        if ([HTMLString length] != 0) {
            RefPtr<DocumentFragment> fragment = createFragmentFromMarkup(frame->document(), HTMLString, "");
            if (fragment)
                return fragment.release();
        }
    }
    
    if (allowPlainText && [types containsObject:NSStringPboardType]) {
        chosePlainText = true;
        RefPtr<DocumentFragment> fragment = createFragmentFromText(context.get(), [m_pasteboard stringForType:NSStringPboardType]);
        if (fragment)
            return fragment.release();
    }
    
    return 0;
}

