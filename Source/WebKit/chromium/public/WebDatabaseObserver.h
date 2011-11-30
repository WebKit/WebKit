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

#ifndef WebDatabaseObserver_h
#define WebDatabaseObserver_h

namespace WebKit {
class WebDatabase;

class WebDatabaseObserver {
public:
    virtual void databaseOpened(const WebDatabase&) = 0;
    virtual void databaseModified(const WebDatabase&) = 0;
    virtual void databaseClosed(const WebDatabase&) = 0;

    virtual void reportOpenDatabaseResult(const WebDatabase&, int errorSite, int webSqlErrorCode, int sqliteErrorCode) { }
    virtual void reportChangeVersionResult(const WebDatabase&, int errorSite, int webSqlErrorCode, int sqliteErrorCode) { }
    virtual void reportStartTransactionResult(const WebDatabase&, int errorSite, int webSqlErrorCode, int sqliteErrorCode) { }
    virtual void reportCommitTransactionResult(const WebDatabase&, int errorSite, int webSqlErrorCode, int sqliteErrorCode) { }
    virtual void reportExecuteStatementResult(const WebDatabase&, int errorSite, int webSqlErrorCode, int sqliteErrorCode) { }
    virtual void reportVacuumDatabaseResult(const WebDatabase&, int sqliteErrorCode) { }

protected:
    ~WebDatabaseObserver() {}
};

} // namespace WebKit

#endif
