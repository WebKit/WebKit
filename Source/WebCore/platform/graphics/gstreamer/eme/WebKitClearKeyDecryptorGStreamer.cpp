/* GStreamer ClearKey common encryption decryptor
 *
 * Copyright (C) 2016 Metrological
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
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */

#include "config.h"
#include "WebKitClearKeyDecryptorGStreamer.h"

#if ENABLE(ENCRYPTED_MEDIA) && USE(GSTREAMER)

#include "CDMClearKey.h"
#include "CDMProxyClearKey.h"
#include "GStreamerCommon.h"
#include "GStreamerEMEUtilities.h"
#include <gcrypt.h>
#include <gst/base/gstbytereader.h>
#include <wtf/RunLoop.h>
#include <wtf/glib/WTFGType.h>

using namespace WebCore;

#define CLEARKEY_SIZE 16

#define WEBKIT_MEDIA_CK_DECRYPT_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), WEBKIT_TYPE_MEDIA_CK_DECRYPT, WebKitMediaClearKeyDecryptPrivate))
struct _WebKitMediaClearKeyDecryptPrivate {
    RefPtr<CDMProxyClearKey> cdmProxy;
};

static const char* protectionSystemId(WebKitMediaCommonEncryptionDecrypt*);
static bool cdmProxyAttached(WebKitMediaCommonEncryptionDecrypt* self, const RefPtr<CDMProxy>&);
static bool decrypt(WebKitMediaCommonEncryptionDecrypt*, GstBuffer* iv, GstBuffer* keyid, GstBuffer* sample, unsigned subSamplesCount, GstBuffer* subSamples);

GST_DEBUG_CATEGORY_STATIC(webkit_media_clear_key_decrypt_debug_category);
#define GST_CAT_DEFAULT webkit_media_clear_key_decrypt_debug_category

static GstStaticPadTemplate sinkTemplate = GST_STATIC_PAD_TEMPLATE("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS("application/x-cenc, original-media-type=(string)video/x-h264, protection-system=(string)" WEBCORE_GSTREAMER_EME_UTILITIES_CLEARKEY_UUID "; "
    "application/x-cenc, original-media-type=(string)audio/mpeg, protection-system=(string)" WEBCORE_GSTREAMER_EME_UTILITIES_CLEARKEY_UUID";"
    "application/x-webm-enc, original-media-type=(string)video/x-vp8;"
    "application/x-webm-enc, original-media-type=(string)video/x-vp9;"));

