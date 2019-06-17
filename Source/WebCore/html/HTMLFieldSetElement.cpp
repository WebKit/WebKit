/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2010 Apple Inc. All rights reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
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
#include "HTMLFieldSetElement.h"

#include "ElementIterator.h"
#include "GenericCachedHTMLCollection.h"
#include "HTMLFormControlsCollection.h"
#include "HTMLLegendElement.h"
#include "HTMLNames.h"
#include "HTMLObjectElement.h"
#include "NodeRareData.h"
#include "RenderElement.h"
#include "ScriptDisallowedScope.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLFieldSetElement);

using namespace HTMLNames;

inline HTMLFieldSetElement::HTMLFieldSetElement(const QualifiedName& tagName, Document& document, HTMLFormElement* form)
    : HTMLFormControlElement(tagName, document, form)
{
    ASSERT(hasTagName(fieldsetTag));
}

HTMLFieldSetElement::~HTMLFieldSetElement()
{
    if (m_hasDisabledAttribute)
        document().removeDisabledFieldsetElement();
}

Ref<HTMLFieldSetElement> HTMLFieldSetElement::create(const QualifiedName& tagName, Document& document, HTMLFormElement* form)
{
    return adoptRef(*new HTMLFieldSetElement(tagName, document, form));
}

static void updateFromControlElementsAncestorDisabledStateUnder(HTMLElement& startNode, bool isDisabled)
{
    RefPtr<HTMLFormControlElement> control;
    if (is<HTMLFormControlElement>(startNode))
        control = &downcast<HTMLFormControlElement>(startNode);
    else
        control = Traversal<HTMLFormControlElement>::firstWithin(startNode);
    while (control) {
        control->setAncestorDisabled(isDisabled);
        // Don't call setAncestorDisabled(false) on form controls inside disabled fieldsets.
        if (is<HTMLFieldSetElement>(*control) && control->hasAttributeWithoutSynchronization(disabledAttr))
            control = Traversal<HTMLFormControlElement>::nextSkippingChildren(*control, &startNode);
        else
            control = Traversal<HTMLFormControlElement>::next(*control, &startNode);
    }
}

void HTMLFieldSetElement::disabledAttributeChanged()
{
    bool hasDisabledAttribute = hasAttributeWithoutSynchronization(disabledAttr);
    if (m_hasDisabledAttribute != hasDisabledAttribute) {
        m_hasDisabledAttribute = hasDisabledAttribute;
        if (hasDisabledAttribute)
            document().addDisabledFieldsetElement();
        else
            document().removeDisabledFieldsetElement();
    }

    HTMLFormControlElement::disabledAttributeChanged();
}

void HTMLFieldSetElement::disabledStateChanged()
{
    // This element must be updated before the style of nodes in its subtree gets recalculated.
    HTMLFormControlElement::disabledStateChanged();

    if (disabledByAncestorFieldset())
        return;

    bool thisFieldsetIsDisabled = hasAttributeWithoutSynchronization(disabledAttr);
    bool hasSeenFirstLegendElement = false;
    for (RefPtr<HTMLElement> control = Traversal<HTMLElement>::firstChild(*this); control; control = Traversal<HTMLElement>::nextSibling(*control)) {
        if (!hasSeenFirstLegendElement && is<HTMLLegendElement>(*control)) {
            hasSeenFirstLegendElement = true;
            updateFromControlElementsAncestorDisabledStateUnder(*control, false /* isDisabled */);
            continue;
        }
        updateFromControlElementsAncestorDisabledStateUnder(*control, thisFieldsetIsDisabled);
    }
}

void HTMLFieldSetElement::childrenChanged(const ChildChange& change)
{
    HTMLFormControlElement::childrenChanged(change);
    if (!hasAttributeWithoutSynchronization(disabledAttr))
        return;

    RefPtr<HTMLLegendElement> legend = Traversal<HTMLLegendElement>::firstChild(*this);
    if (!legend)
        return;

    // We only care about the first legend element (in which form controls are not disabled by this element) changing here.
    updateFromControlElementsAncestorDisabledStateUnder(*legend, false /* isDisabled */);
    while ((legend = Traversal<HTMLLegendElement>::nextSibling(*legend)))
        updateFromControlElementsAncestorDisabledStateUnder(*legend, true);
}

void HTMLFieldSetElement::didMoveToNewDocument(Document& oldDocument, Document& newDocument)
{
    ASSERT_WITH_SECURITY_IMPLICATION(&document() == &newDocument);
    HTMLFormControlElement::didMoveToNewDocument(oldDocument, newDocument);
    if (m_hasDisabledAttribute) {
        oldDocument.removeDisabledFieldsetElement();
        newDocument.addDisabledFieldsetElement();
    }
}

bool HTMLFieldSetElement::matchesValidPseudoClass() const
{
    return m_invalidDescendants.isEmpty();
}

bool HTMLFieldSetElement::matchesInvalidPseudoClass() const
{
    return !m_invalidDescendants.isEmpty();
}

bool HTMLFieldSetElement::supportsFocus() const
{
    return HTMLElement::supportsFocus();
}

const AtomString& HTMLFieldSetElement::formControlType() const
{
    static NeverDestroyed<const AtomString> fieldset("fieldset", AtomString::ConstructFromLiteral);
    return fieldset;
}

RenderPtr<RenderElement> HTMLFieldSetElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return RenderElement::createFor(*this, WTFMove(style), RenderElement::OnlyCreateBlockAndFlexboxRenderers);
}

HTMLLegendElement* HTMLFieldSetElement::legend() const
{
    return const_cast<HTMLLegendElement*>(childrenOfType<HTMLLegendElement>(*this).first());
}

Ref<HTMLCollection> HTMLFieldSetElement::elements()
{
    return ensureRareData().ensureNodeLists().addCachedCollection<GenericCachedHTMLCollection<CollectionTypeTraits<FieldSetElements>::traversalType>>(*this, FieldSetElements);
}

void HTMLFieldSetElement::addInvalidDescendant(const HTMLFormControlElement& invalidFormControlElement)
{
    ASSERT_WITH_MESSAGE(!is<HTMLFieldSetElement>(invalidFormControlElement), "FieldSet are never candidates for constraint validation.");
    ASSERT(static_cast<const Element&>(invalidFormControlElement).matchesInvalidPseudoClass());
    ASSERT_WITH_MESSAGE(!m_invalidDescendants.contains(&invalidFormControlElement), "Updating the fieldset on validity change is not an efficient operation, it should only be done when necessary.");

    if (m_invalidDescendants.isEmpty())
        invalidateStyleForSubtree();
    m_invalidDescendants.add(&invalidFormControlElement);
}

void HTMLFieldSetElement::removeInvalidDescendant(const HTMLFormControlElement& formControlElement)
{
    ASSERT_WITH_MESSAGE(!is<HTMLFieldSetElement>(formControlElement), "FieldSet are never candidates for constraint validation.");
    ASSERT_WITH_MESSAGE(m_invalidDescendants.contains(&formControlElement), "Updating the fieldset on validity change is not an efficient operation, it should only be done when necessary.");

    m_invalidDescendants.remove(&formControlElement);
    if (m_invalidDescendants.isEmpty())
        invalidateStyleForSubtree();
}

} // namespace
