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

#include "JSProxy.h"

namespace JSC {

    class JSNonDestructibleProxy : public JSProxy {
        public:
            using Base = JSProxy;
            static const unsigned StructureFlags = Base::StructureFlags;
            static const bool needsDestruction = false;

            template<typename CellType, SubspaceAccess mode>
                static CompleteSubspace* subspaceFor(VM& vm)
                {
                    // JSProxy is JSDestrucitbleObject, but we make this JSNonDestructibleProxy non-destructible by using non-destructible subspace.
                    // The motivation behind this is (1) except for JSWindowProxy JSProxy does not need to be destructible, and (2) subspace of destructible
                    // and non-destructible objects are separated and JSProxy is using one MarkedBlock only for JSProxy class in the JSC framework and wasting memory.
                    // Basically, to make objects destructible, objects need to inherit JSDestructibleObject. It holds a classInfo at a specific offset
                    // so that Heap can get methodTable::destroy even if structures held by objects are destroyed before objects' destructions. But this
                    // requirement forces JSProxy to inherit JSDestructibleObject for JSWindowProxy even while the other JSProxy does not need to be
                    // destructible. We create JSNonDestructibleProxy, which is a subclass of JSProxy, and make it non-destructible so that we still keep
                    // JSWindowProxy destructible while making JSNonDestructibleProxy non-destructible.
                    return JSNonFinalObject::subspaceFor<CellType, mode>(vm);
                }

            static JSNonDestructibleProxy* create(VM& vm, Structure* structure, JSObject* target)
            {
                JSNonDestructibleProxy* proxy = new (NotNull, allocateCell<JSNonDestructibleProxy>(vm.heap)) JSNonDestructibleProxy(vm, structure);
                proxy->finishCreation(vm, target);
                return proxy;
            }

            static JSNonDestructibleProxy* create(VM& vm, Structure* structure)
            {
                JSNonDestructibleProxy* proxy = new (NotNull, allocateCell<JSNonDestructibleProxy>(vm.heap)) JSNonDestructibleProxy(vm, structure);
                proxy->finishCreation(vm);
                return proxy;
            }

            static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype, JSType proxyType)
            {
                ASSERT(proxyType == ImpureProxyType || proxyType == PureForwardingProxyType);
                return Structure::create(vm, globalObject, prototype, TypeInfo(proxyType, StructureFlags), info());
            }

            DECLARE_EXPORT_INFO;

        protected:
            JSNonDestructibleProxy(VM& vm, Structure* structure)
                : Base(vm, structure)
            {
            }
    };

} // namespace JSC
