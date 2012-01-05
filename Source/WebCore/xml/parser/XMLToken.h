/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
 * Copyright (C) 2011 Apple Inc. All Rights Reserved.
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

#ifndef XMLToken_h
#define XMLToken_h

#include "Element.h"
#include "MarkupTokenBase.h"

namespace WebCore {

class XMLTokenTypes {
public:
    enum Type {
        Uninitialized,
        ProcessingInstruction,
        XMLDeclaration,
        DOCTYPE,
        CDATA,
        StartTag,
        EndTag,
        Comment,
        Character,
        Entity,
        EndOfFile,
    };
};

class PrefixedAttribute : public AttributeBase {
public:
    Range m_prefixRange;
    WTF::Vector<UChar, 32> m_prefix;
};

class XMLToken : public MarkupTokenBase<XMLTokenTypes, DoctypeDataBase, PrefixedAttribute> {
public:
    class XMLDeclarationData {
        WTF_MAKE_NONCOPYABLE(XMLDeclarationData);
    public:
        XMLDeclarationData()
            : m_hasStandalone(false)
            , m_hasEncoding(false)
            , m_standalone(false)
        {
        }

        bool m_hasStandalone;
        bool m_hasEncoding;
        bool m_standalone;
        WTF::Vector<UChar> m_encoding;
        WTF::Vector<UChar> m_version;
    };

    virtual void clear()
    {
        MarkupTokenBase<XMLTokenTypes, DoctypeDataBase, PrefixedAttribute>::clear();
        m_target.clear();
    }

    void appendToName(UChar character)
    {
        ASSERT(m_type == XMLTokenTypes::StartTag || m_type == XMLTokenTypes::EndTag || m_type == XMLTokenTypes::DOCTYPE || m_type == XMLTokenTypes::Entity);
        MarkupTokenBase<XMLTokenTypes, DoctypeDataBase, PrefixedAttribute>::appendToName(character);
    }

    const DataVector& name() const
    {
        ASSERT(m_type == XMLTokenTypes::StartTag || m_type == XMLTokenTypes::EndTag || m_type == XMLTokenTypes::DOCTYPE || m_type == XMLTokenTypes::Entity);
        return MarkupTokenBase<XMLTokenTypes, DoctypeDataBase, PrefixedAttribute>::name();
    }

    const DataVector& target() const
    {
        ASSERT(m_type == XMLTokenTypes::ProcessingInstruction);
        return m_target;
    }

    const DataVector& data() const
    {
        ASSERT(m_type == XMLTokenTypes::CDATA || m_type == XMLTokenTypes::ProcessingInstruction);
        return m_data;
    }

    const DataVector& prefix() const
    {
        ASSERT(m_type == XMLTokenTypes::StartTag || m_type == XMLTokenTypes::EndTag);
        return m_target;
    }

    const XMLDeclarationData& xmlDeclarationData() const
    {
        ASSERT(m_type == XMLTokenTypes::XMLDeclaration);
        ASSERT(m_xmlDeclarationData);
        return *m_xmlDeclarationData.get();
    }

    void beginDOCTYPE()
    {
        ASSERT(m_type == XMLTokenTypes::Uninitialized);
        m_type = XMLTokenTypes::DOCTYPE;
        m_doctypeData = adoptPtr(new DoctypeData());
    }

    void beginDOCTYPE(UChar character)
    {
        ASSERT(character);
        beginDOCTYPE();
        m_data.append(character);
    }

    void beginXMLDeclaration()
    {
        ASSERT(m_type == XMLTokenTypes::Uninitialized);
        m_type = XMLTokenTypes::XMLDeclaration;
        m_xmlDeclarationData = adoptPtr(new XMLDeclarationData());
    }

    void appendToXMLVersion(UChar character)
    {
        ASSERT(character);
        ASSERT(m_type == XMLTokenTypes::XMLDeclaration);
        ASSERT(m_xmlDeclarationData);
        m_xmlDeclarationData->m_version.append(character);
    }

    void beginXMLEncoding(UChar character)
    {
        ASSERT(character);
        ASSERT(m_type == XMLTokenTypes::XMLDeclaration);
        ASSERT(m_xmlDeclarationData);
        m_xmlDeclarationData->m_hasEncoding = true;
        m_xmlDeclarationData->m_encoding.append(character);
    }

    void appendToXMLEncoding(UChar character)
    {
        ASSERT(character);
        ASSERT(m_type == XMLTokenTypes::XMLDeclaration);
        ASSERT(m_xmlDeclarationData);
        ASSERT(m_xmlDeclarationData->m_hasEncoding);
        m_xmlDeclarationData->m_encoding.append(character);
    }

    void setXMLStandalone(bool standalone)
    {
        ASSERT(m_type == XMLTokenTypes::XMLDeclaration);
        ASSERT(m_xmlDeclarationData);
        m_xmlDeclarationData->m_hasStandalone = true;
        m_xmlDeclarationData->m_standalone = standalone;
    }

