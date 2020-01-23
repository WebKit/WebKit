/*
 * Copyright (C) 2007-2018 Apple Inc. All rights reserved.
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
#include "RadioButtonGroups.h"

#include "HTMLInputElement.h"
#include "Range.h"
#include <wtf/WeakHashSet.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class RadioButtonGroup {
    WTF_MAKE_FAST_ALLOCATED;
public:
    bool isEmpty() const { return m_members.computesEmpty(); }
    bool isRequired() const { return m_requiredCount; }
    RefPtr<HTMLInputElement> checkedButton() const { return m_checkedButton.get(); }
    void add(HTMLInputElement&);
    void updateCheckedState(HTMLInputElement&);
    void requiredStateChanged(HTMLInputElement&);
    void remove(HTMLInputElement&);
    bool contains(HTMLInputElement&) const;
    Vector<Ref<HTMLInputElement>> members() const;

private:
    void setNeedsStyleRecalcForAllButtons();
    void updateValidityForAllButtons();
    bool isValid() const;
    void setCheckedButton(HTMLInputElement*);

    WeakHashSet<HTMLInputElement> m_members;
    WeakPtr<HTMLInputElement> m_checkedButton;
    size_t m_requiredCount { 0 };
};

inline bool RadioButtonGroup::isValid() const
{
    return !isRequired() || m_checkedButton;
}

Vector<Ref<HTMLInputElement>> RadioButtonGroup::members() const
{
    Vector<Ref<HTMLInputElement>> sortedMembers;
    for (auto& memeber : m_members)
        sortedMembers.append(memeber);
    std::sort(sortedMembers.begin(), sortedMembers.end(), [](auto& a, auto& b) {
        return documentOrderComparator(a.ptr(), b.ptr());
    });
    return sortedMembers;
}

void RadioButtonGroup::setCheckedButton(HTMLInputElement* button)
{
    RefPtr<HTMLInputElement> oldCheckedButton = m_checkedButton.get();
    if (oldCheckedButton == button)
        return;

    bool hadCheckedButton = m_checkedButton.get();
    bool willHaveCheckedButton = button;
    if (hadCheckedButton != willHaveCheckedButton)
        setNeedsStyleRecalcForAllButtons();

    m_checkedButton = makeWeakPtr(button);
    if (oldCheckedButton)
        oldCheckedButton->setChecked(false);
}

void RadioButtonGroup::add(HTMLInputElement& button)
{
    ASSERT(button.isRadioButton());
    if (!m_members.add(&button).isNewEntry)
        return;
    bool groupWasValid = isValid();
    if (button.isRequired())
        ++m_requiredCount;
    if (button.checked())
        setCheckedButton(&button);

    bool groupIsValid = isValid();
    if (groupWasValid != groupIsValid)
        updateValidityForAllButtons();
    else if (!groupIsValid) {
        // A radio button not in a group is always valid. We need to make it
        // invalid only if the group is invalid.
        button.updateValidity();
    }
}

void RadioButtonGroup::updateCheckedState(HTMLInputElement& button)
{
    ASSERT(button.isRadioButton());
    ASSERT(m_members.contains(button));
    bool wasValid = isValid();
    if (button.checked())
        setCheckedButton(&button);
    else {
        if (m_checkedButton == &button)
            setCheckedButton(nullptr);
    }
    if (wasValid != isValid())
        updateValidityForAllButtons();
}

void RadioButtonGroup::requiredStateChanged(HTMLInputElement& button)
{
    ASSERT(button.isRadioButton());
    ASSERT(m_members.contains(button));
    bool wasValid = isValid();
    if (button.isRequired())
        ++m_requiredCount;
    else {
        ASSERT(m_requiredCount);
        --m_requiredCount;
    }
    if (wasValid != isValid())
        updateValidityForAllButtons();
}

void RadioButtonGroup::remove(HTMLInputElement& button)
{
    ASSERT(button.isRadioButton());
    if (!m_members.contains(button))
        return;

    bool wasValid = isValid();
    m_members.remove(button);
    if (button.isRequired()) {
        ASSERT(m_requiredCount);
        --m_requiredCount;
    }
    if (m_checkedButton) {
        button.invalidateStyleForSubtree();
        if (m_checkedButton == &button) {
            m_checkedButton = nullptr;
            setNeedsStyleRecalcForAllButtons();
        }
    }

    if (m_members.computesEmpty()) {
        ASSERT(!m_requiredCount);
        ASSERT(!m_checkedButton);
    } else if (wasValid != isValid())
        updateValidityForAllButtons();
    if (!wasValid) {
        // A radio button not in a group is always valid. We need to make it
        // valid only if the group was invalid.
        button.updateValidity();
    }
}

void RadioButtonGroup::setNeedsStyleRecalcForAllButtons()
{
    for (auto& button : m_members) {
        ASSERT(button.isRadioButton());
        button.invalidateStyleForSubtree();
    }
}

void RadioButtonGroup::updateValidityForAllButtons()
{
    for (auto& button : m_members) {
        ASSERT(button.isRadioButton());
        button.updateValidity();
    }
}

bool RadioButtonGroup::contains(HTMLInputElement& button) const
{
    return m_members.contains(button);
}

// ----------------------------------------------------------------

// Explicitly define default constructor and destructor here outside the header
// so we can compile the header without including the definition of RadioButtonGroup.
RadioButtonGroups::RadioButtonGroups() = default;
RadioButtonGroups::~RadioButtonGroups() = default;

void RadioButtonGroups::addButton(HTMLInputElement& element)
{
    ASSERT(element.isRadioButton());
    if (element.name().isEmpty())
        return;

    auto& group = m_nameToGroupMap.add(element.name().impl(), nullptr).iterator->value;
    if (!group)
        group = makeUnique<RadioButtonGroup>();
    group->add(element);
}

Vector<Ref<HTMLInputElement>> RadioButtonGroups::groupMembers(const HTMLInputElement& element) const
{
    ASSERT(element.isRadioButton());
    if (!element.isRadioButton())
        return { };

    auto* name = element.name().impl();
    if (!name)
        return { };

    auto* group = m_nameToGroupMap.get(name);
    if (!group)
        return { };
    return group->members();
}

void RadioButtonGroups::updateCheckedState(HTMLInputElement& element)
{
    ASSERT(element.isRadioButton());
    if (element.name().isEmpty())
        return;
    m_nameToGroupMap.get(element.name().impl())->updateCheckedState(element);
}

void RadioButtonGroups::requiredStateChanged(HTMLInputElement& element)
{
    ASSERT(element.isRadioButton());
    if (element.name().isEmpty())
        return;
    auto* group = m_nameToGroupMap.get(element.name().impl());
    ASSERT(group);
    group->requiredStateChanged(element);
}

RefPtr<HTMLInputElement> RadioButtonGroups::checkedButtonForGroup(const AtomString& name) const
{
    m_nameToGroupMap.checkConsistency();
    RadioButtonGroup* group = m_nameToGroupMap.get(name.impl());
    return group ? group->checkedButton() : nullptr;
}

bool RadioButtonGroups::hasCheckedButton(const HTMLInputElement& element) const
{
    ASSERT(element.isRadioButton());
    const AtomString& name = element.name();
    if (name.isEmpty())
        return element.checked();
    auto* group = m_nameToGroupMap.get(name.impl());
    if (!group)
        return false; // FIXME: Update the radio button group before author script had a chance to run in didFinishInsertingNode().
    return group->checkedButton();
}

bool RadioButtonGroups::isInRequiredGroup(HTMLInputElement& element) const
{
    ASSERT(element.isRadioButton());
    if (element.name().isEmpty())
        return false;
    auto* group = m_nameToGroupMap.get(element.name().impl());
    return group && group->isRequired() && group->contains(element);
}

void RadioButtonGroups::removeButton(HTMLInputElement& element)
{
    ASSERT(element.isRadioButton());
    if (element.name().isEmpty())
        return;

    m_nameToGroupMap.checkConsistency();
    auto it = m_nameToGroupMap.find(element.name().impl());
    if (it == m_nameToGroupMap.end())
        return;
    it->value->remove(element);
    if (it->value->isEmpty())
        m_nameToGroupMap.remove(it);
}

} // namespace
