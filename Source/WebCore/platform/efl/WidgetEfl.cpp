/*
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com
 * Copyright (C) 2007 Holger Hans Peter Freyther
 * Copyright (C) 2008 Kenneth Rohde Christiansen
 * Copyright (C) 2009-2010 ProFUSION embedded systems
 * Copyright (C) 2009-2010 Samsung Electronics
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "config.h"
#include "Widget.h"

#include "ChromeClient.h"
#include "Cursor.h"
#include "EflScreenUtilities.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "IntRect.h"
#include "NotImplemented.h"
#include "Page.h"

#include <Ecore.h>
#include <Ecore_Evas.h>
#include <Edje.h>
#include <Evas.h>

#ifdef HAVE_ECORE_X
#include <Ecore_X.h>
#endif

namespace WebCore {

class WidgetPrivate {
public:
    Evas* m_evas;
    Evas_Object* m_evasObject;
    String m_theme;

    WidgetPrivate()
        : m_evas(0)
        , m_evasObject(0)
        , m_cursorObject(0)
#ifdef HAVE_ECORE_X
        , m_isUsingEcoreX(false)
#endif
    { }

    /* cursor */
    String m_cursorGroup;
    Evas_Object* m_cursorObject;

#ifdef HAVE_ECORE_X
    bool m_isUsingEcoreX;
#endif
};

Widget::Widget(PlatformWidget widget)
    : m_parent(0)
    , m_widget(0)
    , m_selfVisible(false)
    , m_parentVisible(false)
    , m_frame(0, 0, 0, 0)
    , m_data(new WidgetPrivate)
{
    init(widget);
}

Widget::~Widget()
{
    ASSERT(!parent());

    if (m_data->m_cursorObject)
        evas_object_del(m_data->m_cursorObject);

    delete m_data;
}

IntRect Widget::frameRect() const
{
    return m_frame;
}

void Widget::setFrameRect(const IntRect& rect)
{
    m_frame = rect;
    Widget::frameRectsChanged();
}

void Widget::frameRectsChanged()
{
    Evas_Object* object = evasObject();
    Evas_Coord x, y;

    if (!parent() || !object)
        return;

    IntRect rect = frameRect();
    if (parent()->isScrollViewScrollbar(this))
        rect.setLocation(parent()->convertToContainingWindow(rect.location()));
    else
        rect.setLocation(parent()->contentsToWindow(rect.location()));

    evas_object_geometry_get(root()->evasObject(), &x, &y, 0, 0);
    evas_object_move(object, x + rect.x(), y + rect.y());
    evas_object_resize(object, rect.width(), rect.height());
}

void Widget::setFocus(bool focused)
{
}

void Widget::applyFallbackCursor()
{
#ifdef HAVE_ECORE_X
    if (m_data->m_isUsingEcoreX && !m_data->m_cursorGroup.isNull()) {
        int shape = getEcoreCursor(m_data->m_cursorGroup);

        if (shape < ECORE_X_CURSOR_X || shape > ECORE_X_CURSOR_XTERM) {
            LOG_ERROR("cannot map an equivalent X cursor for"
                      " c ursor group %s", m_data->m_cursorGroup.utf8().data());
            shape = ECORE_X_CURSOR_LEFT_PTR;
        }

        Ecore_X_Window win = ecore_evas_software_x11_window_get(ecoreEvas());
        Ecore_X_Cursor cur = ecore_x_cursor_shape_get(shape);
        ecore_x_window_cursor_set(win, cur);
        return;
    }
#endif
}

void Widget::applyCursor()
{
    CString file = edjeThemeRecursive().utf8();

    m_data->m_cursorObject = edje_object_add(evas());
    if (!file.isNull() && !edje_object_file_set(m_data->m_cursorObject, file.data(), m_data->m_cursorGroup.utf8().data())) {
        evas_object_del(m_data->m_cursorObject);
        m_data->m_cursorObject = 0;
        ecore_evas_object_cursor_set(ecoreEvas(), 0, 0, 0, 0);
        applyFallbackCursor();
    } else {
        Evas_Coord x, y, w, h;
        const char *d;

        edje_object_size_min_get(m_data->m_cursorObject, &w, &h);
        if ((w <= 0) || (h <= 0))
            edje_object_size_min_calc(m_data->m_cursorObject, &w, &h);
        if ((w <= 0) || (h <= 0))
            w = h = 16;
        evas_object_resize(m_data->m_cursorObject, w, h);

        d = edje_object_data_get(m_data->m_cursorObject, "hot.x");
        x = d ? atoi(d) : 0;

        d = edje_object_data_get(m_data->m_cursorObject, "hot.y");
        y = d ? atoi(d) : 0;

        ecore_evas_object_cursor_set(ecoreEvas(), m_data->m_cursorObject,
                                     EVAS_LAYER_MAX, x, y);
    }
}

void Widget::setCursor(const Cursor& cursor)
{
    if (!evas())
         return;

    const char* group = cursor.platformCursor();
    if (!group || String(group) == m_data->m_cursorGroup)
        return;

    m_data->m_cursorGroup = group;

    applyCursor();
}

void Widget::show()
{
    if (!platformWidget())
         return;

    evas_object_show(platformWidget());
}

void Widget::hide()
{
    if (!platformWidget())
         return;

    evas_object_hide(platformWidget());
}

void Widget::paint(GraphicsContext* context, const IntRect&)
{
    notImplemented();
}

void Widget::setIsSelected(bool)
{
    notImplemented();
}

const String Widget::edjeTheme() const
{
    return m_data->m_theme;
}

void Widget::setEdjeTheme(const String& themePath)
{
    if (m_data->m_theme == themePath)
        return;

    m_data->m_theme = themePath;
}

const String Widget::edjeThemeRecursive() const
{
    if (!m_data->m_theme.isNull())
        return m_data->m_theme;
    if (m_parent)
        return m_parent->edjeThemeRecursive();

    return String();
}

Evas* Widget::evas() const
{
    return m_data->m_evas;
}

Ecore_Evas* Widget::ecoreEvas() const
{
    // FIXME EFL: XXX assume evas was created by ecore_evas
    return static_cast<Ecore_Evas*>(evas_data_attach_get(evas()));
}

void Widget::setEvasObject(Evas_Object *object)
{
    // FIXME: study platformWidget() and use it
    // FIXME: right now platformWidget() requires implementing too much
    if (m_data->m_evasObject == object)
        return;
    m_data->m_evasObject = object;
    if (!object) {
        m_data->m_evas = 0;
#ifdef HAVE_ECORE_X
        m_data->m_isUsingEcoreX = false;
#endif
        return;
    }

    m_data->m_evas = evas_object_evas_get(object);

#ifdef HAVE_ECORE_X
    const char *engine = ecore_evas_engine_name_get(ecoreEvas());
    m_data->m_isUsingEcoreX = (!strcmp(engine, "software_x11")
                               || !strcmp(engine, "software_xcb")
                               || !strcmp(engine, "software_16_x11")
                               || !strncmp(engine, "xrender", sizeof("xrender") - 1));
#endif

    Widget::frameRectsChanged();
}

Evas_Object* Widget::evasObject() const
{
    return m_data->m_evasObject;
}

}
