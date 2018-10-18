/*
 * Copyright (C) 2011-2017 Apple Inc. All rights reserved.
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

#if USE(CG)

#include "GraphicsContext.h"
#include "IntRect.h"
#include <CoreGraphics/CoreGraphics.h>
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/TypedArrayInlines.h>
#include <JavaScriptCore/Uint8ClampedArray.h>
#include <wtf/Assertions.h>

#if USE(ACCELERATE)
#include <Accelerate/Accelerate.h>
#endif

#if USE(IOSURFACE_CANVAS_BACKING_STORE)
#include "IOSurface.h"
#include <dispatch/dispatch.h>
#include <pal/spi/cocoa/IOSurfaceSPI.h>
#endif

// CA uses ARGB32 for textures and ARGB32 -> ARGB32 resampling is optimized.
#define USE_ARGB32 PLATFORM(IOS_FAMILY)

namespace WebCore {

#if USE(ACCELERATE) && (USE_ARGB32 || USE(IOSURFACE_CANVAS_BACKING_STORE))
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
#endif // USE(ACCELERATE) && (USE_ARGB32 || USE(IOSURFACE_CANVAS_BACKING_STORE))

static inline void transferData(void* output, void* input, int width, int height, size_t inputBytesPerRow)
{
#if USE(ACCELERATE)
    ASSERT(input);
    ASSERT(output);

    vImage_Buffer src;
    src.width = width;
    src.height = height;
    src.rowBytes = inputBytesPerRow;
    src.data = input;

    vImage_Buffer dest;
    dest.width = width;
    dest.height = height;
    dest.rowBytes = width * 4;
    dest.data = output;

    vImageUnpremultiplyData_BGRA8888(&src, &dest, kvImageNoFlags);
#else
    UNUSED_PARAM(output);
    UNUSED_PARAM(input);
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
    // FIXME: Add support for not ACCELERATE.
    ASSERT_NOT_REACHED();
#endif
}

Vector<uint8_t> ImageBufferData::toBGRAData(bool accelerateRendering, int width, int height) const
{
    Vector<uint8_t> result(4 * width * height);

    if (!accelerateRendering) {
        transferData(result.data(), data, width, height, 4 * backingStoreSize.width());
        return result;
    }
#if USE(IOSURFACE_CANVAS_BACKING_STORE)
    IOSurface::Locker lock(*surface);
    transferData(result.data(), lock.surfaceBaseAddress(), width, height, surface->bytesPerRow());
#else
    ASSERT_NOT_REACHED();
#endif
    return result;
}

RefPtr<Uint8ClampedArray> ImageBufferData::getData(AlphaPremultiplication outputFormat, const IntRect& rect, const IntSize& size, bool accelerateRendering) const
{
    Checked<unsigned, RecordOverflow> area = 4;
    area *= rect.width();
    area *= rect.height();
    if (area.hasOverflowed())
        return nullptr;

    auto result = Uint8ClampedArray::tryCreateUninitialized(area.unsafeGet());
    uint8_t* resultData = result ? result->data() : nullptr;
    if (!resultData)
        return nullptr;

    Checked<int> endx = rect.maxX();
    Checked<int> endy = rect.maxY();
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
    destw = std::min<int>(destw.unsafeGet(), size.width() - originx);
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
    desth = std::min<int>(desth.unsafeGet(), size.height() - originy);
    if (endy.unsafeGet() > size.height())
        endy = size.height();
    Checked<int> height = endy - originy;
    
    if (width.unsafeGet() <= 0 || height.unsafeGet() <= 0)
        return result;
    
    unsigned destBytesPerRow = 4 * rect.width();
    uint8_t* destRows = resultData + desty * destBytesPerRow + destx * 4;
    
    unsigned srcBytesPerRow;
    uint8_t* srcRows;
    
    if (!accelerateRendering) {
        if (!data)
            return result;

        srcBytesPerRow = bytesPerRow.unsafeGet();
        srcRows = reinterpret_cast<uint8_t*>(data) + originy * srcBytesPerRow + originx * 4;

#if USE(ACCELERATE)
        if (outputFormat == AlphaPremultiplication::Unpremultiplied) {

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
            vImageUnpremultiplyData_RGBA8888(&src, &dest, kvImageNoFlags);
#endif

            return result;
        }
#endif
        if (outputFormat == AlphaPremultiplication::Unpremultiplied) {
            if ((width * 4).hasOverflowed())
                CRASH();
            for (int y = 0; y < height.unsafeGet(); ++y) {
                for (int x = 0; x < width.unsafeGet(); x++) {
                    int basex = x * 4;
                    uint8_t alpha = srcRows[basex + 3];
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
                        reinterpret_cast<uint32_t*>(destRows + basex)[0] = reinterpret_cast<const uint32_t*>(srcRows + basex)[0];
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
        IOSurface::Locker lock(*surface);
        srcBytesPerRow = surface->bytesPerRow();
        srcRows = static_cast<uint8_t*>(lock.surfaceBaseAddress()) + originy * srcBytesPerRow + originx * 4;

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

        if (outputFormat == AlphaPremultiplication::Unpremultiplied)
            unpremultiplyBufferData(src, dest);
        else {
            // Swap pixel channels from BGRA to RGBA.
            const uint8_t map[4] = { 2, 1, 0, 3 };
            vImagePermuteChannels_ARGB8888(&src, &dest, map, kvImageNoFlags);
        }
#else
        if ((width * 4).hasOverflowed())
            CRASH();

        if (outputFormat == AlphaPremultiplication::Unpremultiplied) {
            for (int y = 0; y < height.unsafeGet(); ++y) {
                for (int x = 0; x < width.unsafeGet(); x++) {
                    int basex = x * 4;
                    uint8_t b = srcRows[basex];
                    uint8_t alpha = srcRows[basex + 3];
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
                    uint8_t b = srcRows[basex];
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
#else
        ASSERT_NOT_REACHED();
#endif // USE(IOSURFACE_CANVAS_BACKING_STORE)
    }
    
    return result;
}

void ImageBufferData::putData(const Uint8ClampedArray& source, AlphaPremultiplication sourceFormat, const IntSize& sourceSize, const IntRect& sourceRect, const IntPoint& destPoint, const IntSize& size, bool accelerateRendering)
{
#if ASSERT_DISABLED
    UNUSED_PARAM(size);
#endif

    ASSERT(sourceRect.width() > 0);
    ASSERT(sourceRect.height() > 0);
    
    Checked<int> originx = sourceRect.x();
    Checked<int> destx = (Checked<int>(destPoint.x()) + sourceRect.x());
    ASSERT(destx.unsafeGet() >= 0);
    ASSERT(destx.unsafeGet() < size.width());
    ASSERT(originx.unsafeGet() >= 0);
    ASSERT(originx.unsafeGet() <= sourceRect.maxX());
    
    Checked<int> endx = (Checked<int>(destPoint.x()) + sourceRect.maxX());
    ASSERT(endx.unsafeGet() <= size.width());
    
    Checked<int> width = sourceRect.width();
    Checked<int> destw = endx - destx;

    Checked<int> originy = sourceRect.y();
    Checked<int> desty = (Checked<int>(destPoint.y()) + sourceRect.y());
    ASSERT(desty.unsafeGet() >= 0);
    ASSERT(desty.unsafeGet() < size.height());
    ASSERT(originy.unsafeGet() >= 0);
    ASSERT(originy.unsafeGet() <= sourceRect.maxY());
    
    Checked<int> endy = (Checked<int>(destPoint.y()) + sourceRect.maxY());
    ASSERT(endy.unsafeGet() <= size.height());

    Checked<int> height = sourceRect.height();
    Checked<int> desth = endy - desty;
    
    if (width <= 0 || height <= 0)
        return;
    
    unsigned srcBytesPerRow = 4 * sourceSize.width();
    const uint8_t* srcRows = source.data() + (originy * srcBytesPerRow + originx * 4).unsafeGet();
    unsigned destBytesPerRow;
    uint8_t* destRows;
    
    if (!accelerateRendering) {
        if (!data)
            return;

        destBytesPerRow = bytesPerRow.unsafeGet();
        destRows = reinterpret_cast<uint8_t*>(data) + (desty * destBytesPerRow + destx * 4).unsafeGet();

#if USE(ACCELERATE)
        if (sourceFormat == AlphaPremultiplication::Unpremultiplied) {

            vImage_Buffer src;
            src.width = width.unsafeGet();
            src.height = height.unsafeGet();
            src.rowBytes = srcBytesPerRow;
            src.data = const_cast<uint8_t*>(srcRows);

            vImage_Buffer dest;
            dest.width = destw.unsafeGet();
            dest.height = desth.unsafeGet();
            dest.rowBytes = destBytesPerRow;
            dest.data = destRows;

#if USE_ARGB32
            premultiplyBufferData(src, dest);
#else
            vImagePremultiplyData_RGBA8888(&src, &dest, kvImageNoFlags);
#endif
            return;
        }
#endif

        for (int y = 0; y < height.unsafeGet(); ++y) {
            for (int x = 0; x < width.unsafeGet(); x++) {
                int basex = x * 4;
                uint8_t alpha = srcRows[basex + 3];
#if USE_ARGB32
                // Byte order is different as we use image buffers of ARGB32
                if (sourceFormat == AlphaPremultiplication::Unpremultiplied && alpha != 255) {
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
                if (sourceFormat == AlphaPremultiplication::Unpremultiplied && alpha != 255) {
                    destRows[basex] = (srcRows[basex] * alpha + 254) / 255;
                    destRows[basex + 1] = (srcRows[basex + 1] * alpha + 254) / 255;
                    destRows[basex + 2] = (srcRows[basex + 2] * alpha + 254) / 255;
                    destRows[basex + 3] = alpha;
                } else
                    reinterpret_cast<uint32_t*>(destRows + basex)[0] = reinterpret_cast<const uint32_t*>(srcRows + basex)[0];
#endif
            }
            destRows += destBytesPerRow;
            srcRows += srcBytesPerRow;
        }
    } else {
#if USE(IOSURFACE_CANVAS_BACKING_STORE)
        IOSurface::Locker lock(*surface, IOSurface::Locker::AccessMode::ReadWrite);
        destBytesPerRow = surface->bytesPerRow();
        destRows = static_cast<uint8_t*>(lock.surfaceBaseAddress()) + (desty * destBytesPerRow + destx * 4).unsafeGet();

#if USE(ACCELERATE)
        vImage_Buffer src;
        src.width = width.unsafeGet();
        src.height = height.unsafeGet();
        src.rowBytes = srcBytesPerRow;
        src.data = const_cast<uint8_t*>(srcRows);

        vImage_Buffer dest;
        dest.width = destw.unsafeGet();
        dest.height = desth.unsafeGet();
        dest.rowBytes = destBytesPerRow;
        dest.data = destRows;

        if (sourceFormat == AlphaPremultiplication::Unpremultiplied)
            premultiplyBufferData(src, dest);
        else {
            // Swap pixel channels from RGBA to BGRA.
            const uint8_t map[4] = { 2, 1, 0, 3 };
            vImagePermuteChannels_ARGB8888(&src, &dest, map, kvImageNoFlags);
        }
#else
        for (int y = 0; y < height.unsafeGet(); ++y) {
            for (int x = 0; x < width.unsafeGet(); x++) {
                int basex = x * 4;
                uint8_t b = srcRows[basex];
                uint8_t alpha = srcRows[basex + 3];
                if (sourceFormat == AlphaPremultiplication::Unpremultiplied && alpha != 255) {
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
#else
        ASSERT_NOT_REACHED();
#endif // USE(IOSURFACE_CANVAS_BACKING_STORE)
    }
}

} // namespace WebCore

#endif
