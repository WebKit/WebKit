/*
 * Copyright (C) 2019 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/Noncopyable.h>
#include <wtf/RunLoop.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/text/CString.h>

typedef struct _GDBusProxy GDBusProxy;

namespace WebCore {
class GeolocationPositionData;
}

namespace WebKit {

class GeoclueGeolocationProvider {
    WTF_MAKE_NONCOPYABLE(GeoclueGeolocationProvider); WTF_MAKE_FAST_ALLOCATED;
public:
    GeoclueGeolocationProvider();
    ~GeoclueGeolocationProvider();

    using UpdateNotifyFunction = Function<void(WebCore::GeolocationPositionData&&, std::optional<CString> error)>;
    void start(UpdateNotifyFunction&&);
    void stop();
    void setEnableHighAccuracy(bool);

private:
    enum class LocationProviderSource : uint8_t { Unknown, Portal, Geoclue };

    void destroyState();
    void destroyStateLater();

    void acquirePortalProxy();
    void setupPortalProxy(GRefPtr<GDBusProxy>&&);
    void createPortalSession();
    void startPortalSession();
    void portalLocationUpdated(GVariant*);
    void stopPortalSession();

    void createGeoclueManager();
    void setupGeoclueManager(GRefPtr<GDBusProxy>&&);
    void createGeoclueClient(const char*);
    void setupGeoclueClient(GRefPtr<GDBusProxy>&&);
    void requestAccuracyLevel();
    void createLocation(const char*);
    void locationUpdated(GRefPtr<GDBusProxy>&&);
    void didFail(CString);

    void startGeoclueClient();
    void stopGeoclueClient();

    static void clientLocationUpdatedCallback(GDBusProxy*, gchar*, gchar*, GVariant*, gpointer);

    bool m_isRunning { false };
    bool m_isHighAccuracyEnabled { false };
    struct {
        GRefPtr<GDBusProxy> manager;
        GRefPtr<GDBusProxy> client;
    } m_geoclue;
    struct {
        GRefPtr<GDBusProxy> locationPortal;
        std::optional<String> senderName;
        std::optional<String> sessionId;
        unsigned locationUpdatedSignalId;
        unsigned responseSignalId;
    } m_portal;
    GRefPtr<GCancellable> m_cancellable;
    UpdateNotifyFunction m_updateNotifyFunction;
    LocationProviderSource m_sourceType { LocationProviderSource::Unknown };
    RunLoop::Timer m_destroyLaterTimer;
};

} // namespace WebKit
