/*
 * Copyright (C) 2017 Yusuke Suzuki <utatane.tea@gmail.com>.
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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

#include "CustomGetterSetter.h"
#include "DOMAnnotation.h"

namespace JSC {
namespace DOMJIT {

class GetterSetter;

}

class DOMAttributeGetterSetter final : public CustomGetterSetter {
public:
    using Base = CustomGetterSetter;

    template<typename CellType, SubspaceAccess>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return &vm.domAttributeGetterSetterSpace();
    }

    static DOMAttributeGetterSetter* create(VM& vm, CustomGetter customGetter, CustomSetter customSetter, DOMAttributeAnnotation domAttribute)
    {
        DOMAttributeGetterSetter* customGetterSetter = new (NotNull, allocateCell<DOMAttributeGetterSetter>(vm)) DOMAttributeGetterSetter(vm, customGetter, customSetter, domAttribute);
        customGetterSetter->finishCreation(vm);
        return customGetterSetter;
    }

    DOMAttributeAnnotation domAttribute() const { return m_domAttribute; }

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(CustomGetterSetterType, StructureFlags), info());
    }

    DECLARE_EXPORT_INFO;

private:
    DOMAttributeGetterSetter(VM& vm, CustomGetter getter, CustomSetter setter, DOMAttributeAnnotation domAttribute)
        : Base(vm, vm.domAttributeGetterSetterStructure.get(), getter, setter)
        , m_domAttribute(domAttribute)
    {
    }

    DOMAttributeAnnotation m_domAttribute;
};

} // namespace JSC
