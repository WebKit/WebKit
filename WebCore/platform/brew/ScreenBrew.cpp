/*
 * Copyright (C) 2009 Company 100, Inc. All rights reserved.
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

#include <AEEAppGen.h>
#include <AEEStdLib.h>

namespace WebCore {

struct DisplayInfo {
    int width;
    int height;
    int depth;
};

static void getDisplayInfo(DisplayInfo& info)
{
    IDisplay* display = reinterpret_cast<AEEApplet*>(GETAPPINSTANCE())->m_pIDisplay;
    IBitmap* bitmap = IDisplay_GetDestination(display);
    ASSERT(bitmap);

    AEEBitmapInfo bitmapInfo;
    IBitmap_GetInfo(bitmap, &bitmapInfo, sizeof(AEEBitmapInfo));

    info.width  = bitmapInfo.cx;
    info.height = bitmapInfo.cy;
    info.depth  = bitmapInfo.nDepth;

    IBitmap_Release(bitmap);
}

FloatRect screenRect(Widget*)
{
    DisplayInfo info;
    getDisplayInfo(info);

    return FloatRect(0, 0, info.width, info.height);
}

FloatRect screenAvailableRect(Widget* widget)
{
    return screenRect(widget);
}

int screenDepth(Widget*)
{
    DisplayInfo info;
    getDisplayInfo(info);

    return info.depth;
}

int screenDepthPerComponent(Widget* widget)
{
    return screenDepth(widget);
}

bool screenIsMonochrome(Widget*)
{
    return false;
}

} // namespace WebCore

