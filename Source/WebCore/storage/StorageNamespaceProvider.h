/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef StorageNamespaceProvider_h
#define StorageNamespaceProvider_h

#include <WebCore/SecurityOriginHash.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class Page;
class SecurityOrigin;
class StorageNamespace;

class StorageNamespaceProvider : public RefCounted<StorageNamespaceProvider> {
public:
    StorageNamespaceProvider();
    virtual ~StorageNamespaceProvider();

    StorageNamespace& localStorageNamespace();
    StorageNamespace& transientLocalStorageNamespace(SecurityOrigin&);
    virtual RefPtr<StorageNamespace> createSessionStorageNamespace(Page&) = 0;

    void addPage(Page&);
    void removePage(Page&);

private:
    virtual RefPtr<StorageNamespace> createLocalStorageNamespace() = 0;
    virtual RefPtr<StorageNamespace> createTransientLocalStorageNamespace(SecurityOrigin&) = 0;

    HashSet<Page*> m_pages;

    RefPtr<StorageNamespace> m_localStorageNamespace;
    HashMap<RefPtr<SecurityOrigin>, RefPtr<StorageNamespace>> m_transientLocalStorageMap;
};

}

#endif // StorageNamespaceProvider_h
