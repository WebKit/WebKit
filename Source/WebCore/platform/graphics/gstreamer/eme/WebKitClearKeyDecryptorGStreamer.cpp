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
#include "GStreamerCommon.h"
#include "GStreamerEMEUtilities.h"
#include <gcrypt.h>
#include <gst/base/gstbytereader.h>
#include <wtf/RunLoop.h>

using namespace WebCore;

#define CLEARKEY_SIZE 16

#define WEBKIT_MEDIA_CK_DECRYPT_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), WEBKIT_TYPE_MEDIA_CK_DECRYPT, WebKitMediaClearKeyDecryptPrivate))
struct _WebKitMediaClearKeyDecryptPrivate {
    RefPtr<CDMInstanceClearKey> cdmInstance;
    gcry_cipher_hd_t handle;
};

static void finalize(GObject*);
static bool handleKeyResponse(WebKitMediaCommonEncryptionDecrypt* self, RefPtr<CDMInstance>);
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
G_DEFINE_TYPE(WebKitMediaClearKeyDecrypt, webkit_media_clear_key_decrypt, WEBKIT_TYPE_MEDIA_CENC_DECRYPT);

static void webkit_media_clear_key_decrypt_class_init(WebKitMediaClearKeyDecryptClass* klass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(klass);
    gobjectClass->finalize = finalize;

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
    cencClass->protectionSystemId = GStreamerEMEUtilities::s_ClearKeyUUID;
    cencClass->handleKeyResponse = GST_DEBUG_FUNCPTR(handleKeyResponse);
    cencClass->decrypt = GST_DEBUG_FUNCPTR(decrypt);

    g_type_class_add_private(klass, sizeof(WebKitMediaClearKeyDecryptPrivate));
}

static void webkit_media_clear_key_decrypt_init(WebKitMediaClearKeyDecrypt* self)
{
    WebKitMediaClearKeyDecryptPrivate* priv = WEBKIT_MEDIA_CK_DECRYPT_GET_PRIVATE(self);

    self->priv = priv;
    new (priv) WebKitMediaClearKeyDecryptPrivate();
    if (gcry_error_t error = gcry_cipher_open(&(priv->handle), GCRY_CIPHER_AES128, GCRY_CIPHER_MODE_CTR, GCRY_CIPHER_SECURE)) {
        GST_ERROR_OBJECT(self, "Failed to create AES 128 CTR cipher handle: %s", gpg_strerror(error));
        ASSERT(!error);
    }
}

static void finalize(GObject* object)
{
    WebKitMediaClearKeyDecrypt* self = WEBKIT_MEDIA_CK_DECRYPT(object);
    WebKitMediaClearKeyDecryptPrivate* priv = self->priv;
    gcry_cipher_close(priv->handle);
    priv->~WebKitMediaClearKeyDecryptPrivate();

    GST_CALL_PARENT(G_OBJECT_CLASS, finalize, (object));
}

static bool handleKeyResponse(WebKitMediaCommonEncryptionDecrypt* self, RefPtr<CDMInstance> cdmInstance)
{
    WebKitMediaClearKeyDecryptPrivate* priv = WEBKIT_MEDIA_CK_DECRYPT_GET_PRIVATE(WEBKIT_MEDIA_CK_DECRYPT(self));
    priv->cdmInstance = downcast<CDMInstanceClearKey>(cdmInstance.get());
    return priv->cdmInstance;
}

static bool findAndSetKey(WebKitMediaClearKeyDecryptPrivate* priv, const Ref<SharedBuffer>& keyID)
{
    RefPtr<SharedBuffer> keyValue;
    for (const auto& key : priv->cdmInstance->keys()) {
        if (*key.keyIDData == keyID) {
            keyValue = key.keyValueData;
            break;
        }
    }

    if (!keyValue) {
        GST_ERROR_OBJECT(priv, "failed to find a decryption key");
        return false;
    }

    ASSERT(keyValue->size() == CLEARKEY_SIZE);
    if (gcry_error_t error = gcry_cipher_setkey(priv->handle, keyValue->data(), keyValue->size())) {
        GST_ERROR_OBJECT(priv, "gcry_cipher_setkey failed: %s", gpg_strerror(error));
        return false;
    }

    return true;
}

