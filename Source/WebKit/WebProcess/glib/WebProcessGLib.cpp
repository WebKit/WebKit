/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2011 Motorola Mobility, Inc. All rights reserved.
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
#include "WebProcess.h"

#include "Logging.h"
#include "SystemSettingsManager.h"
#include "WebKitWebProcessExtensionPrivate.h"
#include "WebPage.h"
#include "WebProcessCreationParameters.h"
#include "WebProcessExtensionManager.h"
#include "WebSystemSoundDelegate.h"
#include <WebCore/PlatformScreen.h>
#include <WebCore/ProcessCapabilities.h>
#include <WebCore/RenderTheme.h>
#include <WebCore/ScreenProperties.h>
#include <WebCore/SystemSoundManager.h>

#if ENABLE(REMOTE_INSPECTOR)
#include <JavaScriptCore/RemoteInspector.h>
#endif

#if USE(GSTREAMER)
#include <WebCore/GStreamerCommon.h>
#endif

#include <WebCore/ApplicationGLib.h>
#include <WebCore/MemoryCache.h>

#if USE(WPE_RENDERER)
#include <WebCore/PlatformDisplayLibWPE.h>
#include <wpe/wpe.h>
#endif

#if USE(GBM)
#include <WebCore/DRMDeviceManager.h>
#include <WebCore/GBMDevice.h>
#endif

#if PLATFORM(GTK) || PLATFORM(WPE)
#include <WebCore/PlatformDisplayGBM.h>
#include <WebCore/PlatformDisplaySurfaceless.h>
#endif

#if OS(ANDROID)
#include <WebCore/PlatformDisplayAndroid.h>
#endif

#if PLATFORM(GTK) || OS(ANDROID)
#include <WebCore/PlatformDisplayDefault.h>
#endif

#if ENABLE(MEDIA_STREAM)
#include "UserMediaCaptureManager.h"
#endif

#if HAVE(MALLOC_TRIM)
#include <malloc.h>
#endif

#if OS(LINUX)
#include <wtf/linux/RealTimeThreads.h>
#endif

#if USE(ATSPI)
#include <WebCore/AccessibilityAtspi.h>
#endif

#define RELEASE_LOG_SESSION_ID (m_sessionID ? m_sessionID->toUInt64() : 0)
#define WEBPROCESS_RELEASE_LOG(channel, fmt, ...) RELEASE_LOG(channel, "%p - [sessionID=%" PRIu64 "] WebProcess::" fmt, this, RELEASE_LOG_SESSION_ID, ##__VA_ARGS__)
#define WEBPROCESS_RELEASE_LOG_ERROR(channel, fmt, ...) RELEASE_LOG_ERROR(channel, "%p - [sessionID=%" PRIu64 "] WebProcess::" fmt, this, RELEASE_LOG_SESSION_ID, ##__VA_ARGS__)

