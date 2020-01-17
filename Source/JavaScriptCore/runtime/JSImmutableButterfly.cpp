/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "JSImmutableButterfly.h"

#include "ButterflyInlines.h"
#include "CodeBlock.h"

namespace JSC {

const ClassInfo JSImmutableButterfly::s_info = { "Immutable Butterfly", nullptr, nullptr, nullptr, CREATE_METHOD_TABLE(JSImmutableButterfly) };

void JSImmutableButterfly::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    ASSERT_GC_OBJECT_INHERITS(cell, info());
    Base::visitChildren(cell, visitor);
    if (!hasContiguous(cell->indexingType())) {
        ASSERT(hasDouble(cell->indexingType()) || hasInt32(cell->indexingType()));
        return;
    }

    Butterfly* butterfly = jsCast<JSImmutableButterfly*>(cell)->toButterfly();
    visitor.appendValuesHidden(butterfly->contiguous().data(), butterfly->publicLength());
}

void JSImmutableButterfly::copyToArguments(JSGlobalObject*, JSValue* firstElementDest, unsigned offset, unsigned length)
{
    for (unsigned i = 0; i < length; ++i) {
        if ((i + offset) < publicLength())
            firstElementDest[i] = get(i + offset);
        else
            firstElementDest[i] = jsUndefined();
    }
}

static_assert(JSImmutableButterfly::offsetOfData() == sizeof(JSImmutableButterfly), "m_header needs to be adjacent to Data");

} // namespace JSC
