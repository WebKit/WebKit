/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#import "Test.h"
#import <WebCore/AttributedString.h>
#import <wtf/RetainPtr.h>

#if PLATFORM(MAC)
#import <AppKit/AppKit.h>
#else
#import <UIKit/UIKit.h>
#endif

namespace TestWebKitAPI {

static RetainPtr<NSData> roundTripFileWrapperAttachmentContents(NSData *contents, NSString *preferredFilename)
{
    RetainPtr<NSData> result;
    @autoreleasepool {
        RetainPtr fileWrapper = adoptNS([[NSFileWrapper alloc] initRegularFileWithContents:contents]);
        [fileWrapper setPreferredFilename:preferredFilename];
        RetainPtr attachment = adoptNS([[NSTextAttachment alloc] initWithData:nil ofType:nil]);
        [attachment setFileWrapper:fileWrapper.get()];
        RetainPtr input = adoptNS([[NSMutableAttributedString alloc] initWithAttributedString:[NSAttributedString attributedStringWithAttachment:attachment.get()]]);

        auto webCoreString = WebCore::AttributedString::fromNSAttributedString(WTF::move(input));
        RetainPtr roundTripped = webCoreString.nsAttributedString();

        NSTextAttachment *outputAttachment = [roundTripped attribute:NSAttachmentAttributeName atIndex:0 effectiveRange:nullptr];
        EXPECT_NOT_NULL(outputAttachment);
        result = retainPtr([[outputAttachment fileWrapper] regularFileContents]);
    }
    return result;
}

TEST(AttributedStringImageAttachment, PreservesWebSafeImageData)
{
    static constexpr uint8_t pngBytes[] = {
        137, 80, 78, 71, 13, 10, 26, 10, 0, 0, 0, 13, 73, 72, 68, 82, 0, 0, 0, 1, 0, 0, 0, 1, 8, 2,
        0, 0, 0, 144, 119, 83, 222, 0, 0, 0, 12, 73, 68, 65, 84, 120, 218, 99, 248, 207, 192, 0, 0,
        3, 1, 1, 0, 247, 3, 65, 67, 0, 0, 0, 0, 73, 69, 78, 68, 174, 66, 96, 130
    };
    RetainPtr png = adoptNS([[NSData alloc] initWithBytes:pngBytes length:sizeof(pngBytes)]);
    RetainPtr result = roundTripFileWrapperAttachmentContents(png.get(), @"image.png");
    EXPECT_NOT_NULL(result.get());
    EXPECT_TRUE([result isEqualToData:png.get()]);
}

TEST(AttributedStringImageAttachment, DropsUnsupportedImageData)
{
    static constexpr uint8_t photoshopBytes[] = {
        '8', 'B', 'P', 'S', 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00,
        0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x08, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00
    };
    RetainPtr photoshop = adoptNS([[NSData alloc] initWithBytes:photoshopBytes length:sizeof(photoshopBytes)]);
    RetainPtr result = roundTripFileWrapperAttachmentContents(photoshop.get(), @"image.psd");
    EXPECT_FALSE([result isEqualToData:photoshop.get()]);
}

TEST(AttributedStringImageAttachment, PreservesNonImageData)
{
    RetainPtr text = retainPtr([@"this is not an image" dataUsingEncoding:NSUTF8StringEncoding]);
    RetainPtr textResult = roundTripFileWrapperAttachmentContents(text.get(), @"note.txt");
    EXPECT_NOT_NULL(textResult.get());
    EXPECT_TRUE([textResult isEqualToData:text.get()]);

    static constexpr uint8_t emptyZipBytes[] = {
        0x50, 0x4B, 0x05, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    RetainPtr zip = adoptNS([[NSData alloc] initWithBytes:emptyZipBytes length:sizeof(emptyZipBytes)]);
    RetainPtr zipResult = roundTripFileWrapperAttachmentContents(zip.get(), @"archive.zip");
    EXPECT_NOT_NULL(zipResult.get());
    EXPECT_TRUE([zipResult isEqualToData:zip.get()]);
}

} // namespace TestWebKitAPI
