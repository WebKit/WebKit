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

#ifndef SessionStorage_h
#define SessionStorage_h

#include "SecurityOriginHash.h"
#include "SessionStorageArea.h"

#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/RefCounted.h>

namespace WebCore {

    class Page;

    class SessionStorage : public RefCounted<SessionStorage> {
    public:
        static PassRefPtr<SessionStorage> create(Page*);
        PassRefPtr<SessionStorage> copy(Page*);
        
        PassRefPtr<StorageArea> storageArea(SecurityOrigin*);

#ifndef NDEBUG
        Page* page() { return m_page; }
#endif

    private:
        SessionStorage(Page*);

        void dispatchStorageEvent(StorageArea*, const String& key, const String& oldValue, const String& newValue, Frame* sourceFrame);

        Page* m_page;
        
        typedef HashMap<RefPtr<SecurityOrigin>, RefPtr<SessionStorageArea>, SecurityOriginHash> SessionStorageAreaMap;
        SessionStorageAreaMap m_storageAreaMap;
    };

} // namespace WebCore

#endif // SessionStorage_h
