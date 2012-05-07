/*
 * Copyright (C) 2010, 2011 Nokia Corporation and/or its subsidiary(-ies)
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

#include "config.h"
#include "HTMLDetailsElement.h"

#if ENABLE(DETAILS)

#include "ElementShadow.h"
#include "HTMLContentElement.h"
#include "HTMLNames.h"
#include "HTMLSummaryElement.h"
#include "LocalizedStrings.h"
#include "MouseEvent.h"
#include "NodeRenderingContext.h"
#include "RenderBlock.h"
#include "ShadowRoot.h"
#include "Text.h"

namespace WebCore {

using namespace HTMLNames;

static const AtomicString& summaryQuerySelector()
{
    DEFINE_STATIC_LOCAL(AtomicString, selector, ("summary:first-of-type"));
    return selector;
};

class DetailsContentElement : public HTMLContentElement {
public:
    static PassRefPtr<DetailsContentElement> create(Document*);

private:
    DetailsContentElement(Document* document)
        : HTMLContentElement(HTMLNames::webkitShadowContentTag, document)
    {
    }
};

PassRefPtr<DetailsContentElement> DetailsContentElement::create(Document* document)
{
    return adoptRef(new DetailsContentElement(document));
}

class DetailsSummaryElement : public HTMLContentElement {
public:
    static PassRefPtr<DetailsSummaryElement> create(Document*);

    Element* fallbackSummary()
    {
        ASSERT(firstChild() && firstChild()->hasTagName(summaryTag));
        return toElement(firstChild());
    }

private:
    DetailsSummaryElement(Document* document)
        : HTMLContentElement(HTMLNames::webkitShadowContentTag, document)
    {
        setSelect(summaryQuerySelector());
    }
};

PassRefPtr<DetailsSummaryElement> DetailsSummaryElement::create(Document* document)
{
    RefPtr<HTMLSummaryElement> defaultSummary = HTMLSummaryElement::create(summaryTag, document);
    defaultSummary->appendChild(Text::create(document, defaultDetailsSummaryText()), ASSERT_NO_EXCEPTION);

    RefPtr<DetailsSummaryElement> elem = adoptRef(new DetailsSummaryElement(document));
    elem->appendChild(defaultSummary);
    return elem.release();
}

PassRefPtr<HTMLDetailsElement> HTMLDetailsElement::create(const QualifiedName& tagName, Document* document)
{
    RefPtr<HTMLDetailsElement> elem = adoptRef(new HTMLDetailsElement(tagName, document));
    elem->createShadowSubtree();

    return elem.release();
}

HTMLDetailsElement::HTMLDetailsElement(const QualifiedName& tagName, Document* document)
    : HTMLElement(tagName, document)
    , m_isOpen(false)
{
    ASSERT(hasTagName(detailsTag));
}

RenderObject* HTMLDetailsElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    return new (arena) RenderBlock(this);
}

void HTMLDetailsElement::createShadowSubtree()
{
    ASSERT(!hasShadowRoot());

    RefPtr<ShadowRoot> root = ShadowRoot::create(this, ShadowRoot::CreatingUserAgentShadowRoot);
    root->appendChild(DetailsSummaryElement::create(document()), ASSERT_NO_EXCEPTION, true);
    root->appendChild(DetailsContentElement::create(document()), ASSERT_NO_EXCEPTION, true);
}

Element* HTMLDetailsElement::findMainSummary() const
{
    for (Node* child = firstChild(); child; child = child->nextSibling()) {
        if (child->hasTagName(summaryTag))
            return toElement(child);
    }

    return static_cast<DetailsSummaryElement*>(shadow()->oldestShadowRoot()->firstChild())->fallbackSummary();
}

void HTMLDetailsElement::parseAttribute(Attribute* attr)
{
    if (attr->name() == openAttr) {
        bool oldValue = m_isOpen;
        m_isOpen =  !attr->value().isNull();
        if (oldValue != m_isOpen)
            reattachIfAttached();
    } else
        HTMLElement::parseAttribute(attr);
}

bool HTMLDetailsElement::childShouldCreateRenderer(const NodeRenderingContext& childContext) const
{
    if (!childContext.isOnEncapsulationBoundary())
        return false;

    if (m_isOpen)
        return HTMLElement::childShouldCreateRenderer(childContext);

    if (!childContext.node()->hasTagName(summaryTag))
        return false;

    return childContext.node() == findMainSummary() && HTMLElement::childShouldCreateRenderer(childContext);
}

void HTMLDetailsElement::toggleOpen()
{
    setAttribute(openAttr, m_isOpen ? nullAtom : emptyAtom);
}

}

#endif
