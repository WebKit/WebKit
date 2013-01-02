/*
 * Copyright (C) 2007-2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "V8CSSStyleDeclaration.h"

#include "CSSParser.h"
#include "CSSPropertyNames.h"
#include "CSSStyleDeclaration.h"
#include "CSSValue.h"
#include "CSSPrimitiveValue.h"
#include "EventTarget.h"

#include "V8Binding.h"

#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringConcatenate.h>
#include <wtf/ASCIICType.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>

using namespace WTF;
using namespace std;

namespace WebCore {

// FIXME: Next two functions look lifted verbatim from JSCSSStyleDeclarationCustom. Please remove duplication.

// Check for a CSS prefix.
// Passed prefix is all lowercase.
// First character of the prefix within the property name may be upper or lowercase.
// Other characters in the prefix within the property name must be lowercase.
// The prefix within the property name must be followed by a capital letter.
static bool hasCSSPropertyNamePrefix(const String& propertyName, const char* prefix)
{
#ifndef NDEBUG
    ASSERT(*prefix);
    for (const char* p = prefix; *p; ++p)
        ASSERT(isASCIILower(*p));
    ASSERT(propertyName.length());
#endif

    if (toASCIILower(propertyName[0]) != prefix[0])
        return false;

    unsigned length = propertyName.length();
    for (unsigned i = 1; i < length; ++i) {
        if (!prefix[i])
            return isASCIIUpper(propertyName[i]);
        if (propertyName[i] != prefix[i])
            return false;
    }
    return false;
}

class CSSPropertyInfo {
public:
    CSSPropertyID propID;
    bool hadPixelOrPosPrefix;
};

// When getting properties on CSSStyleDeclarations, the name used from
// Javascript and the actual name of the property are not the same, so
// we have to do the following translation.  The translation turns upper
// case characters into lower case characters and inserts dashes to
// separate words.
//
// Example: 'backgroundPositionY' -> 'background-position-y'
//
// Also, certain prefixes such as 'pos', 'css-' and 'pixel-' are stripped
// and the hadPixelOrPosPrefix out parameter is used to indicate whether or
// not the property name was prefixed with 'pos-' or 'pixel-'.
static CSSPropertyInfo* cssPropertyInfo(v8::Handle<v8::String>v8PropertyName)
{
    String propertyName = toWebCoreString(v8PropertyName);
    typedef HashMap<String, CSSPropertyInfo*> CSSPropertyInfoMap;
    DEFINE_STATIC_LOCAL(CSSPropertyInfoMap, map, ());
    CSSPropertyInfo* propInfo = map.get(propertyName);
    if (!propInfo) {
        unsigned length = propertyName.length();
        bool hadPixelOrPosPrefix = false;
        if (!length)
            return 0;

        StringBuilder builder;
        builder.reserveCapacity(length);

        unsigned i = 0;

        if (hasCSSPropertyNamePrefix(propertyName, "css"))
            i += 3;
        else if (hasCSSPropertyNamePrefix(propertyName, "pixel")) {
            i += 5;
            hadPixelOrPosPrefix = true;
        } else if (hasCSSPropertyNamePrefix(propertyName, "pos")) {
            i += 3;
            hadPixelOrPosPrefix = true;
        } else if (hasCSSPropertyNamePrefix(propertyName, "webkit")
#if ENABLE(LEGACY_CSS_VENDOR_PREFIXES)
                || hasCSSPropertyNamePrefix(propertyName, "khtml")
                || hasCSSPropertyNamePrefix(propertyName, "apple")
#endif
                  )
            builder.append('-');
        else if (isASCIIUpper(propertyName[0]))
            return 0;

        builder.append(toASCIILower(propertyName[i++]));

        for (; i < length; ++i) {
            UChar c = propertyName[i];
            if (!isASCIIUpper(c))
                builder.append(c);
            else
                builder.append(makeString('-', toASCIILower(c)));
        }

        String propName = builder.toString();
        CSSPropertyID propertyID = cssPropertyID(propName);
        if (propertyID) {
            propInfo = new CSSPropertyInfo();
            propInfo->hadPixelOrPosPrefix = hadPixelOrPosPrefix;
            propInfo->propID = propertyID;
            map.add(propertyName, propInfo);
        }
    }
    return propInfo;
}

v8::Handle<v8::Array> V8CSSStyleDeclaration::namedPropertyEnumerator(const v8::AccessorInfo& info)
{
    typedef Vector<String, numCSSProperties - 1> PreAllocatedPropertyVector;
    DEFINE_STATIC_LOCAL(PreAllocatedPropertyVector, propertyNames, ());
    static unsigned propertyNamesLength = 0;

    if (propertyNames.isEmpty()) {
        for (int id = firstCSSProperty; id < firstCSSProperty + numCSSProperties; ++id)
            propertyNames.append(getJSPropertyName(static_cast<CSSPropertyID>(id)));
        sort(propertyNames.begin(), propertyNames.end(), codePointCompareLessThan);
        propertyNamesLength = propertyNames.size();
    }

    v8::Handle<v8::Array> properties = v8::Array::New(propertyNamesLength);
    for (unsigned i = 0; i < propertyNamesLength; ++i) {
        String key = propertyNames.at(i);
        ASSERT(!key.isNull());
        properties->Set(v8Integer(i, info.GetIsolate()), v8String(key, info.GetIsolate()));
    }

    return properties;
}

v8::Handle<v8::Integer> V8CSSStyleDeclaration::namedPropertyQuery(v8::Local<v8::String> v8Name, const v8::AccessorInfo& info)
{

    if (cssPropertyInfo(v8Name))
        return v8Integer(0, info.GetIsolate());

    return v8::Handle<v8::Integer>();
}

v8::Handle<v8::Value> V8CSSStyleDeclaration::namedPropertyGetter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    // First look for API defined attributes on the style declaration object.
    if (info.Holder()->HasRealNamedCallbackProperty(name))
        return v8Undefined();

    // Search the style declaration.
    CSSPropertyInfo* propInfo = cssPropertyInfo(name);

    // Do not handle non-property names.
    if (!propInfo)
        return v8Undefined();

    CSSStyleDeclaration* imp = V8CSSStyleDeclaration::toNative(info.Holder());
    RefPtr<CSSValue> cssValue = imp->getPropertyCSSValueInternal(static_cast<CSSPropertyID>(propInfo->propID));
    if (cssValue) {
        if (propInfo->hadPixelOrPosPrefix &&
            cssValue->isPrimitiveValue()) {
            return v8::Number::New(static_cast<CSSPrimitiveValue*>(
                cssValue.get())->getFloatValue(CSSPrimitiveValue::CSS_PX));
        }
        return v8StringOrNull(cssValue->cssText(), info.GetIsolate());
    }

    String result = imp->getPropertyValueInternal(static_cast<CSSPropertyID>(propInfo->propID));
    if (result.isNull())
        result = "";  // convert null to empty string.

    return v8String(result, info.GetIsolate());
}

v8::Handle<v8::Value> V8CSSStyleDeclaration::namedPropertySetter(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::AccessorInfo& info)
{
    CSSStyleDeclaration* imp = V8CSSStyleDeclaration::toNative(info.Holder());
    CSSPropertyInfo* propInfo = cssPropertyInfo(name);
    if (!propInfo)
        return v8Undefined();

    String propertyValue = toWebCoreStringWithNullCheck(value);
    if (propInfo->hadPixelOrPosPrefix)
        propertyValue.append("px");

    ExceptionCode ec = 0;
    imp->setPropertyInternal(static_cast<CSSPropertyID>(propInfo->propID), propertyValue, false, ec);

    if (ec)
        setDOMException(ec, info.GetIsolate());

    return value;
}

} // namespace WebCore
