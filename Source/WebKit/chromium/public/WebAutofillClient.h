/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef WebAutofillClient_h
#define WebAutofillClient_h

namespace WebKit {

class WebFormElement;
class WebFrame;
class WebInputElement;
class WebKeyboardEvent;
class WebNode;
class WebString;

template <typename T> class WebVector;

class WebAutofillClient {
public:
    enum {
        MenuItemIDAutocompleteEntry = 0,
        MenuItemIDWarningMessage = -1,
        MenuItemIDPasswordEntry = -2,
        MenuItemIDSeparator = -3,
        MenuItemIDClearForm = -4,
        MenuItemIDAutofillOptions = -5,
        MenuItemIDDataListEntry = -6
    };

    // Informs the browser that the user has accepted an Autofill suggestion for
    // a WebNode. A positive |itemID| is a unique id used to identify the set
    // of Autofill profiles. If it is AutocompleteEntryMenuItemID, then the
    // suggestion is an Autocomplete suggestion; and |value| stores the
    // suggested text. |index| is an index of the selected suggestion in the
    // list of suggestions provided by the client.
    virtual void didAcceptAutofillSuggestion(const WebNode&,
                                             const WebString& value,
                                             const WebString& label,
                                             int itemID,
                                             unsigned index) { }

    // Informs the browser that the user has selected an Autofill suggestion for
    // a WebNode.  This happens when the user hovers over a suggestion or uses
    // the arrow keys to navigate to a suggestion.
    virtual void didSelectAutofillSuggestion(const WebNode&,
                                             const WebString& name,
                                             const WebString& label,
                                             int itemID) { }

    // Informs the browser that the user has cleared the selection from the
    // Autofill suggestions popup. This happens when a user uses the arrow
    // keys to navigate outside the range of possible selections.
    virtual void didClearAutofillSelection(const WebNode&) { }

    // Informs the browser an interactive autocomplete has been requested.
    virtual void didRequestAutocomplete(WebFrame*, const WebFormElement&) { }

    // Instructs the browser to remove the Autocomplete entry specified from
    // its DB.
    virtual void removeAutocompleteSuggestion(const WebString& name,
                                              const WebString& value) { }

    // These methods are called when the users edits a text-field.
    virtual void textFieldDidEndEditing(const WebInputElement&) { }
    virtual void textFieldDidChange(const WebInputElement&) { }
    virtual void textFieldDidReceiveKeyDown(const WebInputElement&, const WebKeyboardEvent&) { }

    // Informs the client whether or not any subsequent text changes should be ignored.
    virtual void setIgnoreTextChanges(bool ignore) { }

    virtual void didAssociateFormControls(const WebVector<WebNode>&) { }

protected:
    virtual ~WebAutofillClient() { }
};

} // namespace WebKit

#endif
