/*
 * Copyright (C) 2012-2014 Apple Inc. All rights reserved.
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

#include "ButterflyInlines.h"
#include "JSCInlines.h"

namespace JSC {

JSArray* createRegExpMatchesArray(ExecState* exec, JSString* input, RegExp* regExp, MatchResult result)
{
    ASSERT(result);
    VM& vm = exec->vm();
    JSArray* array = JSArray::tryCreateUninitialized(vm, exec->lexicalGlobalObject()->regExpMatchesArrayStructure(), regExp->numSubpatterns() + 1);
    RELEASE_ASSERT(array);

    SamplingRegion samplingRegion("Reifying substring properties");

    array->putDirectIndex(exec, 0, jsSubstring(exec, input, result.start, result.end - result.start));

    if (unsigned numSubpatterns = regExp->numSubpatterns()) {
        Vector<int, 32> subpatternResults;
        int position = regExp->match(exec->vm(), input->value(exec), result.start, subpatternResults);
        ASSERT_UNUSED(position, position >= 0 && static_cast<size_t>(position) == result.start);
        ASSERT(result.start == static_cast<size_t>(subpatternResults[0]));
        ASSERT(result.end == static_cast<size_t>(subpatternResults[1]));

        for (unsigned i = 1; i <= numSubpatterns; ++i) {
            int start = subpatternResults[2 * i];
            if (start >= 0)
                array->putDirectIndex(exec, i, jsSubstring(exec, input, start, subpatternResults[2 * i + 1] - start));
            else
                array->putDirectIndex(exec, i, jsUndefined());
        }
    }

    array->putDirect(exec->vm(), exec->propertyNames().index, jsNumber(result.start));
    array->putDirect(exec->vm(), exec->propertyNames().input, input);

    return array;
}

} // namespace JSC
