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
#include "XSSAuditorDelegate.h"

namespace WebCore {

struct SameSizeAsCompactHTMLToken  {
    unsigned bitfields;
    String name;
    Vector<CompactAttribute> vector;
    TextPosition textPosition;
    OwnPtr<XSSInfo> xssInfo;
};

COMPILE_ASSERT(sizeof(CompactHTMLToken) == sizeof(SameSizeAsCompactHTMLToken), CompactHTMLToken_should_stay_small);

CompactHTMLToken::CompactHTMLToken(const HTMLToken* token, const TextPosition& textPosition)
    : m_type(token->type())
    , m_isAll8BitData(false)
    , m_doctypeForcesQuirks(false)
    , m_textPosition(textPosition)
{
    switch (m_type) {
    case HTMLTokenTypes::Uninitialized:
        ASSERT_NOT_REACHED();
        break;
    case HTMLTokenTypes::DOCTYPE: {
        m_data = String(token->name().data(), token->name().size());
        // There is only 1 DOCTYPE token per document, so to avoid increasing the
        // size of CompactHTMLToken, we just use the m_attributes vector.
        String publicIdentifier(token->publicIdentifier().data(), token->publicIdentifier().size());
        String systemIdentifier(token->systemIdentifier().data(), token->systemIdentifier().size());
        m_attributes.append(CompactAttribute(publicIdentifier, systemIdentifier));
        m_doctypeForcesQuirks = token->forceQuirks();
        break;
    }
    case HTMLTokenTypes::EndOfFile:
        break;
    case HTMLTokenTypes::StartTag:
        m_attributes.reserveInitialCapacity(token->attributes().size());
        for (Vector<HTMLToken::Attribute>::const_iterator it = token->attributes().begin(); it != token->attributes().end(); ++it)
            m_attributes.append(CompactAttribute(String(it->m_name.data(), it->m_name.size()), String(it->m_value.data(), it->m_value.size())));
        // Fall through!
    case HTMLTokenTypes::EndTag:
        m_selfClosing = token->selfClosing();
        // Fall through!
    case HTMLTokenTypes::Comment:
    case HTMLTokenTypes::Character:
        if (token->isAll8BitData()) {
            m_data = String::make8BitFrom16BitSource(token->data().data(), token->data().size());
            m_isAll8BitData = true;
        } else
            m_data = String(token->data().data(), token->data().size());
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
}

CompactHTMLToken::CompactHTMLToken(const CompactHTMLToken& other)
    : m_type(other.m_type)
    , m_selfClosing(other.m_selfClosing)
    , m_isAll8BitData(other.m_isAll8BitData)
    , m_doctypeForcesQuirks(other.m_doctypeForcesQuirks)
    , m_data(other.m_data)
    , m_attributes(other.m_attributes)
    , m_textPosition(other.m_textPosition)
{
    if (other.m_xssInfo)
        m_xssInfo = adoptPtr(new XSSInfo(*other.m_xssInfo));
}

bool CompactHTMLToken::isSafeToSendToAnotherThread() const
{
    for (Vector<CompactAttribute>::const_iterator it = m_attributes.begin(); it != m_attributes.end(); ++it) {
        if (!it->name().isSafeToSendToAnotherThread())
            return false;
        if (!it->value().isSafeToSendToAnotherThread())
            return false;
    }
    if (m_xssInfo && !m_xssInfo->isSafeToSendToAnotherThread())
        return false;
    return m_data.isSafeToSendToAnotherThread();
}

XSSInfo* CompactHTMLToken::xssInfo() const
{
    return m_xssInfo.get();
}

void CompactHTMLToken::setXSSInfo(PassOwnPtr<XSSInfo> xssInfo)
{
    m_xssInfo = xssInfo;
}

}

#endif // ENABLE(THREADED_HTML_PARSER)
