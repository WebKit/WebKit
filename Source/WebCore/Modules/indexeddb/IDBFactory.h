/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef IDBFactory_h
#define IDBFactory_h

#include "IDBOpenDBRequest.h"
#include "ScriptWrappable.h"
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

class IDBKey;
class IDBKeyRange;
class ScriptExecutionContext;

struct ExceptionCodeWithMessage;

typedef int ExceptionCode;

class IDBFactory : public RefCounted<IDBFactory> {
public:
    virtual ~IDBFactory() { }

    // FIXME: getDatabaseNames is no longer a web-facing API, and should be removed from IDBFactory.
    // The Web Inspector currently uses this to enumerate the list of databases, but is more complicated as a result.
    // We should provide a simpler API to the Web Inspector then remove getDatabaseNames.
    virtual RefPtr<IDBRequest> getDatabaseNames(ScriptExecutionContext*, ExceptionCode&) = 0;

    virtual RefPtr<IDBOpenDBRequest> open(ScriptExecutionContext*, const String& name, ExceptionCode&) = 0;
    virtual RefPtr<IDBOpenDBRequest> open(ScriptExecutionContext*, const String& name, unsigned long long version, ExceptionCode&) = 0;
    virtual RefPtr<IDBOpenDBRequest> deleteDatabase(ScriptExecutionContext*, const String& name, ExceptionCode&) = 0;

    virtual short cmp(ScriptExecutionContext*, const Deprecated::ScriptValue& first, const Deprecated::ScriptValue& second, ExceptionCodeWithMessage&) = 0;

protected:
    IDBFactory();
};

} // namespace WebCore

#endif

#endif // IDBFactory_h
