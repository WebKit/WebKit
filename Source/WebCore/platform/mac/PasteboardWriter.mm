/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#import "PasteboardWriter.h"

#if PLATFORM(MAC)

#import "NSPasteboardSPI.h"
#import "PasteboardWriterData.h"

namespace WebCore {

static RetainPtr<NSString> toUTI(NSString *pasteboardType)
{
    return adoptNS((__bridge NSString *)UTTypeCreatePreferredIdentifierForTag(kUTTagClassNSPboardType, (__bridge CFStringRef)pasteboardType, nullptr));
}

RetainPtr<id <NSPasteboardWriting>> createPasteboardWriter(const PasteboardWriterData& data)
{
    auto pasteboardItem = adoptNS([[NSPasteboardItem alloc] init]);

    if (auto& plainText = data.plainText()) {
        [pasteboardItem setString:plainText->text forType:NSPasteboardTypeString];
        if (plainText->canSmartCopyOrDelete) {
            auto smartPasteType = adoptNS((__bridge NSString *)UTTypeCreatePreferredIdentifierForTag(kUTTagClassNSPboardType, (__bridge CFStringRef)_NXSmartPaste, nullptr));
            [pasteboardItem setData:nil forType:smartPasteType.get()];
        }
    }

    if (auto& url = data.url()) {
        NSURL *cocoaURL = url->url;
        NSString *userVisibleString = url->userVisibleForm;
        NSString *title = (NSString *)url->title;
        if (!title.length) {
            title = cocoaURL.path.lastPathComponent;
            if (!title.length)
                title = userVisibleString;
        }

        // WebURLsWithTitlesPboardType.
        auto paths = adoptNS([[NSArray alloc] initWithObjects:@[ @[ cocoaURL.absoluteString ] ], @[ url->title.stripWhiteSpace() ], nil]);
        [pasteboardItem setPropertyList:paths.get() forType:toUTI(@"WebURLsWithTitlesPboardType").get()];

        // NSURLPboardType.
        if (NSURL *baseCocoaURL = cocoaURL.baseURL)
            [pasteboardItem setPropertyList:@[ cocoaURL.relativeString, baseCocoaURL.absoluteString ] forType:toUTI(NSURLPboardType).get()];
        else if (cocoaURL)
            [pasteboardItem setPropertyList:@[ cocoaURL.absoluteString, @"" ] forType:toUTI(NSURLPboardType).get()];
        else
            [pasteboardItem setPropertyList:@[ @"", @"" ] forType:toUTI(NSURLPboardType).get()];

        if (cocoaURL.fileURL)
            [pasteboardItem setString:cocoaURL.absoluteString forType:(NSString *)kUTTypeFileURL];
        [pasteboardItem setString:userVisibleString forType:(NSString *)kUTTypeURL];

        // WebURLNamePboardType.
        [pasteboardItem setString:title forType:@"public.url-name"];

        // NSPasteboardTypeString.
        [pasteboardItem setString:userVisibleString forType:NSPasteboardTypeString];
    }

    return pasteboardItem;
}

}

#endif
