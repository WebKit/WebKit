/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebSearchableFormData.h"

#include "Document.h"
#include "DocumentLoader.h"
#include "FormDataBuilder.h"
#include "FormDataList.h"
#include "Frame.h"
#include "HTMLFormControlElement.h"
#include "HTMLFormElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLOptionElement.h"
#include "HTMLOptionsCollection.h"
#include "HTMLSelectElement.h"
#include "TextEncoding.h"
#include "WebFormElement.h"
#include "WebInputElement.h"

using namespace WebCore;
using namespace HTMLNames;

namespace {

// Gets the encoding for the form.
void GetFormEncoding(const HTMLFormElement* form, TextEncoding* encoding)
{
    String str(form->getAttribute(HTMLNames::accept_charsetAttr));
    str.replace(',', ' ');
    Vector<String> charsets;
    str.split(' ', charsets);
    for (Vector<String>::const_iterator i(charsets.begin()); i != charsets.end(); ++i) {
        *encoding = TextEncoding(*i);
        if (encoding->isValid())
            return;
    }
    if (!form->document()->loader())
         return;
    *encoding = TextEncoding(form->document()->encoding());
}

// Returns true if the submit request results in an HTTP URL.
bool IsHTTPFormSubmit(const HTMLFormElement* form)
{
    // FIXME: This function is insane. This is an overly complicated way to get this information.
    String action(form->action());
    // The isNull() check is trying to avoid completeURL returning KURL() when passed a null string.
    return form->document()->completeURL(action.isNull() ? "" : action).protocolIs("http");
}

// If the form does not have an activated submit button, the first submit
// button is returned.
HTMLFormControlElement* GetButtonToActivate(HTMLFormElement* form)
{
    HTMLFormControlElement* firstSubmitButton = 0;
    // FIXME: Consider refactoring this code so that we don't call form->associatedElements() twice.
    for (Vector<FormAssociatedElement*>::const_iterator i(form->associatedElements().begin()); i != form->associatedElements().end(); ++i) {
      if (!(*i)->isFormControlElement())
          continue;
      HTMLFormControlElement* formElement = static_cast<HTMLFormControlElement*>(*i);
      if (formElement->isActivatedSubmit())
          // There's a button that is already activated for submit, return 0.
          return 0;
      if (!firstSubmitButton && formElement->isSuccessfulSubmitButton())
          firstSubmitButton = formElement;
    }
    return firstSubmitButton;
}

// Returns true if the selected state of all the options matches the default
// selected state.
bool IsSelectInDefaultState(HTMLSelectElement* select)
{
    const Vector<HTMLElement*>& listItems = select->listItems();
    if (select->multiple() || select->size() > 1) {
        for (Vector<HTMLElement*>::const_iterator i(listItems.begin()); i != listItems.end(); ++i) {
            if (!(*i)->hasLocalName(HTMLNames::optionTag))
                continue;
            HTMLOptionElement* optionElement = static_cast<HTMLOptionElement*>(*i);
            if (optionElement->selected() != optionElement->hasAttribute(selectedAttr))
                return false;
        }
        return true;
    }

    // The select is rendered as a combobox (called menulist in WebKit). At
    // least one item is selected, determine which one.
    HTMLOptionElement* initialSelected = 0;
    for (Vector<HTMLElement*>::const_iterator i(listItems.begin()); i != listItems.end(); ++i) {
        if (!(*i)->hasLocalName(HTMLNames::optionTag))
            continue;
        HTMLOptionElement* optionElement = static_cast<HTMLOptionElement*>(*i);
        if (optionElement->hasAttribute(selectedAttr)) {
            // The page specified the option to select.
            initialSelected = optionElement;
            break;
        }
        if (!initialSelected)
            initialSelected = optionElement;
    }
    return !initialSelected || initialSelected->selected();
}

// Returns true if the form element is in its default state, false otherwise.
// The default state is the state of the form element on initial load of the
// page, and varies depending upon the form element. For example, a checkbox is
// in its default state if the checked state matches the state of the checked attribute.
bool IsInDefaultState(HTMLFormControlElement* formElement)
{
    if (formElement->hasTagName(HTMLNames::inputTag)) {
        const HTMLInputElement* inputElement = static_cast<HTMLInputElement*>(formElement);
        if (inputElement->isCheckbox() || inputElement->isRadioButton())
            return inputElement->checked() == inputElement->hasAttribute(checkedAttr);
    } else if (formElement->hasTagName(HTMLNames::selectTag))
        return IsSelectInDefaultState(static_cast<HTMLSelectElement*>(formElement));
    return true;
}

// Look for a suitable search text field in a given HTMLFormElement 
// Return nothing if one of those items are found:
//  - A text area field
//  - A file upload field 
//  - A Password field
//  - More than one text field
HTMLInputElement* findSuitableSearchInputElement(const HTMLFormElement* form)
{
    HTMLInputElement* textElement = 0;
    // FIXME: Consider refactoring this code so that we don't call form->associatedElements() twice.
    for (Vector<FormAssociatedElement*>::const_iterator i(form->associatedElements().begin()); i != form->associatedElements().end(); ++i) {
        if (!(*i)->isFormControlElement())
            continue;

        HTMLFormControlElement* formElement = static_cast<HTMLFormControlElement*>(*i);

        if (formElement->disabled() || formElement->name().isNull())
            continue;

        if (!IsInDefaultState(formElement) || formElement->hasTagName(HTMLNames::textareaTag))
            return 0;

        if (formElement->hasTagName(HTMLNames::inputTag) && formElement->willValidate()) {
            const HTMLInputElement* input = static_cast<const HTMLInputElement*>(formElement);

            // Return nothing if a file upload field or a password field are found.
            if (input->isFileUpload() || input->isPasswordField())
                return 0;

            if (input->isTextField()) {
                if (textElement) {
                    // The auto-complete bar only knows how to fill in one value.
                    // This form has multiple fields; don't treat it as searchable.
                    return 0;
                }
                textElement = static_cast<HTMLInputElement*>(formElement);
            }
        }
    }
    return textElement;
}

// Build a search string based on a given HTMLFormElement and HTMLInputElement
// 
// Search string output example from www.google.com:
// "hl=en&source=hp&biw=1085&bih=854&q={searchTerms}&btnG=Google+Search&aq=f&aqi=&aql=&oq="
// 
// Return false if the provided HTMLInputElement is not found in the form
bool buildSearchString(const HTMLFormElement* form, Vector<char>* encodedString, TextEncoding* encoding, const HTMLInputElement* textElement)
{
    bool isElementFound = false;   

    // FIXME: Consider refactoring this code so that we don't call form->associatedElements() twice.
    for (Vector<FormAssociatedElement*>::const_iterator i(form->associatedElements().begin()); i != form->associatedElements().end(); ++i) {
        if (!(*i)->isFormControlElement())
            continue;

        HTMLFormControlElement* formElement = static_cast<HTMLFormControlElement*>(*i);

        if (formElement->disabled() || formElement->name().isNull())
            continue;

        FormDataList dataList(*encoding);
        if (!formElement->appendFormData(dataList, false))
            continue;

        const Vector<FormDataList::Item>& items = dataList.items();

        for (Vector<FormDataList::Item>::const_iterator j(items.begin()); j != items.end(); ++j) {
            // Handle ISINDEX / <input name=isindex> specially, but only if it's
            // the first entry.
            if (!encodedString->isEmpty() || j->data() != "isindex") {
                if (!encodedString->isEmpty())
                    encodedString->append('&');
                FormDataBuilder::encodeStringAsFormData(*encodedString, j->data());
                encodedString->append('=');
            }
            ++j;
            if (formElement == textElement) {
                encodedString->append("{searchTerms}", 13);
                isElementFound = true;
            } else
                FormDataBuilder::encodeStringAsFormData(*encodedString, j->data());
        }
    }
    return isElementFound;
}
} // namespace

