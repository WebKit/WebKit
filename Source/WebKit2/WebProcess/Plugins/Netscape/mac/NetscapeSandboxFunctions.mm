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

#if ENABLE(NETSCAPE_PLUGIN_API) && ENABLE(PLUGIN_PROCESS)

#if !defined(BUILDING_ON_LEOPARD) && !defined(BUILDING_ON_SNOW_LEOPARD)

#import "PluginProcess.h"
#import "NetscapePluginModule.h"
#import "WebKitSystemInterface.h"
#import <WebCore/FileSystem.h>
#import <WebCore/SoftLinking.h>
#import <sys/stat.h>
#import <sysexits.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>
#import <wtf/text/CString.h>

SOFT_LINK_FRAMEWORK(CoreServices)
SOFT_LINK_OPTIONAL(CoreServices, CFURLStopAccessingSecurityScopedResource, void, unused, (CFURLRef))

using namespace WebKit;
using namespace WebCore;

WKNSandboxFunctions* netscapeSandboxFunctions()
{
    static WKNSandboxFunctions functions = {
        sizeof(WKNSandboxFunctions),
        WKNVSandboxFunctionsVersionCurrent,
        WKN_EnterSandbox,
        WKN_FileStopAccessing
    };
    return &functions;
}

static bool enteredSandbox;

static CString readSandboxProfile()
{
    RetainPtr<CFURLRef> profileURL(AdoptCF, CFBundleCopyResourceURL(CFBundleGetMainBundle(), CFSTR("com.apple.WebKit.PluginProcess"), CFSTR("sb"), 0));
    char profilePath[PATH_MAX];
    if (!CFURLGetFileSystemRepresentation(profileURL.get(), false, reinterpret_cast<UInt8*>(profilePath), sizeof(profilePath))) {
        WTFLogAlways("Could not get file system representation of plug-in sandbox URL\n");
        return CString();
    }

    FILE *file = fopen(profilePath, "r");
    if (!file) {
        WTFLogAlways("Could not open plug-in sandbox file '%s'\n", profilePath);
        return CString();
    }

    struct stat fileInfo;
    if (stat(profilePath, &fileInfo)) {
        WTFLogAlways("Could not get plug-in sandbox file size '%s'\n", profilePath);
        return CString();
    }

    char* characterBuffer;
    CString result = CString::newUninitialized(fileInfo.st_size, characterBuffer);

    if (1 != fread(characterBuffer, fileInfo.st_size, 1, file)) {
        WTFLogAlways("Could not read plug-in sandbox file '%s'\n", profilePath);
        return CString();
    }

    fclose(file);

    return result;
}

NPError WKN_EnterSandbox(const char* readOnlyPaths[], const char* readWritePaths[])
{
    if (enteredSandbox)
        return NPERR_GENERIC_ERROR;

    CString profile = readSandboxProfile();
    if (profile.isNull())
        exit(EX_NOPERM);

#if !defined(BUILDING_ON_LION)
    // Use private temporary and cache directories.
    String systemDirectorySuffix = "com.apple.WebKit.PluginProcess+" + PluginProcess::shared().netscapePluginModule()->module()->bundleIdentifier();
    setenv("DIRHELPER_USER_DIR_SUFFIX", fileSystemRepresentation(systemDirectorySuffix).data(), 0);
    char temporaryDirectory[PATH_MAX];
    if (!confstr(_CS_DARWIN_USER_TEMP_DIR, temporaryDirectory, sizeof(temporaryDirectory))) {
        WTFLogAlways("PluginProcess: couldn't retrieve private temporary directory path: %d\n", errno);
        exit(EX_NOPERM);
    }
    setenv("TMPDIR", temporaryDirectory, 1);
#endif


    Vector<const char*> extendedReadOnlyPaths;
    if (readOnlyPaths) {
        for (unsigned i = 0; readOnlyPaths[i]; ++i)
            extendedReadOnlyPaths.append(readOnlyPaths[i]);
    }

    CString pluginModulePath = fileSystemRepresentation(PluginProcess::shared().pluginPath());
    extendedReadOnlyPaths.append(pluginModulePath.data());

    // On-disk WebKit framework locations, to account for debug installations.
    // Allowing the whole directory containing WebKit2.framework for the sake of APIs that implicitly load other WebKit frameworks.
    // We don't want to load them now, and thus don't have any better idea of where they are located on disk.
    extendedReadOnlyPaths.append([[[[[NSBundle bundleWithIdentifier:@"com.apple.WebKit2"] bundleURL] URLByDeletingLastPathComponent] path] fileSystemRepresentation]);

    extendedReadOnlyPaths.append(static_cast<const char*>(0));

    Vector<const char*> extendedReadWritePaths;
    if (readWritePaths) {
        for (unsigned i = 0; readWritePaths[i]; ++i)
            extendedReadWritePaths.append(readWritePaths[i]);
    }

    char darwinUserTempDirectory[PATH_MAX];
    if (confstr(_CS_DARWIN_USER_TEMP_DIR, darwinUserTempDirectory, PATH_MAX) > 0)
        extendedReadWritePaths.append(darwinUserTempDirectory);

    char darwinUserCacheDirectory[PATH_MAX];
    if (confstr(_CS_DARWIN_USER_CACHE_DIR, darwinUserCacheDirectory, PATH_MAX) > 0)
        extendedReadWritePaths.append(darwinUserCacheDirectory);

    RetainPtr<CFStringRef> cachePath(AdoptCF, WKCopyFoundationCacheDirectory());
    extendedReadWritePaths.append([(NSString *)cachePath.get() fileSystemRepresentation]);

    extendedReadWritePaths.append(static_cast<const char*>(0));

    // WKEnterPluginSandbox canonicalizes path arrays, but not parameters (because it cannot know if one is a path).
    char* homeDirectory = realpath([NSHomeDirectory() fileSystemRepresentation], 0);
    if (!homeDirectory)
        exit(EX_NOPERM);
    const char* sandboxParameters[] = { "HOME_DIR", homeDirectory, 0, 0 };

    if (!WKEnterPluginSandbox(profile.data(), sandboxParameters, extendedReadOnlyPaths.data(), extendedReadWritePaths.data())) {
        WTFLogAlways("Couldn't initialize sandbox profile\n");
        exit(EX_NOPERM);
    }

    if (noErr != WKEnableSandboxStyleFileQuarantine()) {
        WTFLogAlways("Couldn't enable file quarantine\n");
        exit(EX_NOPERM);
    }

    free(homeDirectory);
    enteredSandbox = true;
    return NPERR_NO_ERROR;
}

NPError WKN_FileStopAccessing(const char* path)
{
    if (!enteredSandbox)
        return NPERR_GENERIC_ERROR;

    if (!CFURLStopAccessingSecurityScopedResourcePtr())
        return NPERR_NO_ERROR;

    RetainPtr<CFStringRef> urlString(AdoptCF, CFStringCreateWithFileSystemRepresentation(0, path));
    if (!urlString)
        return NPERR_INVALID_PARAM;
    RetainPtr<CFURLRef> url(AdoptCF, CFURLCreateWithFileSystemPath(0, urlString.get(), kCFURLPOSIXPathStyle, false));

    CFURLStopAccessingSecurityScopedResourcePtr()(url.get());

    return NPERR_NO_ERROR;
}

#endif // !defined(BUILDING_ON_LEOPARD) && !defined(BUILDING_ON_SNOW_LEOPARD)

#endif // ENABLE(NETSCAPE_PLUGIN_API) && ENABLE(PLUGIN_PROCESS)
