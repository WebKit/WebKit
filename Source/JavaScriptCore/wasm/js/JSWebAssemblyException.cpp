/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "JSWebAssemblyException.h"

#if ENABLE(WEBASSEMBLY)

#include "AuxiliaryBarrierInlines.h"
#include "JSBigInt.h"
#include "JSCJSValueInlines.h"
#include "JSCellInlines.h"
#include "JSWebAssemblyHelpers.h"
#include "StructureInlines.h"

namespace JSC {

const ClassInfo JSWebAssemblyException::s_info = { "WebAssembly.Exception"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSWebAssemblyException) };

Structure* JSWebAssemblyException::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ErrorInstanceType, StructureFlags), info());
}

JSWebAssemblyException::JSWebAssemblyException(VM& vm, Structure* structure, const Wasm::Tag& tag, FixedVector<uint64_t>&& payload)
    : Base(vm, structure)
    , m_tag(Ref { tag })
    , m_payload(WTFMove(payload))
{
}

template<typename Visitor>
void JSWebAssemblyException::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    Base::visitChildren(cell, visitor);

    auto* exception = jsCast<JSWebAssemblyException*>(cell);
    const auto& tagType = exception->tag().type();
    for (unsigned i = 0; i < tagType.argumentCount(); ++i) {
        if (isRefType(tagType.argumentType(i)))
            visitor.append(bitwise_cast<WriteBarrier<Unknown>>(exception->payload()[i]));
    }
}

DEFINE_VISIT_CHILDREN(JSWebAssemblyException);

void JSWebAssemblyException::destroy(JSCell* cell)
{
    static_cast<JSWebAssemblyException*>(cell)->JSWebAssemblyException::~JSWebAssemblyException();
}

JSValue JSWebAssemblyException::getArg(JSGlobalObject* globalObject, unsigned i) const
{
    ASSERT(i < tag().type().argumentCount());
    return toJSValue(globalObject, tag().type().argumentType(i), payload()[i]);
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
