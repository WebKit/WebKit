/*
 * Copyright (C) 2018-2024 Apple Inc. All rights reserved.
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

#import "BAssert.h"
#import <Foundation/Foundation.h>
#import <cstdlib>
#import <mutex>
#import <string.h>

namespace bmalloc {

class ProcessNames {
public:
    static NSString* getAppName()
    {
        return singleton().appName();
    }

    static NSString* getProcessName()
    {
        return singleton().processName();
    }

    static const char* getCString()
    {
        return singleton().asCString();
    }

private:
    ProcessNames() = default;

    static void ensureSingleton()
    {
        static std::once_flag onceFlag;
        std::call_once(
            onceFlag,
            [] {
                theProcessNames = new ProcessNames();
            }
        );
    }

    static ProcessNames& singleton()
    {
        if (!theProcessNames)
            ensureSingleton();
        BASSERT(theProcessNames);
        return *theProcessNames;
    }

    NSString* appName()
    {
        static std::once_flag onceFlag;
        std::call_once(
            onceFlag,
            [&] {
                m_appName = [[NSBundle mainBundle] bundleIdentifier];
            });

        return m_appName;
    }

    NSString* processName()
    {
        static std::once_flag onceFlag;
        std::call_once(
            onceFlag,
            [&] {
                m_processName = [[NSProcessInfo processInfo] processName];
            });

        return m_processName;
    }

    const char* asCString()
    {
        static std::once_flag onceFlag;
        std::call_once(onceFlag, [&] () {
            NSString* realName = appName();
            if (!realName)
                realName = processName();

            strncpy(m_cString, [realName UTF8String], s_maxCStringLen);
            m_cString[s_maxCStringLen] = '\0';
        });

        return m_cString;
    }

    static const size_t s_maxCStringLen = 64;
    static ProcessNames* theProcessNames;
    NSString* m_appName { nullptr };
    NSString* m_processName { nullptr };
    char m_cString[s_maxCStringLen + 1] { 0 };
};

ProcessNames* ProcessNames::theProcessNames = nullptr;

const char* processNameString()
{
    return ProcessNames::getCString();
}

#if BPLATFORM(COCOA) && !BPLATFORM(WATCHOS)
bool gigacageEnabledForProcess()
{
    // Note that this function is only called once.
    // If we wanted to make it efficient to call more than once, we could memoize the result in a global boolean.

    @autoreleasepool {
        if (NSString *appName = ProcessNames::getAppName()) {
            bool isWebProcess = [appName hasPrefix:@"com.apple.WebKit.WebContent"];
            return isWebProcess;
        }

        NSString *processName = ProcessNames::getProcessName();
        bool isOptInBinary = [processName isEqualToString:@"jsc"]
            || [processName isEqualToString:@"DumpRenderTree"]
            || [processName isEqualToString:@"wasm"]
            || [processName hasPrefix:@"test"]
            || [processName hasPrefix:@"Test"];

        return isOptInBinary;
    }
}
#endif // BPLATFORM(COCOA) && !BPLATFORM(WATCHOS)

bool shouldAllowMiniMode()
{
    // Mini mode is mainly meant for constraining memory usage in bursty daemons that use JavaScriptCore.
    // It's also contributed to power regressions when enabled for large application processes and in the
    // WebKit XPC services. So we disable mini mode for those processes.
    bool isApplication = false;
    bool isWebKitProcess = false;
    if (const char* serviceName = getenv("XPC_SERVICE_NAME")) {
        static constexpr char appPrefix[] = "application.";
        static constexpr char webKitPrefix[] = "com.apple.WebKit.";
        isApplication = !strncmp(serviceName, appPrefix, sizeof(appPrefix) - 1);
        isWebKitProcess = !strncmp(serviceName, webKitPrefix, sizeof(webKitPrefix) - 1);
    }
    return !isApplication && !isWebKitProcess;
}

#if BPLATFORM(IOS_FAMILY)
bool shouldProcessUnconditionallyUseBmalloc()
{
    static bool result;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [&] () {
        @autoreleasepool {
            if (NSString *appName = ProcessNames::getAppName()) {
                auto contains = [&] (NSString *string) {
                    return [appName rangeOfString:string options:NSCaseInsensitiveSearch].location != NSNotFound;
                };
                result = contains(@"com.apple.WebKit") || contains(@"safari");
            } else {
                NSString *processName = ProcessNames::getProcessName();
                result = [processName isEqualToString:@"jsc"]
                    || [processName isEqualToString:@"wasm"]
                    || [processName hasPrefix:@"test"];
            }
        }
    });

    return result;
}
#endif // BPLATFORM(IOS_FAMILY)

}
