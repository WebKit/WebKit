/*
 * Copyright (C) 2007 Kevin Ollivier  All rights reserved.
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
#include "Path.h"

#include "AffineTransform.h"
#include "FloatPoint.h"
#include "FloatRect.h"
#include "NotImplemented.h"
#include "PlatformString.h"
#include "StrokeStyleApplier.h" 

#include <stdio.h>

#include <wx/defs.h>
#include <wx/graphics.h>

namespace WebCore {

int getWxWindRuleForWindRule(WindRule rule)
{
     if (rule == RULE_EVENODD)
        return wxODDEVEN_RULE;

    return wxWINDING_RULE;
}

Path::Path() 
{ 
    m_path = 0;
    // NB: This only supports the 'default' renderer as determined by wx on
    // each platform. If an app uses a non-default renderer (e.g. Cairo on Win),  
    // there will be problems, but there's no way we can determine which 
    // renderer an app is using right now with wx API, so we will just handle
    // the common case.
#if USE(WXGC)
    wxGraphicsRenderer* renderer = 0;
#if wxUSE_CAIRO
    renderer = wxGraphicsRenderer::GetCairoRenderer();
#else
    renderer = wxGraphicsRenderer::GetDefaultRenderer();
#endif
    if (renderer) {
        wxGraphicsPath path = renderer->CreatePath();
        m_path = new wxGraphicsPath(path);
    }
#endif
}

Path::~Path()
{
    clear();
}

Path::Path(const Path& path)
{ 
    m_path = new wxGraphicsPath(*path.m_path);
}

bool Path::contains(const FloatPoint& point, const WindRule rule) const
{
#if USE(WXGC)
    if (m_path) {
#if wxCHECK_VERSION(2,9,0)
        return m_path->Contains(point.x(), point.y(), static_cast<wxPolygonFillMode>(getWxWindRuleForWindRule(rule)));
#else
        return m_path->Contains(point.x(), point.y(), getWxWindRuleForWindRule(rule));
#endif
    }
#endif
    return false; 
}

void Path::translate(const FloatSize&)
{ 
    notImplemented();
}

FloatRect Path::boundingRect() const 
{ 
#if USE(WXGC)
    if (m_path) {
        return m_path->GetBox();     
    }
#endif

    return FloatRect();
}

FloatRect Path::strokeBoundingRect(StrokeStyleApplier* applier) const
{
    notImplemented();
    return FloatRect();
}

bool Path::strokeContains(StrokeStyleApplier*, const FloatPoint&) const
{
    notImplemented();
    return false;
}

Path& Path::operator=(const Path& path)
{ 
    *m_path = *path.platformPath();
    return *this; 
}

void Path::clear() 
{ 
    if (m_path)
        delete m_path;

#if USE(WXGC)
#if wxUSE_CAIRO
    wxGraphicsRenderer* renderer = wxGraphicsRenderer::GetCairoRenderer();
#else
    wxGraphicsRenderer* renderer = wxGraphicsRenderer::GetDefaultRenderer();
#endif
    if (renderer) {
        wxGraphicsPath path = renderer->CreatePath();
        m_path = new wxGraphicsPath(path);
    }
#endif
}

void Path::moveTo(const FloatPoint& point) 
{
#if USE(WXGC)
    if (m_path)
        m_path->MoveToPoint(point.x(), point.y());
#endif
}

void Path::addLineTo(const FloatPoint& point) 
{
#if USE(WXGC)
    if (m_path)
        m_path->AddLineToPoint(point.x(), point.y());
#endif
}

void Path::addQuadCurveTo(const FloatPoint& control, const FloatPoint& end) 
{
#if USE(WXGC)
    if (m_path)
        m_path->AddQuadCurveToPoint(control.x(), control.y(), end.x(), end.y());
#endif
}

void Path::addBezierCurveTo(const FloatPoint& control1, const FloatPoint& control2, const FloatPoint& end) 
{
#if USE(WXGC)
    if (m_path)
        m_path->AddCurveToPoint(control1.x(), control1.y(), control2.x(), control2.y(), end.x(), end.y());
#endif
}

void Path::addArcTo(const FloatPoint& point1, const FloatPoint& point2, float radius) 
{
#if USE(WXGC)
    if (m_path)
        m_path->AddArcToPoint(point1.x(), point1.y(), point2.x(), point2.y(), radius);
#endif
}

void Path::closeSubpath() 
{
#if USE(WXGC)
    if (m_path)
        m_path->CloseSubpath();
#endif
}

void Path::addArc(const FloatPoint& point, float radius, float startAngle, float endAngle, bool clockwise) 
{
#if USE(WXGC)
    if (m_path)
        m_path->AddArc(point.x(), point.y(), radius, startAngle, endAngle, clockwise);
#endif
}

void Path::addRect(const FloatRect& rect) 
{
#if USE(WXGC)
    if (m_path)
        m_path->AddRectangle(rect.x(), rect.y(), rect.width(), rect.height());
#endif
}

void Path::addEllipse(const FloatRect& rect) 
{
#if USE(WXGC)
    if (m_path)
        m_path->AddEllipse(rect.x(), rect.y(), rect.width(), rect.height());
#endif
}

void Path::transform(const AffineTransform& transform) 
{
#if USE(WXGC)
    if (m_path)
        m_path->Transform(transform);
#endif
}

void Path::apply(void* info, PathApplierFunction function) const 
{
    notImplemented(); 
}

bool Path::isEmpty() const
{
#if USE(WXGC)
    if (m_path) {
        wxDouble x, y, width, height;
        m_path->GetBox(&x, &y, &width, &height);
        return (width == 0 && height == 0);
    }
#endif
    return true;
}

bool Path::hasCurrentPoint() const
{
    return !isEmpty();
}

FloatPoint Path::currentPoint() const 
{
    // FIXME: return current point of subpath.
    float quietNaN = std::numeric_limits<float>::quiet_NaN();
    return FloatPoint(quietNaN, quietNaN);
}

}
