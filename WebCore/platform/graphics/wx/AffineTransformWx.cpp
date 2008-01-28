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
#include "AffineTransform.h"

#include "FloatRect.h"
#include "IntRect.h"
#include "NotImplemented.h"

#include <stdio.h>
#include <wx/defs.h>
#include <wx/graphics.h>

namespace WebCore {

#if USE(WXGC)
AffineTransform::AffineTransform(const wxGraphicsMatrix &matrix)
{
    m_transform = matrix;
}
#endif

AffineTransform::AffineTransform(double a, double b, double c, double d, double e, double f)
{
#if USE(WXGC)
    wxGraphicsRenderer* renderer = wxGraphicsRenderer::GetDefaultRenderer(); 
    m_transform = renderer->CreateMatrix();
    m_transform.Set(a, b, c, d, e, f);
#endif
}

AffineTransform::AffineTransform() 
{ 
    // NB: If we ever support using Cairo backend on Win/Mac, this will need to be
    // changed somehow (though I'm not sure how as we don't have a reference to the
    // graphics context here.
#if USE(WXGC)
    wxGraphicsRenderer* renderer = wxGraphicsRenderer::GetDefaultRenderer(); 
    m_transform = renderer->CreateMatrix();
#endif
}

AffineTransform AffineTransform::inverse() const
{
    notImplemented();
    return *this;
}

void AffineTransform::map(double x, double y, double *x2, double *y2) const 
{ 
    notImplemented();
}

IntRect AffineTransform::mapRect(const IntRect &rect) const
{
    notImplemented();
    return IntRect();
}

FloatRect AffineTransform::mapRect(const FloatRect &rect) const
{
    notImplemented();
    return FloatRect();
}


AffineTransform& AffineTransform::scale(double sx, double sy) 
{
#if USE(WXGC)
    m_transform.Scale((wxDouble)sx, (wxDouble)sy); 
#endif
    return *this; 
}

void AffineTransform::reset()
{
    notImplemented();
}

AffineTransform& AffineTransform::rotate(double d) 
{ 
#if USE(WXGC)
    m_transform.Rotate((wxDouble)d); 
#endif
    return *this; 
}

AffineTransform& AffineTransform::translate(double tx, double ty) 
{ 
#if USE(WXGC)
    m_transform.Translate((wxDouble)tx, (wxDouble)ty); 
#endif
    return *this; 
}

AffineTransform& AffineTransform::shear(double sx, double sy) 
{ 
    notImplemented(); 
    return *this; 
}

AffineTransform& AffineTransform::operator*=(const AffineTransform& other) 
{ 
    notImplemented();
    return *this; 
}

bool AffineTransform::operator== (const AffineTransform &other) const
{
#if USE(WXGC)
    return m_transform.IsEqual((wxGraphicsMatrix)other);
#endif
}

AffineTransform AffineTransform::operator* (const AffineTransform &other)
{
    notImplemented();
    return *this; //m_transform * other.m_transform;
}

double AffineTransform::det() const 
{ 
    notImplemented(); 
    return 0;
}

#if USE(WXGC)
AffineTransform::operator wxGraphicsMatrix() const
{
    return m_transform;
}
#endif

}
