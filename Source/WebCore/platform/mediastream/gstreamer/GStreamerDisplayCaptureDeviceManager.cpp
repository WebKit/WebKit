/*
 * Copyright (C) 2018 Metrological Group B.V.
 * Author: Thibault Saunier <tsaunier@igalia.com>
 * Author: Alejandro G. Castro <alex@igalia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"

#if ENABLE(MEDIA_STREAM) && USE(GSTREAMER)
#include "GStreamerCaptureDeviceManager.h"

#include "GStreamerVideoCaptureSource.h"
#include "PipeWireCaptureDevice.h"
#include <wtf/UUID.h>
#include <wtf/glib/GMallocString.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

GStreamerDisplayCaptureDeviceManager& GStreamerDisplayCaptureDeviceManager::singleton()
{
    static NeverDestroyed<GStreamerDisplayCaptureDeviceManager> manager;
    return manager;
}

GStreamerDisplayCaptureDeviceManager::GStreamerDisplayCaptureDeviceManager()
{
}

GStreamerDisplayCaptureDeviceManager::~GStreamerDisplayCaptureDeviceManager()
{
    for (auto& sourceId : m_sessions.keys())
        stopSource(sourceId);
}

void GStreamerDisplayCaptureDeviceManager::computeCaptureDevices(CompletionHandler<void()>&& callback)
{
    m_devices.clear();

    CaptureDevice screenCaptureDevice(createVersion4UUIDString(), CaptureDevice::DeviceType::Screen, "Capture Screen"_s);
    screenCaptureDevice.setEnabled(true);
    m_devices.append(WTF::move(screenCaptureDevice));
    callback();
}

CaptureSourceOrError GStreamerDisplayCaptureDeviceManager::createDisplayCaptureSource(const CaptureDevice& device, MediaDeviceHashSalts&& hashSalts, const MediaConstraints* constraints)
{
    const auto it = m_sessions.find(device.persistentId());
    if (it != m_sessions.end()) {
        auto& node = it->value;
        PipeWireCaptureDevice pipewireCaptureDevice { *node, device.persistentId(), device.type(), device.label(), device.groupId() };
        return GStreamerVideoCaptureSource::createPipewireSource(WTF::move(pipewireCaptureDevice), WTF::move(hashSalts), constraints);
    }

    if (!m_portal)
        m_portal = DesktopPortalScreenCast::create();
    if (!m_portal)
        return CaptureSourceOrError({ { }, MediaAccessDenialReason::PermissionDenied });

    auto session = m_portal->createScreencastSession();
    if (!session)
        return CaptureSourceOrError({ { }, MediaAccessDenialReason::PermissionDenied });

    // FIXME: Maybe check this depending on device.type().
    auto outputType = GStreamerDisplayCaptureDeviceManager::PipeWireOutputType::Monitor | GStreamerDisplayCaptureDeviceManager::PipeWireOutputType::Window;

    GVariantBuilder options;
    g_variant_builder_init(&options, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&options, "{sv}", "types", g_variant_new_uint32(static_cast<uint32_t>(outputType)));
    g_variant_builder_add(&options, "{sv}", "multiple", g_variant_new_boolean(false));

    if (auto version = m_portal->getProperty("version")) {
        if (g_variant_get_uint32(version.get()) >= 2) {
            // Enable embedded cursor. FIXME: Should be checked in the constraints.
            g_variant_builder_add(&options, "{sv}", "cursor_mode", g_variant_new_uint32(2));
        }
    }

    auto result = session->selectSources(options);
    if (!result)
        return CaptureSourceOrError({ { }, MediaAccessDenialReason::PermissionDenied });

    GUniqueOutPtr<char> objectPathChars;
    g_variant_get(result.get(), "(o)", &objectPathChars.outPtr());
    auto objectPath = GMallocString::unsafeAdoptFromUTF8(WTF::move(objectPathChars));
    m_portal->waitResponseSignal(toCStringView(objectPath));

    result = session->start();
    if (!result)
        return CaptureSourceOrError({ { }, MediaAccessDenialReason::PermissionDenied });

    std::optional<uint32_t> nodeId;
    g_variant_get(result.get(), "(o)", &objectPathChars.outPtr());
    objectPath = GMallocString::unsafeAdoptFromUTF8(WTF::move(objectPathChars));
    m_portal->waitResponseSignal(toCStringView(objectPath), [&nodeId](GVariant* parameters) mutable {
        uint32_t portalResponse;
        GRefPtr<GVariant> responseData;
        g_variant_get(parameters, "(u@a{sv})", &portalResponse, &responseData.outPtr());

        if (portalResponse) {
            WTFLogAlways("User cancelled the Start request or an unknown error happened");
            return;
        }

        // The portal interface allows multiple streams but we care only about the first one.
        GUniqueOutPtr<GVariantIter> iter;
        if (g_variant_lookup(responseData.get(), "streams", "a(ua{sv})", &iter.outPtr())) {
            auto variant = adoptGRef(g_variant_iter_next_value(iter.get()));
            if (!variant) {
                WTFLogAlways("Stream list is empty");
                return;
            }

            uint32_t streamId;
            GRefPtr<GVariant> options;
            g_variant_get(variant.get(), "(u@a{sv})", &streamId, &options.outPtr());
            nodeId = streamId;
        }
    });

    if (!nodeId) {
        WTFLogAlways("Unable to retrieve display capture session data");
        return CaptureSourceOrError({ { } , MediaAccessDenialReason::PermissionDenied });
    }

    auto nodeData = session->openPipewireRemote();
    if (!nodeData)
        return CaptureSourceOrError({ { }, MediaAccessDenialReason::PermissionDenied });

    nodeData->objectId = *nodeId;
    PipeWireCaptureDevice pipewireCaptureDevice { *nodeData, device.persistentId(), device.type(), device.label(), device.groupId() };
    m_sessions.add(device.persistentId(), makeUnique<PipeWireNodeData>(WTF::move(*nodeData)));
    return GStreamerVideoCaptureSource::createPipewireSource(WTF::move(pipewireCaptureDevice), WTF::move(hashSalts), constraints);
}

void GStreamerDisplayCaptureDeviceManager::stopSource(const String& persistentID)
{
    if (!m_portal) [[unlikely]]
        return;

    auto session = m_sessions.take(persistentID);
    m_portal->closeSession(session->path);
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && USE(GSTREAMER)
