/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2015-2016 Apple Inc. All rights reserved.
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
#include "FormSubmission.h"

#include "ContentSecurityPolicy.h"
#include "DOMFormData.h"
#include "Document.h"
#include "Event.h"
#include "FormData.h"
#include "FormDataBuilder.h"
#include "FormState.h"
#include "Frame.h"
#include "FrameLoadRequest.h"
#include "FrameLoader.h"
#include "HTMLFormControlElement.h"
#include "HTMLFormElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "ScriptDisallowedScope.h"
#include "TextEncoding.h"
#include <wtf/WallTime.h>

namespace WebCore {

using namespace HTMLNames;

static int64_t generateFormDataIdentifier()
{
    // Initialize to the current time to reduce the likelihood of generating
    // identifiers that overlap with those from past/future browser sessions.
    static int64_t nextIdentifier = static_cast<int64_t>(WallTime::now().secondsSinceEpoch().microseconds());
    return ++nextIdentifier;
}

static void appendMailtoPostFormDataToURL(URL& url, const FormData& data, const String& encodingType)
{
    String body = data.flattenToString();

    if (equalLettersIgnoringASCIICase(encodingType, "text/plain")) {
        // Convention seems to be to decode, and s/&/\r\n/. Also, spaces are encoded as %20.
        body = decodeURLEscapeSequences(makeString(body.replaceWithLiteral('&', "\r\n").replace('+', ' '), "\r\n"));
    }

    Vector<char> bodyData;
    bodyData.append("body=", 5);
    FormDataBuilder::encodeStringAsFormData(bodyData, body.utf8());
    body = String(bodyData.data(), bodyData.size()).replaceWithLiteral('+', "%20");

    auto query = url.query();
    if (query.isEmpty())
        url.setQuery(body);
    else
        url.setQuery(makeString(query, '&', body));
}

ASCIILiteral FormSubmission::Attributes::methodString(Method method)
{
    if (RuntimeEnabledFeatures::sharedFeatures().dialogElementEnabled() && method == Method::Dialog)
        return "dialog"_s;
    return method == Method::Post ? "post"_s : "get"_s;
}

void FormSubmission::Attributes::parseAction(const String& action)
{
    // FIXME: Can we parse into a URL?
    m_action = stripLeadingAndTrailingHTMLSpaces(action);
}

String FormSubmission::Attributes::parseEncodingType(const String& type)
{
    if (equalLettersIgnoringASCIICase(type, "multipart/form-data"))
        return "multipart/form-data"_s;
    if (equalLettersIgnoringASCIICase(type, "text/plain"))
        return "text/plain"_s;
    return "application/x-www-form-urlencoded"_s;
}

void FormSubmission::Attributes::updateEncodingType(const String& type)
{
    m_encodingType = parseEncodingType(type);
    m_isMultiPartForm = (m_encodingType == "multipart/form-data");
}

FormSubmission::Method FormSubmission::Attributes::parseMethodType(const String& type)
{
    if (RuntimeEnabledFeatures::sharedFeatures().dialogElementEnabled() && equalLettersIgnoringASCIICase(type, "dialog"))
        return FormSubmission::Method::Dialog;

    if (equalLettersIgnoringASCIICase(type, "post"))
        return FormSubmission::Method::Post;

    return FormSubmission::Method::Get;
}

void FormSubmission::Attributes::updateMethodType(const String& type)
{
    m_method = parseMethodType(type);
}

inline FormSubmission::FormSubmission(Method method, const String& returnValue, const URL& action, const String& target, const String& contentType, LockHistory lockHistory, Event* event)
    : m_method(method)
    , m_action(action)
    , m_target(target)
    , m_contentType(contentType)
    , m_lockHistory(lockHistory)
    , m_event(event)
    , m_returnValue(returnValue)
{
}

inline FormSubmission::FormSubmission(Method method, const URL& action, const String& target, const String& contentType, Ref<FormState>&& state, Ref<FormData>&& data, const String& boundary, LockHistory lockHistory, Event* event)
    : m_method(method)
    , m_action(action)
    , m_target(target)
    , m_contentType(contentType)
    , m_formState(WTFMove(state))
    , m_formData(WTFMove(data))
    , m_boundary(boundary)
    , m_lockHistory(lockHistory)
    , m_event(event)
{
}

static TextEncoding encodingFromAcceptCharset(const String& acceptCharset, Document& document)
{
    String normalizedAcceptCharset = acceptCharset;
    normalizedAcceptCharset.replace(',', ' ');

    for (auto& charset : normalizedAcceptCharset.split(' ')) {
        TextEncoding encoding(charset);
        if (encoding.isValid())
            return encoding;
    }

    return document.textEncoding();
}

Ref<FormSubmission> FormSubmission::create(HTMLFormElement& form, HTMLFormControlElement* overrideSubmitter, const Attributes& attributes, Event* event, LockHistory lockHistory, FormSubmissionTrigger trigger)
{
    auto copiedAttributes = attributes;

    auto submitter = makeRefPtr(overrideSubmitter ? overrideSubmitter : form.findSubmitter(event));
    if (submitter) {
        AtomString attributeValue;
        if (!(attributeValue = submitter->attributeWithoutSynchronization(formactionAttr)).isNull())
            copiedAttributes.parseAction(attributeValue);
        if (!(attributeValue = submitter->attributeWithoutSynchronization(formenctypeAttr)).isNull())
            copiedAttributes.updateEncodingType(attributeValue);
        if (!(attributeValue = submitter->attributeWithoutSynchronization(formmethodAttr)).isNull())
            copiedAttributes.updateMethodType(attributeValue);
        if (!(attributeValue = submitter->attributeWithoutSynchronization(formtargetAttr)).isNull())
            copiedAttributes.setTarget(attributeValue);
    }

    auto& document = form.document();
    auto encodingType = copiedAttributes.encodingType();
    auto actionURL = document.completeURL(copiedAttributes.action().isEmpty() ? document.url().string() : copiedAttributes.action());

    if (RuntimeEnabledFeatures::sharedFeatures().dialogElementEnabled() && copiedAttributes.method() == Method::Dialog) {
        String returnValue = submitter ? submitter->resultForDialogSubmit() : emptyString();
        return adoptRef(*new FormSubmission(copiedAttributes.method(), returnValue, actionURL, form.effectiveTarget(event, submitter.get()), encodingType, lockHistory, event));
    }

    ASSERT(copiedAttributes.method() == Method::Post || copiedAttributes.method() == Method::Get);

    bool isMailtoForm = actionURL.protocolIs("mailto");
    bool isMultiPartForm = false;

    document.contentSecurityPolicy()->upgradeInsecureRequestIfNeeded(actionURL, ContentSecurityPolicy::InsecureRequestType::FormSubmission);

    if (copiedAttributes.method() == Method::Post) {
        isMultiPartForm = copiedAttributes.isMultiPartForm();
        if (isMultiPartForm && isMailtoForm) {
            encodingType = "application/x-www-form-urlencoded";
            isMultiPartForm = false;
        }
    }

    auto dataEncoding = isMailtoForm ? UTF8Encoding() : encodingFromAcceptCharset(copiedAttributes.acceptCharset(), document);
    auto domFormData = DOMFormData::create(dataEncoding.encodingForFormSubmissionOrURLParsing());
    StringPairVector formValues;

    auto result = form.constructEntryList(WTFMove(domFormData), &formValues, isMultiPartForm ? HTMLFormElement::IsMultipartForm::Yes : HTMLFormElement::IsMultipartForm::No);
    RELEASE_ASSERT(result);
    domFormData = result.releaseNonNull();

    RefPtr<FormData> formData;
    String boundary;

    if (isMultiPartForm) {
        formData = FormData::createMultiPart(domFormData);
        boundary = formData->boundary().data();
    } else {
        formData = FormData::create(domFormData, attributes.method() == Method::Get ? FormData::FormURLEncoded : FormData::parseEncodingType(encodingType));
        if (copiedAttributes.method() == Method::Post && isMailtoForm) {
            // Convert the form data into a string that we put into the URL.
            appendMailtoPostFormDataToURL(actionURL, *formData, encodingType);
            formData = FormData::create();
        }
    }

    formData->setIdentifier(generateFormDataIdentifier());

    auto formState = FormState::create(form, WTFMove(formValues), document, trigger);

    return adoptRef(*new FormSubmission(copiedAttributes.method(), actionURL, form.effectiveTarget(event, submitter.get()), encodingType, WTFMove(formState), formData.releaseNonNull(), boundary, lockHistory, event));
}

URL FormSubmission::requestURL() const
{
    ASSERT(m_method == Method::Post || m_method == Method::Get);

    if (m_method == Method::Post)
        return m_action;

    URL requestURL(m_action);
    requestURL.setQuery(m_formData->flattenToString());
    return requestURL;
}

void FormSubmission::populateFrameLoadRequest(FrameLoadRequest& frameRequest)
{
    ASSERT(m_method == Method::Post || m_method == Method::Get);

    if (!m_target.isEmpty())
        frameRequest.setFrameName(m_target);

    if (!m_referrer.isEmpty())
        frameRequest.resourceRequest().setHTTPReferrer(m_referrer);

    if (m_method == Method::Post) {
        frameRequest.resourceRequest().setHTTPMethod("POST");
        frameRequest.resourceRequest().setHTTPBody(m_formData.copyRef());

        // construct some user headers if necessary
        if (m_boundary.isEmpty())
            frameRequest.resourceRequest().setHTTPContentType(m_contentType);
        else
            frameRequest.resourceRequest().setHTTPContentType(m_contentType + "; boundary=" + m_boundary);
    }

    frameRequest.resourceRequest().setURL(requestURL());
    FrameLoader::addHTTPOriginIfNeeded(frameRequest.resourceRequest(), m_origin);
}

}
