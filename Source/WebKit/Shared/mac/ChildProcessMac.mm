/*
 * Copyright (C) 2012-2018 Apple Inc. All rights reserved.
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

#if PLATFORM(MAC) || ENABLE(MINIMAL_SIMULATOR)
#import "ChildProcess.h"

#import "CodeSigning.h"
#import "QuarantineSPI.h"
#import "SandboxInitializationParameters.h"
#import "WKFoundation.h"
#import "XPCServiceEntryPoint.h"
#import <WebCore/FileSystem.h>
#import <WebCore/SystemVersion.h>
#import <mach/mach.h>
#import <mach/task.h>
#import <pwd.h>
#import <stdlib.h>
#import <sysexits.h>
#import <wtf/Scope.h>
#import <wtf/spi/darwin/SandboxSPI.h>

#if USE(APPLE_INTERNAL_SDK)
#include <HIServices/ProcessesPriv.h>
#endif

typedef bool (^LSServerConnectionAllowedBlock) ( CFDictionaryRef optionsRef );
extern "C" void _LSSetApplicationLaunchServicesServerConnectionStatus(uint64_t flags, LSServerConnectionAllowedBlock block);
extern "C" CFDictionaryRef _LSApplicationCheckIn(int sessionID, CFDictionaryRef applicationInfo);

extern "C" OSStatus SetApplicationIsDaemon(Boolean isDaemon);

using namespace WebCore;

namespace WebKit {

static void initializeTimerCoalescingPolicy()
{
    // Set task_latency and task_throughput QOS tiers as appropriate for a visible application.
    struct task_qos_policy qosinfo = { LATENCY_QOS_TIER_0, THROUGHPUT_QOS_TIER_0 };
    kern_return_t kr = task_policy_set(mach_task_self(), TASK_BASE_QOS_POLICY, (task_policy_t)&qosinfo, TASK_QOS_POLICY_COUNT);
    ASSERT_UNUSED(kr, kr == KERN_SUCCESS);
}

void ChildProcess::setApplicationIsDaemon()
{
#if !ENABLE(MINIMAL_SIMULATOR)
    OSStatus error = SetApplicationIsDaemon(true);
    ASSERT_UNUSED(error, error == noErr);
#endif

    launchServicesCheckIn();
}

void ChildProcess::launchServicesCheckIn()
{
    _LSSetApplicationLaunchServicesServerConnectionStatus(0, 0);
    RetainPtr<CFDictionaryRef> unused = _LSApplicationCheckIn(-2, CFBundleGetInfoDictionary(CFBundleGetMainBundle()));
}

void ChildProcess::platformInitialize()
{
    initializeTimerCoalescingPolicy();
    [[NSFileManager defaultManager] changeCurrentDirectoryPath:[[NSBundle mainBundle] bundlePath]];
}

static OSStatus enableSandboxStyleFileQuarantine()
{
#if !ENABLE(MINIMAL_SIMULATOR)
    int error;
    qtn_proc_t quarantineProperties = qtn_proc_alloc();
    auto quarantinePropertiesDeleter = makeScopeExit([quarantineProperties]() {
        qtn_proc_free(quarantineProperties);
    });

    if ((error = qtn_proc_init_with_self(quarantineProperties)))
        return error;

    if ((error = qtn_proc_set_flags(quarantineProperties, QTN_FLAG_SANDBOX)))
        return error;

    // QTN_FLAG_SANDBOX is silently ignored if security.mac.qtn.sandbox_enforce sysctl is 0.
    // In that case, quarantine falls back to advisory QTN_FLAG_DOWNLOAD.
    return qtn_proc_apply_to_self(quarantineProperties);
#else
    return false;
#endif
}

void ChildProcess::initializeSandbox(const ChildProcessInitializationParameters& parameters, SandboxInitializationParameters& sandboxParameters)
{
#if WK_API_ENABLED
    NSBundle *webKit2Bundle = [NSBundle bundleForClass:NSClassFromString(@"WKWebView")];
#else
    NSBundle *webKit2Bundle = [NSBundle bundleForClass:NSClassFromString(@"WKView")];
#endif
    String defaultProfilePath = [webKit2Bundle pathForResource:[[NSBundle mainBundle] bundleIdentifier] ofType:@"sb"];

    if (sandboxParameters.userDirectorySuffix().isNull()) {
        auto userDirectorySuffix = parameters.extraInitializationData.find("user-directory-suffix");
        if (userDirectorySuffix != parameters.extraInitializationData.end())
            sandboxParameters.setUserDirectorySuffix([makeString(userDirectorySuffix->value, '/', String([[NSBundle mainBundle] bundleIdentifier])) fileSystemRepresentation]);
        else {
            String clientIdentifier = codeSigningIdentifier(parameters.connectionIdentifier.xpcConnection.get());
            if (clientIdentifier.isNull())
                clientIdentifier = parameters.clientIdentifier;
            String defaultUserDirectorySuffix = makeString(String([[NSBundle mainBundle] bundleIdentifier]), '+', clientIdentifier);
            sandboxParameters.setUserDirectorySuffix(defaultUserDirectorySuffix);
        }
    }

    Vector<String> osVersionParts;
    String osSystemMarketingVersion = systemMarketingVersion();
    osSystemMarketingVersion.split('.', false, osVersionParts);
    if (osVersionParts.size() < 2) {
        WTFLogAlways("%s: Couldn't find OS Version\n", getprogname());
        exit(EX_NOPERM);
    }
    String osVersion = osVersionParts[0] + '.' + osVersionParts[1];
    sandboxParameters.addParameter("_OS_VERSION", osVersion.utf8().data());

    // Use private temporary and cache directories.
    setenv("DIRHELPER_USER_DIR_SUFFIX", FileSystem::fileSystemRepresentation(sandboxParameters.userDirectorySuffix()).data(), 1);
    char temporaryDirectory[PATH_MAX];
    if (!confstr(_CS_DARWIN_USER_TEMP_DIR, temporaryDirectory, sizeof(temporaryDirectory))) {
        WTFLogAlways("%s: couldn't retrieve private temporary directory path: %d\n", getprogname(), errno);
        exit(EX_NOPERM);
    }
    setenv("TMPDIR", temporaryDirectory, 1);

    sandboxParameters.addPathParameter("WEBKIT2_FRAMEWORK_DIR", [[webKit2Bundle bundlePath] stringByDeletingLastPathComponent]);
    sandboxParameters.addConfDirectoryParameter("DARWIN_USER_TEMP_DIR", _CS_DARWIN_USER_TEMP_DIR);
    sandboxParameters.addConfDirectoryParameter("DARWIN_USER_CACHE_DIR", _CS_DARWIN_USER_CACHE_DIR);

    char buffer[4096];
    int bufferSize = sizeof(buffer);
    struct passwd pwd;
    struct passwd* result = 0;
    if (getpwuid_r(getuid(), &pwd, buffer, bufferSize, &result) || !result) {
        WTFLogAlways("%s: Couldn't find home directory\n", getprogname());
        exit(EX_NOPERM);
    }

    sandboxParameters.addPathParameter("HOME_DIR", pwd.pw_dir);

    String path = String::fromUTF8(pwd.pw_dir);
    path.append("/Library");

    sandboxParameters.addPathParameter("HOME_LIBRARY_DIR", FileSystem::fileSystemRepresentation(path).data());

    path.append("/Preferences");

    sandboxParameters.addPathParameter("HOME_LIBRARY_PREFERENCES_DIR", FileSystem::fileSystemRepresentation(path).data());

    switch (sandboxParameters.mode()) {
    case SandboxInitializationParameters::UseDefaultSandboxProfilePath:
    case SandboxInitializationParameters::UseOverrideSandboxProfilePath: {
        String sandboxProfilePath = sandboxParameters.mode() == SandboxInitializationParameters::UseDefaultSandboxProfilePath ? defaultProfilePath : sandboxParameters.overrideSandboxProfilePath();
        if (!sandboxProfilePath.isEmpty()) {
            CString profilePath = FileSystem::fileSystemRepresentation(sandboxProfilePath);
            char* errorBuf;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
            if (sandbox_init_with_parameters(profilePath.data(), SANDBOX_NAMED_EXTERNAL, sandboxParameters.namedParameterArray(), &errorBuf)) {
#pragma clang diagnostic pop
                WTFLogAlways("%s: Couldn't initialize sandbox profile [%s], error '%s'\n", getprogname(), profilePath.data(), errorBuf);
                for (size_t i = 0, count = sandboxParameters.count(); i != count; ++i)
                    WTFLogAlways("%s=%s\n", sandboxParameters.name(i), sandboxParameters.value(i));
                exit(EX_NOPERM);
            }
        }

        break;
    }
    case SandboxInitializationParameters::UseSandboxProfile: {
        char* errorBuf;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        if (sandbox_init_with_parameters(sandboxParameters.sandboxProfile().utf8().data(), 0, sandboxParameters.namedParameterArray(), &errorBuf)) {
#pragma clang diagnostic pop
            WTFLogAlways("%s: Couldn't initialize sandbox profile, error '%s'\n", getprogname(), errorBuf);
            for (size_t i = 0, count = sandboxParameters.count(); i != count; ++i)
                WTFLogAlways("%s=%s\n", sandboxParameters.name(i), sandboxParameters.value(i));
            exit(EX_NOPERM);
        }

        break;
    }
    }

    // This will override LSFileQuarantineEnabled from Info.plist unless sandbox quarantine is globally disabled.
    OSStatus error = enableSandboxStyleFileQuarantine();
    if (error) {
        WTFLogAlways("%s: Couldn't enable sandbox style file quarantine: %ld\n", getprogname(), static_cast<long>(error));
        exit(EX_NOPERM);
    }
}

#if USE(APPKIT)
void ChildProcess::stopNSAppRunLoop()
{
    ASSERT([NSApp isRunning]);
    [NSApp stop:nil];

    NSEvent *event = [NSEvent otherEventWithType:NSEventTypeApplicationDefined location:NSMakePoint(0, 0) modifierFlags:0 timestamp:0.0 windowNumber:0 context:nil subtype:0 data1:0 data2:0];
    [NSApp postEvent:event atStart:true];
}
#endif

#if !ENABLE(MINIMAL_SIMULATOR) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400
void ChildProcess::stopNSRunLoop()
{
    ASSERT([NSRunLoop mainRunLoop]);
    [[NSRunLoop mainRunLoop] performBlock:^{
        exit(0);
    }];
}
#endif

#if ENABLE(MINIMAL_SIMULATOR)
void ChildProcess::platformStopRunLoop()
{
    XPCServiceExit(WTFMove(m_priorityBoostMessage));
}
#endif

void ChildProcess::setQOS(int latencyQOS, int throughputQOS)
{
    if (!latencyQOS && !throughputQOS)
        return;

    struct task_qos_policy qosinfo = {
        latencyQOS ? LATENCY_QOS_TIER_0 + latencyQOS - 1 : LATENCY_QOS_TIER_UNSPECIFIED,
        throughputQOS ? THROUGHPUT_QOS_TIER_0 + throughputQOS - 1 : THROUGHPUT_QOS_TIER_UNSPECIFIED
    };

    task_policy_set(mach_task_self(), TASK_OVERRIDE_QOS_POLICY, (task_policy_t)&qosinfo, TASK_QOS_POLICY_COUNT);
}

} // namespace WebKit

#endif
