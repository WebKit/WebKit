/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2000 Peter Kelly (pmk@post.com)
 * Copyright (C) 2006 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef DOM_XmlImpl_h_
#define DOM_XmlImpl_h_

#include "CachedObjectClient.h"
#include "ContainerNode.h"

namespace WebCore {
    class CachedCSSStyleSheet;
}

namespace WebCore {

class Document;
class String;
class ProcessingInstruction;
class StyleSheet;

class Entity : public ContainerNode
{
public:
    Entity(Document*);
    Entity(Document*, const String& name);
    Entity(Document*, const String& publicId, const String& systemId, const String& notationName);

    // DOM methods & attributes for Entity
    String publicId() const { return m_publicId.get(); }
    String systemId() const { return m_systemId.get(); }
    String notationName() const { return m_notationName.get(); }

    virtual String nodeName() const;
    virtual NodeType nodeType() const;
    virtual PassRefPtr<Node> cloneNode(bool deep);
    virtual bool childTypeAllowed(NodeType);
    virtual String toString() const;

private:
    RefPtr<StringImpl> m_publicId;
    RefPtr<StringImpl> m_systemId;
    RefPtr<StringImpl> m_notationName;
    RefPtr<StringImpl> m_name;
};

class EntityReference : public ContainerNode
{
public:
    EntityReference(Document*);
    EntityReference(Document*, StringImpl* entityName);

    virtual String nodeName() const;
    virtual NodeType nodeType() const;
    virtual PassRefPtr<Node> cloneNode(bool deep);
    virtual bool childTypeAllowed(NodeType);
    virtual String toString() const;

private:
    RefPtr<StringImpl> m_entityName;
};

class Notation : public ContainerNode
{
public:
    Notation(Document*);
    Notation(Document*, const String& name, const String& publicId, const String& systemId);

    // DOM methods & attributes for Notation
    String publicId() const { return m_publicId.get(); }
    String systemId() const { return m_systemId. get(); }

    virtual String nodeName() const;
    virtual NodeType nodeType() const;
    virtual PassRefPtr<Node> cloneNode(bool deep);
    virtual bool childTypeAllowed(NodeType);

private:
    RefPtr<StringImpl> m_name;
    RefPtr<StringImpl> m_publicId;
    RefPtr<StringImpl> m_systemId;
};


class ProcessingInstruction : public ContainerNode, private CachedObjectClient
{
public:
    ProcessingInstruction(Document*);
    ProcessingInstruction(Document*, const String& target, const String& data);
    virtual ~ProcessingInstruction();

    // DOM methods & attributes for Notation
    String target() const { return m_target.get(); }
    String data() const { return m_data.get(); }
    void setData(const String &data, ExceptionCode&);

    virtual String nodeName() const;
    virtual NodeType nodeType() const;
    virtual String nodeValue() const;
    virtual void setNodeValue(const String& nodeValue, ExceptionCode&);
    virtual PassRefPtr<Node> cloneNode(bool deep);
    virtual bool childTypeAllowed(NodeType);
    virtual bool offsetInCharacters() const;

    // Other methods (not part of DOM)
    String localHref() const { return m_localHref.get(); }
    StyleSheet* sheet() const { return m_sheet.get(); }
    bool checkStyleSheet();
    virtual void setStyleSheet(const String& URL, const String& sheet);
    void setStyleSheet(StyleSheet*);
    bool isLoading() const;
    void sheetLoaded();
    virtual String toString() const;

#ifdef KHTML_XSLT
    bool isXSL() const { return m_isXSL; }
#endif

private:
    RefPtr<StringImpl> m_target;
    RefPtr<StringImpl> m_data;
    RefPtr<StringImpl> m_localHref;
    CachedObject* m_cachedSheet;
    RefPtr<StyleSheet> m_sheet;
    bool m_loading;
#ifdef KHTML_XSLT
    bool m_isXSL;
#endif
};

} //namespace

#endif
