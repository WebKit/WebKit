/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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
#if SVG_SUPPORT
#include "KCanvasPathQuartz.h"

KCanvasPathQuartz::KCanvasPathQuartz()
{
    m_cgPath = CGPathCreateMutable();
}

KCanvasPathQuartz::~KCanvasPathQuartz()
{
    CGPathRelease(m_cgPath);
}

bool KCanvasPathQuartz::isEmpty() const
{
    return CGPathIsEmpty(m_cgPath);
}

void KCanvasPathQuartz::moveTo(float x, float y)
{
    CGPathMoveToPoint(m_cgPath, 0, x, y);
}

void KCanvasPathQuartz::lineTo(float x, float y)
{
    CGPathAddLineToPoint(m_cgPath, 0, x, y);
}

void KCanvasPathQuartz::curveTo(float x1, float y1, float x2, float y2, float x3, float y3)
{
    CGPathAddCurveToPoint(m_cgPath, 0, x1, y1, x2, y2, x3, y3);
}

void KCanvasPathQuartz::closeSubpath()
{
    CGPathCloseSubpath(m_cgPath);
}
#endif // SVG_SUPPORT

