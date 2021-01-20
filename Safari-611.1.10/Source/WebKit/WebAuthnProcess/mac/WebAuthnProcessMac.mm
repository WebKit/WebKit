/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#import "WebAuthnProcess.h"

#if ENABLE(WEB_AUTHN) && PLATFORM(MAC)

#import "SandboxInitializationParameters.h"
#import "WKFoundation.h"
#import "WebAuthnProcessCreationParameters.h"
#import <WebCore/LocalizedStrings.h>
#import <pal/spi/cocoa/LaunchServicesSPI.h>
#import <pal/spi/mac/HIServicesSPI.h>
#import <sysexits.h>
#import <wtf/MemoryPressureHandler.h>
#import <wtf/text/WTFString.h>

namespace WebKit {
using namespace WebCore;

void WebAuthnProcess::initializeProcess(const AuxiliaryProcessInitializationParameters&)
{
    // Having a window server connection in this process would result in spin logs (<rdar://problem/13239119>).
    OSStatus error = SetApplicationIsDaemon(true);
    ASSERT_UNUSED(error, error == noErr);

    launchServicesCheckIn();
}

void WebAuthnProcess::initializeProcessName(const AuxiliaryProcessInitializationParameters& parameters)
{
    NSString *applicationName = [NSString stringWithFormat:WEB_UI_STRING("%@ Web Authentication", "visible name of the WebAuthn process. The argument is the application name."), (NSString *)parameters.uiProcessName];
    _LSSetApplicationInformationItem(kLSDefaultSessionID, _LSGetCurrentApplicationASN(), _kLSDisplayNameKey, (CFStringRef)applicationName, nullptr);
}

void WebAuthnProcess::initializeSandbox(const AuxiliaryProcessInitializationParameters& parameters, SandboxInitializationParameters& sandboxParameters)
{
    // Need to override the default, because service has a different bundle ID.
    NSBundle *webKit2Bundle = [NSBundle bundleForClass:NSClassFromString(@"WKWebView")];

    sandboxParameters.setOverrideSandboxProfilePath([webKit2Bundle pathForResource:@"com.apple.WebKit.WebAuthnProcess" ofType:@"sb"]);

    AuxiliaryProcess::initializeSandbox(parameters, sandboxParameters);
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
