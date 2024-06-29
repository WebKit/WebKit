/*
 * Copyright (C) 2024 Igalia S.L.
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

#ifndef WPEViewQtQuick_h
#define WPEViewQtQuick_h

#include "WPEDisplayQtQuick.h"

class QHoverEvent;
class QKeyEvent;
class QMouseEvent;
class QSGTexture;
class QSize;
class QTouchEvent;
class QWheelEvent;
class WPEQtView;

G_BEGIN_DECLS

#define WPE_TYPE_VIEW_QTQUICK (wpe_view_qtquick_get_type())
G_DECLARE_FINAL_TYPE (WPEViewQtQuick, wpe_view_qtquick, WPE, VIEW_QTQUICK, WPEView)

WPEView *wpe_view_qtquick_new                              (WPEDisplayQtQuick *display);
gboolean         wpe_view_qtquick_initialize_rendering     (WPEViewQtQuick    *view, WPEQtView   *wpeQtView, GError **error);

QSGTexture*      wpe_view_qtquick_render_buffer_to_texture (WPEViewQtQuick    *view, QSize             size, GError **error);
void             wpe_view_qtquick_did_update_scene         (WPEViewQtQuick    *view);

void             wpe_view_dispatch_mouse_press_event       (WPEViewQtQuick    *view, QMouseEvent *);
void             wpe_view_dispatch_mouse_move_event        (WPEViewQtQuick    *view, QMouseEvent *);
void             wpe_view_dispatch_mouse_release_event     (WPEViewQtQuick    *view, QMouseEvent *);
void             wpe_view_dispatch_wheel_event             (WPEViewQtQuick    *view, QWheelEvent *);

void             wpe_view_dispatch_hover_enter_event       (WPEViewQtQuick    *view, QHoverEvent *);
void             wpe_view_dispatch_hover_move_event        (WPEViewQtQuick    *view, QHoverEvent *);
void             wpe_view_dispatch_hover_leave_event       (WPEViewQtQuick    *view, QHoverEvent *);

void             wpe_view_dispatch_key_press_event         (WPEViewQtQuick    *view, QKeyEvent *);
void             wpe_view_dispatch_key_release_event       (WPEViewQtQuick    *view, QKeyEvent *);

void             wpe_view_dispatch_touch_event             (WPEViewQtQuick    *view, QTouchEvent *);

G_END_DECLS

#endif /* WPEViewQtQuick_h */
