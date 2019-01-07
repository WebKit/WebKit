/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
 * Copyright (C) 2012 Samsung Electronics Ltd. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS AS IS''
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
#include "WebProcessPool.h"

#include "APIProcessPoolConfiguration.h"
#include "Logging.h"
#include "WebCookieManagerProxy.h"
#include "WebProcessCreationParameters.h"
#include "WebProcessMessages.h"
#include <JavaScriptCore/RemoteInspectorServer.h>
#include <WebCore/FileSystem.h>
#include <WebCore/GStreamerCommon.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/SchemeRegistry.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/CString.h>

namespace WebKit {

#if ENABLE(REMOTE_INSPECTOR)
static bool initializeRemoteInspectorServer(const char* address)
{
    if (Inspector::RemoteInspectorServer::singleton().isRunning())
        return true;

    if (!address[0])
        return false;

    GUniquePtr<char> inspectorAddress(g_strdup(address));
    char* portPtr = g_strrstr(inspectorAddress.get(), ":");
    if (!portPtr)
        return false;

    *portPtr = '\0';
    portPtr++;
    guint64 port = g_ascii_strtoull(portPtr, nullptr, 10);
    if (!port)
        return false;

    return Inspector::RemoteInspectorServer::singleton().start(inspectorAddress.get(), port);
}
#endif

void WebProcessPool::platformInitialize()
{
#if ENABLE(REMOTE_INSPECTOR)
    if (const char* address = g_getenv("WEBKIT_INSPECTOR_SERVER")) {
        if (!initializeRemoteInspectorServer(address))
            g_unsetenv("WEBKIT_INSPECTOR_SERVER");
    }
#endif
}

void WebProcessPool::platformInitializeWebProcess(WebProcessCreationParameters& parameters)
{
    parameters.memoryCacheDisabled = m_memoryCacheDisabled || cacheModel() == CacheModel::DocumentViewer;
    parameters.proxySettings = m_networkProxySettings;

    parameters.shouldAlwaysUseComplexTextCodePath = true;
    const char* forceComplexText = getenv("WEBKIT_FORCE_COMPLEX_TEXT");
    if (forceComplexText && !strcmp(forceComplexText, "0"))
        parameters.shouldAlwaysUseComplexTextCodePath = m_alwaysUsesComplexTextCodePath;

    const char* disableMemoryPressureMonitor = getenv("WEBKIT_DISABLE_MEMORY_PRESSURE_MONITOR");
    if (disableMemoryPressureMonitor && !strcmp(disableMemoryPressureMonitor, "1"))
        parameters.shouldSuppressMemoryPressureHandler = true;

#if USE(GSTREAMER)
    parameters.gstreamerOptions = WebCore::extractGStreamerOptionsFromCommandLine();
#endif
}

void WebProcessPool::platformInvalidateContext()
{
}

void WebProcessPool::platformResolvePathsForSandboxExtensions()
{
}

} // namespace WebKit
