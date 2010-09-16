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

using namespace WebCore;

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
    const Frame* frame = form->document()->frame();
    *encoding = frame ? TextEncoding(frame->loader()->writer()->encoding()) : Latin1Encoding();
}

// Returns true if the submit request results in an HTTP URL.
bool IsHTTPFormSubmit(const HTMLFormElement* form)
{
    String action(form->action());
    return form->document()->frame()->loader()->completeURL(action.isNull() ? "" : action).protocol() == "http";
}

// If the form does not have an activated submit button, the first submit
// button is returned.
HTMLFormControlElement* GetButtonToActivate(HTMLFormElement* form)
{
    HTMLFormControlElement* firstSubmitButton = 0;
    // FIXME: Consider refactoring this code so that we don't call form->associatedElements() twice.
    for (Vector<HTMLFormControlElement*>::const_iterator i(form->associatedElements().begin()); i != form->associatedElements().end(); ++i) {
      HTMLFormControlElement* formElement = *i;
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
bool IsSelectInDefaultState(const HTMLSelectElement* select)
{
    const Vector<Element*>& listItems = select->listItems();
    if (select->multiple() || select->size() > 1) {
        for (Vector<Element*>::const_iterator i(listItems.begin()); i != listItems.end(); ++i) {
            if (!(*i)->hasLocalName(HTMLNames::optionTag))
                continue;
            const HTMLOptionElement* optionElement = static_cast<const HTMLOptionElement*>(*i);
            if (optionElement->selected() != optionElement->defaultSelected())
                return false;
        }
        return true;
    }

    // The select is rendered as a combobox (called menulist in WebKit). At
    // least one item is selected, determine which one.
    const HTMLOptionElement* initialSelected = 0;
    for (Vector<Element*>::const_iterator i(listItems.begin()); i != listItems.end(); ++i) {
        if (!(*i)->hasLocalName(HTMLNames::optionTag))
            continue;
        const HTMLOptionElement* optionElement = static_cast<const HTMLOptionElement*>(*i);
        if (optionElement->defaultSelected()) {
            // The page specified the option to select.
            initialSelected = optionElement;
            break;
        }
        if (!initialSelected)
            initialSelected = optionElement;
    }
    return initialSelected ? initialSelected->selected() : true;
}

// Returns true if the form element is in its default state, false otherwise.
// The default state is the state of the form element on initial load of the
// page, and varies depending upon the form element. For example, a checkbox is
// in its default state if the checked state matches the defaultChecked state.
bool IsInDefaultState(const HTMLFormControlElement* formElement)
{
    if (formElement->hasTagName(HTMLNames::inputTag)) {
        const HTMLInputElement* inputElement = static_cast<const HTMLInputElement*>(formElement);
        if (inputElement->isCheckbox() || inputElement->isRadioButton())
            return inputElement->checked() == inputElement->defaultChecked();
    } else if (formElement->hasTagName(HTMLNames::selectTag))
        return IsSelectInDefaultState(static_cast<const HTMLSelectElement*>(formElement));
    return true;
}

// If form has only one text input element, return true. If a valid input
// element is not found, return false. Additionally, the form data for all
// elements is added to enc_string and the encoding used is set in
// encoding_name.
bool HasSuitableTextElement(const HTMLFormElement* form, Vector<char>* encodedString, String* encodingName)
{
    TextEncoding encoding;
    GetFormEncoding(form, &encoding);
    if (!encoding.isValid()) {
        // Need a valid encoding to encode the form elements.
        // If the encoding isn't found webkit ends up replacing the params with
        // empty strings. So, we don't try to do anything here.
        return 0;
    }
    *encodingName = encoding.name();

    HTMLInputElement* textElement = 0;
    // FIXME: Consider refactoring this code so that we don't call form->associatedElements() twice.
    for (Vector<HTMLFormControlElement*>::const_iterator i(form->associatedElements().begin()); i != form->associatedElements().end(); ++i) {
        HTMLFormControlElement* formElement = *i;
        if (formElement->disabled() || formElement->name().isNull())
            continue;

        if (!IsInDefaultState(formElement) || formElement->hasTagName(HTMLNames::textareaTag))
            return 0;

        bool isTextElement = false;
        if (formElement->hasTagName(HTMLNames::inputTag)) {
            if (static_cast<const HTMLInputElement*>(formElement)->isFileUpload()) {
                // Too big, don't try to index this.
                return 0;
            }

            if (static_cast<const HTMLInputElement*>(formElement)->isPasswordField()) {
                // Don't store passwords! This is most likely an https anyway.
                return 0;
            }

            // FIXME: This needs to use a function on HTMLInputElement other than inputType.
            // Also, it's not clear why TEXT should be handled differently than, say, SEARCH.
            switch (static_cast<const HTMLInputElement*>(formElement)->inputType()) {
            case HTMLInputElement::TEXT:
            case HTMLInputElement::ISINDEX:
                isTextElement = true;
                break;
            default:
                break;
            }
      }

      FormDataList dataList(encoding);
      if (!formElement->appendFormData(dataList, false))
          continue;

      const Vector<FormDataList::Item>& items = dataList.items();
      if (isTextElement && !items.isEmpty()) {
          if (textElement) {
              // The auto-complete bar only knows how to fill in one value.
              // This form has multiple fields; don't treat it as searchable.
              return false;
          }
          textElement = static_cast<HTMLInputElement*>(formElement);
      }
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
          if (formElement == textElement)
              encodedString->append("{searchTerms}", 13);
          else
              FormDataBuilder::encodeStringAsFormData(*encodedString, j->data());
      }
    }

    return textElement;
}

} // namespace

namespace WebKit {

WebSearchableFormData::WebSearchableFormData(const WebFormElement& form)
{
    RefPtr<HTMLFormElement> formElement = form.operator PassRefPtr<HTMLFormElement>();
    const Frame* frame = formElement->document()->frame();
    if (!frame)
        return;

    // Only consider forms that GET data and the action targets an http page.
    if (equalIgnoringCase(formElement->getAttribute(HTMLNames::methodAttr), "post") || !IsHTTPFormSubmit(formElement.get()))
        return;

    HTMLFormControlElement* firstSubmitButton = GetButtonToActivate(formElement.get());
    if (firstSubmitButton) {
        // The form does not have an active submit button, make the first button
        // active. We need to do this, otherwise the URL will not contain the
        // name of the submit button.
        firstSubmitButton->setActivatedSubmit(true);
    }
    Vector<char> encodedString;
    String encoding;
    bool hasElement = HasSuitableTextElement(formElement.get(), &encodedString, &encoding);
    if (firstSubmitButton)
        firstSubmitButton->setActivatedSubmit(false);
    if (!hasElement) {
        // Not a searchable form.
        return;
    }

    String action(formElement->action());
    KURL url(frame->loader()->completeURL(action.isNull() ? "" : action));
    RefPtr<FormData> formData = FormData::create(encodedString);
    url.setQuery(formData->flattenToString());
    m_url = url;
    m_encoding = encoding;
}

} // namespace WebKit
