/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include <wtf/Lock.h>
#include <wtf/SharedTask.h>

namespace JSC {

template<typename OuterType, typename InnerType, typename UnwrapFunc>
class ParallelSourceAdapter final : public SharedTask<InnerType()> {
public:
    ParallelSourceAdapter(RefPtr<SharedTask<OuterType()>> outerSource, const UnwrapFunc& unwrapFunc)
        : m_outerSource(outerSource)
        , m_unwrapFunc(unwrapFunc)
    {
    }
    
    InnerType run() final
    {
        Locker locker { m_lock };
        do {
            if (m_innerSource) {
                if (InnerType result = m_innerSource->run())
                    return result;
                m_innerSource = nullptr;
            }
            
            m_innerSource = m_unwrapFunc(m_outerSource->run());
        } while (m_innerSource);
        return InnerType();
    }

private:
    RefPtr<SharedTask<OuterType()>> m_outerSource;
    RefPtr<SharedTask<InnerType()>> m_innerSource;
    UnwrapFunc m_unwrapFunc;
    Lock m_lock;
};

template<typename OuterType, typename InnerType, typename UnwrapFunc>
Ref<ParallelSourceAdapter<OuterType, InnerType, UnwrapFunc>> createParallelSourceAdapter(RefPtr<SharedTask<OuterType()>> outerSource, const UnwrapFunc& unwrapFunc)
{
    return adoptRef(*new ParallelSourceAdapter<OuterType, InnerType, UnwrapFunc>(outerSource, unwrapFunc));
}
    
} // namespace JSC

