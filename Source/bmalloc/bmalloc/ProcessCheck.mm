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

#import "ProcessCheck.h"

#import <Foundation/Foundation.h>
#import <cstdlib>
#import <mutex>

namespace bmalloc {

#if BPLATFORM(COCOA) && !BPLATFORM(WATCHOS)
bool gigacageEnabledForProcess()
{
    // Note that this function is only called once.
    // If we wanted to make it efficient to call more than once, we could memoize the result in a global boolean.

    NSString *appName = [[NSBundle mainBundle] bundleIdentifier];
    if (appName) {
        bool isWebProcess = [appName hasPrefix:@"com.apple.WebKit.WebContent"];
        return isWebProcess;
    }

    NSString *processName = [[NSProcessInfo processInfo] processName];
    bool isOptInBinary = [processName isEqualToString:@"jsc"]
        || [processName isEqualToString:@"DumpRenderTree"]
        || [processName isEqualToString:@"wasm"]
        || [processName hasPrefix:@"test"]
        || [processName hasPrefix:@"Test"];

    return isOptInBinary;
}
#endif // BPLATFORM(COCOA) && !BPLATFORM(WATCHOS)

#if BPLATFORM(COCOA)
bool shouldAllowMiniMode()
{
    // Mini mode is mainly meant for constraining memory usage in bursty daemons that use JavaScriptCore.
    // It's also contributed to power regressions when enabled for large application processes and in the
    // WebKit GPU process. So we disable mini mode for those processes.
    bool isApplication = false;
    bool isGPUProcess = false;
    if (const char* serviceName = getenv("XPC_SERVICE_NAME")) {
        static constexpr char appPrefix[] = "application.";
        static constexpr char gpuProcessPrefix[] = "com.apple.WebKit.GPU";
        isApplication = !strncmp(serviceName, appPrefix, sizeof(appPrefix) - 1);
        isGPUProcess = !strncmp(serviceName, gpuProcessPrefix, sizeof(gpuProcessPrefix) - 1);
    }
    return !isApplication && !isGPUProcess;
}
#endif

#if BPLATFORM(IOS_FAMILY)
bool shouldProcessUnconditionallyUseBmalloc()
{
    static bool result;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [&] () {
        if (NSString *appName = [[NSBundle mainBundle] bundleIdentifier]) {
            auto contains = [&] (NSString *string) {
                return [appName rangeOfString:string options:NSCaseInsensitiveSearch].location != NSNotFound;
            };
            result = contains(@"com.apple.WebKit") || contains(@"safari");
        } else {
            NSString *processName = [[NSProcessInfo processInfo] processName];
            result = [processName isEqualToString:@"jsc"]
                || [processName isEqualToString:@"wasm"]
                || [processName hasPrefix:@"test"];
        }
    });

    return result;
}
#endif // BPLATFORM(IOS_FAMILY)

}
