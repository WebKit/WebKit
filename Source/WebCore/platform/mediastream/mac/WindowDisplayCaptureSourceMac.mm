/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#import "config.h"
#import "WindowDisplayCaptureSourceMac.h"

#if ENABLE(MEDIA_STREAM) && PLATFORM(MAC)

#import "GraphicsContextCG.h"
#import "ImageBuffer.h"
#import "Logging.h"
#import "MediaConstraints.h"
#import "MediaSampleAVFObjC.h"
#import "NotImplemented.h"
#import "PixelBufferConformerCV.h"
#import "PlatformLayer.h"
#import "RealtimeMediaSourceSettings.h"
#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <wtf/cf/TypeCastsCF.h>

#import "CoreVideoSoftLink.h"

WTF_DECLARE_CF_TYPE_TRAIT(CGImage);

namespace WebCore {
using namespace PAL;

static bool anyOfCGWindow(const Function<bool(CFDictionaryRef info, unsigned id, const String& title)>& predicate)
{
    auto windows = adoptCF(CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements, kCGNullWindowID));
    if (!windows) {
        RELEASE_LOG(Media, "CGWindowListCopyWindowInfo returned NULL");
        return false;
    }

    auto windowCount = CFArrayGetCount(windows.get());
    for (auto i = 0; i < windowCount; i++) {
        auto windowInfo = checked_cf_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(windows.get(), i));
        if (!windowInfo)
            continue;

        // Menus, the dock, etc have layers greater than 0, skip them.
        auto windowLayerRef = checked_cf_cast<CFNumberRef>(CFDictionaryGetValue(windowInfo, kCGWindowLayer));
        if (!windowLayerRef)
            continue;
        unsigned windowLayer;
        CFNumberGetValue(windowLayerRef, kCFNumberIntType, &windowLayer);
        if (windowLayer)
            continue;

        auto onScreen = checked_cf_cast<CFBooleanRef>(CFDictionaryGetValue(windowInfo, kCGWindowIsOnscreen));
        if (!CFBooleanGetValue(onScreen))
            continue;

        auto windowIDRef = checked_cf_cast<CFNumberRef>(CFDictionaryGetValue(windowInfo, kCGWindowNumber));
        if (!windowIDRef)
            continue;
        unsigned windowID;
        CFNumberGetValue(windowIDRef, kCFNumberIntType, &windowID);
        if (!windowID)
            continue;

        auto windowTitle = checked_cf_cast<CFStringRef>(CFDictionaryGetValue(windowInfo, kCGWindowName));

        if (predicate(windowInfo, windowID, windowTitle))
            return true;
    }

    return false;
}

static RetainPtr<CFDictionaryRef> windowDescription(CGWindowID id)
{
    auto ids = adoptCF(CFArrayCreate(nullptr, reinterpret_cast<const void**>(&id), 1, nullptr));
    auto windows = adoptCF(CGWindowListCreateDescriptionFromArray(ids.get()));
    if (!windows)
        return nullptr;

    return checked_cf_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(windows.get(), 0));
}

CaptureSourceOrError WindowDisplayCaptureSourceMac::create(String&& windowID, const MediaConstraints* constraints)
{
    bool ok;
    auto actualID = windowID.toUIntStrict(&ok);
    if (!ok) {
        RELEASE_LOG(Media, "WindowDisplayCaptureSourceMac::create: window ID does not convert to 32-bit integer");
        return { };
    }

    auto windowInfo = windowDescription(actualID);
    if (!windowInfo) {
        RELEASE_LOG(Media, "WindowDisplayCaptureSourceMac::create: invalid window ID");
        return { };
    }

    auto source = adoptRef(*new WindowDisplayCaptureSourceMac(actualID, checked_cf_cast<CFStringRef>(CFDictionaryGetValue(windowInfo.get(), kCGWindowName))));
    if (constraints && source->applyConstraints(*constraints))
        return { };

    return CaptureSourceOrError(WTFMove(source));
}

WindowDisplayCaptureSourceMac::WindowDisplayCaptureSourceMac(uint32_t windowID, String&& title)
    : DisplayCaptureSourceCocoa(WTFMove(title))
    , m_windowID(windowID)
{
}

RetainPtr<CGImageRef> WindowDisplayCaptureSourceMac::windowImage()
{
    auto image = adoptCF(CGWindowListCreateImage(CGRectNull, kCGWindowListOptionIncludingWindow, m_windowID, kCGWindowImageBoundsIgnoreFraming | kCGWindowImageShouldBeOpaque));
    if (!image)
        RELEASE_LOG(Media, "WindowDisplayCaptureSourceMac::windowImage: failed to capture window image");

    return image;
}

