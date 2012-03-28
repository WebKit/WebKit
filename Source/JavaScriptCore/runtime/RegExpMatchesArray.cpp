/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#include "RegExpMatchesArray.h"

namespace JSC {

ASSERT_CLASS_FITS_IN_CELL(RegExpMatchesArray);

const ClassInfo RegExpMatchesArray::s_info = {"Array", &JSArray::s_info, 0, 0, CREATE_METHOD_TABLE(RegExpMatchesArray)};

void RegExpMatchesArray::finishCreation(JSGlobalData& globalData)
{
    Base::finishCreation(globalData, m_regExp->numSubpatterns() + 1);
}

void RegExpMatchesArray::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    RegExpMatchesArray* thisObject = jsCast<RegExpMatchesArray*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, &s_info);
    COMPILE_ASSERT(StructureFlags & OverridesVisitChildren, OverridesVisitChildrenWithoutSettingFlag);
    ASSERT(thisObject->structure()->typeInfo().overridesVisitChildren());

    Base::visitChildren(thisObject, visitor);
    visitor.append(&thisObject->m_input);
    visitor.append(&thisObject->m_regExp);
}

void RegExpMatchesArray::reifyAllProperties(ExecState* exec)
{
    ASSERT(m_state != ReifiedAll);
    ASSERT(m_result);
 
    reifyMatchPropertyIfNecessary(exec);

    if (unsigned numSubpatterns = m_regExp->numSubpatterns()) {
        Vector<int, 32> subpatternResults;
        int position = m_regExp->match(exec->globalData(), m_input->value(exec), m_result.start, subpatternResults);
        ASSERT_UNUSED(position, position >= 0 && static_cast<size_t>(position) == m_result.start);
        ASSERT(m_result.start == static_cast<size_t>(subpatternResults[0]));
        ASSERT(m_result.end == static_cast<size_t>(subpatternResults[1]));

        for (unsigned i = 1; i <= numSubpatterns; ++i) {
            int start = subpatternResults[2 * i];
            if (start >= 0)
                putDirectIndex(exec, i, jsSubstring(exec, m_input.get(), start, subpatternResults[2 * i + 1] - start), false);
            else
                putDirectIndex(exec, i, jsUndefined(), false);
        }
    }

    PutPropertySlot slot;
    JSArray::put(this, exec, exec->propertyNames().index, jsNumber(m_result.start), slot);
    JSArray::put(this, exec, exec->propertyNames().input, m_input.get(), slot);

    m_state = ReifiedAll;
}

void RegExpMatchesArray::reifyMatchProperty(ExecState* exec)
{
    ASSERT(m_state == ReifiedNone);
    ASSERT(m_result);
    putDirectIndex(exec, 0, jsSubstring(exec, m_input.get(), m_result.start, m_result.end - m_result.start), false);
    m_state = ReifiedMatch;
}

JSString* RegExpMatchesArray::leftContext(ExecState* exec)
{
    if (!m_result.start)
        return jsEmptyString(exec);
    return jsSubstring(exec, m_input.get(), 0, m_result.start);
}

JSString* RegExpMatchesArray::rightContext(ExecState* exec)
{
    unsigned length = m_input->length();
    if (m_result.end == length)
        return jsEmptyString(exec);
    return jsSubstring(exec, m_input.get(), m_result.end, length - m_result.end);
}

} // namespace JSC
