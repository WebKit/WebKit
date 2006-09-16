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
#include "Screen.h"

#include "Page.h"
#include "Frame.h"
#include "Widget.h"
#include "FloatRect.h"

#include <gdk/gdk.h>

namespace WebCore {

static GdkDrawable* drawableForPage(const Page* page)
{
    Frame* frame = (page ? page->mainFrame() : 0);
    FrameView* frameView = (frame ? frame->view() : 0);
    
    if (!frameView)
        return 0;
    
    return frameView->drawable();
}

FloatRect screenRect(const Page* page)
{
    GdkDrawable* drawable = drawableForPage(page);
    if (!drawable)
        return FloatRect();
    GdkScreen* screen = gdk_drawable_get_screen(drawable);
    return FloatRect(0, 0, gdk_screen_get_width(screen), gdk_screen_get_height(screen));
}

int screenDepth(const Page* page)
{
    GdkDrawable* drawable = drawableForPage(page);
    if (!drawable)
        return 32;
    return gdk_drawable_get_depth(drawable);
}

FloatRect usableScreenRect(const Page* page)
{
    return screenRect(page);
}

float scaleFactor(const Page*)
{
    return 1.0f;

}

}
