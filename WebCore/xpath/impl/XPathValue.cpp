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

#include "DeprecatedString.h"
#include "Logging.h"

#ifdef _MSC_VER // math functions missing from Microsoft Visual Studio standard C library
#include <xmath.h>
#define isnan(x) _isnan(x)
#define isinf(x) !_finite(x)
#define signbit(x) (_copysign(1.0, (x)) < 0)
#endif

namespace WebCore {
namespace XPath {

Value::Value()
    : m_type(Boolean), m_bool(false)
{
}

Value::Value(Node* value)
    : m_type(NodeVector_)
{
    m_nodeVector.append(value);
}

Value::Value(const NodeVector& value)
    : m_type(NodeVector_), m_nodeVector(value)
{
}

Value::Value(bool value)
    : m_type(Boolean), m_bool(value)
{
}

Value::Value(unsigned value)
    : m_type(Number), m_number(value)
{
}

Value::Value(unsigned long value)
    : m_type(Number), m_number(value)
{
}

Value::Value(double value)
    : m_type(Number), m_number(value)
{
}

Value::Value(const String& value)
    : m_type(String_), m_string(value)
{
}

const NodeVector &Value::toNodeVector() const
{
    if (m_type != NodeVector_)
        LOG(XPath, "Cannot convert anything to a nodevector.");    
    return m_nodeVector;    
}    

bool Value::toBoolean() const
{
    switch (m_type) {
        case NodeVector_:
            return !m_nodeVector.isEmpty();
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
            double value = m_string.deprecatedString().simplifyWhiteSpace().toDouble(&canConvert);
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
            if (m_nodeVector.isEmpty()) 
                return "";
            return stringValue(m_nodeVector[0].get());
        case String_:
            return m_string;
        case Number:
            if (isnan(m_number))
                return "NaN";
            if (m_number == 0)
                return "0";
            if (isinf(m_number))
                return signbit(m_number) ? "-Infinity" : "Infinity";
            return String::number(m_number);
        case Boolean:
            return m_bool ? "true" : "false";
    }
    return String();
}

}
}

#endif // XPATH_SUPPORT