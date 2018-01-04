/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "DeprecatedCSSOMValue.h"

#include "DeprecatedCSSOMPrimitiveValue.h"
#include "DeprecatedCSSOMValueList.h"

namespace WebCore {

void DeprecatedCSSOMValue::destroy()
{
    switch (classType()) {
    case DeprecatedComplexValueClass: {
        delete downcast<DeprecatedCSSOMComplexValue>(this);
        return;
    }
    case DeprecatedPrimitiveValueClass: {
        delete downcast<DeprecatedCSSOMPrimitiveValue>(this);
        return;
    }
    case DeprecatedValueListClass: {
        delete downcast<DeprecatedCSSOMValueList>(this);
        return;
    }
    }
    ASSERT_NOT_REACHED();
    delete this;
}

unsigned DeprecatedCSSOMValue::cssValueType() const
{
    switch (m_classType) {
    case DeprecatedComplexValueClass:
        return downcast<DeprecatedCSSOMComplexValue>(*this).cssValueType();
    case DeprecatedPrimitiveValueClass:
        return downcast<DeprecatedCSSOMPrimitiveValue>(*this).cssValueType();
    case DeprecatedValueListClass:
        return downcast<DeprecatedCSSOMValueList>(*this).cssValueType();
    }
    ASSERT_NOT_REACHED();
    return CSS_CUSTOM;
}

String DeprecatedCSSOMValue::cssText() const
{
    switch (m_classType) {
    case DeprecatedComplexValueClass:
        return downcast<DeprecatedCSSOMComplexValue>(*this).cssText();
    case DeprecatedPrimitiveValueClass:
        return downcast<DeprecatedCSSOMPrimitiveValue>(*this).cssText();
    case DeprecatedValueListClass:
        return downcast<DeprecatedCSSOMValueList>(*this).cssText();
    }
    ASSERT_NOT_REACHED();
    return "";
}

}
