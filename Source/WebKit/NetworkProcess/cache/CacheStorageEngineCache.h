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

#pragma once

#include <WebCore/DOMCacheEngine.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

namespace CacheStorage {

class Cache {
public:
    Cache(uint64_t identifier, String&& name);

    uint64_t identifier() const { return m_identifier; }
    const String& name() const { return m_name; }

    Vector<WebCore::DOMCacheEngine::Record> retrieveRecords(const WebCore::URL&) const;
    WebCore::DOMCacheEngine::CacheInfo info() const { return { m_identifier, m_name }; }

    void put(Vector<WebCore::DOMCacheEngine::Record>&&, WebCore::DOMCacheEngine::RecordIdentifiersCallback&&);
    void remove(WebCore::ResourceRequest&&, WebCore::CacheQueryOptions&&, WebCore::DOMCacheEngine::RecordIdentifiersCallback&&);

private:
    Vector<WebCore::DOMCacheEngine::Record>* recordsFromURL(const WebCore::URL&);
    const Vector<WebCore::DOMCacheEngine::Record>* recordsFromURL(const WebCore::URL&) const;
    WebCore::DOMCacheEngine::Record& addNewURLRecord(WebCore::DOMCacheEngine::Record&&);

    void writeRecordsList(WebCore::DOMCacheEngine::CompletionCallback&&);
    void writeRecordToDisk(WebCore::DOMCacheEngine::Record&);
    void removeRecordFromDisk(WebCore::DOMCacheEngine::Record&);

    uint64_t m_identifier { 0 };
    String m_name;
    HashMap<String, Vector<WebCore::DOMCacheEngine::Record>> m_records;
    uint64_t m_nextRecordIdentifier { 0 };
};

} // namespace CacheStorage

} // namespace WebKit
