/*
 * Copyright (C) 2022 Igalia S.L.
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

#include "config.h"
#include "NotificationService.h"

#include "WebNotification.h"
#include <WebCore/Image.h>
#include <WebCore/NotificationResources.h>
#include <WebCore/RefPtrCairo.h>
#include <cairo.h>
#include <gio/gdesktopappinfo.h>
#include <gio/gio.h>
#include <glib/gi18n-lib.h>
#include <mutex>
#include <unistd.h>
#include <wtf/FastMalloc.h>
#include <wtf/RunLoop.h>
#include <wtf/SafeStrerror.h>
#include <wtf/Seconds.h>
#include <wtf/StdLibExtras.h>
#include <wtf/UUID.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/glib/RunLoopSourcePriority.h>
#include <wtf/glib/Sandbox.h>
#include <wtf/text/CString.h>

#if PLATFORM(GTK)
#include <WebCore/GtkVersioning.h>
#endif

namespace WebKit {

static const Seconds s_dbusCallTimeout = 20_ms;

class IconCache {
    WTF_MAKE_FAST_ALLOCATED;
public:
    IconCache()
        : m_timer(RunLoop::main(), this, &IconCache::timerFired)
    {
        m_timer.setPriority(RunLoopSourcePriority::ReleaseUnusedResourcesTimer);
    }

    ~IconCache()
    {
        removeUnusedIcons(true);
    }

    const char* iconPath(const String& iconURL, const RefPtr<WebCore::Image>& icon)
    {
        if (!icon)
            return nullptr;

        auto writeIconToTemporaryFile = [](const RefPtr<WebCore::Image>& icon) -> CString {
            auto nativeImage = icon->nativeImage();
            if (!nativeImage)
                return { };

            auto surface = nativeImage->platformImage();
            if (!surface)
                return { };

            int fd;
            GUniqueOutPtr<char> filename;
            GUniqueOutPtr<GError> error;
            if ((fd = g_file_open_tmp(nullptr, &filename.outPtr(), &error.outPtr())) == -1) {
                g_warning("Failed to create temporary file for notification icon: %s", error->message);
                return { };
            }

            auto status = cairo_surface_write_to_png_stream(surface.get(), [](void* userData, const unsigned char* data, unsigned length) -> cairo_status_t {
                int fd = *static_cast<int*>(userData);
                while (length) {
                    auto written = write(fd, data, length);
                    if (written == -1)
                        return CAIRO_STATUS_WRITE_ERROR;

                    length -= written;
                    data += written;
                }
                return CAIRO_STATUS_SUCCESS;
            }, &fd);

            close(fd);

            return status == CAIRO_STATUS_SUCCESS ? filename.get() : CString();
        };

        auto addResult = m_iconCache.add(iconURL, std::pair<uint32_t, CString>({ 0, CString() }));
        if (addResult.isNewEntry) {
            auto path = writeIconToTemporaryFile(icon);
            if (path.isNull()) {
                m_iconCache.remove(addResult.iterator);
                return nullptr;
            }
            addResult.iterator->value = { 1, WTFMove(path) };
        } else
            addResult.iterator->value.first++;

        return std::get<CString>(addResult.iterator->value.second).data();
    }

    GBytes* iconBytes(const String& iconURL, const RefPtr<WebCore::Image>& icon)
    {
        if (!icon)
            return nullptr;

        auto writeIconToBuffer = [](const RefPtr<WebCore::Image>& icon) -> GRefPtr<GBytes> {
            auto nativeImage = icon->nativeImage();
            if (!nativeImage)
                return nullptr;

            auto surface = nativeImage->platformImage();
            if (!surface)
                return nullptr;

            GRefPtr<GByteArray> buffer = adoptGRef(g_byte_array_new());
            auto status = cairo_surface_write_to_png_stream(surface.get(), [](void* userData, const unsigned char* data, unsigned length) -> cairo_status_t {
                auto* buffer = static_cast<GByteArray*>(userData);
                g_byte_array_append(buffer, data, length);
                return CAIRO_STATUS_SUCCESS;
            }, buffer.get());

            if (status != CAIRO_STATUS_SUCCESS)
                return nullptr;

            return adoptGRef(g_byte_array_free_to_bytes(buffer.leakRef()));
        };

        auto addResult = m_iconCache.add(iconURL, std::pair<uint32_t, GRefPtr<GBytes>>({ 0, nullptr }));
        if (addResult.isNewEntry) {
            auto bytes = writeIconToBuffer(icon);
            if (!bytes) {
                m_iconCache.remove(addResult.iterator);
                return nullptr;
            }
            addResult.iterator->value = { 1, WTFMove(bytes) };
        } else
            addResult.iterator->value.first++;

        return std::get<GRefPtr<GBytes>>(addResult.iterator->value.second).get();
    }

    void unuseIcon(const String& iconURL)
    {
        auto it = m_iconCache.find(iconURL);
        if (it == m_iconCache.end())
            return;

        if (!it->value.first)
            return;

        bool isNull = WTF::switchOn(it->value.second,
            [](const CString& path) {
                return path.isNull();
            },
            [](const GRefPtr<GBytes>& bytes) {
                return !bytes.get();
            });
        if (isNull)
            return;

        if (--it->value.first)
            return;

        m_timer.startOneShot(5_min);
    }

    void removeUnusedIcons(bool force)
    {
        m_iconCache.removeIf([force](auto& it) -> bool {
            if (!it.value.first || force) {
                WTF::switchOn(it.value.second,
                    [](const CString& path) {
                        if (!path.isNull()) {
                            if (unlink(path.data()) == -1)
                                WTFLogAlways("Failed to remove cached notification icon %s: %s", path.data(), safeStrerror(errno).data());
                        }
                    },
                    [](const GRefPtr<GBytes>&) {
                    });
                return true;
            }
            return false;
        });
    }

    void timerFired()
    {
        removeUnusedIcons(false);
    }

private:
    HashMap<String, std::pair<uint32_t, std::variant<CString, GRefPtr<GBytes>>>> m_iconCache;
    RunLoop::Timer m_timer;
};

static IconCache& iconCache()
{
    static std::unique_ptr<IconCache> cache = makeUnique<IconCache>();
    return *cache;
}

NotificationService& NotificationService::singleton()
{
    static std::once_flag onceFlag;
    static LazyNeverDestroyed<NotificationService> service;

    std::call_once(onceFlag, [] {
        service.construct();
    });

    return service;
}

NotificationService::NotificationService()
{
    const char* busName = shouldUsePortal() ? "org.freedesktop.portal.Desktop" : "org.freedesktop.Notifications";
    const char* objectPath = shouldUsePortal() ? "/org/freedesktop/portal/desktop" : "/org/freedesktop/Notifications";
    const char* interfaceName = shouldUsePortal() ? "org.freedesktop.portal.Notification" : "org.freedesktop.Notifications";
    GUniqueOutPtr<GError> error;
    m_proxy = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SESSION, G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES, nullptr, busName, objectPath, interfaceName, nullptr, &error.outPtr());
    if (!m_proxy) {
        g_warning("Failed to connect to notification service at %s: %s", busName, error->message);
        return;
    }

    if (!shouldUsePortal()) {
        GRefPtr<GVariant> capabilities = adoptGRef(g_dbus_proxy_call_sync(m_proxy.get(), "GetCapabilities", nullptr, G_DBUS_CALL_FLAGS_NONE, s_dbusCallTimeout.millisecondsAs<int>(), nullptr, &error.outPtr()));
        if (!capabilities) {
            g_warning("Failed to get capabilities from notification server: %s", error->message);
            m_proxy = nullptr;
            return;
        }

        processCapabilities(capabilities.get());
    }

    g_signal_connect(m_proxy.get(), "g-signal", G_CALLBACK(handleSignal), this);
}

void NotificationService::processCapabilities(GVariant* variant)
{
    GUniqueOutPtr<GVariantIter> iter;
    g_variant_get(variant, "(as)", &iter.outPtr());
    const char* capability;
    while (g_variant_iter_loop(iter.get(), "&s", &capability)) {
        if (!g_strcmp0(capability, "action-icons"))
            m_capabilities.add(Capabilities::ActionIcons);
        else if (!g_strcmp0(capability, "actions"))
            m_capabilities.add(Capabilities::Actions);
        else if (!g_strcmp0(capability, "body"))
            m_capabilities.add(Capabilities::Body);
        else if (!g_strcmp0(capability, "body-hyperlinks"))
            m_capabilities.add(Capabilities::BodyHyperlinks);
        else if (!g_strcmp0(capability, "body-images"))
            m_capabilities.add(Capabilities::BodyImages);
        else if (!g_strcmp0(capability, "body-markup"))
            m_capabilities.add(Capabilities::BodyMarkup);
        else if (!g_strcmp0(capability, "icon-multi"))
            m_capabilities.add(Capabilities::IconMulti);
        else if (!g_strcmp0(capability, "icon-static"))
            m_capabilities.add(Capabilities::IconStatic);
        else if (!g_strcmp0(capability, "persistence"))
            m_capabilities.add(Capabilities::Persistence);
        else if (!g_strcmp0(capability, "sound"))
            m_capabilities.add(Capabilities::Sound);
    }
}

static const char* applicationIcon(const char* applicationID)
{
    static std::optional<CString> appIcon;
    if (!appIcon) {
        appIcon = [applicationID]() -> CString {
            if (!applicationID)
                return { };

#if PLATFORM(GTK)
            if (auto* iconTheme = gtk_icon_theme_get_for_display(gdk_display_get_default())) {
                if (gtk_icon_theme_has_icon(iconTheme, applicationID))
                    return applicationID;
            }
#endif

            GUniquePtr<char> desktopFileID(g_strdup_printf("%s.desktop", applicationID));
            GRefPtr<GDesktopAppInfo> appInfo = adoptGRef(g_desktop_app_info_new(desktopFileID.get()));
            if (!appInfo)
                return { };

            auto* icon = g_app_info_get_icon(G_APP_INFO(appInfo.get()));
            if (!icon)
                return { };

            if (G_IS_FILE_ICON(icon)) {
                GUniquePtr<char> uri(g_file_get_uri(g_file_icon_get_file(G_FILE_ICON(icon))));
                return uri.get();
            }

            if (G_IS_THEMED_ICON(icon)) {
                const char* const* iconNames = g_themed_icon_get_names(G_THEMED_ICON(icon));
                return iconNames[0];
            }

            return { };
        }();
    }

    return appIcon->data();
}

bool NotificationService::showNotification(const WebNotification& notification, const RefPtr<WebCore::NotificationResources>& resources)
{
    if (!m_proxy)
        return false;

    auto findNotificationByTag = [this](const String& tag) -> Notification {
        if (tag.isEmpty())
            return Notification();

        uint64_t notificationID = 0;
        for (const auto& it : m_notifications) {
            if (it.value.tag == tag) {
                notificationID = it.key;
                break;
            }
        }

        return notificationID ? m_notifications.take(notificationID) : Notification({ 0, { }, tag, { } });
    };

    auto addResult = m_notifications.add(notification.notificationID(), findNotificationByTag(notification.tag()));
    addResult.iterator->value.iconURL = notification.iconURL();

    if (shouldUsePortal()) {
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);

        g_variant_builder_add(&builder, "{sv}", "title", g_variant_new_string(notification.title().utf8().data()));
        g_variant_builder_add(&builder, "{sv}", "body", g_variant_new_string(notification.body().utf8().data()));
        g_variant_builder_add(&builder, "{sv}", "default-action", g_variant_new_string("default"));
        if (resources) {
            if (auto* bytes = iconCache().iconBytes(notification.iconURL(), resources->icon())) {
                GRefPtr<GIcon> icon = adoptGRef(g_bytes_icon_new(bytes));
                g_variant_builder_add(&builder, "{sv}", "icon", g_icon_serialize(icon.get()));
            }
        }
        addResult.iterator->value.portalID = createVersion4UUIDString();
        g_dbus_proxy_call(m_proxy.get(), "AddNotification", g_variant_new("(s@a{sv})", addResult.iterator->value.portalID.utf8().data(), g_variant_builder_end(&builder)),
            G_DBUS_CALL_FLAGS_NONE, -1, nullptr, [](GObject* source, GAsyncResult* result, gpointer) {
                GUniqueOutPtr<GError> error;
                GRefPtr<GVariant> variant = adoptGRef(g_dbus_proxy_call_finish(G_DBUS_PROXY(source), result, &error.outPtr()));
                if (error)
                    g_warning("Failed to call org.freedesktop.portal.Notification.AddNotification: %s", error->message);
            }, nullptr);
    } else {
        GVariantBuilder actionsBuilder;
        g_variant_builder_init(&actionsBuilder, G_VARIANT_TYPE("as"));

        if (m_capabilities.contains(Capabilities::Actions)) {
            g_variant_builder_add(&actionsBuilder, "s", "default");
            // TRANSLATORS: the default action for a desktop notification created by a website.
            g_variant_builder_add(&actionsBuilder, "s", _("Acknowledge"));
        }

        const char* applicationID = nullptr;
        if (auto* app = g_application_get_default())
            applicationID = g_application_get_application_id(app);

        GVariantBuilder hintsBuilder;
        g_variant_builder_init(&hintsBuilder, G_VARIANT_TYPE("a{sv}"));
        if (applicationID)
            g_variant_builder_add(&hintsBuilder, "{sv}", "desktop-entry", g_variant_new_string(applicationID));
        if (m_capabilities.contains(Capabilities::Persistence) && notification.isPersistentNotification())
            g_variant_builder_add(&hintsBuilder, "{sv}", "resident", g_variant_new_boolean(TRUE));
        if (resources && m_capabilities.contains(Capabilities::IconStatic)) {
            if (const char* iconPath = iconCache().iconPath(notification.iconURL(), resources->icon()))
                g_variant_builder_add(&hintsBuilder, "{sv}", "image-path", g_variant_new_string(iconPath));
        }

        auto* value = static_cast<GValue*>(fastZeroedMalloc(sizeof(GValue)));
        g_value_init(value, G_TYPE_UINT64);
        g_value_set_uint64(value, notification.notificationID());

        CString body;
        if (m_capabilities.contains(Capabilities::Body))
            body = notification.body().utf8();

        const char* appIcon = applicationIcon(applicationID);

        g_dbus_proxy_call(m_proxy.get(), "Notify", g_variant_new(
            "(susssasa{sv}i)",
            g_get_application_name(), addResult.iterator->value.id, appIcon ? appIcon : "",
            notification.title().utf8().data(), body.data(),
            &actionsBuilder, &hintsBuilder, -1
            ), G_DBUS_CALL_FLAGS_NONE, -1, nullptr, [](GObject* source, GAsyncResult* result, gpointer userData) {
                GUniqueOutPtr<GError> error;
                GRefPtr<GVariant> notificationID = adoptGRef(g_dbus_proxy_call_finish(G_DBUS_PROXY(source), result, &error.outPtr()));
                if (!notificationID)
                    g_warning("Failed to show notification: %s", error->message);

                guint32 id;
                g_variant_get(notificationID.get(), "(u)", &id);

                auto* value = static_cast<GValue*>(userData);
                NotificationService::singleton().setNotificationID(g_value_get_uint64(value), id);
                g_value_unset(value);
                fastFree(value);
            }, value);
    }
    return true;
}

void NotificationService::cancelNotification(uint64_t webNotificationID)
{
    if (!m_proxy)
        return;

    auto it = m_notifications.find(webNotificationID);
    if (it == m_notifications.end())
        return;

    if (shouldUsePortal()) {
        if (it->value.portalID.isEmpty())
            return;

        g_dbus_proxy_call(m_proxy.get(), "RemoveNotification", g_variant_new("(s)", it->value.portalID.utf8().data()), G_DBUS_CALL_FLAGS_NONE, -1, nullptr,
            [](GObject* source, GAsyncResult* result, gpointer) {
                GUniqueOutPtr<GError> error;
                GRefPtr<GVariant> variant = adoptGRef(g_dbus_proxy_call_finish(G_DBUS_PROXY(source), result, &error.outPtr()));
                if (error)
                    g_warning("Failed to call org.freedesktop.portal.Notification.RemoveNotification: %s", error->message);
            }, nullptr);
    } else {
        if (!it->value.id)
            return;

        g_dbus_proxy_call(m_proxy.get(), "CloseNotification", g_variant_new("(u)", it->value.id), G_DBUS_CALL_FLAGS_NONE, -1, nullptr,
            [](GObject* source, GAsyncResult* result, gpointer) {
                GUniqueOutPtr<GError> error;
                GRefPtr<GVariant> variant = adoptGRef(g_dbus_proxy_call_finish(G_DBUS_PROXY(source), result, &error.outPtr()));
                if (error)
                    g_warning("Failed to call org.freedesktop.Notifications.CloseNotification: %s", error->message);
            }, nullptr);
    }
}

void NotificationService::setNotificationID(uint64_t webNotificationID, uint32_t notificationID)
{
    auto it = m_notifications.find(webNotificationID);
    if (it == m_notifications.end())
        return;

    it->value.id = notificationID;
}

uint64_t NotificationService::findNotification(uint32_t notificationID)
{
    for (const auto& it : m_notifications) {
        if (it.value.id == notificationID)
            return it.key;
    }

    return 0;
}

uint64_t NotificationService::findNotification(const String& notificationID)
{
    for (const auto& it : m_notifications) {
        if (it.value.portalID == notificationID)
            return it.key;
    }

    return 0;
}

void NotificationService::handleSignal(GDBusProxy* proxy, char*, char* signal, GVariant* parameters, NotificationService* service)
{
    if (!g_strcmp0(signal, "NotificationClosed")) {
        guint32 id, reason;
        g_variant_get(parameters, "(uu)", &id, &reason);
        service->didCloseNotification(service->findNotification(id));
    } else if (!g_strcmp0(signal, "ActionInvoked")) {
        if (!g_strcmp0(g_dbus_proxy_get_interface_name(proxy), "org.freedesktop.portal.Notification")) {
            const char* id;
            const char* action;
            g_variant_get(parameters, "(&s&s@av)", &id, &action, nullptr);
            if (!g_strcmp0(action, "default")) {
                if (auto notificationID = service->findNotification(String::fromUTF8(id))) {
                    service->didClickNotification(notificationID);
                    service->didCloseNotification(notificationID);
                }
            }
        } else {
            guint32 id;
            const char* action;
            g_variant_get(parameters, "(u&s)", &id, &action);
            if (!g_strcmp0(action, "default"))
                service->didClickNotification(service->findNotification(id));
        }
    }
}

void NotificationService::didClickNotification(uint64_t notificationID)
{
    if (!notificationID)
        return;

    for (auto* observer : m_observers)
        observer->didClickNotification(notificationID);
}

void NotificationService::didCloseNotification(uint64_t notificationID)
{
    if (!notificationID)
        return;

    for (auto* observer : m_observers)
        observer->didCloseNotification(notificationID);

    auto notification = m_notifications.take(notificationID);
    if (!notification.iconURL.isEmpty())
        iconCache().unuseIcon(notification.iconURL);
}

void NotificationService::addObserver(Observer& observer)
{
    m_observers.add(&observer);
}

void NotificationService::removeObserver(Observer& observer)
{
    m_observers.remove(&observer);
}

} // namespace WebKit
