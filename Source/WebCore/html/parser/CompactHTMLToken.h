/*
 * Copyright (C) 2013 Google, Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef CompactHTMLToken_h
#define CompactHTMLToken_h

#if ENABLE(THREADED_HTML_PARSER)

#include "HTMLTokenTypes.h"
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class HTMLToken;

class CompactAttribute {
public:
    CompactAttribute(const String& name, const String& value)
        : m_name(name)
        , m_value(value)
    {
    }

    const String& name() const { return m_name; }
    const String& value() const { return m_value; }

private:
    String m_name;
    String m_value;
};

class CompactHTMLToken {
public:
    explicit CompactHTMLToken(const HTMLToken&);

    bool isSafeToSendToAnotherThread() const;

    HTMLTokenTypes::Type type() const { return m_type; }
    const String& data() const { return m_data; }
    bool selfClosing() const { return m_selfClosing; }
    const Vector<CompactAttribute>& attributes() const { return m_attributes; }

    const String& publicIdentifier() const { return m_publicIdentifier; }
    const String& systemIdentifier() const { return m_systemIdentifier; }

private:
    HTMLTokenTypes::Type m_type;
    String m_data; // "name", "characters", or "data" depending on m_type
    bool m_selfClosing;
    Vector<CompactAttribute> m_attributes;

    // For doctype only.
    String m_publicIdentifier;
    String m_systemIdentifier;
};

}

#endif // ENABLE(THREADED_HTML_PARSER)

#endif
