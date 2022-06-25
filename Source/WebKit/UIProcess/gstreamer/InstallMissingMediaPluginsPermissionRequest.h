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

#pragma once

#if ENABLE(VIDEO) && USE(GSTREAMER) && !USE(GSTREAMER_FULL)
#include <WebCore/GUniquePtrGStreamer.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

class WebPageProxy;

class InstallMissingMediaPluginsPermissionRequest : public RefCounted<InstallMissingMediaPluginsPermissionRequest> {
public:
    static Ref<InstallMissingMediaPluginsPermissionRequest> create(WebPageProxy& page, const String& details, const String& description)
    {
        return adoptRef(*new InstallMissingMediaPluginsPermissionRequest(page, details, description));
    }
    ~InstallMissingMediaPluginsPermissionRequest();

    void allow(GUniquePtr<GstInstallPluginsContext>&& = nullptr);
    void deny();

    WebPageProxy& page() const { return *m_page; }
    const String& details() const { return m_details; }
    const String& description() const { return m_description; }

private:
    InstallMissingMediaPluginsPermissionRequest(WebPageProxy&, const String& details, const String& description);

    void didEndRequestInstallMissingMediaPlugins(uint32_t result);

    RefPtr<WebPageProxy> m_page;
    String m_details;
    String m_description;
};

} // namespace WebKit

#else

namespace WebKit {
class InstallMissingMediaPluginsPermissionRequest;
} // namespace WebKit

#endif // ENABLE(VIDEO) && USE(GSTREAMER) && !USE(GSTREAMER_FULL)

