/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "InspectorValues.h"

#if ENABLE(INSPECTOR)

namespace WebCore {

inline bool escapeChar(UChar c, Vector<UChar>* dst)
{
    switch (c) {
    case '\b': dst->append("\\b", 2); break;
    case '\f': dst->append("\\f", 2); break;
    case '\n': dst->append("\\n", 2); break;
    case '\r': dst->append("\\r", 2); break;
    case '\t': dst->append("\\t", 2); break;
    case '\\': dst->append("\\\\", 2); break;
    case '"': dst->append("\\\"", 2); break;
    default:
        return false;
    }
    return true;
}

inline void doubleQuoteString(const String& str, Vector<UChar>* dst)
{
    dst->append('"');
    for (unsigned i = 0; i < str.length(); ++i) {
        UChar c = str[i];
        if (!escapeChar(c, dst)) {
            if (c < 32 || c > 126 || c == '<' || c == '>') {
                // 1. Escaping <, > to prevent script execution.
                // 2. Technically, we could also pass through c > 126 as UTF8, but this
                //    is also optional.  It would also be a pain to implement here.
                unsigned int symbol = static_cast<unsigned int>(c);
                String symbolCode = String::format("\\u%04X", symbol);
                dst->append(symbolCode.characters(), symbolCode.length());
            } else
                dst->append(c);
        }
    }
    dst->append('"');
}

String InspectorValue::toJSONString() const
{
    Vector<UChar> result;
    result.reserveCapacity(512);
    writeJSON(&result);
    return String(result.data(), result.size());
}

void InspectorValue::writeJSON(Vector<UChar>* output) const
{
    ASSERT(m_type == TypeNull);
    output->append("null", 4);
}

void InspectorBasicValue::writeJSON(Vector<UChar>* output) const
{
    ASSERT(type() == TypeBoolean || type() == TypeDouble);
    if (type() == TypeBoolean) {
        if (m_boolValue)
            output->append("true", 4);
        else
            output->append("false", 5);
    } else if (type() == TypeDouble) {
        String value = String::format("%f", m_doubleValue);
        output->append(value.characters(), value.length());
    }
}

void InspectorString::writeJSON(Vector<UChar>* output) const
{
    ASSERT(type() == TypeString);
    doubleQuoteString(m_stringValue, output);
}

void InspectorObject::writeJSON(Vector<UChar>* output) const
{
    output->append('{');
    for (Dictionary::const_iterator it = m_data.begin(); it != m_data.end(); ++it) {
        if (it != m_data.begin())
            output->append(',');
        doubleQuoteString(it->first, output);
        output->append(':');
        it->second->writeJSON(output);
    }
    output->append('}');
}

void InspectorArray::writeJSON(Vector<UChar>* output) const
{
    output->append('[');
    for (Vector<RefPtr<InspectorValue> >::const_iterator it = m_data.begin(); it != m_data.end(); ++it) {
        if (it != m_data.begin())
            output->append(',');
        (*it)->writeJSON(output);
    }
    output->append(']');
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
