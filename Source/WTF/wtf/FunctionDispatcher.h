/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/Function.h>
#include <wtf/ThreadSafetyAnalysis.h>

namespace WTF {

// FunctionDispatcher is an abstract representation of something that functions can be
// dispatched to. This can for example be a run loop or a work queue.

class WTF_EXPORT_PRIVATE FunctionDispatcher {
public:
    virtual ~FunctionDispatcher();

    virtual void dispatch(Function<void ()>&&) = 0;

protected:
    FunctionDispatcher();
};

class WTF_CAPABILITY("is current") WTF_EXPORT_PRIVATE SerialFunctionDispatcher : public FunctionDispatcher {
public:
    virtual bool isCurrent() const = 0;
};

// A RefCountedSerialFunctionDispatcher guarantees that a dispatched function will always be run.
class RefCountedSerialFunctionDispatcher : public SerialFunctionDispatcher {
public:
    virtual void ref() const = 0;
    virtual void deref() const = 0;
};

inline void assertIsCurrent(const SerialFunctionDispatcher& queue) WTF_ASSERTS_ACQUIRED_CAPABILITY(queue)
{
    ASSERT(queue.isCurrent());
#if !ASSERT_ENABLED
    UNUSED_PARAM(queue);
#endif
}

} // namespace WTF

using WTF::FunctionDispatcher;
using WTF::SerialFunctionDispatcher;
using WTF::RefCountedSerialFunctionDispatcher;
