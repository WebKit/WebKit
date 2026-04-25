/*
 * Copyright (C) 2017 Igalia S.L.
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

#include "config.h"

#include "LogInitialization.h"
#include "WebDriverService.h"
#include <wtf/MainThread.h>
#include <wtf/Threading.h>

//
// On Android, WebDriver is built as a shared library, and the process is spawned
// from the Java side, which calls into a C++ function using JNI, and that in turn
// jumps into the entry point. The mangled name is used directly, from the code at
// https://github.com/Igalia/wpe-android/blob/b918e3f8b86eda406436cb251c2e7b10a529008c/wpeview/src/main/cpp/Service/EntryPoint.cpp#L55
//
#if OS(ANDROID)
namespace WebKit {
__attribute__((visibility("default")))
int WebDriverProcessMain(int argc, char** argv)
#else
int main(int argc, char** argv)
#endif
{
    WebDriver::WebDriverService::platformInit();

    WTF::initializeMainThread();
#if !LOG_DISABLED || !RELEASE_LOG_DISABLED
    WebDriver::logChannels().initializeLogChannelsIfNecessary(WebDriver::logLevelString());
#endif

    auto service = makeUnique<WebDriver::WebDriverService>();
    return service->run(argc, argv);
}
#if OS(ANDROID)
}
#endif
