/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "ProxyObjectAccessCase.h"

#if ENABLE(JIT)

#include "CCallHelpers.h"
#include "CacheableIdentifierInlines.h"
#include "InlineCacheCompiler.h"
#include "LinkBuffer.h"
#include "LinkTimeConstant.h"
#include "ProxyObject.h"
#include "StructureStubInfo.h"

namespace JSC {

ProxyObjectAccessCase::ProxyObjectAccessCase(VM& vm, JSCell* owner, AccessType type, CacheableIdentifier identifier)
    : Base(vm, owner, type, identifier, invalidOffset, nullptr, ObjectPropertyConditionSet(), nullptr)
{
}

ProxyObjectAccessCase::ProxyObjectAccessCase(const ProxyObjectAccessCase& other)
    : Base(other)
{
    // Keep m_callLinkInfo nullptr.
}

Ref<AccessCase> ProxyObjectAccessCase::create(VM& vm, JSCell* owner, AccessType type, CacheableIdentifier identifier)
{
    return adoptRef(*new ProxyObjectAccessCase(vm, owner, type, identifier));
}

Ref<AccessCase> ProxyObjectAccessCase::cloneImpl() const
{
    auto result = adoptRef(*new ProxyObjectAccessCase(*this));
    result->resetState();
    return result;
}

void ProxyObjectAccessCase::dumpImpl(PrintStream& out, CommaPrinter& comma, Indenter& indent) const
{
    Base::dumpImpl(out, comma, indent);
}

} // namespace JSC

#endif // ENABLE(JIT)
