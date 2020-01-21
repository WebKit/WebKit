/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#include "MessageReceiver.h"
#include "StorageAreaImplIdentifier.h"
#include <WebCore/StorageArea.h>
#include <wtf/HashMap.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
class SecurityOrigin;
}

namespace WebKit {

class StorageAreaMap;

class StorageAreaImpl final : public WebCore::StorageArea {
public:
    using Identifier = StorageAreaImplIdentifier;

    static Ref<StorageAreaImpl> create(StorageAreaMap&);
    virtual ~StorageAreaImpl();

    Identifier identifier() const { return m_identifier; }

private:
    StorageAreaImpl(StorageAreaMap&);

    // WebCore::StorageArea.
    unsigned length() override;
    String key(unsigned index) override;
    String item(const String& key) override;
    void setItem(WebCore::Frame* sourceFrame, const String& key, const String& value, bool& quotaException) override;
    void removeItem(WebCore::Frame* sourceFrame, const String& key) override;
    void clear(WebCore::Frame* sourceFrame) override;
    bool contains(const String& key) override;
    WebCore::StorageType storageType() const override;
    size_t memoryBytesUsedByCache() override;
    void incrementAccessCount() override;
    void decrementAccessCount() override;
    void closeDatabaseIfIdle() override;

    Identifier m_identifier;
    WeakPtr<StorageAreaMap> m_storageAreaMap;
};

} // namespace WebKit
