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

#ifndef FunctorInputCursor_h
#define FunctorInputCursor_h

#if ENABLE(WEB_REPLAY)

#include "SegmentedInputStorage.h"
#include <replay/InputCursor.h>
#include <replay/NondeterministicInput.h>
#include <wtf/Assertions.h>
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>

namespace WebCore {

class FunctorInputCursor final : public InputCursor {
    WTF_MAKE_NONCOPYABLE(FunctorInputCursor);
public:
    FunctorInputCursor(SegmentedInputStorage&);
    virtual ~FunctorInputCursor() { }

    // InputCursor
    virtual bool isCapturing() const override { return false; }
    virtual bool isReplaying() const override { return false; }

    virtual void storeInput(std::unique_ptr<NondeterministicInputBase>) override;
    virtual NondeterministicInputBase* uncheckedLoadInput(InputQueue) override;

    template<typename Functor>
    typename Functor::ReturnType forEachInputInQueue(InputQueue, Functor&);
protected:
    virtual NondeterministicInputBase* loadInput(InputQueue, const AtomicString&) override;
private:
    SegmentedInputStorage& m_storage;
};

template<typename Functor> inline
typename Functor::ReturnType FunctorInputCursor::forEachInputInQueue(InputQueue queue, Functor& functor)
{
    for (size_t i = 0; i < m_storage.queueSize(queue); i++)
        functor(i, m_storage.queue(queue).at(i).get());

    return functor.returnValue();
}

inline FunctorInputCursor::FunctorInputCursor(SegmentedInputStorage& storage)
    : m_storage(storage)
{
}

inline void FunctorInputCursor::storeInput(std::unique_ptr<NondeterministicInputBase>)
{
    ASSERT_NOT_REACHED();
}

inline NondeterministicInputBase* FunctorInputCursor::loadInput(InputQueue, const AtomicString&)
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

inline NondeterministicInputBase* FunctorInputCursor::uncheckedLoadInput(InputQueue)
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

} // namespace WebCore

#endif // ENABLE(WEB_REPLAY)

#endif // FunctorInputCursor_h
