/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#pragma once

#include "ArgList.h"
#include "ErrorInstance.h"
#include "JSCJSValue.h"
#include "JSCell.h"
#include "JSGlobalObject.h"
#include "Structure.h"
#include "VM.h"
#include <wtf/text/WTFString.h>

namespace JSC {

class AggregateError final : public ErrorInstance {
public:
    using Base = ErrorInstance;
    static constexpr unsigned StructureFlags = Base::StructureFlags;
    static constexpr bool needsDestruction = Base::needsDestruction;

    DECLARE_EXPORT_INFO;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(ErrorInstanceType, StructureFlags), info());
    }

    static AggregateError* create(JSGlobalObject* globalObject, VM& vm, Structure* structure, const MarkedArgumentBuffer& errors, const String& message, JSValue cause, SourceAppender appender = nullptr, RuntimeType type = TypeNothing, bool useCurrentFrame = true)
    {
        auto* instance = new (NotNull, allocateCell<AggregateError>(vm.heap)) AggregateError(vm, structure);
        instance->finishCreation(vm, globalObject, errors, message, cause, appender, type, useCurrentFrame);
        return instance;
    }

    static AggregateError* create(JSGlobalObject*, VM&, Structure*, JSValue errors, JSValue message, JSValue options, SourceAppender = nullptr, RuntimeType = TypeNothing, bool useCurrentFrame = true);

    void finishCreation(VM&, JSGlobalObject*, const MarkedArgumentBuffer& errors, const String& message, JSValue cause, SourceAppender = nullptr, RuntimeType = TypeNothing, bool useCurrentFrame = true);

private:
    explicit AggregateError(VM&, Structure*);
};

STATIC_ASSERT_ISO_SUBSPACE_SHARABLE(AggregateError, AggregateError::Base);

} // namespace JSC
