/*
 * Copyright (C) 2011 Google, Inc. All rights reserved.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ContentSecurityPolicy.h"
#include "Document.h"

namespace WebCore {

class CSPDirective {
public:
    CSPDirective(const String& name, const String& value)
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

ContentSecurityPolicy::ContentSecurityPolicy()
    : m_isEnabled(false)
{
}

ContentSecurityPolicy::~ContentSecurityPolicy()
{
}

void ContentSecurityPolicy::didReceiveHeader(const String& header)
{
    if (!m_directives.isEmpty())
        return; // The first policy wins.

    m_isEnabled = true;
    parse(header);
}

bool ContentSecurityPolicy::canLoadExternalScriptFromSrc(const String&) const
{
    return !m_isEnabled;
}

void ContentSecurityPolicy::parse(const String& policy)
{
    ASSERT(m_directives.isEmpty());

    if (policy.isEmpty())
        return;

    enum {
        BeforeDirectiveName,
        DirectiveName,
        AfterDirectiveName,
        DirectiveValue,
    } state = BeforeDirectiveName;

    const UChar* pos = policy.characters();
    const UChar* end = pos + policy.length();

    Vector<UChar, 32> name;
    Vector<UChar, 64> value;

    while (pos < end) {
        UChar currentCharacter = *pos++;
        switch (state) {
        case BeforeDirectiveName:
            if (isASCIISpace(currentCharacter))
                continue;
            state = DirectiveName;
            // Fall through.
        case DirectiveName:
            if (!isASCIISpace(currentCharacter)) {
                name.append(currentCharacter);
                continue;
            }
            state = AfterDirectiveName;
            // Fall through.
        case AfterDirectiveName:
            if (isASCIISpace(currentCharacter))
                continue;
            state = DirectiveValue;
            // Fall through.
        case DirectiveValue:
            if (currentCharacter != ';') {
                value.append(currentCharacter);
                continue;
            }
            // We use a copy here instead of String::adopt because we expect
            // the name and the value to be relatively short, so the copy will
            // be cheaper than the extra malloc.
            // FIXME: Perform directive-specific parsing of the value.
            m_directives.append(CSPDirective(String(name), String(value)));
            name.clear();
            value.clear();
            state = BeforeDirectiveName;
            continue;
        }
    }
}

}
