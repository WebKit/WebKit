/*
 * This file is part of the XSL implementation.
 *
 * Copyright (C) 2004 Apple Computer, Inc.
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

#ifndef _xslt_processorimpl_h_
#define _xslt_processorimpl_h_

#ifdef KHTML_XSLT

#include <libxml/parser.h>
#include <libxml/parserInternals.h>

#include <libxslt/transform.h>
#include <libxslt/documents.h>

#include <Shared.h>
#include "dom_stringimpl.h"
#include "xsl_stylesheetimpl.h"
#include <qstring.h>

#include <kxmlcore/HashMap.h>

class KHTMLView;

namespace DOM {

class NodeImpl;
class DocumentImpl;
class DocumentFragmentImpl;

class XSLTProcessorImpl : public Shared<XSLTProcessorImpl>
{
public:
    void setXSLStylesheet(XSLStyleSheetImpl *styleSheet) { m_stylesheet = styleSheet; }
    bool transformToString(NodeImpl *source, QString &resultMIMEType, QString &resultString, QString &resultEncoding);
    RefPtr<DocumentImpl> createDocumentFromSource(const QString &source, const QString &sourceEncoding, const QString &sourceMIMEType, NodeImpl *sourceNode, KHTMLView *view = 0);
    
    // DOM methods
    void importStylesheet(NodeImpl *style) { m_stylesheetRootNode = style; }
    RefPtr<DocumentFragmentImpl> transformToFragment(NodeImpl *source, DocumentImpl *ouputDoc);
    RefPtr<DocumentImpl> transformToDocument(NodeImpl *source);
    
    void setParameter(DOMStringImpl *namespaceURI, DOMStringImpl *localName, DOMStringImpl *value);
    RefPtr<DOMStringImpl> getParameter(DOMStringImpl *namespaceURI, DOMStringImpl *localName) const;
    void removeParameter(DOMStringImpl *namespaceURI, DOMStringImpl *localName);
    void clearParameters() { m_parameters.clear(); }

    void reset() { m_stylesheet = NULL; m_stylesheetRootNode = NULL;  m_parameters.clear(); }
    
public:
    // Only for libXSLT callbacks
    XSLStyleSheetImpl *xslStylesheet() const { return m_stylesheet.get(); }
    
    typedef HashMap<RefPtr<DOMStringImpl>, RefPtr<DOMStringImpl> > ParameterMap;

private:
    // Convert a libxml doc ptr to a KHTML DOM Document
    RefPtr<DocumentImpl> documentFromXMLDocPtr(xmlDocPtr resultDoc, xsltStylesheetPtr sheet, DocumentImpl *ownerDocument, bool sourceIsDocument);

    RefPtr<XSLStyleSheetImpl> m_stylesheet;
    RefPtr<NodeImpl> m_stylesheetRootNode;

    ParameterMap m_parameters;
};

}

#endif
#endif
