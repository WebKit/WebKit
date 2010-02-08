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

#include "WebCommon.h"
#include "WebSecurityOrigin.h"

#if WEBKIT_IMPLEMENTATION
namespace WebCore { class Database; }
namespace WTF { template <typename T> class PassRefPtr; }
#endif

namespace WebKit {

class WebDatabaseObserver;
class WebDatabasePrivate;
class WebString;

class WebDatabase {
public:
    WebDatabase() : m_private(0) { }
    WebDatabase(const WebDatabase& d) : m_private(0) { assign(d); }
    ~WebDatabase() { reset(); }

    WebDatabase& operator=(const WebDatabase& d)
    {
        assign(d);
        return *this;
    }

    WEBKIT_API void reset();
    WEBKIT_API void assign(const WebDatabase&);
    bool isNull() const { return !m_private; }

    WEBKIT_API WebString name() const;
    WEBKIT_API WebString displayName() const;
    WEBKIT_API unsigned long estimatedSize() const;
    WEBKIT_API WebSecurityOrigin securityOrigin() const;

    WEBKIT_API static void setObserver(WebDatabaseObserver*);
    WEBKIT_API static WebDatabaseObserver* observer();

    WEBKIT_API static void updateDatabaseSize(
        const WebString& originIdentifier, const WebString& databaseName,
        unsigned long long databaseSize, unsigned long long spaceAvailable);
    WEBKIT_API static void closeDatabaseImmediately(
        const WebString& originIdentifier, const WebString& databaseName);

#if WEBKIT_IMPLEMENTATION
    WebDatabase(const WTF::PassRefPtr<WebCore::Database>&);
    WebDatabase& operator=(const WTF::PassRefPtr<WebCore::Database>&);
    operator WTF::PassRefPtr<WebCore::Database>() const;
#endif

private:
    void assign(WebDatabasePrivate*);

    WebDatabasePrivate* m_private;
};

} // namespace WebKit

#endif
