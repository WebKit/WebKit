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
#include "WebPageProxy.h"

#include "resource.h"
#include <tchar.h>
#include <WebCore/WebCoreInstanceHandle.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/StringConcatenate.h>

using namespace WebCore;

namespace WebKit {

static String windowsVersion()
{
   String osVersion;
   OSVERSIONINFO versionInfo = { 0 };
   versionInfo.dwOSVersionInfoSize = sizeof(versionInfo);
   ::GetVersionEx(&versionInfo);

   if (versionInfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) {
       if (versionInfo.dwMajorVersion == 4) {
           if (versionInfo.dwMinorVersion == 0)
               osVersion = "Windows 95";
           else if (versionInfo.dwMinorVersion == 10)
               osVersion = "Windows 98";
           else if (versionInfo.dwMinorVersion == 90)
               osVersion = "Windows 98; Win 9x 4.90";
       }
   } else if (versionInfo.dwPlatformId == VER_PLATFORM_WIN32_NT)
       osVersion = makeString("Windows NT ", String::number(versionInfo.dwMajorVersion), '.', String::number(versionInfo.dwMinorVersion));

   if (!osVersion.length())
       osVersion = makeString("Windows ", String::number(versionInfo.dwMajorVersion), '.', String::number(versionInfo.dwMinorVersion));
   return osVersion;
}

static String userVisibleWebKitVersionString()
{
    LPWSTR buildNumberStringPtr;
    if (!::LoadStringW(instanceHandle(), BUILD_NUMBER, reinterpret_cast<LPWSTR>(&buildNumberStringPtr), 0) || !buildNumberStringPtr)
        return "534+";

    return buildNumberStringPtr;
}

String WebPageProxy::standardUserAgent(const String& applicationNameForUserAgent)
{
    DEFINE_STATIC_LOCAL(String, osVersion, (windowsVersion()));
    DEFINE_STATIC_LOCAL(String, webKitVersion, (userVisibleWebKitVersionString()));

    if (applicationNameForUserAgent.isEmpty())
        return makeString("Mozilla/5.0 (", osVersion, ") AppleWebKit/", webKitVersion, " (KHTML, like Gecko)");
    return makeString("Mozilla/5.0 (", osVersion, ") AppleWebKit/", webKitVersion, " (KHTML, like Gecko) ", applicationNameForUserAgent);
}

} // namespace WebKit
