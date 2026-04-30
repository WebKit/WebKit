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

#include "config.h"
#include "CVPixelBufferUtilities.h"

#if PLATFORM(COCOA)

#include "CVUtilities.h"
#include "Logging.h"
#include "ShareableCVPixelBuffer.h"
#include <pal/spi/cg/ImageIOSPI.h>
#include <wtf/FileHandle.h>
#include <wtf/FileSystem.h>
#include <wtf/text/StringBuilder.h>

#include "CoreVideoSoftLink.h"

namespace WebCore {

RetainPtr<CVPixelBufferRef> createScratchCVPixelBuffer(unsigned width, unsigned height, OSType pixelFormat, CFDictionaryRef attributes, CGColorSpaceRef colorSpace)
{
    CVPixelBufferRef pixelBuffer = nullptr;
    auto status = CVPixelBufferCreate(kCFAllocatorDefault, width, height, pixelFormat, attributes, &pixelBuffer);

    if (status != kCVReturnSuccess) {
        RELEASE_LOG_ERROR(Images, "%s: CVPixelBufferCreate() failed with status=%d", __FUNCTION__, static_cast<int>(status));
        return nullptr;
    }

    if (colorSpace)
        CVBufferSetAttachment(pixelBuffer, kCVImageBufferCGColorSpaceKey, colorSpace, kCVAttachmentMode_ShouldPropagate);

    return adoptCF(pixelBuffer);
}

RetainPtr<CVPixelBufferRef> createScratchMetalCompatibleCVPixelBuffer(unsigned width, unsigned height, OSType pixelFormat, CGColorSpaceRef colorSpace)
{
    RetainPtr attributes = adoptCF(CFDictionaryCreateMutable(nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    RetainPtr surfaceProperties = adoptCF(CFDictionaryCreateMutable(nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    CFDictionarySetValue(attributes, kCVPixelBufferIOSurfacePropertiesKey, surfaceProperties);
    CFDictionarySetValue(attributes, kCVPixelBufferMetalCompatibilityKey, kCFBooleanTrue);
    return createScratchCVPixelBuffer(width, height, pixelFormat, attributes, colorSpace);
}

RetainPtr<CVPixelBufferRef> createScratchMetalCompatibleCVPixelBuffer(CVPixelBufferRef pixelBuffer, CGColorSpaceRef colorSpace)
{
    unsigned width = CVPixelBufferGetWidth(pixelBuffer);
    unsigned height = CVPixelBufferGetHeight(pixelBuffer);
    OSType pixelFormat = CVPixelBufferGetPixelFormatType(pixelBuffer);
    return createScratchMetalCompatibleCVPixelBuffer(width, height, pixelFormat, colorSpace);
}

RetainPtr<CVPixelBufferRef> createScratchMetalCompatibleCVPixelBuffer(const ShareableCVPixelBuffer& pixelBuffer, CGColorSpaceRef colorSpace)
{
    unsigned width = pixelBuffer.configuration().width();
    unsigned height = pixelBuffer.configuration().height();
    OSType pixelFormat = toCVPixelFormat(pixelBuffer.configuration().pixelFormat());
    return createScratchMetalCompatibleCVPixelBuffer(width, height, pixelFormat, colorSpace);
}

RetainPtr<CVPixelBufferRef> createMetalCompatibleCVPixelBufferFromImage(PlatformImagePtr platformImage)
{
    unsigned width = CGImageGetWidth(platformImage);
    unsigned height = CGImageGetHeight(platformImage);
    RetainPtr colorSpace = CGImageGetColorSpace(platformImage);

    RetainPtr pixelBuffer = createScratchMetalCompatibleCVPixelBuffer(width, height, kCVPixelFormatType_32BGRA);
    if (!pixelBuffer)
        return nullptr;

    CVPixelBufferLockBaseAddress(pixelBuffer, 0);
    {
        unsigned destinationBytesPerRow = CVPixelBufferGetBytesPerRow(pixelBuffer);
        auto* destinationBaseAddress = static_cast<uint8_t*>(CVPixelBufferGetBaseAddress(pixelBuffer));

        // kCGBitmapByteOrder32Little | kCGImageAlphaPremultipliedFirst => BGRA layout
        auto bitmapInfo = static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Little) | static_cast<CGBitmapInfo>(kCGImageAlphaPremultipliedFirst);

        RetainPtr context = adoptCF(CGBitmapContextCreate(destinationBaseAddress, width, height, 8, destinationBytesPerRow, colorSpace, bitmapInfo));
        CGContextDrawImage(context, CGRectMake(0, 0, static_cast<CGFloat>(width), static_cast<CGFloat>(height)), platformImage);
    }
    CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);

    return WTF::move(pixelBuffer);
}

#if ENABLE(DUMP_GAIN_MAP_IMAGES)
void CVPixelBufferDumpToFile(CVPixelBufferRef pixelBuffer, const String& name)
{
    if (!pixelBuffer || name.length() < 3)
        return;

    String localName = name;

    if (((localName[0] == '*') || (localName[0] == '+'))  && localName[1] == '/') {
        StringBuilder builder;
        builder.append("/tmp"_s);
        builder.append(name.right(localName.length() - 1));
        localName = builder.toString();
    }

    auto fileHandle = FileSystem::openFile(localName, FileSystem::FileOpenMode::Truncate);
    if (!fileHandle)
        return;

    uint32_t magic       = 'CVPB';
    uint32_t width       = static_cast<uint32_t>(CVPixelBufferGetWidth(pixelBuffer));
    uint32_t height      = static_cast<uint32_t>(CVPixelBufferGetHeight(pixelBuffer));
    uint32_t pixelFormat = static_cast<uint32_t>(CVPixelBufferGetPixelFormatType(pixelBuffer));
    uint32_t isPlanar    = CVPixelBufferIsPlanar(pixelBuffer) ? 1 : 0;
    uint32_t planeCount  = isPlanar ? static_cast<uint32_t>(CVPixelBufferGetPlaneCount(pixelBuffer)) : 1;

    fileHandle.write(unsafeMakeSpan<const uint8_t>(reinterpret_cast<const uint8_t*>(&magic), sizeof(uint32_t)));
    fileHandle.write(unsafeMakeSpan<const uint8_t>(reinterpret_cast<const uint8_t*>(&width), sizeof(uint32_t)));
    fileHandle.write(unsafeMakeSpan<const uint8_t>(reinterpret_cast<const uint8_t*>(&height), sizeof(uint32_t)));
    fileHandle.write(unsafeMakeSpan<const uint8_t>(reinterpret_cast<const uint8_t*>(&pixelFormat), sizeof(uint32_t)));
    fileHandle.write(unsafeMakeSpan<const uint8_t>(reinterpret_cast<const uint8_t*>(&isPlanar), sizeof(uint32_t)));
    fileHandle.write(unsafeMakeSpan<const uint8_t>(reinterpret_cast<const uint8_t*>(&planeCount), sizeof(uint32_t)));

    CVPixelBufferLockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);
    {
        uint32_t planeWidth;
        uint32_t planeHeight;
        uint32_t bytesPerRow;
        uint8_t* baseAddress;

        for (uint32_t i = 0; i < planeCount; i++) {
            if (isPlanar) {
                planeWidth  = static_cast<uint32_t>(CVPixelBufferGetWidthOfPlane(pixelBuffer, i));
                planeHeight = static_cast<uint32_t>(CVPixelBufferGetHeightOfPlane(pixelBuffer, i));
                bytesPerRow = static_cast<uint32_t>(CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, i));
                baseAddress = static_cast<uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, i));
            } else {
                planeWidth  = width;
                planeHeight = height;
                bytesPerRow = static_cast<uint32_t>(CVPixelBufferGetBytesPerRow(pixelBuffer));
                baseAddress = static_cast<uint8_t*>(CVPixelBufferGetBaseAddress(pixelBuffer));
            }

            fileHandle.write(unsafeMakeSpan<const uint8_t>(reinterpret_cast<const uint8_t*>(&planeWidth), sizeof(uint32_t)));
            fileHandle.write(unsafeMakeSpan<const uint8_t>(reinterpret_cast<const uint8_t*>(&planeHeight), sizeof(uint32_t)));
            fileHandle.write(unsafeMakeSpan<const uint8_t>(reinterpret_cast<const uint8_t*>(&bytesPerRow), sizeof(uint32_t)));

            fileHandle.write(unsafeMakeSpan(baseAddress, planeHeight * bytesPerRow));
        }
    }
    CVPixelBufferUnlockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);
}
#endif

} // namespace WebCore

#endif
