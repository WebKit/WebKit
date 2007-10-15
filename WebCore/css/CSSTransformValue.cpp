/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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
#include "CSSTransformValue.h"
#include "PlatformString.h"

namespace WebCore {

CSSTransformValue::CSSTransformValue(TransformOperationType op)
: m_type(op)
{}

CSSTransformValue::~CSSTransformValue()
{}

void CSSTransformValue::addValue(CSSValue* val)
{
    if (!m_values)
        m_values = new CSSValueList;
    m_values->append(val);
}

String CSSTransformValue::cssText() const
{
    String result;
    switch(m_type) {
        case ScaleTransformOperation:
            result += "scale(";
            break;
        case ScaleXTransformOperation:
            result += "scaleX(";
            break;
        case ScaleYTransformOperation:
            result += "scaleY(";
            break;
        case RotateTransformOperation:
            result += "rotate(";
            break;
        case SkewTransformOperation:
            result += "skew(";
            break;
        case SkewXTransformOperation:
            result += "skewX(";
            break;
        case SkewYTransformOperation:
            result += "skewY(";
            break;
        case TranslateTransformOperation:
            result += "translate(";
            break;
        case TranslateXTransformOperation:
            result += "translateX(";
            break;
        case TranslateYTransformOperation:
            result += "translateY(";
            break;
        case MatrixTransformOperation:
            result += "matrix(";
            break;
        default:
            break;
    }
    
    if (m_values)
        result += m_values->cssText();
    
    result += ")";
    return result;
}

}
