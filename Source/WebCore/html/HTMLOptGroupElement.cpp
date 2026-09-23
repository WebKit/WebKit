/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2026 Apple Inc. All rights reserved.
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
#include "HTMLOptGroupElement.h"

#include "Document.h"
#include "ElementAncestorIteratorInlines.h"
#include "ElementIterator.h"
#include "HTMLDivElement.h"
#include "HTMLLegendElement.h"
#include "HTMLNames.h"
#include "HTMLOptionElement.h"
#include "HTMLSelectElement.h"
#include "HTMLSlotElement.h"
#include "PseudoClassChangeInvalidation.h"
#include "NodeRenderStyle.h"
#include "ScriptDisallowedScope.h"
#include "Settings.h"
#include "StyleResolver.h"
#include "TypedElementDescendantIteratorInlines.h"
#include "UserAgentParts.h"
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(HTMLOptGroupElement);

using namespace HTMLNames;

inline HTMLOptGroupElement::HTMLOptGroupElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
{
    ASSERT(hasTagName(optgroupTag));
}

Ref<HTMLOptGroupElement> HTMLOptGroupElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new HTMLOptGroupElement(tagName, document));
}

void HTMLOptGroupElement::invalidateShadowTree()
{
    if (m_shadowTreeNeedsUpdate)
        return;

    if (!document().settings().htmlEnhancedSelectEnabled())
        return;

    m_shadowTreeNeedsUpdate = true;
    if (isConnected())
        protect(document())->addElementWithPendingUserAgentShadowTreeUpdate(*this);
}

void HTMLOptGroupElement::updateUserAgentShadowTree()
{
    if (!m_shadowTreeNeedsUpdate)
        return;

    m_shadowTreeNeedsUpdate = false;
    protect(document())->removeElementWithPendingUserAgentShadowTreeUpdate(*this);

    if (!m_ownerSelect && !userAgentShadowRoot())
        return;

    bool showLabel = m_ownerSelect && !legendElement();

    if (!userAgentShadowRoot()) {
        if (!showLabel)
            return;

        ensureUserAgentShadowRoot();
    }

    Ref labelContainer = *m_labelContainer;

    ScriptDisallowedScope::EventAllowedScope labelContainerScope { labelContainer };

    if (auto label = groupLabelText(); labelContainer->textContent() != label)
        labelContainer->setTextContent(WTF::move(label));

    if (showLabel)
        labelContainer->removeInlineStyleProperty(CSSPropertyDisplay);
    else
        labelContainer->setInlineStyleProperty(CSSPropertyDisplay, CSSValueNone);
}

void HTMLOptGroupElement::didAddUserAgentShadowRoot(ShadowRoot& root)
{
    Ref document = this->document();
    ScriptDisallowedScope::EventAllowedScope rootScope { root };

    Ref labelContainer = HTMLDivElement::create(document);
    ScriptDisallowedScope::EventAllowedScope labelContainerScope { labelContainer };
    labelContainer->setUserAgentPart(UserAgentParts::internalOptgroupLabel());
    m_labelContainer = labelContainer;
    root.appendChild(WTF::move(labelContainer));

    root.appendChild(HTMLSlotElement::create(slotTag, document));
}

auto HTMLOptGroupElement::insertionSteps(InsertionType insertionType, ContainerNode& parentOfInsertedTree) -> NeedsPostConnectionSteps
{
    auto result = HTMLElement::insertionSteps(insertionType, parentOfInsertedTree);

    if (!document().settings().htmlEnhancedSelectParsingEnabled())
        return result;

    if (insertionType.connectedToDocument && m_shadowTreeNeedsUpdate)
        protect(document())->addElementWithPendingUserAgentShadowTreeUpdate(*this);

    if (!m_ownerSelect) {
        if (RefPtr select = HTMLSelectElement::findOwnerSelect(parentNode(), HTMLSelectElement::ExcludeOptGroup::Yes)) {
            m_ownerSelect = select.get();
            select->setRecalcListItems();
            invalidateShadowTree();
        }
    }

    return result;
}

