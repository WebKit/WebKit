/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#import "NetscapeSandboxFunctions.h"

#if ENABLE(PLUGIN_PROCESS)

#import "PluginProcess.h"
#import "WebKitSystemInterface.h"
#import <WebCore/FileSystem.h>
#import <sys/stat.h>
#import <sysexits.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>
#import <wtf/text/CString.h>

using namespace WebKit;
using namespace WebCore;

static bool enteredSandbox;

bool enterSandbox(const char* sandboxProfile)
{
    if (enteredSandbox)
        return false;

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 1080
    // Use private temporary and cache directories.
    char temporaryDirectory[PATH_MAX];
    if (!confstr(_CS_DARWIN_USER_TEMP_DIR, temporaryDirectory, sizeof(temporaryDirectory))) {
        WTFLogAlways("PluginProcess: couldn't retrieve system temporary directory path: %d\n", errno);
        exit(EX_OSERR);
    }

    if (strlcpy(temporaryDirectory, [[[[NSFileManager defaultManager] stringWithFileSystemRepresentation:temporaryDirectory length:strlen(temporaryDirectory)] stringByAppendingPathComponent:@"WebKitPlugin-XXXXXX"] fileSystemRepresentation], sizeof(temporaryDirectory)) >= sizeof(temporaryDirectory)
        || !mkdtemp(temporaryDirectory)) {
        WTFLogAlways("PluginProcess: couldn't create private temporary directory\n");
        exit(EX_OSERR);
    }

    char* systemDirectorySuffix = strdup([[[[NSFileManager defaultManager] stringWithFileSystemRepresentation:temporaryDirectory length:strlen(temporaryDirectory)] lastPathComponent] fileSystemRepresentation]);
    setenv("DIRHELPER_USER_DIR_SUFFIX", systemDirectorySuffix, 0);
    free(systemDirectorySuffix);

    if (!confstr(_CS_DARWIN_USER_TEMP_DIR, temporaryDirectory, sizeof(temporaryDirectory))) {
        WTFLogAlways("PluginProcess: couldn't retrieve private temporary directory path: %d\n", errno);
        exit(EX_OSERR);
    }
    setenv("TMPDIR", temporaryDirectory, 1);
    if (chdir(temporaryDirectory) == -1) {
        WTFLogAlways("PluginProcess: couldn't change working directory to temporary path: %s, errno %d\n", temporaryDirectory, errno);
        exit(EX_OSERR);
    }
#endif

    Vector<const char*> readOnlyPaths;

    CString pluginModulePath = fileSystemRepresentation(PluginProcess::shared().pluginPath());
    readOnlyPaths.append(pluginModulePath.data());

    // On-disk WebKit framework locations, to account for debug installations.
    // Allowing the whole directory containing WebKit2.framework for the sake of APIs that implicitly load other WebKit frameworks.
    // We don't want to load them now, and thus don't have any better idea of where they are located on disk.
    readOnlyPaths.append([[[[[NSBundle bundleWithIdentifier:@"com.apple.WebKit2"] bundleURL] URLByDeletingLastPathComponent] path] fileSystemRepresentation]);

    readOnlyPaths.append(static_cast<const char*>(0));

    Vector<const char*> readWritePaths;

    char darwinUserTempDirectory[PATH_MAX];
    if (confstr(_CS_DARWIN_USER_TEMP_DIR, darwinUserTempDirectory, PATH_MAX) > 0)
        readWritePaths.append(darwinUserTempDirectory);
    else
        exit(EX_OSERR);

    char darwinUserCacheDirectory[PATH_MAX];
    if (confstr(_CS_DARWIN_USER_CACHE_DIR, darwinUserCacheDirectory, PATH_MAX) > 0)
        readWritePaths.append(darwinUserCacheDirectory);

    RetainPtr<CFStringRef> cachePath(AdoptCF, WKCopyFoundationCacheDirectory());
    readWritePaths.append([(NSString *)cachePath.get() fileSystemRepresentation]);

    readWritePaths.append(static_cast<const char*>(0));

    // WKEnterPluginSandbox canonicalizes path arrays, but not parameters (because it cannot know if one is a path).
    char* homeDirectory = realpath([NSHomeDirectory() fileSystemRepresentation], 0);
    if (!homeDirectory)
        exit(EX_OSERR);

    // We already allow reading and writing to the private temp dir via extendedReadWritePaths above.
    // Pass it again as a parameter in case a specific plugin profile has to apply additional rules.
    char* tempDirectory = realpath(darwinUserTempDirectory, 0);
    if (!tempDirectory)
        exit(EX_OSERR);

    const char* sandboxParameters[] = { "HOME_DIR", homeDirectory, "TEMP_DIR", tempDirectory, 0, 0 };
    if (!WKEnterPluginSandbox(sandboxProfile, sandboxParameters, readOnlyPaths.data(), readWritePaths.data())) {
        WTFLogAlways("Couldn't initialize sandbox profile\n");
        exit(EX_NOPERM);
    }

    if (noErr != WKEnableSandboxStyleFileQuarantine()) {
        WTFLogAlways("Couldn't enable file quarantine\n");
        exit(EX_NOPERM);
    }

    free(homeDirectory);
    free(tempDirectory);
    enteredSandbox = true;

    RetainPtr<NSDictionary> defaults = adoptNS([[NSDictionary alloc] initWithObjectsAndKeys:[NSNumber numberWithBool:YES], @"NSUseRemoteSavePanel", nil]);
    [[NSUserDefaults standardUserDefaults] registerDefaults:defaults.get()];

    return true;
}

#endif // ENABLE(PLUGIN_PROCESS)