    void beginProcessingInstruction()
    {
        ASSERT(m_type == XMLTokenTypes::Uninitialized);
        m_type = XMLTokenTypes::ProcessingInstruction;
    }

    void beginProcessingInstruction(UChar character)
    {
        ASSERT(character);
        beginProcessingInstruction();
        m_target.append(character);
    }

    void appendToProcessingInstructionTarget(UChar character)
    {
        ASSERT(m_type == XMLTokenTypes::ProcessingInstruction);
        ASSERT(character);
        m_target.append(character);
    }

    void appendToProcessingInstructionData(UChar character)
    {
        ASSERT(m_type == XMLTokenTypes::ProcessingInstruction);
        ASSERT(character);
        m_data.append(character);
    }

    void endPrefix()
    {
        ASSERT(m_type == XMLTokenTypes::StartTag || m_type == XMLTokenTypes::EndTag);
        ASSERT(m_target.isEmpty());
        // FIXME: see if we can avoid the copy inherent with this swap
        m_target.swap(m_data);
    }

    bool hasPrefix() const
    {
        ASSERT(m_type == XMLTokenTypes::StartTag || m_type == XMLTokenTypes::EndTag);
        return m_target.size();
    }

    void beginCDATA()
    {
        ASSERT(m_type == XMLTokenTypes::Uninitialized);
        m_type = XMLTokenTypes::CDATA;
    }

    void appendToCDATA(UChar character)
    {
        ASSERT(m_type == XMLTokenTypes::CDATA);
        ASSERT(character);
        m_data.append(character);
    }

    void beginEntity()
    {
        ASSERT(m_type == XMLTokenTypes::Uninitialized);
        m_type = XMLTokenTypes::Entity;
    }

    void beginEntity(UChar character)
    {
        ASSERT(character);
        beginEntity();
        m_data.append(character);
    }

    void endAttributePrefix(int offset)
    {
        ASSERT(m_type == XMLTokenTypes::StartTag);
        int index = offset - m_baseOffset;
        m_currentAttribute->m_prefix.swap(m_currentAttribute->m_name);
        m_currentAttribute->m_prefixRange.m_start = m_currentAttribute->m_valueRange.m_start;
        m_currentAttribute->m_prefixRange.m_end = index;
        m_currentAttribute->m_nameRange.m_start = index;
        m_currentAttribute->m_nameRange.m_end = index;
    }

    bool attributeHasPrefix()
    {
        ASSERT(m_type == XMLTokenTypes::StartTag);
        return !m_currentAttribute->m_prefix.isEmpty();
    }

#ifndef NDEBUG
    void printAttrs() const
    {
        AttributeList::const_iterator iter = m_attributes.begin();
        for (; iter != m_attributes.end(); ++iter) {
            fprintf(stderr, " ");
            if (!iter->m_prefix.isEmpty()) {
                printString(iter->m_prefix);
                fprintf(stderr, ":");
            }
            printString(iter->m_name);
            fprintf(stderr, "=\"");
            printString(iter->m_value);
            fprintf(stderr, "\"");
        }
    }

