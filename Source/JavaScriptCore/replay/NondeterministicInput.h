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

#ifndef NondeterministicInput_h
#define NondeterministicInput_h

#if ENABLE(WEB_REPLAY)

#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>

namespace JSC {

class EncodedValue;

enum class InputQueue {
    ScriptMemoizedData = 0,
    LoaderMemoizedData = 1,
    EventLoopInput = 2,
    Count = 3
};

template<typename InputType>
struct JS_EXPORT_PRIVATE InputTraits {
    static InputQueue queue();
    static AtomicString& type();

    static void encode(EncodedValue& encodedInput, InputType& decodedInput);
    static bool decode(EncodedValue& encodedInput, std::unique_ptr<InputType>& decodedInput);
};

class NondeterministicInputBase {
    WTF_MAKE_NONCOPYABLE(NondeterministicInputBase);
public:
    NondeterministicInputBase() { }
    virtual ~NondeterministicInputBase() { }

    virtual const AtomicString& type() const = 0;
    virtual InputQueue queue() const = 0;
};

template<typename InputType>
class NondeterministicInput final : public NondeterministicInputBase {
    virtual const AtomicString& type() const override
    {
        return InputTraits<InputType>::type();
    }

    virtual InputQueue queue() const override
    {
        return InputTraits<InputType>::queue();
    }
};

} // namespace JSC

using JSC::InputQueue;
using JSC::InputTraits;
using JSC::NondeterministicInput;
using JSC::NondeterministicInputBase;

#endif // ENABLE(WEB_REPLAY)

#endif // NondeterministicInput_h
