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

#include "MarkupTokenBase.h"

namespace WebCore {

class HTMLTokenTypes {
public:
    enum Type {
        Uninitialized,
        DOCTYPE,
        StartTag,
        EndTag,
        Comment,
        Character,
        EndOfFile,
    };

    class DoctypeData : public DoctypeDataBase {
        WTF_MAKE_NONCOPYABLE(DoctypeData);
    public:
        DoctypeData()
            : m_forceQuirks(false)
        {
        }

        bool m_forceQuirks;
    };
};

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

class AtomicHTMLToken : public AtomicMarkupTokenBase<HTMLToken> {
    WTF_MAKE_NONCOPYABLE(AtomicHTMLToken);
public:
    AtomicHTMLToken(HTMLToken& token) : AtomicMarkupTokenBase<HTMLToken>(&token) { }

    AtomicHTMLToken(HTMLTokenTypes::Type type, const AtomicString& name, const Vector<Attribute>& attributes = Vector<Attribute>())
        : AtomicMarkupTokenBase<HTMLToken>(type, name, attributes)
    {
    }

    bool forceQuirks() const
    {
        ASSERT(m_type == HTMLTokenTypes::DOCTYPE);
        return m_doctypeData->m_forceQuirks;
    }
};

}

#endif
