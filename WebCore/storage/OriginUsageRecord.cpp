/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "config.h"
#include "OriginUsageRecord.h"

#include "FileSystem.h"
#include <limits>

namespace WebCore {

unsigned long long OriginUsageRecord::unknownDiskUsage()
{
    return std::numeric_limits<unsigned long long>::max();
}

OriginUsageRecord::OriginUsageRecord()
    : m_diskUsage(unknownDiskUsage())
{
}

void OriginUsageRecord::addDatabase(const String& identifier, const String& fullPath)
{
    ASSERT(!m_databaseMap.contains(identifier));
    
    m_databaseMap.set(identifier, DatabaseEntry(fullPath, unknownDiskUsage()));
    m_unknownSet.add(identifier);
     
    m_diskUsage = unknownDiskUsage();
}

void OriginUsageRecord::removeDatabase(const String& identifier)
{
    ASSERT(m_databaseMap.contains(identifier));

    m_diskUsage = unknownDiskUsage();
    m_databaseMap.remove(identifier);
    m_unknownSet.remove(identifier);
}

void OriginUsageRecord::markDatabase(const String& identifier)
{
    m_unknownSet.add(identifier);
    m_diskUsage = unknownDiskUsage();
}

unsigned long long OriginUsageRecord::diskUsage()
{
    // Use the last cached usage value if we have it
    if (m_diskUsage != unknownDiskUsage())
        return m_diskUsage;
    
    // stat() for the sizes known to be dirty
    HashSet<String>::iterator iUnknown = m_unknownSet.begin();
    HashSet<String>::iterator endUnknown = m_unknownSet.end();
    for (; iUnknown != endUnknown; ++iUnknown) {
        String path = m_databaseMap.get(*iUnknown).filename;
        ASSERT(!path.isEmpty());
                
        long long size;
        if (getFileSize(path, size))
            m_databaseMap.set(*iUnknown, DatabaseEntry(path, size));
        else {
            // When we can't determine the file size, we'll just have to assume the file is missing/inaccessible
            m_databaseMap.set(*iUnknown, DatabaseEntry(path, 0));
        }
    }
    m_unknownSet.clear();
    
    // Recalculate the cached usage value
    HashMap<String, DatabaseEntry>::iterator iDatabase = m_databaseMap.begin();
    HashMap<String, DatabaseEntry>::iterator endDatabase = m_databaseMap.end();
    
    m_diskUsage = 0;
    for (; iDatabase != endDatabase; ++iDatabase) {
        m_diskUsage += iDatabase->second.size;
    }

    return m_diskUsage;
}
    
}
