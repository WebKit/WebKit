/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
#include "GraphicsContext.h"

#if PLATFORM(CAIRO)

#include <cairo-win32.h>

namespace WebCore
{
    
HDC GraphicsContext::getWindowsContext(bool /*supportAlphaBlend*/, const IntRect*)
{
    cairo_surface_t* surface = cairo_get_target(platformContext());
    HDC hdc = cairo_win32_surface_get_dc(surface);    
    SaveDC(hdc);

    // FIXME: We need to make sure a clip is really set on the HDC.
    // Call SetWorldTransform to honor the current Cairo transform.
    SetGraphicsMode(hdc, GM_ADVANCED); // We need this call for themes to honor world transforms.
    cairo_matrix_t mat;
    cairo_get_matrix(platformContext(), &mat);
    XFORM xform;
    xform.eM11 = mat.xx;
    xform.eM12 = mat.xy;
    xform.eM21 = mat.yx;
    xform.eM22 = mat.yy;
    xform.eDx = mat.x0;
    xform.eDy = mat.y0;
    SetWorldTransform(hdc, &xform);

    return hdc;
}

void GraphicsContext::releaseWindowsContext(HDC, bool /*supportAlphaBlend*/, const IntRect*)
{
    cairo_surface_t* surface = cairo_get_target(platformContext());
    HDC hdc = cairo_win32_surface_get_dc(surface);
    RestoreDC(hdc, -1);
    cairo_surface_mark_dirty(surface);
}

}

#endif
