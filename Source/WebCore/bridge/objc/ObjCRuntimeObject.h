/*
 * Copyright (C) 2010-2021 Apple Inc. All rights reserved.
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

#ifndef ObjCRuntimeObject_h
#define ObjCRuntimeObject_h

#include "runtime_object.h"

namespace JSC {
namespace Bindings {

class ObjcInstance;

class ObjCRuntimeObject final : public RuntimeObject {
public:
    using Base = RuntimeObject;

    static ObjCRuntimeObject* create(VM& vm, Structure* structure, RefPtr<ObjcInstance>&& inst)
    {
        ObjCRuntimeObject* object = new (NotNull, allocateCell<ObjCRuntimeObject>(vm)) ObjCRuntimeObject(vm, structure, WTFMove(inst));
        object->finishCreation(vm);
        return object;
    }

    ObjcInstance* getInternalObjCInstance() const;

    DECLARE_INFO;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
    }

private:
    ObjCRuntimeObject(VM&, Structure*, RefPtr<ObjcInstance>&&);
    void finishCreation(VM&);
};

}
}

#endif
