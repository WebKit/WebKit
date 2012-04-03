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

#include "AffineTransform.h"
#include "FloatRect.h"
#include "Font.h"
#include "IntRect.h"
#include "NotImplemented.h"
#include "Path.h"
#include <wtf/MathExtras.h>

#include <math.h>
#include <stdio.h>

// see http://trac.wxwidgets.org/ticket/11482
#ifdef __WXMSW__
#   include "wx/msw/winundef.h"
#endif

#include <wx/defs.h>
#include <wx/window.h>
#include <wx/dcclient.h>
#include <wx/dcgraph.h>
#include <wx/graphics.h>

#if wxUSE_CAIRO
#include <cairo.h>
#endif

#if __WXMAC__
#include <Carbon/Carbon.h>
#elif __WXMSW__

#include "wx/msw/private.h"
// TODO remove this dependency (gdiplus needs the macros)

#undef max
#define max(a, b)            (((a) > (b)) ? (a) : (b))

#undef min
#define min(a, b)            (((a) < (b)) ? (a) : (b))

#include <windows.h>

#include <gdiplus.h>
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
    FloatSize currentScale;
    wxRegion gtkCurrentClipRgn;
    wxRegion gtkPaintClipRgn;
};

GraphicsContextPlatformPrivate::GraphicsContextPlatformPrivate() :
    context(0),
    mswDCStateID(0),
    gtkCurrentClipRgn(wxRegion()),
    gtkPaintClipRgn(wxRegion()),
    currentScale(1.0, 1.0)
{
}

GraphicsContextPlatformPrivate::~GraphicsContextPlatformPrivate()
{
}


void GraphicsContext::platformInit(PlatformGraphicsContext* context)
{
    m_data = new GraphicsContextPlatformPrivate;
    setPaintingDisabled(!context);

    if (context) {
        // Make sure the context starts in sync with our state.
        setPlatformFillColor(fillColor(), ColorSpaceDeviceRGB);
        setPlatformStrokeColor(strokeColor(), ColorSpaceDeviceRGB);
    }
#if USE(WXGC)
    m_data->context = (wxGCDC*)context;
#else
    m_data->context = (wxWindowDC*)context;
#endif
}

void GraphicsContext::platformDestroy()
{
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

    save();
    m_data->context->SetPen(wxPen(strokeColor(), strokeThickness(), strokeStyleToWxPenStyle(strokeStyle())));
    m_data->context->DrawRectangle(rect.x(), rect.y(), rect.width(), rect.height());
    restore();
}

// This is only used to draw borders.
void GraphicsContext::drawLine(const IntPoint& point1, const IntPoint& point2)
{
    if (paintingDisabled())
        return;

    FloatPoint p1 = point1;
    FloatPoint p2 = point2;
    
    save();
    m_data->context->SetPen(wxPen(strokeColor(), strokeThickness(), strokeStyleToWxPenStyle(strokeStyle())));
    m_data->context->DrawLine(point1.x(), point1.y(), point2.x(), point2.y());
    restore();
}

// This method is only used to draw the little circles used in lists.
void GraphicsContext::drawEllipse(const IntRect& rect)
{
    if (paintingDisabled())
        return;

    save();
    m_data->context->SetPen(wxPen(strokeColor(), strokeThickness(), strokeStyleToWxPenStyle(strokeStyle())));
    m_data->context->DrawEllipse(rect.x(), rect.y(), rect.width(), rect.height());
    restore();
}

void GraphicsContext::strokeArc(const IntRect& rect, int startAngle, int angleSpan)
{
    if (paintingDisabled())
        return;
    
    save();
    m_data->context->SetPen(wxPen(strokeColor(), strokeThickness(), strokeStyleToWxPenStyle(strokeStyle())));
    m_data->context->DrawEllipticArc(rect.x(), rect.y(), rect.width(), rect.height(), startAngle, startAngle + angleSpan);
    restore();
}

void GraphicsContext::drawConvexPolygon(size_t npoints, const FloatPoint* points, bool shouldAntialias)
{
    if (paintingDisabled())
        return;

    if (npoints <= 1)
        return;

    save();
    wxPoint* polygon = new wxPoint[npoints];
    for (size_t i = 0; i < npoints; i++)
        polygon[i] = wxPoint(points[i].x(), points[i].y());
    m_data->context->SetPen(wxPen(strokeColor(), strokeThickness(), strokeStyleToWxPenStyle(strokeStyle())));
    m_data->context->DrawPolygon((int)npoints, polygon);
    delete [] polygon;
    restore();
}