    void print() const
    {
        switch (m_type) {
        case XMLTokenTypes::Uninitialized:
            fprintf(stderr, "UNITIALIZED");
            break;

        case XMLTokenTypes::ProcessingInstruction:
            fprintf(stderr, "ProcessingInstruction: ");
            fprintf(stderr, "<?");
            printString(m_target);
            fprintf(stderr, " ");
            printString(m_data);
            fprintf(stderr, "?>");
            break;

        case XMLTokenTypes::XMLDeclaration:
            fprintf(stderr, "XML Declaration: ");
            fprintf(stderr, "<?xml version=\"");
            ASSERT(m_xmlDeclarationData);
            printString(m_xmlDeclarationData->m_version);
            fprintf(stderr, "\"");
            if (m_xmlDeclarationData->m_hasEncoding) {
                fprintf(stderr, " encoding=\"");
                printString(m_xmlDeclarationData->m_encoding);
                fprintf(stderr, "\"");
            }
            if (m_xmlDeclarationData->m_hasStandalone)
                fprintf(stderr, " standalone=\"%s\"", m_xmlDeclarationData->m_standalone ? "yes" : "no");
            fprintf(stderr, "?>");
            break;

        case XMLTokenTypes::DOCTYPE:
            fprintf(stderr, "DOCTYPE: ");
            ASSERT(m_doctypeData);
            fprintf(stderr, "<!DOCTYPE ");
            printString(m_data);
            if (m_doctypeData->m_hasPublicIdentifier) {
                fprintf(stderr, " PUBLIC \"");
                printString(m_doctypeData->m_publicIdentifier);
                fprintf(stderr, "\"");
                if (m_doctypeData->m_hasSystemIdentifier) {
                    fprintf(stderr, " \"");
                    printString(m_doctypeData->m_systemIdentifier);
                    fprintf(stderr, "\"");
                }
            } else if (m_doctypeData->m_hasSystemIdentifier) {
                fprintf(stderr, " SYSTEM \"");
                printString(m_doctypeData->m_systemIdentifier);
                fprintf(stderr, "\"");
            }
            fprintf(stderr, ">");
            break;

        case XMLTokenTypes::CDATA:
            fprintf(stderr, "CDATA: ");
            fprintf(stderr, "<![CDATA[");
            printString(m_data);
            fprintf(stderr, "]]>");
            break;

        case XMLTokenTypes::StartTag:
            fprintf(stderr, "Start Tag: ");
            fprintf(stderr, "<");
            if (hasPrefix()) {
                printString(m_target);
                fprintf(stderr, ":");
            }
            printString(m_data);
            printAttrs();
            if (selfClosing())
                fprintf(stderr, "/");
            fprintf(stderr, ">");
            break;

        case XMLTokenTypes::EndTag:
            fprintf(stderr, "End Tag: ");
            fprintf(stderr, "</");
            if (hasPrefix()) {
                printString(m_target);
                fprintf(stderr, ":");
            }
            printString(m_data);
            fprintf(stderr, ">");
            break;

        case XMLTokenTypes::Comment:
            fprintf(stderr, "Comment: ");
            fprintf(stderr, "<!--");
            printString(m_data);
            fprintf(stderr, "-->");
            break;

        case XMLTokenTypes::Character:
            fprintf(stderr, "Characters: ");
            printString(m_data);
            break;

        case XMLTokenTypes::Entity:
            fprintf(stderr, "Entity: ");
            fprintf(stderr, "&");
            printString(m_data);
            fprintf(stderr, ";");
            break;

        case XMLTokenTypes::EndOfFile:
            fprintf(stderr, "EOF");
            break;
        }

        fprintf(stderr, "\n");
    }
#endif // NDEBUG

private:
    // AtomicXMLToken needs to steal some of our pointers from us
    // FIXME: add take* functions
    friend class AtomicXMLToken;

    // "target" for ProcessingInstruction, "prefix" for StartTag and EndTag
    DataVector m_target;

    // For XML Declaration
    OwnPtr<XMLDeclarationData> m_xmlDeclarationData;
};

class AtomicXMLToken : public AtomicMarkupTokenBase<XMLToken> {
    WTF_MAKE_NONCOPYABLE(AtomicXMLToken);
public:
    AtomicXMLToken(XMLToken& token)
        : AtomicMarkupTokenBase<XMLToken>(&token)
    {
        switch (m_type) {
        case XMLTokenTypes::ProcessingInstruction:
            m_name = AtomicString(token.target().data(), token.target().size());
            m_data = String(token.data().data(), token.data().size());
            break;
        case XMLTokenTypes::XMLDeclaration:
            m_xmlDeclarationData = token.m_xmlDeclarationData.release();
            break;
        case XMLTokenTypes::StartTag:
        case XMLTokenTypes::EndTag: {
            if (token.prefix().size())
                m_prefix = AtomicString(token.prefix().data(), token.prefix().size());
            initializeAttributes(token.attributes());
            break;
        }
        case XMLTokenTypes::CDATA:
            m_data = String(token.data().data(), token.data().size());
            break;
        case XMLTokenTypes::Entity:
            m_name = AtomicString(token.name().data(), token.name().size());
            break;
        default:
            break;
        }
    }

    AtomicXMLToken(XMLTokenTypes::Type type, AtomicString name, PassOwnPtr<NamedNodeMap> attributes = nullptr)
        : AtomicMarkupTokenBase<XMLToken>(type, name, attributes)
    {
    }

    const AtomicString& prefix() const
    {
        ASSERT(m_type == XMLTokenTypes::StartTag || m_type == XMLTokenTypes::EndTag);
        return m_prefix;
    }

    const AtomicString& target() const
    {
        ASSERT(m_type == XMLTokenTypes::ProcessingInstruction);
        return m_name;
    }

    const String& data() const
    {
        ASSERT(m_type == XMLTokenTypes::CDATA || m_type == XMLTokenTypes::ProcessingInstruction);
        return m_data;
    }

    WTF::Vector<UChar>& xmlVersion() const
    {
        ASSERT(m_type == XMLTokenTypes::XMLDeclaration);
        return m_xmlDeclarationData->m_version;
    }

    bool xmlStandalone() const
    {
        ASSERT(m_type == XMLTokenTypes::XMLDeclaration);
        return m_xmlDeclarationData->m_hasStandalone && m_xmlDeclarationData->m_standalone;
    }

private:
    // "prefix" for StartTag, EndTag
    AtomicString m_prefix;

    // For XML declaration
    OwnPtr<XMLToken::XMLDeclarationData> m_xmlDeclarationData;
};

}

#endif
