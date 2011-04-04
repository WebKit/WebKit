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

#include "config.h"
#include "WebContext.h"

#include "WebProcessCreationParameters.h"
#include "WebProcessMessages.h"
#include <WebCore/FileSystem.h>

#if USE(CFNETWORK)
#include <CFNetwork/CFURLCachePriv.h>
#include <WebKitSystemInterface/WebKitSystemInterface.h> 
#endif

using namespace WebCore;

namespace WebKit {

String WebContext::applicationCacheDirectory()
{
    return localUserSpecificStorageDirectory();
}

void WebContext::setShouldPaintNativeControls(bool b)
{
    m_shouldPaintNativeControls = b;

    sendToAllProcesses(Messages::WebProcess::SetShouldPaintNativeControls(m_shouldPaintNativeControls));
}

void WebContext::platformInitializeWebProcess(WebProcessCreationParameters& parameters)
{
    parameters.shouldPaintNativeControls = m_shouldPaintNativeControls;

#if USE(CFNETWORK)
    RetainPtr<CFURLCacheRef> cfurlCache(AdoptCF, CFURLCacheCopySharedURLCache());
    parameters.cfURLCacheDiskCapacity = CFURLCacheDiskCapacity(cfurlCache.get());
    parameters.cfURLCacheMemoryCapacity = CFURLCacheMemoryCapacity(cfurlCache.get());

    RetainPtr<CFStringRef> cfURLCachePath(AdoptCF, wkCopyFoundationCacheDirectory());
    parameters.cfURLCachePath = String(cfURLCachePath.get());
    // Remove the ending '\' (necessary to have CFNetwork find the Cache file).
    ASSERT(parameters.cfURLCachePath.length());
    if (parameters.cfURLCachePath[parameters.cfURLCachePath.length() - 1] == '\\')
        parameters.cfURLCachePath.remove(parameters.cfURLCachePath.length() - 1);

#if USE(CFURLSTORAGESESSIONS)
    parameters.uiProcessBundleIdentifier = String(reinterpret_cast<CFStringRef>(CFBundleGetValueForInfoDictionaryKey(CFBundleGetMainBundle(), kCFBundleIdentifierKey)));
#endif // USE(CFURLSTORAGESESSIONS)

    parameters.initialHTTPCookieAcceptPolicy = m_initialHTTPCookieAcceptPolicy;

#endif // USE(CFNETWORK)
}

void WebContext::platformInvalidateContext()
{
}

String WebContext::platformDefaultDatabaseDirectory() const
{
    return WebCore::pathByAppendingComponent(WebCore::localUserSpecificStorageDirectory(), "Databases");
}

String WebContext::platformDefaultIconDatabasePath() const
{
    // IconDatabase should be disabled by default on Windows, and should therefore have no default path.
    return String();
}

String WebContext::platformDefaultLocalStorageDirectory() const
{
    return WebCore::pathByAppendingComponent(WebCore::localUserSpecificStorageDirectory(), "LocalStorage");
}

} // namespace WebKit

