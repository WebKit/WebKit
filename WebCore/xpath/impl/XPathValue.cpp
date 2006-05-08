/*
 * Copyright 2005 Frerich Raabe <raabe@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "config.h"

#if XPATH_SUPPORT

#include "XPathValue.h"
#include "Logging.h"

namespace WebCore {
namespace XPath {

Value::Value()
{
}

Value::Value(Node* value)
    : m_type(NodeVector_)
{
    m_nodevector.append(value);
}

Value::Value(const NodeVector& value)
    : m_type(NodeVector_)
    , m_nodevector(value)
{
}

Value::Value(bool value)
    : m_type(Boolean),
    m_bool(value)
{
}

Value::Value(double value)
    : m_type(Number),
    m_number(value)
{
}

Value::Value(const String& value)
    : m_type(String_),
    m_string(value)
{
}

Value::Type Value::type() const
{
    return m_type;
}

bool Value::isNodeVector() const
{
    return m_type == NodeVector_;
}

bool Value::isBoolean() const
{
    return m_type == Boolean;
}

bool Value::isNumber() const
{
    return m_type == Number;
}

bool Value::isString() const
{
    return m_type == String_;
}

NodeVector& Value::toNodeVector()
{
    if (m_type != NodeVector_) {
        LOG(XPath, "Cannot convert anything to a nodevector.");
    }
    return m_nodevector;
}

const NodeVector &Value::toNodeVector() const
{
    if (m_type != NodeVector_) {
        LOG(XPath, "Cannot convert anything to a nodevector.");
    }
    
    return m_nodevector;    
}    

bool Value::toBoolean() const
{
    switch (m_type) {
        case NodeVector_:
            return !m_nodevector.isEmpty();
        case Boolean:
            return m_bool;
        case Number:
            return m_number != 0;
        case String_:
            return !m_string.isEmpty();
    }
    return false;
}

double Value::toNumber() const
{
    switch (m_type) {
        case NodeVector_:
            return Value(toString()).toNumber();
        case Number:
            return m_number;
        case String_: {
            bool canConvert;
            DeprecatedString s = m_string.deprecatedString().simplifyWhiteSpace();
            
            double value = s.toDouble(&canConvert);
            if (canConvert)
                return value;

            return NAN;
        }
        case Boolean:
            return m_bool;
    }
    return 0.0;
}

String Value::toString() const
{
    switch (m_type) {
        case NodeVector_:
            if (m_nodevector.isEmpty()) 
                return "";

            return stringValue(m_nodevector[0].get());
        case String_:
            return m_string;
        case Number:
            if (isnan(m_number))
                return "NaN";
            else if (m_number == 0)
                return "0";
            else if (isinf(m_number)) {
                if (signbit(m_number) == 0)
                    return "Infinity";
                else
                    return "-Infinity";
            }
            return DeprecatedString::number(m_number);
        case Boolean:
            return m_bool ? "true" : "false";
    }
    
    return String();
}


}
}

#endif // XPATH_SUPPORT