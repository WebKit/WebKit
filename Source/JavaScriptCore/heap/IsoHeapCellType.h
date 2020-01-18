/*
 * Copyright (C) 2019-2020 Apple Inc. All rights reserved.
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

#include "HeapCellType.h"
#include "MarkedBlockInlines.h"

namespace JSC {

class IsoHeapCellType final : public HeapCellType {
public:
    using DestroyFunctionPtr = void (*)(JSCell*);

    JS_EXPORT_PRIVATE IsoHeapCellType(DestructionMode, DestroyFunctionPtr);

    template<typename CellType>
    static std::unique_ptr<IsoHeapCellType> create()
    {
        return makeUnique<IsoHeapCellType>(CellType::needsDestruction ? NeedsDestruction : DoesNotNeedDestruction, &CellType::destroy);
    }

    JS_EXPORT_PRIVATE void finishSweep(MarkedBlock::Handle&, FreeList*) override;
    JS_EXPORT_PRIVATE void destroy(VM&, JSCell*) override;

    ALWAYS_INLINE void operator()(VM&, JSCell* cell) const
    {
        m_destroy(cell);
    }

private:
    DestroyFunctionPtr WTF_VTBL_FUNCPTR_PTRAUTH_STR("IsoHeapCellType.destroy") m_destroy;
};

} // namespace JSC