RetainPtr<CVPixelBufferRef> WindowDisplayCaptureSourceMac::generateFrame()
{
    auto image = windowImage();
    if (!image)
        return nullptr;

    if (m_lastImage && m_lastPixelBuffer && CFEqual(m_lastImage.get(), image.get()))
        return m_lastPixelBuffer.get();

    m_lastImage = WTFMove(image);
    return pixelBufferFromCGImage(m_lastImage.get());
}

RetainPtr<CVPixelBufferRef> WindowDisplayCaptureSourceMac::pixelBufferFromCGImage(CGImageRef image)
{
    static CGColorSpaceRef sRGBColorSpace = sRGBColorSpaceRef();

    auto imageSize = IntSize(CGImageGetWidth(image), CGImageGetHeight(image));
    if (imageSize != intrinsicSize()) {
        m_bufferPool = nullptr;
        m_lastImage = nullptr;
        m_lastPixelBuffer = nullptr;
    }

    if (!m_bufferPool) {
        CVPixelBufferPoolRef bufferPool;
        CFDictionaryRef sourcePixelBufferOptions = (__bridge CFDictionaryRef) @{
        (__bridge NSString *)kCVPixelBufferPixelFormatTypeKey : @(kCVPixelFormatType_32ARGB),
        (__bridge NSString *)kCVPixelBufferWidthKey : @(imageSize.width()),
        (__bridge NSString *)kCVPixelBufferHeightKey : @(imageSize.height()),
#if PLATFORM(IOS)
        (__bridge NSString *)kCVPixelFormatOpenGLESCompatibility : @(YES),
#else
        (__bridge NSString *)kCVPixelBufferOpenGLCompatibilityKey : @(YES),
#endif
        (__bridge NSString *)kCVPixelBufferIOSurfacePropertiesKey : @{ /*empty dictionary*/ }
    };

        CFDictionaryRef pixelBufferPoolOptions = (__bridge CFDictionaryRef) @{
            (__bridge NSString *)kCVPixelBufferPoolMinimumBufferCountKey : @(3)
        };

        CVReturn status = CVPixelBufferPoolCreate(kCFAllocatorDefault, pixelBufferPoolOptions, sourcePixelBufferOptions, &bufferPool);
        if (status != kCVReturnSuccess)
            return nullptr;

        m_bufferPool = adoptCF(bufferPool);
        setIntrinsicSize(imageSize);
    }

    CVPixelBufferRef pixelBuffer;
    CVReturn status = CVPixelBufferPoolCreatePixelBuffer(nullptr, m_bufferPool.get(), &pixelBuffer);
    if (status != kCVReturnSuccess)
        return nullptr;

    CVPixelBufferLockBaseAddress(pixelBuffer, 0);
    void* data = CVPixelBufferGetBaseAddress(pixelBuffer);
    auto context = adoptCF(CGBitmapContextCreate(data, imageSize.width(), imageSize.height(), 8, CVPixelBufferGetBytesPerRow(pixelBuffer), sRGBColorSpace, (CGBitmapInfo) kCGImageAlphaNoneSkipFirst));
    CGContextDrawImage(context.get(), CGRectMake(0, 0, CGImageGetWidth(image), CGImageGetHeight(image)), image);
    CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);

    m_lastPixelBuffer = adoptCF(pixelBuffer);
    return m_lastPixelBuffer;
}

std::optional<CaptureDevice> WindowDisplayCaptureSourceMac::windowCaptureDeviceWithPersistentID(const String& idString)
{
    bool ok;
    auto windowID = idString.toUIntStrict(&ok);
    if (!ok) {
        RELEASE_LOG(Media, "WindowDisplayCaptureSourceMac::windowCaptureDeviceWithPersistentID: window ID does not convert to 32-bit integer");
        return std::nullopt;
    }

    String windowTitle;
    if (!anyOfCGWindow([&windowTitle, windowID] (CFDictionaryRef, CGWindowID id, const String& title) {
        if (windowID != id)
            return false;

        windowTitle = title;
        return true;

    })) {
        RELEASE_LOG(Media, "WindowDisplayCaptureSourceMac::windowCaptureDeviceWithPersistentID: window ID is not valid");
        return std::nullopt;
    }

    auto device = CaptureDevice(String::number(windowID), CaptureDevice::DeviceType::Window, windowTitle);
    device.setEnabled(true);

    return device;
}

void WindowDisplayCaptureSourceMac::windowCaptureDevices(Vector<CaptureDevice>& windows)
{
    anyOfCGWindow([&] (CFDictionaryRef, int id, const String& title) mutable {
        CaptureDevice device(String::number(id), CaptureDevice::DeviceType::Window, title);
        device.setEnabled(true);
        windows.append(WTFMove(device));
        return false;
    });
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && PLATFORM(MAC)
