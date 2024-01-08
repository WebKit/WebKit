/*
 * Copyright (C) 2023 Igalia S.L. All rights reserved.
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
#include "NavigationHistoryEntry.h"

#include "NavigationController.h"
#include "ScriptExecutionContext.h"
#include <JavaScriptCore/JSCJSValueInlines.h>
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(NavigationHistoryEntry);

RefPtr<NavigationHistoryEntry> NavigationHistoryEntry::create(ScriptExecutionContext* context, const RefPtr<NavigationEntry> entry, bool isSameDocument, int64_t index)
{
    return adoptRef(*new NavigationHistoryEntry(context, entry, isSameDocument, index));
}

NavigationHistoryEntry::NavigationHistoryEntry(ScriptExecutionContext* context, RefPtr<NavigationEntry> entry, bool isSameDocument, int64_t index)
    : ContextDestructionObserver(context)
    , m_url(entry->url())
    , m_key(entry->key())
    , m_id(entry->id())
    , m_index(index)
    , m_sameDocument(isSameDocument)
{
}

const JSC::JSValue NavigationHistoryEntry::getState() const
{
    return JSC::jsUndefined();
}

ScriptExecutionContext* NavigationHistoryEntry::scriptExecutionContext() const
{
    return ContextDestructionObserver::scriptExecutionContext();
}

EventTargetInterface NavigationHistoryEntry::eventTargetInterface() const
{
    return NavigationHistoryEntryEventTargetInterfaceType;
}

} // namespace WebCore
