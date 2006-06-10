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
#include "Node.h"
#include <wtf/MathExtras.h>
#include <math.h>

namespace WebCore {
namespace XPath {

Value::Value()
    : m_type(BooleanValue), m_bool(false)
{
}

Value::Value(Node* value)
    : m_type(NodeVectorValue)
{
    m_nodeVector.append(value);
}

Value::Value(const NodeVector& value)
    : m_type(NodeVectorValue), m_nodeVector(value)
{
}

Value::Value(bool value)
    : m_type(BooleanValue), m_bool(value)
{
}

Value::Value(unsigned value)
    : m_type(NumberValue), m_number(value)
{
}

Value::Value(unsigned long value)
    : m_type(NumberValue), m_number(value)
{
}

Value::Value(double value)
    : m_type(NumberValue), m_number(value)
{
}

Value::Value(const String& value)
    : m_type(StringValue), m_string(value)
{
}

const NodeVector &Value::toNodeVector() const
{
    return m_nodeVector;    
}    

bool Value::toBoolean() const
{
    switch (m_type) {
        case NodeVectorValue:
            return !m_nodeVector.isEmpty();
        case BooleanValue:
            return m_bool;
        case NumberValue:
            return m_number != 0;
        case StringValue:
            return !m_string.isEmpty();
    }
    ASSERT_NOT_REACHED();
    return false;
}

double Value::toNumber() const
{
    switch (m_type) {
        case NodeVectorValue:
            return Value(toString()).toNumber();
        case NumberValue:
            return m_number;
        case StringValue: {
            bool canConvert;
            double value = m_string.deprecatedString().simplifyWhiteSpace().toDouble(&canConvert);
            if (canConvert)
                return value;
            return NAN;
        }
        case BooleanValue:
            return m_bool;
    }
    ASSERT_NOT_REACHED();
    return 0.0;
}

String Value::toString() const
{
    switch (m_type) {
        case NodeVectorValue:
            if (m_nodeVector.isEmpty()) 
                return "";
            return stringValue(m_nodeVector[0].get());
        case StringValue:
            return m_string;
        case NumberValue:
            if (isnan(m_number))
                return "NaN";
            if (m_number == 0)
                return "0";
            if (isinf(m_number))
                return signbit(m_number) ? "-Infinity" : "Infinity";
            return String::number(m_number);
        case BooleanValue:
            return m_bool ? "true" : "false";
    }
    ASSERT_NOT_REACHED();
    return String();
}

}
}

#endif // XPATH_SUPPORT
