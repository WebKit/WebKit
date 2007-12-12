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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef ProcessingInstruction_h
#define ProcessingInstruction_h

#include "CachedResourceClient.h"
#include "ContainerNode.h"

namespace WebCore {

class StyleSheet;
class CSSStyleSheet;

class ProcessingInstruction : public ContainerNode, private CachedResourceClient
{
public:
    ProcessingInstruction(Document*);
    ProcessingInstruction(Document*, const String& target, const String& data);
    virtual ~ProcessingInstruction();

    // DOM methods & attributes for Notation
    String target() const { return m_target; }
    String data() const { return m_data; }
    void setData(const String&, ExceptionCode&);

    virtual String nodeName() const;
    virtual NodeType nodeType() const;
    virtual String nodeValue() const;
    virtual void setNodeValue(const String&, ExceptionCode&);
    virtual PassRefPtr<Node> cloneNode(bool deep);
    virtual bool childTypeAllowed(NodeType);
    virtual bool offsetInCharacters() const;
    virtual int maxCharacterOffset() const;

    // Other methods (not part of DOM)
    String localHref() const { return m_localHref; }
    StyleSheet* sheet() const { return m_sheet.get(); }
    bool checkStyleSheet();
    virtual void setCSSStyleSheet(const String& url, const String& charset, const String& sheet);
#if ENABLE(XSLT)
    virtual void setXSLStyleSheet(const String& url, const String& sheet);
#endif
    void setCSSStyleSheet(CSSStyleSheet*);
    bool isLoading() const;
    virtual bool sheetLoaded();
    virtual String toString() const;

#if ENABLE(XSLT)
    bool isXSL() const { return m_isXSL; }
#endif

private:
    void parseStyleSheet(const String& sheet);

    String m_target;
    String m_data;
    String m_localHref;
    CachedResource* m_cachedSheet;
    RefPtr<StyleSheet> m_sheet;
    bool m_loading;
#if ENABLE(XSLT)
    bool m_isXSL;
#endif
};

} //namespace

#endif
