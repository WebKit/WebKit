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

#import "LegacyNSPasteboardTypes.h"
#import "Pasteboard.h"
#import "PasteboardWriterData.h"
#import "SharedBuffer.h"
#import <pal/spi/mac/NSPasteboardSPI.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

namespace WebCore {

static RetainPtr<NSString> toUTI(NSString *pasteboardType)
{
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    return bridge_cast(adoptCF(UTTypeCreatePreferredIdentifierForTag(kUTTagClassNSPboardType, bridge_cast(pasteboardType), nullptr)));
ALLOW_DEPRECATED_DECLARATIONS_END
}

static RetainPtr<NSString> toUTIUnlessAlreadyUTI(NSString *type)
{
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if (UTTypeIsDeclared(bridge_cast(type)) || UTTypeIsDynamic(bridge_cast(type))) {
        // This is already a UTI.
        return type;
    }
ALLOW_DEPRECATED_DECLARATIONS_END

    return toUTI(type);
}

RetainPtr<id <NSPasteboardWriting>> createPasteboardWriter(const PasteboardWriterData& data)
{
    auto pasteboardItem = adoptNS([[NSPasteboardItem alloc] init]);

    if (auto& plainText = data.plainText()) {
        [pasteboardItem setString:plainText->text forType:NSPasteboardTypeString];
        if (plainText->canSmartCopyOrDelete) {
            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            auto smartPasteType = bridge_cast(adoptCF(UTTypeCreatePreferredIdentifierForTag(kUTTagClassNSPboardType, bridge_cast(_NXSmartPaste), nullptr)));
            ALLOW_DEPRECATED_DECLARATIONS_END
            [pasteboardItem setData:[NSData data] forType:smartPasteType.get()];
        }
    }

    if (auto& urlData = data.urlData()) {
        NSURL *cocoaURL = urlData->url;
        NSString *userVisibleString = urlData->userVisibleForm;
        NSString *title = (NSString *)urlData->title;
        if (!title.length) {
            title = cocoaURL.path.lastPathComponent;
            if (!title.length)
                title = userVisibleString;
        }

        // WebURLsWithTitlesPboardType.
        auto paths = adoptNS([[NSArray alloc] initWithObjects:@[ @[ cocoaURL.absoluteString ] ], @[ urlData->title.stripWhiteSpace() ], nil]);
        [pasteboardItem setPropertyList:paths.get() forType:toUTI(@"WebURLsWithTitlesPboardType").get()];

        // NSURLPboardType.
        if (NSURL *baseCocoaURL = cocoaURL.baseURL)
            [pasteboardItem setPropertyList:@[ cocoaURL.relativeString, baseCocoaURL.absoluteString ] forType:toUTI(WebCore::legacyURLPasteboardType()).get()];
        else if (cocoaURL)
            [pasteboardItem setPropertyList:@[ cocoaURL.absoluteString, @"" ] forType:toUTI(WebCore::legacyURLPasteboardType()).get()];
        else
            [pasteboardItem setPropertyList:@[ @"", @"" ] forType:toUTI(WebCore::legacyURLPasteboardType()).get()];

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        if (cocoaURL.fileURL)
            [pasteboardItem setString:cocoaURL.absoluteString forType:(NSString *)kUTTypeFileURL];
        [pasteboardItem setString:userVisibleString forType:(NSString *)kUTTypeURL];
ALLOW_DEPRECATED_DECLARATIONS_END

        // WebURLNamePboardType.
        [pasteboardItem setString:title forType:@"public.url-name"];

        // NSPasteboardTypeString.
        [pasteboardItem setString:userVisibleString forType:NSPasteboardTypeString];
    }

    if (auto& webContent = data.webContent()) {
        if (webContent->canSmartCopyOrDelete) {
            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            auto smartPasteType = bridge_cast(adoptCF(UTTypeCreatePreferredIdentifierForTag(kUTTagClassNSPboardType, bridge_cast(_NXSmartPaste), nullptr)));
            ALLOW_DEPRECATED_DECLARATIONS_END
            [pasteboardItem setData:[NSData data] forType:smartPasteType.get()];
        }
        if (webContent->dataInWebArchiveFormat) {
            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            auto webArchiveType = bridge_cast(adoptCF(UTTypeCreatePreferredIdentifierForTag(kUTTagClassNSPboardType, CFSTR("Apple Web Archive pasteboard type"), nullptr)));
            ALLOW_DEPRECATED_DECLARATIONS_END
            [pasteboardItem setData:webContent->dataInWebArchiveFormat->createNSData().get() forType:webArchiveType.get()];
        }
        if (webContent->dataInRTFDFormat)
            [pasteboardItem setData:webContent->dataInRTFDFormat->createNSData().get() forType:NSPasteboardTypeRTFD];
        if (webContent->dataInRTFFormat)
            [pasteboardItem setData:webContent->dataInRTFFormat->createNSData().get() forType:NSPasteboardTypeRTF];
        if (!webContent->dataInHTMLFormat.isNull())
            [pasteboardItem setString:webContent->dataInHTMLFormat forType:NSPasteboardTypeHTML];
        if (!webContent->dataInStringFormat.isNull())
            [pasteboardItem setString:webContent->dataInStringFormat forType:NSPasteboardTypeString];

        for (unsigned i = 0; i < webContent->clientTypes.size(); ++i)
            [pasteboardItem setData:webContent->clientData[i]->createNSData().get() forType:toUTIUnlessAlreadyUTI(webContent->clientTypes[i]).get()];

        PasteboardCustomData customData;
        customData.setOrigin(webContent->contentOrigin);
        [pasteboardItem setData:customData.createSharedBuffer()->createNSData().get() forType:toUTIUnlessAlreadyUTI(String(PasteboardCustomData::cocoaType())).get()];
    }

    return pasteboardItem;
}

}

#endif
