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
#import "WindowDisplayCapturerMac.h"

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

#import <pal/cf/CoreMediaSoftLink.h>
#import "CoreVideoSoftLink.h"

WTF_DECLARE_CF_TYPE_TRAIT(CGImage);

namespace WebCore {
using namespace PAL;

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

Expected<UniqueRef<DisplayCaptureSourceCocoa::Capturer>, String> WindowDisplayCapturerMac::create(const String& deviceID)
{
    auto displayID = parseInteger<uint32_t>(deviceID);
    if (!displayID)
        return makeUnexpected("Invalid window device ID"_s);

    auto windowInfo = windowDescription(*displayID);
    if (!windowInfo)
        return makeUnexpected("Invalid window ID"_s);

    return UniqueRef<DisplayCaptureSourceCocoa::Capturer>(makeUniqueRef<WindowDisplayCapturerMac>(*displayID));
}

WindowDisplayCapturerMac::WindowDisplayCapturerMac(uint32_t windowID)
    : m_windowID(windowID)
{
}

RetainPtr<CGImageRef> WindowDisplayCapturerMac::windowImage()
{
    auto image = adoptCF(CGWindowListCreateImage(CGRectNull, kCGWindowListOptionIncludingWindow, m_windowID, kCGWindowImageBoundsIgnoreFraming | kCGWindowImageShouldBeOpaque));
    if (!image)
        RELEASE_LOG(WebRTC, "WindowDisplayCapturerMac::windowImage: failed to capture window image");

    return image;
}

DisplayCaptureSourceCocoa::DisplayFrameType WindowDisplayCapturerMac::generateFrame()
{
    return DisplayCaptureSourceCocoa::DisplayFrameType { RetainPtr<CGImageRef> { windowImage() } };
}

Optional<CaptureDevice> WindowDisplayCapturerMac::windowCaptureDeviceWithPersistentID(const String& idString)
{
    auto windowID = parseInteger<uint32_t>(idString);
    if (!windowID) {
        RELEASE_LOG(WebRTC, "WindowDisplayCapturerMac::windowCaptureDeviceWithPersistentID: window ID does not convert to 32-bit integer");
        return WTF::nullopt;
    }

    String windowTitle;
    if (!anyOfCGWindow([&windowTitle, windowID] (CFDictionaryRef, CGWindowID id, const String& title) {
        if (windowID != id)
            return false;

        windowTitle = title;
        return true;

    })) {
        RELEASE_LOG(WebRTC, "WindowDisplayCapturerMac::windowCaptureDeviceWithPersistentID: window ID is not valid");
        return WTF::nullopt;
    }

    auto device = CaptureDevice(String::number(*windowID), CaptureDevice::DeviceType::Window, windowTitle);
    device.setEnabled(true);

    return device;
}

void WindowDisplayCapturerMac::windowCaptureDevices(Vector<CaptureDevice>& windows)
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
