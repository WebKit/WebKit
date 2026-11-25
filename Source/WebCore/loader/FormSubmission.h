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

#pragma once

#include <WebCore/FormState.h>
#include <WebCore/FrameLoaderTypes.h>
#include <WebCore/ReferrerPolicy.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/URL.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class Event;
class FormData;
class FrameLoadRequest;
class HTMLFormControlElement;

class FormSubmission : public RefCountedAndCanMakeWeakPtr<FormSubmission> {
public:
    enum class Method : uint8_t { Get, Post, Dialog };

    class Attributes {
    public:
        Method method() const { return m_method; }
        static Method parseMethodType(StringImpl*);
        void updateMethodType(StringImpl*);
        static ASCIILiteral methodString(Method);

        const RefPtr<StringImpl>& action() const LIFETIME_BOUND { return m_action; }
        void parseAction(StringImpl*);

        const RefPtr<AtomStringImpl>& target() const LIFETIME_BOUND { return m_target; }
        void setTarget(AtomStringImpl* target) { m_target = target; }

        const RefPtr<StringImpl>& encodingType() const LIFETIME_BOUND { return m_encodingType; }
        static RefPtr<StringImpl> parseEncodingType(StringImpl*);
        void updateEncodingType(StringImpl*);
        bool isMultiPartForm() const { return m_isMultiPartForm; }

        const RefPtr<StringImpl>& acceptCharset() const LIFETIME_BOUND { return m_acceptCharset; }
        void setAcceptCharset(StringImpl* value) { m_acceptCharset = value; }

    private:
        Method m_method { Method::Get };
        bool m_isMultiPartForm { false };
        RefPtr<StringImpl> m_action;
        RefPtr<AtomStringImpl> m_target;
        RefPtr<StringImpl> m_encodingType { "application/x-www-form-urlencoded"_si };
        RefPtr<StringImpl> m_acceptCharset;
    };

    static RefPtr<FormSubmission> create(HTMLFormElement&, HTMLFormControlElement* overrideSubmitter, const Attributes&, Event*, LockHistory, FormSubmissionTrigger);

    void populateFrameLoadRequest(FrameLoadRequest&);
    URL requestURL() const;

    Method method() const { return m_method; }
    const URL& action() const LIFETIME_BOUND { return m_action; }
    const RefPtr<AtomStringImpl>& target() const LIFETIME_BOUND { return m_target; }
    const RefPtr<StringImpl>& contentType() const LIFETIME_BOUND { return m_contentType; }
    FormState* state() const { return m_formState.get(); }
    FormData& data() const { return *m_formData; }
    const RefPtr<StringImpl>& boundary() const LIFETIME_BOUND { return m_boundary; }
    LockHistory lockHistory() const { return m_lockHistory; }
    Event* event() const { return m_event.get(); }
    const RefPtr<StringImpl>& referrer() const LIFETIME_BOUND { return m_referrer; }
    const RefPtr<StringImpl>& origin() const LIFETIME_BOUND { return m_origin; }

    const RefPtr<StringImpl>& returnValue() const LIFETIME_BOUND { return m_returnValue; }

    void clearTarget() { m_target = nullptr; }
    void setReferrer(StringImpl* referrer) { m_referrer = referrer; }
    void setReferrer(RefPtr<StringImpl>&& referrer) { m_referrer = WTF::move(referrer); }
    void setOrigin(RefPtr<StringImpl>&& origin) { m_origin = WTF::move(origin); }

    void cancel() { m_wasCancelled = true; }
    bool wasCancelled() const { return m_wasCancelled; }

    NewFrameOpenerPolicy newFrameOpenerPolicy() const { return m_newFrameOpenerPolicy; }
    void setNewFrameOpenerPolicy(NewFrameOpenerPolicy newFrameOpenerPolicy) { m_newFrameOpenerPolicy = newFrameOpenerPolicy; }

    ReferrerPolicy referrerPolicy() const { return m_referrerPolicy; }
    void setReferrerPolicy(ReferrerPolicy referrerPolicy) { m_referrerPolicy = referrerPolicy; }

private:
    // dialog form submissions
    FormSubmission(Method, StringImpl* returnValue, const URL& action, AtomStringImpl* target, StringImpl* contentType, LockHistory, Event*);

    // get/post form submissions
    FormSubmission(Method, const URL& action, AtomStringImpl* target, StringImpl* contentType, Ref<FormState>&&, Ref<FormData>&&, StringImpl* boundary, LockHistory, Event*);

    // FIXME: Hold an instance of Attributes instead of individual members.
    Method m_method;
    bool m_wasCancelled { false };
    URL m_action;
    RefPtr<AtomStringImpl> m_target;
    RefPtr<StringImpl> m_contentType;
    const RefPtr<FormState> m_formState;
    const RefPtr<FormData> m_formData;
    RefPtr<StringImpl> m_boundary;
    LockHistory m_lockHistory;
    const RefPtr<Event> m_event;
    RefPtr<StringImpl> m_referrer;
    RefPtr<StringImpl> m_origin;

    RefPtr<StringImpl> m_returnValue; // for form[method=dialog]

    NewFrameOpenerPolicy m_newFrameOpenerPolicy { NewFrameOpenerPolicy::Allow };
    ReferrerPolicy m_referrerPolicy { ReferrerPolicy::EmptyString };
};

} // namespace WebCore
