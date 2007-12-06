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
#include "JSCSSStyleDeclaration.h"

#include "AtomicString.h"
#include "CSSPrimitiveValue.h"
#include "CSSStyleDeclaration.h"
#include "CSSValue.h"
#include "DeprecatedString.h"
#include "PlatformString.h"
#include <kjs/string_object.h>

namespace WebCore {

using namespace KJS;

static String cssPropertyName(const Identifier& propertyName, bool* hadPixelOrPosPrefix = 0)
{
    DeprecatedString prop = propertyName;

    int i = prop.length();

    if (!i)
        return prop;

    while (--i) {
        ::UChar c = prop[i].unicode();
        if (c >= 'A' && c <= 'Z')
            prop.insert(i, '-');
    }

    prop = prop.lower();

    if (hadPixelOrPosPrefix)
        *hadPixelOrPosPrefix = false;

    if (prop.startsWith("css-"))
        prop = prop.mid(4);
    else if (prop.startsWith("pixel-")) {
        prop = prop.mid(6);
        if (hadPixelOrPosPrefix)
            *hadPixelOrPosPrefix = true;
    } else if (prop.startsWith("pos-")) {
        prop = prop.mid(4);
        if (hadPixelOrPosPrefix)
            *hadPixelOrPosPrefix = true;
    } else if (prop.startsWith("khtml-") || prop.startsWith("apple-") || prop.startsWith("webkit-"))
        prop.insert(0, '-');

    return prop;
}

static bool isCSSPropertyName(const Identifier& propertyName)
{
    return CSSStyleDeclaration::isPropertyName(cssPropertyName(propertyName));
}

bool JSCSSStyleDeclaration::canGetItemsForName(ExecState*, CSSStyleDeclaration*, const Identifier& propertyName)
{
    return isCSSPropertyName(propertyName);
}

JSValue* JSCSSStyleDeclaration::nameGetter(ExecState* exec, JSObject* originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
    JSCSSStyleDeclaration* thisObj = static_cast<JSCSSStyleDeclaration*>(slot.slotBase());

    // Set up pixelOrPos boolean to handle the fact that
    // pixelTop returns "CSS Top" as number value in unit pixels
    // posTop returns "CSS top" as number value in unit pixels _if_ its a
    // positioned element. if it is not a positioned element, return 0
    // from MSIE documentation FIXME: IMPLEMENT THAT (Dirk)
    bool pixelOrPos;
    String prop = cssPropertyName(propertyName, &pixelOrPos);
    RefPtr<CSSValue> v = thisObj->impl()->getPropertyCSSValue(prop);
    if (v) {
        if (pixelOrPos && v->cssValueType() == CSSValue::CSS_PRIMITIVE_VALUE)
            return jsNumber(static_pointer_cast<CSSPrimitiveValue>(v)->getFloatValue(CSSPrimitiveValue::CSS_PX));
        return jsStringOrNull(v->cssText());
    }

    // If the property is a shorthand property (such as "padding"), 
    // it can only be accessed using getPropertyValue.

    // Make the SVG 'filter' attribute undetectable, to avoid confusion with the IE 'filter' attribute.
    if (propertyName == "filter")
        return new StringInstanceThatMasqueradesAsUndefined(exec->lexicalGlobalObject()->stringPrototype(), thisObj->impl()->getPropertyValue(prop));

    return jsString(thisObj->impl()->getPropertyValue(prop));
}


bool JSCSSStyleDeclaration::customPut(ExecState* exec, const Identifier& propertyName, JSValue* value, int /*attr*/)
{
    if (!isCSSPropertyName(propertyName))
        return false;

    DOMExceptionTranslator exception(exec);
    bool pixelOrPos;
    String prop = cssPropertyName(propertyName, &pixelOrPos);
    String propValue = valueToStringWithNullCheck(exec, value);
    if (pixelOrPos)
        propValue += "px";
    impl()->setProperty(prop, propValue, exception);
    return true;
}

} // namespace WebCore
