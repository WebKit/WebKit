/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
 */

#include "config.h"
#include "VisitedLinkState.h"

#include "ElementIterator.h"
#include "FrameDestructionObserverInlines.h"
#include "HTMLAnchorElementInlines.h"
#include "LocalFrame.h"
#include "Page.h"
#include "SVGAElement.h"
#include "SVGElementTypeHelpers.h"
#include "SVGNames.h"
#include "TypedElementDescendantIteratorInlines.h"
#include "VisitedLinkStore.h"
#include "XLinkNames.h"

namespace WebCore {

using namespace HTMLNames;

inline static const AtomString linkAttribute(const Element& element)
{
    if (!element.isLink())
        return nullAtom();
    if (element.isHTMLElement())
        return element.attributeWithoutSynchronization(HTMLNames::hrefAttr);
    if (element.isSVGElement())
        return element.getAttribute(SVGNames::hrefAttr, XLinkNames::hrefAttr);
    return nullAtom();
}

VisitedLinkState::VisitedLinkState(Document& document)
    : m_document(document)
{
}

void VisitedLinkState::invalidateStyleForAllLinks()
{
    if (m_linksCheckedForVisitedState.isEmpty())
        return;
    Ref document = m_document.get();
    for (Ref element : descendantsOfType<Element>(document.get())) {
        if (element->isLink())
            element->invalidateStyleForSubtree();
    }
}

inline static std::optional<SharedStringHash> linkHashForElement(const Element& element)
{
    if (auto anchor = dynamicDowncast<HTMLAnchorElement>(element))
        return anchor->visitedLinkHash();
    if (auto anchor = dynamicDowncast<SVGAElement>(element))
        return anchor->visitedLinkHash();
    return std::nullopt;
}

void VisitedLinkState::invalidateStyleForLink(SharedStringHash linkHash)
{
    if (!m_linksCheckedForVisitedState.contains(linkHash))
        return;
    Ref document = m_document.get();
    for (Ref element : descendantsOfType<Element>(document.get())) {
        if (element->isLink() && linkHashForElement(element) == linkHash)
            element->invalidateStyleForSubtree();
    }
}

InsideLink VisitedLinkState::determineLinkStateSlowCase(const Element& element)
{
    ASSERT(element.isLink());

    auto attribute = linkAttribute(element);
    if (attribute.isNull())
        return InsideLink::NotInside;

    auto hashIfFound = linkHashForElement(element);

    if (!hashIfFound)
        return attribute.isEmpty() ? InsideLink::InsideVisited : InsideLink::InsideUnvisited;

    auto hash = *hashIfFound;

    // An empty href (hash==0) refers to the document itself which is always visited. It is useful to check this explicitly so
    // that visited links can be tested in platform independent manner, without explicit support in the test harness.
    if (!hash)
        return InsideLink::InsideVisited;

    RefPtr frame = element.document().frame();
    if (!frame)
        return InsideLink::InsideUnvisited;

    RefPtr page = frame->page();
    if (!page)
        return InsideLink::InsideUnvisited;

    m_linksCheckedForVisitedState.add(hash);

    if (!page->visitedLinkStore().isLinkVisited(*page, hash, element.document().baseURL(), attribute))
        return InsideLink::InsideUnvisited;

    return InsideLink::InsideVisited;
}

} // namespace WebCore
