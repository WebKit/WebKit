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

#ifndef FormSubmission_h
#define FormSubmission_h

#include "PlatformString.h"
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class Event;
class FormData;
class FormState;

class FormSubmission : public RefCounted<FormSubmission> {
public:
    enum Method {
        GetMethod,
        PostMethod
    };

    static PassRefPtr<FormSubmission> create(Method, const String& action, const String& target, const String& contentType, PassRefPtr<FormState>, PassRefPtr<FormData>, const String& boundary, bool lockHistory, PassRefPtr<Event>);

    Method method() const { return m_method; }
    String action() const { return m_action; }
    String target() const { return m_target; }
    String contentType() const { return m_contentType; }
    FormState* state() const { return m_formState.get(); }
    FormData* data() const { return m_formData.get(); }
    String boundary() const { return m_boundary; }
    bool lockHistory() const { return m_lockHistory; }
    Event* event() const { return m_event.get(); }

private:
    FormSubmission(Method, const String& action, const String& target, const String& contentType, PassRefPtr<FormState>, PassRefPtr<FormData>, const String& boundary, bool lockHistory, PassRefPtr<Event>);

    Method m_method;
    String m_action;
    String m_target;
    String m_contentType;
    RefPtr<FormState> m_formState;
    RefPtr<FormData> m_formData;
    String m_boundary;
    bool m_lockHistory;
    RefPtr<Event> m_event;
};

}

#endif // FormSubmission_h
