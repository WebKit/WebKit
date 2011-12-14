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
#include "Path.h"

#include "AffineTransform.h"
#include "FloatRect.h"
#include "NotImplemented.h"
#include "PlatformString.h"
#include <Region.h>


namespace WebCore {

Path::Path()
    : m_path(new BRegion())
{
}

Path::~Path()
{
    delete m_path;
}

Path::Path(const Path& other)
    : m_path(new BRegion(*other.platformPath()))
{
}

Path& Path::operator=(const Path& other)
{
    if (&other != this)
        m_path = other.platformPath();

    return *this;
}

bool Path::hasCurrentPoint() const
{
    return !isEmpty();
}

FloatPoint Path::currentPoint() const 
{
    // FIXME: implement safe way to return current point of subpath.
    notImplemented();
    float quietNaN = std::numeric_limits<float>::quiet_NaN();
    return FloatPoint(quietNaN, quietNaN);
}

bool Path::contains(const FloatPoint& point, WindRule rule) const
{
    return m_path->Contains(point);
}

void Path::translate(const FloatSize& size)
{
    notImplemented();
}

FloatRect Path::boundingRect() const
{
    return m_path->Frame();
}

void Path::moveTo(const FloatPoint& point)
{
    // FIXME: Use OffsetBy?
    notImplemented();
}

void Path::addLineTo(const FloatPoint& p)
{
    notImplemented();
}

void Path::addQuadCurveTo(const FloatPoint& cp, const FloatPoint& p)
{
    notImplemented();
}

void Path::addBezierCurveTo(const FloatPoint& cp1, const FloatPoint& cp2, const FloatPoint& p)
{
    notImplemented();
}

void Path::addArcTo(const FloatPoint& p1, const FloatPoint& p2, float radius)
{
    notImplemented();
}

void Path::closeSubpath()
{
    notImplemented();
}

void Path::addArc(const FloatPoint& p, float r, float sar, float ear, bool anticlockwise)
{
    notImplemented();
}

void Path::addRect(const FloatRect& r)
{
    m_path->Include(r);
}

void Path::addEllipse(const FloatRect& r)
{
    notImplemented();
}

void Path::clear()
{
    m_path->MakeEmpty();
}

bool Path::isEmpty() const
{
    return !m_path->Frame().IsValid();
}

void Path::apply(void* info, PathApplierFunction function) const
{
    notImplemented();
}

void Path::transform(const AffineTransform& transform)
{
    notImplemented();
}

FloatRect Path::strokeBoundingRect(StrokeStyleApplier* applier)
{
    notImplemented();
    return FloatRect();
}

} // namespace WebCore

