
/*
 * Copyright (C) 2019-2023 Apple Inc. All rights reserved.
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
#include "WasmFunctionIPIntMetadataGenerator.h"

#include "WasmTypeDefinition.h"
#include <numeric>
#include <wtf/TZoneMallocInlines.h>

#if ENABLE(WEBASSEMBLY)

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

namespace Wasm {

WTF_MAKE_TZONE_ALLOCATED_IMPL(FunctionIPIntMetadataGenerator);

void FunctionIPIntMetadataGenerator::addLength(size_t length)
{
    IPInt::InstructionLengthMetadata instructionLength {
        .length = safeCast<uint8_t>(length)
    };
    size_t size = m_metadata.size();
    m_metadata.grow(size + sizeof(instructionLength));
    WRITE_TO_METADATA(m_metadata.mutableSpan().data() + size, instructionLength, IPInt::InstructionLengthMetadata);
}

void FunctionIPIntMetadataGenerator::addMemorySize(uint8_t memoryIndex)
{
    IPInt::MemorySizeMetadata md {
        .memoryIndex = memoryIndex
    };
    appendMetadata(md);
}

void FunctionIPIntMetadataGenerator::addMemoryGrow(uint8_t memoryIndex)
{
    IPInt::MemoryGrowMetadata md {
        .memoryIndex = memoryIndex
    };
    appendMetadata(md);
}

void FunctionIPIntMetadataGenerator::addTableAccess(uint32_t index, size_t length)
{
    IPInt::TableAccessMetadata md {
        .index = index,
        .instructionLength = { .length = safeCast<uint8_t>(length) }
    };
    appendMetadata(md);
}

void FunctionIPIntMetadataGenerator::addRefFunc(uint32_t index, size_t length)
{
    IPInt::RefFuncMetadata md {
        .index = index,
        .instructionLength = { .length = safeCast<uint8_t>(length) }
    };
    appendMetadata(md);
}

void FunctionIPIntMetadataGenerator::addElemDrop(uint32_t index, size_t length)
{
    IPInt::ElemDropMetadata md {
        .index = index,
        .instructionLength = { .length = safeCast<uint8_t>(length) }
    };
    appendMetadata(md);
}

void FunctionIPIntMetadataGenerator::addDataAccess(uint32_t index, size_t length)
{
    IPInt::DataAccessMetadata md {
        .index = index,
        .instructionLength = { .length = safeCast<uint8_t>(length) }
    };
    appendMetadata(md);
}

void FunctionIPIntMetadataGenerator::addMemoryInit(uint8_t memoryIndex, uint32_t dataIndex, size_t length)
{
    IPInt::MemoryInitMetadata md {
        .memoryIndex = memoryIndex,
        .dataIndex = dataIndex,
        .instructionLength = { .length = safeCast<uint8_t>(length) }
    };
    appendMetadata(md);
}

void FunctionIPIntMetadataGenerator::addMemoryFill(uint8_t memoryIndex, size_t length)
{
    IPInt::MemoryFillMetadata md {
        .memoryIndex = memoryIndex,
        .instructionLength = { .length = safeCast<uint8_t>(length) }
    };
    appendMetadata(md);
}

void FunctionIPIntMetadataGenerator::addMemoryCopy(uint8_t dstMemoryIndex, uint8_t srcMemoryIndex, size_t length)
{
    IPInt::MemoryCopyMetadata md {
        .dstMemoryIndex = dstMemoryIndex,
        .srcMemoryIndex = srcMemoryIndex,
        .instructionLength = { .length = safeCast<uint8_t>(length) }
    };
    appendMetadata(md);
}

void FunctionIPIntMetadataGenerator::addAtomicMemoryAccess(uint8_t memoryIndex, uint64_t offset, size_t length)
{
    IPInt::AtomicMemoryAccessMetadata md {
        .memoryIndex = memoryIndex,
        .offset = offset,
        .instructionLength = { .length = safeCast<uint8_t>(length) }
    };
    appendMetadata(md);
}


} }

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBASSEMBLY)
