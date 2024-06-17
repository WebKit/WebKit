/*
 * Copyright (C) 2008-2024 Apple Inc. All rights reserved.
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

#include "config.h"
#include "SharedJITStubSet.h"

#include "BaselineJITRegisters.h"
#include "CacheableIdentifierInlines.h"
#include "DFGJITCode.h"
#include "InlineCacheCompiler.h"
#include "Repatch.h"

namespace JSC {

#if ENABLE(JIT)

RefPtr<PolymorphicAccessJITStubRoutine> SharedJITStubSet::getStatelessStub(StatelessCacheKey key) const
{
    return m_statelessStubs.get(key);
}

void SharedJITStubSet::setStatelessStub(StatelessCacheKey key, Ref<PolymorphicAccessJITStubRoutine> stub)
{
    m_statelessStubs.add(key, WTFMove(stub));
}

MacroAssemblerCodeRef<JITStubRoutinePtrTag> SharedJITStubSet::getDOMJITCode(DOMJITCacheKey key) const
{
    return m_domJITCodes.get(key);
}

void SharedJITStubSet::setDOMJITCode(DOMJITCacheKey key, MacroAssemblerCodeRef<JITStubRoutinePtrTag> code)
{
    m_domJITCodes.add(key, WTFMove(code));
}

RefPtr<InlineCacheHandler> SharedJITStubSet::getSlowPathHandler(AccessType type) const
{
    return m_slowPathHandlers[static_cast<unsigned>(type)];
}

void SharedJITStubSet::setSlowPathHandler(AccessType type, Ref<InlineCacheHandler> handler)
{
    m_slowPathHandlers[static_cast<unsigned>(type)] = WTFMove(handler);
}

#endif // ENABLE(JIT)

} // namespace JSC
