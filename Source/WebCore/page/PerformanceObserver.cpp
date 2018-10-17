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
#include "PerformanceObserver.h"

#include "DOMWindow.h"
#include "Document.h"
#include "Performance.h"
#include "PerformanceObserverEntryList.h"
#include "WorkerGlobalScope.h"

namespace WebCore {

PerformanceObserver::PerformanceObserver(ScriptExecutionContext& scriptExecutionContext, Ref<PerformanceObserverCallback>&& callback)
    : m_callback(WTFMove(callback))
{
    if (is<Document>(scriptExecutionContext)) {
        auto& document = downcast<Document>(scriptExecutionContext);
        if (DOMWindow* window = document.domWindow())
            m_performance = &window->performance();
    } else if (is<WorkerGlobalScope>(scriptExecutionContext)) {
        auto& workerGlobalScope = downcast<WorkerGlobalScope>(scriptExecutionContext);
        m_performance = &workerGlobalScope.performance();
    } else
        ASSERT_NOT_REACHED();
}

void PerformanceObserver::disassociate()
{
    m_performance = nullptr;
    m_registered = false;
}

ExceptionOr<void> PerformanceObserver::observe(Init&& init)
{
    if (!m_performance)
        return Exception { TypeError };

    if (init.entryTypes.isEmpty())
        return Exception { TypeError, "entryTypes cannot be an empty list"_s };

    OptionSet<PerformanceEntry::Type> filter;
    for (const String& entryType : init.entryTypes) {
        if (auto type = PerformanceEntry::parseEntryTypeString(entryType))
            filter.add(*type);
    }

    if (filter.isEmpty())
        return Exception { TypeError, "entryTypes contained only unsupported types"_s };

    m_typeFilter = filter;

    if (!m_registered) {
        m_performance->registerPerformanceObserver(*this);
        m_registered = true;
    }

    return { };
}

void PerformanceObserver::disconnect()
{
    if (m_performance)
        m_performance->unregisterPerformanceObserver(*this);

    m_registered = false;
    m_entriesToDeliver.clear();
}

void PerformanceObserver::queueEntry(PerformanceEntry& entry)
{
    m_entriesToDeliver.append(&entry);
}

void PerformanceObserver::deliver()
{
    if (m_entriesToDeliver.isEmpty())
        return;

    Vector<RefPtr<PerformanceEntry>> entries = WTFMove(m_entriesToDeliver);
    auto list = PerformanceObserverEntryList::create(WTFMove(entries));
    m_callback->handleEvent(list, *this);
}

} // namespace WebCore
