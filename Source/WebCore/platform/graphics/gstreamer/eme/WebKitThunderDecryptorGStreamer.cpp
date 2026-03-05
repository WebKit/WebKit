/* GStreamer Thunder common encryption decryptor
 *
 * Copyright (C) 2020 Metrological
 * Copyright (C) 2020 Igalia S.L
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
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */

#include "config.h"
#include "WebKitThunderDecryptorGStreamer.h"

#if ENABLE(ENCRYPTED_MEDIA) && ENABLE(THUNDER) && USE(GSTREAMER)

#include "CDMProxyThunder.h"
#include "GStreamerCommon.h"
#include "GStreamerEMEUtilities.h"
#include <wtf/glib/WTFGType.h>

using namespace WebCore;

struct WebKitMediaThunderDecryptPrivate {
    RefPtr<CDMProxyThunder> cdmProxy;
    GRefPtr<GstCaps> inputCaps;
};

static ASCIILiteral protectionSystemId(WebKitMediaCommonEncryptionDecrypt*);
static bool cdmProxyAttached(WebKitMediaCommonEncryptionDecrypt*, const RefPtr<CDMProxy>&);
static bool decrypt(WebKitMediaCommonEncryptionDecrypt*, GstBuffer* iv, GstBuffer* keyid, GstBuffer* sample, unsigned subSamplesCount,
    GstBuffer* subSamples);

GST_DEBUG_CATEGORY(webkitMediaThunderDecryptDebugCategory);
#define GST_CAT_DEFAULT webkitMediaThunderDecryptDebugCategory

static GstStaticPadTemplate thunderSrcTemplate = GST_STATIC_PAD_TEMPLATE("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS(
        "video/webm; "
        "audio/webm; "
        "video/mp4; "
        "audio/mp4; "
        "audio/mpeg; "
        "audio/x-flac; "
        "audio/x-eac3; "
        "audio/x-ac3; "
        "video/x-h264; "
        "video/x-h265; "
        "video/x-vp9; video/x-vp8; "
        "video/x-av1; "
        "audio/x-opus; audio/x-vorbis"));

WEBKIT_DEFINE_TYPE(WebKitMediaThunderDecrypt, webkit_media_thunder_decrypt, WEBKIT_TYPE_MEDIA_CENC_DECRYPT)

static GRefPtr<GstCaps> createSinkPadTemplateCaps()
{
    GRefPtr<GstCaps> caps = adoptGRef(gst_caps_new_empty());

    auto& supportedKeySystems = CDMFactoryThunder::singleton().supportedKeySystems();

    if (supportedKeySystems.isEmpty()) {
        GST_WARNING("no supported key systems in Thunder, we won't be able to decrypt anything with the decryptor");
        return caps;
    }

    for (const auto& mediaType : GStreamerEMEUtilities::s_cencEncryptionMediaTypes) {
        gst_caps_append_structure(caps.get(), gst_structure_new("application/x-cenc", "original-media-type", G_TYPE_STRING,
            mediaType.characters(), nullptr));
    }
    for (const auto& keySystem : supportedKeySystems) {
        for (const auto& mediaType : GStreamerEMEUtilities::s_cencEncryptionMediaTypes) {
            gst_caps_append_structure(caps.get(), gst_structure_new("application/x-cenc", "original-media-type", G_TYPE_STRING,
                mediaType.characters(), "protection-system", G_TYPE_STRING, GStreamerEMEUtilities::keySystemToUuid(keySystem).characters(), nullptr));
        }
    }

    if (supportedKeySystems.contains(GStreamerEMEUtilities::s_WidevineKeySystem) || supportedKeySystems.contains(GStreamerEMEUtilities::s_ClearKeyKeySystem)) {
        for (const auto& mediaType : GStreamerEMEUtilities::s_webmEncryptionMediaTypes) {
            gst_caps_append_structure(caps.get(), gst_structure_new("application/x-webm-enc", "original-media-type", G_TYPE_STRING,
                mediaType.characters(), nullptr));
        }
    }

    GST_DEBUG("sink pad template caps %" GST_PTR_FORMAT, caps.get());

    return caps;
}

