/*
 * Copyright (C) 2025 Igalia S.L.
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
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WPEClipboard_h
#define WPEClipboard_h

#if !defined(__WPE_PLATFORM_H_INSIDE__) && !defined(BUILDING_WEBKIT)
#error "Only <wpe/wpe-platform.h> can be included directly."
#endif

#include <gio/gio.h>
#include <glib-object.h>
#include <wpe/WPEDefines.h>

G_BEGIN_DECLS

#define WPE_TYPE_CLIPBOARD (wpe_clipboard_get_type())
WPE_DECLARE_DERIVABLE_TYPE (WPEClipboard, wpe_clipboard, WPE, CLIPBOARD, GObject)

typedef struct _WPEClipboardContent WPEClipboardContent;
typedef struct _WPEDisplay WPEDisplay;

struct _WPEClipboardClass
{
    GObjectClass parent_class;

    GBytes* (*read)    (WPEClipboard        *clipboard,
                        const char          *format);
    void    (*changed) (WPEClipboard        *clipboard,
                        GPtrArray           *formats,
                        gboolean             is_local,
                        WPEClipboardContent *content);

    gpointer padding[32];
};

WPE_API WPEClipboard        *wpe_clipboard_new              (WPEDisplay          *display);
WPE_API WPEDisplay          *wpe_clipboard_get_display      (WPEClipboard        *clipboard);
WPE_API gint64               wpe_clipboard_get_change_count (WPEClipboard        *clipboard);
WPE_API const char * const  *wpe_clipboard_get_formats      (WPEClipboard        *clipboard);
WPE_API void                 wpe_clipboard_set_content      (WPEClipboard        *clipboard,
                                                             WPEClipboardContent *content);
WPE_API WPEClipboardContent *wpe_clipboard_get_content      (WPEClipboard        *clipboard);
WPE_API GBytes              *wpe_clipboard_read_bytes       (WPEClipboard        *clipboard,
                                                             const char          *format);
WPE_API char                *wpe_clipboard_read_text        (WPEClipboard        *clipboard,
                                                             const char          *format,
                                                             gsize               *size);

#define WPE_TYPE_CLIPBOARD_CONTENT (wpe_clipboard_content_get_type())

WPE_API GType                wpe_clipboard_content_get_type   (void);
WPE_API WPEClipboardContent *wpe_clipboard_content_new        (void);
WPE_API WPEClipboardContent *wpe_clipboard_content_ref        (WPEClipboardContent *content);
WPE_API void                 wpe_clipboard_content_unref      (WPEClipboardContent *content);
WPE_API void                 wpe_clipboard_content_set_text   (WPEClipboardContent *content,
                                                               const char          *text);
WPE_API const char          *wpe_clipboard_content_get_text   (WPEClipboardContent *content);
WPE_API void                 wpe_clipboard_content_set_bytes  (WPEClipboardContent *content,
                                                               const char          *format,
                                                               GBytes              *bytes);
WPE_API GBytes              *wpe_clipboard_content_get_bytes  (WPEClipboardContent *content,
                                                               const char          *format);
WPE_API gboolean             wpe_clipboard_content_serialize  (WPEClipboardContent *content,
                                                               const char          *format,
                                                               GOutputStream       *stream);

G_END_DECLS

#endif /* WPEClipboard_h */
