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

#include "CSSParser.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyNames.h"
#include "CSSValue.h"
#include "JSCSSValue.h"
#include "JSNode.h"
#include "PlatformString.h"
#include "StylePropertySet.h"
#include <runtime/StringPrototype.h>
#include <wtf/ASCIICType.h>
#include <wtf/text/AtomicString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringConcatenate.h>
#include <wtf/text/WTFString.h>

using namespace JSC;
using namespace WTF;
using namespace std;

namespace WebCore {

void JSCSSStyleDeclaration::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    JSCSSStyleDeclaration* thisObject = jsCast<JSCSSStyleDeclaration*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, &s_info);
    COMPILE_ASSERT(StructureFlags & OverridesVisitChildren, OverridesVisitChildrenWithoutSettingFlag);
    ASSERT(thisObject->structure()->typeInfo().overridesVisitChildren());
    Base::visitChildren(thisObject, visitor);
    visitor.addOpaqueRoot(root(thisObject->impl()));
}

enum PropertyNamePrefix
{
    PropertyNamePrefixNone,
    PropertyNamePrefixCSS,
    PropertyNamePrefixPixel,
    PropertyNamePrefixPos,
    PropertyNamePrefixApple,
    PropertyNamePrefixEpub,
    PropertyNamePrefixKHTML,
    PropertyNamePrefixWebKit
};

template<size_t prefixCStringLength>
static inline bool matchesCSSPropertyNamePrefix(const StringImpl& propertyName, const char (&prefix)[prefixCStringLength])
{
    size_t prefixLength = prefixCStringLength - 1;

    ASSERT(toASCIILower(propertyName[0]) == prefix[0]);
    const size_t offset = 1;

#ifndef NDEBUG
    for (size_t i = 0; i < prefixLength; ++i)
        ASSERT(isASCIILower(prefix[i]));
    ASSERT(!prefix[prefixLength]);
    ASSERT(propertyName.length());
#endif

    // The prefix within the property name must be followed by a capital letter.
    // Other characters in the prefix within the property name must be lowercase.
    if (propertyName.length() < (prefixLength + 1))
        return false;

    for (size_t i = offset; i < prefixLength; ++i) {
        if (propertyName[i] != prefix[i])
            return false;
    }

    if (!isASCIIUpper(propertyName[prefixLength]))
        return false;
    return true;
}

static PropertyNamePrefix getCSSPropertyNamePrefix(const StringImpl& propertyName)
{
    ASSERT(propertyName.length());

    // First character of the prefix within the property name may be upper or lowercase.
    UChar firstChar = toASCIILower(propertyName[0]);
    switch (firstChar) {
    case 'a':
        if (matchesCSSPropertyNamePrefix(propertyName, "apple"))
            return PropertyNamePrefixApple;
        break;
    case 'c':
        if (matchesCSSPropertyNamePrefix(propertyName, "css"))
            return PropertyNamePrefixCSS;
        break;
    case 'k':
        if (matchesCSSPropertyNamePrefix(propertyName, "khtml"))
            return PropertyNamePrefixKHTML;
        break;
    case 'e':
        if (matchesCSSPropertyNamePrefix(propertyName, "epub"))
            return PropertyNamePrefixEpub;
        break;
    case 'p':
        if (matchesCSSPropertyNamePrefix(propertyName, "pos"))
            return PropertyNamePrefixPos;
        if (matchesCSSPropertyNamePrefix(propertyName, "pixel"))
            return PropertyNamePrefixPixel;
        break;
    case 'w':
        if (matchesCSSPropertyNamePrefix(propertyName, "webkit"))
            return PropertyNamePrefixWebKit;
        break;
    default:
        break;
    }
    return PropertyNamePrefixNone;
}

template<typename CharacterType>
static inline bool containsASCIIUpperChar(const CharacterType* string, size_t length)
{
    for (unsigned i = 0; i < length; ++i) {
        if (isASCIIUpper(string[i]))
            return true;
    }
    return false;
}

static inline bool containsASCIIUpperChar(const StringImpl& string)
{
    if (string.is8Bit())
        return containsASCIIUpperChar(string.characters8(), string.length());
    return containsASCIIUpperChar(string.characters16(), string.length());
}

