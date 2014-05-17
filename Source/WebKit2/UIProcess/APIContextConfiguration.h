/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef APIContextConfiguration_h
#define APIContextConfiguration_h

#include "APIObject.h"
#include <wtf/text/WTFString.h>

namespace WebKit {
struct WebContextConfiguration;
}

namespace API {

class ContextConfiguration : public ObjectImpl<Object::Type::ContextConfiguration> {
public:
    static PassRefPtr<ContextConfiguration> create()
    {
        return adoptRef(new ContextConfiguration);
    }
    virtual ~ContextConfiguration();

    String indexedDBDatabaseDirectory() const { return m_indexedDBDatabaseDirectory; }
    void setIndexedDBDatabaseDirectory(const String& indexedDBDatabaseDirectory) { m_indexedDBDatabaseDirectory = indexedDBDatabaseDirectory; }

    String injectedBundlePath() const { return m_injectedBundlePath; }
    void setInjectedBundlePath(const String& injectedBundlePath) { m_injectedBundlePath = injectedBundlePath; }

    String localStorageDirectory() const { return m_localStorageDirectory; }
    void setLocalStorageDirectory(const String& localStorageDirectory) { m_localStorageDirectory = localStorageDirectory; }

    String webSQLDatabaseDirectory() const { return m_webSQLDatabaseDirectory; }
    void setWebSQLDatabaseDirectory(const String& webSQLDatabaseDirectory) { m_webSQLDatabaseDirectory = webSQLDatabaseDirectory; }

    WebKit::WebContextConfiguration webContextConfiguration() const;

private:
    ContextConfiguration();

    String m_indexedDBDatabaseDirectory;
    String m_injectedBundlePath;
    String m_localStorageDirectory;
    String m_webSQLDatabaseDirectory;
};

} // namespace API

#endif // APIContextConfiguration_h
