/*
 * Copyright (C) 2007, 2008, 2009 Apple Inc. All rights reserved.
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
#include "JSCSSStyleDeclarationCustom.h"

#include "AtomicString.h"
#include "CSSMutableStyleDeclaration.h"
#include "CSSPrimitiveValue.h"
#include "CSSValue.h"
#include "PlatformString.h"
#include <runtime/StringObjectThatMasqueradesAsUndefined.h>
#include <runtime/StringPrototype.h>
#include <wtf/ASCIICType.h>

using namespace JSC;
using namespace WTF;

namespace WebCore {

void JSCSSStyleDeclaration::markChildren(MarkStack& markStack)
{
    Base::markChildren(markStack);

    CSSStyleDeclaration* declaration = impl();
    JSGlobalData& globalData = *Heap::heap(this)->globalData();

    if (declaration->isMutableStyleDeclaration()) {
        CSSMutableStyleDeclaration* mutableDeclaration = static_cast<CSSMutableStyleDeclaration*>(declaration);
        CSSMutableStyleDeclaration::const_iterator end = mutableDeclaration->end();
        for (CSSMutableStyleDeclaration::const_iterator it = mutableDeclaration->begin(); it != end; ++it)
            markDOMObjectWrapper(markStack, globalData, it->value());
    }
}

// Check for a CSS prefix.
// Passed prefix is all lowercase.
// First character of the prefix within the property name may be upper or lowercase.
// Other characters in the prefix within the property name must be lowercase.
// The prefix within the property name must be followed by a capital letter.
static bool hasCSSPropertyNamePrefix(const Identifier& propertyName, const char* prefix)
{
#ifndef NDEBUG
    ASSERT(*prefix);
    for (const char* p = prefix; *p; ++p)
        ASSERT(isASCIILower(*p));
    ASSERT(propertyName.size());
#endif

    if (toASCIILower(propertyName.data()[0]) != prefix[0])
        return false;

    unsigned length = propertyName.size();
    for (unsigned i = 1; i < length; ++i) {
        if (!prefix[i])
            return isASCIIUpper(propertyName.data()[i]);
        if (propertyName.data()[i] != prefix[i])
            return false;
    }
    return false;
}

static String cssPropertyName(const Identifier& propertyName, bool* hadPixelOrPosPrefix = 0)
{
    if (hadPixelOrPosPrefix)
        *hadPixelOrPosPrefix = false;

    unsigned length = propertyName.size();
    if (!length)
        return String();

    Vector<UChar> name;
    name.reserveInitialCapacity(length);

    unsigned i = 0;

    if (hasCSSPropertyNamePrefix(propertyName, "css"))
        i += 3;
    else if (hasCSSPropertyNamePrefix(propertyName, "pixel")) {
        i += 5;
        if (hadPixelOrPosPrefix)
            *hadPixelOrPosPrefix = true;
    } else if (hasCSSPropertyNamePrefix(propertyName, "pos")) {
        i += 3;
        if (hadPixelOrPosPrefix)
            *hadPixelOrPosPrefix = true;
    } else if (hasCSSPropertyNamePrefix(propertyName, "webkit")
            || hasCSSPropertyNamePrefix(propertyName, "khtml")
            || hasCSSPropertyNamePrefix(propertyName, "apple"))
        name.append('-');
    else {
        if (isASCIIUpper(propertyName.data()[0]))
            return String();
    }

    name.append(toASCIILower(propertyName.data()[i++]));

    for (; i < length; ++i) {
        UChar c = propertyName.data()[i];
        if (!isASCIIUpper(c))
            name.append(c);
        else {
            name.append('-');
            name.append(toASCIILower(c));
        }
    }

    return String::adopt(name);
}

static bool isCSSPropertyName(const Identifier& propertyName)
{
    return CSSStyleDeclaration::isPropertyName(cssPropertyName(propertyName));
}

bool JSCSSStyleDeclaration::canGetItemsForName(ExecState*, CSSStyleDeclaration*, const Identifier& propertyName)
{
    return isCSSPropertyName(propertyName);
}

// FIXME: You can get these properties, and set them (see putDelegate below),
// but you should also be able to enumerate them.
JSValue JSCSSStyleDeclaration::nameGetter(ExecState* exec, const Identifier& propertyName, const PropertySlot& slot)
{
    JSCSSStyleDeclaration* thisObj = static_cast<JSCSSStyleDeclaration*>(asObject(slot.slotBase()));

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
            return jsNumber(exec, static_pointer_cast<CSSPrimitiveValue>(v)->getFloatValue(CSSPrimitiveValue::CSS_PX));
        return jsStringOrNull(exec, v->cssText());
    }

    // If the property is a shorthand property (such as "padding"), 
    // it can only be accessed using getPropertyValue.

    // Make the SVG 'filter' attribute undetectable, to avoid confusion with the IE 'filter' attribute.
    if (propertyName == "filter")
        return StringObjectThatMasqueradesAsUndefined::create(exec, thisObj->impl()->getPropertyValue(prop));

    return jsString(exec, thisObj->impl()->getPropertyValue(prop));
}


bool JSCSSStyleDeclaration::putDelegate(ExecState* exec, const Identifier& propertyName, JSValue value, PutPropertySlot&)
{
    if (!isCSSPropertyName(propertyName))
        return false;

    bool pixelOrPos;
    String prop = cssPropertyName(propertyName, &pixelOrPos);
    String propValue = valueToStringWithNullCheck(exec, value);
    if (pixelOrPos)
        propValue += "px";
    ExceptionCode ec = 0;
    impl()->setProperty(prop, propValue, ec);
    setDOMException(exec, ec);
    return true;
}

} // namespace WebCore