void GraphicsContext::clipConvexPolygon(size_t numPoints, const FloatPoint* points, bool antialiased)
{
    if (paintingDisabled())
        return;

    if (numPoints <= 1)
        return;

    notImplemented();
}

void GraphicsContext::fillRect(const FloatRect& rect, const Color& color, ColorSpace colorSpace)
{
    if (paintingDisabled())
        return;

    save();

    m_data->context->SetPen(*wxTRANSPARENT_PEN);
    m_data->context->SetBrush(wxBrush(color));
    m_data->context->DrawRectangle(rect.x(), rect.y(), rect.width(), rect.height());

    restore();
}

void GraphicsContext::fillRoundedRect(const IntRect& rect, const IntSize& topLeft, const IntSize& topRight, const IntSize& bottomLeft, const IntSize& bottomRight, const Color& color, ColorSpace colorSpace)
{
    if (paintingDisabled())
        return;
    
#if USE(WXGC)
    Path path;
    path.addRoundedRect(rect, topLeft, topRight, bottomLeft, bottomRight);
    m_data->context->SetBrush(wxBrush(color));
    wxGraphicsContext* gc = m_data->context->GetGraphicsContext();
    gc->FillPath(*path.platformPath());
#endif
}

void GraphicsContext::drawFocusRing(const Path& path, int width, int offset, const Color& color)
{
    // FIXME: implement
    notImplemented();
}

void GraphicsContext::drawFocusRing(const Vector<IntRect>& rects, int width, int offset, const Color& color)
{
    if (paintingDisabled())
        return;

    notImplemented();
}

void GraphicsContext::clip(const FloatRect& r)
{
    m_data->context->SetClippingRegion(r.x(), r.y(), r.width(), r.height());
}

void GraphicsContext::clipOut(const Path&)
{
    if (paintingDisabled())
        return;

    notImplemented();
}

void GraphicsContext::clipOut(const IntRect& rect)
{
    if (paintingDisabled())
        return;

#if USE(WXGC)
    wxGraphicsContext* gc = m_data->context->GetGraphicsContext();

#if wxUSE_CAIRO
    double x1, y1, x2, y2;
    cairo_t* cr = (cairo_t*)gc->GetNativeContext();
    cairo_clip_extents(cr, &x1, &y1, &x2, &y2);
    cairo_rectangle(cr, x1, y1, x2 - x1, y2 - y1);
    cairo_rectangle(cr, rect.x(), rect.y(), rect.width(), rect.height());
    cairo_fill_rule_t savedFillRule = cairo_get_fill_rule(cr);
    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
    cairo_clip(cr);
    cairo_set_fill_rule(cr, savedFillRule);

#elif __WXMAC__
    CGContextRef context = (CGContextRef)gc->GetNativeContext();

    CGRect rects[2] = { CGContextGetClipBoundingBox(context), CGRectMake(rect.x(), rect.y(), rect.width(), rect.height()) };
    CGContextBeginPath(context);
    CGContextAddRects(context, rects, 2);
    CGContextEOClip(context);
    return;

#elif __WXMSW__
    Gdiplus::Graphics* g = (Gdiplus::Graphics*)gc->GetNativeContext();
    Gdiplus::Region excludeRegion(Gdiplus::Rect(rect.x(), rect.y(), rect.width(), rect.height()));
    g->ExcludeClip(&excludeRegion);
    return; 
#endif

#endif // USE(WXGC)

    notImplemented();
}