void HTMLOptGroupElement::removingSteps(RemovalType removalType, ContainerNode& oldParentOfRemovedTree)
{
    HTMLElement::removingSteps(removalType, oldParentOfRemovedTree);

    if (removalType.disconnectedFromDocument && m_shadowTreeNeedsUpdate)
        protect(document())->removeElementWithPendingUserAgentShadowTreeUpdate(*this);

    if (!document().settings().htmlEnhancedSelectParsingEnabled() || !m_ownerSelect)
        return;

    if (auto* select = HTMLSelectElement::findOwnerSelect(parentNode(), HTMLSelectElement::ExcludeOptGroup::Yes)) {
        ASSERT_UNUSED(select, select == m_ownerSelect.get());
        return;
    }

    if (RefPtr select = std::exchange(m_ownerSelect, nullptr).get()) {
        select->setRecalcListItems();
        invalidateShadowTree();
    }
}

bool HTMLOptGroupElement::isDisabledFormControl() const
{
    return m_isDisabled;
}

bool HTMLOptGroupElement::isActuallyDisabled() const
{
    if (HTMLElement::isActuallyDisabled())
        return true;
    RefPtr select = ownerSelectElement();
    return select && select->isDisabledFormControl();
}

bool HTMLOptGroupElement::isFocusable() const
{
    RefPtr select = ownerSelectElement();
    if (select && select->isDropdownBox())
        return false;
    return HTMLElement::isFocusable();
}

const AtomString& HTMLOptGroupElement::formControlType() const
{
    static MainThreadNeverDestroyed<const AtomString> optgroup("optgroup"_s);
    return optgroup;
}

void HTMLOptGroupElement::childrenChanged(const ChildChange& change)
{
    if (document().settings().htmlEnhancedSelectParsingEnabled()) {
        if (change.affectsElements != ChildChange::AffectsElements::No)
            invalidateShadowTree();
        HTMLElement::childrenChanged(change);
        return;
    }

    bool isRelevant = change.affectsElements == ChildChange::AffectsElements::Yes;
    RefPtr select = isRelevant ? ownerSelectElement() : nullptr;
    if (!isRelevant || !select) {
        HTMLElement::childrenChanged(change);
        return;
    }

    auto selectOptionIfNecessaryScope = select->optionToSelectFromChildChangeScope(change, this);

    recalcSelectOptions();
    HTMLElement::childrenChanged(change);
}

void HTMLOptGroupElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    HTMLElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
    recalcSelectOptions();

    if (name == disabledAttr) {
        bool newDisabled = !newValue.isNull();
        if (m_isDisabled != newDisabled) {
            Style::PseudoClassChangeInvalidation disabledInvalidation(*this, { { CSSSelector::PseudoClass::Disabled, newDisabled }, { CSSSelector::PseudoClass::Enabled, !newDisabled } });

            Vector<Style::PseudoClassChangeInvalidation> optionInvalidation;
            for (Ref descendant : descendantsOfType<HTMLOptionElement>(*this))
                optionInvalidation.append({ descendant, { { CSSSelector::PseudoClass::Disabled, newDisabled }, { CSSSelector::PseudoClass::Enabled, !newDisabled } } });

            m_isDisabled = newDisabled;
        }
    } else if (name == labelAttr)
        invalidateShadowTree();
}

void HTMLOptGroupElement::recalcSelectOptions()
{
    if (RefPtr selectElement = ownerSelectElement()) {
        selectElement->setRecalcListItems();
        selectElement->updateValidity();
    }
}

HTMLLegendElement* HTMLOptGroupElement::legendElement() const
{
    if (!document().settings().htmlEnhancedSelectEnabled())
        return nullptr;
    return dynamicDowncast<HTMLLegendElement>(firstElementChild());
}

String HTMLOptGroupElement::groupLabelText() const
{
    if (RefPtr legend = legendElement())
        return htmlAwareTextContent(*legend, IncludeAltText::Yes);

    return protect(document())->displayStringModifiedByEncoding(attributeWithoutSynchronization(labelAttr)).simplifyWhiteSpace(deprecatedIsSpaceOrNewline);
}

HTMLSelectElement* HTMLOptGroupElement::ownerSelectElement() const
{
    if (!document().settings().htmlEnhancedSelectParsingEnabled())
        return dynamicDowncast<HTMLSelectElement>(parentNode());

    return m_ownerSelect.get();
}

bool HTMLOptGroupElement::accessKeyAction(bool)
{
    RefPtr select = ownerSelectElement();
    // send to the parent to bring focus to the list box
    if (select && !select->focused())
        return select->accessKeyAction(false);
    return false;
}

} // namespace WebCore
