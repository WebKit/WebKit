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

#include "config.h"
#include "dom2_viewsimpl.h"

#include "css/css_computedstyle.h"
#include "dom_elementimpl.h"
#include "DocumentImpl.h"
#include "cssstyleselector.h"

namespace DOM {

AbstractViewImpl::AbstractViewImpl(DocumentImpl *_document)
{
    m_document = _document;
}

AbstractViewImpl::~AbstractViewImpl()
{
}

CSSStyleDeclarationImpl *AbstractViewImpl::getComputedStyle(ElementImpl *elt, DOMStringImpl *pseudoElt)
{
    // FIXME: This should work even if we do not have a renderer.
    // FIXME: This needs to work with pseudo elements.
    if (!elt || !elt->renderer())
        return 0;

    return new CSSComputedStyleDeclarationImpl(elt);
}

RefPtr<CSSRuleListImpl> AbstractViewImpl::getMatchedCSSRules(ElementImpl* elt, DOMStringImpl* pseudoElt, bool authorOnly)
{
    if (pseudoElt && pseudoElt->l)
        return m_document->styleSelector()->pseudoStyleRulesForElement(elt, pseudoElt, authorOnly);
    return m_document->styleSelector()->styleRulesForElement(elt, authorOnly);
}

}
