/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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
#import "Image.h"

#import "FloatRect.h"
#import "FoundationExtras.h"
#import "GraphicsContext.h"
#import "PDFDocumentImage.h"
#import "PlatformString.h"
#import "WebCoreFrameBridge.h"
#import "WebCoreSystemInterface.h"

namespace WebCore {

// ================================================
// Image Class
// ================================================

Image* Image::loadResource(const char *name)
{
    NSBundle *bundle = [NSBundle bundleForClass:[WebCoreFrameBridge class]];
    NSString *imagePath = [bundle pathForResource:[NSString stringWithUTF8String:name] ofType:@"tiff"];
    NSData *namedImageData = [NSData dataWithContentsOfFile:imagePath];
    if (namedImageData) {
        Image* image = new Image;
        image->setNativeData((CFDataRef)namedImageData, true);
        return image;
    }
    return 0;
}

bool Image::supportsType(const String& type)
{
    // FIXME: Would be better if this was looking in a set rather than an NSArray.
    // FIXME: Would be better not to convert to an NSString just to check if a type is supported.
    return [[WebCoreFrameBridge supportedImageResourceMIMETypes] containsObject:type];
}

NSImage* Image::getNSImage()
{
    if (m_nsImage)
        return m_nsImage;

    CFDataRef data = getTIFFRepresentation();
    if (!data)
        return 0;
    
    m_nsImage = HardRetainWithNSRelease([[NSImage alloc] initWithData:(NSData*)data]);
    return m_nsImage;
}

}
