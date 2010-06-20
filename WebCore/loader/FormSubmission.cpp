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

#include "config.h"
#include "FormSubmission.h"

#include "Event.h"
#include "FormData.h"
#include "FormState.h"
#include "Frame.h"
#include "HTMLFormElement.h"
#include <wtf/PassRefPtr.h>

namespace WebCore {

FormSubmission::FormSubmission(Method method, const String& action, const String& target, const String& contentType, PassRefPtr<FormState> state, PassRefPtr<FormData> data, const String& boundary, bool lockHistory, PassRefPtr<Event> event)
    : m_method(method)
    , m_action(action)
    , m_target(target)
    , m_contentType(contentType)
    , m_formState(state)
    , m_formData(data)
    , m_boundary(boundary)
    , m_lockHistory(lockHistory)
    , m_event(event)
{
}


PassRefPtr<FormSubmission> FormSubmission::create(Method method, const String& action, const String& target, const String& contentType, PassRefPtr<FormState> state, PassRefPtr<FormData> data, const String& boundary, bool lockHistory, PassRefPtr<Event> event)
{
    return adoptRef(new FormSubmission(method, action, target, contentType, state, data, boundary, lockHistory, event));
}

}
