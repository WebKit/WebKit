/*
 * Copyright (C) 2015-2022 Apple Inc. All rights reserved.
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

#include "ExceptionStack.h"
#include "JSObject.h"
#include <wtf/Vector.h>

namespace JSC {

class ErrorInstance;

class Exception : public JSCell {
public:
    using Base = JSCell;
    static constexpr unsigned StructureFlags = Base::StructureFlags | StructureIsImmortal;

    template<typename CellType, SubspaceAccess mode>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return &vm.exceptionSpace();
    }

    enum StackCaptureAction : uint8_t {
        CaptureStack,
        DoNotCaptureStack
    };
    JS_EXPORT_PRIVATE static Exception* create(VM&, JSValue thrownValue, StackCaptureAction = CaptureStack);
    JS_EXPORT_PRIVATE static Exception* createWithStackFromError(VM&, ErrorInstance*);
    JS_EXPORT_PRIVATE static Exception* createWithFrameAdjustment(VM&, ErrorInstance*, LineColumn, unsigned callDepth);

    static Structure* createStructure(VM&, JSGlobalObject*, JSValue prototype);

    DECLARE_VISIT_CHILDREN;

    DECLARE_EXPORT_INFO;

    static constexpr ptrdiff_t valueOffset()
    {
        return OBJECT_OFFSETOF(Exception, m_value);
    }

    JSValue value() const { return m_value.get(); }

    bool didNotifyInspectorOfThrow() const { return perCellBit(); }
    void setDidNotifyInspectorOfThrow() { setPerCellBit(true); }

#if ENABLE(WEBASSEMBLY)
    void wrapValueForJSTag(JSGlobalObject*);
#endif

    ~Exception();

    ExceptionStack stack() const;

protected:
    Exception(VM&, Structure*, JSValue thrownValue);

    WriteBarrier<Unknown> m_value;
    LineColumn m_replacedLineColumn { };
    unsigned m_callDepth { };

    friend class LLIntOffsetsExtractor;
};

} // namespace JSC
