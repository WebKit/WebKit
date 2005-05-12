/*
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 2001 Peter Kelly (pmk@post.com)
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

#ifndef _DOM_ViewsImpl_h_
#define _DOM_ViewsImpl_h_

#include "css/css_valueimpl.h"
#include "misc/shared.h"
#include "misc/main_thread_malloc.h"

namespace DOM {

class DocumentImpl;
class CSSStyleDeclarationImpl;
class ElementImpl;
class DOMStringImpl;

// Introduced in DOM Level 2:
class AbstractViewImpl : public khtml::Shared<AbstractViewImpl>
{
public:
    AbstractViewImpl(DocumentImpl *_document);
    ~AbstractViewImpl();

    MAIN_THREAD_ALLOCATED;
    
    DocumentImpl *document() const { return m_document; }
    CSSStyleDeclarationImpl *getComputedStyle(ElementImpl *elt, DOMStringImpl *pseudoElt);
protected:
    DocumentImpl *m_document;
};

}; //namespace
#endif
