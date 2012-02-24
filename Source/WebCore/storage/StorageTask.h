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

#ifndef StorageTask_h
#define StorageTask_h

#include "PlatformString.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/Threading.h>

namespace WebCore {

    class StorageAreaSync;
    class StorageThread;

    class StorageTask {
        WTF_MAKE_NONCOPYABLE(StorageTask); WTF_MAKE_FAST_ALLOCATED;
    public:
        enum Type { AreaImport, AreaSync, DeleteEmptyDatabase, SetOriginDetails, ImportOrigins, DeleteAllOrigins, DeleteOrigin, TerminateThread };

        ~StorageTask();

        static PassOwnPtr<StorageTask> createImport(StorageAreaSync* area) { return adoptPtr(new StorageTask(AreaImport, area)); }
        static PassOwnPtr<StorageTask> createSync(StorageAreaSync* area) { return adoptPtr(new StorageTask(AreaSync, area)); }
        static PassOwnPtr<StorageTask> createDeleteEmptyDatabase(StorageAreaSync* area) { return adoptPtr(new StorageTask(DeleteEmptyDatabase, area)); }
        static PassOwnPtr<StorageTask> createOriginIdentifiersImport() { return adoptPtr(new StorageTask(ImportOrigins)); }
        static PassOwnPtr<StorageTask> createSetOriginDetails(const String& originIdentifier, const String& databaseFilename) { return adoptPtr(new StorageTask(SetOriginDetails, originIdentifier, databaseFilename)); }
        static PassOwnPtr<StorageTask> createDeleteOrigin(const String& originIdentifier) { return adoptPtr(new StorageTask(DeleteOrigin, originIdentifier)); }
        static PassOwnPtr<StorageTask> createDeleteAllOrigins() { return adoptPtr(new StorageTask(DeleteAllOrigins)); }
        static PassOwnPtr<StorageTask> createTerminate(StorageThread* thread) { return adoptPtr(new StorageTask(TerminateThread, thread)); }

        void performTask();

    private:
        StorageTask(Type, StorageAreaSync*);
        StorageTask(Type, StorageThread*);
        StorageTask(Type, const String& originIdentifier);
        StorageTask(Type, const String& originIdentifier, const String& databaseFilename);
        StorageTask(Type);

        Type m_type;
        StorageAreaSync* m_area;
        StorageThread* m_thread;
        String m_originIdentifier;
        String m_databaseFilename;
    };

} // namespace WebCore

#endif // StorageTask_h
