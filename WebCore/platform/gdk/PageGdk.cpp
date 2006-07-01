/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com 
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
#include "Page.h"

#include "FloatRect.h"
#include "Frame.h"
#include "FrameView.h"
#include <gdk/gdk.h>

namespace WebCore {

static GdkDrawable* rootWindowForFrame(Frame* frame)
{
    if (!frame)
        return 0;
    FrameView* frameView = frame->view();
    if (!frameView)
        return 0;
    GdkDrawable* drawable = frameView->drawable();
    if (!drawable)
        return 0;
    if (!GDK_WINDOW(drawable))
        return drawable;
    GdkWindow* window = GDK_WINDOW(drawable);
    return gdk_window_get_toplevel(window);
}

FloatRect Page::windowRect() const
{
    GdkDrawable* drawable = rootWindowForFrame(mainFrame());
    if (!drawable)
        return FloatRect();

    gint x, y, width, height, depth;

    if (!GDK_IS_WINDOW(drawable)) {
        gdk_drawable_get_size(drawable, &width, &height);
        return FloatRect(0, 0, width, height);
    }

    GdkWindow* window = GDK_WINDOW(drawable);
    gdk_window_get_geometry(window, &x, &y, &width, &height, &depth);
    return FloatRect(x, y, width, height);
}

void Page::setWindowRect(const FloatRect& r)
{
    GdkDrawable* drawable = rootWindowForFrame(mainFrame());
    if (!drawable || !GDK_IS_WINDOW(drawable))
        return;
    GdkWindow* window = GDK_WINDOW(drawable);
    gdk_window_move_resize(window, (int)r.x(), (int)r.y(), (int)r.width(), (int)r.height());
}

}
