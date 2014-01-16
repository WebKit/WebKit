/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef WebIDBFactoryBackend_h
#define WebIDBFactoryBackend_h

#include <WebCore/IDBFactoryBackendInterface.h>

#if ENABLE(INDEXED_DATABASE)

namespace WebKit {

class WebIDBFactoryBackend final : public WebCore::IDBFactoryBackendInterface {
public:
    static PassRefPtr<WebIDBFactoryBackend> create(const String& databaseDirectoryIdentifier) { return adoptRef(new WebIDBFactoryBackend(databaseDirectoryIdentifier)); }

    virtual ~WebIDBFactoryBackend();

    virtual void getDatabaseNames(PassRefPtr<WebCore::IDBCallbacks>, PassRefPtr<WebCore::SecurityOrigin>, WebCore::ScriptExecutionContext*, const String& dataDir) override;
    virtual void open(const String& name, uint64_t version, int64_t transactionId, PassRefPtr<WebCore::IDBCallbacks>, PassRefPtr<WebCore::IDBDatabaseCallbacks>, const WebCore::SecurityOrigin& openingOrigin, const WebCore::SecurityOrigin& mainFrameOrigin) override;
    virtual void deleteDatabase(const String& name, PassRefPtr<WebCore::IDBCallbacks>, PassRefPtr<WebCore::SecurityOrigin>, WebCore::ScriptExecutionContext*, const String& dataDir) override;

    virtual void removeIDBDatabaseBackend(const String& uniqueIdentifier) override;

private:
    explicit WebIDBFactoryBackend(const String& databaseDirectoryIdentifier);
};

} // namespace WebKit

#endif // ENABLE(INDEXED_DATABASE)
#endif // WebIDBFactoryBackend_h
