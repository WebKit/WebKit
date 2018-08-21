/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "Range.h"
#include "TextChecking.h"
#include "Timer.h"
#include <wtf/Deque.h>
#include <wtf/Forward.h>
#include <wtf/RefPtr.h>
#include <wtf/Noncopyable.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class Frame;
class Node;
class TextCheckerClient;
class SpellChecker;

class SpellCheckRequest : public TextCheckingRequest {
public:
    static RefPtr<SpellCheckRequest> create(OptionSet<TextCheckingType>, TextCheckingProcessType, Ref<Range>&& checkingRange, Ref<Range>&& automaticReplacementRange, Ref<Range>&& paragraphRange);
    virtual ~SpellCheckRequest();

    Range& checkingRange() const { return m_checkingRange.get(); }
    Range& paragraphRange() const { return m_paragraphRange.get(); }
    Range& automaticReplacementRange() const { return m_automaticReplacementRange.get(); }
    Element* rootEditableElement() const { return m_rootEditableElement.get(); }

    void setCheckerAndSequence(SpellChecker*, int sequence);
    void requesterDestroyed();
    bool isStarted() const { return m_checker; }

    const TextCheckingRequestData& data() const override;
    void didSucceed(const Vector<TextCheckingResult>&) override;
    void didCancel() override;

private:
    SpellCheckRequest(Ref<Range>&& checkingRange, Ref<Range>&& automaticReplacementRange, Ref<Range>&& paragraphRange, const String&, OptionSet<TextCheckingType>, TextCheckingProcessType);

    SpellChecker* m_checker { nullptr };
    Ref<Range> m_checkingRange;
    Ref<Range> m_automaticReplacementRange;
    Ref<Range> m_paragraphRange;
    RefPtr<Element> m_rootEditableElement;
    TextCheckingRequestData m_requestData;
};

class SpellChecker {
    WTF_MAKE_NONCOPYABLE(SpellChecker); WTF_MAKE_FAST_ALLOCATED;
public:
    friend class SpellCheckRequest;

    explicit SpellChecker(Frame&);
    ~SpellChecker();

    bool isAsynchronousEnabled() const;
    bool isCheckable(Range&) const;

    void requestCheckingFor(Ref<SpellCheckRequest>&&);

    int lastRequestSequence() const
    {
        return m_lastRequestSequence;
    }

    int lastProcessedSequence() const
    {
        return m_lastProcessedSequence;
    }

private:
    typedef Deque<Ref<SpellCheckRequest>> RequestQueue;

    bool canCheckAsynchronously(Range&) const;
    TextCheckerClient* client() const;
    void timerFiredToProcessQueuedRequest();
    void invokeRequest(Ref<SpellCheckRequest>&&);
    void enqueueRequest(Ref<SpellCheckRequest>&&);
    void didCheckSucceed(int sequence, const Vector<TextCheckingResult>&);
    void didCheckCancel(int sequence);
    void didCheck(int sequence, const Vector<TextCheckingResult>&);

    Frame& m_frame;
    int m_lastRequestSequence;
    int m_lastProcessedSequence;

    Timer m_timerToProcessQueuedRequest;

    RefPtr<SpellCheckRequest> m_processingRequest;
    RequestQueue m_requestQueue;
};

} // namespace WebCore
