/*
 * Copyright (C) 2017 Yusuke Suzuki <utatane.tea@gmail.com>.
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

#include "AdaptiveInferredPropertyValueWatchpointBase.h"

namespace JSC {

template<typename Watchpoint>
class ObjectPropertyChangeAdaptiveWatchpoint final : public AdaptiveInferredPropertyValueWatchpointBase {
public:
    using Base = AdaptiveInferredPropertyValueWatchpointBase;
    ObjectPropertyChangeAdaptiveWatchpoint(JSCell* owner, const ObjectPropertyCondition& condition, Watchpoint& watchpoint)
        : Base(condition)
        , m_owner(owner)
        , m_watchpoint(watchpoint)
    {
        RELEASE_ASSERT(watchpoint.stateOnJSThread() == IsWatched);
    }

private:
    bool isValid() const override
    {
        return m_owner->isLive();
    }

    void handleFire(VM& vm, const FireDetail&) override
    {
        m_watchpoint.fireAll(vm, StringFireDetail("Object Property is changed."));
    }

    JSCell* m_owner;
    Watchpoint& m_watchpoint;
};

} // namespace JSC
