/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef WebKitCSSMatrix_h
#define WebKitCSSMatrix_h

#include "StyleBase.h"
#include "PlatformString.h"
#include "TransformationMatrix.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class WebKitCSSMatrix : public StyleBase {
public:
    static PassRefPtr<WebKitCSSMatrix> create()
    {
        return adoptRef(new WebKitCSSMatrix());
    }
    static PassRefPtr<WebKitCSSMatrix> create(const WebKitCSSMatrix& m)
    {
        return adoptRef(new WebKitCSSMatrix(m));
    }
    static PassRefPtr<WebKitCSSMatrix> create(const TransformationMatrix& m)
    {
        return adoptRef(new WebKitCSSMatrix(m));
    }
    static PassRefPtr<WebKitCSSMatrix> create(const String& s, ExceptionCode& ec)
    {
        return adoptRef(new WebKitCSSMatrix(s, ec));
    }
    
    virtual ~WebKitCSSMatrix();

    float a() const { return static_cast<float>(m_matrix.a()); }
    float b() const { return static_cast<float>(m_matrix.b()); }
    float c() const { return static_cast<float>(m_matrix.c()); }
    float d() const { return static_cast<float>(m_matrix.d()); }
    float e() const { return static_cast<float>(m_matrix.e()); }
    float f() const { return static_cast<float>(m_matrix.f()); }
    
    void setA(float f) { m_matrix.setA(f); }
    void setB(float f) { m_matrix.setB(f); }
    void setC(float f) { m_matrix.setC(f); }
    void setD(float f) { m_matrix.setD(f); }
    void setE(float f) { m_matrix.setE(f); }
    void setF(float f) { m_matrix.setF(f); }
 
    void setMatrixValue(const String& string, ExceptionCode&);
    
    // this = this * secondMatrix
    PassRefPtr<WebKitCSSMatrix> multiply(WebKitCSSMatrix* secondMatrix);
    
    // FIXME: we really should have an exception here, for when matrix is not invertible
    PassRefPtr<WebKitCSSMatrix> inverse(ExceptionCode&);
    PassRefPtr<WebKitCSSMatrix> translate(float x, float y);
    PassRefPtr<WebKitCSSMatrix> scale(float scaleX, float scaleY);
    PassRefPtr<WebKitCSSMatrix> rotate(float rot);
    
    const TransformationMatrix& transform() { return m_matrix; }
    
    String toString();
    
protected:
    WebKitCSSMatrix();
    WebKitCSSMatrix(const WebKitCSSMatrix&);
    WebKitCSSMatrix(const TransformationMatrix&);
    WebKitCSSMatrix(const String&, ExceptionCode& );

    TransformationMatrix m_matrix;
};

} // namespace WebCore

#endif // WebKitCSSMatrix_h
