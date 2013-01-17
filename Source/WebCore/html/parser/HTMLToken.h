/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef HTMLToken_h
#define HTMLToken_h

#include "CompactHTMLToken.h"
#include "HTMLTokenTypes.h"
#include "MarkupTokenBase.h"
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class HTMLToken : public MarkupTokenBase<HTMLTokenTypes, HTMLTokenTypes::DoctypeData> {
public:
    void appendToName(UChar character)
    {
        ASSERT(m_type == HTMLTokenTypes::StartTag || m_type == HTMLTokenTypes::EndTag || m_type == HTMLTokenTypes::DOCTYPE);
        MarkupTokenBase<HTMLTokenTypes, HTMLTokenTypes::DoctypeData>::appendToName(character);
    }

    const DataVector& name() const
    {
        ASSERT(m_type == HTMLTokenTypes::StartTag || m_type == HTMLTokenTypes::EndTag || m_type == HTMLTokenTypes::DOCTYPE);
        return MarkupTokenBase<HTMLTokenTypes, HTMLTokenTypes::DoctypeData>::name();
    }

    bool forceQuirks() const
    {
        ASSERT(m_type == HTMLTokenTypes::DOCTYPE);
        return m_doctypeData->m_forceQuirks;
    }

    void setForceQuirks()
    {
        ASSERT(m_type == HTMLTokenTypes::DOCTYPE);
        m_doctypeData->m_forceQuirks = true;
    }
};

class AtomicHTMLToken : public AtomicMarkupTokenBase<HTMLToken>, public RefCounted<AtomicHTMLToken> {
    WTF_MAKE_NONCOPYABLE(AtomicHTMLToken);
public:
    static PassRefPtr<AtomicHTMLToken> create(HTMLToken& token)
    {
        return adoptRef(new AtomicHTMLToken(token));
    }

#if ENABLE(THREADED_HTML_PARSER)

    static PassRefPtr<AtomicHTMLToken> create(const CompactHTMLToken& token)
    {
        return adoptRef(new AtomicHTMLToken(token));
    }

#endif

    static PassRefPtr<AtomicHTMLToken> create(HTMLTokenTypes::Type type, const AtomicString& name, const Vector<Attribute>& attributes = Vector<Attribute>())
    {
        return adoptRef(new AtomicHTMLToken(type, name, attributes));
    }

    bool forceQuirks() const
    {
        ASSERT(m_type == HTMLTokenTypes::DOCTYPE);
        return m_doctypeData->m_forceQuirks;
    }
private:
    explicit AtomicHTMLToken(HTMLToken& token)
        : AtomicMarkupTokenBase<HTMLToken>(&token)
    {
    }

#if ENABLE(THREADED_HTML_PARSER)

    explicit AtomicHTMLToken(const CompactHTMLToken& token)
        : AtomicMarkupTokenBase<HTMLToken>(token.type())
    {
        switch (m_type) {
        case HTMLTokenTypes::Uninitialized:
            ASSERT_NOT_REACHED();
            break;
        case HTMLTokenTypes::DOCTYPE:
            m_name = token.data();
            m_doctypeData = adoptPtr(new HTMLToken::DoctypeData());
            m_doctypeData->m_hasPublicIdentifier = true;
            m_doctypeData->m_publicIdentifier.append(token.publicIdentifier().characters(), token.publicIdentifier().length());
            m_doctypeData->m_hasSystemIdentifier = true;
            m_doctypeData->m_systemIdentifier.append(token.systemIdentifier().characters(), token.systemIdentifier().length());
            break;
        case HTMLTokenTypes::EndOfFile:
            break;
        case HTMLTokenTypes::StartTag:
            m_attributes.reserveInitialCapacity(token.attributes().size());
            for (Vector<CompactAttribute>::const_iterator it = token.attributes().begin(); it != token.attributes().end(); ++it)
                m_attributes.append(Attribute(QualifiedName(nullAtom, it->name(), nullAtom), it->value()));
            // Fall through!
        case HTMLTokenTypes::EndTag:
            m_selfClosing = token.selfClosing();
            m_name = AtomicString(token.data());
            break;
        case HTMLTokenTypes::Comment:
            m_data = token.data();
            break;
        case HTMLTokenTypes::Character:
            m_externalCharacters = token.data().characters();
            m_externalCharactersLength = token.data().length();
            break;
        default:
            break;
        }
    }

#endif

    AtomicHTMLToken(HTMLTokenTypes::Type type, const AtomicString& name, const Vector<Attribute>& attributes = Vector<Attribute>())
        : AtomicMarkupTokenBase<HTMLToken>(type, name, attributes)
    {
    }
};

}

#endif