static bool decrypt(WebKitMediaCommonEncryptionDecrypt* self, GstBuffer* ivBuffer, GstBuffer* keyIDBuffer, GstBuffer* buffer, unsigned subSampleCount, GstBuffer* subSamplesBuffer)
{
    if (!ivBuffer) {
        GST_ERROR_OBJECT(self, "no IV buffer");
        return false;
    }

    auto mappedIVBuffer = WebCore::GstMappedBuffer::create(ivBuffer, GST_MAP_READ);
    if (!mappedIVBuffer) {
        GST_ERROR_OBJECT(self, "failed to map IV buffer");
        return false;
    }

    uint8_t ctr[CLEARKEY_SIZE];
    if (mappedIVBuffer->size() == 8) {
        memset(ctr + 8, 0, 8);
        memcpy(ctr, mappedIVBuffer->data(), 8);
    } else {
        ASSERT(mappedIVBuffer->size() == CLEARKEY_SIZE);
        memcpy(ctr, mappedIVBuffer->data(), CLEARKEY_SIZE);
    }

    WebKitMediaClearKeyDecryptPrivate* priv = WEBKIT_MEDIA_CK_DECRYPT_GET_PRIVATE(WEBKIT_MEDIA_CK_DECRYPT(self));
    gcry_error_t cipherError = gcry_cipher_setctr(priv->handle, ctr, CLEARKEY_SIZE);
    if (cipherError) {
        GST_ERROR_OBJECT(self, "gcry_cipher_setctr failed: %s", gpg_strerror(cipherError));
        return false;
    }

    if (!buffer) {
        GST_ERROR_OBJECT(self, "No buffer to decrypt");
        return false;
    }

    auto mappedKeyIdBuffer = WebCore::GstMappedBuffer::create(keyIDBuffer, GST_MAP_READ);
    if (!mappedKeyIdBuffer) {
        GST_ERROR_OBJECT(self, "Failed to map key id buffer");
        return false;
    }

    auto mappedBuffer = WebCore::GstMappedBuffer::create(buffer, GST_MAP_READWRITE);
    if (!mappedBuffer) {
        GST_ERROR_OBJECT(self, "Failed to map buffer");
        return false;
    }

    findAndSetKey(priv, mappedKeyIdBuffer->createSharedBuffer());

    unsigned position = 0;
    unsigned sampleIndex = 0;

    if (!subSampleCount) {
        // Full sample encryption.
        GST_TRACE_OBJECT(self, "full sample encryption: %zu encrypted bytes", mappedBuffer->size());

        // Check if the buffer is empty.
        if (mappedBuffer->size()) {
            cipherError = gcry_cipher_decrypt(priv->handle, mappedBuffer->data(), mappedBuffer->size(), 0, 0);
            if (cipherError) {
                GST_ERROR_OBJECT(self, "full sample decryption failed: %s", gpg_strerror(cipherError));
                return false;
            }
        }
        return true;
    }

    // Check subSamplesBuffer isn't null.
    if (!subSamplesBuffer) {
        GST_ERROR_OBJECT(self, "Error, the subSampleBuffer is null");
        return false;
    }

    // Subsample encryption.
    auto mappedSubSamplesBuffer = WebCore::GstMappedBuffer::create(subSamplesBuffer, GST_MAP_READ);
    if (!mappedSubSamplesBuffer) {
        GST_ERROR_OBJECT(self, "Failed to map subsample buffer");
        return false;
    }

    GUniquePtr<GstByteReader> reader(gst_byte_reader_new(mappedSubSamplesBuffer->data(), mappedSubSamplesBuffer->size()));
    GST_DEBUG_OBJECT(self, "position: %d, size: %zu", position, mappedBuffer->size());

    while (position < mappedBuffer->size()) {
        guint16 nBytesClear = 0;
        guint32 nBytesEncrypted = 0;

        if (sampleIndex < subSampleCount) {
            if (!gst_byte_reader_get_uint16_be(reader.get(), &nBytesClear)
                || !gst_byte_reader_get_uint32_be(reader.get(), &nBytesEncrypted)) {
                GST_DEBUG_OBJECT(self, "unsupported");
                return false;
            }
            sampleIndex++;
        } else {
            nBytesClear = 0;
            nBytesEncrypted = mappedBuffer->size() - position;
        }

        GST_TRACE_OBJECT(self, "subsample index %u - %hu bytes clear (todo=%zu)", sampleIndex, nBytesClear, mappedBuffer->size() - position);
        position += nBytesClear;
        if (nBytesEncrypted) {
            GST_TRACE_OBJECT(self, "subsample index %u - %u bytes encrypted (todo=%zu)", sampleIndex, nBytesEncrypted, mappedBuffer->size() - position);
            cipherError = gcry_cipher_decrypt(priv->handle, mappedBuffer->data() + position, nBytesEncrypted, 0, 0);
            if (cipherError) {
                GST_ERROR_OBJECT(self, "sub sample index %u decryption failed: %s", sampleIndex, gpg_strerror(cipherError));
                return false;
            }
            position += nBytesEncrypted;
        }
    }

    return true;
}

#endif // ENABLE(ENCRYPTED_MEDIA) && USE(GSTREAMER)
