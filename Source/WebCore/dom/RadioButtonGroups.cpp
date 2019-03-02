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
#include <wtf/HashSet.h>

namespace WebCore {

class RadioButtonGroup {
    WTF_MAKE_FAST_ALLOCATED;
public:
    bool isEmpty() const { return m_members.isEmpty(); }
    bool isRequired() const { return m_requiredCount; }
    HTMLInputElement* checkedButton() const { return m_checkedButton; }
    void add(HTMLInputElement&);
    void updateCheckedState(HTMLInputElement&);
    void requiredStateChanged(HTMLInputElement&);
    void remove(HTMLInputElement&);
    bool contains(HTMLInputElement&) const;
    Vector<HTMLInputElement*> members() const;

private:
    void setNeedsStyleRecalcForAllButtons();
    void updateValidityForAllButtons();
    bool isValid() const;
    void setCheckedButton(HTMLInputElement*);

    HashSet<HTMLInputElement*> m_members;
    HTMLInputElement* m_checkedButton { nullptr };
    size_t m_requiredCount { 0 };
};

inline bool RadioButtonGroup::isValid() const
{
    return !isRequired() || m_checkedButton;
}

Vector<HTMLInputElement*> RadioButtonGroup::members() const
{
    auto members = copyToVector(m_members);
    std::sort(members.begin(), members.end(), documentOrderComparator);
    return members;
}

void RadioButtonGroup::setCheckedButton(HTMLInputElement* button)
{
    RefPtr<HTMLInputElement> oldCheckedButton = m_checkedButton;
    if (oldCheckedButton == button)
        return;

    bool hadCheckedButton = m_checkedButton;
    bool willHaveCheckedButton = button;
    if (hadCheckedButton != willHaveCheckedButton)
        setNeedsStyleRecalcForAllButtons();

    m_checkedButton = button;
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
    ASSERT(m_members.contains(&button));
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
    ASSERT(m_members.contains(&button));
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
    auto it = m_members.find(&button);
    if (it == m_members.end())
        return;

    bool wasValid = isValid();
    m_members.remove(it);
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

    if (m_members.isEmpty()) {
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
        ASSERT(button->isRadioButton());
        button->invalidateStyleForSubtree();
    }
}

void RadioButtonGroup::updateValidityForAllButtons()
{
    for (auto& button : m_members) {
        ASSERT(button->isRadioButton());
        button->updateValidity();
    }
}

bool RadioButtonGroup::contains(HTMLInputElement& button) const
{
    return m_members.contains(&button);
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

    if (!m_nameToGroupMap)
        m_nameToGroupMap = std::make_unique<NameToGroupMap>();

    auto& group = m_nameToGroupMap->add(element.name().impl(), nullptr).iterator->value;
    if (!group)
        group = std::make_unique<RadioButtonGroup>();
    group->add(element);
}

Vector<HTMLInputElement*> RadioButtonGroups::groupMembers(const HTMLInputElement& element) const
{
    ASSERT(element.isRadioButton());
    if (!element.isRadioButton())
        return { };

    auto* name = element.name().impl();
    if (!name)
        return { };

    if (!m_nameToGroupMap)
        return { };
    
    auto* group = m_nameToGroupMap->get(name);
    if (!group)
        return { };
    return group->members();
}

void RadioButtonGroups::updateCheckedState(HTMLInputElement& element)
{
    ASSERT(element.isRadioButton());
    if (element.name().isEmpty())
        return;
    ASSERT(m_nameToGroupMap);
    if (!m_nameToGroupMap)
        return;
    m_nameToGroupMap->get(element.name().impl())->updateCheckedState(element);
}

void RadioButtonGroups::requiredStateChanged(HTMLInputElement& element)
{
    ASSERT(element.isRadioButton());
    if (element.name().isEmpty())
        return;
    ASSERT(m_nameToGroupMap);
    if (!m_nameToGroupMap)
        return;
    auto* group = m_nameToGroupMap->get(element.name().impl());
    ASSERT(group);
    group->requiredStateChanged(element);
}

HTMLInputElement* RadioButtonGroups::checkedButtonForGroup(const AtomicString& name) const
{
    if (!m_nameToGroupMap)
        return nullptr;
    m_nameToGroupMap->checkConsistency();
    RadioButtonGroup* group = m_nameToGroupMap->get(name.impl());
    return group ? group->checkedButton() : nullptr;
}

bool RadioButtonGroups::hasCheckedButton(const HTMLInputElement& element) const
{
    ASSERT(element.isRadioButton());
    const AtomicString& name = element.name();
    if (name.isEmpty() || !m_nameToGroupMap)
        return element.checked();
    return m_nameToGroupMap->get(name.impl())->checkedButton();
}

bool RadioButtonGroups::isInRequiredGroup(HTMLInputElement& element) const
{
    ASSERT(element.isRadioButton());
    if (element.name().isEmpty())
        return false;
    if (!m_nameToGroupMap)
        return false;
    auto* group = m_nameToGroupMap->get(element.name().impl());
    return group && group->isRequired() && group->contains(element);
}

void RadioButtonGroups::removeButton(HTMLInputElement& element)
{
    ASSERT(element.isRadioButton());
    if (element.name().isEmpty())
        return;
    if (!m_nameToGroupMap)
        return;

    m_nameToGroupMap->checkConsistency();
    auto it = m_nameToGroupMap->find(element.name().impl());
    if (it == m_nameToGroupMap->end())
        return;
    it->value->remove(element);
    if (it->value->isEmpty()) {
        // FIXME: We may skip deallocating the empty RadioButtonGroup for
        // performance improvement. If we do so, we need to change the key type
        // of m_nameToGroupMap from AtomicStringImpl* to RefPtr<AtomicStringImpl>.
        m_nameToGroupMap->remove(it);
        if (m_nameToGroupMap->isEmpty())
            m_nameToGroupMap = nullptr;
    }
}

} // namespace
