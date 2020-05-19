/*
 * Copyright (C) 2017 Oleksandr Skachkov <gskachkov@gmail.com>.
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "AsyncGeneratorPrototype.h"

#include "JSCInlines.h"

#include "AsyncGeneratorPrototype.lut.h"

namespace JSC {

const ClassInfo AsyncGeneratorPrototype::s_info = { "AsyncGenerator", &Base::s_info, &asyncGeneratorPrototypeTable, nullptr, CREATE_METHOD_TABLE(AsyncGeneratorPrototype) };

/* Source for AsyncGeneratorPrototype.lut.h
@begin asyncGeneratorPrototypeTable
  next      JSBuiltin    DontEnum|Function 1
  return    JSBuiltin    DontEnum|Function 1
  throw     JSBuiltin    DontEnum|Function 1
@end
*/

void AsyncGeneratorPrototype::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

} // namespace JSC
