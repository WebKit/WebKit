/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#import "WebContext.h"

#import "WebKitSystemInterface.h"
#import "WebProcessCreationParameters.h"
#import <WebCore/FileSystem.h>
#import <sys/param.h>

using namespace WebCore;

NSString *WebDatabaseDirectoryDefaultsKey = @"WebDatabaseDirectory";
NSString *WebKitLocalCacheDefaultsKey = @"WebKitLocalCache";
NSString *WebStorageDirectoryDefaultsKey = @"WebKitLocalStorageDatabasePathPreferenceKey";

static NSString *WebKitApplicationDidChangeAccessibilityEnhancedUserInterfaceNotification = @"NSApplicationDidChangeAccessibilityEnhancedUserInterfaceNotification";

// FIXME: <rdar://problem/9138817> - After this "backwards compatibility" radar is removed, this code should be removed to only return an empty String.
NSString *WebIconDatabaseDirectoryDefaultsKey = @"WebIconDatabaseDirectoryDefaultsKey";

namespace WebKit {

String WebContext::applicationCacheDirectory()
{
    NSString *appName = [[NSBundle mainBundle] bundleIdentifier];
    if (!appName)
        appName = [[NSProcessInfo processInfo] processName];
    
    ASSERT(appName);
    
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSString *cacheDir = [defaults objectForKey:WebKitLocalCacheDefaultsKey];

    if (!cacheDir || ![cacheDir isKindOfClass:[NSString class]]) {
        char cacheDirectory[MAXPATHLEN];
        size_t cacheDirectoryLen = confstr(_CS_DARWIN_USER_CACHE_DIR, cacheDirectory, MAXPATHLEN);
    
        if (cacheDirectoryLen)
            cacheDir = [[NSFileManager defaultManager] stringWithFileSystemRepresentation:cacheDirectory length:cacheDirectoryLen - 1];
    }

    return [cacheDir stringByAppendingPathComponent:appName];
}


void WebContext::platformInitializeWebProcess(WebProcessCreationParameters& parameters)
{
    if (!omitPDFSupport()) {
        // We want to use a PDF view in the UI process for PDF MIME types.
        HashSet<String, CaseFoldingHash> mimeType = pdfAndPostScriptMIMETypes();
        parameters.mimeTypesWithCustomRepresentation.appendRange(mimeType.begin(), mimeType.end());
    }

    RetainPtr<CFStringRef> cachePath(AdoptCF, WKCopyFoundationCacheDirectory());
    if (!cachePath)
        cachePath = reinterpret_cast<CFStringRef>(NSHomeDirectory());

    NSURLCache *urlCache = [NSURLCache sharedURLCache];

    parameters.parentProcessName = [[NSProcessInfo processInfo] processName];    
    parameters.nsURLCachePath = [(NSString *)cachePath.get() stringByStandardizingPath];
    parameters.nsURLCacheMemoryCapacity = [urlCache memoryCapacity];
    parameters.nsURLCacheDiskCapacity = [urlCache diskCapacity];

    ASSERT(!parameters.nsURLCachePath.isEmpty());

#if USE(ACCELERATED_COMPOSITING) && HAVE(HOSTED_CORE_ANIMATION)
    mach_port_t renderServerPort = WKInitializeRenderServer();
    if (renderServerPort != MACH_PORT_NULL)
        parameters.acceleratedCompositingPort = CoreIPC::MachPort(renderServerPort, MACH_MSG_TYPE_COPY_SEND);
#endif

    // FIXME: This should really be configurable; we shouldn't just blindly allow read access to the UI process bundle.
    parameters.uiProcessBundleResourcePath = [[NSBundle mainBundle] resourcePath];

#if USE(CFURLSTORAGESESSIONS)
    parameters.uiProcessBundleIdentifier = String([[NSBundle mainBundle] bundleIdentifier]);
#endif
    
    // Listen for enhanced accessibility changes and propagate them to the WebProcess.
    m_enhancedAccessibilityObserver = [[NSNotificationCenter defaultCenter] addObserverForName:WebKitApplicationDidChangeAccessibilityEnhancedUserInterfaceNotification object:nil queue:[NSOperationQueue currentQueue] usingBlock:^(NSNotification *note) {
        setEnhancedAccessibility([[[note userInfo] objectForKey:@"AXEnhancedUserInterface"] boolValue]);
    }];
}

void WebContext::platformInvalidateContext()
{
    [[NSNotificationCenter defaultCenter] removeObserver:(id)m_enhancedAccessibilityObserver.get()];
}
    
String WebContext::platformDefaultDatabaseDirectory() const
{
    NSString *databasesDirectory = [[NSUserDefaults standardUserDefaults] objectForKey:WebDatabaseDirectoryDefaultsKey];
    if (!databasesDirectory || ![databasesDirectory isKindOfClass:[NSString class]])
        databasesDirectory = @"~/Library/WebKit/Databases";
    return [databasesDirectory stringByStandardizingPath];
}

String WebContext::platformDefaultIconDatabasePath() const
{
    // FIXME: <rdar://problem/9138817> - After this "backwards compatibility" radar is removed, this code should be removed to only return an empty String.
    NSString *databasesDirectory = [[NSUserDefaults standardUserDefaults] objectForKey:WebIconDatabaseDirectoryDefaultsKey];
    if (!databasesDirectory || ![databasesDirectory isKindOfClass:[NSString class]])
        databasesDirectory = @"~/Library/Icons/WebpageIcons.db";
    return [databasesDirectory stringByStandardizingPath];
}

String WebContext::platformDefaultLocalStorageDirectory() const
{
    NSString *localStorageDirectory = [[NSUserDefaults standardUserDefaults] objectForKey:WebStorageDirectoryDefaultsKey];
    if (!localStorageDirectory || ![localStorageDirectory isKindOfClass:[NSString class]])
        localStorageDirectory = @"~/Library/WebKit/LocalStorage";
    return [localStorageDirectory stringByStandardizingPath];
}

bool WebContext::omitPDFSupport()
{
    // Since this is a "secret default" we don't bother registering it.
    return [[NSUserDefaults standardUserDefaults] boolForKey:@"WebKitOmitPDFSupport"];
}

} // namespace WebKit

