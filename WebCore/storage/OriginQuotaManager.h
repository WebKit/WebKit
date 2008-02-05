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
#ifndef OriginQuotaManager_h
#define OriginQuotaManager_h

#include "PlatformString.h"
#include "StringHash.h"
#include "SecurityOriginHash.h"
#include "Threading.h"
#include <wtf/HashMap.h>

namespace WebCore {

class Database;
class OriginUsageRecord;
class SecurityOrigin;
class String;

class OriginQuotaManager : public Noncopyable {
public:
    OriginQuotaManager();

    void lock();
    void unlock();

    // To setup the origin usage records on the main (DatabaseTracker) thread
    void trackOrigin(PassRefPtr<SecurityOrigin>);
    bool tracksOrigin(SecurityOrigin*) const;
    void addDatabase(SecurityOrigin*, const String& databaseIdentifier, const String& fullPath);
    void removeDatabase(SecurityOrigin*, const String& databaseIdentifier);
    void removeOrigin(SecurityOrigin*);

    // To mark dirtiness of a specific database on the background thread
    void markDatabase(Database*);
    unsigned long long diskUsage(SecurityOrigin*) const;
private:
    mutable Mutex m_usageRecordGuard;
    typedef HashMap<RefPtr<SecurityOrigin>, OriginUsageRecord*, SecurityOriginHash, SecurityOriginTraits> OriginUsageMap;
    OriginUsageMap m_usageMap;
};

} // namespace WebCore

#endif // OriginQuotaManager_h
