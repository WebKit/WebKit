/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef StorageAreaImpl_h
#define StorageAreaImpl_h

#if ENABLE(DOM_STORAGE)

#include "StorageArea.h"

namespace WebCore {

    class StorageAreaImpl : public StorageArea {
    public:
        static PassRefPtr<StorageArea> create(StorageType, SecurityOrigin*, PassRefPtr<StorageSyncManager>);
        virtual ~StorageAreaImpl();
        virtual PassRefPtr<StorageArea> copy(SecurityOrigin*);

        // The HTML5 DOM Storage API
        virtual unsigned length() const;
        virtual String key(unsigned index, ExceptionCode& ec) const;
        virtual String getItem(const String& key) const;
        virtual void setItem(const String& key, const String& value, ExceptionCode& ec, Frame* sourceFrame);
        virtual void removeItem(const String& key, Frame* sourceFrame);
        virtual void clear(Frame* sourceFrame);

        virtual bool contains(const String& key) const;
        virtual void close();

        // Could be called from a background thread.
        void importItem(const String& key, const String& value);
        SecurityOrigin* securityOrigin();

    private:
        StorageAreaImpl(StorageType, SecurityOrigin*, PassRefPtr<StorageSyncManager>);
        StorageAreaImpl(SecurityOrigin*, StorageAreaImpl*);

        void blockUntilImportComplete() const;

        void dispatchStorageEvent(const String& key, const String& oldValue, const String& newValue, Frame* sourceFrame);

        StorageType m_storageType;
        RefPtr<SecurityOrigin> m_securityOrigin;
        RefPtr<StorageMap> m_storageMap;

        RefPtr<StorageAreaSync> m_storageAreaSync;
        RefPtr<StorageSyncManager> m_storageSyncManager;

#ifndef NDEBUG
        bool m_isShutdown;
#endif
    };

} // namespace WebCore

#endif // ENABLE(DOM_STORAGE)

#endif // StorageAreaImpl_h
