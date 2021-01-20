/*
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

#pragma once

#include "DirectArguments.h"
#include "JSBigInt.h"
#include "JSLexicalEnvironment.h"
#include "JSModuleEnvironment.h"

namespace JSC {

inline constexpr bool isDynamicallySizedType(JSType type)
{
    if (type == DirectArgumentsType
        || type == FinalObjectType
        || type == LexicalEnvironmentType
        || type == ModuleEnvironmentType)
        return true;
    return false;
}

inline size_t cellSize(VM& vm, JSCell* cell)
{
    Structure* structure = cell->structure(vm);
    const ClassInfo* classInfo = structure->classInfo();
    JSType cellType = cell->type();

    if (isDynamicallySizedType(cellType)) {
        switch (cellType) {
        case DirectArgumentsType: {
            auto* args = jsCast<DirectArguments*>(cell);
            return DirectArguments::allocationSize(args->m_minCapacity);
        }
        case FinalObjectType:
            return JSFinalObject::allocationSize(structure->inlineCapacity());
        case LexicalEnvironmentType: {
            auto* env = jsCast<JSLexicalEnvironment*>(cell);
            return JSLexicalEnvironment::allocationSize(env->symbolTable());
        }
        case ModuleEnvironmentType: {
            auto* env = jsCast<JSModuleEnvironment*>(cell);
            return JSModuleEnvironment::allocationSize(env->symbolTable());
        }
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }
    return classInfo->staticClassSize;
}

} // namespace JSC
