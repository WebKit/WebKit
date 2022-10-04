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
#define WEBCORE_GSTREAMER_EME_UTILITIES_WIDEVINE_UUID "edef8ba9-79d6-4ace-a3c8-27dcd51d21ed"
#define WEBCORE_GSTREAMER_EME_UTILITIES_PLAYREADY_UUID "9a04f079-9840-4286-ab92-e65be0885f95"

GST_DEBUG_CATEGORY_EXTERN(webkit_media_common_encryption_decrypt_debug_category);

namespace WebCore {
class InitData {
public:
    InitData() = default;
    // FIXME: We should have an enum for system uuids for better type safety.
    InitData(const String& systemId, GstBuffer* initData)
        : m_systemId(systemId)
    {
        auto mappedInitData = GstMappedOwnedBuffer::create(initData);
        if (!mappedInitData) {
            GST_CAT_LEVEL_LOG(webkit_media_common_encryption_decrypt_debug_category, GST_LEVEL_ERROR, nullptr, "cannot map %s protection data", systemId.utf8().data());
            ASSERT_NOT_REACHED();
        }
        m_payload = extractCencIfNeeded(mappedInitData->createSharedBuffer());
    }

    InitData(const String& systemId, RefPtr<SharedBuffer>&& payload)
        : m_systemId(systemId)
    {
        if (payload)
            m_payload = extractCencIfNeeded(WTFMove(payload));
    }

    RefPtr<SharedBuffer> payload() const { return m_payload; }
    const String& systemId() const { return m_systemId; }
    String payloadContainerType() const
    {
#if GST_CHECK_VERSION(1, 16, 0)
        if (m_systemId == GST_PROTECTION_UNSPECIFIED_SYSTEM_ID ""_s)
            return "webm"_s;
#endif
        return "cenc"_s;
    }

private:
    static RefPtr<SharedBuffer> extractCencIfNeeded(RefPtr<SharedBuffer>&&);
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
                m_availableSystems.append(String::fromLatin1(streamEncryptionAllowedSystems[i]));
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
    static constexpr auto s_ClearKeyUUID = WEBCORE_GSTREAMER_EME_UTILITIES_CLEARKEY_UUID ""_s;
    static constexpr auto s_ClearKeyKeySystem = "org.w3.clearkey"_s;
    static constexpr auto s_WidevineUUID = WEBCORE_GSTREAMER_EME_UTILITIES_WIDEVINE_UUID ""_s;
    static constexpr auto s_WidevineKeySystem = "com.widevine.alpha"_s;
    static constexpr auto s_PlayReadyUUID = WEBCORE_GSTREAMER_EME_UTILITIES_PLAYREADY_UUID ""_s;
    static constexpr std::array<ASCIILiteral, 2> s_PlayReadyKeySystems = { "com.microsoft.playready"_s,  "com.youtube.playready"_s };
#if GST_CHECK_VERSION(1, 16, 0)
    static constexpr auto s_unspecifiedUUID = GST_PROTECTION_UNSPECIFIED_SYSTEM_ID ""_s;
    static constexpr auto s_unspecifiedKeySystem = GST_PROTECTION_UNSPECIFIED_SYSTEM_ID ""_s;
#endif

    static bool isClearKeyKeySystem(const String& keySystem)
    {
        return equalIgnoringASCIICase(keySystem, s_ClearKeyKeySystem);
    }

    static bool isClearKeyUUID(const String& uuid)
    {
        return equalIgnoringASCIICase(uuid, s_ClearKeyUUID);
    }

    static bool isWidevineKeySystem(const String& keySystem)
    {
        return equalIgnoringASCIICase(keySystem, s_WidevineKeySystem);
    }

    static bool isWidevineUUID(const String& uuid)
    {
        return equalIgnoringASCIICase(uuid, s_WidevineUUID);
    }

    static bool isPlayReadyKeySystem(const String& keySystem)
    {
        return equalIgnoringASCIICase(keySystem, s_PlayReadyKeySystems[0]) || equalIgnoringASCIICase(keySystem, s_PlayReadyKeySystems[1]);
    }

    static bool isPlayReadyUUID(const String& uuid)
    {
        return equalIgnoringASCIICase(uuid, s_PlayReadyUUID);
    }

#if GST_CHECK_VERSION(1, 16, 0)
    static bool isUnspecifiedKeySystem(const String& keySystem)
    {
        return equalIgnoringASCIICase(keySystem, s_unspecifiedKeySystem);
    }

    static bool isUnspecifiedUUID(const String& uuid)
    {
        return equalIgnoringASCIICase(uuid, s_unspecifiedUUID);
    }
#endif

    static const char* keySystemToUuid(const String& keySystem)
    {
        if (isClearKeyKeySystem(keySystem))
            return s_ClearKeyUUID;

        if (isWidevineKeySystem(keySystem))
            return s_WidevineUUID;

        if (isPlayReadyKeySystem(keySystem))
            return s_PlayReadyUUID;

#if GST_CHECK_VERSION(1, 16, 0)
        if (isUnspecifiedKeySystem(keySystem))
            return s_unspecifiedUUID;
#endif

        ASSERT_NOT_REACHED();
        return { };
    }

    static const ASCIILiteral& uuidToKeySystem(const String& uuid)
    {
        if (isClearKeyUUID(uuid))
            return s_ClearKeyKeySystem;

        if (isWidevineUUID(uuid))
            return s_WidevineKeySystem;

        if (isPlayReadyUUID(uuid))
            return s_PlayReadyKeySystems[0];

#if GST_CHECK_VERSION(1, 16, 0)
        if (isUnspecifiedUUID(uuid))
            return s_unspecifiedKeySystem;
#endif

        ASSERT_NOT_REACHED();
        static NeverDestroyed<ASCIILiteral> empty(""_s);
        return empty;
    }
};

}

#endif // ENABLE(ENCRYPTED_MEDIA) && USE(GSTREAMER)
