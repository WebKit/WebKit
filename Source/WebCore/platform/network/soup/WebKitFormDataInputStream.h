/*
 * Copyright (C) 2021 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "FormData.h"
#include <gio/gio.h>
#include <wtf/Forward.h>
#include <wtf/glib/GRefPtr.h>

#define WEBKIT_TYPE_FORM_DATA_INPUT_STREAM            (webkit_form_data_input_stream_get_type ())
#define WEBKIT_FORM_DATA_INPUT_STREAM(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), WEBKIT_TYPE_FORM_DATA_INPUT_STREAM, WebKitFormDataInputStream))
#define WEBKIT_IS_FORM_DATA_INPUT_STREAM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), WEBKIT_TYPE_FORM_DATA_INPUT_STREAM))
#define WEBKIT_FORM_DATA_INPUT_STREAM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), WEBKIT_TYPE_FORM_DATA_INPUT_STREAM, WebKitFormDataInputStreamClass))
#define WEBKIT_IS_FORM_DATA_INPUT_STREAM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), WEBKIT_TYPE_FORM_DATA_INPUT_STREAM))
#define WEBKIT_FORM_DATA_INPUT_STREAM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), WEBKIT_TYPE_FORM_DATA_INPUT_STREAM, WebKitFormDataInputStreamClass))

typedef struct _WebKitFormDataInputStream WebKitFormDataInputStream;
typedef struct _WebKitFormDataInputStreamClass WebKitFormDataInputStreamClass;
typedef struct _WebKitFormDataInputStreamPrivate WebKitFormDataInputStreamPrivate;

struct _WebKitFormDataInputStream {
    GInputStream parent;

    WebKitFormDataInputStreamPrivate* priv;
};

struct _WebKitFormDataInputStreamClass {
    GInputStreamClass parentClass;
};

GType webkit_form_data_input_stream_get_type(void);

GRefPtr<GInputStream> webkitFormDataInputStreamNew(Ref<WebCore::FormData>&&);
GBytes* webkitFormDataInputStreamReadAll(WebKitFormDataInputStream*);
