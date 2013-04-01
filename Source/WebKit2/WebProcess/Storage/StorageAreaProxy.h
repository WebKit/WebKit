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

#ifndef StorageAreaProxy_h
#define StorageAreaProxy_h

#include "MessageReceiver.h"
#include <WebCore/StorageArea.h>
#include <wtf/HashCountedSet.h>
#include <wtf/HashMap.h>

namespace WebCore {
class StorageMap;
}

namespace WebKit {

class StorageNamespaceProxy;

class StorageAreaProxy : public WebCore::StorageArea, private CoreIPC::MessageReceiver {
public:
    static PassRefPtr<StorageAreaProxy> create(StorageNamespaceProxy*, PassRefPtr<WebCore::SecurityOrigin>);
    virtual ~StorageAreaProxy();

private:
    StorageAreaProxy(StorageNamespaceProxy*, PassRefPtr<WebCore::SecurityOrigin>);

    // WebCore::StorageArea.
    virtual unsigned length(WebCore::ExceptionCode&, WebCore::Frame* sourceFrame) OVERRIDE;
    virtual String key(unsigned index, WebCore::ExceptionCode&, WebCore::Frame* sourceFrame) OVERRIDE;
    virtual String getItem(const String& key, WebCore::ExceptionCode&, WebCore::Frame* sourceFrame) OVERRIDE;
    virtual void setItem(const String& key, const String& value, WebCore::ExceptionCode&, WebCore::Frame* sourceFrame) OVERRIDE;
    virtual void removeItem(const String& key, WebCore::ExceptionCode&, WebCore::Frame* sourceFrame) OVERRIDE;
    virtual void clear(WebCore::ExceptionCode&, WebCore::Frame* sourceFrame) OVERRIDE;
    virtual bool contains(const String& key, WebCore::ExceptionCode&, WebCore::Frame* sourceFrame) OVERRIDE;
    virtual bool canAccessStorage(WebCore::Frame*) OVERRIDE;
    virtual size_t memoryBytesUsedByCache() OVERRIDE;
    virtual void incrementAccessCount() OVERRIDE;
    virtual void decrementAccessCount() OVERRIDE;
    virtual void closeDatabaseIfIdle() OVERRIDE;

    // CoreIPC::MessageReceiver
    virtual void didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageDecoder&) OVERRIDE;

    void didSetItem(const String& key, bool quotaError);
    void dispatchStorageEvent(const String& key, const String& oldValue, const String& newValue, const String& urlString);

    WebCore::StorageType storageType() const;
    bool disabledByPrivateBrowsingInFrame(const WebCore::Frame* sourceFrame) const;

    bool shouldApplyChangesForKey(const String& key) const;
    void loadValuesIfNeeded();
    void resetValues();

    void dispatchSessionStorageEvent(const String& key, const String& oldValue, const String& newValue, const String& urlString);
    void dispatchLocalStorageEvent(const String& key, const String& oldValue, const String& newValue, const String& urlString);

    uint64_t m_storageNamespaceID;
    unsigned m_quotaInBytes;
    uint64_t m_storageAreaID;

    RefPtr<WebCore::SecurityOrigin> m_securityOrigin;
    RefPtr<WebCore::StorageMap> m_storageMap;

    HashCountedSet<String> m_pendingValueChanges;
};

} // namespace WebKit

#endif // StorageAreaProxy_h
