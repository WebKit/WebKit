/*
 * Copyright (C) 2013 Igalia S.L.
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

#ifndef BrowserSearchBar_h
#define BrowserSearchBar_h

#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

#if !GTK_CHECK_VERSION(3, 98, 0)

G_BEGIN_DECLS

#define BROWSER_TYPE_SEARCH_BAR            (browser_search_bar_get_type())
#define BROWSER_SEARCH_BAR(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), BROWSER_TYPE_SEARCH_BAR, BrowserSearchBar))
#define BROWSER_IS_SEARCH_BAR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), BROWSER_TYPE_SEARCH_BAR))
#define BROWSER_SEARCH_BAR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  BROWSER_TYPE_SEARCH_BAR, BrowserSearchBarClass))
#define BROWSER_IS_SEARCH_BAR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  BROWSER_TYPE_SEARCH_BAR))
#define BROWSER_SEARCH_BAR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  BROWSER_TYPE_SEARCH_BAR, BrowserSearchBarClass))

typedef struct _BrowserSearchBar       BrowserSearchBar;
typedef struct _BrowserSearchBarClass  BrowserSearchBarClass;

struct _BrowserSearchBarClass {
    GtkSearchBarClass parent_class;
};

GType browser_search_bar_get_type(void);

GtkWidget *browser_search_bar_new(WebKitWebView *);
void browser_search_bar_open(BrowserSearchBar *);
void browser_search_bar_close(BrowserSearchBar *);
gboolean browser_search_bar_is_open(BrowserSearchBar *);


G_END_DECLS

#endif

#endif
