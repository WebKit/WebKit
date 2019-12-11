/*
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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

#include "JSCInlines.h"
#include "OpcodeInlines.h"
#include "UnlinkedMetadataTableInlines.h"
#include <wtf/FastMalloc.h>

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
        table->forEach<Op>([](auto& entry) {
            entry.~Metadata();
        });
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
    Ref<UnlinkedMetadataTable> unlinkedMetadata = WTFMove(table->linkingData().unlinkedMetadata);
    table->~MetadataTable();
    // Since UnlinkedMetadata::unlink frees the underlying memory of MetadataTable.
    // We need to destroy LinkingData before calling it.
    unlinkedMetadata->unlink(*table);
}

size_t MetadataTable::sizeInBytes()
{
    return linkingData().unlinkedMetadata->sizeInBytes(*this);
}

} // namespace JSC
