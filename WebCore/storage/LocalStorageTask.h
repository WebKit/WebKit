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

#ifndef LocalStorageTask_h
#define LocalStorageTask_h

#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/Threading.h>

namespace WebCore {

    class LocalStorage;
    class LocalStorageArea;
    class LocalStorageThread;

    class LocalStorageTask : public ThreadSafeShared<LocalStorageTask> {
    public:
        enum Type { StorageImport, StorageSync, AreaImport, AreaSync, TerminateThread };

        static PassRefPtr<LocalStorageTask> createImport(PassRefPtr<LocalStorage> storage) { return adoptRef(new LocalStorageTask(StorageImport, storage)); }
        static PassRefPtr<LocalStorageTask> createImport(PassRefPtr<LocalStorageArea> area) { return adoptRef(new LocalStorageTask(AreaImport, area)); }
        static PassRefPtr<LocalStorageTask> createSync(PassRefPtr<LocalStorage> storage) { return adoptRef(new LocalStorageTask(StorageSync, storage)); }
        static PassRefPtr<LocalStorageTask> createSync(PassRefPtr<LocalStorageArea> area) { return adoptRef(new LocalStorageTask(AreaSync, area)); }
        static PassRefPtr<LocalStorageTask> createTerminate(PassRefPtr<LocalStorageThread> thread) { return adoptRef(new LocalStorageTask(TerminateThread, thread)); }

        void performTask();

    private:
        LocalStorageTask(Type, PassRefPtr<LocalStorageArea>);
        LocalStorageTask(Type, PassRefPtr<LocalStorage>);
        LocalStorageTask(Type, PassRefPtr<LocalStorageThread>);

        Type m_type;
        RefPtr<LocalStorageArea> m_area;
        RefPtr<LocalStorage> m_storage;
        RefPtr<LocalStorageThread> m_thread;
    };

} // namespace WebCore

#endif // LocalStorageTask_h
