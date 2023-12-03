/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "InvokeEvent.h"

#include "Element.h"

#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(InvokeEvent);

InvokeEvent::InvokeEvent(const AtomString& type, const InvokeEvent::Init& initializer, IsTrusted isTrusted)
    : Event(type, initializer, isTrusted)
    , m_invoker(initializer.invoker)
    , m_action(initializer.action)
{
}

Ref<InvokeEvent> InvokeEvent::create(const AtomString& eventType, const InvokeEvent::Init& init, IsTrusted isTrusted)
{
    return adoptRef(*new InvokeEvent(eventType, init, isTrusted));
}

Ref<InvokeEvent> InvokeEvent::createForBindings()
{
    return adoptRef(*new InvokeEvent);
}

EventInterface InvokeEvent::eventInterface() const
{
    return InvokeEventInterfaceType;
}

bool InvokeEvent::isInvokeEvent() const
{
    return true;
}

RefPtr<Element> InvokeEvent::invoker() const
{
    auto* invoker = m_invoker.get();
    if (RefPtr target = dynamicDowncast<Node>(currentTarget())) {
        auto& treeScope = target->treeScope();
        auto node = treeScope.retargetToScope(*invoker);
        return &downcast<Element>(node).get();
    }
    return invoker;
}

} // namespace WebCore