namespace WebKit {

using namespace WebCore;

void WebProcess::stopRunLoop()
{
    // Pages are normally closed after Close message is received from the UI
    // process, but it can happen that the connection is closed before the
    // Close message is processed because the UI process close the socket
    // right after sending the Close message. Close here any pending page to
    // ensure the threaded compositor is invalidated and GL resources
    // released (see https://bugs.webkit.org/show_bug.cgi?id=217655).
    for (auto& webPage : copyToVector(m_pageMap.values()))
        webPage->close();

    if (auto* display = PlatformDisplay::sharedDisplayIfExists())
        display->clearGLContexts();

    AuxiliaryProcess::stopRunLoop();
}

void WebProcess::platformSetCacheModel(CacheModel cacheModel)
{
    WebCore::MemoryCache::singleton().setDisabled(cacheModel == CacheModel::DocumentViewer);
}

void WebProcess::platformInitializeProcess(const AuxiliaryProcessInitializationParameters&)
{
#if OS(LINUX)
    // Disable RealTimeThreads in WebProcess initially, since it depends on having a visible web page.
    RealTimeThreads::singleton().setEnabled(false);
#endif

    addSupplementWithoutRefCountedCheck<SystemSettingsManager>();
}

void WebProcess::initializePlatformDisplayIfNeeded() const
{
    if (PlatformDisplay::sharedDisplayIfExists())
        return;

#if USE(GBM)
    if (m_rendererBufferTransportMode.contains(RendererBufferTransportMode::Hardware)) {
        bool disabled = false;
#if PLATFORM(GTK)
        const char* disableGBM = getenv("WEBKIT_DMABUF_RENDERER_DISABLE_GBM");
        IGNORE_CLANG_WARNINGS_BEGIN("unsafe-buffer-usage-in-libc-call")
        disabled = disableGBM && strcmp(disableGBM, "0");
        IGNORE_CLANG_WARNINGS_END
#endif
        if (!disabled) {
            if (auto device = DRMDeviceManager::singleton().mainGBMDevice(DRMDeviceManager::NodeType::Render)) {
                PlatformDisplay::setSharedDisplay(PlatformDisplayGBM::create(device->device()));
                return;
            }
        }
    }
#endif

#if OS(ANDROID)
    if (auto display = PlatformDisplayAndroid::create()) {
        PlatformDisplay::setSharedDisplay(WTF::move(display));
        return;
    }
#endif

    if (auto display = PlatformDisplaySurfaceless::create()) {
        PlatformDisplay::setSharedDisplay(WTF::move(display));
        return;
    }

#if PLATFORM(GTK) || OS(ANDROID)
    if (auto display = PlatformDisplayDefault::create()) {
        PlatformDisplay::setSharedDisplay(WTF::move(display));
        return;
    }
#endif

    WTFLogAlways("Could not create EGL display: no supported platform available. Aborting...");
    CRASH();
}

void WebProcess::platformInitializeWebProcess(WebProcessCreationParameters& parameters)
{
    const char* enableCPURendering = getenv("WEBKIT_SKIA_ENABLE_CPU_RENDERING");
    IGNORE_CLANG_WARNINGS_BEGIN("unsafe-buffer-usage-in-libc-call")
    if (enableCPURendering && strcmp(enableCPURendering, "0"))
        ProcessCapabilities::setCanUseAcceleratedBuffers(false);
    IGNORE_CLANG_WARNINGS_END

#if ENABLE(MEDIA_STREAM)
    addSupplementWithoutRefCountedCheck<UserMediaCaptureManager>();
#endif

#if USE(GBM)
    DRMDeviceManager::singleton().initializeMainDevice(WTF::move(parameters.drmDevice));
#endif

    m_rendererBufferTransportMode = parameters.rendererBufferTransportMode;
#if PLATFORM(WPE)
#if USE(WPE_RENDERER)
    if (!parameters.isServiceWorkerProcess) {
        if (m_rendererBufferTransportMode.isEmpty()) {
            auto& implementationLibraryName = parameters.implementationLibraryName;
            if (!implementationLibraryName.isNull() && implementationLibraryName.data()[0] != '\0')
                wpe_loader_init(parameters.implementationLibraryName.data());
            PlatformDisplay::setSharedDisplay(PlatformDisplayLibWPE::create(parameters.hostClientFileDescriptor.release()));
        } else
            initializePlatformDisplayIfNeeded();
    }
#else
    initializePlatformDisplayIfNeeded();
#endif
#endif

    m_availableInputDevices = parameters.availableInputDevices;

#if USE(GSTREAMER)
    WebCore::setGStreamerOptionsFromUIProcess(WTF::move(parameters.gstreamerOptions));
#endif

    if (parameters.memoryPressureHandlerConfiguration)
        MemoryPressureHandler::singleton().setConfiguration(WTF::move(*parameters.memoryPressureHandlerConfiguration));

    if (!parameters.applicationID.isEmpty())
        WebCore::setApplicationID(parameters.applicationID);

    if (!parameters.applicationName.isEmpty())
        WebCore::setApplicationName(parameters.applicationName);

#if ENABLE(REMOTE_INSPECTOR)
    if (!parameters.inspectorServerAddress.isNull())
        Inspector::RemoteInspector::setInspectorServerAddress(WTF::move(parameters.inspectorServerAddress));
#endif

#if USE(ATSPI)
    AccessibilityAtspi::singleton().connect(parameters.accessibilityBusAddress, parameters.accessibilityBusName);
#endif

    if (parameters.disableFontHintingForTesting)
        FontRenderOptions::singleton().disableHintingForTesting();

#if PLATFORM(GTK)
    WebCore::setScreenProperties(parameters.screenProperties);

    WebCore::SystemSoundManager::singleton().setSystemSoundDelegate(makeUnique<WebSystemSoundDelegate>());
#endif

#if PLATFORM(WPE) && ENABLE(WPE_PLATFORM)
    if (!m_rendererBufferTransportMode.isEmpty())
        WebCore::setScreenProperties(parameters.screenProperties);
#endif
}

void WebProcess::platformSetWebsiteDataStoreParameters(WebProcessDataStoreParameters&&)
{
}

void WebProcess::platformTerminate()
{
}

void WebProcess::sendMessageToWebProcessExtension(UserMessage&& message)
{
    if (auto* extension = WebProcessExtensionManager::singleton().extension())
        webkitWebProcessExtensionDidReceiveUserMessage(extension, WTF::move(message));
}

void WebProcess::grantAccessToAssetServices(Vector<WebKit::SandboxExtension::Handle>&&)
{
}

void WebProcess::revokeAccessToAssetServices()
{
}

void WebProcess::switchFromStaticFontRegistryToUserFontRegistry(Vector<WebKit::SandboxExtension::Handle>&&)
{
}

void WebProcess::releaseSystemMallocMemory()
{
#if HAVE(MALLOC_TRIM)
#if !RELEASE_LOG_DISABLED
    const auto startTime = MonotonicTime::now();
#endif

    malloc_trim(0);

#if !RELEASE_LOG_DISABLED
    const auto endTime = MonotonicTime::now();
    WEBPROCESS_RELEASE_LOG(ProcessSuspension, "releaseSystemMallocMemory: took %.2fms", (endTime - startTime).milliseconds());
#endif
#endif
}

#if PLATFORM(GTK) || PLATFORM(WPE)
void WebProcess::setScreenProperties(const WebCore::ScreenProperties& properties)
{
#if PLATFORM(GTK) || (PLATFORM(WPE) && ENABLE(WPE_PLATFORM))
    WebCore::setScreenProperties(properties);
#endif
    for (auto& page : m_pageMap.values())
        page->screenPropertiesDidChange();
}

std::optional<AvailableInputDevices> WebProcess::primaryPointingDevice() const
{
    if (m_availableInputDevices.contains(AvailableInputDevices::Mouse))
        return AvailableInputDevices::Mouse;
    if (m_availableInputDevices.contains(AvailableInputDevices::Touchscreen))
        return AvailableInputDevices::Touchscreen;
    return std::nullopt;
}

void WebProcess::setAvailableInputDevices(OptionSet<AvailableInputDevices> availableInputDevices)
{
    m_availableInputDevices = availableInputDevices;
}
#endif // PLATFORM(GTK) || PLATFORM(WPE)

} // namespace WebKit

#undef RELEASE_LOG_SESSION_ID
#undef WEBPROCESS_RELEASE_LOG
#undef WEBPROCESS_RELEASE_LOG_ERROR
