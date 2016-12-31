/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2016 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "DOMNamedFlowCollection.h"

#include "WebKitNamedFlow.h"
#include <wtf/text/AtomicStringHash.h>

namespace WebCore {

inline DOMNamedFlowCollection::DOMNamedFlowCollection(Vector<Ref<WebKitNamedFlow>>&& flows)
    : m_flows(WTFMove(flows))
{
}

Ref<DOMNamedFlowCollection> DOMNamedFlowCollection::create(Vector<Ref<WebKitNamedFlow>>&& flows)
{
    return adoptRef(*new DOMNamedFlowCollection(WTFMove(flows)));
}

DOMNamedFlowCollection::~DOMNamedFlowCollection()
{
}

WebKitNamedFlow* DOMNamedFlowCollection::item(unsigned index) const
{
    if (index >= m_flows.size())
        return nullptr;
    return const_cast<WebKitNamedFlow*>(m_flows[index].ptr());
}

struct DOMNamedFlowCollection::HashFunctions {
    static unsigned hash(const WebKitNamedFlow* key) { return AtomicStringHash::hash(key->name()); }
    static bool equal(const WebKitNamedFlow* a, const WebKitNamedFlow* b) { return a->name() == b->name(); }
    static const bool safeToCompareToEmptyOrDeleted = false;

    static unsigned hash(const AtomicString& key) { return AtomicStringHash::hash(key); }
    static bool equal(const WebKitNamedFlow* a, const AtomicString& b) { return a->name() == b; }
};

WebKitNamedFlow* DOMNamedFlowCollection::namedItem(const AtomicString& name) const
{
    if (m_flowsByName.isEmpty()) {
        // No need to optimize the case where m_flows is empty; will do nothing very quickly.
        for (auto& flow : m_flows)
            m_flowsByName.add(const_cast<WebKitNamedFlow*>(flow.ptr()));
    }
    auto it = m_flowsByName.find<HashFunctions>(name);
    if (it == m_flowsByName.end())
        return nullptr;
    return *it;
}

const Vector<AtomicString>& DOMNamedFlowCollection::supportedPropertyNames()
{
    if (m_flowNames.isEmpty()) {
        // No need to optimize the case where m_flows is empty; will do nothing relatively quickly.
        m_flowNames.reserveInitialCapacity(m_flows.size());
        for (auto& flow : m_flows)
            m_flowNames.uncheckedAppend(flow->name());
    }
    return m_flowNames;
}

} // namespace WebCore
