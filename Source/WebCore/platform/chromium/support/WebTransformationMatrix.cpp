/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include <public/WebTransformationMatrix.h>

#include "TransformationMatrix.h"

using namespace WebCore;

namespace WebKit {

WebTransformationMatrix::WebTransformationMatrix()
    : m_private(new TransformationMatrix)
{
}

WebTransformationMatrix::WebTransformationMatrix(const WebTransformationMatrix& matrix)
    : m_private(new TransformationMatrix(*matrix.m_private.get()))
{
}

WebTransformationMatrix::WebTransformationMatrix(const TransformationMatrix& t)
    : m_private(new TransformationMatrix(t))
{
}

void WebTransformationMatrix::reset()
{
    m_private.reset(0);
}

double WebTransformationMatrix::m11() const
{
    return m_private->m11();
}

double WebTransformationMatrix::m12() const
{
    return m_private->m12();
}

double WebTransformationMatrix::m13() const
{
    return m_private->m13();
}

double WebTransformationMatrix::m14() const
{
    return m_private->m14();
}

double WebTransformationMatrix::m21() const
{
    return m_private->m21();
}

double WebTransformationMatrix::m22() const
{
    return m_private->m22();
}

double WebTransformationMatrix::m23() const
{
    return m_private->m23();
}

double WebTransformationMatrix::m24() const
{
    return m_private->m24();
}

double WebTransformationMatrix::m31() const
{
    return m_private->m31();
}

double WebTransformationMatrix::m32() const
{
    return m_private->m32();
}

double WebTransformationMatrix::m33() const
{
    return m_private->m33();
}

double WebTransformationMatrix::m34() const
{
    return m_private->m34();
}

double WebTransformationMatrix::m41() const
{
    return m_private->m41();
}

double WebTransformationMatrix::m42() const
{
    return m_private->m42();
}

double WebTransformationMatrix::m43() const
{
    return m_private->m43();
}

double WebTransformationMatrix::m44() const
{
    return m_private->m44();
}

} // namespace WebKit
