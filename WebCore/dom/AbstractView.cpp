/**
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2004, 2006 Apple Computer, Inc.
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
#include "AbstractView.h"

#include "Document.h"
#include "CSSComputedStyleDeclaration.h"
#include "cssstyleselector.h"
#include "dom_elementimpl.h"

namespace WebCore {

AbstractView::AbstractView(Document *_document)
{
    m_document = _document;
}

AbstractView::~AbstractView()
{
}

CSSStyleDeclaration *AbstractView::getComputedStyle(Element *elt, StringImpl *pseudoElt)
{
    // FIXME: This should work even if we do not have a renderer.
    // FIXME: This needs to work with pseudo elements.
    if (!elt || !elt->renderer())
        return 0;

    return new CSSComputedStyleDeclaration(elt);
}

RefPtr<CSSRuleList> AbstractView::getMatchedCSSRules(Element* elt, StringImpl* pseudoElt, bool authorOnly)
{
    if (pseudoElt && pseudoElt->length())
        return m_document->styleSelector()->pseudoStyleRulesForElement(elt, pseudoElt, authorOnly);
    return m_document->styleSelector()->styleRulesForElement(elt, authorOnly);
}

}
