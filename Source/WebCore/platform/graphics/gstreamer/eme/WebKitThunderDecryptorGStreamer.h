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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#if ENABLE(ENCRYPTED_MEDIA) && ENABLE(THUNDER) && USE(GSTREAMER)

#include "WebKitCommonEncryptionDecryptorGStreamer.h"

G_BEGIN_DECLS

#define WEBKIT_TYPE_MEDIA_THUNDER_DECRYPT          (webkit_media_thunder_decrypt_get_type())
#define WEBKIT_MEDIA_THUNDER_DECRYPT(obj)          (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_MEDIA_THUNDER_DECRYPT, WebKitMediaThunderDecrypt))
#define WEBKIT_MEDIA_THUNDER_DECRYPT_CLASS(klass)  (G_TYPE_CHECK_CLASS_CAST((klass), WEBKIT_TYPE_MEDIA_THUNDER_DECRYPT, WebKitMediaThunderDecryptClass))
#define WEBKIT_IS_MEDIA_THUNDER_DECRYPT(obj)       (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_MEDIA_THUNDER_DECRYPT))
#define WEBKIT_IS_MEDIA_THUNDER_DECRYPT_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((klass), WEBKIT_TYPE_MEDIA_THUNDER_DECRYPT))

typedef struct _WebKitMediaThunderDecrypt        WebKitMediaThunderDecrypt;
typedef struct _WebKitMediaThunderDecryptClass   WebKitMediaThunderDecryptClass;
struct WebKitMediaThunderDecryptPrivate;

GType webkit_media_thunder_decrypt_get_type(void);

struct _WebKitMediaThunderDecrypt {
    WebKitMediaCommonEncryptionDecrypt parent;

    WebKitMediaThunderDecryptPrivate* priv;
};

struct _WebKitMediaThunderDecryptClass {
    WebKitMediaCommonEncryptionDecryptClass parentClass;
};

G_END_DECLS

#endif // ENABLE(ENCRYPTED_MEDIA) && ENABLE(THUNDER) && USE(GSTREAMER)
