/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2017 Sony Interactive Entertainment Inc.
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

#include "WebProcessCreationParameters.h"
#include <WebCore/NotImplemented.h>

#if ENABLE(REMOTE_INSPECTOR)
#include <JavaScriptCore/RemoteInspectorServer.h>
#include <WebCore/WebCoreBundleWin.h>
#endif

namespace WebKit {

#if ENABLE(REMOTE_INSPECTOR)
static String backendCommandsPath()
{
    RetainPtr<CFURLRef> urlRef = adoptCF(CFBundleCopyResourceURL(WebCore::webKitBundle(), CFSTR("InspectorBackendCommands"), CFSTR("js"), CFSTR("WebInspectorUI\\Protocol")));
    if (!urlRef)
        return { };

    char path[MAX_PATH];
    if (!CFURLGetFileSystemRepresentation(urlRef.get(), false, reinterpret_cast<UInt8*>(path), MAX_PATH))
        return { };

    return path;
}

static void initializeRemoteInspectorServer(StringView address)
{
    if (Inspector::RemoteInspectorServer::singleton().isRunning())
        return;

    auto pos = address.find(':');
    if (pos == notFound)
        return;

    auto host = address.substring(0, pos);
    auto port = address.substring(pos + 1).toUInt64Strict();
    if (!port)
        return;

    Inspector::RemoteInspector::singleton().setBackendCommandsPath(backendCommandsPath());
    Inspector::RemoteInspectorServer::singleton().start(host.utf8().data(), port.value());
}
#endif

void WebProcessPool::platformInitialize()
{
#if ENABLE(REMOTE_INSPECTOR)
    if (const char* address = getenv("WEBKIT_INSPECTOR_SERVER"))
        initializeRemoteInspectorServer(address);
#endif
}

void WebProcessPool::platformInitializeNetworkProcess(NetworkProcessCreationParameters&)
{
    notImplemented();
}

void WebProcessPool::platformInitializeWebProcess(const WebProcessProxy&, WebProcessCreationParameters&)
{
    notImplemented();
}

void WebProcessPool::platformInvalidateContext()
{
    notImplemented();
}

void WebProcessPool::platformResolvePathsForSandboxExtensions()
{
}

} // namespace WebKit
