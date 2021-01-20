/*
 * Copyright (C) 2018, 2019 Igalia S.L
 * Copyright (C) 2018, 2019 Zodiac Inflight Innovations
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

// This include order is necessary to enforce the GBM EGL platform.
#include <gbm.h>
#include <epoxy/egl.h>

#include <QHoverEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QPointer>
#include <QWheelEvent>
#include <wpe/fdo-egl.h>
#include <wpe/fdo.h>

class WPEQtView;

class Q_DECL_EXPORT WPEQtViewBackend {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static std::unique_ptr<WPEQtViewBackend> create(const QSizeF&, QPointer<QOpenGLContext>, EGLDisplay, QPointer<WPEQtView>);
    WPEQtViewBackend(const QSizeF&, EGLDisplay, EGLContext, QPointer<QOpenGLContext>, QPointer<WPEQtView>);
    virtual ~WPEQtViewBackend();

    void resize(const QSizeF&);
    GLuint texture(QOpenGLContext*);
    bool hasValidSurface() const { return m_surface.isValid(); };

    void dispatchHoverEnterEvent(QHoverEvent*);
    void dispatchHoverLeaveEvent(QHoverEvent*);
    void dispatchHoverMoveEvent(QHoverEvent*);

    void dispatchMousePressEvent(QMouseEvent*);
    void dispatchMouseReleaseEvent(QMouseEvent*);
    void dispatchWheelEvent(QWheelEvent*);

    void dispatchKeyEvent(QKeyEvent*, bool state);

    void dispatchTouchEvent(QTouchEvent*);

    struct wpe_view_backend* backend() const { return wpe_view_backend_exportable_fdo_get_view_backend(m_exportable); };

private:
    void displayImage(struct wpe_fdo_egl_exported_image*);
    uint32_t modifiers() const;

    EGLDisplay m_eglDisplay { nullptr };
    EGLContext m_eglContext { nullptr };
    struct wpe_view_backend_exportable_fdo* m_exportable { nullptr };
    struct wpe_fdo_egl_exported_image* m_lockedImage { nullptr };

    QPointer<WPEQtView> m_view;
    QOffscreenSurface m_surface;
    QSizeF m_size;
    GLuint m_textureId { 0 };
    unsigned m_program { 0 };
    unsigned m_textureUniform { 0 };

    bool m_hovering { false };
    uint32_t m_mouseModifiers { 0 };
    uint32_t m_keyboardModifiers { 0 };
    uint32_t m_mousePressedButton { 0 };
};
