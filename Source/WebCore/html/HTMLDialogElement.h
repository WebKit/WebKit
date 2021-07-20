/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "HTMLElement.h"

namespace WebCore {

template<typename T> class EventSender;
using DialogEventSender = EventSender<HTMLDialogElement>;

class HTMLDialogElement final : public HTMLElement {
    WTF_MAKE_ISO_ALLOCATED(HTMLDialogElement);
public:
    template<typename... Args> static Ref<HTMLDialogElement> create(Args&&... args) { return adoptRef(*new HTMLDialogElement(std::forward<Args>(args)...)); }
    ~HTMLDialogElement();
    
    bool isOpen() const;

    const String& returnValue();
    void setReturnValue(String&&);

    void show();
    ExceptionOr<void> showModal();
    void close(const String&);

    void dispatchPendingEvent(DialogEventSender*);

    bool isModal() const;

private:
    HTMLDialogElement(const QualifiedName&, Document&);

    void parseAttribute(const QualifiedName&, const AtomString&) final;

    String m_returnValue;
    bool m_isModal { false };
    bool m_isOpen { false };
};

} // namespace WebCore
