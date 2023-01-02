/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2011 Motorola Mobility, Inc.  All rights reserved.
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

#include "WebKitExtensionManager.h"
#include "WebKitWebExtensionPrivate.h"
#include "WebPage.h"
#include "WebProcessCreationParameters.h"

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

#if PLATFORM(GTK) && !USE(GTK4)
#include <WebCore/ScrollbarThemeGtk.h>
#endif

#if ENABLE(MEDIA_STREAM)
#include "UserMediaCaptureManager.h"
#endif

#if OS(LINUX)
#include <wtf/linux/RealTimeThreads.h>
#endif

#if USE(ATSPI)
#include <WebCore/AccessibilityAtspi.h>
#endif

#if PLATFORM(GTK)
#include "GtkSettingsManagerProxy.h"
#include <gtk/gtk.h>
#endif

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
}

void WebProcess::platformInitializeWebProcess(WebProcessCreationParameters& parameters)
{
#if ENABLE(MEDIA_STREAM)
    addSupplement<UserMediaCaptureManager>();
#endif

#if PLATFORM(WPE)
    if (!parameters.isServiceWorkerProcess) {
        auto& implementationLibraryName = parameters.implementationLibraryName;
        if (!implementationLibraryName.isNull() && implementationLibraryName.data()[0] != '\0')
            wpe_loader_init(parameters.implementationLibraryName.data());

        RELEASE_ASSERT(is<PlatformDisplayLibWPE>(PlatformDisplay::sharedDisplay()));
        downcast<PlatformDisplayLibWPE>(PlatformDisplay::sharedDisplay()).initialize(parameters.hostClientFileDescriptor.release());
    }
#endif

#if PLATFORM(WAYLAND)
    if (PlatformDisplay::sharedDisplay().type() == PlatformDisplay::Type::Wayland && !parameters.isServiceWorkerProcess) {
        auto hostClientFileDescriptor = parameters.hostClientFileDescriptor.release();
        if (hostClientFileDescriptor != -1) {
            wpe_loader_init(parameters.implementationLibraryName.data());
            m_wpeDisplay = WebCore::PlatformDisplayLibWPE::create();
            if (!m_wpeDisplay->initialize(hostClientFileDescriptor))
                m_wpeDisplay = nullptr;
        }
    }
#endif

#if USE(GSTREAMER)
    WebCore::setGStreamerOptionsFromUIProcess(WTFMove(parameters.gstreamerOptions));
#endif

#if PLATFORM(GTK) && !USE(GTK4)
    setUseSystemAppearanceForScrollbars(parameters.useSystemAppearanceForScrollbars);
#endif

    if (parameters.memoryPressureHandlerConfiguration)
        MemoryPressureHandler::singleton().setConfiguration(WTFMove(*parameters.memoryPressureHandlerConfiguration));

    if (!parameters.applicationID.isEmpty())
        WebCore::setApplicationID(parameters.applicationID);

    if (!parameters.applicationName.isEmpty())
        WebCore::setApplicationName(parameters.applicationName);

#if ENABLE(REMOTE_INSPECTOR)
    if (!parameters.inspectorServerAddress.isNull())
        Inspector::RemoteInspector::setInspectorServerAddress(WTFMove(parameters.inspectorServerAddress));
#endif

#if USE(ATSPI)
    AccessibilityAtspi::singleton().connect(parameters.accessibilityBusAddress);
#endif

#if PLATFORM(GTK)
    GtkSettingsManagerProxy::singleton().applySettings(WTFMove(parameters.gtkSettings));
#endif

}

void WebProcess::platformSetWebsiteDataStoreParameters(WebProcessDataStoreParameters&&)
{
}

void WebProcess::platformTerminate()
{
}

void WebProcess::sendMessageToWebExtension(UserMessage&& message)
{
    if (auto* extension = WebKitExtensionManager::singleton().extension())
        webkitWebExtensionDidReceiveUserMessage(extension, WTFMove(message));
}

#if PLATFORM(GTK) && !USE(GTK4)
void WebProcess::setUseSystemAppearanceForScrollbars(bool useSystemAppearanceForScrollbars)
{
    static_cast<ScrollbarThemeGtk&>(ScrollbarTheme::theme()).setUseSystemAppearance(useSystemAppearanceForScrollbars);
}
#endif

void WebProcess::grantAccessToAssetServices(Vector<WebKit::SandboxExtension::Handle>&&)
{
}

void WebProcess::revokeAccessToAssetServices()
{
}

void WebProcess::switchFromStaticFontRegistryToUserFontRegistry(Vector<WebKit::SandboxExtension::Handle>&&)
{
}

} // namespace WebKit
