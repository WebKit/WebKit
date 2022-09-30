/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "ScreenOrientation.h"

#include "Document.h"
#include "EventNames.h"
#include "JSDOMPromiseDeferred.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(ScreenOrientation);

Ref<ScreenOrientation> ScreenOrientation::create(Document* document)
{
    auto screenOrientation = adoptRef(*new ScreenOrientation(document));
    screenOrientation->suspendIfNeeded();
    return screenOrientation;
}

ScreenOrientation::ScreenOrientation(Document* document)
    : ActiveDOMObject(document)
{
}

Document* ScreenOrientation::document() const
{
    return downcast<Document>(scriptExecutionContext());
}

void ScreenOrientation::lock(LockType, Ref<DeferredPromise>&& promise)
{
    promise->reject(Exception { NotSupportedError });
}

void ScreenOrientation::unlock()
{
}

auto ScreenOrientation::type() const -> Type
{
    return Type::PortraitPrimary;
}

uint16_t ScreenOrientation::angle() const
{
    return 0;
}

const char* ScreenOrientation::activeDOMObjectName() const
{
    return "ScreenOrientation";
}

bool ScreenOrientation::virtualHasPendingActivity() const
{
    return m_hasChangeEventListener;
}

void ScreenOrientation::eventListenersDidChange()
{
    m_hasChangeEventListener = hasEventListeners(eventNames().changeEvent);
}

} // namespace WebCore
