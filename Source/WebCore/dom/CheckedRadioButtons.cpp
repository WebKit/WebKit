/*
 * Copyright (C) 2007, 2008, 2009 Apple Inc. All rights reserved.
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
#include "CheckedRadioButtons.h"

#include "HTMLInputElement.h"

namespace WebCore {

static inline bool shouldMakeRadioGroup(HTMLInputElement* element)
{
    return element->isRadioButton() && !element->name().isEmpty() && element->inDocument();
}

void CheckedRadioButtons::addButton(HTMLInputElement* element)
{
    if (!shouldMakeRadioGroup(element))
        return;

    // We only track checked buttons.
    if (!element->checked())
        return;

    if (!m_nameToCheckedRadioButtonMap)
        m_nameToCheckedRadioButtonMap = adoptPtr(new NameToInputMap);

    pair<NameToInputMap::iterator, bool> result = m_nameToCheckedRadioButtonMap->add(element->name().impl(), element);
    if (result.second)
        return;
    
    HTMLInputElement* oldCheckedButton = result.first->second;
    if (oldCheckedButton == element)
        return;

    result.first->second = element;
    oldCheckedButton->setChecked(false);
}

HTMLInputElement* CheckedRadioButtons::checkedButtonForGroup(const AtomicString& name) const
{
    if (!m_nameToCheckedRadioButtonMap)
        return 0;

    m_nameToCheckedRadioButtonMap->checkConsistency();
    
    return m_nameToCheckedRadioButtonMap->get(name.impl());
}

void CheckedRadioButtons::removeButton(HTMLInputElement* element)
{
    if (element->name().isEmpty() || !m_nameToCheckedRadioButtonMap)
        return;
    
    m_nameToCheckedRadioButtonMap->checkConsistency();

    NameToInputMap::iterator it = m_nameToCheckedRadioButtonMap->find(element->name().impl());
    if (it == m_nameToCheckedRadioButtonMap->end() || it->second != element)
        return;
    
    ASSERT(element->shouldAppearChecked());
    ASSERT(element->isRadioButton());

    m_nameToCheckedRadioButtonMap->remove(it);
    if (m_nameToCheckedRadioButtonMap->isEmpty())
        m_nameToCheckedRadioButtonMap.clear();
}

} // namespace
