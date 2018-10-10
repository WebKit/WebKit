/*
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
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

uint32_t Table::allocatedLength(uint32_t length)
{
    return WTF::roundUpToPowerOfTwo(length);
}

void Table::setLength(uint32_t length)
{
    m_length = length;
    m_mask = WTF::maskForSize(length);
    ASSERT(isValidLength(length));
    ASSERT(m_mask == WTF::maskForSize(allocatedLength(length)));
}

RefPtr<Table> Table::tryCreate(uint32_t initial, std::optional<uint32_t> maximum)
{
    if (!isValidLength(initial))
        return nullptr;
    return adoptRef(new (NotNull, fastMalloc(sizeof(Table))) Table(initial, maximum));
}

Table::~Table()
{
}

Table::Table(uint32_t initial, std::optional<uint32_t> maximum)
{
    setLength(initial);
    m_maximum = maximum;
    ASSERT(!m_maximum || *m_maximum >= m_length);

    // FIXME: It might be worth trying to pre-allocate maximum here. The spec recommends doing so.
    // But for now, we're not doing that.
    m_importableFunctions = MallocPtr<WasmToWasmImportableFunction>::malloc((sizeof(WasmToWasmImportableFunction) * Checked<size_t>(allocatedLength(m_length))).unsafeGet());
    // FIXME this over-allocates and could be smarter about not committing all of that memory https://bugs.webkit.org/show_bug.cgi?id=181425
    m_instances = MallocPtr<Instance*>::malloc((sizeof(Instance*) * Checked<size_t>(allocatedLength(m_length))).unsafeGet());
    for (uint32_t i = 0; i < allocatedLength(m_length); ++i) {
        new (&m_importableFunctions.get()[i]) WasmToWasmImportableFunction();
        ASSERT(m_importableFunctions.get()[i].signatureIndex == Wasm::Signature::invalidIndex); // We rely on this in compiled code.
        m_instances.get()[i] = nullptr;
    }
}

std::optional<uint32_t> Table::grow(uint32_t delta)
{
    if (delta == 0)
        return length();

    using Checked = Checked<uint32_t, RecordOverflow>;
    Checked newLengthChecked = length();
    newLengthChecked += delta;
    uint32_t newLength;
    if (newLengthChecked.safeGet(newLength) == CheckedState::DidOverflow)
        return std::nullopt;

    if (maximum() && newLength > *maximum())
        return std::nullopt;
    if (!isValidLength(newLength))
        return std::nullopt;

    auto checkedGrow = [&] (auto& container) {
        if (newLengthChecked.unsafeGet() > allocatedLength(m_length)) {
            Checked reallocSizeChecked = allocatedLength(newLengthChecked.unsafeGet());
            reallocSizeChecked *= sizeof(*container.get());
            uint32_t reallocSize;
            if (reallocSizeChecked.safeGet(reallocSize) == CheckedState::DidOverflow)
                return false;
            // FIXME this over-allocates and could be smarter about not committing all of that memory https://bugs.webkit.org/show_bug.cgi?id=181425
            container.realloc(reallocSize);
        }
        for (uint32_t i = m_length; i < allocatedLength(newLength); ++i)
            new (&container.get()[i]) std::remove_reference_t<decltype(*container.get())>();
        return true;
    };

    if (!checkedGrow(m_importableFunctions))
        return std::nullopt;
    if (!checkedGrow(m_instances))
        return std::nullopt;

    setLength(newLength);

    return newLength;
}

void Table::clearFunction(uint32_t index)
{
    RELEASE_ASSERT(index < length());
    m_importableFunctions.get()[index & m_mask] = WasmToWasmImportableFunction();
    ASSERT(m_importableFunctions.get()[index & m_mask].signatureIndex == Wasm::Signature::invalidIndex); // We rely on this in compiled code.
    m_instances.get()[index & m_mask] = nullptr;
}

void Table::setFunction(uint32_t index, WasmToWasmImportableFunction function, Instance* instance)
{
    RELEASE_ASSERT(index < length());
    m_importableFunctions.get()[index & m_mask] = function;
    m_instances.get()[index & m_mask] = instance;
}

} } // namespace JSC::Table

#endif // ENABLE(WEBASSEMBLY)
