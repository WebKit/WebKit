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

#include "HTMLParserIdioms.h"
#include "HTMLToken.h"
#include "QualifiedName.h"
#include "XSSAuditorDelegate.h"

namespace WebCore {

struct SameSizeAsCompactHTMLToken  {
    unsigned bitfields;
    String name;
    Vector<Attribute> vector;
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
    case HTMLToken::Uninitialized:
        ASSERT_NOT_REACHED();
        break;
    case HTMLToken::DOCTYPE: {
        m_data = String(token->name());
        // There is only 1 DOCTYPE token per document, so to avoid increasing the
        // size of CompactHTMLToken, we just use the m_attributes vector.
        m_attributes.append(Attribute(String(token->publicIdentifier()), String(token->systemIdentifier())));
        m_doctypeForcesQuirks = token->forceQuirks();
        break;
    }
    case HTMLToken::EndOfFile:
        break;
    case HTMLToken::StartTag:
        m_attributes.reserveInitialCapacity(token->attributes().size());
        // FIXME: Attribute names and values should be 8bit when possible.
        for (Vector<HTMLToken::Attribute>::const_iterator it = token->attributes().begin(); it != token->attributes().end(); ++it)
            m_attributes.append(Attribute(String(it->name), String(it->value)));
        // Fall through!
    case HTMLToken::EndTag:
        m_selfClosing = token->selfClosing();
        // Fall through!
    case HTMLToken::Comment:
    case HTMLToken::Character:
        if (token->isAll8BitData()) {
            m_data = String::make8BitFrom16BitSource(token->data());
            m_isAll8BitData = true;
        } else
            m_data = String(token->data());
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

const CompactHTMLToken::Attribute* CompactHTMLToken::getAttributeItem(const QualifiedName& name) const
{
    for (unsigned i = 0; i < m_attributes.size(); ++i) {
        if (threadSafeMatch(m_attributes.at(i).name, name))
            return &m_attributes.at(i);
    }
    return 0;
}

bool CompactHTMLToken::isSafeToSendToAnotherThread() const
{
    for (Vector<Attribute>::const_iterator it = m_attributes.begin(); it != m_attributes.end(); ++it) {
        if (!it->name.isSafeToSendToAnotherThread())
            return false;
        if (!it->value.isSafeToSendToAnotherThread())
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
