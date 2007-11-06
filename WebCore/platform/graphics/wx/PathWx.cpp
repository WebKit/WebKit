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
    wxGraphicsRenderer* renderer = wxGraphicsRenderer::GetDefaultRenderer(); 
    if (renderer) {
        wxGraphicsPath path = renderer->CreatePath();
        m_path = new wxGraphicsPath(path);
    }
#endif
}

Path::~Path()
{ 
}

Path::Path(const Path& path)
{ 
    m_path = (PlatformPath*)&path.m_path;
}

bool Path::contains(const FloatPoint& point, const WindRule rule) const
{ 
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

Path& Path::operator=(const Path&)
{ 
    notImplemented(); 
    return*this; 
}

void Path::clear() 
{ 
    if (m_path)
        delete m_path;

#if USE(WXGC)   
    wxGraphicsRenderer* renderer = wxGraphicsRenderer::GetDefaultRenderer(); 
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

void Path::addLineTo(const FloatPoint&) 
{ 
    notImplemented(); 
}

void Path::addQuadCurveTo(const FloatPoint&, const FloatPoint&) 
{ 
    notImplemented(); 
}

void Path::addBezierCurveTo(const FloatPoint&, const FloatPoint&, const FloatPoint&) 
{ 
    notImplemented(); 
}

void Path::addArcTo(const FloatPoint&, const FloatPoint&, float) 
{ 
    notImplemented(); 
}

void Path::closeSubpath() 
{ 
    notImplemented(); 
}

void Path::addArc(const FloatPoint&, float, float, float, bool) 
{ 
    notImplemented(); 
}

void Path::addRect(const FloatRect&) 
{ 
    notImplemented(); 
}

void Path::addEllipse(const FloatRect&) 
{ 
    notImplemented(); 
}

void Path::transform(const AffineTransform&) 
{ 
    notImplemented(); 
}

void Path::apply(void* info, PathApplierFunction function) const 
{ 
    notImplemented(); 
}

}
