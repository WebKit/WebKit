/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "config.h"

#if ENABLE(3D_CANVAS)

#include "GraphicsContext3D.h"

#include "Image.h"

#include <CoreGraphics/CGBitmapContext.h>
#include <CoreGraphics/CGContext.h>
#include <CoreGraphics/CGDataProvider.h>
#include <CoreGraphics/CGImage.h>

#include <wtf/RetainPtr.h>

namespace WebCore {

enum SourceDataFormatBase {
    SourceFormatBaseR = 0,
    SourceFormatBaseA,
    SourceFormatBaseRA,
    SourceFormatBaseAR,
    SourceFormatBaseRGB,
    SourceFormatBaseRGBA,
    SourceFormatBaseARGB,
    SourceFormatBaseNumFormats
};

enum AlphaFormat {
    AlphaFormatNone = 0,
    AlphaFormatFirst,
    AlphaFormatLast,
    AlphaFormatNumFormats
};

// This returns kSourceFormatNumFormats if the combination of input parameters is unsupported.
static GraphicsContext3D::SourceDataFormat getSourceDataFormat(unsigned int componentsPerPixel, AlphaFormat alphaFormat, bool is16BitFormat, bool bigEndian)
{
    const static SourceDataFormatBase formatTableBase[4][AlphaFormatNumFormats] = { // componentsPerPixel x AlphaFormat
        // AlphaFormatNone            AlphaFormatFirst            AlphaFormatLast
        { SourceFormatBaseR,          SourceFormatBaseA,          SourceFormatBaseA          }, // 1 componentsPerPixel
        { SourceFormatBaseNumFormats, SourceFormatBaseAR,         SourceFormatBaseRA         }, // 2 componentsPerPixel
        { SourceFormatBaseRGB,        SourceFormatBaseNumFormats, SourceFormatBaseNumFormats }, // 3 componentsPerPixel
        { SourceFormatBaseNumFormats, SourceFormatBaseARGB,       SourceFormatBaseRGBA        } // 4 componentsPerPixel
    };
    const static GraphicsContext3D::SourceDataFormat formatTable[SourceFormatBaseNumFormats][3] = { // SourceDataFormatBase x bitsPerComponentAndEndian
        // 8bits                                 16bits, little endian                         16bits, big endian
        { GraphicsContext3D::kSourceFormatR8,    GraphicsContext3D::kSourceFormatR16Little,    GraphicsContext3D::kSourceFormatR16Big },
        { GraphicsContext3D::kSourceFormatA8,    GraphicsContext3D::kSourceFormatA16Little,    GraphicsContext3D::kSourceFormatA16Big },
        { GraphicsContext3D::kSourceFormatRA8,   GraphicsContext3D::kSourceFormatRA16Little,   GraphicsContext3D::kSourceFormatRA16Big },
        { GraphicsContext3D::kSourceFormatAR8,   GraphicsContext3D::kSourceFormatAR16Little,   GraphicsContext3D::kSourceFormatAR16Big },
        { GraphicsContext3D::kSourceFormatRGB8,  GraphicsContext3D::kSourceFormatRGB16Little,  GraphicsContext3D::kSourceFormatRGB16Big },
        { GraphicsContext3D::kSourceFormatRGBA8, GraphicsContext3D::kSourceFormatRGBA16Little, GraphicsContext3D::kSourceFormatRGBA16Big },
        { GraphicsContext3D::kSourceFormatARGB8, GraphicsContext3D::kSourceFormatARGB16Little, GraphicsContext3D::kSourceFormatARGB16Big }
    };

    ASSERT(componentsPerPixel <= 4 && componentsPerPixel > 0);
    SourceDataFormatBase formatBase = formatTableBase[componentsPerPixel - 1][alphaFormat];
    if (formatBase == SourceFormatBaseNumFormats)
        return GraphicsContext3D::kSourceFormatNumFormats;
    if (!is16BitFormat)
        return formatTable[formatBase][0];
    if (!bigEndian)
        return formatTable[formatBase][1];
    return formatTable[formatBase][2];
}

bool GraphicsContext3D::getImageData(Image* image,
                                     unsigned int format,
                                     unsigned int type,
                                     bool premultiplyAlpha,
                                     Vector<uint8_t>& outputVector)
{
    if (!image)
        return false;
    CGImageRef cgImage;
    RetainPtr<CGImageRef> decodedImage;
    if (image->data()) {
        ImageSource decoder(false);
        decoder.setData(image->data(), true);
        if (!decoder.frameCount())
            return false;
        decodedImage = decoder.createFrameAtIndex(0);
        cgImage = decodedImage.get();
    } else
        cgImage = image->nativeImageForCurrentFrame();
    if (!cgImage)
        return false;

    size_t width = CGImageGetWidth(cgImage);
    size_t height = CGImageGetHeight(cgImage);
    if (!width || !height)
        return false;
    size_t bitsPerComponent = CGImageGetBitsPerComponent(cgImage);
    size_t bitsPerPixel = CGImageGetBitsPerPixel(cgImage);
    if (bitsPerComponent != 8 && bitsPerComponent != 16)
        return false;
    if (bitsPerPixel % bitsPerComponent)
        return false;
    size_t componentsPerPixel = bitsPerPixel / bitsPerComponent;

    bool srcByteOrder16Big = false;
    if (bitsPerComponent == 16) {
        CGBitmapInfo bitInfo = CGImageGetBitmapInfo(cgImage);
        switch (bitInfo & kCGBitmapByteOrderMask) {
        case kCGBitmapByteOrder16Big:
            srcByteOrder16Big = true;
            break;
        case kCGBitmapByteOrder16Little:
            srcByteOrder16Big = false;
            break;
        case kCGBitmapByteOrderDefault:
            // This is a bug in earlier version of cg where the default endian
            // is little whereas the decoded 16-bit png image data is actually
            // Big. Later version (10.6.4) no longer returns ByteOrderDefault.
            srcByteOrder16Big = true;
            break;
        default:
            return false;
        }
    }

    AlphaOp neededAlphaOp = kAlphaDoNothing;
    AlphaFormat alphaFormat = AlphaFormatNone;
    switch (CGImageGetAlphaInfo(cgImage)) {
    case kCGImageAlphaPremultipliedFirst:
        // This path is only accessible for MacOS earlier than 10.6.4.
        // This is a special case for texImage2D with HTMLCanvasElement input,
        // in which case image->data() should be null.
        ASSERT(!image->data());
        if (!premultiplyAlpha)
            neededAlphaOp = kAlphaDoUnmultiply;
        alphaFormat = AlphaFormatFirst;
        break;
    case kCGImageAlphaFirst:
        // This path is only accessible for MacOS earlier than 10.6.4.
        if (premultiplyAlpha)
            neededAlphaOp = kAlphaDoPremultiply;
        alphaFormat = AlphaFormatFirst;
        break;
    case kCGImageAlphaNoneSkipFirst:
        // This path is only accessible for MacOS earlier than 10.6.4.
        alphaFormat = AlphaFormatFirst;
        break;
    case kCGImageAlphaPremultipliedLast:
        // This is a special case for texImage2D with HTMLCanvasElement input,
        // in which case image->data() should be null.
        ASSERT(!image->data());
        if (!premultiplyAlpha)
            neededAlphaOp = kAlphaDoUnmultiply;
        alphaFormat = AlphaFormatLast;
        break;
    case kCGImageAlphaLast:
        if (premultiplyAlpha)
            neededAlphaOp = kAlphaDoPremultiply;
        alphaFormat = AlphaFormatLast;
        break;
    case kCGImageAlphaNoneSkipLast:
        alphaFormat = AlphaFormatLast;
        break;
    case kCGImageAlphaNone:
        alphaFormat = AlphaFormatNone;
        break;
    default:
        return false;
    }
    SourceDataFormat srcDataFormat = getSourceDataFormat(componentsPerPixel, alphaFormat, bitsPerComponent == 16, srcByteOrder16Big);
    if (srcDataFormat == kSourceFormatNumFormats)
        return false;

    RetainPtr<CFDataRef> pixelData;
    pixelData.adoptCF(CGDataProviderCopyData(CGImageGetDataProvider(cgImage)));
    if (!pixelData)
        return false;
    const UInt8* rgba = CFDataGetBytePtr(pixelData.get());
    outputVector.resize(width * height * 4);
    unsigned int srcUnpackAlignment = 0;
    size_t bytesPerRow = CGImageGetBytesPerRow(cgImage);
    unsigned int padding = bytesPerRow - bitsPerPixel / 8 * width;
    if (padding) {
        srcUnpackAlignment = padding + 1;
        while (bytesPerRow % srcUnpackAlignment)
            ++srcUnpackAlignment;
    }
    bool rt = packPixels(rgba, srcDataFormat, width, height, srcUnpackAlignment,
                         format, type, neededAlphaOp, outputVector.data());
    return rt;
}

void GraphicsContext3D::paintToCanvas(const unsigned char* imagePixels, int imageWidth, int imageHeight, int canvasWidth, int canvasHeight, CGContextRef context)
{
    if (!imagePixels || imageWidth <= 0 || imageHeight <= 0 || canvasWidth <= 0 || canvasHeight <= 0 || !context)
        return;
    int rowBytes = imageWidth * 4;
    RetainPtr<CGDataProviderRef> dataProvider(AdoptCF, CGDataProviderCreateWithData(0, imagePixels, rowBytes * imageHeight, 0));
    RetainPtr<CGColorSpaceRef> colorSpace(AdoptCF, CGColorSpaceCreateDeviceRGB());
    RetainPtr<CGImageRef> cgImage(AdoptCF, CGImageCreate(imageWidth, imageHeight, 8, 32, rowBytes, colorSpace.get(), kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host,
        dataProvider.get(), 0, false, kCGRenderingIntentDefault));
    // CSS styling may cause the canvas's content to be resized on
    // the page. Go back to the Canvas to figure out the correct
    // width and height to draw.
    CGRect rect = CGRectMake(0, 0, canvasWidth, canvasHeight);
    // We want to completely overwrite the previous frame's
    // rendering results.
    CGContextSaveGState(context);
    CGContextSetBlendMode(context, kCGBlendModeCopy);
    CGContextSetInterpolationQuality(context, kCGInterpolationNone);
    CGContextDrawImage(context, rect, cgImage.get());
    CGContextRestoreGState(context);
}

} // namespace WebCore

#endif // ENABLE(3D_CANVAS)
