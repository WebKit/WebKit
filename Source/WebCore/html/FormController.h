/*
 * Copyright (C) 2006-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2010, 2011, 2012 Google Inc. All rights reserved.
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

#pragma once

#include "RadioButtonGroups.h"
#include <wtf/Forward.h>
#include <wtf/ListHashSet.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class FormKeyGenerator;
class HTMLFormControlElementWithState;
class HTMLFormElement;
class SavedFormState;

using FormControlState = Vector<String>;

class FormController {
    WTF_MAKE_FAST_ALLOCATED;
public:
    FormController();
    ~FormController();

    RadioButtonGroups& radioButtonGroups() { return m_radioButtonGroups; }

    void registerFormElementWithState(HTMLFormControlElementWithState&);
    void unregisterFormElementWithState(HTMLFormControlElementWithState&);

    unsigned formElementsCharacterCount() const;

    Vector<String> formElementsState() const;
    void setStateForNewFormElements(const Vector<String>&);

    void willDeleteForm(HTMLFormElement&);
    void restoreControlStateFor(HTMLFormControlElementWithState&);
    void restoreControlStateIn(HTMLFormElement&);
    bool hasFormStateToRestore() const;

    WEBCORE_EXPORT static Vector<String> referencedFilePaths(const Vector<String>& stateVector);

private:
    typedef ListHashSet<RefPtr<HTMLFormControlElementWithState>> FormElementListHashSet;
    typedef HashMap<RefPtr<AtomStringImpl>, std::unique_ptr<SavedFormState>> SavedFormStateMap;

    static std::unique_ptr<SavedFormStateMap> createSavedFormStateMap(const FormElementListHashSet&);
    FormControlState takeStateForFormElement(const HTMLFormControlElementWithState&);
    static void formStatesFromStateVector(const Vector<String>&, SavedFormStateMap&);

    RadioButtonGroups m_radioButtonGroups;
    FormElementListHashSet m_formElementsWithState;
    SavedFormStateMap m_savedFormStateMap;
    std::unique_ptr<FormKeyGenerator> m_formKeyGenerator;
};

} // namespace WebCore
