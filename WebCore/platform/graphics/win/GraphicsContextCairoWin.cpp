/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#include "TransformationMatrix.h"
#include "Path.h"

#include <cairo-win32.h>
#include "GraphicsContextPlatformPrivateCairo.h"

using namespace std;

namespace WebCore {

GraphicsContext::GraphicsContext(HDC dc, bool hasAlpha)
    : m_common(createGraphicsContextPrivate())
    , m_data(new GraphicsContextPlatformPrivate)
{
    if (dc) {
        cairo_surface_t* surface = cairo_win32_surface_create(dc);
        m_data->cr = cairo_create(surface);
        m_data->m_hdc = dc;
    } else {
        setPaintingDisabled(true);
        m_data->cr = 0;
        m_data->m_hdc = 0;
    }

    if (m_data->cr) {
        // Make sure the context starts in sync with our state.
        setPlatformFillColor(fillColor());
        setPlatformStrokeColor(strokeColor());
    }
}

HDC GraphicsContext::getWindowsContext(const IntRect& dstRect, bool supportAlphaBlend, bool mayCreateBitmap)
{
    // FIXME:  We aren't really doing anything with the 'mayCreateBitmap' flag.  This needs
    // to be addressed.
    if (dstRect.isEmpty())
       return 0;

    // This is probably wrong, and definitely out of date.  Pulled from old SVN
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
    ::SetWorldTransform(hdc, &xform);

    return hdc;
}

void GraphicsContext::releaseWindowsContext(HDC hdc, const IntRect& dstRect, bool supportAlphaBlend, bool mayCreateBitmap)
{
    // FIXME:  We aren't really doing anything with the 'mayCreateBitmap' flag.  This needs
    // to be addressed.
    if (dstRect.isEmpty())
       return;

    cairo_surface_t* surface = cairo_get_target(platformContext());
    HDC hdc2 = cairo_win32_surface_get_dc(surface);
    RestoreDC(hdc2, -1);
    cairo_surface_mark_dirty(surface);
}

void GraphicsContextPlatformPrivate::concatCTM(const TransformationMatrix& transform)
{
    cairo_surface_t* surface = cairo_get_target(cr);
    HDC hdc = cairo_win32_surface_get_dc(surface);   
    SaveDC(hdc);

    const cairo_matrix_t* matrix = reinterpret_cast<const cairo_matrix_t*>(&transform);

    XFORM xform;
    xform.eM11 = matrix->xx;
    xform.eM12 = matrix->xy;
    xform.eM21 = matrix->yx;
    xform.eM22 = matrix->yy;
    xform.eDx = matrix->x0;
    xform.eDy = matrix->y0;

    ModifyWorldTransform(hdc, &xform, MWT_LEFTMULTIPLY);
}

}
