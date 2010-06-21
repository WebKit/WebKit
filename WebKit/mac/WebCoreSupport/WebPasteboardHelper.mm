/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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

#import "WebPasteboardHelper.h"

#import "WebArchive.h"
#import "WebHTMLViewInternal.h"
#import "WebNSPasteboardExtras.h"
#import "WebNSURLExtras.h"
#import <WebCore/PlatformString.h>
#import <WebKit/DOMDocument.h>
#import <WebKit/DOMDocumentFragment.h>
#import <wtf/RetainPtr.h>
#import <wtf/StdLibExtras.h>

using namespace WebCore;

String WebPasteboardHelper::urlFromPasteboard(NSPasteboard* pasteboard, String* title) const
{
    NSURL *URL = [pasteboard _web_bestURL];
    if (title) {
        if (NSString *URLTitleString = [pasteboard stringForType:WebURLNamePboardType])
            *title = URLTitleString;
    }

    return [URL _web_originalDataAsString];
}

String WebPasteboardHelper::plainTextFromPasteboard(NSPasteboard *pasteboard) const
{    
    NSArray *types = [pasteboard types];
    
    if ([types containsObject:NSStringPboardType])
        return [[pasteboard stringForType:NSStringPboardType] precomposedStringWithCanonicalMapping];
    
    NSAttributedString *attributedString = nil;
    NSString *string;
    
    if ([types containsObject:NSRTFDPboardType])
        attributedString = [[NSAttributedString alloc] initWithRTFD:[pasteboard dataForType:NSRTFDPboardType] documentAttributes:nil];
    if (!attributedString && [types containsObject:NSRTFPboardType])
        attributedString = [[NSAttributedString alloc] initWithRTF:[pasteboard dataForType:NSRTFPboardType] documentAttributes:nil];
    if (attributedString) {
        string = [[attributedString string] precomposedStringWithCanonicalMapping];
        [attributedString release];
        return string;
    }
    
    if ([types containsObject:NSFilenamesPboardType]) {
        string = [[pasteboard propertyListForType:NSFilenamesPboardType] componentsJoinedByString:@"\n"];
        if (string)
            return [string precomposedStringWithCanonicalMapping];
    }
    
    NSURL *URL;
    
    if ((URL = [NSURL URLFromPasteboard:pasteboard])) {
        string = [URL _web_userVisibleString];
        if ([string length] > 0)
            return string;
    }
    
    return String();
}

DOMDocumentFragment *WebPasteboardHelper::fragmentFromPasteboard(NSPasteboard *pasteboard) const
{   
    return [m_view _documentFragmentFromPasteboard:pasteboard];
}

NSArray *WebPasteboardHelper::insertablePasteboardTypes() const
{
    DEFINE_STATIC_LOCAL(RetainPtr<NSArray>, types, ([[NSArray alloc] initWithObjects:WebArchivePboardType, NSHTMLPboardType, NSFilenamesPboardType, NSTIFFPboardType, NSPDFPboardType,
#if defined(BUILDING_ON_TIGER) || defined(BUILDING_ON_LEOPARD)
           NSPICTPboardType,
#endif
           NSURLPboardType, NSRTFDPboardType, NSRTFPboardType, NSStringPboardType, NSColorPboardType, nil]));

    return types.get();
}