static void webkitMediaThunderDecryptorConstructed(GObject* object)
{
    G_OBJECT_CLASS(webkit_media_thunder_decrypt_parent_class)->constructed(object);

    auto self = WEBKIT_MEDIA_THUNDER_DECRYPT(object);
    auto baseTransform = GST_BASE_TRANSFORM_CAST(self);
    self->priv->inputCaps = adoptGRef(gst_pad_get_current_caps(GST_BASE_TRANSFORM_SINK_PAD(baseTransform)));

    g_signal_connect_swapped(GST_BASE_TRANSFORM_SINK_PAD(baseTransform), "notify::caps", G_CALLBACK(+[](WebKitMediaThunderDecrypt* self) {
        self->priv->inputCaps = adoptGRef(gst_pad_get_current_caps(GST_BASE_TRANSFORM_SINK_PAD(GST_BASE_TRANSFORM_CAST(self))));
    }), self);
}

static void webkit_media_thunder_decrypt_class_init(WebKitMediaThunderDecryptClass* klass)
{
    GST_DEBUG_CATEGORY_INIT(webkitMediaThunderDecryptDebugCategory, "webkitthunderdecrypt", 0, "Thunder decrypt");

    auto objectClass = G_OBJECT_CLASS(klass);
    objectClass->constructed = webkitMediaThunderDecryptorConstructed;

    GstElementClass* elementClass = GST_ELEMENT_CLASS(klass);
    GRefPtr<GstCaps> gstSinkPadTemplateCaps = createSinkPadTemplateCaps();
    gst_element_class_add_pad_template(elementClass, gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS, gstSinkPadTemplateCaps.get()));
    gst_element_class_add_pad_template(elementClass, gst_static_pad_template_get(&thunderSrcTemplate));

    gst_element_class_set_static_metadata(elementClass, "Decrypt encrypted content using Thunder", GST_ELEMENT_FACTORY_KLASS_DECRYPTOR,
        "Decrypts encrypted media using Thunder.", "Xabier Rodríguez Calvar <calvaris@igalia.com>");

    WebKitMediaCommonEncryptionDecryptClass* commonClass = WEBKIT_MEDIA_CENC_DECRYPT_CLASS(klass);
    commonClass->protectionSystemId = GST_DEBUG_FUNCPTR(protectionSystemId);
    commonClass->cdmProxyAttached = GST_DEBUG_FUNCPTR(cdmProxyAttached);
    commonClass->decrypt = GST_DEBUG_FUNCPTR(decrypt);
}

static ASCIILiteral protectionSystemId(WebKitMediaCommonEncryptionDecrypt* decryptor)
{
    auto* self = WEBKIT_MEDIA_THUNDER_DECRYPT(decryptor);
    ASSERT(self->priv->cdmProxy);
    return GStreamerEMEUtilities::keySystemToUuid(self->priv->cdmProxy->keySystem());
}

static bool cdmProxyAttached(WebKitMediaCommonEncryptionDecrypt* decryptor, const RefPtr<CDMProxy>& cdmProxy)
{
    auto* self = WEBKIT_MEDIA_THUNDER_DECRYPT(decryptor);
    self->priv->cdmProxy = reinterpret_cast<CDMProxyThunder*>(cdmProxy.get());
    return self->priv->cdmProxy;
}

static bool decrypt(WebKitMediaCommonEncryptionDecrypt* decryptor, GstBuffer* ivBuffer, GstBuffer* keyIDBuffer, GstBuffer* buffer, unsigned subsampleCount,
    GstBuffer* subsamplesBuffer)
{
    auto* self = WEBKIT_MEDIA_THUNDER_DECRYPT(decryptor);
    auto* priv = self->priv;

    if (!ivBuffer || !keyIDBuffer || !buffer) {
        GST_ERROR_OBJECT(self, "invalid decrypt() parameter");
        ASSERT_NOT_REACHED();
        return false;
    }

    if (subsampleCount && !subsamplesBuffer) {
        GST_ERROR_OBJECT(self, "invalid decrypt() subsamples parameter");
        ASSERT_NOT_REACHED();
        return false;
    }

    CDMProxyThunder::DecryptionContext context = { };
    context.keyIDBuffer = keyIDBuffer;
    context.ivBuffer = ivBuffer;
    context.dataBuffer = buffer;
    context.numSubsamples = subsampleCount;
    context.subsamplesBuffer = subsampleCount ? subsamplesBuffer : nullptr;
    context.cdmProxyDecryptionClient = webKitMediaCommonEncryptionDecryptGetCDMProxyDecryptionClient(decryptor);
    bool result = priv->cdmProxy->decrypt(context, priv->inputCaps);

    return result;
}

#undef GST_CAT_DEFAULT

#endif // ENABLE(ENCRYPTED_MEDIA) && ENABLE(THUNDER) && USE(GSTREAMER)
