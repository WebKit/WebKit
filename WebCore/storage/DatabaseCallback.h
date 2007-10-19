/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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

#ifndef DatabaseCallback_h
#define DatabaseCallback_h

#include "Threading.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class SQLCallback;
class SQLResultSet;
class VersionChangeCallback;

class DatabaseCallback : public ThreadSafeShared<DatabaseCallback> {
public:
    virtual ~DatabaseCallback() { }
    virtual void performCallback() = 0;
};

class DatabaseChangeVersionCallback : public DatabaseCallback {
public:
    DatabaseChangeVersionCallback(PassRefPtr<VersionChangeCallback>, bool versionChanged);
    virtual ~DatabaseChangeVersionCallback() { }
    virtual void performCallback();

private:
    RefPtr<VersionChangeCallback> m_callback;
    bool m_versionChanged;
};

class DatabaseExecuteSqlCallback : public DatabaseCallback {
public:
    DatabaseExecuteSqlCallback(PassRefPtr<SQLCallback>, PassRefPtr<SQLResultSet>);
    virtual ~DatabaseExecuteSqlCallback() { }
    virtual void performCallback();
private:
    RefPtr<SQLCallback> m_callback;
    RefPtr<SQLResultSet> m_resultSet;
};

} // namespace WebCore

#endif // DatabaseCallback_h
