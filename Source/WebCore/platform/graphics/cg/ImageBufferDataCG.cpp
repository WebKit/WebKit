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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "ImageBufferData.h"

#include "GraphicsContext.h"
#include "IntRect.h"
#include <CoreGraphics/CoreGraphics.h>
#include <runtime/JSCInlines.h>
#include <runtime/TypedArrayInlines.h>
#include <runtime/Uint8ClampedArray.h>
#include <wtf/Assertions.h>

#if USE(ACCELERATE)
#include <Accelerate/Accelerate.h>
#endif

#if USE(IOSURFACE_CANVAS_BACKING_STORE)
#include "IOSurface.h"
#include "IOSurfaceSPI.h"
#include <dispatch/dispatch.h>
#endif

// CA uses ARGB32 for textures and ARGB32 -> ARGB32 resampling is optimized.
#define USE_ARGB32 PLATFORM(IOS)

namespace WebCore {

#if USE(ACCELERATE)
#if USE_ARGB32 || USE(IOSURFACE_CANVAS_BACKING_STORE)
static void unpremultiplyBufferData(const vImage_Buffer& src, const vImage_Buffer& dest)
{
    ASSERT(src.data);
    ASSERT(dest.data);

    if (kvImageNoError != vImageUnpremultiplyData_RGBA8888(&src, &dest, kvImageNoFlags))
        return;

    // Swap channels 1 and 3, to convert BGRA<->RGBA. IOSurfaces are BGRA, ImageData expects RGBA.
    const uint8_t map[4] = { 2, 1, 0, 3 };
    vImagePermuteChannels_ARGB8888(&dest, &dest, map, kvImageNoFlags);
}

static void premultiplyBufferData(const vImage_Buffer& src, const vImage_Buffer& dest)
{
    ASSERT(src.data);
    ASSERT(dest.data);

    if (kvImageNoError != vImagePremultiplyData_RGBA8888(&src, &dest, kvImageNoFlags))
        return;

    // Swap channels 1 and 3, to convert BGRA<->RGBA. IOSurfaces are BGRA, ImageData expects RGBA.
    const uint8_t map[4] = { 2, 1, 0, 3 };
    vImagePermuteChannels_ARGB8888(&dest, &dest, map, kvImageNoFlags);
}
#endif // USE_ARGB32 || USE(IOSURFACE_CANVAS_BACKING_STORE)

#if !PLATFORM(IOS_SIMULATOR)
static void affineWarpBufferData(const vImage_Buffer& src, const vImage_Buffer& dest, float scale)
{
    vImage_AffineTransform scaleTransform = { scale, 0, 0, scale, 0, 0 }; // FIXME: Add subpixel translation.
    Pixel_8888 backgroundColor;
    vImageAffineWarp_ARGB8888(&src, &dest, 0, &scaleTransform, backgroundColor, kvImageEdgeExtend);
}
#endif // !PLATFORM(IOS_SIMULATOR)
#endif // USE(ACCELERATE)

RefPtr<Uint8ClampedArray> ImageBufferData::getData(const IntRect& rect, const IntSize& size, bool accelerateRendering, bool unmultiplied, float resolutionScale) const
{
    Checked<unsigned, RecordOverflow> area = 4;
    area *= rect.width();
    area *= rect.height();
    if (area.hasOverflowed())
        return nullptr;

    RefPtr<Uint8ClampedArray> result = Uint8ClampedArray::createUninitialized(area.unsafeGet());
    unsigned char* resultData = result->data();
    if (!resultData) {
        WTFLogAlways("ImageBufferData: Unable to create buffer. Requested size was %d x %d = %u\n", rect.width(), rect.height(), area.unsafeGet());
        return nullptr;
    }

    Checked<int> endx = rect.maxX();
    endx *= ceilf(resolutionScale);
    Checked<int> endy = rect.maxY();
    endy *= resolutionScale;
    if (rect.x() < 0 || rect.y() < 0 || endx.unsafeGet() > size.width() || endy.unsafeGet() > size.height())
        result->zeroFill();
    
    int originx = rect.x();
    int destx = 0;
    Checked<int> destw = rect.width();
    if (originx < 0) {
        destw += originx;
        destx = -originx;
        originx = 0;
    }
    destw = std::min<int>(destw.unsafeGet(), ceilf(size.width() / resolutionScale) - originx);
    originx *= resolutionScale;
    if (endx.unsafeGet() > size.width())
        endx = size.width();
    Checked<int> width = endx - originx;
    
    int originy = rect.y();
    int desty = 0;
    Checked<int> desth = rect.height();
    if (originy < 0) {
        desth += originy;
        desty = -originy;
        originy = 0;
    }
    desth = std::min<int>(desth.unsafeGet(), ceilf(size.height() / resolutionScale) - originy);
    originy *= resolutionScale;
    if (endy.unsafeGet() > size.height())
        endy = size.height();
    Checked<int> height = endy - originy;
    
    if (width.unsafeGet() <= 0 || height.unsafeGet() <= 0)
        return result.release();
    
    unsigned destBytesPerRow = 4 * rect.width();
    unsigned char* destRows = resultData + desty * destBytesPerRow + destx * 4;
    
    unsigned srcBytesPerRow;
    unsigned char* srcRows;
    
    if (!accelerateRendering) {
        srcBytesPerRow = bytesPerRow.unsafeGet();
        srcRows = reinterpret_cast<unsigned char*>(data) + originy * srcBytesPerRow + originx * 4;

#if USE(ACCELERATE)
        if (unmultiplied) {

            vImage_Buffer src;
            src.width = width.unsafeGet();
            src.height = height.unsafeGet();
            src.rowBytes = srcBytesPerRow;
            src.data = srcRows;

            vImage_Buffer dest;
            dest.width = destw.unsafeGet();
            dest.height = desth.unsafeGet();
            dest.rowBytes = destBytesPerRow;
            dest.data = destRows;

#if USE_ARGB32
            unpremultiplyBufferData(src, dest);
#else
            if (resolutionScale != 1) {
                affineWarpBufferData(src, dest, 1 / resolutionScale);
                // The unpremultiplying will be done in-place.
                src = dest;
            }

            vImageUnpremultiplyData_RGBA8888(&src, &dest, kvImageNoFlags);
#endif

            return result;
        }
#endif
        if (resolutionScale != 1) {
            RetainPtr<CGContextRef> sourceContext = adoptCF(CGBitmapContextCreate(srcRows, width.unsafeGet(), height.unsafeGet(), 8, srcBytesPerRow, colorSpace, kCGImageAlphaPremultipliedLast));
            RetainPtr<CGImageRef> sourceImage = adoptCF(CGBitmapContextCreateImage(sourceContext.get()));
            RetainPtr<CGContextRef> destinationContext = adoptCF(CGBitmapContextCreate(destRows, destw.unsafeGet(), desth.unsafeGet(), 8, destBytesPerRow, colorSpace, kCGImageAlphaPremultipliedLast));
            CGContextSetBlendMode(destinationContext.get(), kCGBlendModeCopy);
            CGContextDrawImage(destinationContext.get(), CGRectMake(0, 0, width.unsafeGet() / resolutionScale, height.unsafeGet() / resolutionScale), sourceImage.get()); // FIXME: Add subpixel translation.
            if (!unmultiplied)
                return result;

            srcRows = destRows;
            srcBytesPerRow = destBytesPerRow;
            width = destw;
            height = desth;
        }
        if (unmultiplied) {
            if ((width * 4).hasOverflowed())
                CRASH();
            for (int y = 0; y < height.unsafeGet(); ++y) {
                for (int x = 0; x < width.unsafeGet(); x++) {
                    int basex = x * 4;
                    unsigned char alpha = srcRows[basex + 3];
#if USE_ARGB32
                    // Byte order is different as we use image buffers of ARGB32
                    if (alpha) {
                        destRows[basex] = (srcRows[basex + 2] * 255) / alpha;
                        destRows[basex + 1] = (srcRows[basex + 1] * 255) / alpha;
                        destRows[basex + 2] = (srcRows[basex] * 255) / alpha;
                        destRows[basex + 3] = alpha;
                    } else {
                        destRows[basex] = srcRows[basex + 2];
                        destRows[basex + 1] = srcRows[basex + 1];
                        destRows[basex + 2] = srcRows[basex];
                        destRows[basex + 3] = alpha;
                    }
#else
                    if (alpha) {
                        destRows[basex] = (srcRows[basex] * 255) / alpha;
                        destRows[basex + 1] = (srcRows[basex + 1] * 255) / alpha;
                        destRows[basex + 2] = (srcRows[basex + 2] * 255) / alpha;
                        destRows[basex + 3] = alpha;
                    } else
                        reinterpret_cast<uint32_t*>(destRows + basex)[0] = reinterpret_cast<uint32_t*>(srcRows + basex)[0];
#endif
                }
                srcRows += srcBytesPerRow;
                destRows += destBytesPerRow;
            }
        } else {
            for (int y = 0; y < height.unsafeGet(); ++y) {
#if USE_ARGB32
                for (int x = 0; x < width.unsafeGet(); x++) {
                    int basex = x * 4;
                    destRows[basex] = srcRows[basex + 2];
                    destRows[basex + 1] = srcRows[basex + 1];
                    destRows[basex + 2] = srcRows[basex];
                    destRows[basex + 3] = srcRows[basex + 3];
                }
#else
                for (int x = 0; x < (width * 4).unsafeGet(); x += 4)
                    reinterpret_cast<uint32_t*>(destRows + x)[0] = reinterpret_cast<uint32_t*>(srcRows + x)[0];
#endif
                srcRows += srcBytesPerRow;
                destRows += destBytesPerRow;
            }
        }
    } else {
#if USE(IOSURFACE_CANVAS_BACKING_STORE)
        // FIXME: WebCore::IOSurface should have a locking RAII object and base-address getter.
        IOSurfaceRef surfaceRef = surface->surface();
        IOSurfaceLock(surfaceRef, kIOSurfaceLockReadOnly, nullptr);
        srcBytesPerRow = IOSurfaceGetBytesPerRow(surfaceRef);
        srcRows = static_cast<unsigned char*>(IOSurfaceGetBaseAddress(surfaceRef)) + originy * srcBytesPerRow + originx * 4;

#if USE(ACCELERATE)

        vImage_Buffer src;
        src.width = width.unsafeGet();
        src.height = height.unsafeGet();
        src.rowBytes = srcBytesPerRow;
        src.data = srcRows;

        vImage_Buffer dest;
        dest.width = destw.unsafeGet();
        dest.height = desth.unsafeGet();
        dest.rowBytes = destBytesPerRow;
        dest.data = destRows;

        if (resolutionScale != 1) {
            affineWarpBufferData(src, dest, 1 / resolutionScale);
            // The unpremultiplying and channel-swapping will be done in-place.
            src = dest;
        }

        if (unmultiplied)
            unpremultiplyBufferData(src, dest);
        else {
            // Swap pixel channels from BGRA to RGBA.
            const uint8_t map[4] = { 2, 1, 0, 3 };
            vImagePermuteChannels_ARGB8888(&src, &dest, map, kvImageNoFlags);
        }
#else
        if (resolutionScale != 1) {
            RetainPtr<CGContextRef> sourceContext = adoptCF(CGBitmapContextCreate(srcRows, width.unsafeGet(), height.unsafeGet(), 8, srcBytesPerRow, colorSpace, kCGImageAlphaPremultipliedLast));
            RetainPtr<CGImageRef> sourceImage = adoptCF(CGBitmapContextCreateImage(sourceContext.get()));
            RetainPtr<CGContextRef> destinationContext = adoptCF(CGBitmapContextCreate(destRows, destw.unsafeGet(), desth.unsafeGet(), 8, destBytesPerRow, colorSpace, kCGImageAlphaPremultipliedLast));
            CGContextSetBlendMode(destinationContext.get(), kCGBlendModeCopy);
            CGContextDrawImage(destinationContext.get(), CGRectMake(0, 0, width.unsafeGet() / resolutionScale, height.unsafeGet() / resolutionScale), sourceImage.get()); // FIXME: Add subpixel translation.

            srcRows = destRows;
            srcBytesPerRow = destBytesPerRow;
            width = destw;
            height = desth;
        }
        
        if ((width * 4).hasOverflowed())
            CRASH();

        if (unmultiplied) {
            for (int y = 0; y < height.unsafeGet(); ++y) {
                for (int x = 0; x < width.unsafeGet(); x++) {
                    int basex = x * 4;
                    unsigned char b = srcRows[basex];
                    unsigned char alpha = srcRows[basex + 3];
                    if (alpha) {
                        destRows[basex] = (srcRows[basex + 2] * 255) / alpha;
                        destRows[basex + 1] = (srcRows[basex + 1] * 255) / alpha;
                        destRows[basex + 2] = (b * 255) / alpha;
                        destRows[basex + 3] = alpha;
                    } else {
                        destRows[basex] = srcRows[basex + 2];
                        destRows[basex + 1] = srcRows[basex + 1];
                        destRows[basex + 2] = b;
                        destRows[basex + 3] = srcRows[basex + 3];
                    }
                }
                srcRows += srcBytesPerRow;
                destRows += destBytesPerRow;
            }
        } else {
            for (int y = 0; y < height.unsafeGet(); ++y) {
                for (int x = 0; x < width.unsafeGet(); x++) {
                    int basex = x * 4;
                    unsigned char b = srcRows[basex];
                    destRows[basex] = srcRows[basex + 2];
                    destRows[basex + 1] = srcRows[basex + 1];
                    destRows[basex + 2] = b;
                    destRows[basex + 3] = srcRows[basex + 3];
                }
                srcRows += srcBytesPerRow;
                destRows += destBytesPerRow;
            }
        }
#endif // USE(ACCELERATE)
        IOSurfaceUnlock(surfaceRef, kIOSurfaceLockReadOnly, nullptr);
#else
        ASSERT_NOT_REACHED();
#endif // USE(IOSURFACE_CANVAS_BACKING_STORE)
    }
    
    return result;
}

void ImageBufferData::putData(Uint8ClampedArray*& source, const IntSize& sourceSize, const IntRect& sourceRect, const IntPoint& destPoint, const IntSize& size, bool accelerateRendering, bool unmultiplied, float resolutionScale)
{
#if ASSERT_DISABLED
    UNUSED_PARAM(size);
#endif

    ASSERT(sourceRect.width() > 0);
    ASSERT(sourceRect.height() > 0);
    
    Checked<int> originx = sourceRect.x();
    Checked<int> destx = (Checked<int>(destPoint.x()) + sourceRect.x());
    destx *= resolutionScale;
    ASSERT(destx.unsafeGet() >= 0);
    ASSERT(destx.unsafeGet() < size.width());
    ASSERT(originx.unsafeGet() >= 0);
    ASSERT(originx.unsafeGet() <= sourceRect.maxX());
    
    Checked<int> endx = (Checked<int>(destPoint.x()) + sourceRect.maxX());
    endx *= resolutionScale;
    ASSERT(endx.unsafeGet() <= size.width());
    
    Checked<int> width = sourceRect.width();
    Checked<int> destw = endx - destx;

    Checked<int> originy = sourceRect.y();
    Checked<int> desty = (Checked<int>(destPoint.y()) + sourceRect.y());
    desty *= resolutionScale;
    ASSERT(desty.unsafeGet() >= 0);
    ASSERT(desty.unsafeGet() < size.height());
    ASSERT(originy.unsafeGet() >= 0);
    ASSERT(originy.unsafeGet() <= sourceRect.maxY());
    
    Checked<int> endy = (Checked<int>(destPoint.y()) + sourceRect.maxY());
    endy *= resolutionScale;
    ASSERT(endy.unsafeGet() <= size.height());

    Checked<int> height = sourceRect.height();
    Checked<int> desth = endy - desty;
    
    if (width <= 0 || height <= 0)
        return;
    
    unsigned srcBytesPerRow = 4 * sourceSize.width();
    unsigned char* srcRows = source->data() + (originy * srcBytesPerRow + originx * 4).unsafeGet();
    unsigned destBytesPerRow;
    unsigned char* destRows;
    
    if (!accelerateRendering) {
        destBytesPerRow = bytesPerRow.unsafeGet();
        destRows = reinterpret_cast<unsigned char*>(data) + (desty * destBytesPerRow + destx * 4).unsafeGet();

#if USE(ACCELERATE)
        if (unmultiplied) {

            vImage_Buffer src;
            src.width = width.unsafeGet();
            src.height = height.unsafeGet();
            src.rowBytes = srcBytesPerRow;
            src.data = srcRows;

            vImage_Buffer dest;
            dest.width = destw.unsafeGet();
            dest.height = desth.unsafeGet();
            dest.rowBytes = destBytesPerRow;
            dest.data = destRows;

#if USE_ARGB32
            premultiplyBufferData(src, dest);
#else
            if (resolutionScale != 1) {
                affineWarpBufferData(src, dest, resolutionScale);
                // The premultiplying will be done in-place.
                src = dest;
            }

            vImagePremultiplyData_RGBA8888(&src, &dest, kvImageNoFlags);
#endif
            return;
        }
#endif
        if (resolutionScale != 1) {
            RetainPtr<CGContextRef> sourceContext = adoptCF(CGBitmapContextCreate(srcRows, width.unsafeGet(), height.unsafeGet(), 8, srcBytesPerRow, colorSpace, kCGImageAlphaPremultipliedLast));
            RetainPtr<CGImageRef> sourceImage = adoptCF(CGBitmapContextCreateImage(sourceContext.get()));
            RetainPtr<CGContextRef> destinationContext = adoptCF(CGBitmapContextCreate(destRows, destw.unsafeGet(), desth.unsafeGet(), 8, destBytesPerRow, colorSpace, kCGImageAlphaPremultipliedLast));
            CGContextSetBlendMode(destinationContext.get(), kCGBlendModeCopy);
            CGContextDrawImage(destinationContext.get(), CGRectMake(0, 0, width.unsafeGet() / resolutionScale, height.unsafeGet() / resolutionScale), sourceImage.get()); // FIXME: Add subpixel translation.
            if (!unmultiplied)
                return;

            // The premultiplying will be done in-place.
            srcRows = destRows;
            srcBytesPerRow = destBytesPerRow;
            width = destw;
            height = desth;
        }

        for (int y = 0; y < height.unsafeGet(); ++y) {
            for (int x = 0; x < width.unsafeGet(); x++) {
                int basex = x * 4;
                unsigned char alpha = srcRows[basex + 3];
#if USE_ARGB32
                // Byte order is different as we use image buffers of ARGB32
                if (unmultiplied && alpha != 255) {
                    destRows[basex] = (srcRows[basex + 2] * alpha + 254) / 255;
                    destRows[basex + 1] = (srcRows[basex + 1] * alpha + 254) / 255;
                    destRows[basex + 2] = (srcRows[basex + 0] * alpha + 254) / 255;
                    destRows[basex + 3] = alpha;
                } else {
                    destRows[basex] = srcRows[basex + 2];
                    destRows[basex + 1] = srcRows[basex + 1];
                    destRows[basex + 2] = srcRows[basex];
                    destRows[basex + 3] = alpha;
                }
#else
                if (unmultiplied && alpha != 255) {
                    destRows[basex] = (srcRows[basex] * alpha + 254) / 255;
                    destRows[basex + 1] = (srcRows[basex + 1] * alpha + 254) / 255;
                    destRows[basex + 2] = (srcRows[basex + 2] * alpha + 254) / 255;
                    destRows[basex + 3] = alpha;
                } else
                    reinterpret_cast<uint32_t*>(destRows + basex)[0] = reinterpret_cast<uint32_t*>(srcRows + basex)[0];
#endif
            }
            destRows += destBytesPerRow;
            srcRows += srcBytesPerRow;
        }
    } else {
#if USE(IOSURFACE_CANVAS_BACKING_STORE)
        IOSurfaceRef surfaceRef = surface->surface();
        IOSurfaceLock(surfaceRef, 0, nullptr);
        destBytesPerRow = IOSurfaceGetBytesPerRow(surfaceRef);
        destRows = static_cast<unsigned char*>(IOSurfaceGetBaseAddress(surfaceRef)) + (desty * destBytesPerRow + destx * 4).unsafeGet();

#if USE(ACCELERATE)
        vImage_Buffer src;
        src.width = width.unsafeGet();
        src.height = height.unsafeGet();
        src.rowBytes = srcBytesPerRow;
        src.data = srcRows;

        vImage_Buffer dest;
        dest.width = destw.unsafeGet();
        dest.height = desth.unsafeGet();
        dest.rowBytes = destBytesPerRow;
        dest.data = destRows;

        if (resolutionScale != 1) {
            affineWarpBufferData(src, dest, resolutionScale);
            // The unpremultiplying and channel-swapping will be done in-place.
            src = dest;
        }

        if (unmultiplied)
            premultiplyBufferData(src, dest);
        else {
            // Swap pixel channels from RGBA to BGRA.
            const uint8_t map[4] = { 2, 1, 0, 3 };
            vImagePermuteChannels_ARGB8888(&src, &dest, map, kvImageNoFlags);
        }
#else
        if (resolutionScale != 1) {
            RetainPtr<CGContextRef> sourceContext = adoptCF(CGBitmapContextCreate(srcRows, width.unsafeGet(), height.unsafeGet(), 8, srcBytesPerRow, colorSpace, kCGImageAlphaPremultipliedLast));
            RetainPtr<CGImageRef> sourceImage = adoptCF(CGBitmapContextCreateImage(sourceContext.get()));
            RetainPtr<CGContextRef> destinationContext = adoptCF(CGBitmapContextCreate(destRows, destw.unsafeGet(), desth.unsafeGet(), 8, destBytesPerRow, colorSpace, kCGImageAlphaPremultipliedLast));
            CGContextSetBlendMode(destinationContext.get(), kCGBlendModeCopy);
            CGContextDrawImage(destinationContext.get(), CGRectMake(0, 0, width.unsafeGet() / resolutionScale, height.unsafeGet() / resolutionScale), sourceImage.get()); // FIXME: Add subpixel translation.

            srcRows = destRows;
            srcBytesPerRow = destBytesPerRow;
            width = destw;
            height = desth;
        }

        for (int y = 0; y < height.unsafeGet(); ++y) {
            for (int x = 0; x < width.unsafeGet(); x++) {
                int basex = x * 4;
                unsigned char b = srcRows[basex];
                unsigned char alpha = srcRows[basex + 3];
                if (unmultiplied && alpha != 255) {
                    destRows[basex] = (srcRows[basex + 2] * alpha + 254) / 255;
                    destRows[basex + 1] = (srcRows[basex + 1] * alpha + 254) / 255;
                    destRows[basex + 2] = (b * alpha + 254) / 255;
                    destRows[basex + 3] = alpha;
                } else {
                    destRows[basex] = srcRows[basex + 2];
                    destRows[basex + 1] = srcRows[basex + 1];
                    destRows[basex + 2] = b;
                    destRows[basex + 3] = alpha;
                }
            }
            destRows += destBytesPerRow;
            srcRows += srcBytesPerRow;
        }
#endif // USE(ACCELERATE)

        IOSurfaceUnlock(surfaceRef, 0, nullptr);
#else
        ASSERT_NOT_REACHED();
#endif // USE(IOSURFACE_CANVAS_BACKING_STORE)
    }
}

} // namespace WebCore
