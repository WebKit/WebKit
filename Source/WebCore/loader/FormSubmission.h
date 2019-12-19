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

#include "FormState.h"
#include "FrameLoaderTypes.h"
#include <wtf/URL.h>

namespace WebCore {

class Event;
class FormData;
class FrameLoadRequest;

class FormSubmission : public RefCounted<FormSubmission> {
public:
    enum class Method { Get, Post };

    class Attributes {
    public:
        Method method() const { return m_method; }
        static Method parseMethodType(const String&);
        void updateMethodType(const String&);
        static ASCIILiteral methodString(Method method) { return method == Method::Post ? "post"_s : "get"_s; }

        const String& action() const { return m_action; }
        void parseAction(const String&);

        const String& target() const { return m_target; }
        void setTarget(const String& target) { m_target = target; }

        const String& encodingType() const { return m_encodingType; }
        static String parseEncodingType(const String&);
        void updateEncodingType(const String&);
        bool isMultiPartForm() const { return m_isMultiPartForm; }

        const String& acceptCharset() const { return m_acceptCharset; }
        void setAcceptCharset(const String& value) { m_acceptCharset = value; }

    private:
        Method m_method { Method::Get };
        bool m_isMultiPartForm { false };
        String m_action;
        String m_target;
        String m_encodingType { "application/x-www-form-urlencoded"_s };
        String m_acceptCharset;
    };

    static Ref<FormSubmission> create(HTMLFormElement&, const Attributes&, Event*, LockHistory, FormSubmissionTrigger);

    void populateFrameLoadRequest(FrameLoadRequest&);
    URL requestURL() const;

    Method method() const { return m_method; }
    const URL& action() const { return m_action; }
    const String& target() const { return m_target; }
    const String& contentType() const { return m_contentType; }
    FormState& state() const { return *m_formState; }
    Ref<FormState> takeState() { return m_formState.releaseNonNull(); }
    FormData& data() const { return m_formData; }
    const String boundary() const { return m_boundary; }
    LockHistory lockHistory() const { return m_lockHistory; }
    Event* event() const { return m_event.get(); }
    const String& referrer() const { return m_referrer; }
    const String& origin() const { return m_origin; }

    void clearTarget() { m_target = { }; }
    void setReferrer(const String& referrer) { m_referrer = referrer; }
    void setOrigin(const String& origin) { m_origin = origin; }

private:
    FormSubmission(Method, const URL& action, const String& target, const String& contentType, Ref<FormState>&&, Ref<FormData>&&, const String& boundary, LockHistory, Event*);

    // FIXME: Hold an instance of Attributes instead of individual members.
    Method m_method;
    URL m_action;
    String m_target;
    String m_contentType;
    RefPtr<FormState> m_formState;
    Ref<FormData> m_formData;
    String m_boundary;
    LockHistory m_lockHistory;
    RefPtr<Event> m_event;
    String m_referrer;
    String m_origin;
};

} // namespace WebCore
