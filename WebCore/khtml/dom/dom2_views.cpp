/**
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 2001 Peter Kelly (pmk@post.com)
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

#include "dom/dom2_views.h"

#include "dom/dom_element.h"
#include "dom/dom_exception.h"
#include "dom/dom_doc.h"
#include "xml/dom_elementimpl.h"
#include "xml/dom2_viewsimpl.h"
#include "css/css_computedstyle.h"

using namespace DOM;


AbstractView::AbstractView()
{
    impl = 0;
}


AbstractView::AbstractView(const AbstractView &other)
{
    impl = other.impl;
    if (impl) impl->ref();
}


AbstractView::AbstractView(AbstractViewImpl *i)
{
    impl = i;
    if (impl) impl->ref();
}

AbstractView::~AbstractView()
{
    if (impl)
	impl->deref();
}

AbstractView &AbstractView::operator = (const AbstractView &other)
{
    if ( impl != other.impl ) {
    if(impl) impl->deref();
    impl = other.impl;
    if(impl) impl->ref();
    }
    return *this;
}

Document AbstractView::document() const
{
    if (!impl)
	throw DOMException(DOMException::INVALID_STATE_ERR);

    return impl->document();
}

CSSStyleDeclaration AbstractView::getComputedStyle(const Element &elt, const DOMString &pseudoElt)
{
    if (!impl)
	throw DOMException(DOMException::INVALID_STATE_ERR);

    return impl->getComputedStyle(static_cast<ElementImpl*>(elt.handle()),pseudoElt.implementation());
}


AbstractViewImpl *AbstractView::handle() const
{
    return impl;
}

bool AbstractView::isNull() const
{
    return (impl == 0);
}