namespace WebKit {

WebSearchableFormData::WebSearchableFormData(const WebFormElement& form, const WebInputElement& selectedInputElement)
{
    RefPtr<HTMLFormElement> formElement = form.operator PassRefPtr<HTMLFormElement>();
    HTMLInputElement* inputElement = selectedInputElement.operator PassRefPtr<HTMLInputElement>().get();

    // Only consider forms that GET data.
    // Allow HTTPS only when an input element is provided. 
    if (equalIgnoringCase(formElement->getAttribute(methodAttr), "post") 
        || (!IsHTTPFormSubmit(formElement.get()) && !inputElement))
        return;

    Vector<char> encodedString;
    TextEncoding encoding;

    GetFormEncoding(formElement.get(), &encoding);
    if (!encoding.isValid()) {
        // Need a valid encoding to encode the form elements.
        // If the encoding isn't found webkit ends up replacing the params with
        // empty strings. So, we don't try to do anything here.
        return;
    } 

    // Look for a suitable search text field in the form when a 
    // selectedInputElement is not provided.
    if (!inputElement) {
        inputElement = findSuitableSearchInputElement(formElement.get());

        // Return if no suitable text element has been found.
        if (!inputElement)
            return;
    }

    HTMLFormControlElement* firstSubmitButton = GetButtonToActivate(formElement.get());
    if (firstSubmitButton) {
        // The form does not have an active submit button, make the first button
        // active. We need to do this, otherwise the URL will not contain the
        // name of the submit button.
        firstSubmitButton->setActivatedSubmit(true);
    }

    bool isValidSearchString = buildSearchString(formElement.get(), &encodedString, &encoding, inputElement);

    if (firstSubmitButton)
        firstSubmitButton->setActivatedSubmit(false);

    // Return if the search string is not valid. 
    if (!isValidSearchString)
        return;

    String action(formElement->action());
    KURL url(formElement->document()->completeURL(action.isNull() ? "" : action));
    RefPtr<FormData> formData = FormData::create(encodedString);
    url.setQuery(formData->flattenToString());
    m_url = url;
    m_encoding = String(encoding.name()); 
}

} // namespace WebKit
