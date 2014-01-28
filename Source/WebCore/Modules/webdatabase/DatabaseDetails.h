/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef DatabaseDetails_h
#define DatabaseDetails_h

#if ENABLE(SQL_DATABASE)

#include <thread>
#include <wtf/text/WTFString.h>

namespace WebCore {

class DatabaseDetails {
public:
    DatabaseDetails()
        : m_expectedUsage(0)
        , m_currentUsage(0)
        , m_creationTime(0)
        , m_modificationTime(0)
#ifndef NDEBUG
        , m_threadID(std::this_thread::get_id())
#endif
    {
    }

    DatabaseDetails(const String& databaseName, const String& displayName, unsigned long long expectedUsage, unsigned long long currentUsage, double creationTime, double modificationTime)
        : m_name(databaseName)
        , m_displayName(displayName)
        , m_expectedUsage(expectedUsage)
        , m_currentUsage(currentUsage)
        , m_creationTime(creationTime)
        , m_modificationTime(modificationTime)
#ifndef NDEBUG
        , m_threadID(std::this_thread::get_id())
#endif
    {
    }

    const String& name() const { return m_name; }
    const String& displayName() const { return m_displayName; }
    uint64_t expectedUsage() const { return m_expectedUsage; }
    uint64_t currentUsage() const { return m_currentUsage; }
    double creationTime() const { return m_creationTime; }
    double modificationTime() const { return m_modificationTime; }
#ifndef NDEBUG
    std::thread::id threadID() const { return m_threadID; }
#endif

private:
    String m_name;
    String m_displayName;
    uint64_t m_expectedUsage;
    uint64_t m_currentUsage;
    double m_creationTime;
    double m_modificationTime;
#ifndef NDEBUG
    std::thread::id m_threadID;
#endif
};

} // namespace WebCore

#endif

#endif // DatabaseDetails_h
