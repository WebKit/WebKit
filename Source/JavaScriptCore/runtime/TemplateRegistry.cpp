/*
 * Copyright (C) 2015 Yusuke Suzuki <utatane.tea@gmail.com>.
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
#include "TemplateRegistry.h"

#include "JSCInlines.h"
#include "JSGlobalObject.h"
#include "ObjectConstructor.h"
#include "WeakGCMapInlines.h"

namespace JSC {

TemplateRegistry::TemplateRegistry(VM& vm)
    : m_templateMap(vm)
{
}

JSArray* TemplateRegistry::getTemplateObject(ExecState* exec, const TemplateRegistryKey& templateKey)
{
    JSArray* cached = m_templateMap.get(templateKey);
    if (cached)
        return cached;

    VM& vm = exec->vm();
    unsigned count = templateKey.cookedStrings().size();
    JSArray* templateObject = constructEmptyArray(exec, nullptr, count);
    if (UNLIKELY(vm.exception()))
        return nullptr;
    JSArray* rawObject = constructEmptyArray(exec, nullptr, count);
    if (UNLIKELY(vm.exception()))
        return nullptr;

    for (unsigned index = 0; index < count; ++index) {
        templateObject->putDirectIndex(exec, index, jsString(exec, templateKey.cookedStrings()[index]), ReadOnly | DontDelete, PutDirectIndexLikePutDirect);
        rawObject->putDirectIndex(exec, index, jsString(exec, templateKey.rawStrings()[index]), ReadOnly | DontDelete, PutDirectIndexLikePutDirect);
    }

    objectConstructorFreeze(exec, rawObject);
    ASSERT(!exec->hadException());

    templateObject->putDirect(vm, exec->propertyNames().raw, rawObject, ReadOnly | DontEnum | DontDelete);

    objectConstructorFreeze(exec, templateObject);
    ASSERT(!exec->hadException());

    m_templateMap.set(templateKey, templateObject);

    return templateObject;
}


} // namespace JSC
