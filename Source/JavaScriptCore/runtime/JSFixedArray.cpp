/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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
#include "JSFixedArray.h"

#include "CodeBlock.h"
#include "JSCInlines.h"
#include <wtf/CommaPrinter.h>

namespace JSC {

const ClassInfo JSFixedArray::s_info = { "JSFixedArray", nullptr, nullptr, nullptr, CREATE_METHOD_TABLE(JSFixedArray) };

void JSFixedArray::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    JSFixedArray* thisObject = jsCast<JSFixedArray*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    visitor.appendValuesHidden(thisObject->buffer(), thisObject->size());
}

void JSFixedArray::copyToArguments(ExecState* exec, VirtualRegister firstElementDest, unsigned offset, unsigned length)
{
    for (unsigned i = 0; i < length; ++i) {
        if ((i + offset) < m_size)
            exec->r(firstElementDest + i) = get(i + offset);
        else
            exec->r(firstElementDest + i) = jsUndefined();
    }
}

void JSFixedArray::dumpToStream(const JSCell* cell, PrintStream& out)
{
    VM& vm = cell->vm();
    const auto* thisObject = jsCast<const JSFixedArray*>(cell);
    out.printf("<%p, %s, [%u], [", thisObject, thisObject->className(vm), thisObject->length());
    CommaPrinter comma;
    for (unsigned index = 0; index < thisObject->length(); ++index)
        out.print(comma, thisObject->get(index));
    out.print("]>");
}

} // namespace JSC
