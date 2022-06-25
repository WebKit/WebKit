/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#pragma once

#include "Element.h"
#include "markup.h"
#include <wtf/HashMap.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

class Attribute;
class DocumentType;
class Element;
class Node;
class Range;

typedef HashMap<AtomString, AtomStringImpl*> Namespaces;

enum EntityMask {
    EntityAmp = 0x0001,
    EntityLt = 0x0002,
    EntityGt = 0x0004,
    EntityQuot = 0x0008,
    EntityNbsp = 0x0010,
    EntityTab = 0x0020,
    EntityLineFeed = 0x0040,
    EntityCarriageReturn = 0x0080,

    // Non-breaking space needs to be escaped in innerHTML for compatibility reason. See http://trac.webkit.org/changeset/32879
    // However, we cannot do this in a XML document because it does not have the entity reference defined (See the bug 19215).
    EntityMaskInCDATA = 0,
    EntityMaskInPCDATA = EntityAmp | EntityLt | EntityGt,
    EntityMaskInHTMLPCDATA = EntityMaskInPCDATA | EntityNbsp,
    EntityMaskInAttributeValue = EntityAmp | EntityLt | EntityGt | EntityQuot | EntityTab | EntityLineFeed | EntityCarriageReturn,
    EntityMaskInHTMLAttributeValue = EntityAmp | EntityQuot | EntityNbsp,
};

class MarkupAccumulator {
    WTF_MAKE_NONCOPYABLE(MarkupAccumulator);
public:
    MarkupAccumulator(Vector<Node*>*, ResolveURLs, SerializationSyntax = SerializationSyntax::HTML);
    virtual ~MarkupAccumulator();

    String serializeNodes(Node& targetNode, SerializedNodes, Vector<QualifiedName>* tagNamesToSkip = nullptr);

    static void appendCharactersReplacingEntities(StringBuilder&, const String&, unsigned, unsigned, EntityMask);

protected:
    unsigned length() const { return m_markup.length(); }
    bool isAllASCII() const { return m_markup.isAllASCII(); }

    StringBuilder takeMarkup();

    template<typename ...StringTypes> void append(StringTypes&&... strings) { m_markup.append(std::forward<StringTypes>(strings)...); }

    void startAppendingNode(const Node&, Namespaces* = nullptr);
    void endAppendingNode(const Node&);

    virtual void appendStartTag(StringBuilder&, const Element&, Namespaces*);
    virtual void appendEndTag(StringBuilder&, const Element&);
    virtual void appendCustomAttributes(StringBuilder&, const Element&, Namespaces*);
    virtual void appendText(StringBuilder&, const Text&);

    void appendOpenTag(StringBuilder&, const Element&, Namespaces*);
    void appendCloseTag(StringBuilder&, const Element&);

    void appendNonElementNode(StringBuilder&, const Node&, Namespaces*);

    static void appendAttributeValue(StringBuilder&, const String&, bool isSerializingHTML);
    void appendAttribute(StringBuilder&, const Element&, const Attribute&, Namespaces*);

    EntityMask entityMaskForText(const Text&) const;

    Vector<Node*>* const m_nodes;

private:
    void appendNamespace(StringBuilder&, const AtomString& prefix, const AtomString& namespaceURI, Namespaces&, bool allowEmptyDefaultNS = false);
    String resolveURLIfNeeded(const Element&, const String&) const;
    void appendQuotedURLAttributeValue(StringBuilder&, const Element&, const Attribute&);
    void serializeNodesWithNamespaces(Node& targetNode, SerializedNodes, const Namespaces*, Vector<QualifiedName>* tagNamesToSkip);
    bool inXMLFragmentSerialization() const { return m_serializationSyntax == SerializationSyntax::XML; }
    void generateUniquePrefix(QualifiedName&, const Namespaces&);
    QualifiedName xmlAttributeSerialization(const Attribute&, Namespaces*);

    StringBuilder m_markup;
    const ResolveURLs m_resolveURLs;
    const SerializationSyntax m_serializationSyntax;
    unsigned m_prefixLevel { 0 };
};

inline void MarkupAccumulator::endAppendingNode(const Node& node)
{
    if (is<Element>(node))
        appendEndTag(m_markup, downcast<Element>(node));
}

} // namespace WebCore
