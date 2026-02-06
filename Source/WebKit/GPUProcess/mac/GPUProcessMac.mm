/*
 * Copyright (C) 2019-2023 Apple Inc. All rights reserved.
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
#import <WebCore/WebMAudioUtilitiesCocoa.h>
#import <algorithm>
#import <pal/spi/cocoa/CoreServicesSPI.h>
#import <pal/spi/cocoa/LaunchServicesSPI.h>
#import <sysexits.h>
#import <wtf/BlockPtr.h>
#import <wtf/MemoryPressureHandler.h>
#import <wtf/ProcessPrivilege.h>
#import <wtf/darwin/DispatchExtras.h>
#import <wtf/text/WTFString.h>

namespace WebKit {
using namespace WebCore;

void GPUProcess::initializeProcess(const AuxiliaryProcessInitializationParameters&)
{
    setApplicationIsDaemon();

#if HAVE(CSCHECKFIXDISABLE)
    _CSCheckFixDisable();
#endif
}

void GPUProcess::initializeProcessName(const AuxiliaryProcessInitializationParameters& parameters)
{
#if PLATFORM(MAC)
    setUIProcessName(parameters.uiProcessName);
#endif
}

#if PLATFORM(MAC)
void GPUProcess::updateProcessName()
{
#if !PLATFORM(MACCATALYST)
ALLOW_NONLITERAL_FORMAT_BEGIN
    RetainPtr applicationName = adoptNS([[NSString alloc] initWithFormat:WEB_UI_STRING("%@ Graphics and Media", "visible name of the GPU process. The argument is the application name.").createNSString().get(), uiProcessName().createNSString().get()]);
ALLOW_NONLITERAL_FORMAT_END
    RetainPtr asn = _LSGetCurrentApplicationASN();
    auto result = _LSSetApplicationInformationItem(kLSDefaultSessionID, asn.get(), _kLSDisplayNameKey, (CFStringRef)applicationName.get(), nullptr);
    ASSERT_UNUSED(result, result == noErr);
#endif
}
#endif

void GPUProcess::initializeSandbox(const AuxiliaryProcessInitializationParameters& parameters, SandboxInitializationParameters& sandboxParameters)
{
    // Need to overide the default, because service has a different bundle ID.
    RetainPtr webKit2Bundle = [NSBundle bundleForClass:NSClassFromString(@"WKWebView")];

    sandboxParameters.setOverrideSandboxProfilePath([webKit2Bundle pathForResource:@"com.apple.WebKit.GPUProcess" ofType:@"sb"]);

    AuxiliaryProcess::initializeSandbox(parameters, sandboxParameters);
}

#if PLATFORM(MAC)
void GPUProcess::setScreenProperties(const WebCore::ScreenProperties& screenProperties)
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

    bool allScreensAreHDR = std::ranges::all_of(screenProperties.screenDataMap.values(), [](auto& screenData) {
        return screenData.screenSupportsHighDynamicRange;
    });
    setShouldOverrideScreenSupportsHighDynamicRange(true, allScreensAreHDR);
#endif
}

void GPUProcess::openDirectoryCacheInvalidated(SandboxExtension::Handle&& handle)
{
    auto cacheInvalidationHandler = [handle = WTF::move(handle)] () mutable {
        AuxiliaryProcess::openDirectoryCacheInvalidated(WTF::move(handle));
    };
    dispatch_async(globalDispatchQueueSingleton(QOS_CLASS_UTILITY, 0), makeBlockPtr(WTF::move(cacheInvalidationHandler)).get());
}
#endif // PLATFORM(MAC)

#if HAVE(POWERLOG_TASK_MODE_QUERY)
void GPUProcess::enablePowerLogging(SandboxExtension::Handle&& handle)
{
    SandboxExtension::consumePermanently(WTF::move(handle));
}
#endif // HAVE(POWERLOG_TASK_MODE_QUERY)

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && (PLATFORM(MAC) || PLATFORM(MACCATALYST))
