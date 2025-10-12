/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "PageHeapAgent.h"

#include "CustomElementRegistry.h"
#include "JSCustomElementInterface.h"
#include "JSElement.h"
#include "JSNode.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

using namespace Inspector;

WTF_MAKE_TZONE_ALLOCATED_IMPL(PageHeapAgent);

PageHeapAgent::PageHeapAgent(PageAgentContext& context)
    : WebHeapAgent(context)
    , m_instrumentingAgents(context.instrumentingAgents)
{
}

PageHeapAgent::~PageHeapAgent() = default;

Inspector::Protocol::ErrorStringOr<void> PageHeapAgent::enable()
{
    auto result = WebHeapAgent::enable();

    m_instrumentingAgents.setEnabledPageHeapAgent(this);

    return result;
}

Inspector::Protocol::ErrorStringOr<void> PageHeapAgent::disable()
{
    m_instrumentingAgents.setEnabledPageHeapAgent(nullptr);

    return WebHeapAgent::disable();
}

String PageHeapAgent::heapSnapshotBuilderOverrideClassName(JSC::HeapSnapshotBuilder& builder, JSC::JSCell* cell, const String& currentClassName)
{
    if (currentClassName == "HTMLElement"_s) {
        if (auto* jsElement = jsDynamicCast<JSElement*>(cell)) {
            Ref element = jsElement->wrapped();
            if (element->isDefinedCustomElement()) {
                if (RefPtr customElementRegistry = element->customElementRegistry()) {
                    if (RefPtr customElementInterface = customElementRegistry->findInterface(element)) {
                        if (JSC::JSObject* customElementConstructorObject = customElementInterface->constructor()) {
                            if (JSC::JSFunction* customElementConstructorFunction = jsDynamicCast<JSC::JSFunction*>(customElementConstructorObject)) {
                                String customElementClassName = customElementConstructorFunction->calculatedDisplayName(customElementConstructorFunction->vm());
                                if (!customElementClassName.isNull())
                                    return customElementClassName;
                            }
                        }
                    }
                }
            }
        }
    }

    return JSC::HeapSnapshotBuilder::Client::heapSnapshotBuilderOverrideClassName(builder, cell, currentClassName);
}

bool PageHeapAgent::heapSnapshotBuilderIsElement(JSC::HeapSnapshotBuilder&, JSC::JSCell* cell)
{
    return jsDynamicCast<JSNode*>(cell);
}

void PageHeapAgent::mainFrameNavigated()
{
    clearHeapSnapshots();
}

} // namespace WebCore
