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

#include "config.h"
#include "JSDatabaseSync.h"

#if ENABLE(DATABASE)

#include "DatabaseSync.h"
#include "ExceptionCode.h"
#include "JSSQLTransactionSyncCallback.h"
#include "PlatformString.h"
#include "SQLValue.h"
#include <runtime/JSArray.h>

namespace WebCore {

using namespace JSC;

JSValue JSDatabaseSync::changeVersion(ExecState* exec, const ArgList& args)
{
    String oldVersion = ustringToString(args.at(0).toString(exec));
    if (exec->hadException())
        return jsUndefined();

    String newVersion = ustringToString(args.at(1).toString(exec));
    if (exec->hadException())
        return jsUndefined();

    JSObject* object = args.at(2).getObject();
    if (!object) {
        setDOMException(exec, TYPE_MISMATCH_ERR);
        return jsUndefined();
    }

    RefPtr<SQLTransactionSyncCallback> callback(JSSQLTransactionSyncCallback::create(object, static_cast<JSDOMGlobalObject*>(globalObject())));

    ExceptionCode ec = 0;
    m_impl->changeVersion(oldVersion, newVersion, callback.release(), ec);
    setDOMException(exec, ec);

    return jsUndefined();
}

static JSValue createTransaction(ExecState* exec, const ArgList& args, DatabaseSync* database, JSDOMGlobalObject* globalObject, bool readOnly)
{
    JSObject* object = args.at(0).getObject();
    if (!object) {
        setDOMException(exec, TYPE_MISMATCH_ERR);
        return jsUndefined();
    }

    RefPtr<SQLTransactionSyncCallback> callback(JSSQLTransactionSyncCallback::create(object, globalObject));

    ExceptionCode ec = 0;
    database->transaction(callback.release(), readOnly, ec);
    setDOMException(exec, ec);

    return jsUndefined();
}

JSValue JSDatabaseSync::transaction(ExecState* exec, const ArgList& args)
{
    return createTransaction(exec, args, m_impl.get(), static_cast<JSDOMGlobalObject*>(globalObject()), false);
}

JSValue JSDatabaseSync::readTransaction(ExecState* exec, const ArgList& args)
{
    return createTransaction(exec, args, m_impl.get(), static_cast<JSDOMGlobalObject*>(globalObject()), true);
}

}

#endif // ENABLE(DATABASE)
