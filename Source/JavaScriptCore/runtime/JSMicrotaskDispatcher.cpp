/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "JSMicrotaskDispatcher.h"

#include "JSCJSValueInlines.h"
#include "SlotVisitorInlines.h"

namespace JSC {

const ClassInfo JSMicrotaskDispatcher::s_info = { "JSMicrotaskDispatcher"_s, nullptr, nullptr, nullptr, CREATE_METHOD_TABLE(JSMicrotaskDispatcher) };

Structure* JSMicrotaskDispatcher::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(JSMicrotaskDispatcherType, StructureFlags), info());
}

JSMicrotaskDispatcher* JSMicrotaskDispatcher::create(VM& vm, Structure* structure, Ref<MicrotaskDispatcher>&& dispatcher, JSGlobalObject* globalObject)
{
    auto* cell = new (NotNull, allocateCell<JSMicrotaskDispatcher>(vm)) JSMicrotaskDispatcher(vm, structure, WTF::move(dispatcher), globalObject);
    cell->finishCreation(vm);
    return cell;
}

JSMicrotaskDispatcher* JSMicrotaskDispatcher::create(VM& vm, Ref<MicrotaskDispatcher>&& dispatcher, JSGlobalObject* globalObject)
{
    return create(vm, vm.jsMicrotaskDispatcherStructure.get(), WTF::move(dispatcher), globalObject);
}

JSMicrotaskDispatcher::JSMicrotaskDispatcher(VM& vm, Structure* structure, Ref<MicrotaskDispatcher>&& dispatcher, JSGlobalObject* globalObject)
    : Base(vm, structure)
    , m_dispatcher(WTF::move(dispatcher))
    , m_globalObject(vm, this, globalObject, WriteBarrier<JSGlobalObject>::MayBeNull)
    , m_type(m_dispatcher->type())
{
}

template<typename Visitor>
void JSMicrotaskDispatcher::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    auto* thisObject = jsCast<JSMicrotaskDispatcher*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    visitor.append(thisObject->m_globalObject);
}
DEFINE_VISIT_CHILDREN(JSMicrotaskDispatcher);

void JSMicrotaskDispatcher::destroy(JSCell* cell)
{
    SUPPRESS_MEMORY_UNSAFE_CAST auto* thisObject = static_cast<JSMicrotaskDispatcher*>(cell);
    thisObject->JSMicrotaskDispatcher::~JSMicrotaskDispatcher();
}

} // namespace JSC
