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

#include "config.h"

#if ENABLE(THREADED_HTML_PARSER)

#include "CompactHTMLToken.h"

#include "HTMLToken.h"

namespace WebCore {

CompactHTMLToken::CompactHTMLToken(const HTMLToken& token)
    : m_type(token.type())
{
    switch (m_type) {
    case HTMLTokenTypes::Uninitialized:
        ASSERT_NOT_REACHED();
        break;
    case HTMLTokenTypes::DOCTYPE:
        m_data = String(token.name().data(), token.name().size());
        m_publicIdentifier = String(token.publicIdentifier().data(), token.publicIdentifier().size());
        m_systemIdentifier = String(token.systemIdentifier().data(), token.systemIdentifier().size());
        break;
    case HTMLTokenTypes::EndOfFile:
        break;
    case HTMLTokenTypes::StartTag:
        m_attributes.reserveInitialCapacity(token.attributes().size());
        for (Vector<AttributeBase>::const_iterator it = token.attributes().begin(); it != token.attributes().end(); ++it)
            m_attributes.append(CompactAttribute(String(it->m_name.data(), it->m_name.size()), String(it->m_value.data(), it->m_value.size())));
        // Fall through!
    case HTMLTokenTypes::EndTag:
        m_selfClosing = token.selfClosing();
        // Fall through!
    case HTMLTokenTypes::Comment:
    case HTMLTokenTypes::Character:
        if (token.isAll8BitData())
            m_data = String::make8BitFrom16BitSource(token.data().data(), token.data().size());
        else
            m_data = String(token.data().data(), token.data().size());
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
}

static bool isStringSafeToSendToAnotherThread(const String& string)
{
    StringImpl* impl = string.impl();
    if (!impl)
        return true;
    if (impl->hasOneRef())
        return true;
    if (string.isEmpty())
        return true;
    return false;
}

bool CompactHTMLToken::isSafeToSendToAnotherThread() const
{
    for (Vector<CompactAttribute>::const_iterator it = m_attributes.begin(); it != m_attributes.end(); ++it) {
        if (!isStringSafeToSendToAnotherThread(it->name()))
            return false;
        if (!isStringSafeToSendToAnotherThread(it->value()))
            return false;
    }

    return isStringSafeToSendToAnotherThread(m_data)
        && isStringSafeToSendToAnotherThread(m_publicIdentifier)
        && isStringSafeToSendToAnotherThread(m_systemIdentifier);
}

}

#endif // ENABLE(THREADED_HTML_PARSER)
