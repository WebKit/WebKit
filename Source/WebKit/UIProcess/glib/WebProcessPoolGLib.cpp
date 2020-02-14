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

#include "WebMemoryPressureHandler.h"
#include "WebProcessCreationParameters.h"
#include <JavaScriptCore/RemoteInspectorServer.h>
#include <WebCore/PlatformDisplay.h>
#include <wtf/FileSystem.h>
#include <wtf/glib/GUniquePtr.h>

#if USE(GSTREAMER)
#include <WebCore/GStreamerCommon.h>
#endif

#if PLATFORM(WAYLAND)
#if USE(WPE_RENDERER)
#include <wpe/fdo-egl.h>
#else
#include "WaylandCompositor.h"
#endif
#endif

#if USE(WPE_RENDERER)
#include <wpe/wpe.h>
#endif

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

static bool memoryPressureMonitorDisabled()
{
    static const char* disableMemoryPressureMonitor = getenv("WEBKIT_DISABLE_MEMORY_PRESSURE_MONITOR");
    return disableMemoryPressureMonitor && !strcmp(disableMemoryPressureMonitor, "1");
}

void WebProcessPool::platformInitialize()
{
#if PLATFORM(GTK)
    m_alwaysUsesComplexTextCodePath = true;
#endif
    if (const char* forceComplexText = getenv("WEBKIT_FORCE_COMPLEX_TEXT"))
        m_alwaysUsesComplexTextCodePath = !strcmp(forceComplexText, "1");

#if ENABLE(REMOTE_INSPECTOR)
    if (const char* address = g_getenv("WEBKIT_INSPECTOR_SERVER"))
        initializeRemoteInspectorServer(address);
#endif

    if (!memoryPressureMonitorDisabled())
        installMemoryPressureHandler();
}

void WebProcessPool::platformInitializeWebProcess(const WebProcessProxy& process, WebProcessCreationParameters& parameters)
{
#if PLATFORM(WPE)
    parameters.isServiceWorkerProcess = process.isServiceWorkerProcess();

    if (!parameters.isServiceWorkerProcess) {
        parameters.hostClientFileDescriptor = wpe_renderer_host_create_client();
        parameters.implementationLibraryName = FileSystem::fileSystemRepresentation(wpe_loader_get_loaded_implementation_library_name());
    }
#endif

#if PLATFORM(WAYLAND)
    if (WebCore::PlatformDisplay::sharedDisplay().type() == WebCore::PlatformDisplay::Type::Wayland) {
#if USE(WPE_RENDERER)
        wpe_loader_init("libWPEBackend-fdo-1.0.so");
        if (wpe_fdo_initialize_for_egl_display(WebCore::PlatformDisplay::sharedDisplay().eglDisplay())) {
            parameters.hostClientFileDescriptor = wpe_renderer_host_create_client();
            parameters.implementationLibraryName = FileSystem::fileSystemRepresentation(wpe_loader_get_loaded_implementation_library_name());
        }
#elif USE(EGL)
        parameters.waylandCompositorDisplayName = WaylandCompositor::singleton().displayName();
#endif
    }
#endif

    parameters.memoryCacheDisabled = m_memoryCacheDisabled || cacheModel() == CacheModel::DocumentViewer;
    parameters.proxySettings = m_networkProxySettings;

    if (memoryPressureMonitorDisabled())
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
