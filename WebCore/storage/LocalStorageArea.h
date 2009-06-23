/*
 * Copyright (C) 2008, 2009 Apple Inc. All Rights Reserved.
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

#ifndef LocalStorageArea_h
#define LocalStorageArea_h

#if ENABLE(DOM_STORAGE)

#include "SQLiteDatabase.h"
#include "StorageArea.h"
#include "StringHash.h"
#include "StorageSyncManager.h"
#include "Timer.h"
#include <wtf/HashMap.h>

namespace WebCore {
    
    class StorageSyncManager;
    
    class LocalStorageArea : public StorageArea {
    public:
        virtual ~LocalStorageArea();

        static PassRefPtr<LocalStorageArea> create(SecurityOrigin* origin, PassRefPtr<StorageSyncManager> syncManager) { return adoptRef(new LocalStorageArea(origin, syncManager)); }

        void scheduleFinalSync();

    private:
        LocalStorageArea(SecurityOrigin*, PassRefPtr<StorageSyncManager> syncManager);

        virtual void itemChanged(const String& key, const String& oldValue, const String& newValue, Frame* sourceFrame);
        virtual void itemRemoved(const String& key, const String& oldValue, Frame* sourceFrame);
        virtual void areaCleared(Frame* sourceFrame);

        void scheduleItemForSync(const String& key, const String& value);
        void scheduleClear();
        void dispatchStorageEvent(const String& key, const String& oldValue, const String& newValue, Frame* sourceFrame);

        Timer<LocalStorageArea> m_syncTimer;        
        HashMap<String, String> m_changedItems;
        bool m_itemsCleared;
        
        bool m_finalSyncScheduled;

        RefPtr<StorageSyncManager> m_syncManager;

        // The database handle will only ever be opened and used on the background thread.
        SQLiteDatabase m_database;

    // The following members are subject to thread synchronization issues.
    public:
        // Called from the background thread
        virtual void performImport();
        virtual void performSync();

    private:
        void syncTimerFired(Timer<LocalStorageArea>*);
        void sync(bool clearItems, const HashMap<String, String>& items);

        Mutex m_syncLock;
        HashMap<String, String> m_itemsPendingSync;
        bool m_clearItemsWhileSyncing;
        bool m_syncScheduled;

        mutable Mutex m_importLock;
        mutable ThreadCondition m_importCondition;
        mutable bool m_importComplete;
        void markImported();
        void blockUntilImportComplete() const;
    };

} // namespace WebCore

#endif // ENABLE(DOM_STORAGE)

#endif // LocalStorageArea_h
