/* GStreamer ClearKey common encryption decryptor
 *
 * Copyright (C) 2013 YouView TV Ltd. <alex.ashley@youview.com>
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#if ENABLE(ENCRYPTED_MEDIA) && USE(GSTREAMER)

#include <CDMInstance.h>
#include <gst/base/gstbasetransform.h>
#include <gst/gst.h>
#include <wtf/RefPtr.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_MEDIA_CENC_DECRYPT          (webkit_media_common_encryption_decrypt_get_type())
#define WEBKIT_MEDIA_CENC_DECRYPT(obj)          (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_MEDIA_CENC_DECRYPT, WebKitMediaCommonEncryptionDecrypt))
#define WEBKIT_MEDIA_CENC_DECRYPT_CLASS(klass)  (G_TYPE_CHECK_CLASS_CAST((klass), WEBKIT_TYPE_MEDIA_CENC_DECRYPT, WebKitMediaCommonEncryptionDecryptClass))
#define WEBKIT_MEDIA_CENC_DECRYPT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), WEBKIT_TYPE_MEDIA_CENC_DECRYPT, WebKitMediaCommonEncryptionDecryptClass))

#define WEBKIT_IS_MEDIA_CENC_DECRYPT(obj)       (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_MEDIA_CENC_DECRYPT))
#define WEBKIT_IS_MEDIA_CENC_DECRYPT_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((klass), WEBKIT_TYPE_MEDIA_CENC_DECRYPT))

typedef struct _WebKitMediaCommonEncryptionDecrypt        WebKitMediaCommonEncryptionDecrypt;
typedef struct _WebKitMediaCommonEncryptionDecryptClass   WebKitMediaCommonEncryptionDecryptClass;
typedef struct _WebKitMediaCommonEncryptionDecryptPrivate WebKitMediaCommonEncryptionDecryptPrivate;

GType webkit_media_common_encryption_decrypt_get_type(void);

struct _WebKitMediaCommonEncryptionDecrypt {
    GstBaseTransform parent;

    WebKitMediaCommonEncryptionDecryptPrivate* priv;
};

struct _WebKitMediaCommonEncryptionDecryptClass {
    GstBaseTransformClass parentClass;

    const char* protectionSystemId;
    bool (*handleKeyResponse)(WebKitMediaCommonEncryptionDecrypt*, RefPtr<WebCore::ProxyCDM>);
    bool (*decrypt)(WebKitMediaCommonEncryptionDecrypt*, GstBuffer* ivBuffer, GstBuffer* keyIDBuffer, GstBuffer* buffer, unsigned subSamplesCount, GstBuffer* subSamplesBuffer);
};

G_END_DECLS

#endif // ENABLE(ENCRYPTED_MEDIA) && USE(GSTREAMER)
