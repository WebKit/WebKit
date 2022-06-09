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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#if ENABLE(ENCRYPTED_MEDIA) && USE(GSTREAMER)

#include "WebKitCommonEncryptionDecryptorGStreamer.h"

G_BEGIN_DECLS

#define WEBKIT_TYPE_MEDIA_CK_DECRYPT          (webkit_media_clear_key_decrypt_get_type())
#define WEBKIT_MEDIA_CK_DECRYPT(obj)          (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_MEDIA_CK_DECRYPT, WebKitMediaClearKeyDecrypt))
#define WEBKIT_MEDIA_CK_DECRYPT_CLASS(klass)  (G_TYPE_CHECK_CLASS_CAST((klass), WEBKIT_TYPE_MEDIA_CK_DECRYPT, WebKitMediaClearKeyDecryptClass))
#define WEBKIT_IS_MEDIA_CK_DECRYPT(obj)       (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_MEDIA_CK_DECRYPT))
#define WEBKIT_IS_MEDIA_CK_DECRYPT_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((klass), WEBKIT_TYPE_MEDIA_CK_DECRYPT))

typedef struct _WebKitMediaClearKeyDecrypt        WebKitMediaClearKeyDecrypt;
typedef struct _WebKitMediaClearKeyDecryptClass   WebKitMediaClearKeyDecryptClass;
typedef struct _WebKitMediaClearKeyDecryptPrivate WebKitMediaClearKeyDecryptPrivate;

GType webkit_media_clear_key_decrypt_get_type(void);

struct _WebKitMediaClearKeyDecrypt {
    WebKitMediaCommonEncryptionDecrypt parent;

    WebKitMediaClearKeyDecryptPrivate* priv;
};

struct _WebKitMediaClearKeyDecryptClass {
    WebKitMediaCommonEncryptionDecryptClass parentClass;
};

G_END_DECLS

#endif // ENABLE(ENCRYPTED_MEDIA) && USE(GSTREAMER)
