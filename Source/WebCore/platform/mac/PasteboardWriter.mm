/*
 * Copyright (C) 2017-2025 Apple Inc. All rights reserved.
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
#import <UniformTypeIdentifiers/UTCoreTypes.h>
#import <UniformTypeIdentifiers/UTType.h>
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
    RetainPtr utType = [UTType typeWithIdentifier:type];
    if ([utType isDeclared] || [utType isDynamic]) {
        // This is already a UTI.
        return type;
    }

    return toUTI(type);
}

RetainPtr<id <NSPasteboardWriting>> createPasteboardWriter(const PasteboardWriterData& data)
{
    auto pasteboardItem = adoptNS([[NSPasteboardItem alloc] init]);

    if (auto& plainText = data.plainText()) {
        [pasteboardItem setString:plainText->text.createNSString().get() forType:NSPasteboardTypeString];
        if (plainText->canSmartCopyOrDelete) {
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            auto smartPasteType = bridge_cast(adoptCF(UTTypeCreatePreferredIdentifierForTag(kUTTagClassNSPboardType, RetainPtr { bridge_cast(_NXSmartPaste) }.get(), nullptr)));
ALLOW_DEPRECATED_DECLARATIONS_END
            [pasteboardItem setData:[NSData data] forType:smartPasteType.get()];
        }
    }

    if (auto& urlData = data.urlData()) {
        RetainPtr nsURL = urlData->url.createNSURL();
        RetainPtr userVisibleString = urlData->userVisibleForm.createNSString();
        RetainPtr title = urlData->title.createNSString();
        if (!title.get().length) {
            title = nsURL.get().path.lastPathComponent;
            if (!title.get().length)
                title = userVisibleString;
        }

        // WebURLsWithTitlesPboardType.
        // FIXME: This could use StringView (the one that creates NSString) to save an allocation
        auto paths = adoptNS([[NSArray alloc] initWithObjects:@[ @[ nsURL.get().absoluteString ] ], @[ urlData->title.trim(deprecatedIsSpaceOrNewline).createNSString().get() ], nil]);
        [pasteboardItem setPropertyList:paths.get() forType:toUTI(@"WebURLsWithTitlesPboardType").get()];

        // NSURLPboardType.
        if (RetainPtr<NSURL> baseCocoaURL = nsURL.get().baseURL)
            [pasteboardItem setPropertyList:@[ nsURL.get().relativeString, baseCocoaURL.get().absoluteString ] forType:toUTI(WebCore::legacyURLPasteboardTypeSingleton()).get()];
        else if (nsURL)
            [pasteboardItem setPropertyList:@[ nsURL.get().absoluteString, @"" ] forType:toUTI(WebCore::legacyURLPasteboardTypeSingleton()).get()];
        else
            [pasteboardItem setPropertyList:@[ @"", @"" ] forType:toUTI(WebCore::legacyURLPasteboardTypeSingleton()).get()];

        if (nsURL.get().fileURL)
            [pasteboardItem setString:retainPtr(nsURL.get().absoluteString).get() forType:UTTypeFileURL.identifier];
        [pasteboardItem setString:userVisibleString.get() forType:UTTypeURL.identifier];

        // WebURLNamePboardType.
        [pasteboardItem setString:title.get() forType:@"public.url-name"];

        // NSPasteboardTypeString.
        [pasteboardItem setString:userVisibleString.get() forType:NSPasteboardTypeString];
    }

    if (auto& webContent = data.webContent()) {
        if (webContent->canSmartCopyOrDelete) {
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            auto smartPasteType = bridge_cast(adoptCF(UTTypeCreatePreferredIdentifierForTag(kUTTagClassNSPboardType, RetainPtr { bridge_cast(_NXSmartPaste) }.get(), nullptr)));
ALLOW_DEPRECATED_DECLARATIONS_END
            [pasteboardItem setData:[NSData data] forType:smartPasteType.get()];
        }
        if (RefPtr dataInWebArchiveFormat = webContent->dataInWebArchiveFormat) {
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            auto webArchiveType = bridge_cast(adoptCF(UTTypeCreatePreferredIdentifierForTag(kUTTagClassNSPboardType, CFSTR("Apple Web Archive pasteboard type"), nullptr)));
ALLOW_DEPRECATED_DECLARATIONS_END
            [pasteboardItem setData:dataInWebArchiveFormat->createNSData().get() forType:webArchiveType.get()];
        }
        if (RefPtr dataInRTFDFormat = webContent->dataInRTFDFormat)
            [pasteboardItem setData:dataInRTFDFormat->createNSData().get() forType:NSPasteboardTypeRTFD];
        if (RefPtr dataInRTFFormat = webContent->dataInRTFFormat)
            [pasteboardItem setData:dataInRTFFormat->createNSData().get() forType:NSPasteboardTypeRTF];
        if (!webContent->dataInHTMLFormat.isNull())
            [pasteboardItem setString:webContent->dataInHTMLFormat.createNSString().get() forType:NSPasteboardTypeHTML];
        if (!webContent->dataInStringFormat.isNull())
            [pasteboardItem setString:webContent->dataInStringFormat.createNSString().get() forType:NSPasteboardTypeString];

        for (unsigned i = 0; i < webContent->clientTypesAndData.size(); ++i)
            [pasteboardItem setData:RefPtr { webContent->clientTypesAndData[i].second }->createNSData().get() forType:toUTIUnlessAlreadyUTI(webContent->clientTypesAndData[i].first.createNSString().get()).get()];

        PasteboardCustomData customData;
        customData.setOrigin(webContent->contentOrigin);
        [pasteboardItem setData:customData.createSharedBuffer()->createNSData().get() forType:toUTIUnlessAlreadyUTI(PasteboardCustomData::cocoaType().createNSString().get()).get()];
    }

    return pasteboardItem;
}

}

#endif
