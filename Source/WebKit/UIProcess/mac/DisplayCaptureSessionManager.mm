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

static void alertForWindowSelection(WebPageProxy& page, const WebCore::SecurityOriginData& origin, CompletionHandler<void(std::optional<String>, std::optional<String>)>&& completionHandler)
{
    auto webView = page.cocoaView();
    if (!webView) {
        completionHandler(std::nullopt, std::nullopt);
        return;
    }

    Vector<WebCore::DisplayCaptureManager::WindowCaptureDevice> windowInfo;
    WebCore::RealtimeMediaSourceCenter::singleton().displayCaptureFactory().displayCaptureDeviceManager().windowDevices(windowInfo);
    if (windowInfo.isEmpty()) {
        completionHandler(std::nullopt, std::nullopt);
        return;
    }

    std::sort(windowInfo.begin(), windowInfo.end(), [](auto& a, auto& b) {
        if (a.m_application != b.m_application)
            return codePointCompareLessThan(a.m_application, b.m_application);

        return codePointCompareLessThan(a.m_device.label(), b.m_device.label());
    });

    auto alert = adoptNS([[NSAlert alloc] init]);
    [alert setMessageText:WEB_UI_NSSTRING(@"Choose a window to share", "Message for window sharing prompt")];

    auto popupButton = adoptNS([[NSPopUpButton alloc] initWithFrame:NSMakeRect(10, 0, 290, 36) pullsDown:NO]);
    auto menu = [popupButton menu];
    menu.autoenablesItems = NO;
    String currentApplication;
    unsigned infoIndex = 0;
    for (auto& info : windowInfo) {
        if (info.m_application != currentApplication)    {
            auto applicationItem = adoptNS([[NSMenuItem alloc] initWithTitle:info.m_application action:nil keyEquivalent:@""]);
            [applicationItem setEnabled:NO];
            [menu addItem:applicationItem.get()];
            currentApplication = info.m_application;
        }

        auto title = info.m_device.label();
        auto windowItem = adoptNS([[NSMenuItem alloc] initWithTitle:(!title.isEmpty() ? title : info.m_application) action:nil keyEquivalent:@""]);
        [windowItem setIndentationLevel:1];
        [windowItem setRepresentedObject:@(infoIndex++)];
        [menu addItem:windowItem.get()];
    }
    [popupButton selectItem:nil];

    auto menuLabel = adoptNS([[NSTextView alloc] init]);
    [menuLabel setString:WEB_UI_NSSTRING(@"Window: ", "Label for window sharing menu")];
    [menuLabel setDrawsBackground:NO];
    [menuLabel setSelectable:NO];
    [menuLabel setEditable:NO];

    auto accessoryView = adoptNS([[NSView alloc] initWithFrame:NSMakeRect(0, 0, 300, 40)]);
    [accessoryView addSubview:popupButton.get()];
    [accessoryView addSubview:menuLabel.get()];
    [alert setAccessoryView:accessoryView.get()];

    NSButton *button = [alert addButtonWithTitle:WEB_UI_NSSTRING_KEY(@"Allow", @"Allow (window sharing)", "Allow button title in window sharing prompt")];
    button.keyEquivalent = @"";
    button = [alert addButtonWithTitle:WEB_UI_NSSTRING_KEY(@"Don’t Allow", @"Don’t Allow (window sharing)", "Disallow button title in window sharing prompt")];
    button.keyEquivalent = @"\E";

    [alert beginSheetModalForWindow:[webView window] completionHandler:[popupButton = WTFMove(popupButton), windowInfo, completionBlock = makeBlockPtr(WTFMove(completionHandler))](NSModalResponse returnCode) {
        if (returnCode != NSAlertFirstButtonReturn) {
            completionBlock(std::nullopt, std::nullopt);
            return;
        }

        NSNumber *infoIndex = [[popupButton selectedItem] representedObject];
        if (!infoIndex || [infoIndex unsignedIntegerValue] > windowInfo.size()) {
            completionBlock(std::nullopt, std::nullopt);
            return;
        }

        auto info = windowInfo[[infoIndex unsignedIntegerValue]];
        completionBlock(info.m_device.persistentId(), info.m_device.label());
    }];
}

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

    alertForWindowSelection(page, origin, [completionHandler = WTFMove(completionHandler)] (std::optional<String> windowID, std::optional<String> windowTitle) mutable {

        if (!windowID || !windowTitle) {
            completionHandler(std::nullopt);
            return;
        }

        WebCore::CaptureDevice device = { windowID.value(), WebCore::CaptureDevice::DeviceType::Window, windowTitle.value(), emptyString(), true };
        completionHandler({ device });
    });
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

    callOnMainRunLoop([completionHandler = WTFMove(completionHandler)] () mutable {
        for (auto& device : WebCore::RealtimeMediaSourceCenter::singleton().displayCaptureFactory().displayCaptureDeviceManager().captureDevices()) {
            if (device.enabled() && device.type() == WebCore::CaptureDevice::DeviceType::Screen) {
                completionHandler({ device });
                return;
            }
        }

        completionHandler(std::nullopt);
    });
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
