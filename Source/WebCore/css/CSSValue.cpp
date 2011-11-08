/*
 * Copyright (C) 2011 Andreas Kling (kling@webkit.org)
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
 *
 */

#include "config.h"
#include "CSSValue.h"

#include "CSSBorderImageValue.h"
#include "CSSFontFaceSrcValue.h"
#include "CSSPrimitiveValue.h"
#include "CSSReflectValue.h"
#include "CSSValueList.h"

namespace WebCore {

CSSValue::Type CSSValue::cssValueType() const
{
    if (isInheritedValue())
        return CSS_INHERIT;
    if (isPrimitiveValue())
        return CSS_PRIMITIVE_VALUE;
    if (isValueList())
        return CSS_VALUE_LIST;
    if (isInitialValue())
        return CSS_INITIAL;
    return CSS_CUSTOM;
}

void CSSValue::addSubresourceStyleURLs(ListHashSet<KURL>& urls, const CSSStyleSheet* styleSheet)
{
    if (isPrimitiveValue())
        static_cast<CSSPrimitiveValue*>(this)->addSubresourceStyleURLs(urls, styleSheet);
    else if (isValueList())
        static_cast<CSSValueList*>(this)->addSubresourceStyleURLs(urls, styleSheet);
    else if (classType() == BorderImageClass)
        static_cast<CSSBorderImageValue*>(this)->addSubresourceStyleURLs(urls, styleSheet);
    else if (classType() == FontFaceSrcClass)
        static_cast<CSSFontFaceSrcValue*>(this)->addSubresourceStyleURLs(urls, styleSheet);
    else if (classType() == ReflectClass)
        static_cast<CSSReflectValue*>(this)->addSubresourceStyleURLs(urls, styleSheet);
}

}
