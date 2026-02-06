/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#import "ProcessStartupSandboxExtensions.h"

#import <pal/spi/cocoa/CoreServicesSPI.h>
#import <sysexits.h>
#import <wtf/FileSystem.h>
#import <wtf/WTFProcess.h>
#import <wtf/text/MakeString.h>
#import <wtf/text/StringBuilder.h>
#import <wtf/text/cf/StringConcatenateCF.h>

namespace WebKit {

void ProcessStartupSandboxExtensions::createSandboxExtensions(ASCIILiteral processName)
{
    String bundlePath = webKitBundlePath();
    if (auto handle = SandboxExtension::createHandle(bundlePath, SandboxExtension::Type::ReadOnly))
        webKitBundleDirectoryExtension = WTF::move(*handle);
    userDirectorySuffix = makeString([[NSBundle mainBundle] bundleIdentifier], '+', processName);
    createCacheDirectorySandboxExtension(userDirectorySuffix);
    createTempDirectorySandboxExtension(userDirectorySuffix);
}

void ProcessStartupSandboxExtensions::createCacheDirectorySandboxExtension(String userDirectorySuffix)
{
    char cacheDirectory[PATH_MAX] = { 0 };
    if (__user_local_dirname(getuid(), DIRHELPER_USER_LOCAL_CACHE, cacheDirectory, PATH_MAX)) {
        auto cacheDirectorySpan = unsafeSpan(cacheDirectory);
        String cacheDirectoryForProcess = FileSystem::pathByAppendingComponent(cacheDirectorySpan, userDirectorySuffix);
        if (auto handle = SandboxExtension::createHandleForReadWriteDirectory(cacheDirectoryForProcess))
            cacheDirectoryExtension = WTF::move(*handle);
    }
}

void ProcessStartupSandboxExtensions::createTempDirectorySandboxExtension(String userDirectorySuffix)
{
    char tempDirectory[PATH_MAX] = { 0 };
    if (__user_local_dirname(getuid(), DIRHELPER_USER_LOCAL_TEMP, tempDirectory, PATH_MAX)) {
        auto tempDirectorySpan = unsafeSpan(tempDirectory);
        String tempDirectoryForProcess = FileSystem::pathByAppendingComponent(tempDirectorySpan, userDirectorySuffix);
        if (auto handle = SandboxExtension::createHandleForReadWriteDirectory(tempDirectoryForProcess))
            tempDirectoryExtension = WTF::move(*handle);
    }
}

void ProcessStartupSandboxExtensions::consumeSandboxExtensions() const
{
    SandboxExtension::consumePermanently(webKitBundleDirectoryExtension);
    SandboxExtension::consumePermanently(cacheDirectoryExtension);
    SandboxExtension::consumePermanently(tempDirectoryExtension);

    // Use private temporary and cache directories.
    setenv("DIRHELPER_USER_DIR_SUFFIX", FileSystem::fileSystemRepresentation(userDirectorySuffix).data(), 1);
    char temporaryDirectory[PATH_MAX];
    if (!confstr(_CS_DARWIN_USER_TEMP_DIR, temporaryDirectory, sizeof(temporaryDirectory))) {
        WTFLogAlways("%s: couldn't retrieve private temporary directory path: %d\n", getprogname(), errno);
        exitProcess(EX_NOPERM);
    }
    setenv("TMPDIR", temporaryDirectory, 1);
}

String ProcessStartupSandboxExtensions::webKitBundlePath() const
{
    RetainPtr<NSBundle> bundle = [NSBundle bundleForClass:NSClassFromString(@"WKWebView")];
    String bundlePath = bundle.get().bundlePath;
    if (!bundlePath.startsWith("/System/Library/Frameworks"_s))
        bundlePath = bundle.get().bundlePath.stringByDeletingLastPathComponent;
    return bundlePath;
}

}
