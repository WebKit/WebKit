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
#include "ValidationMessage.h"

#include <wtf/PassOwnPtr.h>

namespace WebCore {

ALWAYS_INLINE ValidationMessage::ValidationMessage(HTMLFormControlElement* element)
    : m_element(element)
{
}

ValidationMessage::~ValidationMessage()
{
    hideMessage();
}

PassOwnPtr<ValidationMessage> ValidationMessage::create(HTMLFormControlElement* element)
{
    return adoptPtr(new ValidationMessage(element));
}

void ValidationMessage::setMessage(const String& message)
{
    // FIXME: Construct validation message UI if m_message is empty.

    m_message = message;

    m_timer.set(new Timer<ValidationMessage>(this, &ValidationMessage::hideMessage));
    m_timer->startOneShot(6.0); // FIXME: should be <message length> * something.
}

void ValidationMessage::hideMessage(Timer<ValidationMessage>*)
{
    // FIXME: Implement.

    m_message = String();
}

} // namespace WebCore
