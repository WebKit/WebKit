/*
 * Copyright (C) 2009 Torch Mobile, Inc. All rights reserved.
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
 *  This library is distributed in the hope that i will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */


#ifndef LocalStorageThreadWince_h
#define LocalStorageThreadWince_h

#include <wtf/Deque.h>
#include <wtf/PassRefPtr.h>

namespace WebCore {

    class StorageAreaSync;
    class LocalStorageTask;

    class LocalStorageThread : public RefCounted<LocalStorageThread> {
    public:
        static PassRefPtr<LocalStorageThread> create() { return adoptRef(new LocalStorageThread); }

        ~LocalStorageThread();
        bool start();
        void scheduleImport(PassRefPtr<StorageAreaSync>);
        void scheduleSync(PassRefPtr<StorageAreaSync>);
        void terminate();
        void performTerminate();

    private:
        LocalStorageThread();

        void timerFired(Timer<LocalStorageThread>*);

        Deque<RefPtr<LocalStorageTask> > m_queue;
        Timer<LocalStorageThread> m_timer;
    };

} // namespace WebCore

#endif // LocalStorageThreadWince_h
