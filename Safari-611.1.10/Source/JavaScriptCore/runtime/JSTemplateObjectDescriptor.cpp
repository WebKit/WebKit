/*
 * Copyright (C) 2015 Yusuke Suzuki <utatane.tea@gmail.com>.
 * Copyright (C) 2016-2019 Apple Inc. All Rights Reserved.
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

#include "config.h"
#include "JSTemplateObjectDescriptor.h"

#include "JSCInlines.h"
#include "ObjectConstructor.h"
#include "VM.h"

namespace JSC {

const ClassInfo JSTemplateObjectDescriptor::s_info = { "TemplateObjectDescriptor", nullptr, nullptr, nullptr, CREATE_METHOD_TABLE(JSTemplateObjectDescriptor) };


JSTemplateObjectDescriptor::JSTemplateObjectDescriptor(VM& vm, Ref<TemplateObjectDescriptor>&& descriptor, int endOffset)
    : Base(vm, vm.templateObjectDescriptorStructure.get())
    , m_descriptor(WTFMove(descriptor))
    , m_endOffset(endOffset)
{
}

JSTemplateObjectDescriptor* JSTemplateObjectDescriptor::create(VM& vm, Ref<TemplateObjectDescriptor>&& descriptor, int endOffset)
{
    JSTemplateObjectDescriptor* result = new (NotNull, allocateCell<JSTemplateObjectDescriptor>(vm.heap)) JSTemplateObjectDescriptor(vm, WTFMove(descriptor), endOffset);
    result->finishCreation(vm);
    return result;
}

void JSTemplateObjectDescriptor::destroy(JSCell* cell)
{
    static_cast<JSTemplateObjectDescriptor*>(cell)->JSTemplateObjectDescriptor::~JSTemplateObjectDescriptor();
}

JSArray* JSTemplateObjectDescriptor::createTemplateObject(JSGlobalObject* globalObject)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    unsigned count = descriptor().cookedStrings().size();
    JSArray* templateObject = constructEmptyArray(globalObject, nullptr, count);
    RETURN_IF_EXCEPTION(scope, nullptr);
    JSArray* rawObject = constructEmptyArray(globalObject, nullptr, count);
    RETURN_IF_EXCEPTION(scope, nullptr);

    for (unsigned index = 0; index < count; ++index) {
        auto cooked = descriptor().cookedStrings()[index];
        if (cooked)
            templateObject->putDirectIndex(globalObject, index, jsString(vm, cooked.value()), PropertyAttribute::ReadOnly | PropertyAttribute::DontDelete, PutDirectIndexLikePutDirect);
        else
            templateObject->putDirectIndex(globalObject, index, jsUndefined(), PropertyAttribute::ReadOnly | PropertyAttribute::DontDelete, PutDirectIndexLikePutDirect);
        RETURN_IF_EXCEPTION(scope, nullptr);

        rawObject->putDirectIndex(globalObject, index, jsString(vm, descriptor().rawStrings()[index]), PropertyAttribute::ReadOnly | PropertyAttribute::DontDelete, PutDirectIndexLikePutDirect);
        RETURN_IF_EXCEPTION(scope, nullptr);
    }

    objectConstructorFreeze(globalObject, rawObject);
    scope.assertNoException();

    templateObject->putDirect(vm, vm.propertyNames->raw, rawObject, PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum | PropertyAttribute::DontDelete);

    objectConstructorFreeze(globalObject, templateObject);
    scope.assertNoException();

    return templateObject;
}

} // namespace JSC
