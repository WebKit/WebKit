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

#include "DRMDevice.h"
#include "LegacyGlobalSettings.h"
#include "MemoryPressureMonitor.h"
#include "WebMemoryPressureHandler.h"
#include "WebProcessCreationParameters.h"
#include <WebCore/PlatformDisplay.h>
#include <WebCore/SystemSettings.h>
#include <wtf/FileSystem.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/glib/Application.h>
#include <wtf/glib/Sandbox.h>

#if USE(ATSPI)
#include <wtf/UUID.h>
#endif

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
#include "AcceleratedBackingStoreDMABuf.h"
#include "Display.h"
#endif

#if PLATFORM(WPE) && ENABLE(WPE_PLATFORM)
#include <wpe/wpe-platform.h>
#endif

#if !USE(SYSTEM_MALLOC) && OS(LINUX)
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
#include <bmalloc/valgrind.h>
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
#endif

namespace WebKit {

void WebProcessPool::platformInitialize(NeedsGlobalStaticInitialization)
{
    m_alwaysUsesComplexTextCodePath = true;

    if (const char* forceComplexText = getenv("WEBKIT_FORCE_COMPLEX_TEXT"))
        m_alwaysUsesComplexTextCodePath = !strcmp(forceComplexText, "1");

#if !ENABLE(2022_GLIB_API)
    if (const char* forceSandbox = getenv("WEBKIT_FORCE_SANDBOX")) {
        if (!strcmp(forceSandbox, "1"))
            setSandboxEnabled(true);
        else {
            static bool once = false;
            if (!once) {
                g_warning("WEBKIT_FORCE_SANDBOX no longer allows disabling the sandbox. Use WEBKIT_DISABLE_SANDBOX_THIS_IS_DANGEROUS=1 instead.");
                once = true;
            }
        }
    }
#endif

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
    parameters.renderDeviceFile = drmRenderNodeDevice();
#endif

#if PLATFORM(GTK)
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

#if PLATFORM(GTK) && !USE(GTK4) && USE(CAIRO)
    parameters.useSystemAppearanceForScrollbars = m_configuration->useSystemAppearanceForScrollbars();
#endif

    parameters.memoryPressureHandlerConfiguration = m_configuration->memoryPressureHandlerConfiguration();

    parameters.disableFontHintingForTesting = m_configuration->disableFontHintingForTesting();

    parameters.applicationID = String::fromUTF8(WTF::applicationID().span());
    parameters.applicationName = String::fromLatin1(g_get_application_name());

#if ENABLE(REMOTE_INSPECTOR)
    parameters.inspectorServerAddress = Inspector::RemoteInspector::inspectorServerAddress();
#endif

#if USE(ATSPI)
    static const char* address = getenv("WEBKIT_A11Y_BUS_ADDRESS");
    if (address)
        parameters.accessibilityBusAddress = String::fromUTF8(address);
    else
        parameters.accessibilityBusAddress = m_sandboxEnabled && shouldUseBubblewrap() ? sandboxedAccessibilityBusAddress() : accessibilityBusAddress();

    parameters.accessibilityBusName = accessibilityBusName();
#endif

    parameters.systemSettings = WebCore::SystemSettings::singleton().settingsState();

#if PLATFORM(GTK)
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

void WebProcessPool::setSandboxEnabled(bool enabled)
{
    if (m_sandboxEnabled == enabled)
        return;

    if (!enabled) {
#if !ENABLE(2022_GLIB_API)
        if (const char* forceSandbox = getenv("WEBKIT_FORCE_SANDBOX")) {
            if (!strcmp(forceSandbox, "1"))
                return;
        }
#endif
        m_sandboxEnabled = false;
#if USE(ATSPI)
        m_sandboxedAccessibilityBusAddress = String();
#endif
        return;
    }

#if !USE(SYSTEM_MALLOC)
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
    if (RUNNING_ON_VALGRIND)
        return;
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
#endif

    if (const char* disableSandbox = getenv("WEBKIT_DISABLE_SANDBOX_THIS_IS_DANGEROUS")) {
        if (strcmp(disableSandbox, "0"))
            return;
    }

    m_sandboxEnabled = true;
#if USE(ATSPI)
    if (shouldUseBubblewrap())
        m_sandboxedAccessibilityBusAddress = makeString("unix:path="_s, FileSystem::pathByAppendingComponent(FileSystem::stringFromFileSystemRepresentation(sandboxedUserRuntimeDirectory().data()), "at-spi-bus"_s));
#endif
}

#if USE(ATSPI)
static const String& queryAccessibilityBusAddress()
{
    static LazyNeverDestroyed<String> address;
    static std::once_flag onceKey;
    std::call_once(onceKey, [] {
        GRefPtr<GDBusConnection> sessionBus = adoptGRef(g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, nullptr));
        if (sessionBus.get()) {
            GRefPtr<GDBusMessage> message = adoptGRef(g_dbus_message_new_method_call("org.a11y.Bus", "/org/a11y/bus", "org.a11y.Bus", "GetAddress"));
            g_dbus_message_set_body(message.get(), g_variant_new("()"));
            GRefPtr<GDBusMessage> reply = adoptGRef(g_dbus_connection_send_message_with_reply_sync(sessionBus.get(), message.get(),
                G_DBUS_SEND_MESSAGE_FLAGS_NONE, 30000, nullptr, nullptr, nullptr));
            if (reply) {
                GUniqueOutPtr<GError> error;
                if (g_dbus_message_to_gerror(reply.get(), &error.outPtr())) {
                    if (!g_error_matches(error.get(), G_DBUS_ERROR, G_DBUS_ERROR_SERVICE_UNKNOWN))
                        WTFLogAlways("Can't find a11y bus: %s", error->message);
                } else {
                    GUniqueOutPtr<char> a11yAddress;
                    g_variant_get(g_dbus_message_get_body(reply.get()), "(s)", &a11yAddress.outPtr());
                    address.construct(String::fromUTF8(a11yAddress.get()));
                    return;
                }
            }
        }
        address.construct();
    });
    return address.get();
}

const String& WebProcessPool::accessibilityBusAddress() const
{
    if (m_accessibilityBusAddress.has_value())
        return m_accessibilityBusAddress.value();

    const char* addressEnv = getenv("AT_SPI_BUS_ADDRESS");
    if (addressEnv && *addressEnv) {
        m_accessibilityBusAddress = String::fromUTF8(addressEnv);
        return m_accessibilityBusAddress.value();
    }

#if PLATFORM(GTK)
    auto address = Display::singleton().accessibilityBusAddress();
    if (!address.isEmpty()) {
        m_accessibilityBusAddress = WTFMove(address);
        return m_accessibilityBusAddress.value();
    }
#endif

    m_accessibilityBusAddress = queryAccessibilityBusAddress();
    return m_accessibilityBusAddress.value();
}

const String& WebProcessPool::accessibilityBusName() const
{
    RELEASE_ASSERT(m_accessibilityBusName.has_value());
    return m_accessibilityBusName.value();
}

const String& WebProcessPool::sandboxedAccessibilityBusAddress() const
{
    return m_sandboxedAccessibilityBusAddress;
}

const String& WebProcessPool::generateNextAccessibilityBusName()
{
    m_accessibilityBusName = makeString(String::fromUTF8(WTF::applicationID().span()), ".Sandboxed.WebProcess-"_s, WTF::UUID::createVersion4());
    RELEASE_ASSERT(g_dbus_is_name(m_accessibilityBusName.value().utf8().data()));
    RELEASE_ASSERT(!g_dbus_is_unique_name(m_accessibilityBusName.value().utf8().data()));

    return accessibilityBusName();
}

#endif

} // namespace WebKit
