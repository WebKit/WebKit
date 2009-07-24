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

#include "config.h"
#include "LocalStorageThread.h"

#include "LocalStorageTask.h"
#include "StorageAreaSync.h"

namespace WebCore {

LocalStorageThread::LocalStorageThread()
: m_timer(this, &LocalStorageThread::timerFired)
{
}

LocalStorageThread::~LocalStorageThread()
{
}

bool LocalStorageThread::start()
{
    return true;
}

void LocalStorageThread::timerFired(Timer<LocalStorageThread>*)
{
    if (!m_queue.isEmpty()) {
        RefPtr<LocalStorageTask> task = m_queue.first();
        task->performTask();
        m_queue.removeFirst();
        if (!m_queue.isEmpty())
            m_timer.startOneShot(0);
    }
}

void LocalStorageThread::scheduleImport(PassRefPtr<StorageAreaSync> area)
{
    m_queue.append(LocalStorageTask::createImport(area));
    if (!m_timer.isActive())
        m_timer.startOneShot(0);
}

void LocalStorageThread::scheduleSync(PassRefPtr<StorageAreaSync> area)
{
    m_queue.append(LocalStorageTask::createSync(area));
    if (!m_timer.isActive())
        m_timer.startOneShot(0);
}

void LocalStorageThread::terminate()
{
    m_queue.clear();
    m_timer.stop();
}

void LocalStorageThread::performTerminate()
{
    m_queue.clear();
    m_timer.stop();
}

}
