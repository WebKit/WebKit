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
#include "DeprecatedCSSOMPrimitiveValue.h"

#include "DeprecatedCSSOMCounter.h"
#include "DeprecatedCSSOMRGBColor.h"
#include "DeprecatedCSSOMRect.h"

namespace WebCore {
    
// FIXME: For now these still call into CSSPrimitiveValue, but as we refactor into subclasses
// such as StyleCounterValue, StyleRectValue, and StyleColorValue, these methods will get
// more complicated.


unsigned short DeprecatedCSSOMPrimitiveValue::primitiveType() const
{
    return m_value->primitiveType();
}

ExceptionOr<void> DeprecatedCSSOMPrimitiveValue::setFloatValue(unsigned short unitType, double floatValue)
{
    return m_value->setFloatValue(unitType, floatValue);
}

ExceptionOr<float> DeprecatedCSSOMPrimitiveValue::getFloatValue(unsigned short unitType) const
{
    return m_value->getFloatValue(unitType);
}

ExceptionOr<void> DeprecatedCSSOMPrimitiveValue::setStringValue(unsigned short stringType, const String& stringValue)
{
    return m_value->setStringValue(stringType, stringValue);
}

ExceptionOr<String> DeprecatedCSSOMPrimitiveValue::getStringValue() const
{
    return m_value->getStringValue();
}

ExceptionOr<Ref<DeprecatedCSSOMCounter>> DeprecatedCSSOMPrimitiveValue::getCounterValue() const
{
    ExceptionOr<Counter&> counter = m_value->getCounterValue();
    if (counter.hasException())
        return Exception { InvalidAccessError };
    return DeprecatedCSSOMCounter::create(counter.releaseReturnValue(), m_owner.get());
}
    
ExceptionOr<Ref<DeprecatedCSSOMRect>> DeprecatedCSSOMPrimitiveValue::getRectValue() const
{
    ExceptionOr<Rect&> rect = m_value->getRectValue();
    if (rect.hasException())
        return Exception { InvalidAccessError };
    return DeprecatedCSSOMRect::create(rect.releaseReturnValue(), m_owner.get());
}

ExceptionOr<Ref<DeprecatedCSSOMRGBColor>> DeprecatedCSSOMPrimitiveValue::getRGBColorValue() const
{
    ExceptionOr<Ref<RGBColor>> color = m_value->getRGBColorValue();
    if (color.hasException())
        return Exception { InvalidAccessError };
    return DeprecatedCSSOMRGBColor::create(color.releaseReturnValue(), m_owner.get());
}

}
