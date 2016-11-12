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

#pragma once

#if ENABLE(WEB_REPLAY)

#include <replay/InputCursor.h>
#include <wtf/Noncopyable.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class EventLoopInputExtent;
class Page;
class ReplaySessionSegment;

class CapturingInputCursor final : public InputCursor {
    WTF_MAKE_NONCOPYABLE(CapturingInputCursor);
public:
    static Ref<CapturingInputCursor> create(RefPtr<ReplaySessionSegment>&&);
    virtual ~CapturingInputCursor();

    bool isCapturing() const override { return true; }
    bool isReplaying() const override { return false; }

protected:
    NondeterministicInputBase* loadInput(InputQueue, const String& type) override;

private:
    CapturingInputCursor(RefPtr<ReplaySessionSegment>&&);

    NondeterministicInputBase* uncheckedLoadInput(InputQueue) override;
    void storeInput(std::unique_ptr<NondeterministicInputBase>) override;

    RefPtr<ReplaySessionSegment> m_segment;
};

} // namespace WebCore

#endif // ENABLE(WEB_REPLAY)
