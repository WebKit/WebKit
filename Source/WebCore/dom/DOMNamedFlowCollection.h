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

#pragma once

#include <wtf/Forward.h>
#include <wtf/HashSet.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

class WebKitNamedFlow;

class DOMNamedFlowCollection : public RefCounted<DOMNamedFlowCollection> {
public:
    static Ref<DOMNamedFlowCollection> create(Vector<Ref<WebKitNamedFlow>>&&);
    ~DOMNamedFlowCollection();

    unsigned length() const;
    WebKitNamedFlow* item(unsigned index) const;
    WebKitNamedFlow* namedItem(const AtomicString& name) const;
    const Vector<AtomicString>& supportedPropertyNames();

private:
    struct HashFunctions;

    explicit DOMNamedFlowCollection(Vector<Ref<WebKitNamedFlow>>&&);

    const Vector<Ref<WebKitNamedFlow>> m_flows;
    mutable HashSet<WebKitNamedFlow*, HashFunctions> m_flowsByName;
    mutable Vector<AtomicString> m_flowNames;
};

inline unsigned DOMNamedFlowCollection::length() const
{
    return m_flows.size();
}

} // namespace WebCore
