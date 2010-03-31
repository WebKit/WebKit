/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include <wtf/StdLibExtras.h>
#include <wtf/text/CString.h>

namespace WebCore {

QuotaTracker& QuotaTracker::instance()
{
    DEFINE_STATIC_LOCAL(QuotaTracker, tracker, ());
    return tracker;
}

void QuotaTracker::getDatabaseSizeAndSpaceAvailableToOrigin(
    const String& originIdentifier, const String& databaseName,
    unsigned long long* databaseSize, unsigned long long* spaceAvailable)
{
    MutexLocker lockData(m_dataGuard);
    ASSERT(m_databaseSizes.contains(originIdentifier));
    HashMap<String, SizeMap>::const_iterator it = m_databaseSizes.find(originIdentifier);
    ASSERT(it->second.contains(databaseName));
    *databaseSize = it->second.get(databaseName);

    ASSERT(m_spaceAvailableToOrigins.contains(originIdentifier));
    *spaceAvailable = m_spaceAvailableToOrigins.get(originIdentifier);
}

void QuotaTracker::updateDatabaseSizeAndSpaceAvailableToOrigin(
    const String& originIdentifier, const String& databaseName,
    unsigned long long databaseSize, unsigned long long spaceAvailable)
{
    MutexLocker lockData(m_dataGuard);
    m_spaceAvailableToOrigins.set(originIdentifier, spaceAvailable);
    HashMap<String, SizeMap>::iterator it = m_databaseSizes.add(originIdentifier, SizeMap()).first;
    it->second.set(databaseName, databaseSize);
}

}
