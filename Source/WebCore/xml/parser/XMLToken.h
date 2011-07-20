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

#include "MarkupTokenBase.h"

namespace WebCore {

class XMLTokenTypes {
public:
    enum Type {
        Uninitialized,
        ProcessingInstruction,
        XMLDeclaration,
        DOCTYPE,
        AttributeListDeclaration,
        ElementDeclaration,
        EntityDeclaration,
        CDATA,
        StartTag,
        EndTag,
        Comment,
        Character,
        Entity,
        EndOfFile,
    };
};

class XMLToken : public MarkupTokenBase<XMLTokenTypes> {
    WTF_MAKE_NONCOPYABLE(XMLToken);
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual void clear()
    {
        MarkupTokenBase<XMLTokenTypes>::clear();
        m_target.clear();
    }
    
    void appendToName(UChar character)
    {
        ASSERT(m_type == XMLTokenTypes::StartTag || m_type == XMLTokenTypes::EndTag || m_type == XMLTokenTypes::DOCTYPE || m_type == XMLTokenTypes::Entity);
        MarkupTokenBase<XMLTokenTypes>::appendToName(character);
    }
    
    const DataVector& name() const
    {
        ASSERT(m_type == XMLTokenTypes::StartTag || m_type == XMLTokenTypes::EndTag || m_type == XMLTokenTypes::DOCTYPE || m_type == XMLTokenTypes::Entity);
        return MarkupTokenBase<XMLTokenTypes>::name();
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

#ifndef NDEBUG
    void print() const
    {
        switch (m_type) {
        case XMLTokenTypes::Uninitialized:
            printf("UNITIALIZED");
            break;

        case XMLTokenTypes::ProcessingInstruction:
            printf("ProcessingInstruction: ");
            printf("<?");
            printString(m_target);
            printf(" ");
            printString(m_data);
            printf("?>");
            break;

        case XMLTokenTypes::XMLDeclaration:
            printf("XML Declaration: ");
            printf("<?xml version=\"");
            ASSERT(m_xmlDeclarationData);
            printString(m_xmlDeclarationData->m_version);
            printf("\"");
            if (m_xmlDeclarationData->m_hasEncoding) {
                printf(" encoding=\"");
                printString(m_xmlDeclarationData->m_encoding);
                printf("\"");
            }
            if (m_xmlDeclarationData->m_hasStandalone)
                printf(" standalone=\"%s\"", m_xmlDeclarationData->m_standalone ? "yes" : "no");
            printf("?>");
            break;

        case XMLTokenTypes::DOCTYPE:
            printf("DOCTYPE: ");
            ASSERT(m_doctypeData);
            printf("<!DOCTYPE ");
            printString(m_data);
            if (m_doctypeData->m_hasPublicIdentifier) {
                printf(" PUBLIC \"");
                printString(m_doctypeData->m_publicIdentifier);
                printf("\"");
                if (m_doctypeData->m_hasSystemIdentifier) {
                    printf(" \"");
                    printString(m_doctypeData->m_systemIdentifier);
                    printf("\"");
                }
            } else if (m_doctypeData->m_hasSystemIdentifier) {
                printf(" SYSTEM \"");
                printString(m_doctypeData->m_systemIdentifier);
                printf("\"");
            }
            printf(">");
            break;

        case XMLTokenTypes::AttributeListDeclaration:
            printf("Attribute List: ");
            printf("<!ATTLIST>");
            break;

        case XMLTokenTypes::ElementDeclaration:
            printf("Element Declaration: ");
            printf("<!ELEMENT>");
            break;

        case XMLTokenTypes::EntityDeclaration:
            printf("Entity Declaration: ");
            printf("<!ENTITY>");
            break;

        case XMLTokenTypes::CDATA:
            printf("CDATA: ");
            printf("<![CDATA[");
            printString(m_data);
            printf("]]>");
            break;

        case XMLTokenTypes::StartTag:
            printf("Start Tag: ");
            printf("<");
            printString(m_data);
            printAttrs();
            if (selfClosing())
                printf("/");
            printf(">");
            break;

        case XMLTokenTypes::EndTag:
            printf("End Tag: ");
            printf("</");
            printString(m_data);
            printf(">");
            break;

        case XMLTokenTypes::Comment:
            printf("Comment: ");
            printf("<!--");
            printString(m_data);
            printf("-->");
            break;

        case XMLTokenTypes::Character:
            printf("Characters: ");
            printString(m_data);
            break;

        case XMLTokenTypes::Entity:
            printf("Entity: ");
            printf("&");
            printString(m_data);
            printf(";");
            break;

        case XMLTokenTypes::EndOfFile:
            printf("EOF");
            break;
        }

        printf("\n");
    }
#endif // NDEBUG

private:
    typedef BaseDoctypeData DoctypeData;

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

    // "target" for ProcessingInstruction
    DataVector m_target;
    
    // For XML Declaration
    OwnPtr<XMLDeclarationData> m_xmlDeclarationData;
};

}

#endif
