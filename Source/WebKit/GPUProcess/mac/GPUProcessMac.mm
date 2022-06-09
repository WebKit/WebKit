/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#import "GPUProcess.h"

#if ENABLE(GPU_PROCESS) && (PLATFORM(MAC) || PLATFORM(MACCATALYST))

#import "GPUProcessCreationParameters.h"
#import "SandboxInitializationParameters.h"
#import "WKFoundation.h"
#import <WebCore/LocalizedStrings.h>
#import <WebCore/PlatformScreen.h>
#import <WebCore/ScreenProperties.h>
#import <pal/spi/cocoa/LaunchServicesSPI.h>
#import <sysexits.h>
#import <wtf/MemoryPressureHandler.h>
#import <wtf/ProcessPrivilege.h>
#import <wtf/text/WTFString.h>

namespace WebKit {
using namespace WebCore;

void GPUProcess::initializeProcess(const AuxiliaryProcessInitializationParameters&)
{
    setApplicationIsDaemon();

#if ENABLE(LOWER_FORMATREADERBUNDLE_CODESIGNING_REQUIREMENTS)
    // For testing in engineering builds, allow CoreMedia to load the MediaFormatReader bundle no matter its code signature.
    auto userDefaults = adoptNS([[NSUserDefaults alloc] initWithSuiteName:@"com.apple.coremedia"]);
    [userDefaults registerDefaults:@{ @"pluginformatreader_unsigned": @YES }];
#endif

    launchServicesCheckIn();
}

void GPUProcess::initializeProcessName(const AuxiliaryProcessInitializationParameters& parameters)
{
#if !PLATFORM(MACCATALYST)
    NSString *applicationName = [NSString stringWithFormat:WEB_UI_NSSTRING(@"%@ Graphics and Media", "visible name of the GPU process. The argument is the application name."), (NSString *)parameters.uiProcessName];
    _LSSetApplicationInformationItem(kLSDefaultSessionID, _LSGetCurrentApplicationASN(), _kLSDisplayNameKey, (CFStringRef)applicationName, nullptr);
#endif
}

void GPUProcess::initializeSandbox(const AuxiliaryProcessInitializationParameters& parameters, SandboxInitializationParameters& sandboxParameters)
{
    // Need to overide the default, because service has a different bundle ID.
    NSBundle *webKit2Bundle = [NSBundle bundleForClass:NSClassFromString(@"WKWebView")];

    sandboxParameters.setOverrideSandboxProfilePath([webKit2Bundle pathForResource:@"com.apple.WebKit.GPUProcess" ofType:@"sb"]);

    AuxiliaryProcess::initializeSandbox(parameters, sandboxParameters);
}

#if PLATFORM(MAC)
void GPUProcess::setScreenProperties(const ScreenProperties& screenProperties)
{
#if !HAVE(AVPLAYER_VIDEORANGEOVERRIDE)
    // Only override HDR support at the MediaToolbox level if AVPlayer.videoRangeOverride support is
    // not present, as the MediaToolbox override functionality is both duplicative and process global.

    // This override is not necessary if AVFoundation is allowed to communicate
    // with the window server to query for HDR support.
    if (hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer)) {
        setShouldOverrideScreenSupportsHighDynamicRange(false, false);
        return;
    }

    bool allScreensAreHDR = allOf(screenProperties.screenDataMap.values(), [] (auto& screenData) {
        return screenData.screenSupportsHighDynamicRange;
    });
    setShouldOverrideScreenSupportsHighDynamicRange(true, allScreensAreHDR);
#endif
}
#endif

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && (PLATFORM(MAC) || PLATFORM(MACCATALYST))
