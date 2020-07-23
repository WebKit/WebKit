/* GStreamer EME Utilities class
 *
 * Copyright (C) 2017 Metrological
 * Copyright (C) 2017 Igalia S.L
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
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#if ENABLE(ENCRYPTED_MEDIA) && USE(GSTREAMER)

#include "GStreamerCommon.h"
#include "SharedBuffer.h"
#include <gst/gst.h>
#include <wtf/text/WTFString.h>

#define WEBCORE_GSTREAMER_EME_UTILITIES_CLEARKEY_UUID "1077efec-c0b2-4d02-ace3-3c1e52e2fb4b"
#if ENABLE(THUNDER)
#define WEBCORE_GSTREAMER_EME_UTILITIES_WIDEVINE_UUID "edef8ba9-79d6-4ace-a3c8-27dcd51d21ed"
#endif

namespace WebCore {
class InitData {
public:
    InitData()
        : m_payload(SharedBuffer::create()) { }

    // FIXME: We should have an enum for system uuids for better type safety.
    InitData(const String& systemId, GstBuffer* initData)
        : m_systemId(systemId)
    {
        auto mappedInitData = GstMappedOwnedBuffer::create(initData);
        if (!mappedInitData) {
            GST_ERROR("cannot map %s protection data", systemId.utf8().data());
            ASSERT_NOT_REACHED();
        }
        m_payload = mappedInitData->createSharedBuffer();
    }

    InitData(const String& systemId, RefPtr<SharedBuffer>&& payload)
        : m_systemId(systemId)
        , m_payload(WTFMove(payload))
    {
    }

    void append(InitData&& initData)
    {
        // FIXME: There is some confusion here about how to detect the
        // correct "initialization data type", if the system ID is
        // GST_PROTECTION_UNSPECIFIED_SYSTEM_ID, then we know it came
        // from WebM. If the system id is specified with one of the
        // defined ClearKey / Playready / Widevine / etc UUIDs, then
        // we know it's MP4. For the latter case, it does not matter
        // which of the UUIDs it is, so we just overwrite it. This is
        // a quirk of how GStreamer provides protection events, and
        // it's not very robust, so be careful here!
        m_systemId = initData.m_systemId;

        m_payload->append(*initData.payload());
    }

    const RefPtr<SharedBuffer>& payload() const { return m_payload; }
    const String& systemId() const { return m_systemId; }
    String payloadContainerType() const
    {
#if GST_CHECK_VERSION(1, 15, 0)
        if (m_systemId == GST_PROTECTION_UNSPECIFIED_SYSTEM_ID)
            return "webm"_s;
#endif
        return "cenc"_s;
    }

private:
    String m_systemId;
    RefPtr<SharedBuffer> m_payload;
};

class ProtectionSystemEvents {
public:
    using EventVector = Vector<GRefPtr<GstEvent>>;

    explicit ProtectionSystemEvents(GstMessage* message)
    {
        const GstStructure* structure = gst_message_get_structure(message);

        const GValue* streamEncryptionEventsList = gst_structure_get_value(structure, "stream-encryption-events");
        ASSERT(streamEncryptionEventsList && GST_VALUE_HOLDS_LIST(streamEncryptionEventsList));
        unsigned numEvents = gst_value_list_get_size(streamEncryptionEventsList);
        m_events.reserveInitialCapacity(numEvents);
        for (unsigned i = 0; i < numEvents; ++i)
            m_events.uncheckedAppend(GRefPtr<GstEvent>(static_cast<GstEvent*>(g_value_get_boxed(gst_value_list_get_value(streamEncryptionEventsList, i)))));
        const GValue* streamEncryptionAllowedSystemsValue = gst_structure_get_value(structure, "available-stream-encryption-systems");
        const char** streamEncryptionAllowedSystems = reinterpret_cast<const char**>(g_value_get_boxed(streamEncryptionAllowedSystemsValue));
        if (streamEncryptionAllowedSystems) {
            for (unsigned i = 0; streamEncryptionAllowedSystems[i]; ++i)
                m_availableSystems.append(streamEncryptionAllowedSystems[i]);
        }
    }
    const EventVector& events() const { return m_events; }
    const Vector<String>& availableSystems() const { return m_availableSystems; }

private:
    EventVector m_events;
    Vector<String> m_availableSystems;
};


class GStreamerEMEUtilities {

public:
    static constexpr char const* s_ClearKeyUUID = WEBCORE_GSTREAMER_EME_UTILITIES_CLEARKEY_UUID;
    static constexpr char const* s_ClearKeyKeySystem = "org.w3.clearkey";
#if ENABLE(THUNDER)
    static constexpr char const* s_WidevineUUID = WEBCORE_GSTREAMER_EME_UTILITIES_WIDEVINE_UUID;
    static constexpr char const* s_WidevineKeySystem = "com.widevine.alpha";
#endif

    static bool isClearKeyKeySystem(const String& keySystem)
    {
        return equalIgnoringASCIICase(keySystem, s_ClearKeyKeySystem);
    }

#if ENABLE(THUNDER)
    static bool isWidevineKeySystem(const String& keySystem)
    {
        return equalIgnoringASCIICase(keySystem, s_WidevineKeySystem);
    }
#endif

    static const char* keySystemToUuid(const String& keySystem)
    {
        if (isClearKeyKeySystem(keySystem))
            return s_ClearKeyUUID;

#if ENABLE(THUNDER)
        if (isWidevineKeySystem(keySystem))
            return s_WidevineUUID;
#endif

        ASSERT_NOT_REACHED();
        return { };
    }
};

}

#endif // ENABLE(ENCRYPTED_MEDIA) && USE(GSTREAMER)
