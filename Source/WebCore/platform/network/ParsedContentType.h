/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
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

#pragma once

#include <wtf/HashMap.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

WEBCORE_EXPORT bool isValidContentType(StringImpl*);

class ParsedContentType {
public:
    WEBCORE_EXPORT static std::optional<ParsedContentType> create(StringImpl*);
    ParsedContentType(ParsedContentType&&) = default;

    RefPtr<StringImpl> mimeType() const { return m_mimeType; }
    RefPtr<StringImpl> contentType() const { return m_contentType; }
    RefPtr<StringImpl> charset() const;
    void setCharset(RefPtr<StringImpl>&&);

    // Note that in the case of multiple values for the same name, the last value is returned.
    WEBCORE_EXPORT RefPtr<StringImpl> parameterValueForName(StringImpl*) const;
    NODELETE size_t parameterCount() const;

    WEBCORE_EXPORT RefPtr<StringImpl> serialize() const;

private:
    ParsedContentType(StringImpl*);
    ParsedContentType(const ParsedContentType&) = delete;
    ParsedContentType& operator=(const ParsedContentType&) = delete;
    bool parseContentType();
    void setContentType(RefPtr<StringImpl>&&);
    void setContentTypeParameter(StringImpl*, StringImpl*);

    typedef HashMap<RefPtr<StringImpl>, RefPtr<StringImpl>> KeyValuePairs;
    RefPtr<StringImpl> m_contentType;
    KeyValuePairs m_parameterValues;
    Vector<RefPtr<StringImpl>> m_parameterNames;
    RefPtr<StringImpl> m_mimeType;
};

}
