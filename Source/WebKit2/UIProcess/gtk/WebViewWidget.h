/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
 * Copyright (C) 2011 Igalia S.L.
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

#ifndef WebViewWidget_h
#define WebViewWidget_h

#include "WebView.h"

#include <gtk/gtk.h>

using namespace WebKit;

G_BEGIN_DECLS

#define WEB_VIEW_TYPE_WIDGET              (webViewWidgetGetType())
#define WEB_VIEW_WIDGET(object)           (G_TYPE_CHECK_INSTANCE_CAST((object), WEB_VIEW_TYPE_WIDGET, WebViewWidget))
#define WEB_VIEW_WIDGET_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), WEB_VIEW_TYPE_WIDGET, WebViewWidgetClass))
#define WEB_VIEW_IS_WIDGET(object)        (G_TYPE_CHECK_INSTANCE_TYPE((object), WEB_VIEW_TYPE_WIDGET))
#define WEB_VIEW_IS_CLASS(klass)          (G_TYPE_CHECK_CLASS_TYPE((klass), WEB_VIEW_TYPE_WIDGET))
#define WEB_VIEW_WIDGET_GET_CLASS(object) (G_TYPE_INSTANCE_GET_CLASS((object), WEB_VIEW_TYPE_WIDGET, WebViewWidgetClass))

typedef struct _WebViewWidget WebViewWidget;
typedef struct _WebViewWidgetClass WebViewWidgetClass;
typedef struct _WebViewWidgetPrivate WebViewWidgetPrivate;

struct _WebViewWidget {
    GtkContainer parentInstance;
    /*< private >*/
    WebViewWidgetPrivate* priv;
};

struct _WebViewWidgetClass {
    GtkContainerClass parentClass;
};

GType webViewWidgetGetType();

WebView* webViewWidgetGetWebViewInstance(WebViewWidget*);

void webViewWidgetSetWebViewInstance(WebViewWidget*, WebView*);

GtkIMContext* webViewWidgetGetIMContext(WebViewWidget*);

G_END_DECLS

#endif // WebViewWidget_h
