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

#include <gio/gio.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/text/CString.h>

#define WEBKIT_TYPE_DIRECTORY_INPUT_STREAM            (webkit_directory_input_stream_get_type ())
#define WEBKIT_DIRECTORY_INPUT_STREAM(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), WEBKIT_TYPE_DIRECTORY_INPUT_STREAM, WebKitDirectoryInputStream))
#define WEBKIT_IS_DIRECTORY_INPUT_STREAM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), WEBKIT_TYPE_DIRECTORY_INPUT_STREAM))
#define WEBKIT_DIRECTORY_INPUT_STREAM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), WEBKIT_TYPE_DIRECTORY_INPUT_STREAM, WebKitDirectoryInputStreamClass))
#define WEBKIT_IS_DIRECTORY_INPUT_STREAM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), WEBKIT_TYPE_DIRECTORY_INPUT_STREAM))
#define WEBKIT_DIRECTORY_INPUT_STREAM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), WEBKIT_TYPE_DIRECTORY_INPUT_STREAM, WebKitDirectoryInputStreamClass))

typedef struct _WebKitDirectoryInputStream WebKitDirectoryInputStream;
typedef struct _WebKitDirectoryInputStreamClass WebKitDirectoryInputStreamClass;
typedef struct _WebKitDirectoryInputStreamPrivate WebKitDirectoryInputStreamPrivate;

struct _WebKitDirectoryInputStream {
    GInputStream parent;

    WebKitDirectoryInputStreamPrivate* priv;
};

struct _WebKitDirectoryInputStreamClass {
    GInputStreamClass parentClass;
};

GType webkit_directory_input_stream_get_type(void);

GRefPtr<GInputStream> webkitDirectoryInputStreamNew(GRefPtr<GFileEnumerator>&&, CString&& uri);
