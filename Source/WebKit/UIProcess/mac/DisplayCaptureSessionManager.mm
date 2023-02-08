/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#import "DisplayCaptureSessionManager.h"

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)

#import "Logging.h"
#import "MediaPermissionUtilities.h"
#import "WKWebViewInternal.h"
#import "WebPageProxy.h"
#import "WebProcess.h"
#import "WebProcessPool.h"
#import <WebCore/CaptureDeviceManager.h>
#import <WebCore/LocalizedStrings.h>
#import <WebCore/MockRealtimeMediaSourceCenter.h>
#import <WebCore/ScreenCaptureKitCaptureSource.h>
#import <WebCore/ScreenCaptureKitSharingSessionManager.h>
#import <WebCore/SecurityOriginData.h>
#import <wtf/BlockPtr.h>
#import <wtf/MainThread.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/URLHelpers.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/text/StringToIntegerConversion.h>

namespace WebKit {

#if HAVE(SCREEN_CAPTURE_KIT)

void DisplayCaptureSessionManager::alertForGetDisplayMedia(WebPageProxy& page, const WebCore::SecurityOriginData& origin, CompletionHandler<void(DisplayCaptureSessionManager::CaptureSessionType)>&& completionHandler)
{

    auto webView = page.cocoaView();
    if (!webView) {
        completionHandler(DisplayCaptureSessionManager::CaptureSessionType::None);
        return;
    }

    NSString *visibleOrigin = applicationVisibleNameFromOrigin(origin);
    if (!visibleOrigin)
        visibleOrigin = applicationVisibleName();

    NSString *alertTitle = [NSString stringWithFormat:WEB_UI_NSSTRING(@"Allow “%@” to observe one of your windows or screens?", "Message for window and screen sharing prompt"), visibleOrigin];
    auto *allowWindowButtonString = WEB_UI_NSSTRING(@"Allow to Share Window", "Allow window button title in window and screen sharing prompt");
    auto *allowScreenButtonString = WEB_UI_NSSTRING(@"Allow to Share Screen", "Allow screen button title in window and screen sharing prompt");
    auto *doNotAllowButtonString = WEB_UI_NSSTRING_KEY(@"Don’t Allow", @"Don’t Allow (window and screen sharing)", "Disallow button title in window and screen sharing prompt");

    auto alert = adoptNS([[NSAlert alloc] init]);
    [alert setMessageText:alertTitle];

    auto *button = [alert addButtonWithTitle:allowWindowButtonString];
    button.keyEquivalent = @"";

    button = [alert addButtonWithTitle:allowScreenButtonString];
    button.keyEquivalent = @"";

    button = [alert addButtonWithTitle:doNotAllowButtonString];
    button.keyEquivalent = @"\E";

    [alert beginSheetModalForWindow:[webView window] completionHandler:[completionBlock = makeBlockPtr(WTFMove(completionHandler))](NSModalResponse returnCode) {
        DisplayCaptureSessionManager::CaptureSessionType result = DisplayCaptureSessionManager::CaptureSessionType::None;
        switch (returnCode) {
        case NSAlertFirstButtonReturn:
            result = DisplayCaptureSessionManager::CaptureSessionType::Window;
            break;
        case NSAlertSecondButtonReturn:
            result = DisplayCaptureSessionManager::CaptureSessionType::Screen;
            break;
        case NSAlertThirdButtonReturn:
            result = DisplayCaptureSessionManager::CaptureSessionType::None;
            break;
        }

        completionBlock(result);
    }];
}

std::optional<WebCore::CaptureDevice> DisplayCaptureSessionManager::deviceSelectedForTesting(WebCore::CaptureDevice::DeviceType deviceType, unsigned indexOfDeviceSelectedForTesting)
{
    unsigned index = 0;
    for (auto& device : WebCore::RealtimeMediaSourceCenter::singleton().displayCaptureFactory().displayCaptureDeviceManager().captureDevices()) {
        if (device.enabled() && device.type() == deviceType) {
            if (index == indexOfDeviceSelectedForTesting)
                return { device };
            ++index;
        }
    }

    return std::nullopt;
}

void DisplayCaptureSessionManager::showWindowPicker(WebPageProxy& page, const WebCore::SecurityOriginData& origin, CompletionHandler<void(std::optional<WebCore::CaptureDevice>)>&& completionHandler)
{
    if (m_indexOfDeviceSelectedForTesting || page.preferences().mockCaptureDevicesEnabled()) {
        completionHandler(deviceSelectedForTesting(WebCore::CaptureDevice::DeviceType::Window, m_indexOfDeviceSelectedForTesting.value_or(0)));
        return;
    }

#if HAVE(SC_CONTENT_SHARING_SESSION)
    if (WebCore::ScreenCaptureKitSharingSessionManager::isAvailable()) {
        if (!page.preferences().useGPUProcessForDisplayCapture()) {
            WebCore::ScreenCaptureKitSharingSessionManager::singleton().showWindowPicker(WTFMove(completionHandler));
            return;
        }

        auto& gpuProcess = page.process().processPool().ensureGPUProcess();
        gpuProcess.updateSandboxAccess(false, false, true);
        gpuProcess.showWindowPicker(WTFMove(completionHandler));
        return;
    }
#endif

    completionHandler(std::nullopt);
}

void DisplayCaptureSessionManager::showScreenPicker(WebPageProxy& page, const WebCore::SecurityOriginData&, CompletionHandler<void(std::optional<WebCore::CaptureDevice>)>&& completionHandler)
{
    if (m_indexOfDeviceSelectedForTesting || page.preferences().mockCaptureDevicesEnabled()) {
        completionHandler(deviceSelectedForTesting(WebCore::CaptureDevice::DeviceType::Screen, m_indexOfDeviceSelectedForTesting.value_or(0)));
        return;
    }

#if HAVE(SC_CONTENT_SHARING_SESSION)
    if (WebCore::ScreenCaptureKitSharingSessionManager::isAvailable()) {
        if (!page.preferences().useGPUProcessForDisplayCapture()) {
            WebCore::ScreenCaptureKitSharingSessionManager::singleton().showScreenPicker(WTFMove(completionHandler));
            return;
        }

        auto& gpuProcess = page.process().processPool().ensureGPUProcess();
        gpuProcess.updateSandboxAccess(false, false, true);
        gpuProcess.showScreenPicker(WTFMove(completionHandler));
        return;
    }
#endif

    completionHandler(std::nullopt);
}
#endif

bool DisplayCaptureSessionManager::isAvailable()
{
#if HAVE(SCREEN_CAPTURE_KIT)
    return WebCore::ScreenCaptureKitCaptureSource::isAvailable();
#else
    return false;
#endif
}

DisplayCaptureSessionManager& DisplayCaptureSessionManager::singleton()
{
    ASSERT(isMainRunLoop());
    static NeverDestroyed<DisplayCaptureSessionManager> manager;
    return manager;
}

DisplayCaptureSessionManager::DisplayCaptureSessionManager()
{
}

DisplayCaptureSessionManager::~DisplayCaptureSessionManager()
{
}

void DisplayCaptureSessionManager::promptForGetDisplayMedia(UserMediaPermissionRequestProxy::UserMediaDisplayCapturePromptType promptType, WebPageProxy& page, const WebCore::SecurityOriginData& origin, CompletionHandler<void(std::optional<WebCore::CaptureDevice>)>&& completionHandler)
{
    ASSERT(isAvailable());

#if HAVE(SCREEN_CAPTURE_KIT)
    if (!isAvailable() || !completionHandler) {
        completionHandler(std::nullopt);
        return;
    }

    if (promptType == UserMediaPermissionRequestProxy::UserMediaDisplayCapturePromptType::Screen) {
        showScreenPicker(page, origin, WTFMove(completionHandler));
        return;
    }

    if (promptType == UserMediaPermissionRequestProxy::UserMediaDisplayCapturePromptType::Window) {
        showWindowPicker(page, origin, WTFMove(completionHandler));
        return;
    }

    alertForGetDisplayMedia(page, origin, [this, page = Ref { page }, origin, completionHandler = WTFMove(completionHandler)] (DisplayCaptureSessionManager::CaptureSessionType sessionType) mutable {
        if (sessionType == CaptureSessionType::None) {
            completionHandler(std::nullopt);
            return;
        }

        if (sessionType == CaptureSessionType::Screen)
            showScreenPicker(page, origin, WTFMove(completionHandler));
        else
            showWindowPicker(page, origin, WTFMove(completionHandler));
    });
#else
    completionHandler(std::nullopt);
#endif

}
} // namespace WebKit

#endif // PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
