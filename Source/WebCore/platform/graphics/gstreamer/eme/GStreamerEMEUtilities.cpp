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

#include "config.h"
#include "GStreamerEMEUtilities.h"

#if ENABLE(ENCRYPTED_MEDIA) && USE(GSTREAMER)

GST_DEBUG_CATEGORY_EXTERN(webkit_media_player_debug);
#define GST_CAT_DEFAULT webkit_media_player_debug

namespace WebCore {

const char* GStreamerEMEUtilities::s_ClearKeyUUID = WEBCORE_GSTREAMER_EME_UTILITIES_CLEARKEY_UUID;
const char* GStreamerEMEUtilities::s_ClearKeyKeySystem = "org.w3.clearkey";

#if GST_CHECK_VERSION(1, 5, 3)
GstElement* GStreamerEMEUtilities::createDecryptor(const char* protectionSystem)
{
    GstElement* decryptor = nullptr;
    GList* decryptors = gst_element_factory_list_get_elements(GST_ELEMENT_FACTORY_TYPE_DECRYPTOR, GST_RANK_MARGINAL);

    GST_TRACE("looking for decryptor for %s", protectionSystem);

    for (GList* walk = decryptors; !decryptor && walk; walk = g_list_next(walk)) {
        GstElementFactory* factory = reinterpret_cast<GstElementFactory*>(walk->data);

        GST_TRACE("checking factory %s", GST_OBJECT_NAME(factory));

        for (const GList* current = gst_element_factory_get_static_pad_templates(factory); current && !decryptor; current = g_list_next(current)) {
            GstStaticPadTemplate* staticPadTemplate = static_cast<GstStaticPadTemplate*>(current->data);
            GRefPtr<GstCaps> caps = adoptGRef(gst_static_pad_template_get_caps(staticPadTemplate));
            unsigned length = gst_caps_get_size(caps.get());

            GST_TRACE("factory %s caps has size %u", GST_OBJECT_NAME(factory), length);
            for (unsigned i = 0; !decryptor && i < length; ++i) {
                GstStructure* structure = gst_caps_get_structure(caps.get(), i);
                GST_TRACE("checking structure %s", gst_structure_get_name(structure));
                if (gst_structure_has_field_typed(structure, GST_PROTECTION_SYSTEM_ID_CAPS_FIELD, G_TYPE_STRING)) {
                    const char* sysId = gst_structure_get_string(structure, GST_PROTECTION_SYSTEM_ID_CAPS_FIELD);
                    GST_TRACE("structure %s has protection system %s", gst_structure_get_name(structure), sysId);
                    if (!g_ascii_strcasecmp(protectionSystem, sysId)) {
                        GST_DEBUG("found decryptor %s for %s", GST_OBJECT_NAME(factory), protectionSystem);
                        decryptor = gst_element_factory_create(factory, nullptr);
                        break;
                    }
                }
            }
        }
    }
    gst_plugin_feature_list_free(decryptors);
    GST_TRACE("returning decryptor %p", decryptor);
    return decryptor;
}
#else
#error "At least a GStreamer version 1.5.3 is required to enable ENCRYPTED_MEDIA."
#endif

}

#endif // ENABLE(ENCRYPTED_MEDIA) && USE(GSTREAMER)
