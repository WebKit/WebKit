/*
 * Copyright (C) 2013 University of Washington. All rights reserved.
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ReplayingInputCursor_h
#define ReplayingInputCursor_h

#if ENABLE(WEB_REPLAY)

#include <replay/InputCursor.h>
#include <wtf/Vector.h>

namespace WebCore {

class EventLoopInputBase;
class EventLoopInputDispatcher;
class EventLoopInputDispatcherClient;
class Page;
class ReplaySessionSegment;

struct EventLoopInputData {
    EventLoopInputBase* input;
    double timestamp;
};

class ReplayingInputCursor final : public InputCursor {
    WTF_MAKE_NONCOPYABLE(ReplayingInputCursor);
public:
    static Ref<ReplayingInputCursor> create(RefPtr<ReplaySessionSegment>&&, Page&, EventLoopInputDispatcherClient*);
    virtual ~ReplayingInputCursor();

    bool isCapturing() const override { return false; }
    bool isReplaying() const override { return true; }

    EventLoopInputDispatcher& dispatcher() const { return *m_dispatcher; }

    EventLoopInputData loadEventLoopInput();
protected:
    NondeterministicInputBase* loadInput(InputQueue, const String& type) override;
private:
    ReplayingInputCursor(RefPtr<ReplaySessionSegment>&&, Page&, EventLoopInputDispatcherClient*);

    void storeInput(std::unique_ptr<NondeterministicInputBase>) override;
    NondeterministicInputBase* uncheckedLoadInput(InputQueue) override;

    RefPtr<ReplaySessionSegment> m_segment;
    std::unique_ptr<EventLoopInputDispatcher> m_dispatcher;
    Vector<size_t> m_positions;
};

} // namespace WebCore

#endif // ENABLE(WEB_REPLAY)

#endif // ReplayingInputCursor_h
