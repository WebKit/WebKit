/*
 * Copyright (C) 2020 Igalia S.L.
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
#include "WebKitInitialize.h"

#include "RemoteInspectorHTTPServer.h"
#include "WebKit2Initialize.h"
#include <JavaScriptCore/RemoteInspector.h>
#include <JavaScriptCore/RemoteInspectorServer.h>
#include <mutex>
#include <wtf/glib/GUniquePtr.h>

namespace WebKit {

#if ENABLE(REMOTE_INSPECTOR)
static void initializeRemoteInspectorServer()
{
    const char* address = g_getenv("WEBKIT_INSPECTOR_SERVER");
    const char* httpAddress = g_getenv("WEBKIT_INSPECTOR_HTTP_SERVER");
    if (!address && !httpAddress)
        return;

    if (Inspector::RemoteInspectorServer::singleton().isRunning())
        return;

    auto parseAddress = [](const char* address, guint64& port) -> GUniquePtr<char> {
        if (!address || !address[0])
            return nullptr;

        GUniquePtr<char> inspectorAddress(g_strdup(address));
        char* portPtr = g_strrstr(inspectorAddress.get(), ":");
        if (!portPtr)
            return nullptr;

        *portPtr = '\0';
        portPtr++;
        port = g_ascii_strtoull(portPtr, nullptr, 10);
        if (!port)
            return nullptr;

        return inspectorAddress;
    };

    guint64 inspectorHTTPPort;
    auto inspectorHTTPAddress = parseAddress(httpAddress, inspectorHTTPPort);
    guint64 inspectorPort;
    auto inspectorAddress = !httpAddress ? parseAddress(address, inspectorPort) : GUniquePtr<char>();
    if (!inspectorHTTPAddress && !inspectorAddress)
        return;

    if (!Inspector::RemoteInspectorServer::singleton().start(inspectorAddress ? inspectorAddress.get() : inspectorHTTPAddress.get(), inspectorAddress ? inspectorPort : 0))
        return;

    Inspector::RemoteInspector::setInspectorServerAddress(address);

    if (httpAddress) {
        inspectorAddress.reset(g_strdup_printf("%s:%u", inspectorHTTPAddress.get(), Inspector::RemoteInspectorServer::singleton().port()));
        Inspector::RemoteInspector::setInspectorServerAddress(inspectorAddress.get());
        RemoteInspectorHTTPServer::singleton().start(inspectorHTTPAddress.get(), inspectorHTTPPort, Inspector::RemoteInspectorServer::singleton().port());
    } else
        Inspector::RemoteInspector::setInspectorServerAddress(address);
}
#endif

void webkitInitialize()
{
    static std::once_flag onceFlag;

    std::call_once(onceFlag, [] {
        InitializeWebKit2();
#if ENABLE(REMOTE_INSPECTOR)
        initializeRemoteInspectorServer();
#endif
    });
}

} // namespace WebKit
