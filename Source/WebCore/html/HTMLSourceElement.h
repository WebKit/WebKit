/*
 * Copyright (C) 2007-2017 Apple Inc. All rights reserved.
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
#include "Timer.h"

namespace WebCore {

class MediaQuerySet;

class HTMLSourceElement final : public HTMLElement, private ActiveDOMObject {
    WTF_MAKE_ISO_ALLOCATED(HTMLSourceElement);
public:
    static Ref<HTMLSourceElement> create(Document&);
    static Ref<HTMLSourceElement> create(const QualifiedName&, Document&);

    void scheduleErrorEvent();
    void cancelPendingErrorEvent();

    const MediaQuerySet* parsedMediaAttribute(Document&) const;

private:
    HTMLSourceElement(const QualifiedName&, Document&);
    
    InsertedIntoAncestorResult insertedIntoAncestor(InsertionType, ContainerNode&) final;
    void removedFromAncestor(RemovalType, ContainerNode&) final;
    bool isURLAttribute(const Attribute&) const final;

    // ActiveDOMObject.
    const char* activeDOMObjectName() const final;
    bool canSuspendForDocumentSuspension() const final;
    void suspend(ReasonForSuspension) final;
    void resume() final;
    void stop() final;

    void parseAttribute(const QualifiedName&, const AtomicString&) final;

    void errorEventTimerFired();

    Timer m_errorEventTimer;
    bool m_shouldRescheduleErrorEventOnResume { false };
    mutable Optional<RefPtr<const MediaQuerySet>> m_cachedParsedMediaAttribute;
};

} // namespace WebCore
