/*
 * Copyright (C) 2003, 2006 Apple Computer, Inc.  All rights reserved.
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

#include "IntPointArray.h"
#include "IntRect.h"
#include <ApplicationServices/ApplicationServices.h>

namespace WebCore {

Path::Path()
    : m_path(0)
{
}

Path::Path(const IntRect& r, Type t)
{
    CGMutablePathRef path = CGPathCreateMutable();
    if (t == Rectangle)
        CGPathAddRect(path, 0, r);
    else // Ellipse
        CGPathAddEllipseInRect(path, 0, r);
    m_path = path;
}

Path::Path(const IntPointArray& arr)
{
    CGMutablePathRef path = CGPathCreateMutable();
    CGPathMoveToPoint(path, 0, arr[0].x(), arr[0].y());
    for (uint i = 1; i < arr.count(); ++i)
        CGPathAddLineToPoint(path, 0, arr[i].x(), arr[i].y());
    CGPathCloseSubpath(path);
    m_path = path;
}

Path::~Path()
{
    CGPathRelease(m_path);
}

Path::Path(const Path& other)
    : m_path(CGPathCreateCopy(other.m_path))
{
}

Path& Path::operator=(const Path& other)
{
    CGPathRef path = CGPathCreateCopy(other.m_path);
    CGPathRelease(m_path);
    m_path = path;
    return *this;
}

bool Path::contains(const IntPoint& point) const
{
    return CGPathContainsPoint(m_path, 0, point, false);
}

void Path::translate(int deltaX, int deltaY)
{
    CGAffineTransform translation = CGAffineTransformMake(1, 0, 0, 1, deltaX, deltaY);
    CGMutablePathRef newPath = CGPathCreateMutable();
    CGPathAddPath(newPath, &translation, m_path);
    CGPathRelease(m_path);
    m_path = newPath;
}

IntRect Path::boundingRect() const
{
    return m_path ? enclosingIntRect(CGPathGetBoundingBox(m_path)) : IntRect();
}

}
