/*
 * Copyright (C) 2011, 2012 University of Washington. All rights reserved.
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

#ifndef InputCursor_h
#define InputCursor_h

#if ENABLE(WEB_REPLAY)

#include "NondeterministicInput.h"
#include <wtf/Noncopyable.h>
#include <wtf/RefCounted.h>
#include <wtf/text/AtomicString.h>

namespace JSC {

class InputCursor : public RefCounted<InputCursor> {
    WTF_MAKE_NONCOPYABLE(InputCursor);
public:
    InputCursor() { }

    virtual ~InputCursor() { }

    virtual bool isCapturing() const = 0;
    virtual bool isReplaying() const = 0;

    template <class InputType, class... Args> inline
    void appendInput(Args&&... args)
    {
        InputType* rawInput = WTF::safeCast<InputType*>(new InputType(std::forward<Args>(args)...));
        return storeInput(std::unique_ptr<NondeterministicInputBase>(rawInput));
    }

    template <class InputType> inline
    InputType* fetchInput()
    {
        return static_cast<InputType*>(loadInput(InputTraits<InputType>::queue(), InputTraits<InputType>::type()));
    }

    virtual void storeInput(std::unique_ptr<NondeterministicInputBase>) = 0;
    virtual NondeterministicInputBase* uncheckedLoadInput(InputQueue) = 0;
protected:
    virtual NondeterministicInputBase* loadInput(InputQueue, const AtomicString&) = 0;
};

} // namespace JSC

using JSC::InputCursor;

#endif // ENABLE(WEB_REPLAY)

#endif // InputCursor_h
