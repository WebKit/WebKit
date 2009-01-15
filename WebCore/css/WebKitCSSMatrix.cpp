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

#include "config.h"

#include "CSSParser.h"
#include "CSSStyleSelector.h"
#include "CSSMutableStyleDeclaration.h"
#include "CSSPropertyNames.h"
#include "ExceptionCode.h"
#include "RenderStyle.h"
#include "WebKitCSSMatrix.h"

namespace WebCore {

WebKitCSSMatrix::WebKitCSSMatrix()
    : StyleBase(0)
{
}

WebKitCSSMatrix::WebKitCSSMatrix(const WebKitCSSMatrix& m)
    : StyleBase(0)
    , m_matrix(m.m_matrix)
{
}

WebKitCSSMatrix::WebKitCSSMatrix(const TransformationMatrix& m)
    : StyleBase(0)
    , m_matrix(m)
{
}

WebKitCSSMatrix::WebKitCSSMatrix(const String& s, ExceptionCode& ec) 
    : StyleBase(0)
{
    setMatrixValue(s, ec);
}

WebKitCSSMatrix::~WebKitCSSMatrix()
{
}

void WebKitCSSMatrix::setMatrixValue(const String& string, ExceptionCode& ec)
{
    CSSParser p(useStrictParsing());
    RefPtr<CSSMutableStyleDeclaration> styleDeclaration = CSSMutableStyleDeclaration::create();
    if (p.parseValue(styleDeclaration.get(), CSSPropertyWebkitTransform, string, true)) {
        // Convert to TransformOperations. This can fail if a property 
        // requires style (i.e., param uses 'ems' or 'exs')
        PassRefPtr<CSSValue> val =  styleDeclaration->getPropertyCSSValue(CSSPropertyWebkitTransform);
        TransformOperations operations;
        if (!CSSStyleSelector::createTransformOperations(val.get(), 0, operations)) {
            ec = SYNTAX_ERR;
            return;
        }
        
        // Convert transform operations to a TransformationMatrix. This can fail
        // if a param has a percentage ('%')
        TransformationMatrix t;
        for (unsigned i = 0; i < operations.operations().size(); ++i) {
            if (operations.operations()[i].get()->apply(t, IntSize(0, 0))) {
                ec = SYNTAX_ERR;
                return;
            }
        }
        
        // set the matrix
        m_matrix = t;
    } else if (!string.isEmpty()) // There is something there but parsing failed
        ec = SYNTAX_ERR;
}

// This is a multRight (this = this * secondMatrix)
PassRefPtr<WebKitCSSMatrix> WebKitCSSMatrix::multiply(WebKitCSSMatrix* secondMatrix)
{
    TransformationMatrix tmp(m_matrix);
    tmp.multiply(secondMatrix->m_matrix);
    return WebKitCSSMatrix::create(tmp);
}

PassRefPtr<WebKitCSSMatrix> WebKitCSSMatrix::inverse(ExceptionCode& ec)
{
    if (!m_matrix.isInvertible()) {
        ec = NOT_SUPPORTED_ERR;
        return 0;
    }
    
    return WebKitCSSMatrix::create(m_matrix.inverse());
}

PassRefPtr<WebKitCSSMatrix> WebKitCSSMatrix::translate(float x, float y)
{
    if (isnan(x))
        x = 0; 
    if (isnan(y))
        y = 0; 
    return WebKitCSSMatrix::create(m_matrix.translate(x, y));
}

PassRefPtr<WebKitCSSMatrix> WebKitCSSMatrix::scale(float scaleX, float scaleY)
{
    if (isnan(scaleX))
        scaleX = 1; 
    if (isnan(scaleY))
        scaleY = scaleX; 
    return WebKitCSSMatrix::create(m_matrix.scale(scaleX,scaleY));
}

PassRefPtr<WebKitCSSMatrix> WebKitCSSMatrix::rotate(float rot)
{
    if (isnan(rot))
        rot = 0;
    return WebKitCSSMatrix::create(m_matrix.rotate(rot));
}

String WebKitCSSMatrix::toString()
{
    return String::format("matrix(%f, %f, %f, %f, %f, %f)",
                            m_matrix.a(), m_matrix.b(), m_matrix.c(), m_matrix.d(), m_matrix.e(), m_matrix.f());
}

} // namespace WebCore
