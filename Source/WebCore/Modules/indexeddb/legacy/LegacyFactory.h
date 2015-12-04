/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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
#ifndef LegacyFactory_h
#define LegacyFactory_h

#include "IDBFactory.h"
#include "IDBOpenDBRequest.h"
#include "ScriptWrappable.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

class IDBKey;
class IDBKeyRange;
class IDBFactoryBackendInterface;
class ScriptExecutionContext;

typedef int ExceptionCode;

class LegacyFactory : public ScriptWrappable, public IDBFactory {
public:
    static PassRefPtr<LegacyFactory> create(IDBFactoryBackendInterface* factory)
    {
        // FIXME: While the feature is under development we'll handle a null factory backend here,
        // returning null, so javascript can't try to use the feature.
        // Once the feature is fully functional we should remove the null factory backend.
        return factory ? adoptRef(new LegacyFactory(factory)) : nullptr;
    }
    ~LegacyFactory();

    // FIXME: getDatabaseNames is no longer a web-facing API, and should be removed from LegacyFactory.
    // The Web Inspector currently uses this to enumerate the list of databases, but is more complicated as a result.
    // We should provide a simpler API to the Web Inspector then remove getDatabaseNames.
    virtual RefPtr<IDBRequest> getDatabaseNames(ScriptExecutionContext*, ExceptionCode&) override;

    virtual RefPtr<IDBOpenDBRequest> open(ScriptExecutionContext*, const String& name, ExceptionCode&) override;
    virtual RefPtr<IDBOpenDBRequest> open(ScriptExecutionContext*, const String& name, unsigned long long version, ExceptionCode&) override;
    virtual RefPtr<IDBOpenDBRequest> deleteDatabase(ScriptExecutionContext*, const String& name, ExceptionCode&) override;

    short cmp(ScriptExecutionContext*, const Deprecated::ScriptValue& first, const Deprecated::ScriptValue& second, ExceptionCodeWithMessage&) override;

private:
    LegacyFactory(IDBFactoryBackendInterface*);

    PassRefPtr<IDBOpenDBRequest> openInternal(ScriptExecutionContext*, const String& name, uint64_t version, IndexedDB::VersionNullness, ExceptionCode&);

    RefPtr<IDBFactoryBackendInterface> m_backend;
};

} // namespace WebCore

#endif

#endif // LegacyFactory_h
