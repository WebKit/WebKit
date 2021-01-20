/*
 * Copyright (C) 2015 Igalia S.L.
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
#include "InstallMissingMediaPluginsPermissionRequest.h"

#if ENABLE(VIDEO) && USE(GSTREAMER) && !USE(GSTREAMER_FULL)
#include "WebPageMessages.h"
#include "WebPageProxy.h"
#include <wtf/text/CString.h>

namespace WebKit {

InstallMissingMediaPluginsPermissionRequest::InstallMissingMediaPluginsPermissionRequest(WebPageProxy& page,  const String& details, const String& description)
    : m_page(&page)
    , m_details(details)
    , m_description(description)
{
}

InstallMissingMediaPluginsPermissionRequest::~InstallMissingMediaPluginsPermissionRequest()
{
}

void InstallMissingMediaPluginsPermissionRequest::allow(GUniquePtr<GstInstallPluginsContext>&& context)
{
    if (!m_page->hasRunningProcess())
        return;

    CString detail = m_details.utf8();
    const char* detailArray[2] = { detail.data(), nullptr };
    ref();
    GstInstallPluginsReturn result = gst_install_plugins_async(detailArray, context.get(), [](GstInstallPluginsReturn result, gpointer userData) {
        RefPtr<InstallMissingMediaPluginsPermissionRequest> request = adoptRef(static_cast<InstallMissingMediaPluginsPermissionRequest*>(userData));
        request->didEndRequestInstallMissingMediaPlugins(static_cast<uint32_t>(result));
        }, this);

    if (result != GST_INSTALL_PLUGINS_STARTED_OK) {
        // If the installer didn't start, the callback will not be called, so remove the ref manually.
        deref();
        didEndRequestInstallMissingMediaPlugins(static_cast<uint32_t>(result));
        WTFLogAlways("Missing GStreamer Plugin: %s\n", detail.data());
    }
}

void InstallMissingMediaPluginsPermissionRequest::deny()
{
    didEndRequestInstallMissingMediaPlugins(static_cast<uint32_t>(GST_INSTALL_PLUGINS_USER_ABORT));
}

void InstallMissingMediaPluginsPermissionRequest::didEndRequestInstallMissingMediaPlugins(uint32_t result)
{
    if (!m_page->hasRunningProcess())
        return;

    m_page->send(Messages::WebPage::DidEndRequestInstallMissingMediaPlugins(result));
}

} // namespace WebKit

#endif // ENABLE(VIDEO) && USE(GSTREAMER) && !USE(GSTREAMER_FULL)
