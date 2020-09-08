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

#include <JavaScriptCore/RemoteInspectorServer.h>
#include <WebKit/Shared/WebKit2Initialize.h>
#include <wtf/glib/GUniquePtr.h>

namespace WebKit {

#if ENABLE(REMOTE_INSPECTOR)
static void initializeRemoteInspectorServer(const char* address)
{
    if (Inspector::RemoteInspectorServer::singleton().isRunning())
        return;

    if (!address[0])
        return;

    GUniquePtr<char> inspectorAddress(g_strdup(address));
    char* portPtr = g_strrstr(inspectorAddress.get(), ":");
    if (!portPtr)
        return;

    *portPtr = '\0';
    portPtr++;
    guint64 port = g_ascii_strtoull(portPtr, nullptr, 10);
    if (!port)
        return;

    Inspector::RemoteInspectorServer::singleton().start(inspectorAddress.get(), port);
}
#endif

void webkitInitialize()
{
    static std::once_flag onceFlag;

    std::call_once(onceFlag, [] {
        InitializeWebKit2();
#if ENABLE(REMOTE_INSPECTOR)
        if (const char* address = g_getenv("WEBKIT_INSPECTOR_SERVER"))
            initializeRemoteInspectorServer(address);
#endif
    });
}

} // namespace WebKit
