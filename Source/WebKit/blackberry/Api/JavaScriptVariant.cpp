/*
 * Copyright (C) 2012 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "JavaScriptVariant.h"

#include "JavaScriptVariant_p.h"
#include "WebPage.h"
#include <JSStringRef.h>
#include <JSValueRef.h>
#include <stdlib.h>
#include <wtf/Vector.h>

namespace BlackBerry {
namespace WebKit {

JavaScriptVariant JSValueRefToBlackBerryJavaScriptVariant(const JSGlobalContextRef& ctx, const JSValueRef& value)
{
    JavaScriptVariant returnValue;

    switch (JSValueGetType(ctx, value)) {
    case kJSTypeNull:
        returnValue.setType(JavaScriptVariant::Null);
        break;
    case kJSTypeBoolean:
        returnValue.setBoolean(JSValueToBoolean(ctx, value));
        break;
    case kJSTypeNumber:
        returnValue.setDouble(JSValueToNumber(ctx, value, 0));
        break;
    case kJSTypeString: {
        JSStringRef stringRef = JSValueToStringCopy(ctx, value, 0);
        size_t bufferSize = JSStringGetMaximumUTF8CStringSize(stringRef);
        WTF::Vector<char> buffer(bufferSize);
        JSStringGetUTF8CString(stringRef, buffer.data(), bufferSize);
        returnValue.setString(WebString::fromUtf8(buffer.data()).utf8().c_str());
        break;
    }
    case kJSTypeObject:
        returnValue.setType(JavaScriptVariant::Object);
        break;
    case kJSTypeUndefined:
        returnValue.setType(JavaScriptVariant::Undefined);
        break;
    }
    return returnValue;
}

JSValueRef BlackBerryJavaScriptVariantToJSValueRef(const JSGlobalContextRef& ctx, const JavaScriptVariant& variant)
{
    JSValueRef ref = 0;
    switch (variant.type()) {
    case JavaScriptVariant::Undefined:
        ref = JSValueMakeUndefined(ctx);
        break;
    case JavaScriptVariant::Null:
        ref = JSValueMakeNull(ctx);
        break;
    case JavaScriptVariant::Boolean:
        ref = JSValueMakeBoolean(ctx, variant.booleanValue());
        break;
    case JavaScriptVariant::Number:
        ref = JSValueMakeNumber(ctx, variant.doubleValue());
        break;
    case JavaScriptVariant::String: {
        JSStringRef str = JSStringCreateWithUTF8CString(variant.stringValue());
        ref = JSValueMakeString(ctx, str);
        JSStringRelease(str);
        break;
    }
    case JavaScriptVariant::Exception:
    case JavaScriptVariant::Object:
        ASSERT_NOT_REACHED();
        break;
    }
    return ref;
}

JavaScriptVariant::JavaScriptVariant()
    : m_type(Undefined)
    , m_stringValue(0)
{
}

JavaScriptVariant::JavaScriptVariant(double value)
    : m_type(Undefined)
    , m_stringValue(0)
{
    setDouble(value);
}

JavaScriptVariant::JavaScriptVariant(int value)
    : m_type(Undefined)
    , m_stringValue(0)
{
    setDouble(value);
}

JavaScriptVariant::JavaScriptVariant(const char* value)
    : m_type(Undefined)
    , m_stringValue(0)
{
    setString(value);
}

JavaScriptVariant::JavaScriptVariant(const std::string& value)
    : m_type(Undefined)
    , m_stringValue(0)
{
    setString(value.c_str());
}

JavaScriptVariant::JavaScriptVariant(bool value)
    : m_type(Undefined)
    , m_stringValue(0)
{
    setBoolean(value);
}

JavaScriptVariant::JavaScriptVariant(const JavaScriptVariant &v)
    : m_type(Undefined)
    , m_stringValue(0)
{
    this->operator=(v);
}

JavaScriptVariant::~JavaScriptVariant()
{
    // Prevent memory leaks if we have strings
    setType(Undefined);
}

JavaScriptVariant& JavaScriptVariant::operator=(const JavaScriptVariant& v)
{
    switch (v.type()) {
    case Boolean:
        setBoolean(v.booleanValue());
        break;
    case Number:
        setDouble(v.doubleValue());
        break;
    case String:
        setString(v.stringValue());
        break;
    default:
        setType(v.type());
        break;
    }

    return *this;
}

void JavaScriptVariant::setType(const DataType& type)
{
    if (m_type == String)
        free(m_stringValue);
    m_type = type;
    m_stringValue = 0;
}

JavaScriptVariant::DataType JavaScriptVariant::type() const
{
    return m_type;
}

void JavaScriptVariant::setDouble(double value)
{
    setType(Number);
    m_doubleValue = value;
}

double JavaScriptVariant::doubleValue() const
{
    return m_doubleValue;
}

void JavaScriptVariant::setString(const char* value)
{
    setType(String);
    m_stringValue = strdup(value);
}

char* JavaScriptVariant::stringValue() const
{
    return m_stringValue;
}

void JavaScriptVariant::setBoolean(bool value)
{
    setType(Boolean);
    m_booleanValue = value;
}

bool JavaScriptVariant::booleanValue() const
{
    return m_booleanValue;
}

}
}