static GstStaticPadTemplate srcTemplate = GST_STATIC_PAD_TEMPLATE("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS("video/x-h264; audio/mpeg; video/x-vp8; video/x-vp9"));

#define webkit_media_clear_key_decrypt_parent_class parent_class
WEBKIT_DEFINE_TYPE(WebKitMediaClearKeyDecrypt, webkit_media_clear_key_decrypt, WEBKIT_TYPE_MEDIA_CENC_DECRYPT)

static void webkit_media_clear_key_decrypt_class_init(WebKitMediaClearKeyDecryptClass* klass)
{
    GstElementClass* elementClass = GST_ELEMENT_CLASS(klass);
    gst_element_class_add_pad_template(elementClass, gst_static_pad_template_get(&sinkTemplate));
    gst_element_class_add_pad_template(elementClass, gst_static_pad_template_get(&srcTemplate));

    gst_element_class_set_static_metadata(elementClass,
        "Decrypt content encrypted using ISOBMFF ClearKey Common Encryption",
        GST_ELEMENT_FACTORY_KLASS_DECRYPTOR,
        "Decrypts media that has been encrypted using ISOBMFF ClearKey Common Encryption.",
        "Philippe Normand <philn@igalia.com>");

    GST_DEBUG_CATEGORY_INIT(webkit_media_clear_key_decrypt_debug_category,
        "webkitclearkey", 0, "ClearKey decryptor");

    WebKitMediaCommonEncryptionDecryptClass* cencClass = WEBKIT_MEDIA_CENC_DECRYPT_CLASS(klass);
    cencClass->protectionSystemId = GST_DEBUG_FUNCPTR(protectionSystemId);
    cencClass->cdmProxyAttached = GST_DEBUG_FUNCPTR(cdmProxyAttached);
    cencClass->decrypt = GST_DEBUG_FUNCPTR(decrypt);
}

static const char* protectionSystemId(WebKitMediaCommonEncryptionDecrypt*)
{
    return GStreamerEMEUtilities::s_ClearKeyUUID;
}

static bool cdmProxyAttached(WebKitMediaCommonEncryptionDecrypt* self, const RefPtr<CDMProxy>& cdmProxy)
{
    WebKitMediaClearKeyDecryptPrivate* priv = WEBKIT_MEDIA_CK_DECRYPT_GET_PRIVATE(WEBKIT_MEDIA_CK_DECRYPT(self));
    priv->cdmProxy = reinterpret_cast<CDMProxyClearKey*>(cdmProxy.get());
    return priv->cdmProxy;
}

static bool decrypt(WebKitMediaCommonEncryptionDecrypt* self, GstBuffer* ivBuffer, GstBuffer* keyIDBuffer, GstBuffer* buffer, unsigned subsampleCount, GstBuffer* subsamplesBuffer)
{
    WebKitMediaClearKeyDecryptPrivate* priv = WEBKIT_MEDIA_CK_DECRYPT_GET_PRIVATE(WEBKIT_MEDIA_CK_DECRYPT(self));

    if (!ivBuffer || !keyIDBuffer || !buffer) {
        GST_ERROR_OBJECT(self, "invalid decrypt() parameter");
        return false;
    }

    WebCore::GstMappedBuffer mappedIVBuffer(ivBuffer, GST_MAP_READ);
    if (!mappedIVBuffer) {
        GST_ERROR_OBJECT(self, "failed to map IV buffer");
        return false;
    }

    WebCore::GstMappedBuffer mappedKeyIdBuffer(keyIDBuffer, GST_MAP_READ);
    if (!mappedKeyIdBuffer) {
        GST_ERROR_OBJECT(self, "Failed to map key id buffer");
        return false;
    }

    WebCore::GstMappedBuffer mappedBuffer(buffer, GST_MAP_READWRITE);
    if (!mappedBuffer) {
        GST_ERROR_OBJECT(self, "Failed to map buffer");
        return false;
    }

    CDMProxyClearKey::cencDecryptContext context = { };
    context.keyID = mappedKeyIdBuffer.data();
    context.keyIDSizeInBytes = mappedKeyIdBuffer.size();
    context.iv = mappedIVBuffer.data();
    context.ivSizeInBytes = mappedIVBuffer.size();
    context.encryptedBuffer = mappedBuffer.data();
    context.encryptedBufferSizeInBytes = mappedBuffer.size();
    context.numSubsamples = subsampleCount;
    if (!subsampleCount)
        context.subsamplesBuffer = nullptr;
    else {
        ASSERT(subsamplesBuffer);
        WebCore::GstMappedBuffer mappedSubsamplesBuffer(subsamplesBuffer, GST_MAP_READ);
        if (!mappedSubsamplesBuffer) {
            GST_ERROR_OBJECT(self, "Failed to map subsample buffer");
            return false;
        }
        context.subsamplesBuffer = mappedSubsamplesBuffer.data();
        context.subsamplesBufferSizeInBytes = mappedSubsamplesBuffer.size();
    }
    context.cdmProxyDecryptionClient = webKitMediaCommonEncryptionDecryptGetCDMProxyDecryptionClient(self);

    return priv->cdmProxy->cencDecrypt(context);
}

#endif // ENABLE(ENCRYPTED_MEDIA) && USE(GSTREAMER)
