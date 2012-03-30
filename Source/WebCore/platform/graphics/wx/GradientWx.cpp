/*
 * Copyright (C) 2008 Kevin Ollivier <kevino@theolliviers.com>  All rights reserved.
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
#include "Gradient.h"

#include "CSSParser.h"
#include "GraphicsContext.h"
#include "NotImplemented.h"

#include <algorithm>

#include <wx/dc.h>
#include <wx/dcgraph.h>
#include <wx/graphics.h>

using namespace std;

namespace WebCore {

void Gradient::platformDestroy()
{
    delete m_gradient;
}

PlatformGradient Gradient::platformGradient()
{
    if (m_gradient)
        return m_gradient;

#if wxCHECK_VERSION(2, 9, 1)
    sortStopsIfNecessary();
    
    wxGraphicsRenderer* renderer = 0;
#if wxUSE_CAIRO
    renderer = wxGraphicsRenderer::GetCairoRenderer();
#else
    renderer = wxGraphicsRenderer::GetDefaultRenderer();
#endif
    if (renderer) {
        wxGraphicsGradientStops stops;
        Vector<ColorStop>::iterator stopIterator = m_stops.begin();
        int numStops = 0;
        stops.SetStartColour(wxColour(255*stopIterator->red, 255*stopIterator->green, 255*stopIterator->blue, 255*stopIterator->alpha));
        
        while (stopIterator != m_stops.end()) {
            double position = stopIterator->stop;
            if (m_radial && m_r0)
                position = min((m_r0 / m_r1+position*(1.0f-m_r0 / m_r1)), 1.0);
            stops.Add(wxGraphicsGradientStop(wxColour(255*stopIterator->red, 255*stopIterator->green, 255*stopIterator->blue, 255*stopIterator->alpha), position));
            stopIterator++;
            numStops++;
        }
        stopIterator--;
        stops.SetEndColour(wxColour(255*stopIterator->red, 255*stopIterator->green, 255*stopIterator->blue, 255*stopIterator->alpha));
        
        wxGraphicsBrush gradient;
        if (numStops >= 2) {
            if (m_radial)
                gradient = renderer->CreateRadialGradientBrush(m_p1.x(), m_p1.y(), m_p0.x(), m_p0.y(), m_r1, stops);
            else
                gradient = renderer->CreateLinearGradientBrush(m_p0.x(), m_p0.y(), m_p1.x(), m_p1.y(), stops);
        }
        m_gradient = new wxGraphicsBrush(gradient);
    }
#endif
    return m_gradient;
}

void Gradient::fill(GraphicsContext* c, const FloatRect& r)
{    
#if wxCHECK_VERSION(2, 9, 1)
    wxGCDC* context = dynamic_cast<wxGCDC*>(c->platformContext());

    if (!context)
        return;
    wxGraphicsContext* gradientContext = context->GetGraphicsContext();

    gradientContext->SetPen(*wxTRANSPARENT_PEN);
    gradientContext->SetBrush(*platformGradient());
    gradientContext->DrawRectangle(r.x(), r.y(), r.width(), r.height());
#endif
}

} //namespace
