/*
 * Copyright (C) 2007 Kevin Ollivier <kevino@theolliviers.com>
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
#include "FloatRect.h"
#include "Font.h"
#include "IntRect.h"
#include "NotImplemented.h"
#include "Pen.h"
#include <wtf/MathExtras.h>

#include <math.h>
#include <stdio.h>

#include <wx/defs.h>
#include <wx/window.h>
#include <wx/dcclient.h>
#include <wx/dcgraph.h>
#include <wx/graphics.h>

#if __WXMAC__
#include <Carbon/Carbon.h>
#elif __WXMSW__
#include <windows.h>
#endif

namespace WebCore {

int getWxCompositingOperation(CompositeOperator op, bool hasAlpha)
{
    // FIXME: Add support for more operators.
    if (op == CompositeSourceOver && !hasAlpha)
        op = CompositeCopy;

    int function;
    switch (op) {
        case CompositeClear:
            function = wxCLEAR;
        case CompositeCopy:
            function = wxCOPY; 
            break;
        default:
            function = wxCOPY;
    }
    return function;
}

static int strokeStyleToWxPenStyle(int p)
{
    if (p == SolidStroke)
        return wxSOLID;
    if (p == DottedStroke)
        return wxDOT;
    if (p == DashedStroke)
        return wxLONG_DASH;
    if (p == NoStroke)
        return wxTRANSPARENT;
    
    return wxSOLID;
}

class GraphicsContextPlatformPrivate {
public:
    GraphicsContextPlatformPrivate();
    ~GraphicsContextPlatformPrivate();

#if USE(WXGC)
    wxGCDC* context;
#else
    wxWindowDC* context;
#endif
    int mswDCStateID;
    wxRegion gtkCurrentClipRgn;
    wxRegion gtkPaintClipRgn;
};

GraphicsContextPlatformPrivate::GraphicsContextPlatformPrivate() :
    context(0),
    mswDCStateID(0),
    gtkCurrentClipRgn(wxRegion()),
    gtkPaintClipRgn(wxRegion())
{
}

GraphicsContextPlatformPrivate::~GraphicsContextPlatformPrivate()
{
}


GraphicsContext::GraphicsContext(PlatformGraphicsContext* context)
    : m_common(createGraphicsContextPrivate())
    , m_data(new GraphicsContextPlatformPrivate)
{    
    setPaintingDisabled(!context);
    if (context) {
        // Make sure the context starts in sync with our state.
        setPlatformFillColor(fillColor(), DeviceColorSpace);
        setPlatformStrokeColor(strokeColor(), DeviceColorSpace);
    }
#if USE(WXGC)
    m_data->context = (wxGCDC*)context;
#else
    m_data->context = (wxWindowDC*)context;
#endif
}

GraphicsContext::~GraphicsContext()
{
    destroyGraphicsContextPrivate(m_common);
    delete m_data;
}

PlatformGraphicsContext* GraphicsContext::platformContext() const
{
    return (PlatformGraphicsContext*)m_data->context;
}

void GraphicsContext::savePlatformState()
{
    if (m_data->context)
    {
#if USE(WXGC)
        wxGraphicsContext* gc = m_data->context->GetGraphicsContext();
        gc->PushState();
#else
    // when everything is working with USE_WXGC, we can remove this
    #if __WXMAC__
        CGContextRef context;
        wxGraphicsContext* gc = m_data->context->GetGraphicsContext();
        if (gc)
            context = (CGContextRef)gc->GetNativeContext();
        if (context)
            CGContextSaveGState(context);
    #elif __WXMSW__
        HDC dc = (HDC)m_data->context->GetHDC();
        m_data->mswDCStateID = ::SaveDC(dc);
    #elif __WXGTK__
        m_data->gtkCurrentClipRgn = m_data->context->m_currentClippingRegion;
        m_data->gtkPaintClipRgn = m_data->context->m_paintClippingRegion;
    #endif
#endif // __WXMAC__
    }
}

void GraphicsContext::restorePlatformState()
{
    if (m_data->context)
    {
#if USE(WXGC)
        wxGraphicsContext* gc = m_data->context->GetGraphicsContext();
        gc->PopState();
#else
    #if __WXMAC__
        CGContextRef context;
        wxGraphicsContext* gc = m_data->context->GetGraphicsContext();
        if (gc)
            context = (CGContextRef)gc->GetNativeContext();
        if (context)
            CGContextRestoreGState(context); 
    #elif __WXMSW__
        HDC dc = (HDC)m_data->context->GetHDC();
        ::RestoreDC(dc, m_data->mswDCStateID);
    #elif __WXGTK__
        m_data->context->m_currentClippingRegion = m_data->gtkCurrentClipRgn;
        m_data->context->m_paintClippingRegion = m_data->gtkPaintClipRgn;
    #endif

#endif // USE_WXGC 
    }
}

// Draws a filled rectangle with a stroked border.
void GraphicsContext::drawRect(const IntRect& rect)
{
    if (paintingDisabled())
        return;

    m_data->context->SetPen(wxPen(strokeColor(), strokeThickness(), strokeStyleToWxPenStyle(strokeStyle())));
    m_data->context->DrawRectangle(rect.x(), rect.y(), rect.width(), rect.height());
}

// This is only used to draw borders.
void GraphicsContext::drawLine(const IntPoint& point1, const IntPoint& point2)
{
    if (paintingDisabled())
        return;

    FloatPoint p1 = point1;
    FloatPoint p2 = point2;
    
    m_data->context->SetPen(wxPen(strokeColor(), strokeThickness(), strokeStyleToWxPenStyle(strokeStyle())));
    m_data->context->DrawLine(point1.x(), point1.y(), point2.x(), point2.y());
}

// This method is only used to draw the little circles used in lists.
void GraphicsContext::drawEllipse(const IntRect& rect)
{
    if (paintingDisabled())
        return;

    m_data->context->SetPen(wxPen(strokeColor(), strokeThickness(), strokeStyleToWxPenStyle(strokeStyle())));
    m_data->context->DrawEllipse(rect.x(), rect.y(), rect.width(), rect.height());
}

void GraphicsContext::strokeArc(const IntRect& rect, int startAngle, int angleSpan)
{
    if (paintingDisabled())
        return;
    
    m_data->context->SetPen(wxPen(strokeColor(), strokeThickness(), strokeStyleToWxPenStyle(strokeStyle())));
    m_data->context->DrawEllipticArc(rect.x(), rect.y(), rect.width(), rect.height(), startAngle, angleSpan);
}

void GraphicsContext::drawConvexPolygon(size_t npoints, const FloatPoint* points, bool shouldAntialias)
{
    if (paintingDisabled())
        return;

    if (npoints <= 1)
        return;

    wxPoint* polygon = new wxPoint[npoints];
    for (size_t i = 0; i < npoints; i++)
        polygon[i] = wxPoint(points[i].x(), points[i].y());
    m_data->context->SetPen(wxPen(strokeColor(), strokeThickness(), strokeStyleToWxPenStyle(strokeStyle())));
    m_data->context->DrawPolygon((int)npoints, polygon);
    delete [] polygon;
}

void GraphicsContext::fillRect(const FloatRect& rect, const Color& color, ColorSpace colorSpace)
{
    if (paintingDisabled())
        return;

    m_data->context->SetPen(*wxTRANSPARENT_PEN);
    m_data->context->SetBrush(wxBrush(color));
    m_data->context->DrawRectangle(rect.x(), rect.y(), rect.width(), rect.height());
}

void GraphicsContext::fillRoundedRect(const IntRect& rect, const IntSize& topLeft, const IntSize& topRight, const IntSize& bottomLeft, const IntSize& bottomRight, const Color& color, ColorSpace colorSpace)
{
    if (paintingDisabled())
        return;
    
    notImplemented();
}

void GraphicsContext::drawFocusRing(const Color& color)
{
    if (paintingDisabled())
        return;

    notImplemented();
}

void GraphicsContext::clip(const FloatRect& r)
{
    wxWindowDC* windc = dynamic_cast<wxWindowDC*>(m_data->context);
    wxPoint pos(0, 0);

    if (windc) {
#if !defined(__WXGTK__) || wxCHECK_VERSION(2,9,0)
        wxWindow* window = windc->GetWindow();
#else
        wxWindow* window = windc->m_owner;
#endif
        if (window) {
            wxWindow* parent = window->GetParent();
            // we need to convert from WebView "global" to WebFrame "local" coords.
            // FIXME: We only want to go to the top WebView.  
            while (parent) {
                pos += window->GetPosition();
                parent = parent->GetParent();
            }
        }
    }

    m_data->context->SetClippingRegion(r.x() - pos.x, r.y() - pos.y, r.width() + pos.x, r.height() + pos.y);
}

void GraphicsContext::clipOut(const Path&)
{
    notImplemented();
}

void GraphicsContext::clipOut(const IntRect&)
{
    notImplemented();
}

void GraphicsContext::clipOutEllipseInRect(const IntRect&)
{
    notImplemented();
}

void GraphicsContext::drawLineForText(const IntPoint& origin, int width, bool printing)
{
    if (paintingDisabled())
        return;

    IntPoint endPoint = origin + IntSize(width, 0);
    m_data->context->SetPen(wxPen(strokeColor(), strokeThickness(), wxSOLID));
    m_data->context->DrawLine(origin.x(), origin.y(), endPoint.x(), endPoint.y());
}


void GraphicsContext::drawLineForMisspellingOrBadGrammar(const IntPoint& origin, int width, bool grammar)
{
    if (grammar)
        m_data->context->SetPen(wxPen(*wxGREEN, 2, wxLONG_DASH));
    else
        m_data->context->SetPen(wxPen(*wxRED, 2, wxLONG_DASH));
    
    m_data->context->DrawLine(origin.x(), origin.y(), origin.x() + width, origin.y());
}

void GraphicsContext::clip(const Path&) 
{ 
    notImplemented();
}

void GraphicsContext::canvasClip(const Path& path)
{
    clip(path);
}

void GraphicsContext::clipToImageBuffer(const FloatRect&, const ImageBuffer*)
{
    notImplemented();
}

TransformationMatrix GraphicsContext::getCTM() const
{ 
    notImplemented();
    return TransformationMatrix();
}

void GraphicsContext::translate(float tx, float ty) 
{ 
#if USE(WXGC)
    if (m_data->context) {
        wxGraphicsContext* gc = m_data->context->GetGraphicsContext();
        gc->Translate(tx, ty);
    }
#endif
}

void GraphicsContext::rotate(float angle) 
{ 
#if USE(WXGC)
    if (m_data->context) {
        wxGraphicsContext* gc = m_data->context->GetGraphicsContext();
        gc->Rotate(angle);
    }
#endif
}

void GraphicsContext::scale(const FloatSize& scale) 
{ 
#if USE(WXGC)
    if (m_data->context) {
        wxGraphicsContext* gc = m_data->context->GetGraphicsContext();
        gc->Scale(scale.width(), scale.height());
    }
#endif
}


FloatRect GraphicsContext::roundToDevicePixels(const FloatRect& frect)
{
    FloatRect result;

    wxCoord x = (wxCoord)frect.x();
    wxCoord y = (wxCoord)frect.y();

    x = m_data->context->LogicalToDeviceX(x);
    y = m_data->context->LogicalToDeviceY(y);
    result.setX((float)x);
    result.setY((float)y);
    x = (wxCoord)frect.width();
    y = (wxCoord)frect.height();
    x = m_data->context->LogicalToDeviceXRel(x);
    y = m_data->context->LogicalToDeviceYRel(y);
    result.setWidth((float)x);
    result.setHeight((float)y);
    return result; 
}

void GraphicsContext::setURLForRect(const KURL&, const IntRect&)
{
    notImplemented();
}

void GraphicsContext::setCompositeOperation(CompositeOperator op)
{
    if (m_data->context)
    {
#if wxCHECK_VERSION(2,9,0)
        m_data->context->SetLogicalFunction(static_cast<wxRasterOperationMode>(getWxCompositingOperation(op, false)));
#else
        m_data->context->SetLogicalFunction(getWxCompositingOperation(op, false));
#endif
    }
}

void GraphicsContext::beginPath()
{
    notImplemented();
}

void GraphicsContext::addPath(const Path& path)
{
    notImplemented();
}

void GraphicsContext::setPlatformStrokeColor(const Color& color, ColorSpace colorSpace)
{
    if (paintingDisabled())
        return;

    if (m_data->context)
        m_data->context->SetPen(wxPen(color, strokeThickness(), strokeStyleToWxPenStyle(strokeStyle())));
}

void GraphicsContext::setPlatformStrokeThickness(float thickness)
{
    if (paintingDisabled())
        return;
    
    if (m_data->context)
        m_data->context->SetPen(wxPen(strokeColor(), thickness, strokeStyleToWxPenStyle(strokeStyle())));

}

void GraphicsContext::setPlatformFillColor(const Color& color, ColorSpace colorSpace)
{
    if (paintingDisabled())
        return;
    
    if (m_data->context)
        m_data->context->SetBrush(wxBrush(color));
}

void GraphicsContext::concatCTM(const TransformationMatrix& transform)
{
    if (paintingDisabled())
        return;

    notImplemented();
    return;
}

void GraphicsContext::setPlatformShouldAntialias(bool enable)
{
    if (paintingDisabled())
        return;
    notImplemented();
}

void GraphicsContext::setImageInterpolationQuality(InterpolationQuality)
{
}

InterpolationQuality GraphicsContext::imageInterpolationQuality() const
{
    return InterpolationDefault;
}

void GraphicsContext::fillPath()
{
}

void GraphicsContext::strokePath()
{
}

void GraphicsContext::drawPath()
{
    fillPath();
    strokePath();
}

void GraphicsContext::fillRect(const FloatRect& rect)
{
    if (paintingDisabled())
        return;
}

void GraphicsContext::setPlatformShadow(IntSize const&,int,Color const&, ColorSpace) 
{ 
    notImplemented(); 
}

void GraphicsContext::clearPlatformShadow() 
{ 
    notImplemented(); 
}

void GraphicsContext::beginTransparencyLayer(float) 
{ 
    notImplemented(); 
}

void GraphicsContext::endTransparencyLayer() 
{ 
    notImplemented(); 
}

void GraphicsContext::clearRect(const FloatRect&) 
{ 
    notImplemented(); 
}

void GraphicsContext::strokeRect(const FloatRect&, float)
{ 
    notImplemented(); 
}

void GraphicsContext::setLineCap(LineCap) 
{
    notImplemented(); 
}

void GraphicsContext::setLineJoin(LineJoin)
{
    notImplemented();
}

void GraphicsContext::setMiterLimit(float)
{
    notImplemented();
}

void GraphicsContext::setAlpha(float)
{
    notImplemented();
}

void GraphicsContext::addInnerRoundedRectClip(const IntRect& rect, int thickness)
{
    notImplemented();
}

}
