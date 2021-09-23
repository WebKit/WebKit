/*
 * Copyright (C) 2019 Microsoft Corporation.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#if !defined(__WEBKIT_H_INSIDE__) && !defined(WEBKIT2_COMPILATION)
#error "Only <wpe/webkit.h> can be included directly."
#endif

#ifndef WebKitBrowserInspector_h
#define WebKitBrowserInspector_h

#include <glib-object.h>
#include <wpe/WebKitDefines.h>
#include <wpe/WebKitWebView.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_BROWSER_INSPECTOR            (webkit_browser_inspector_get_type())
#define WEBKIT_BROWSER_INSPECTOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_BROWSER_INSPECTOR, WebKitBrowserInspector))
#define WEBKIT_IS_BROWSER_INSPECTOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_BROWSER_INSPECTOR))
#define WEBKIT_BROWSER_INSPECTOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_TYPE_BROWSER_INSPECTOR, WebKitBrowserInspectorClass))
#define WEBKIT_IS_BROWSER_INSPECTOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_TYPE_BROWSER_INSPECTOR))
#define WEBKIT_BROWSER_INSPECTOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_TYPE_BROWSER_INSPECTOR, WebKitBrowserInspectorClass))

typedef struct _WebKitBrowserInspector        WebKitBrowserInspector;
typedef struct _WebKitBrowserInspectorClass   WebKitBrowserInspectorClass;
typedef struct _WebKitBrowserInspectorPrivate WebKitBrowserInspectorPrivate;

struct _WebKitBrowserInspector {
    GObject parent;

    WebKitBrowserInspectorPrivate *priv;
};

struct _WebKitBrowserInspectorClass {
    GObjectClass parent_class;

    WebKitWebView *(* create_new_page)           (WebKitBrowserInspector    *browser_inspector,
                                                  WebKitWebContext          *context);
    WebKitWebView *(* quit_application)          (WebKitBrowserInspector    *browser_inspector);

    void (*_webkit_reserved0) (void);
    void (*_webkit_reserved1) (void);
    void (*_webkit_reserved2) (void);
    void (*_webkit_reserved3) (void);
};

WEBKIT_API GType
webkit_browser_inspector_get_type                     (void);

WEBKIT_API WebKitBrowserInspector *
webkit_browser_inspector_get_default                  (void);

WEBKIT_API void
webkit_browser_inspector_initialize_pipe              (const char* defaultProxyURI,
                                                       const char* const* ignoreHosts);

G_END_DECLS

#endif
