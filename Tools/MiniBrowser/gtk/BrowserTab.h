/*
 * Copyright (C) 2016 Igalia S.L.
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

#ifndef BrowserTab_h
#define BrowserTab_h

#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

G_BEGIN_DECLS

#define BROWSER_TYPE_TAB            (browser_tab_get_type())
#define BROWSER_TAB(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), BROWSER_TYPE_TAB, BrowserTab))
#define BROWSER_TAB_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  BROWSER_TYPE_TAB, BrowserTabClass))
#define BROWSER_IS_TAB(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), BROWSER_TYPE_TAB))
#define BROWSER_IS_TAB_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  BROWSER_TYPE_TAB))
#define BROWSER_TAB_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  BROWSER_TYPE_TAB, BrowserTabClass))

typedef struct _BrowserTab        BrowserTab;
typedef struct _BrowserTabClass   BrowserTabClass;

GType browser_tab_get_type(void);

GtkWidget* browser_tab_new(WebKitWebView*);
WebKitWebView* browser_tab_get_web_view(BrowserTab*);
void browser_tab_load_uri(BrowserTab*, const char* uri);
GtkWidget *browser_tab_get_title_widget(BrowserTab*);
void browser_tab_set_status_text(BrowserTab*, const char* text);
void browser_tab_toggle_inspector(BrowserTab*);
void browser_tab_start_search(BrowserTab*);
void browser_tab_stop_search(BrowserTab*);
void browser_tab_enter_fullscreen(BrowserTab*);
void browser_tab_leave_fullscreen(BrowserTab*);
void browser_tab_set_background_color(BrowserTab*, GdkRGBA*);

G_END_DECLS

#endif
