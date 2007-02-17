/*
 * Copyright (C) 2007 Krzysztof Kowalczyk <kkowalczyk@gmail.com>
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

#include "FloatRect.h"

#include <stdio.h>

#define notImplemented() do { fprintf(stderr, "FIXME: UNIMPLEMENTED %s %s:%d\n", __PRETTY_FUNCTION__, __FILE__, __LINE__); } while(0)

namespace WebCore {

Path::Path()
{
    notImplemented();
}

Path::~Path()
{
    notImplemented();
}

Path::Path(const Path&)
{ 
    notImplemented();
}

bool Path::contains(const FloatPoint&, WindRule rule) const
{ 
    notImplemented();
    return false; 
}

void Path::translate(const FloatSize&)
{
    notImplemented();
}

FloatRect Path::boundingRect() const 
{ 
    notImplemented();
    return FloatRect(); 
}

Path& Path::operator=(const Path&)
{ 
    notImplemented();
    return * this; 
}

void Path::clear() 
{ 
    notImplemented();
}

void Path::moveTo(const FloatPoint&) 
{ 
    notImplemented();
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

void Path::apply(void* , PathApplierFunction) const 
{
    notImplemented();
}

}
