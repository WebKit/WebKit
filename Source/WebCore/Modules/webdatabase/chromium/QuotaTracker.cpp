/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "QuotaTracker.h"
#include "PlatformSupport.h"

#if ENABLE(SQL_DATABASE)

#include <wtf/StdLibExtras.h>

namespace WebCore {

QuotaTracker& QuotaTracker::instance()
{
    AtomicallyInitializedStatic(QuotaTracker&, tracker = *new QuotaTracker);
    return tracker;
}

void QuotaTracker::getDatabaseSizeAndSpaceAvailableToOrigin(
    const String& originIdentifier, const String& databaseName,
    unsigned long long* databaseSize, unsigned long long* spaceAvailable)
{
    // Extra scope to unlock prior to potentially calling PlatformSupport.
    {
        MutexLocker lockData(m_dataGuard);
        ASSERT(m_databaseSizes.contains(originIdentifier));
        HashMap<String, SizeMap>::const_iterator it = m_databaseSizes.find(originIdentifier);
        ASSERT(it->second.contains(databaseName));
        *databaseSize = it->second.get(databaseName);

        if (m_spaceAvailableToOrigins.contains(originIdentifier)) {
            *spaceAvailable = m_spaceAvailableToOrigins.get(originIdentifier);
            return;
        }
    }

    // The embedder hasn't pushed this value to us, so we pull it as needed.
    *spaceAvailable = PlatformSupport::databaseGetSpaceAvailableForOrigin(originIdentifier);
}

void QuotaTracker::updateDatabaseSize(
    const String& originIdentifier, const String& databaseName,
    unsigned long long databaseSize)
{
    MutexLocker lockData(m_dataGuard);
    HashMap<String, SizeMap>::iterator it = m_databaseSizes.add(originIdentifier, SizeMap()).iterator;
    it->second.set(databaseName, databaseSize);
}

void QuotaTracker::updateSpaceAvailableToOrigin(const String& originIdentifier, unsigned long long spaceAvailable)
{
    MutexLocker lockData(m_dataGuard);
    m_spaceAvailableToOrigins.set(originIdentifier, spaceAvailable);
}

void QuotaTracker::resetSpaceAvailableToOrigin(const String& originIdentifier)
{
    MutexLocker lockData(m_dataGuard);
    m_spaceAvailableToOrigins.remove(originIdentifier);
}

} // namespace WebCore

#endif // ENABLE(SQL_DATABASE)
