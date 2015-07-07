/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY MOTOROLA INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MOTOROLA INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ProcessExecutablePath.h"

#include <WebCore/FileSystem.h>
#include <glib.h>
#include <wtf/gobject/GlibUtilities.h>

using namespace WebCore;

namespace WebKit {

static String getExecutablePath()
{
    CString executablePath = getCurrentExecutablePath();
    if (!executablePath.isNull())
        return directoryName(filenameToString(executablePath.data()));
    return String();
}

static String findWebKitProcess(const char* processName)
{
    static const char* execDirectory = g_getenv("WEBKIT_EXEC_PATH");
    if (execDirectory) {
        String processPath = pathByAppendingComponent(filenameToString(execDirectory), processName);
        if (fileExists(processPath))
            return processPath;
    }

    static String executablePath = getExecutablePath();
    if (!executablePath.isNull()) {
        String processPath = pathByAppendingComponent(executablePath, processName);
        if (fileExists(processPath))
            return processPath;
    }

    return pathByAppendingComponent(filenameToString(LIBEXECDIR), processName);
}

String executablePathOfWebProcess()
{
    return findWebKitProcess("WebKitWebProcess");
}

String executablePathOfPluginProcess()
{
    return findWebKitProcess("WebKitPluginProcess");
}

#if ENABLE(NETWORK_PROCESS)
String executablePathOfNetworkProcess()
{
    return findWebKitProcess("WebKitNetworkProcess");
}
#endif

} // namespace WebKit
