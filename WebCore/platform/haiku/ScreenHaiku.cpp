/*
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com>
 *
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

#include "FloatRect.h"
#include "Frame.h"
#include "FrameView.h"
#include "IntRect.h"
#include "NotImplemented.h"
#include "Page.h"
#include "Widget.h"
#include <GraphicsDefs.h>
#include <interface/Screen.h>


namespace WebCore {

FloatRect screenRect(Widget*)
{
    BScreen screen;
    // FIXME: We assume this screen is valid
    return FloatRect(screen.Frame());
}

FloatRect screenAvailableRect(Widget* widget)
{
    // FIXME: We could use the get_deskbar_frame() function
    // from InterfaceDefs.h to make this smaller
    return screenRect(widget);
}

int screenDepth(Widget*)
{
    BScreen screen;
    // FIXME: We assume this screen is valid
    color_space cs = screen.ColorSpace();

    size_t pixelChunk, rowAlignment, pixelsPerChunk;
    if (get_pixel_size_for(cs, &pixelChunk, &rowAlignment, &pixelsPerChunk) == B_OK)
        // FIXME: Not sure if this is right
        return pixelChunk * 8;

    return 8;
}

int screenDepthPerComponent(Widget*)
{
    notImplemented();
    return 8;
}

bool screenIsMonochrome(Widget*)
{
    BScreen screen;
    // FIXME: We assume this screen is valid
    return screen.ColorSpace() == B_MONOCHROME_1_BIT;
}

} // namespace WebCore

