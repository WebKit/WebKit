/*
 * This file is part of the XSL implementation.
 *
 * Copyright (C) 2004, 2007, 2008 Apple, Inc. All rights reserved.
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

#pragma once

#if ENABLE(XSLT)

#include "Node.h"
#include "XSLStyleSheet.h"
#include <libxml/parserInternals.h>
#include <libxslt/documents.h>
#include <wtf/HashMap.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

class Frame;
class Document;
class DocumentFragment;

class XSLTProcessor : public RefCounted<XSLTProcessor> {
public:
    static Ref<XSLTProcessor> create() { return adoptRef(*new XSLTProcessor); }
    ~XSLTProcessor();

    void setXSLStyleSheet(RefPtr<XSLStyleSheet>&& styleSheet) { m_stylesheet = WTFMove(styleSheet); }
    bool transformToString(Node& source, String& resultMIMEType, String& resultString, String& resultEncoding);
    Ref<Document> createDocumentFromSource(const String& source, const String& sourceEncoding, const String& sourceMIMEType, Node* sourceNode, Frame* frame);
    
    // DOM methods
    void importStylesheet(RefPtr<Node>&& style)
    {
        if (style)
            m_stylesheetRootNode = WTFMove(style);
    }
    RefPtr<DocumentFragment> transformToFragment(Node* source, Document* ouputDoc);
    RefPtr<Document> transformToDocument(Node* source);
    
    void setParameter(const String& namespaceURI, const String& localName, const String& value);
    String getParameter(const String& namespaceURI, const String& localName) const;
    void removeParameter(const String& namespaceURI, const String& localName);
    void clearParameters() { m_parameters.clear(); }

    void reset();

    static void parseErrorFunc(void* userData, xmlError*);
    static void genericErrorFunc(void* userData, const char* msg, ...);
    
    // Only for libXSLT callbacks
    XSLStyleSheet* xslStylesheet() const { return m_stylesheet.get(); }

    typedef HashMap<String, String> ParameterMap;

private:
    XSLTProcessor() = default;

    RefPtr<XSLStyleSheet> m_stylesheet;
    RefPtr<Node> m_stylesheetRootNode;
    ParameterMap m_parameters;
};

} // namespace WebCore

#endif // ENABLE(XSLT)
