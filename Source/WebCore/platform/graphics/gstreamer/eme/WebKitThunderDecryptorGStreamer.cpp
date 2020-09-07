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
#include <gcrypt.h>
#include <gst/base/gstbytereader.h>
#include <wtf/RunLoop.h>
#include <wtf/glib/WTFGType.h>

using namespace WebCore;

struct _WebKitMediaThunderDecryptPrivate {
    RefPtr<CDMProxyThunder> cdmProxy;
};

static const char* protectionSystemId(WebKitMediaCommonEncryptionDecrypt*);
static bool cdmProxyAttached(WebKitMediaCommonEncryptionDecrypt*, const RefPtr<CDMProxy>&);
static bool decrypt(WebKitMediaCommonEncryptionDecrypt*, GstBuffer* iv, GstBuffer* keyid, GstBuffer* sample, unsigned subSamplesCount,
    GstBuffer* subSamples);

GST_DEBUG_CATEGORY(webkitMediaThunderDecryptDebugCategory);
#define GST_CAT_DEFAULT webkitMediaThunderDecryptDebugCategory

static const char* cencEncryptionMediaTypes[] = { "video/mp4", "audio/mp4", "video/x-h264", "audio/mpeg", nullptr };
static const char* webmEncryptionMediaTypes[] = { "video/webm", "audio/webm", "video/x-vp9", nullptr };

static GstStaticPadTemplate srcTemplate = GST_STATIC_PAD_TEMPLATE("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS(
        "video/webm; "
        "audio/webm; "
        "video/mp4; "
        "audio/mp4; "
        "audio/mpeg; "
        "video/x-h264; "
        "video/x-vp9; "));

#define webkit_media_thunder_decrypt_parent_class parent_class
WEBKIT_DEFINE_TYPE(WebKitMediaThunderDecrypt, webkit_media_thunder_decrypt, WEBKIT_TYPE_MEDIA_CENC_DECRYPT)

static GRefPtr<GstCaps> createSinkPadTemplateCaps()
{
    GRefPtr<GstCaps> caps = adoptGRef(gst_caps_new_empty());

    for (const auto& keySystem : CDMFactoryThunder::singleton().supportedKeySystems()) {
        for (int i = 0; cencEncryptionMediaTypes[i]; ++i) {
            gst_caps_append_structure(caps.get(), gst_structure_new("application/x-cenc", "original-media-type", G_TYPE_STRING,
                cencEncryptionMediaTypes[i], "protection-system", G_TYPE_STRING, GStreamerEMEUtilities::keySystemToUuid(keySystem), nullptr));
        }
        if (GStreamerEMEUtilities::isWidevineKeySystem(keySystem)) {
            // No key system UUID for webm. It's not set in caps for it and it's just Widevine.
            for (int i = 0; webmEncryptionMediaTypes[i]; ++i) {
                gst_caps_append_structure(caps.get(), gst_structure_new("application/x-webm-enc", "original-media-type", G_TYPE_STRING,
                    webmEncryptionMediaTypes[i], nullptr));
            }
        }
    }

    GST_DEBUG("sink pad template caps %" GST_PTR_FORMAT, caps.get());

    return caps;
}

static void webkit_media_thunder_decrypt_class_init(WebKitMediaThunderDecryptClass* klass)
{
    GST_DEBUG_CATEGORY_INIT(webkitMediaThunderDecryptDebugCategory, "webkitthunder", 0, "Thunder decrypt");

    GstElementClass* elementClass = GST_ELEMENT_CLASS(klass);
    GRefPtr<GstCaps> gstSinkPadTemplateCaps = createSinkPadTemplateCaps();
    gst_element_class_add_pad_template(elementClass, gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS, gstSinkPadTemplateCaps.get()));
    gst_element_class_add_pad_template(elementClass, gst_static_pad_template_get(&srcTemplate));

    gst_element_class_set_static_metadata(elementClass, "Decrypt encrypted content using Thunder", GST_ELEMENT_FACTORY_KLASS_DECRYPTOR,
        "Decrypts encrypted media using Thunder.", "Xabier Rodríguez Calvar <calvaris@igalia.com>");

    WebKitMediaCommonEncryptionDecryptClass* commonClass = WEBKIT_MEDIA_CENC_DECRYPT_CLASS(klass);
    commonClass->protectionSystemId = GST_DEBUG_FUNCPTR(protectionSystemId);
    commonClass->cdmProxyAttached = GST_DEBUG_FUNCPTR(cdmProxyAttached);
    commonClass->decrypt = GST_DEBUG_FUNCPTR(decrypt);

    g_type_class_add_private(klass, sizeof(WebKitMediaThunderDecryptPrivate));
}

static const char* protectionSystemId(WebKitMediaCommonEncryptionDecrypt* self)
{
    WebKitMediaThunderDecryptPrivate* priv = WEBKIT_MEDIA_THUNDER_DECRYPT_GET_PRIVATE(WEBKIT_MEDIA_THUNDER_DECRYPT(self));
    ASSERT(priv->cdmProxy);
    return GStreamerEMEUtilities::keySystemToUuid(priv->cdmProxy->keySystem());
}

static bool cdmProxyAttached(WebKitMediaCommonEncryptionDecrypt* self, const RefPtr<CDMProxy>& cdmProxy)
{
    WebKitMediaThunderDecryptPrivate* priv = WEBKIT_MEDIA_THUNDER_DECRYPT_GET_PRIVATE(WEBKIT_MEDIA_THUNDER_DECRYPT(self));
    priv->cdmProxy = reinterpret_cast<CDMProxyThunder*>(cdmProxy.get());
    return priv->cdmProxy;
}

static bool decrypt(WebKitMediaCommonEncryptionDecrypt* self, GstBuffer* ivBuffer, GstBuffer* keyIDBuffer, GstBuffer* buffer, unsigned subsampleCount,
    GstBuffer* subsamplesBuffer)
{
    WebKitMediaThunderDecryptPrivate* priv = WEBKIT_MEDIA_THUNDER_DECRYPT_GET_PRIVATE(WEBKIT_MEDIA_THUNDER_DECRYPT(self));

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

    CDMProxyThunder::DecryptionContext context;
    context.keyIDBuffer = keyIDBuffer;
    context.ivBuffer = ivBuffer;
    context.dataBuffer = buffer;
    context.numSubsamples = subsampleCount;
    context.subsamplesBuffer = subsampleCount ? subsamplesBuffer : nullptr;

    return priv->cdmProxy->decrypt(context);
}

#endif // ENABLE(ENCRYPTED_MEDIA) && ENABLE(THUNDER) && USE(GSTREAMER)