void GraphicsContext::clipPath(const Path& path, WindRule clipRule)
{
    if (paintingDisabled())
        return;
        
    if (path.isEmpty())
        return; 
    
    wxGraphicsContext* gc = m_data->context->GetGraphicsContext();

#if wxUSE_CAIRO
    cairo_t* cr = (cairo_t*)gc->GetNativeContext();
    cairo_path_t* nativePath = (cairo_path_t*)path.platformPath()->GetNativePath();

    cairo_new_path(cr);
    cairo_append_path(cr, nativePath);

    cairo_set_fill_rule(cr, clipRule == RULE_EVENODD ? CAIRO_FILL_RULE_EVEN_ODD : CAIRO_FILL_RULE_WINDING);
    cairo_clip(cr);
#elif __WXMAC__
    CGContextRef context = (CGContextRef)gc->GetNativeContext();   
    CGPathRef nativePath = (CGPathRef)path.platformPath()->GetNativePath(); 
    
    CGContextBeginPath(context);
    CGContextAddPath(context, nativePath);
    if (clipRule == RULE_EVENODD)
        CGContextEOClip(context);
    else
        CGContextClip(context);
#elif __WXMSW__
    Gdiplus::Graphics* g = (Gdiplus::Graphics*)gc->GetNativeContext();
    Gdiplus::GraphicsPath* nativePath = (Gdiplus::GraphicsPath*)path.platformPath()->GetNativePath();
    if (clipRule == RULE_EVENODD)
        nativePath->SetFillMode(Gdiplus::FillModeAlternate);
    else
        nativePath->SetFillMode(Gdiplus::FillModeWinding);
    g->SetClip(nativePath);
#endif
}

void GraphicsContext::drawLineForText(const FloatPoint& origin, float width, bool printing)
{
    if (paintingDisabled())
        return;

    save();
    FloatPoint endPoint = origin + FloatSize(width, 0);
    m_data->context->SetPen(wxPen(strokeColor(), strokeThickness(), wxSOLID));
    m_data->context->DrawLine(origin.x(), origin.y(), endPoint.x(), endPoint.y());
    restore();
}

void GraphicsContext::drawLineForDocumentMarker(const FloatPoint& origin, float width, DocumentMarkerLineStyle style)
{
    switch (style) {
    case DocumentMarkerSpellingLineStyle:
        m_data->context->SetPen(wxPen(*wxRED, 2, wxLONG_DASH));
        break;
    case DocumentMarkerGrammarLineStyle:
        m_data->context->SetPen(wxPen(*wxGREEN, 2, wxLONG_DASH));
        break;
    default:
        return;
    }
    m_data->context->DrawLine(origin.x(), origin.y(), origin.x() + width, origin.y());
}

void GraphicsContext::clip(const Path& path) 
{ 
    if (paintingDisabled())
        return;

    // if the path is empty, we clip against a zero rect to reduce the clipping region to
    // nothing - which is the intended behavior of clip() if the path is empty.
    if (path.isEmpty())
        m_data->context->SetClippingRegion(0, 0, 0, 0);
    else
        clipPath(path, RULE_NONZERO);
}

void GraphicsContext::canvasClip(const Path& path)
{
    clip(path);
}

AffineTransform GraphicsContext::getCTM() const
{ 
    if (paintingDisabled())
        return AffineTransform();

#if USE(WXGC)
    wxGraphicsContext* gc = m_data->context->GetGraphicsContext();
    if (gc) {
        wxGraphicsMatrix matrix = gc->GetTransform();
        double a, b, c, d, e, f;
        matrix.Get(&a, &b, &c, &d, &e, &f);
        return AffineTransform(a, b, c, d, e, f);
    }
#endif
    return AffineTransform();
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
        m_data->currentScale = scale;
    }
#endif
}

FloatSize GraphicsContext::currentScale()
{
    return m_data->currentScale;
}
FloatRect GraphicsContext::roundToDevicePixels(const FloatRect& frect, RoundingMode)
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

void GraphicsContext::setPlatformCompositeOperation(CompositeOperator op)
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

void GraphicsContext::concatCTM(const AffineTransform& transform)
{
    if (paintingDisabled())
        return;

#if USE(WXGC)
    wxGraphicsContext* gc = m_data->context->GetGraphicsContext();
    if (gc)
        gc->ConcatTransform(transform);
#endif
    return;
}

