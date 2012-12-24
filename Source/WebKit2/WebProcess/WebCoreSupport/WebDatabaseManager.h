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

#include "MessageReceiver.h"
#include <WebCore/DatabaseManagerClient.h>
#include <stdint.h>
#include <wtf/Noncopyable.h>

namespace WebKit {

class WebProcess;

class WebDatabaseManager : public WebCore::DatabaseManagerClient, private CoreIPC::MessageReceiver {
    WTF_MAKE_NONCOPYABLE(WebDatabaseManager);
public:
    WebDatabaseManager(WebProcess*);
    static void initialize(const String& databaseDirectory);

    void setQuotaForOrigin(const String& originIdentifier, unsigned long long quota) const;
    void deleteAllDatabases() const;

private:
    // CoreIPC::MessageReceiver
    void didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::MessageDecoder&) OVERRIDE;
    // Implemented in generated WebDatabaseManagerMessageReceiver.cpp
    void didReceiveWebDatabaseManagerMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::MessageDecoder&);

    void getDatabasesByOrigin(uint64_t callbackID) const;
    void getDatabaseOrigins(uint64_t callbackID) const;
    void deleteDatabaseWithNameForOrigin(const String& databaseIdentifier, const String& originIdentifier) const;
    void deleteDatabasesForOrigin(const String& originIdentifier) const;

    // WebCore::DatabaseManagerClient
    virtual void dispatchDidModifyOrigin(WebCore::SecurityOrigin*) OVERRIDE;
    virtual void dispatchDidModifyDatabase(WebCore::SecurityOrigin*, const String& databaseIdentifier) OVERRIDE;

    WebProcess* m_process;
};

} // namespace WebKit

#endif // ENABLE(SQL_DATABASE)

#endif // WebDatabaseManager_h
