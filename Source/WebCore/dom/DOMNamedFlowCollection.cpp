/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
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

#include "NamedFlowCollection.h"
#include "WebKitNamedFlow.h"

namespace WebCore {

DOMNamedFlowCollection::DOMNamedFlowCollection(NamedFlowCollection::NamedFlowSet& set)
{
    m_namedFlows.swap(set);
}

unsigned long DOMNamedFlowCollection::length() const
{
    return m_namedFlows.size();
}

PassRefPtr<WebKitNamedFlow> DOMNamedFlowCollection::item(unsigned long index) const
{
    if (index >= static_cast<unsigned long>(m_namedFlows.size()))
        return 0;
    NamedFlowCollection::NamedFlowSet::const_iterator it = m_namedFlows.begin();
    for (unsigned long i = 0; i < index; ++i)
        ++it;
    return *it;
}

PassRefPtr<WebKitNamedFlow> DOMNamedFlowCollection::namedItem(const AtomicString& name) const
{
    NamedFlowCollection::NamedFlowSet::const_iterator it = m_namedFlows.find<String, NamedFlowCollection::NamedFlowHashTranslator>(name);
    if (it != m_namedFlows.end())
        return *it;
    return 0;
}

bool DOMNamedFlowCollection::hasNamedItem(const AtomicString& name) const
{
    return namedItem(name);
}
} // namespace WebCore



