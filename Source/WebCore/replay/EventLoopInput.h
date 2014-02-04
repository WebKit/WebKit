/*
 * Copyright (C) 2011-2013 University of Washington. All rights reserved.
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

#ifndef EventLoopInput_h
#define EventLoopInput_h

#if ENABLE(WEB_REPLAY)

#include <replay/NondeterministicInput.h>
#include <wtf/CurrentTime.h>

namespace WebCore {

class ReplayController;

struct ReplayPosition {
public:
    ReplayPosition()
        : m_index(0)
        , m_time(0.0) { }

    explicit ReplayPosition(unsigned index)
        : m_index(index)
        , m_time(monotonicallyIncreasingTime()) { }

    unsigned index() const { return m_index; }
    double time() const { return m_time; }
private:
    unsigned m_index;
    double m_time;
};

class EventLoopInputBase : public NondeterministicInputBase {
public:
    EventLoopInputBase()
        : m_position(ReplayPosition()) { }

    virtual ~EventLoopInputBase() { }
    virtual InputQueue queue() const override final { return InputQueue::EventLoopInput; }

    virtual void dispatch(ReplayController&) = 0;

    // During capture, the position is set when the following event loop input is captured.
    void setPosition(const ReplayPosition& position) { m_position = position; }
    ReplayPosition position() const { return m_position; }
protected:
    ReplayPosition m_position;
};

template <typename InputType>
class EventLoopInput : public EventLoopInputBase {
    virtual const AtomicString& type() const override final
    {
        return InputTraits<InputType>::type();
    }
};

} // namespace WebCore

#endif // ENABLE(WEB_REPLAY)

#endif // EventLoopInput_h
