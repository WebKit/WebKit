/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#pragma once

#include "NetworkResourceLoader.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>

#if ENABLE(TAKE_UNBOUNDED_NETWORKING_ASSERTION)
#include "ProcessAssertion.h"
#endif

namespace WebKit {

class NetworkConnectionToWebProcess;

typedef uint64_t ResourceLoadIdentifier;

class NetworkResourceLoadMap {
public:
    typedef HashMap<ResourceLoadIdentifier, Ref<NetworkResourceLoader>> MapType;

    bool isEmpty() const { return m_loaders.isEmpty(); }
    bool contains(ResourceLoadIdentifier identifier) const { return m_loaders.contains(identifier); }
    MapType::iterator begin() { return m_loaders.begin(); }
    MapType::ValuesIteratorRange values() { return m_loaders.values(); }

    MapType::AddResult add(ResourceLoadIdentifier, Ref<NetworkResourceLoader>&&);
    NetworkResourceLoader* get(ResourceLoadIdentifier) const;
    bool remove(ResourceLoadIdentifier);
    RefPtr<NetworkResourceLoader> take(ResourceLoadIdentifier);

private:
    MapType m_loaders;
};

} // namespace WebKit
