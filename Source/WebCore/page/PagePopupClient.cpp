/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
#include "PagePopupClient.h"

#if ENABLE(PAGE_POPUP)

#include <wtf/text/StringBuilder.h>

namespace WebCore {

#define addLiteral(literal, writer)    writer.addData(literal, sizeof(literal) - 1)

void PagePopupClient::addJavaScriptString(const String& str, DocumentWriter& writer)
{
    addLiteral("\"", writer);
    StringBuilder builder;
    builder.reserveCapacity(str.length());
    for (unsigned i = 0; i < str.length(); ++i) {
        if (str[i] == '\\' || str[i] == '"')
            builder.append('\\');
        builder.append(str[i]);
    }
    addString(builder.toString(), writer);
    addLiteral("\"", writer);
}

void PagePopupClient::addProperty(const char* name, const String& value, DocumentWriter& writer)
{
    writer.addData(name, strlen(name));
    addLiteral(": ", writer);
    addJavaScriptString(value, writer);
    addLiteral(",\n", writer);
}

void PagePopupClient::addProperty(const char* name, unsigned value, DocumentWriter& writer)
{
    writer.addData(name, strlen(name));
    addLiteral(": ", writer);
    addString(String::number(value), writer);
    addLiteral(",\n", writer);
}

void PagePopupClient::addProperty(const char* name, bool value, DocumentWriter& writer)
{
    writer.addData(name, strlen(name));
    addLiteral(": ", writer);
    if (value)
        addLiteral("true", writer);
    else
        addLiteral("false", writer);
    addLiteral(",\n", writer);
}

void PagePopupClient::addProperty(const char* name, const Vector<String>& values, DocumentWriter& writer)
{
    writer.addData(name, strlen(name));
    addLiteral(": [", writer);
    for (unsigned i = 0; i < values.size(); ++i) {
        if (i)
            addLiteral(",", writer);
        addJavaScriptString(values[i], writer);
    }
    addLiteral("],\n", writer);
}

} // namespace WebCore

#endif // ENABLE(PAGE_POPUP)
