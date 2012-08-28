/*
 * Copyright (C) 2012 Apple Inc. All Rights Reserved.
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

#ifndef JSScope_h
#define JSScope_h

#include "JSObject.h"

namespace JSC {

class JSScope : public JSNonFinalObject {
public:
    typedef JSNonFinalObject Base;

    static JSValue resolve(CallFrame*, const Identifier&);
    static JSValue resolveSkip(CallFrame*, const Identifier&, int skip);
    static JSValue resolveGlobal(
        CallFrame*,
        const Identifier&,
        JSGlobalObject* globalObject,
        WriteBarrierBase<Structure>* cachedStructure,
        PropertyOffset* cachedOffset
    );
    static JSValue resolveGlobalDynamic(
        CallFrame*,
        const Identifier&,
        int skip,
        WriteBarrierBase<Structure>* cachedStructure,
        PropertyOffset* cachedOffset
    );
    static JSValue resolveBase(CallFrame*, const Identifier&, bool isStrict);
    static JSValue resolveWithBase(CallFrame*, const Identifier&, Register* base);
    static JSValue resolveWithThis(CallFrame*, const Identifier&, Register* base);

    bool isDynamicScope(bool& requiresDynamicChecks) const;

protected:
    JSScope(JSGlobalData&, Structure*);
};

inline JSScope::JSScope(JSGlobalData& globalData, Structure* structure)
    : Base(globalData, structure)
{
}

} // namespace JSC

#endif // JSScope_h
