/* ***** BEGIN LICENSE BLOCK *****
* Version: MPL 1.1/GPL 2.0/LGPL 2.1
*
* The contents of this file are subject to the Mozilla Public License Version
* 1.1 (the "License"); you may not use this file except in compliance with
* the License. You may obtain a copy of the License at
* http://www.mozilla.org/MPL/
*
* Software distributed under the License is distributed on an "AS IS" basis,
* WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
* for the specific language governing rights and limitations under the
* License.
*
* The Original Code is Mozilla Password Manager.
*
* The Initial Developer of the Original Code is
* Brian Ryner.
* Portions created by the Initial Developer are Copyright (C) 2003
* the Initial Developer. All Rights Reserved.
*
* Contributor(s):
*  Brian Ryner <bryner@brianryner.com>
*
* Alternatively, the contents of this file may be used under the terms of
* either the GNU General Public License Version 2 or later (the "GPL"), or
* the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
* in which case the provisions of the GPL or the LGPL are applicable instead
* of those above. If you wish to allow use of your version of this file only
* under the terms of either the GPL or the LGPL, and not to allow others to
* use your version of this file under the terms of the MPL, indicate your
* decision by deleting the provisions above and replace them with the notice
* and other provisions required by the GPL or the LGPL. If you do not delete
* the provisions above, a recipient may use your version of this file under
* the terms of any one of the MPL, the GPL or the LGPL.
*
* ***** END LICENSE BLOCK ***** */

// Helper to WebPasswordFormData to do the locating of username/password
// fields.
// This method based on Firefox2 code in
//   toolkit/components/passwordmgr/base/nsPasswordManager.cpp

#include "config.h"
#include "WebPasswordFormUtils.h"

#include "HTMLFormElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "KURL.h"

#include "DOMUtilitiesPrivate.h"

using namespace WebCore;

namespace WebKit {

// Maximum number of password fields we will observe before throwing our
// hands in the air and giving up with a given form.
static const size_t maxPasswords = 3;

void findPasswordFormFields(HTMLFormElement* form, PasswordFormFields* fields)
{
    ASSERT(form);
    ASSERT(fields);

    int firstPasswordIndex = 0;
    // First, find the password fields and activated submit button
    const Vector<HTMLFormControlElement*>& formElements = form->associatedElements();
    for (size_t i = 0; i < formElements.size(); i++) {
        HTMLFormControlElement* formElement = formElements[i];
        if (formElement->isActivatedSubmit())
            fields->submit = formElement;

        if (!formElement->hasLocalName(HTMLNames::inputTag))
            continue;

        HTMLInputElement* inputElement = toHTMLInputElement(formElement);
        if (!inputElement->isEnabledFormControl())
            continue;

        if ((fields->passwords.size() < maxPasswords)
            && inputElement->isPasswordField()
            && inputElement->autoComplete()) {
            if (fields->passwords.isEmpty())
                firstPasswordIndex = i;
            fields->passwords.append(inputElement);
        }
    }

    if (!fields->passwords.isEmpty()) {
        // Then, search backwards for the username field
        for (int i = firstPasswordIndex - 1; i >= 0; i--) {
            HTMLFormControlElement* formElement = formElements[i];
            if (!formElement->hasLocalName(HTMLNames::inputTag))
                continue;

            HTMLInputElement* inputElement = toHTMLInputElement(formElement);
            if (!inputElement->isEnabledFormControl())
                continue;

            // Various input types such as text, url, email can be a username field.
            if ((inputElement->isTextField() && !inputElement->isPasswordField())
                && (inputElement->autoComplete())) {
                fields->userName = inputElement;
                break;
            }
        }
    }
}

} // namespace WebKit
