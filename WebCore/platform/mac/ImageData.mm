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
#import <kxmlcore/Vector.h>
#import "IntSize.h"
#import "Array.h"
#import "FloatRect.h"
#import "ImageDecoder.h"
#import "ImageData.h"
#import "Image.h"

namespace WebCore {

static void setCompositingOperation(CGContextRef context, Image::CompositeOperator op)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    [[NSGraphicsContext graphicsContextWithGraphicsPort:context flipped:NO] setCompositingOperation:(NSCompositingOperation)op];
    [pool release];
}

void ImageData::invalidateAppleSpecificData()
{
    if (m_frames.size() != 1)
        return;

    if (m_nsImage) {
        [m_nsImage release];
        m_nsImage = 0;
    }

    if (m_tiffRep) {
        CFRelease(m_tiffRep);
        m_tiffRep = 0;
    }

    m_isSolidColor = false;
    if (m_solidColor) {
        CFRelease(m_solidColor);
        m_solidColor = 0;
    }
}

void ImageData::checkForSolidColor(CGImageRef image)
{
    m_isSolidColor = false;
    if (m_solidColor) {
        CFRelease(m_solidColor);
        m_solidColor = 0;
    }
    
    // Currently we only check for solid color in the important special case of a 1x1 image.
    if (image && CGImageGetWidth(image) == 1 && CGImageGetHeight(image) == 1) {
        float pixel[4]; // RGBA
        CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
// This #if won't be needed once the CG header that includes kCGBitmapByteOrder32Host is included in the OS.
#if __ppc__
        CGContextRef bmap = CGBitmapContextCreate(&pixel, 1, 1, 8*sizeof(float), sizeof(pixel), space,
            kCGImageAlphaPremultipliedLast | kCGBitmapFloatComponents);
#else
        CGContextRef bmap = CGBitmapContextCreate(&pixel, 1, 1, 8*sizeof(float), sizeof(pixel), space,
            kCGImageAlphaPremultipliedLast | kCGBitmapFloatComponents | kCGBitmapByteOrder32Host);
#endif
        if (bmap) {
            setCompositingOperation(bmap, Image::CompositeCopy);
            
            CGRect dst = { {0, 0}, {1, 1} };
            CGContextDrawImage(bmap, dst, image);
            if (pixel[3] > 0)
                m_solidColor = CGColorCreate(space,pixel);
            m_isSolidColor = true;
            CFRelease(bmap);
        } 
        CFRelease(space);
    }
}

CFDataRef ImageData::getTIFFRepresentation()
{
    if (m_tiffRep)
        return m_tiffRep;

    CGImageRef cgImage = frameAtIndex(0);
    if (!cgImage)
        return 0;
   
    CFMutableDataRef data = CFDataCreateMutable(0, 0);
    // FIXME:  Use type kCGImageTypeIdentifierTIFF constant once is becomes available in the API
    CGImageDestinationRef destination = CGImageDestinationCreateWithData(data, CFSTR("public.tiff"), 1, 0);
    if (destination) {
        CGImageDestinationAddImage(destination, cgImage, 0);
        CGImageDestinationFinalize(destination);
        CFRelease(destination);
    }

    m_tiffRep = data;
    return m_tiffRep;
}

NSImage* ImageData::getNSImage()
{
    if (m_nsImage)
        return m_nsImage;

    CFDataRef data = getTIFFRepresentation();
    if (!data)
        return 0;
    
    m_nsImage = [[NSImage alloc] initWithData:(NSData*)data];
    return m_nsImage;
}

}