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
#ifndef _xsl_stylesheetimpl_h_
#define _xsl_stylesheetimpl_h_

#ifdef KHTML_XSLT

#include "css/css_stylesheetimpl.h"

#include <libxml/parser.h>
#include <libxml/parserInternals.h>

#include <libxslt/transform.h>

namespace DOM {

class XSLStyleSheetImpl : public StyleSheetImpl
{
public:
    XSLStyleSheetImpl(DOM::NodeImpl *parentNode, DOM::DOMString href = DOMString());
    XSLStyleSheetImpl(XSLStyleSheetImpl *parentSheet, DOM::DOMString href = DOMString());
    ~XSLStyleSheetImpl();
    
    virtual bool isXSLStyleSheet() const { return true; }
    virtual DOM::DOMString type() const { return "text/xml"; }

    virtual bool parseString(const DOMString &string, bool strict = true);
    
    virtual bool isLoading();
    virtual void checkLoaded();
    
    khtml::DocLoader *docLoader();
    DocumentImpl* ownerDocument() { return m_ownerDocument; }

    xmlDocPtr document() { return m_stylesheetDoc; }
    void clearDocument() { m_stylesheetDoc = 0; }

protected:
    DocumentImpl* m_ownerDocument;
    xmlDocPtr m_stylesheetDoc;
};

}
#endif
#endif
