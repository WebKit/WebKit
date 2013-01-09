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

#ifndef WebDatabase_h
#define WebDatabase_h

#include "WebSecurityOrigin.h"
#include <public/WebCommon.h>

namespace WebCore { class AbstractDatabase; }

namespace WebKit {

class WebDatabaseObserver;
class WebString;

class WebDatabase {
public:
    WEBKIT_EXPORT WebString name() const;
    WEBKIT_EXPORT WebString displayName() const;
    WEBKIT_EXPORT unsigned long estimatedSize() const;
    WEBKIT_EXPORT WebSecurityOrigin securityOrigin() const;
    WEBKIT_EXPORT bool isSyncDatabase() const;

    WEBKIT_EXPORT static void setObserver(WebDatabaseObserver*);
    WEBKIT_EXPORT static WebDatabaseObserver* observer();

    WEBKIT_EXPORT static void updateDatabaseSize(
        const WebString& originIdentifier, const WebString& name, long long size);
    WEBKIT_EXPORT static void updateSpaceAvailable(
        const WebString& originIdentifier, long long spaceAvailable);
    WEBKIT_EXPORT static void resetSpaceAvailable(
        const WebString& originIdentifier);

    WEBKIT_EXPORT static void closeDatabaseImmediately(
        const WebString& originIdentifier, const WebString& databaseName);

#if WEBKIT_IMPLEMENTATION
    WebDatabase(const WebCore::AbstractDatabase*);
#endif

private:
    WebDatabase() { }
    const WebCore::AbstractDatabase* m_database;
};

} // namespace WebKit

#endif