static String cssPropertyName(const Identifier& propertyName, bool* hadPixelOrPosPrefix = 0)
{
    if (hadPixelOrPosPrefix)
        *hadPixelOrPosPrefix = false;

    unsigned length = propertyName.length();
    if (!length)
        return String();

    StringImpl* propertyNameString = propertyName.impl();
    // If there is no uppercase character in the propertyName, there can
    // be no prefix, nor extension and we can return the same string.
    if (!containsASCIIUpperChar(*propertyNameString))
        return String(propertyNameString);

    StringBuilder builder;
    builder.reserveCapacity(length);

    unsigned i = 0;
    switch (getCSSPropertyNamePrefix(*propertyNameString)) {
    case PropertyNamePrefixNone:
        if (isASCIIUpper((*propertyNameString)[0]))
            return String();
        break;
    case PropertyNamePrefixCSS:
        i += 3;
        break;
    case PropertyNamePrefixPixel:
        i += 5;
        if (hadPixelOrPosPrefix)
            *hadPixelOrPosPrefix = true;
        break;
    case PropertyNamePrefixPos:
        i += 3;
        if (hadPixelOrPosPrefix)
            *hadPixelOrPosPrefix = true;
        break;
    case PropertyNamePrefixApple:
    case PropertyNamePrefixEpub:
    case PropertyNamePrefixKHTML:
    case PropertyNamePrefixWebKit:
        builder.append('-');
    }

    builder.append(toASCIILower((*propertyNameString)[i++]));

    for (; i < length; ++i) {
        UChar c = (*propertyNameString)[i];
        if (!isASCIIUpper(c))
            builder.append(c);
        else
            builder.append(makeString('-', toASCIILower(c)));
    }

    return builder.toString();
}

static bool isCSSPropertyName(const Identifier& propertyIdentifier)
{
    return cssPropertyID(cssPropertyName(propertyIdentifier));
}

bool JSCSSStyleDeclaration::canGetItemsForName(ExecState*, CSSStyleDeclaration*, const Identifier& propertyName)
{
    return isCSSPropertyName(propertyName);
}

JSValue JSCSSStyleDeclaration::nameGetter(ExecState* exec, JSValue slotBase, const Identifier& propertyName)
{
    JSCSSStyleDeclaration* thisObj = static_cast<JSCSSStyleDeclaration*>(asObject(slotBase));

    // Set up pixelOrPos boolean to handle the fact that
    // pixelTop returns "CSS Top" as number value in unit pixels
    // posTop returns "CSS top" as number value in unit pixels _if_ its a
    // positioned element. if it is not a positioned element, return 0
    // from MSIE documentation FIXME: IMPLEMENT THAT (Dirk)
    bool pixelOrPos;
    String prop = cssPropertyName(propertyName, &pixelOrPos);
    RefPtr<CSSValue> v = thisObj->impl()->getPropertyCSSValue(prop);
    if (v) {
        if (pixelOrPos && v->isPrimitiveValue())
            return jsNumber(static_pointer_cast<CSSPrimitiveValue>(v)->getFloatValue(CSSPrimitiveValue::CSS_PX));
        return jsStringOrNull(exec, v->cssText());
    }

    // If the property is a shorthand property (such as "padding"), 
    // it can only be accessed using getPropertyValue.

    return jsString(exec, thisObj->impl()->getPropertyValue(prop));
}

bool JSCSSStyleDeclaration::putDelegate(ExecState* exec, const Identifier& propertyName, JSValue value, PutPropertySlot&)
{
    bool pixelOrPos;
    String prop = cssPropertyName(propertyName, &pixelOrPos);
    if (!cssPropertyID(prop))
        return false;

    String propValue = valueToStringWithNullCheck(exec, value);
    if (pixelOrPos)
        propValue += "px";
    ExceptionCode ec = 0;
    impl()->setProperty(prop, propValue, emptyString(), ec);
    setDOMException(exec, ec);
    return true;
}

JSValue JSCSSStyleDeclaration::getPropertyCSSValue(ExecState* exec)
{
    const String& propertyName(ustringToString(exec->argument(0).toString(exec)->value(exec)));
    if (exec->hadException())
        return jsUndefined();

    RefPtr<CSSValue> cssValue = impl()->getPropertyCSSValue(propertyName);
    if (!cssValue)
        return jsNull();

    currentWorld(exec)->m_cssValueRoots.add(cssValue.get(), root(impl())); // Balanced by JSCSSValueOwner::finalize().
    return toJS(exec, globalObject(), WTF::getPtr(cssValue));
}

void JSCSSStyleDeclaration::getOwnPropertyNames(JSObject* object, ExecState* exec, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    JSCSSStyleDeclaration* thisObject = jsCast<JSCSSStyleDeclaration*>(object);
    ASSERT_GC_OBJECT_INHERITS(thisObject, &s_info);

    unsigned length = thisObject->impl()->length();
    for (unsigned i = 0; i < length; ++i)
        propertyNames.add(Identifier::from(exec, i));

    static Identifier* propertyIdentifiers = 0;
    if (!propertyIdentifiers) {
        Vector<String, numCSSProperties> jsPropertyNames;
        for (int id = firstCSSProperty; id < firstCSSProperty + numCSSProperties; ++id)
            jsPropertyNames.append(getJSPropertyName(static_cast<CSSPropertyID>(id)));
        sort(jsPropertyNames.begin(), jsPropertyNames.end(), WTF::codePointCompareLessThan);

        propertyIdentifiers = new Identifier[numCSSProperties];
        for (int i = 0; i < numCSSProperties; ++i)
            propertyIdentifiers[i] = Identifier(exec, jsPropertyNames[i].impl());
    }

    for (int i = 0; i < numCSSProperties; ++i)
        propertyNames.add(propertyIdentifiers[i]);

    Base::getOwnPropertyNames(thisObject, exec, propertyNames, mode);
}

} // namespace WebCore
