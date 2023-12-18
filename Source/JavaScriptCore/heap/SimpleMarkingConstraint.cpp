/*
 * Copyright (C) 2017-2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SimpleMarkingConstraint.h"
#include <wtf/TZoneMallocInlines.h>

namespace JSC {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SimpleMarkingConstraint);

SimpleMarkingConstraint::SimpleMarkingConstraint(
    CString abbreviatedName, CString name,
    MarkingConstraintExecutorPair&& executors,
    ConstraintVolatility volatility, ConstraintConcurrency concurrency,
    ConstraintParallelism parallelism)
    : MarkingConstraint(WTFMove(abbreviatedName), WTFMove(name), volatility, concurrency, parallelism)
    , m_executors(WTFMove(executors))
{
}

SimpleMarkingConstraint::~SimpleMarkingConstraint()
{
}

template<typename Visitor>
void SimpleMarkingConstraint::executeImplImpl(Visitor& visitor)
{
    m_executors.execute(visitor);
}

void SimpleMarkingConstraint::executeImpl(AbstractSlotVisitor& visitor) { executeImplImpl(visitor); }
void SimpleMarkingConstraint::executeImpl(SlotVisitor& visitor) { executeImplImpl(visitor); }

} // namespace JSC

