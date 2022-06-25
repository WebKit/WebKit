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
#include <limits>
#include <mutex>
#include <wtf/glib/GRefPtr.h>
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

    auto parseAddress = [](const char* address) -> GRefPtr<GSocketAddress> {
        if (!address || !address[0])
            return nullptr;

        GUniquePtr<char> inspectorAddress(g_strdup(address));
        char* portPtr = g_strrstr(inspectorAddress.get(), ":");
        if (!portPtr)
            return nullptr;

        *portPtr = '\0';
        portPtr++;
        auto port = g_ascii_strtoull(portPtr, nullptr, 10);
        if (!port || port > std::numeric_limits<uint16_t>::max())
            return nullptr;

        char* addressPtr = inspectorAddress.get();
        if (addressPtr[0] == '[' && *(portPtr - 2) == ']') {
            // Strip the square brackets.
            addressPtr++;
            *(portPtr - 2) = '\0';
        }

        return adoptGRef(g_inet_socket_address_new_from_string(addressPtr, port));
    };

    auto inspectorHTTPAddress = parseAddress(httpAddress);
    GRefPtr<GSocketAddress> inspectorAddress;
    if (inspectorHTTPAddress)
        inspectorAddress = adoptGRef(G_SOCKET_ADDRESS(g_inet_socket_address_new(g_inet_socket_address_get_address(G_INET_SOCKET_ADDRESS(inspectorHTTPAddress.get())), 0)));
    else
        inspectorAddress = parseAddress(address);
    if (!inspectorHTTPAddress && !inspectorAddress) {
        g_warning("Failed to start remote inspector server on %s: invalid address", address ? address : httpAddress);
        return;
    }

    if (!Inspector::RemoteInspectorServer::singleton().start(WTFMove(inspectorAddress)))
        return;

    if (inspectorHTTPAddress) {
        if (RemoteInspectorHTTPServer::singleton().start(WTFMove(inspectorHTTPAddress), Inspector::RemoteInspectorServer::singleton().port()))
            Inspector::RemoteInspector::setInspectorServerAddress(RemoteInspectorHTTPServer::singleton().inspectorServerAddress().utf8());
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