void GraphicsContext::setCTM(const AffineTransform& transform)
{
    if (paintingDisabled())
        return;

#if USE(WXGC)
    wxGraphicsContext* gc = m_data->context->GetGraphicsContext();
    if (gc)
        gc->SetTransform(transform);
#endif
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

void GraphicsContext::fillPath(const Path& path)
{
#if USE(WXGC)
    wxGraphicsContext* gc = m_data->context->GetGraphicsContext();
    if (gc)
        gc->FillPath(*path.platformPath());
#endif
}

void GraphicsContext::strokePath(const Path& path)
{
#if USE(WXGC)
    wxGraphicsContext* gc = m_data->context->GetGraphicsContext();
    if (gc)
        gc->StrokePath(*path.platformPath());
#endif
}

void GraphicsContext::fillRect(const FloatRect& rect)
{
    if (paintingDisabled())
        return;
}

void GraphicsContext::setPlatformShadow(FloatSize const&, float, Color const&, ColorSpace)
{ 
    notImplemented(); 
}

void GraphicsContext::clearPlatformShadow() 
{ 
    notImplemented(); 
}

void GraphicsContext::beginPlatformTransparencyLayer(float)
{ 
    notImplemented(); 
}

void GraphicsContext::endPlatformTransparencyLayer()
{ 
    notImplemented(); 
}

bool GraphicsContext::supportsTransparencyLayers()
{
    return false;
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

void GraphicsContext::setLineDash(const DashArray&, float dashOffset)
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

void GraphicsContext::addInnerRoundedRectClip(const IntRect& r, int thickness)
{
    if (paintingDisabled())
        return;

    FloatRect rect(r);
    clip(rect);
    Path path;
    path.addEllipse(rect);
    rect.inflate(-thickness);
    path.addEllipse(rect);
    clipPath(path, RULE_EVENODD);
}

#if OS(WINDOWS)
HDC GraphicsContext::getWindowsContext(const IntRect& dstRect, bool supportAlphaBlend, bool mayCreateBitmap)
{
    if (dstRect.isEmpty())
        return 0;

    // Create a bitmap DC in which to draw.
    BITMAPINFO bitmapInfo;
    bitmapInfo.bmiHeader.biSize          = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth         = dstRect.width();
    bitmapInfo.bmiHeader.biHeight        = dstRect.height();
    bitmapInfo.bmiHeader.biPlanes        = 1;
    bitmapInfo.bmiHeader.biBitCount      = 32;
    bitmapInfo.bmiHeader.biCompression   = BI_RGB;
    bitmapInfo.bmiHeader.biSizeImage     = 0;
    bitmapInfo.bmiHeader.biXPelsPerMeter = 0;
    bitmapInfo.bmiHeader.biYPelsPerMeter = 0;
    bitmapInfo.bmiHeader.biClrUsed       = 0;
    bitmapInfo.bmiHeader.biClrImportant  = 0;

    void* pixels = 0;
    HBITMAP bitmap = ::CreateDIBSection(0, &bitmapInfo, DIB_RGB_COLORS, &pixels, 0, 0);
    if (!bitmap)
        return 0;

    HDC displayDC = ::GetDC(0);
    HDC bitmapDC = ::CreateCompatibleDC(displayDC);
    ::ReleaseDC(0, displayDC);

    ::SelectObject(bitmapDC, bitmap);

    // Fill our buffer with clear if we're going to alpha blend.
    if (supportAlphaBlend) {
        BITMAP bmpInfo;
        GetObject(bitmap, sizeof(bmpInfo), &bmpInfo);
        int bufferSize = bmpInfo.bmWidthBytes * bmpInfo.bmHeight;
        memset(bmpInfo.bmBits, 0, bufferSize);
    }
    return bitmapDC;
}

void GraphicsContext::releaseWindowsContext(HDC hdc, const IntRect& dstRect, bool supportAlphaBlend, bool mayCreateBitmap)
{
    if (hdc) {

        if (!dstRect.isEmpty()) {

            HBITMAP bitmap = static_cast<HBITMAP>(GetCurrentObject(hdc, OBJ_BITMAP));
            BITMAP info;
            GetObject(bitmap, sizeof(info), &info);
            ASSERT(info.bmBitsPixel == 32);

            wxBitmap bmp;
            bmp.SetHBITMAP(bitmap);
#if !wxCHECK_VERSION(2,9,0)
            if (supportAlphaBlend)
                bmp.UseAlpha();
#endif
            m_data->context->DrawBitmap(bmp, dstRect.x(), dstRect.y(), supportAlphaBlend);

            ::DeleteObject(bitmap);
        }

        ::DeleteDC(hdc);
    }
}
#endif

}
