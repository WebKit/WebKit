/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2008 Collabora Ltd. All rights reserved.
 * Copyright (C) 2009, 2010 Kakai, Inc. <brian@kakai.com>
 * Copyright (C) 2010 Igalia S.L.
 * Copyright (C) 2014 Samsung Electronics
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
#include "PluginView.h"

#include "FrameView.h"
#include "GraphicsContext.h"
#include "JSDOMWindowBase.h"
#include "PlatformContextCairo.h"
#include "PluginPackage.h"
#include "RefPtrCairo.h"
#include <X11/X.h>
#include <cairo-xlib.h>
#include <runtime/JSLock.h>

#if PLATFORM(GTK)
#define String XtStringType
#include "gtk2xtbin.h"
#endif

#define Bool int // this became undefined in npruntime_internal.h
#define None 0L // ditto
#define Status int // ditto
#include <X11/extensions/Xrender.h>

namespace WebCore {

bool PluginView::dispatchNPEvent(NPEvent& event)
{
    if (!m_plugin->pluginFuncs()->event)
        return false;

    PluginView::setCurrentPluginView(this);
    JSC::JSLock::DropAllLocks dropAllLocks(JSDOMWindowBase::commonVM());
    setCallingPlugin(true);

    bool accepted = m_plugin->pluginFuncs()->event(m_instance, &event);

    setCallingPlugin(false);
    PluginView::setCurrentPluginView(0);
    return accepted;
}

void PluginView::updatePluginWidget()
{
    if (!parent())
        return;

    ASSERT(parent()->isFrameView());
    FrameView* frameView = toFrameView(parent());

    IntRect oldWindowRect = m_windowRect;
    IntRect oldClipRect = m_clipRect;

    m_windowRect = IntRect(frameView->contentsToWindow(frameRect().location()), frameRect().size());
    m_clipRect = windowClipRect();
    m_clipRect.move(-m_windowRect.x(), -m_windowRect.y());

    if (m_windowRect == oldWindowRect && m_clipRect == oldClipRect)
        return;

    if (m_status != PluginStatusLoadedSuccessfully)
        return;

    if (!m_isWindowed && !m_windowRect.isEmpty()) {
        Display* display = getPluginDisplay(nullptr);
        if (m_drawable)
            XFreePixmap(display, m_drawable);
            
        m_drawable = XCreatePixmap(display, getRootWindow(m_parentFrame.get()),
            m_windowRect.width(), m_windowRect.height(),
            ((NPSetWindowCallbackStruct*)m_npWindow.ws_info)->depth);
        XSync(display, false); // make sure that the server knows about the Drawable
    }

    setNPWindowIfNeeded();
}

void PluginView::handleFocusInEvent()
{
    if (!m_isStarted || m_status != PluginStatusLoadedSuccessfully)
        return;

    XEvent npEvent;
    initXEvent(&npEvent);

    XFocusChangeEvent& event = npEvent.xfocus;
    event.type = 9; // FocusIn gets unset somewhere
    event.mode = NotifyNormal;
    event.detail = NotifyDetailNone;

    dispatchNPEvent(npEvent);
}

void PluginView::invalidateRect(NPRect* rect)
{
    if (!rect) {
        invalidate();
        return;
    }

    IntRect r(rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top);
    invalidateWindowlessPluginRect(r);
}

void PluginView::invalidateRegion(NPRegion)
{
    invalidate();
}

void PluginView::handleFocusOutEvent()
{
    if (!m_isStarted || m_status != PluginStatusLoadedSuccessfully)
        return;

    XEvent npEvent;
    initXEvent(&npEvent);

    XFocusChangeEvent& event = npEvent.xfocus;
    event.type = 10; // FocusOut gets unset somewhere
    event.mode = NotifyNormal;
    event.detail = NotifyDetailNone;

    dispatchNPEvent(npEvent);
}

void PluginView::initXEvent(XEvent* xEvent)
{
    memset(xEvent, 0, sizeof(XEvent));

    xEvent->xany.serial = 0; // we are unaware of the last request processed by X Server
    xEvent->xany.send_event = false;
    xEvent->xany.display = getPluginDisplay(m_parentFrame.get());

    // Mozilla also sends None here for windowless plugins. See nsObjectFrame.cpp in the Mozilla sources.
    // This method also sets up FocusIn and FocusOut events for windows plugins, but Mozilla doesn't
    // even send these types of events to windowed plugins. In the future, it may be good to only
    // send them to windowless plugins.
    xEvent->xany.window = None;
}

void PluginView::paint(GraphicsContext* context, const IntRect& rect)
{
    if (!m_isStarted || m_status != PluginStatusLoadedSuccessfully) {
        paintMissingPluginIcon(context, rect);
        return;
    }

    if (context->paintingDisabled())
        return;

    setNPWindowIfNeeded();

    if (m_isWindowed)
        return;

    if (!m_drawable)
        return;

    Display* display = getPluginDisplay(nullptr);
    const bool syncX = m_pluginDisplay && m_pluginDisplay != display;

    IntRect exposedRect(rect);
    exposedRect.intersect(frameRect());
    exposedRect.move(-frameRect().x(), -frameRect().y());

    RefPtr<cairo_surface_t> drawableSurface = adoptRef(cairo_xlib_surface_create(display,
        m_drawable, m_visual, m_windowRect.width(), m_windowRect.height()));

    if (m_isTransparent) {
        // If we have a 32 bit drawable and the plugin wants transparency,
        // we'll clear the exposed area to transparent first. Otherwise,
        // we'd end up with junk in there from the last paint, or, worse,
        // uninitialized data.
        RefPtr<cairo_t> cr = adoptRef(cairo_create(drawableSurface.get()));

        if (!(cairo_surface_get_content(drawableSurface.get()) & CAIRO_CONTENT_ALPHA)) {
            // Attempt to fake it when we don't have an alpha channel on our
            // pixmap. If that's not possible, at least clear the window to
            // avoid drawing artifacts.

            // This Would not work without double buffering, but we always use it.
            cairo_set_source_surface(cr.get(), cairo_get_group_target(context->platformContext()->cr()),
                -m_windowRect.x(), -m_windowRect.y());
            cairo_set_operator(cr.get(), CAIRO_OPERATOR_SOURCE);
        } else
            cairo_set_operator(cr.get(), CAIRO_OPERATOR_CLEAR);

        cairo_rectangle(cr.get(), exposedRect.x(), exposedRect.y(),
            exposedRect.width(), exposedRect.height());
        cairo_fill(cr.get());
    }

    XEvent xevent;
    memset(&xevent, 0, sizeof(XEvent));
    XGraphicsExposeEvent& exposeEvent = xevent.xgraphicsexpose;
    exposeEvent.type = GraphicsExpose;
    exposeEvent.display = display;
    exposeEvent.drawable = m_drawable;
    exposeEvent.x = exposedRect.x();
    exposeEvent.y = exposedRect.y();
    exposeEvent.width = exposedRect.x() + exposedRect.width(); // flash bug? it thinks width is the right in transparent mode
    exposeEvent.height = exposedRect.y() + exposedRect.height(); // flash bug? it thinks height is the bottom in transparent mode

    dispatchNPEvent(xevent);

    if (syncX)
        XSync(m_pluginDisplay, false); // sync changes by plugin

    cairo_t* cr = context->platformContext()->cr();
    cairo_save(cr);

    cairo_set_source_surface(cr, drawableSurface.get(), frameRect().x(), frameRect().y());

    cairo_rectangle(cr, frameRect().x() + exposedRect.x(), frameRect().y() + exposedRect.y(), exposedRect.width(), exposedRect.height());
    cairo_clip(cr);

    if (m_isTransparent)
        cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    else
        cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint(cr);

    cairo_restore(cr);
}

void PluginView::setParent(ScrollView* parent)
{
    Widget::setParent(parent);

    if (parent)
        init();
}

void PluginView::setNPWindowRect(const IntRect&)
{
    if (!m_isWindowed)
        setNPWindowIfNeeded();
}

void PluginView::setNPWindowIfNeeded()
{
    if (!m_isStarted || !parent() || !m_plugin->pluginFuncs()->setwindow)
        return;

    // If the plugin didn't load sucessfully, no point in calling setwindow
    if (m_status != PluginStatusLoadedSuccessfully)
        return;

    // On Unix, only call plugin's setwindow if it's full-page or windowed
    if (m_mode != NP_FULL && m_mode != NP_EMBED)
        return;

    // Check if the platformPluginWidget still exists
    if (m_isWindowed && !platformPluginWidget())
        return;

    if (m_clipRect.isEmpty()) {
        // If width or height are null, set the clipRect to null,
        // indicating that the plugin is not visible/scrolled out.
        m_npWindow.clipRect.left = 0;
        m_npWindow.clipRect.right = 0;
        m_npWindow.clipRect.top = 0;
        m_npWindow.clipRect.bottom = 0;
    } else {
        // Clipping rectangle of the plug-in; the origin is the top left corner of the drawable or window. 
        m_npWindow.clipRect.left = m_npWindow.x + m_clipRect.x();
        m_npWindow.clipRect.top = m_npWindow.y + m_clipRect.y();
        m_npWindow.clipRect.right = m_npWindow.x + m_clipRect.x() + m_clipRect.width();
        m_npWindow.clipRect.bottom = m_npWindow.y + m_clipRect.y() + m_clipRect.height();
    }

    // FLASH WORKAROUND: Only set initially. Multiple calls to
    // setNPWindow() cause the plugin to crash in windowed mode.
    if (!m_plugin->quirks().contains(PluginQuirkDontCallSetWindowMoreThanOnce) || !m_isWindowed
        || m_npWindow.width == static_cast<uint32_t>(-1) || m_npWindow.height == static_cast<uint32_t>(-1)) {
        m_npWindow.width = m_windowRect.width();
        m_npWindow.height = m_windowRect.height();
    }

    PluginView::setCurrentPluginView(this);
    JSC::JSLock::DropAllLocks dropAllLocks(JSDOMWindowBase::commonVM());
    setCallingPlugin(true);
    m_plugin->pluginFuncs()->setwindow(m_instance, &m_npWindow);
    setCallingPlugin(false);
    PluginView::setCurrentPluginView(0);

    if (!m_isWindowed)
        return;

#if PLATFORM(GTK)
    // GtkXtBin will call gtk_widget_size_allocate, so we don't need to do it here.
    if (!m_needsXEmbed) {
        gtk_xtbin_set_position(GTK_XTBIN(platformPluginWidget()), m_windowRect.x(), m_windowRect.y());
        gtk_xtbin_resize(platformPluginWidget(), m_windowRect.width(), m_windowRect.height());
        return;
    }

    m_delayedAllocation = m_windowRect;
    updateWidgetAllocationAndClip();
#endif
}
} // namespace WebCore
