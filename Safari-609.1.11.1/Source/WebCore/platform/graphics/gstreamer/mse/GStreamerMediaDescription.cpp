/*
 * Copyright (C) 2016 Metrological Group B.V.
 * Copyright (C) 2016 Igalia S.L
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
#include "GStreamerMediaDescription.h"
#include "GStreamerCommon.h"

#include <gst/pbutils/pbutils.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/WTFString.h>

#if ENABLE(VIDEO) && USE(GSTREAMER) && ENABLE(MEDIA_SOURCE)

namespace WebCore {

AtomString GStreamerMediaDescription::codec() const
{
    return m_codecName;
}

bool GStreamerMediaDescription::isVideo() const
{
    return doCapsHaveType(m_caps.get(), GST_VIDEO_CAPS_TYPE_PREFIX);
}

bool GStreamerMediaDescription::isAudio() const
{
    return doCapsHaveType(m_caps.get(), GST_AUDIO_CAPS_TYPE_PREFIX);
}

bool GStreamerMediaDescription::isText() const
{
    // FIXME: Implement proper text track support.
    return false;
}

AtomString GStreamerMediaDescription::extractCodecName()
{
    GRefPtr<GstCaps> originalCaps = m_caps;

    if (areEncryptedCaps(originalCaps.get())) {
        originalCaps = adoptGRef(gst_caps_copy(originalCaps.get()));
        GstStructure* structure = gst_caps_get_structure(originalCaps.get(), 0);

        if (!gst_structure_has_field(structure, "original-media-type"))
            return AtomString();

        gst_structure_set_name(structure, gst_structure_get_string(structure, "original-media-type"));
        // Remove the DRM related fields from the caps.
        for (int j = 0; j < gst_structure_n_fields(structure); ++j) {
            const char* fieldName = gst_structure_nth_field_name(structure, j);

            if (g_str_has_prefix(fieldName, "protection-system")
                || g_str_has_prefix(fieldName, "original-media-type"))
                gst_structure_remove_field(structure, fieldName);
        }
    }

    GUniquePtr<gchar> description(gst_pb_utils_get_codec_description(originalCaps.get()));
    String codecName(description.get());

    // Report "H.264 (Main Profile)" and "H.264 (High Profile)" just as "H.264" to allow changes between both variants
    // go unnoticed to the SourceBuffer layer.
    if (codecName.startsWith("H.264")) {
        size_t braceStart = codecName.find(" (");
        size_t braceEnd = codecName.find(")");
        if (braceStart != notFound && braceEnd != notFound)
            codecName.remove(braceStart, braceEnd - braceStart);
    }

    return codecName;
}

} // namespace WebCore.

#endif // USE(GSTREAMER)
