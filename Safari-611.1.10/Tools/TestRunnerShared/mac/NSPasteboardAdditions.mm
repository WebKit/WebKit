/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#import "NSPasteboardAdditions.h"

#if PLATFORM(MAC)

#import <CoreServices/CoreServices.h>
#import <WebCore/LegacyNSPasteboardTypes.h>
#import <wtf/RetainPtr.h>

@implementation NSPasteboard (TestRunnerAdditions)

+ (NSPasteboardType)_modernPasteboardType:(NSString *)type
{
    if (UTTypeIsDynamic((__bridge CFStringRef)type)) {
        if (auto legacyType = adoptNS((__bridge NSString *)UTTypeCopyPreferredTagWithClass((__bridge CFStringRef)type, kUTTagClassNSPboardType)))
            type = legacyType.autorelease();
    }

    if ([type isEqualToString:WebCore::legacyStringPasteboardType()])
        return NSPasteboardTypeString;

    if ([type isEqualToString:WebCore::legacyHTMLPasteboardType()])
        return NSPasteboardTypeHTML;

    if ([type isEqualToString:WebCore::legacyTIFFPasteboardType()])
        return NSPasteboardTypeTIFF;

    if ([type isEqualToString:WebCore::legacyURLPasteboardType()])
        return NSPasteboardTypeURL;

    if ([type isEqualToString:WebCore::legacyPDFPasteboardType()])
        return NSPasteboardTypePDF;

    if ([type isEqualToString:WebCore::legacyRTFDPasteboardType()])
        return NSPasteboardTypeRTFD;

    if ([type isEqualToString:WebCore::legacyRTFPasteboardType()])
        return NSPasteboardTypeRTF;

    if ([type isEqualToString:WebCore::legacyColorPasteboardType()])
        return NSPasteboardTypeColor;

    if ([type isEqualToString:WebCore::legacyFontPasteboardType()])
        return NSPasteboardTypeFont;

    return type;
}

@end

#endif // PLATFORM(MAC)
