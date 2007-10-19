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
#ifndef DatabaseTracker_h
#define DatabaseTracker_h

#include "PlatformString.h"
#include "SQLDatabase.h"
#include "StringHash.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/OwnPtr.h>

namespace WebCore {

class DatabaseTracker {
public:
    static void setDatabasePath(const String&);
    static const String& databasePath();

    String fullPathForDatabase(const String& origin, const String& name);

    const HashSet<String>& origins();
    bool databaseNamesForOrigin(const String& origin, Vector<String>& result);

    void deleteAllDatabases();
    void deleteAllDatabasesForOrigin(const String& origin);
    void deleteDatabase(const String& origin, const String& name);

    static DatabaseTracker& tracker();
private:
    DatabaseTracker();

    bool addDatabase(const String& origin, const String& name, const String& path);
    void populateOrigins();

    SQLDatabase m_database;
    mutable OwnPtr<HashSet<String> > m_origins;

    static String s_databasePath;
};

} // namespace WebCore

#endif // DatabaseTracker_h
