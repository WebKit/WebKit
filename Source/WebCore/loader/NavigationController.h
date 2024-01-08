/*
 * Copyright (C) 2023 Igalia S.L. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "FrameLoader.h"
#include "NavigationNavigationType.h"
#include <map>
#include <wtf/RefCounted.h>

namespace WebCore {

class LocalFrame;
class SerializedScriptValue;

class NavigationEntry : public RefCounted<NavigationEntry> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static RefPtr<NavigationEntry> create(const String& id, const String& key);

    const String& id() const { return m_id; };
    const String& url() const { return m_url; };
    const String& key() const { return m_key; };

private:
    NavigationEntry(const String& id, const String& key);

    String m_url;
    String m_id;
    String m_key;
};

class FrameLoader::NavigationController {
    WTF_MAKE_NONCOPYABLE(NavigationController);
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Loader);
public:

    explicit NavigationController(LocalFrame&);
    ~NavigationController();

    RefPtr<NavigationEntry> addEntry(const String& url);

    void dispatchNavigationEvent(const RefPtr<NavigationEntry>, const URL& oldURL, const Document&, NavigationNavigationType);

    std::tuple<const RefPtr<NavigationEntry>, int64_t> currentEntry() const;

private:
    LocalFrame& m_frame;

    std::map<String, RefPtr<NavigationEntry>> m_navigations;
    RefPtr<NavigationEntry> m_currentEntry;
};

} // namespace WebCore
