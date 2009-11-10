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
#include "WebPasswordFormData.h"

#include "Document.h"
#include "DocumentLoader.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HTMLFormElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "KURL.h"

#include "DOMUtilitiesPrivate.h"
#include "WebPasswordFormUtils.h"

using namespace WebCore;

namespace WebKit {

namespace {

// Helper to determine which password is the main one, and which is
// an old password (e.g on a "make new password" form), if any.
bool locateSpecificPasswords(PasswordFormFields* fields,
                             HTMLInputElement** password,
                             HTMLInputElement** oldPassword)
{
    ASSERT(fields);
    ASSERT(password);
    ASSERT(oldPassword);
    switch (fields->passwords.size()) {
    case 1:
        // Single password, easy.
        *password = fields->passwords[0];
        break;
    case 2:
        if (fields->passwords[0]->value() == fields->passwords[1]->value())
            // Treat two identical passwords as a single password.
            *password = fields->passwords[0];
        else {
            // Assume first is old password, second is new (no choice but to guess).
            *oldPassword = fields->passwords[0];
            *password = fields->passwords[1];
        }
        break;
    case 3:
        if (fields->passwords[0]->value() == fields->passwords[1]->value()
            && fields->passwords[0]->value() == fields->passwords[2]->value()) {
            // All three passwords the same? Just treat as one and hope.
            *password = fields->passwords[0];
        } else if (fields->passwords[0]->value() == fields->passwords[1]->value()) {
            // Two the same and one different -> old password is duplicated one.
            *oldPassword = fields->passwords[0];
            *password = fields->passwords[2];
        } else if (fields->passwords[1]->value() == fields->passwords[2]->value()) {
            *oldPassword = fields->passwords[0];
            *password = fields->passwords[1];
        } else {
            // Three different passwords, or first and last match with middle
            // different. No idea which is which, so no luck.
            return false;
        }
        break;
    default:
        return false;
    }
    return true;
}

// Helped method to clear url of unneeded parts.
KURL stripURL(const KURL& url)
{
    KURL strippedURL = url;
    strippedURL.setUser(String());
    strippedURL.setPass(String());
    strippedURL.setQuery(String());
    strippedURL.setFragmentIdentifier(String());
    return strippedURL;
}

// Helper to gather up the final form data and create a PasswordForm.
void assemblePasswordFormResult(const KURL& fullOrigin,
                                const KURL& fullAction,
                                HTMLFormControlElement* submit,
                                HTMLInputElement* userName,
                                HTMLInputElement* oldPassword,
                                HTMLInputElement* password,
                                WebPasswordFormData* result)
{
    // We want to keep the path but strip any authentication data, as well as
    // query and ref portions of URL, for the form action and form origin.
    result->action = stripURL(fullAction);
    result->origin = stripURL(fullOrigin);

    // Naming is confusing here because we have both the HTML form origin URL
    // the page where the form was seen), and the "origin" components of the url
    // (scheme, host, and port).
    KURL signonRealmURL = stripURL(fullOrigin);
    signonRealmURL.setPath("");
    result->signonRealm = signonRealmURL;

    if (submit)
        result->submitElement = submit->name();
    if (userName) {
        result->userNameElement = userName->name();
        result->userNameValue = userName->value();
    }
    if (password) {
        result->passwordElement = password->name();
        result->passwordValue = password->value();
    }
    if (oldPassword) {
        result->oldPasswordElement = oldPassword->name();
        result->oldPasswordValue = oldPassword->value();
    }
}

} // namespace

WebPasswordFormData::WebPasswordFormData(const WebFormElement& webForm)
{
    RefPtr<HTMLFormElement> form = webForm.operator PassRefPtr<HTMLFormElement>();

    Frame* frame = form->document()->frame();
    if (!frame)
        return;

    PasswordFormFields fields;
    findPasswordFormFields(form.get(), &fields);

    // Get the document URL
    KURL fullOrigin(ParsedURLString, form->document()->documentURI());

    // Calculate the canonical action URL
    KURL fullAction = frame->loader()->completeURL(form->action());
    if (!fullAction.isValid())
        return;

    // Determine the types of the password fields
    HTMLInputElement* password = 0;
    HTMLInputElement* oldPassword = 0;
    if (!locateSpecificPasswords(&fields, &password, &oldPassword))
        return;

    assemblePasswordFormResult(fullOrigin, fullAction,
                               fields.submit, fields.userName,
                               oldPassword, password, this);
}

} // namespace WebKit
