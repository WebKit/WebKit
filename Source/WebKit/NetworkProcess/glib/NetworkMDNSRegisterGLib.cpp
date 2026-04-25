/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Comcast Inc.
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
#include "NetworkMDNSRegister.h"

#if ENABLE(WEB_RTC) && USE(GLIB)

#include "NetworkConnectionToWebProcess.h"
#include <WebCore/MDNSRegisterError.h>
#include <gio/gio.h>
#include <wtf/Markable.h>
#include <wtf/UUID.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/MakeString.h>

namespace WebKit {

NetworkMDNSRegister::NetworkMDNSRegister(NetworkConnectionToWebProcess& connection)
    : m_connection(connection)
{
    m_cancellable = adoptGRef(g_cancellable_new());
    g_dbus_proxy_new_for_bus(G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE, nullptr, "org.freedesktop.Avahi", "/", "org.freedesktop.Avahi.Server", m_cancellable.get(), [](GObject*, GAsyncResult* result, gpointer userData) {
        GUniqueOutPtr<GError> error;
        auto proxy = adoptGRef(g_dbus_proxy_new_for_bus_finish(result, &error.outPtr()));
        if (!error) {
            auto self = static_cast<NetworkMDNSRegister*>(userData);
            self->m_dbusProxy = WTF::move(proxy);
            return;
        }

#if PLATFORM(GTK)
        // Check if the connection to the system bus was refused, don't log an error when that
        // happens because it is expected when running as a Flatpak app.
        if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
            return;
#endif
        if (!g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
            LOG_ERROR("Unable to connect to the Avahi daemon: %s", error->message);
    }, this);
}

NetworkMDNSRegister::~NetworkMDNSRegister()
{
    g_cancellable_cancel(m_cancellable.get());
}

void NetworkMDNSRegister::registerMDNSName(WebCore::ScriptExecutionContextIdentifier documentIdentifier, const String& ipAddress, CompletionHandler<void(const String&, std::optional<WebCore::MDNSRegisterError>)>&& completionHandler)
{
    auto name = makeString(WTF::UUID::createVersion4(), ".local"_s);

    if (ipAddress == "0.0.0.0"_s || ipAddress == "::"_s) {
        completionHandler(name, WebCore::MDNSRegisterError::BadParameter);
        return;
    }

    if (!m_dbusProxy) {
        completionHandler(name, WebCore::MDNSRegisterError::Internal);
        return;
    }

    m_registeredNames.add(name);
    m_perDocumentRegisteredNames.ensure(documentIdentifier, [] {
        return Vector<String>();
    }).iterator->value.append(name);

    Ref connection = m_connection.get();
    auto request = makeUnique<PendingRegistrationRequest>(connection.get(), WTF::move(name), ipAddress, sessionID(), WTF::move(completionHandler));

    request->cancellable = m_cancellable;

    g_dbus_proxy_call(m_dbusProxy.get(), "EntryGroupNew", g_variant_new("()"), G_DBUS_CALL_FLAGS_NONE, -1, m_cancellable.get(), [](GObject* object, GAsyncResult* result, gpointer userData) {
        std::unique_ptr<PendingRegistrationRequest> request;
        request.reset(reinterpret_cast<PendingRegistrationRequest*>(userData));

        GUniqueOutPtr<GError> error;
        auto variant = adoptGRef(g_dbus_proxy_call_finish(G_DBUS_PROXY(object), result, &error.outPtr()));
        if (error) {
            // We might have access to the system bus but Avahi is not activatable/installed, don't
            // log an error when that is the case.
            if (!g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED)
                && !g_error_matches(error.get(), G_DBUS_ERROR, G_DBUS_ERROR_SERVICE_UNKNOWN)) {
                LOG_ERROR("Unable to add Avahi entry group: %s", error->message);
            }
            request->completionHandler(request->name, WebCore::MDNSRegisterError::Internal);
            return;
        }
        GUniqueOutPtr<char> objectPath;
        g_variant_get(variant.get(), "(o)", &objectPath.outPtr());

        auto* cancellable = request->cancellable.get();
        g_dbus_proxy_new_for_bus(G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_NONE, nullptr, "org.freedesktop.Avahi", objectPath.get(), "org.freedesktop.Avahi.EntryGroup", cancellable, [](GObject*, GAsyncResult* result, gpointer userData) {
            std::unique_ptr<PendingRegistrationRequest> request;
            request.reset(reinterpret_cast<PendingRegistrationRequest*>(userData));

            GUniqueOutPtr<GError> error;
            auto dbusProxy = adoptGRef(g_dbus_proxy_new_for_bus_finish(result, &error.outPtr()));

            if (error) {
                if (!g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
                    LOG_ERROR("Unable to create DBus proxy for Avahi entry group: %s", error->message);
                request->completionHandler(request->name, WebCore::MDNSRegisterError::Internal);
                return;
            }

            int interface = -1;
            int protocol = -1;
            uint32_t flags = 16; // AVAHI_PUBLISH_NO_REVERSE

            auto* cancellable = request->cancellable.get();
            auto requestName = request->name.ascii();
            auto requestAddress = request->address.ascii();
            g_dbus_proxy_call(dbusProxy.get(), "AddAddress", g_variant_new("(iiuss)", interface, protocol, flags, requestName.data(), requestAddress.data()), G_DBUS_CALL_FLAGS_NONE, -1, cancellable, [](GObject* object, GAsyncResult* result, gpointer userData) {
                std::unique_ptr<PendingRegistrationRequest> request;
                request.reset(reinterpret_cast<PendingRegistrationRequest*>(userData));

                GUniqueOutPtr<GError> error;
                auto proxy = G_DBUS_PROXY(object);
                auto finalResult = adoptGRef(g_dbus_proxy_call_finish(proxy, result, &error.outPtr()));
                if (!finalResult) {
                    if (!g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
                        LOG_ERROR("Unable to register MDNS address %s to Avahi: %s", request->name.ascii().data(), error->message);
                    request->completionHandler(request->name, WebCore::MDNSRegisterError::Internal);
                    return;
                }

                auto* cancellable = request->cancellable.get();
                g_dbus_proxy_call(proxy, "Commit", g_variant_new("()"), G_DBUS_CALL_FLAGS_NONE, -1, cancellable, [](GObject* object, GAsyncResult* result, gpointer userData) {
                    std::unique_ptr<PendingRegistrationRequest> request;
                    request.reset(reinterpret_cast<PendingRegistrationRequest*>(userData));

                    GUniqueOutPtr<GError> error;
                    auto finalResult = adoptGRef(g_dbus_proxy_call_finish(G_DBUS_PROXY(object), result, &error.outPtr()));
                    if (!finalResult) {
                        if (!g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
                            LOG_ERROR("Unable to commit MDNS address %s to Avahi: %s", request->name.ascii().data(), error->message);
                        request->completionHandler(request->name, WebCore::MDNSRegisterError::Internal);
                        return;
                    }
                    request->completionHandler(request->name, { });
                }, request.release());
            }, request.release());
        }, request.release());
    }, request.release());
}

} // namespace WebKit

#endif // ENABLE(WEB_RTC) && USE(GLIB)
