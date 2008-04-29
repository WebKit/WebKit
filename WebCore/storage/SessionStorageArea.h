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

#ifndef SessionStorageArea_h
#define SessionStorageArea_h

#include "StorageArea.h"

namespace WebCore {

    class Page;
    
    class SessionStorageArea : public StorageArea {
    public:
        static PassRefPtr<SessionStorageArea> create(SecurityOrigin* origin, Page* page) { return adoptRef(new SessionStorageArea(origin, page)); }
        PassRefPtr<SessionStorageArea> copy(SecurityOrigin*, Page*);
                
        Page* page() { return m_page; }

    private:
        SessionStorageArea(SecurityOrigin*, Page*);
        SessionStorageArea(SecurityOrigin*, Page*, SessionStorageArea*);

        virtual void itemChanged(const String& key, const String& oldValue, const String& newValue, Frame* sourceFrame);
        virtual void itemRemoved(const String& key, const String& oldValue, Frame* sourceFrame);
        virtual void areaCleared(Frame* sourceFrame);

        void dispatchStorageEvent(const String& key, const String& oldValue, const String& newValue, Frame* sourceFrame);
        
        Page* m_page;
    };

} // namespace WebCore

#endif // SessionStorageArea_h
