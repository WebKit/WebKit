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

#include "LegacyGlobalSettings.h"
#include "MemoryPressureMonitor.h"
#include "WebMemoryPressureHandler.h"
#include "WebProcessCreationParameters.h"
#include <WebCore/PlatformDisplay.h>
#include <wtf/FileSystem.h>
#include <wtf/glib/Sandbox.h>

#if ENABLE(REMOTE_INSPECTOR)
#include <JavaScriptCore/RemoteInspector.h>
#endif

#if USE(GSTREAMER)
#include <WebCore/GStreamerCommon.h>
#endif

#if USE(WPE_RENDERER)
#include <wpe/wpe.h>
#endif

#if PLATFORM(GTK) || (PLATFORM(WPE) && ENABLE(WPE_PLATFORM))
#include "ScreenManager.h"
#endif

#if PLATFORM(GTK)
#if USE(EGL)
#include "AcceleratedBackingStoreDMABuf.h"
#endif
#include "GtkSettingsManager.h"
#endif

#if PLATFORM(WPE) && ENABLE(WPE_PLATFORM)
#include <wpe/wpe-platform.h>
#endif

namespace WebKit {

void WebProcessPool::platformInitialize(NeedsGlobalStaticInitialization)
{
    m_alwaysUsesComplexTextCodePath = true;

    if (const char* forceComplexText = getenv("WEBKIT_FORCE_COMPLEX_TEXT"))
        m_alwaysUsesComplexTextCodePath = !strcmp(forceComplexText, "1");

#if OS(LINUX)
    if (!MemoryPressureMonitor::disabled())
        installMemoryPressureHandler();
#endif
}

void WebProcessPool::platformInitializeWebProcess(const WebProcessProxy& process, WebProcessCreationParameters& parameters)
{
#if PLATFORM(WPE) && ENABLE(WPE_PLATFORM)
    bool usingWPEPlatformAPI = !!g_type_class_peek(WPE_TYPE_DISPLAY);
#endif

#if USE(GBM)
#if PLATFORM(WPE) && ENABLE(WPE_PLATFORM)
    if (usingWPEPlatformAPI)
        parameters.renderDeviceFile = String::fromUTF8(wpe_render_node_device());
    else
        parameters.renderDeviceFile = WebCore::PlatformDisplay::sharedDisplay().drmRenderNodeFile();
#else
    parameters.renderDeviceFile = WebCore::PlatformDisplay::sharedDisplay().drmRenderNodeFile();
#endif
#endif

#if PLATFORM(GTK) && USE(EGL)
    parameters.dmaBufRendererBufferMode = AcceleratedBackingStoreDMABuf::rendererBufferMode();
#elif PLATFORM(WPE) && ENABLE(WPE_PLATFORM)
    if (usingWPEPlatformAPI) {
#if USE(GBM)
        if (!parameters.renderDeviceFile.isEmpty())
            parameters.dmaBufRendererBufferMode.add(DMABufRendererBufferMode::Hardware);
#endif
        parameters.dmaBufRendererBufferMode.add(DMABufRendererBufferMode::SharedMemory);
    }
#endif

#if PLATFORM(WPE)
    parameters.isServiceWorkerProcess = process.isRunningServiceWorkers();

    if (!parameters.isServiceWorkerProcess && parameters.dmaBufRendererBufferMode.isEmpty()) {
        parameters.hostClientFileDescriptor = UnixFileDescriptor { wpe_renderer_host_create_client(), UnixFileDescriptor::Adopt };
        parameters.implementationLibraryName = FileSystem::fileSystemRepresentation(String::fromLatin1(wpe_loader_get_loaded_implementation_library_name()));
    }
#endif

    parameters.memoryCacheDisabled = m_memoryCacheDisabled || LegacyGlobalSettings::singleton().cacheModel() == CacheModel::DocumentViewer;

#if OS(LINUX)
    if (MemoryPressureMonitor::disabled())
        parameters.shouldSuppressMemoryPressureHandler = true;
#endif

#if USE(GSTREAMER)
    parameters.gstreamerOptions = WebCore::extractGStreamerOptionsFromCommandLine();
#endif

#if PLATFORM(GTK) && !USE(GTK4)
    parameters.useSystemAppearanceForScrollbars = m_configuration->useSystemAppearanceForScrollbars();
#endif

    parameters.memoryPressureHandlerConfiguration = m_configuration->memoryPressureHandlerConfiguration();

    parameters.disableFontHintingForTesting = m_configuration->disableFontHintingForTesting();

    GApplication* app = g_application_get_default();
    if (app)
        parameters.applicationID = String::fromLatin1(g_application_get_application_id(app));
    parameters.applicationName = String::fromLatin1(g_get_application_name());

#if ENABLE(REMOTE_INSPECTOR)
    parameters.inspectorServerAddress = Inspector::RemoteInspector::inspectorServerAddress();
#endif

#if USE(ATSPI)
    parameters.accessibilityBusAddress = [this] {
        if (auto* address = getenv("WEBKIT_A11Y_BUS_ADDRESS"))
            return String::fromUTF8(address);

        if (m_sandboxEnabled) {
            String& address = sandboxedAccessibilityBusAddress();
            if (!address.isNull())
                return address;
        }

        return WebCore::PlatformDisplay::sharedDisplay().accessibilityBusAddress();
    }();
#endif

#if PLATFORM(GTK)
    parameters.gtkSettings = GtkSettingsManager::singleton().settingsState();
    parameters.screenProperties = ScreenManager::singleton().collectScreenProperties();
#endif

#if PLATFORM(WPE) && ENABLE(WPE_PLATFORM)
    if (usingWPEPlatformAPI)
        parameters.screenProperties = ScreenManager::singleton().collectScreenProperties();
#endif
}

void WebProcessPool::platformInvalidateContext()
{
}

void WebProcessPool::platformResolvePathsForSandboxExtensions()
{
}

} // namespace WebKit
