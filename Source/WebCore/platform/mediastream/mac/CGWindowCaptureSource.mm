/*
 * Copyright (C) 2018-2021 Apple Inc. All rights reserved.
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
#import "CGWindowCaptureSource.h"

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
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <wtf/cf/TypeCastsCF.h>
#import <wtf/text/StringToIntegerConversion.h>

#import <pal/cf/CoreMediaSoftLink.h>
#import "CoreVideoSoftLink.h"

WTF_DECLARE_CF_TYPE_TRAIT(CGImage);

namespace WebCore {

static bool anyOfCGWindow(const Function<bool(CFDictionaryRef info, unsigned id, const String& title)>& predicate)
{
    auto windows = adoptCF(CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements, kCGNullWindowID));
    if (!windows) {
        RELEASE_LOG(WebRTC, "CGWindowListCopyWindowInfo returned NULL");
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

        if (CFDictionaryGetValue(windowInfo, kCGWindowIsOnscreen) == kCFBooleanFalse)
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

Expected<UniqueRef<DisplayCaptureSourceMac::Capturer>, String> CGWindowCaptureSource::create(const String& deviceID)
{
    auto windowID = parseInteger<uint32_t>(deviceID);
    if (!windowID)
        return makeUnexpected("Invalid window device ID"_s);

    auto windowInfo = windowDescription(*windowID);
    if (!windowInfo)
        return makeUnexpected("Invalid window ID"_s);

    return UniqueRef<DisplayCaptureSourceMac::Capturer>(makeUniqueRef<CGWindowCaptureSource>(*windowID));
}

IntSize CGWindowCaptureSource::intrinsicSize() const
{
    auto windowInfo = windowDescription(m_windowID);
    if (!windowInfo) {
        RELEASE_LOG(WebRTC, "Invalid window ID?");
        return { };
    }

    auto boundsDictionary = checked_cf_cast<CFDictionaryRef>(CFDictionaryGetValue(windowInfo.get(), kCGWindowBounds));
    if (!boundsDictionary) {
        RELEASE_LOG(WebRTC, "Unable to get window bounds");
        return { };
    }

    CGRect windowBounds;
    if (!CGRectMakeWithDictionaryRepresentation(boundsDictionary, &windowBounds)) {
        RELEASE_LOG(WebRTC, "Unable to decode window bounds");
        return { };
    }

    return IntSize(windowBounds.size);
}

CGWindowCaptureSource::CGWindowCaptureSource(uint32_t windowID)
    : m_windowID(windowID)
{
}

RetainPtr<CGImageRef> CGWindowCaptureSource::windowImage()
{
    auto image = adoptCF(CGWindowListCreateImage(CGRectNull, kCGWindowListOptionIncludingWindow, m_windowID, kCGWindowImageBoundsIgnoreFraming | kCGWindowImageShouldBeOpaque));
    if (!image)
        RELEASE_LOG(WebRTC, "CGWindowCaptureSource::windowImage: failed to capture window image");

    return image;
}

DisplayCaptureSourceMac::DisplayFrameType CGWindowCaptureSource::generateFrame()
{
    return NativeImage::create(windowImage());
}

std::optional<CaptureDevice> CGWindowCaptureSource::windowCaptureDeviceWithPersistentID(const String& idString)
{
    auto windowID = parseInteger<uint32_t>(idString);
    if (!windowID) {
        RELEASE_LOG(WebRTC, "CGWindowCaptureSource::windowCaptureDeviceWithPersistentID: window ID does not convert to 32-bit integer");
        return std::nullopt;
    }

    String windowTitle;
    if (!anyOfCGWindow([&windowTitle, windowID] (CFDictionaryRef, CGWindowID id, const String& title) {
        if (windowID != id)
            return false;

        windowTitle = title;
        return true;

    })) {
        RELEASE_LOG(WebRTC, "CGWindowCaptureSource::windowCaptureDeviceWithPersistentID: window ID is not valid");
        return std::nullopt;
    }

    auto device = CaptureDevice(String::number(*windowID), CaptureDevice::DeviceType::Window, windowTitle);
    device.setEnabled(true);

    return device;
}

void CGWindowCaptureSource::windowCaptureDevices(Vector<CaptureDevice>& windows)
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
