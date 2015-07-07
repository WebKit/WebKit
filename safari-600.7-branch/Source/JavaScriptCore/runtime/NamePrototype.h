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

#ifndef NamePrototype_h
#define NamePrototype_h

#include "NameInstance.h"

namespace JSC {

class NamePrototype : public NameInstance {
public:
    typedef NameInstance Base;

    static NamePrototype* create(ExecState* exec, Structure* structure)
    {
        VM& vm = exec->vm();
        NamePrototype* prototype = new (NotNull, allocateCell<NamePrototype>(vm.heap)) NamePrototype(exec, structure);
        prototype->finishCreation(vm);
        return prototype;
    }
    
    DECLARE_INFO;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(NameInstanceType, StructureFlags), info());
    }

protected:
    NamePrototype(ExecState*, Structure*);
    void finishCreation(VM&);

    static const unsigned StructureFlags = OverridesGetOwnPropertySlot | NameInstance::StructureFlags;

private:
    static bool getOwnPropertySlot(JSObject*, ExecState*, PropertyName, PropertySlot&);
};

} // namespace JSC

#endif // NamePrototype_h
