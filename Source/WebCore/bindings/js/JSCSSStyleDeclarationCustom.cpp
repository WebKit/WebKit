/*
 * Copyright (C) 2007-2016 Apple Inc. All rights reserved.
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
#include "JSCSSStyleDeclarationCustom.h"

#include "CSSPropertyNames.h"
#include "CSSPropertyParser.h"
#include "CSSRule.h"
#include "CSSStyleDeclaration.h"
#include "CSSStyleSheet.h"
#include "CustomElementReactionQueue.h"
#include "DeprecatedCSSOMPrimitiveValue.h"
#include "HashTools.h"
#include "JSCSSStyleDeclaration.h"
#include "JSDeprecatedCSSOMValue.h"
#include "JSNode.h"
#include "JSStyleSheetCustom.h"
#include "RuntimeEnabledFeatures.h"
#include "Settings.h"
#include "StyleProperties.h"
#include "StyledElement.h"
#include <runtime/IdentifierInlines.h>
#include <runtime/StringPrototype.h>
#include <wtf/ASCIICType.h>
#include <wtf/text/AtomicString.h>
#include <wtf/text/StringConcatenate.h>

using namespace JSC;

namespace WebCore {

void* root(CSSStyleDeclaration* style)
{
    ASSERT(style);
    if (auto* parentRule = style->parentRule())
        return root(parentRule);
    if (auto* styleSheet = style->parentStyleSheet())
        return root(styleSheet);
    if (auto* parentElement = style->parentElement())
        return root(parentElement);
    return style;
}

void JSCSSStyleDeclaration::visitAdditionalChildren(SlotVisitor& visitor)
{
    visitor.addOpaqueRoot(root(&wrapped()));
}

enum class PropertyNamePrefix {
    None, Epub, CSS, Pixel, Pos, WebKit,
#if ENABLE(LEGACY_CSS_VENDOR_PREFIXES)
    Apple, KHTML,
#endif
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
    if (propertyName.length() < prefixLength + 1)
        return false;

    for (size_t i = offset; i < prefixLength; ++i) {
        if (propertyName[i] != prefix[i])
            return false;
    }

    if (!isASCIIUpper(propertyName[prefixLength]))
        return false;

    return true;
}

static PropertyNamePrefix propertyNamePrefix(const StringImpl& propertyName)
{
    ASSERT(propertyName.length());

    // First character of the prefix within the property name may be upper or lowercase.
    UChar firstChar = toASCIILower(propertyName[0]);
    switch (firstChar) {
#if ENABLE(LEGACY_CSS_VENDOR_PREFIXES)
    case 'a':
        if (RuntimeEnabledFeatures::sharedFeatures().legacyCSSVendorPrefixesEnabled() && matchesCSSPropertyNamePrefix(propertyName, "apple"))
            return PropertyNamePrefix::Apple;
        break;
#endif
    case 'c':
        if (matchesCSSPropertyNamePrefix(propertyName, "css"))
            return PropertyNamePrefix::CSS;
        break;
#if ENABLE(LEGACY_CSS_VENDOR_PREFIXES)
    case 'k':
        if (RuntimeEnabledFeatures::sharedFeatures().legacyCSSVendorPrefixesEnabled() && matchesCSSPropertyNamePrefix(propertyName, "khtml"))
            return PropertyNamePrefix::KHTML;
        break;
#endif
    case 'e':
        if (matchesCSSPropertyNamePrefix(propertyName, "epub"))
            return PropertyNamePrefix::Epub;
        break;
    case 'p':
        if (matchesCSSPropertyNamePrefix(propertyName, "pos"))
            return PropertyNamePrefix::Pos;
        if (matchesCSSPropertyNamePrefix(propertyName, "pixel"))
            return PropertyNamePrefix::Pixel;
        break;
    case 'w':
        if (matchesCSSPropertyNamePrefix(propertyName, "webkit"))
            return PropertyNamePrefix::WebKit;
        break;
    default:
        break;
    }
    return PropertyNamePrefix::None;
}

static inline void writeWebKitPrefix(char*& buffer)
{
    *buffer++ = '-';
    *buffer++ = 'w';
    *buffer++ = 'e';
    *buffer++ = 'b';
    *buffer++ = 'k';
    *buffer++ = 'i';
    *buffer++ = 't';
    *buffer++ = '-';
}

static inline void writeEpubPrefix(char*& buffer)
{
    *buffer++ = '-';
    *buffer++ = 'e';
    *buffer++ = 'p';
    *buffer++ = 'u';
    *buffer++ = 'b';
    *buffer++ = '-';
}

struct CSSPropertyInfo {
    CSSPropertyID propertyID;
    bool hadPixelOrPosPrefix;
};

static CSSPropertyInfo parseJavaScriptCSSPropertyName(PropertyName propertyName)
{
    CSSPropertyInfo propertyInfo = {CSSPropertyInvalid, false};
    bool hadPixelOrPosPrefix = false;

    StringImpl* propertyNameString = propertyName.publicName();
    if (!propertyNameString)
        return propertyInfo;
    unsigned length = propertyNameString->length();
    if (!length)
        return propertyInfo;

    String stringForCache = String(propertyNameString);
    using CSSPropertyInfoMap = HashMap<String, CSSPropertyInfo>;
    static NeverDestroyed<CSSPropertyInfoMap> propertyInfoCache;
    propertyInfo = propertyInfoCache.get().get(stringForCache);
    if (propertyInfo.propertyID)
        return propertyInfo;

    const size_t bufferSize = maxCSSPropertyNameLength + 1;
    char buffer[bufferSize];
    char* bufferPtr = buffer;
    const char* name = bufferPtr;

    unsigned i = 0;
    // Prefixes CSS, Pixel, Pos are ignored.
    // Prefixes Apple, KHTML and Webkit are transposed to "-webkit-".
    // The prefix "Epub" becomes "-epub-".
    switch (propertyNamePrefix(*propertyNameString)) {
    case PropertyNamePrefix::None:
        if (isASCIIUpper((*propertyNameString)[0]))
            return propertyInfo;
        break;
    case PropertyNamePrefix::CSS:
        i += 3;
        break;
    case PropertyNamePrefix::Pixel:
        i += 5;
        hadPixelOrPosPrefix = true;
        break;
    case PropertyNamePrefix::Pos:
        i += 3;
        hadPixelOrPosPrefix = true;
        break;
#if ENABLE(LEGACY_CSS_VENDOR_PREFIXES)
    case PropertyNamePrefix::Apple:
    case PropertyNamePrefix::KHTML:
        ASSERT(RuntimeEnabledFeatures::sharedFeatures().legacyCSSVendorPrefixesEnabled());
        writeWebKitPrefix(bufferPtr);
        i += 5;
        break;
#endif
    case PropertyNamePrefix::Epub:
        writeEpubPrefix(bufferPtr);
        i += 4;
        break;
    case PropertyNamePrefix::WebKit:
        writeWebKitPrefix(bufferPtr);
        i += 6;
        break;
    }

    *bufferPtr++ = toASCIILower((*propertyNameString)[i++]);

    char* bufferEnd = buffer + bufferSize;
    char* stringEnd = bufferEnd - 1;
    size_t bufferSizeLeft = stringEnd - bufferPtr;
    size_t propertySizeLeft = length - i;
    if (propertySizeLeft > bufferSizeLeft)
        return propertyInfo;

    for (; i < length; ++i) {
        UChar c = (*propertyNameString)[i];
        if (!c || !isASCII(c))
            return propertyInfo; // illegal character
        if (isASCIIUpper(c)) {
            size_t bufferSizeLeft = stringEnd - bufferPtr;
            size_t propertySizeLeft = length - i + 1;
            if (propertySizeLeft > bufferSizeLeft)
                return propertyInfo;
            *bufferPtr++ = '-';
            *bufferPtr++ = toASCIILowerUnchecked(c);
        } else
            *bufferPtr++ = c;
        ASSERT_WITH_SECURITY_IMPLICATION(bufferPtr < bufferEnd);
    }
    ASSERT_WITH_SECURITY_IMPLICATION(bufferPtr < bufferEnd);
    *bufferPtr = '\0';

    unsigned outputLength = bufferPtr - buffer;
#if PLATFORM(IOS)
    cssPropertyNameIOSAliasing(buffer, name, outputLength);
#endif

    auto* hashTableEntry = findProperty(name, outputLength);
    if (auto propertyID = hashTableEntry ? hashTableEntry->id : 0) {
        propertyInfo.hadPixelOrPosPrefix = hadPixelOrPosPrefix;
        propertyInfo.propertyID = static_cast<CSSPropertyID>(propertyID);
        propertyInfoCache.get().add(stringForCache, propertyInfo);
    }
    return propertyInfo;
}

static inline JSValue stylePropertyGetter(ExecState& state, JSCSSStyleDeclaration& thisObject, CSSPropertyID propertyID, const RefPtr<CSSValue>& value)
{
    if (value)
        return toJS<IDLNullable<IDLDOMString>>(state, value->cssText());
    // If the property is a shorthand property (such as "padding"), it can only be accessed using getPropertyValue.
    return toJS<IDLDOMString>(state, thisObject.wrapped().getPropertyValueInternal(propertyID));
}

static inline JSValue stylePropertyGetter(ExecState& state, JSCSSStyleDeclaration& thisObject, CSSPropertyID propertyID)
{
    return stylePropertyGetter(state, thisObject, propertyID, thisObject.wrapped().getPropertyCSSValueInternal(propertyID));
}

static inline JSValue stylePropertyGetterPixelOrPosPrefix(ExecState& state, JSCSSStyleDeclaration& thisObject, CSSPropertyID propertyID)
{
    // Call this version of the getter so that, e.g., pixelTop returns top as a number
    // in pixel units and posTop should does the same _if_ this is a positioned element.
    // FIXME: If not a positioned element, MSIE documentation says posTop should return 0; this rule is not implemented.
    auto value = thisObject.wrapped().getPropertyCSSValueInternal(propertyID);
    if (is<CSSPrimitiveValue>(value.get()))
        return jsNumber(downcast<CSSPrimitiveValue>(*value).floatValue(CSSPrimitiveValue::CSS_PX));
    return stylePropertyGetter(state, thisObject, propertyID, value);
}

static inline JSValue stylePropertyGetter(ExecState& state, JSCSSStyleDeclaration& thisObject, const CSSPropertyInfo& propertyInfo)
{
    if (propertyInfo.hadPixelOrPosPrefix)
        return stylePropertyGetterPixelOrPosPrefix(state, thisObject, propertyInfo.propertyID);
    return stylePropertyGetter(state, thisObject, propertyInfo.propertyID);
}

bool JSCSSStyleDeclaration::getOwnPropertySlotDelegate(ExecState* state, PropertyName propertyName, PropertySlot& slot)
{
    auto propertyInfo = parseJavaScriptCSSPropertyName(propertyName);
    if (!propertyInfo.propertyID)
        return false;
    slot.setValue(this, DontDelete, stylePropertyGetter(*state, *this, propertyInfo));
    return true;
}

bool JSCSSStyleDeclaration::putDelegate(ExecState* state, PropertyName propertyName, JSValue value, PutPropertySlot&, bool& putResult)
{
    CustomElementReactionStack customElementReactionStack;
    auto propertyInfo = parseJavaScriptCSSPropertyName(propertyName);
    if (!propertyInfo.propertyID)
        return false;

    auto propertyValue = convert<IDLDOMString>(*state, value, StringConversionConfiguration::TreatNullAsEmptyString);
    if (propertyInfo.hadPixelOrPosPrefix)
        propertyValue.append("px");

    bool important = false;
    if (Settings::shouldRespectPriorityInCSSAttributeSetters()) {
        auto importantIndex = propertyValue.findIgnoringASCIICase("!important");
        if (importantIndex && importantIndex != notFound) {
            important = true;
            propertyValue = propertyValue.left(importantIndex - 1);
        }
    }

    auto setPropertyInternalResult = wrapped().setPropertyInternal(propertyInfo.propertyID, propertyValue, important);
    if (setPropertyInternalResult.hasException()) {
        auto& vm = state->vm();
        auto scope = DECLARE_THROW_SCOPE(vm);
        propagateException(*state, scope, setPropertyInternalResult.releaseException());
        return true;
    }
    putResult = setPropertyInternalResult.releaseReturnValue();
    return true;
}

JSValue JSCSSStyleDeclaration::getPropertyCSSValue(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(state.argumentCount() < 1))
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));

    auto propertyName = state.uncheckedArgument(0).toWTFString(&state);
    RETURN_IF_EXCEPTION(scope, JSValue());

    auto value = wrapped().getPropertyCSSValue(propertyName);
    if (!value)
        return jsNull();

    globalObject()->world().m_deprecatedCSSOMValueRoots.add(value.get(), root(&wrapped())); // Balanced by JSDeprecatedCSSOMValueOwner::finalize().
    return toJS(&state, globalObject(), *value);
}

void JSCSSStyleDeclaration::getOwnPropertyNames(JSObject* object, ExecState* state, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    static const Identifier* const cssPropertyNames = [state] {
        String names[numCSSProperties];
        for (int i = 0; i < numCSSProperties; ++i)
            names[i] = getJSPropertyName(static_cast<CSSPropertyID>(firstCSSProperty + i));
        std::sort(&names[0], &names[numCSSProperties], WTF::codePointCompareLessThan);
        auto* identifiers = new Identifier[numCSSProperties];
        for (int i = 0; i < numCSSProperties; ++i)
            identifiers[i] = Identifier::fromString(state, names[i]);
        return identifiers;
    }();

    unsigned length = jsCast<JSCSSStyleDeclaration*>(object)->wrapped().length();
    for (unsigned i = 0; i < length; ++i)
        propertyNames.add(Identifier::from(state, i));
    for (int i = 0; i < numCSSProperties; ++i)
        propertyNames.add(cssPropertyNames[i]);

    Base::getOwnPropertyNames(object, state, propertyNames, mode);
}

} // namespace WebCore
