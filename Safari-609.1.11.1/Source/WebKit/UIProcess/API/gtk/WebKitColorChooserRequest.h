/*
 * Copyright (C) 2015 Igalia S.L.
 * Copyright (c) 2012, Samsung Electronics
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#if !defined(__WEBKIT2_H_INSIDE__) && !defined(WEBKIT2_COMPILATION)
#error "Only <webkit2/webkit2.h> can be included directly."
#endif

#ifndef WebKitColorChooserRequest_h
#define WebKitColorChooserRequest_h

#include <gtk/gtk.h>
#include <webkit2/WebKitDefines.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_COLOR_CHOOSER_REQUEST            (webkit_color_chooser_request_get_type())
#define WEBKIT_COLOR_CHOOSER_REQUEST(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_COLOR_CHOOSER_REQUEST, WebKitColorChooserRequest))
#define WEBKIT_IS_COLOR_CHOOSER_REQUEST(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_COLOR_CHOOSER_REQUEST))
#define WEBKIT_COLOR_CHOOSER_REQUEST_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_TYPE_COLOR_CHOOSER_REQUEST, WebKitColorChooserRequestClass))
#define WEBKIT_IS_COLOR_CHOOSER_REQUEST_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_TYPE_COLOR_CHOOSER_REQUEST))
#define WEBKIT_COLOR_CHOOSER_REQUEST_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_TYPE_COLOR_CHOOSER_REQUEST, WebKitColorChooserRequestClass))

typedef struct _WebKitColorChooserRequest        WebKitColorChooserRequest;
typedef struct _WebKitColorChooserRequestClass   WebKitColorChooserRequestClass;
typedef struct _WebKitColorChooserRequestPrivate WebKitColorChooserRequestPrivate;

struct _WebKitColorChooserRequest {
    GObject parent;

    /*< private >*/
    WebKitColorChooserRequestPrivate *priv;
};

struct _WebKitColorChooserRequestClass {
    GObjectClass parent_class;
};

WEBKIT_API GType
webkit_color_chooser_request_get_type              (void);

WEBKIT_API void
webkit_color_chooser_request_get_rgba              (WebKitColorChooserRequest *request,
                                                    GdkRGBA                   *rgba);

WEBKIT_API void
webkit_color_chooser_request_set_rgba              (WebKitColorChooserRequest *request,
                                                    const GdkRGBA             *rgba);

WEBKIT_API void
webkit_color_chooser_request_get_element_rectangle (WebKitColorChooserRequest *request,
                                                    GdkRectangle              *rect);

WEBKIT_API void
webkit_color_chooser_request_finish                (WebKitColorChooserRequest *request);

WEBKIT_API void
webkit_color_chooser_request_cancel                (WebKitColorChooserRequest *request);

G_END_DECLS

#endif
