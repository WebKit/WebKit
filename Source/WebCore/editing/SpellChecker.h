/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include "Element.h"
#include "TextChecking.h"
#include "Timer.h"
#include <wtf/Deque.h>

namespace WebCore {

class SpellChecker;
class TextCheckerClient;

class SpellCheckRequest final : public TextCheckingRequest {
public:
    static RefPtr<SpellCheckRequest> create(OptionSet<TextCheckingType>, TextCheckingProcessType, const SimpleRange& checkingRange, const SimpleRange& automaticReplacementRange, const SimpleRange& paragraphRange);
    virtual ~SpellCheckRequest();

    const SimpleRange& checkingRange() const { return m_checkingRange; }
    const SimpleRange& paragraphRange() const { return m_paragraphRange; }
    const SimpleRange& automaticReplacementRange() const { return m_automaticReplacementRange; }
    Element* rootEditableElement() const { return m_rootEditableElement.get(); }

    void setCheckerAndIdentifier(SpellChecker*, TextCheckingRequestIdentifier);
    void requesterDestroyed();
    bool isStarted() const { return m_checker; }

    const TextCheckingRequestData& data() const final;

private:
    void didSucceed(const Vector<TextCheckingResult>&) final;
    void didCancel() final;

    SpellCheckRequest(const SimpleRange& checkingRange, const SimpleRange& automaticReplacementRange, const SimpleRange& paragraphRange, const String&, OptionSet<TextCheckingType>, TextCheckingProcessType);

    SpellChecker* m_checker { nullptr };
    SimpleRange m_checkingRange;
    SimpleRange m_automaticReplacementRange;
    SimpleRange m_paragraphRange;
    RefPtr<Element> m_rootEditableElement;
    TextCheckingRequestData m_requestData;
};

class SpellChecker {
    WTF_MAKE_FAST_ALLOCATED;
public:
    friend class SpellCheckRequest;

    explicit SpellChecker(Document&);
    ~SpellChecker();

    bool isAsynchronousEnabled() const;
    bool isCheckable(const SimpleRange&) const;

    void requestCheckingFor(Ref<SpellCheckRequest>&&);

    TextCheckingRequestIdentifier lastRequestIdentifier() const { return m_lastRequestIdentifier; }
    TextCheckingRequestIdentifier lastProcessedIdentifier() const { return m_lastProcessedIdentifier; }

private:
    bool canCheckAsynchronously(const SimpleRange&) const;
    TextCheckerClient* client() const;
    void timerFiredToProcessQueuedRequest();
    void invokeRequest(Ref<SpellCheckRequest>&&);
    void enqueueRequest(Ref<SpellCheckRequest>&&);
    void didCheckSucceed(TextCheckingRequestIdentifier, const Vector<TextCheckingResult>&);
    void didCheckCancel(TextCheckingRequestIdentifier);
    void didCheck(TextCheckingRequestIdentifier, const Vector<TextCheckingResult>&);

    Document& m_document;
    TextCheckingRequestIdentifier m_lastRequestIdentifier;
    TextCheckingRequestIdentifier m_lastProcessedIdentifier;

    Timer m_timerToProcessQueuedRequest;

    RefPtr<SpellCheckRequest> m_processingRequest;
    Deque<Ref<SpellCheckRequest>> m_requestQueue;
};

} // namespace WebCore
