/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "DOMWindowCaches.h"

#include "CacheStorageProvider.h"
#include "DOMCacheStorage.h"
#include "DOMWindow.h"
#include "Document.h"
#include "Frame.h"
#include "Page.h"

namespace WebCore {

DOMWindowCaches::DOMWindowCaches(DOMWindow* window)
    : DOMWindowProperty(window)
{
}

const char* DOMWindowCaches::supplementName()
{
    return "DOMWindowCaches";
}

DOMWindowCaches* DOMWindowCaches::from(DOMWindow* window)
{
    auto* supplement = static_cast<DOMWindowCaches*>(Supplement<DOMWindow>::from(window, supplementName()));
    if (!supplement) {
        auto newSupplement = makeUnique<DOMWindowCaches>(window);
        supplement = newSupplement.get();
        provideTo(window, supplementName(), WTFMove(newSupplement));
    }
    return supplement;
}

ExceptionOr<DOMCacheStorage*> DOMWindowCaches::caches(ScriptExecutionContext& context, DOMWindow& window)
{
    if (downcast<Document>(context).isSandboxed(SandboxOrigin))
        return Exception { SecurityError, "Cache storage is disabled because the context is sandboxed and lacks the 'allow-same-origin' flag" };

    if (!window.isCurrentlyDisplayedInFrame())
        return nullptr;

    return DOMWindowCaches::from(&window)->caches();
}

DOMCacheStorage* DOMWindowCaches::caches() const
{
    ASSERT(frame());
    ASSERT(frame()->document());
    if (!m_caches && frame()->page())
        m_caches = DOMCacheStorage::create(*frame()->document(), frame()->page()->cacheStorageProvider().createCacheStorageConnection());
    return m_caches.get();
}

} // namespace WebCore
