/*
 * Copyright (C) 2018-2023 Apple Inc. All rights reserved.
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
#include "MetadataTable.h"

#include "JSCJSValueInlines.h"
#include "OpcodeInlines.h"
#include "UnlinkedMetadataTableInlines.h"

namespace JSC {

MetadataTable::MetadataTable(UnlinkedMetadataTable& unlinkedMetadata)
{
    new (&linkingData()) UnlinkedMetadataTable::LinkingData {
        unlinkedMetadata,
        1,
    };
}

struct DeallocTable {
    template<typename Op>
    static void withOpcodeType(MetadataTable* table)
    {
        if constexpr (static_cast<unsigned>(Op::opcodeID) < NUMBER_OF_BYTECODE_WITH_METADATA) {
            table->forEach<Op>([](auto& entry) {
                entry.~Metadata();
            });
        }
    }
};

MetadataTable::~MetadataTable()
{
    for (unsigned i = 0; i < NUMBER_OF_BYTECODE_WITH_METADATA; i++)
        getOpcodeType<DeallocTable>(static_cast<OpcodeID>(i), this);
    linkingData().~LinkingData();
}

void MetadataTable::destroy(MetadataTable* table)
{
    RefPtr<UnlinkedMetadataTable> unlinkedMetadata = WTFMove(table->linkingData().unlinkedMetadata);

    table->~MetadataTable();

    // FIXME: This check should really not be necessary, see https://webkit.org/b/272787
    if (UNLIKELY(!unlinkedMetadata)) {
        ASSERT_NOT_REACHED();
        return;
    }

    // Since UnlinkedMetadata::unlink frees the underlying memory of MetadataTable.
    // We need to destroy LinkingData before calling it.
    unlinkedMetadata->unlink(*table);
}

size_t MetadataTable::sizeInBytesForGC()
{
    return unlinkedMetadata()->sizeInBytesForGC(*this);
}

void MetadataTable::validate() const
{
    auto getOffset = [&](unsigned i) {
        unsigned offset = offsetTable16()[i];
        if (offset)
            return offset;
        return offsetTable32()[i];
    };
    UNUSED_PARAM(getOffset);
    ASSERT(getOffset(0) >= (is32Bit() ? UnlinkedMetadataTable::s_offset16TableSize + UnlinkedMetadataTable::s_offset32TableSize : UnlinkedMetadataTable::s_offset16TableSize));
    for (unsigned i = 1; i < UnlinkedMetadataTable::s_offsetTableEntries; ++i)
        ASSERT(getOffset(i-1) <= getOffset(i));
}

} // namespace JSC
