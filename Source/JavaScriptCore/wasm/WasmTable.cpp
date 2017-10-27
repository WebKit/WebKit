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

#include "config.h"
#include "WasmTable.h"

#if ENABLE(WEBASSEMBLY)

#include <wtf/CheckedArithmetic.h>
#include <wtf/StdLibExtras.h>
#include <type_traits>

namespace JSC { namespace Wasm {

RefPtr<Table> Table::create(uint32_t initial, std::optional<uint32_t> maximum)
{
    if (!isValidSize(initial))
        return nullptr;
    return adoptRef(new (NotNull, fastMalloc(sizeof(Table))) Table(initial, maximum));
}

Table::~Table()
{
}

Table::Table(uint32_t initial, std::optional<uint32_t> maximum)
{
    m_size = initial;
    m_maximum = maximum;
    ASSERT(isValidSize(m_size));
    ASSERT(!m_maximum || *m_maximum >= m_size);

    // FIXME: It might be worth trying to pre-allocate maximum here. The spec recommends doing so.
    // But for now, we're not doing that.
    m_functions = MallocPtr<Wasm::CallableFunction>::malloc((sizeof(Wasm::CallableFunction) * Checked<size_t>(size())).unsafeGet());
    m_instances = MallocPtr<Instance*>::malloc((sizeof(Instance*) * Checked<size_t>(size())).unsafeGet());
    for (uint32_t i = 0; i < size(); ++i) {
        new (&m_functions.get()[i]) CallableFunction();
        ASSERT(m_functions.get()[i].signatureIndex == Wasm::Signature::invalidIndex); // We rely on this in compiled code.
        m_instances.get()[i] = nullptr;
    }
}

std::optional<uint32_t> Table::grow(uint32_t delta)
{
    if (delta == 0)
        return size();

    using Checked = Checked<uint32_t, RecordOverflow>;
    Checked newSizeChecked = size();
    newSizeChecked += delta;
    uint32_t newSize;
    if (newSizeChecked.safeGet(newSize) == CheckedState::DidOverflow)
        return std::nullopt;

    if (maximum() && newSize > *maximum())
        return std::nullopt;
    if (!isValidSize(newSize))
        return std::nullopt;

    auto checkedGrow = [&] (auto& container) {
        Checked reallocSizeChecked = newSizeChecked;
        reallocSizeChecked *= sizeof(*container.get());
        uint32_t reallocSize;
        if (reallocSizeChecked.safeGet(reallocSize) == CheckedState::DidOverflow)
            return false;
        container.realloc(reallocSize);
        for (uint32_t i = m_size; i < newSize; ++i)
            new (&container.get()[i]) std::remove_reference_t<decltype(*container.get())>();
        return true;
    };

    if (!checkedGrow(m_functions))
        return std::nullopt;
    if (!checkedGrow(m_instances))
        return std::nullopt;

    m_size = newSize;

    return newSize;
}

void Table::clearFunction(uint32_t index)
{
    RELEASE_ASSERT(index < size());
    m_functions.get()[index] = Wasm::CallableFunction();
    ASSERT(m_functions.get()[index].signatureIndex == Wasm::Signature::invalidIndex); // We rely on this in compiled code.
    m_instances.get()[index] = nullptr;
}

void Table::setFunction(uint32_t index, CallableFunction function, Instance* instance)
{
    RELEASE_ASSERT(index < size());
    m_functions.get()[index] = function;
    m_instances.get()[index] = instance;
}

} } // namespace JSC::Table

#endif // ENABLE(WEBASSEMBLY)
