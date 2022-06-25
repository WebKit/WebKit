/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#import "PasteboardTypes.h"

#import <WebCore/LegacyNSPasteboardTypes.h>
#import <wtf/RetainPtr.h>

#if PLATFORM(MAC)

namespace WebKit {

NSString * const PasteboardTypes::WebArchivePboardType = @"Apple Web Archive pasteboard type";
NSString * const PasteboardTypes::WebURLsWithTitlesPboardType = @"WebURLsWithTitlesPboardType";
NSString * const PasteboardTypes::WebURLPboardType = @"public.url";
NSString * const PasteboardTypes::WebURLNamePboardType = @"public.url-name";
NSString * const PasteboardTypes::WebDummyPboardType = @"Apple WebKit dummy pasteboard type";
    
NSArray* PasteboardTypes::forEditing()
{
ALLOW_DEPRECATED_DECLARATIONS_BEGIN 
    static NeverDestroyed<RetainPtr<NSArray>> types = @[WebArchivePboardType, (__bridge NSString *)kUTTypeWebArchive, WebCore::legacyHTMLPasteboardType(), WebCore::legacyFilenamesPasteboardType(), WebCore::legacyTIFFPasteboardType(), WebCore::legacyPDFPasteboardType(),
        WebCore::legacyURLPasteboardType(), WebCore::legacyRTFDPasteboardType(), WebCore::legacyRTFPasteboardType(), WebCore::legacyStringPasteboardType(), WebCore::legacyColorPasteboardType(), (__bridge NSString *)kUTTypePNG];
ALLOW_DEPRECATED_DECLARATIONS_END
    return types.get().get();
}

NSArray* PasteboardTypes::forURL()
{
    static NeverDestroyed<RetainPtr<NSArray>> types = @[WebURLsWithTitlesPboardType, WebCore::legacyURLPasteboardType(), WebURLPboardType,  WebURLNamePboardType, WebCore::legacyStringPasteboardType(), WebCore::legacyFilenamesPasteboardType(), WebCore::legacyFilesPromisePasteboardType()];
    return types.get().get();
}

NSArray* PasteboardTypes::forImages()
{
    static NeverDestroyed<RetainPtr<NSArray>> types = @[WebCore::legacyTIFFPasteboardType(), WebURLsWithTitlesPboardType, WebCore::legacyURLPasteboardType(), WebURLPboardType, WebURLNamePboardType, WebCore::legacyStringPasteboardType()];
    return types.get().get();
}

NSArray* PasteboardTypes::forImagesWithArchive()
{
    static NeverDestroyed<RetainPtr<NSArray>> types = @[WebCore::legacyTIFFPasteboardType(), WebURLsWithTitlesPboardType, WebCore::legacyURLPasteboardType(), WebURLPboardType, WebURLNamePboardType, WebCore::legacyStringPasteboardType(), WebCore::legacyRTFDPasteboardType(), WebArchivePboardType];
    return types.get().get();
}

NSArray* PasteboardTypes::forSelection()
{
ALLOW_DEPRECATED_DECLARATIONS_BEGIN 
    static NeverDestroyed<RetainPtr<NSArray>> types = @[WebArchivePboardType, (__bridge NSString *)kUTTypeWebArchive, WebCore::legacyRTFDPasteboardType(), WebCore::legacyRTFPasteboardType(), WebCore::legacyStringPasteboardType()];
ALLOW_DEPRECATED_DECLARATIONS_END
    return types.get().get();
}
    
} // namespace WebKit

#endif // PLATFORM(MAC)
