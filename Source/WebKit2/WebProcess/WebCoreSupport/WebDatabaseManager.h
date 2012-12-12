/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebDatabaseManager_h
#define WebDatabaseManager_h

#if ENABLE(SQL_DATABASE)

#include "Arguments.h"
#include <WebCore/DatabaseManagerClient.h>
#include <wtf/Noncopyable.h>
#include <wtf/text/WTFString.h>

namespace CoreIPC {
class MessageDecoder;
class Connection;
class MessageID;
}

namespace WebKit {

class WebDatabaseManager : public WebCore::DatabaseManagerClient {
    WTF_MAKE_NONCOPYABLE(WebDatabaseManager);
public:
    static WebDatabaseManager& shared();
    static void initialize(const String& databaseDirectory);

    void didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::MessageDecoder&);
    void setQuotaForOrigin(const String& originIdentifier, unsigned long long quota) const;

public:
    void deleteAllDatabases() const;

private:
    WebDatabaseManager();
    virtual ~WebDatabaseManager();

    // Implemented in generated WebDatabaseManagerMessageReceiver.cpp
    void didReceiveWebDatabaseManagerMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::MessageDecoder&);

    void getDatabasesByOrigin(uint64_t callbackID) const;
    void getDatabaseOrigins(uint64_t callbackID) const;
    void deleteDatabaseWithNameForOrigin(const String& databaseIdentifier, const String& originIdentifier) const;
    void deleteDatabasesForOrigin(const String& originIdentifier) const;

    // WebCore::DatabaseManagerClient
    virtual void dispatchDidModifyOrigin(WebCore::SecurityOrigin*) OVERRIDE;
    virtual void dispatchDidModifyDatabase(WebCore::SecurityOrigin*, const String& databaseIdentifier) OVERRIDE;
};

} // namespace WebKit

#endif // ENABLE(SQL_DATABASE)

#endif // WebDatabaseManager_h
