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

#include <qstring.h>

namespace DOM {

class XSLStyleSheetImpl;
class DocumentImpl;
    
class XSLTProcessorImpl
{
public:
    // Constructors
    XSLTProcessorImpl(XSLStyleSheetImpl* stylesheet, DocumentImpl* source);
    ~XSLTProcessorImpl();
    
    // Method for transforming a source document into a result document.
    DocumentImpl* transformDocument(DocumentImpl* sourceDoc);

    // Convert a libxml doc ptr to a KHTML DOM Document
    DocumentImpl* documentFromXMLDocPtr(xmlDocPtr resultDoc, xsltStylesheetPtr sheet);
    
    // Helpers
    void addToResult(const char* buffer, int len);
    
    XSLStyleSheetImpl *stylesheet() { return m_stylesheet; }
    DocumentImpl *sourceDocument() { return m_sourceDocument; }
    
protected:
    XSLStyleSheetImpl* m_stylesheet;
    QString m_resultOutput;
    DocumentImpl* m_sourceDocument;
};

}
#endif
#endif
