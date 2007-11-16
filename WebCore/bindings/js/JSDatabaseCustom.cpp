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

#include "config.h"
#include "JSDatabase.h"

#include "Database.h"
#include "Document.h"
#include "DOMWindow.h"
#include "ExceptionCode.h"
#include "kjs_window.h"
#include "JSCustomSQLTransactionCallback.h"
#include "JSCustomSQLTransactionErrorCallback.h"
#include "JSCustomVoidCallback.h"
#include "PlatformString.h"
#include "SQLValue.h"
#include <kjs/array_instance.h>

namespace WebCore {

using namespace KJS;

JSValue* JSDatabase::changeVersion(ExecState* exec, const List& args)
{
    String oldVersion = args[0]->toString(exec);
    String newVersion = args[1]->toString(exec);

    Frame* frame = Window::retrieveActive(exec)->impl()->frame();
    if (!frame)
        return jsUndefined();
    
    JSObject *object;
    if (!(object = args[2]->getObject())) {
        setDOMException(exec, TYPE_MISMATCH_ERR);
        return jsUndefined();
    }
    
    RefPtr<SQLTransactionCallback> callback(new JSCustomSQLTransactionCallback(object, frame));
    
    RefPtr<SQLTransactionErrorCallback> errorCallback;
    if (!args[3]->isNull()) {
        if (!(object = args[3]->getObject())) {
            setDOMException(exec, TYPE_MISMATCH_ERR);
            return jsUndefined();
        }
        
        errorCallback = new JSCustomSQLTransactionErrorCallback(object, frame);
    }
    
    RefPtr<VoidCallback> successCallback;
    if (!args[4]->isNull()) {
        bool ok;
        successCallback = toVoidCallback(exec, args[4], ok);
        if (!ok) {
            setDOMException(exec, TYPE_MISMATCH_ERR);
            return jsUndefined();
        }
    }
    
    m_impl->changeVersion(oldVersion, newVersion, callback.release(), errorCallback.release(), successCallback.release());
    
    return jsUndefined();
}

JSValue* JSDatabase::transaction(ExecState* exec, const List& args)
{
    JSObject* object;
    
    if (!(object = args[0]->getObject())) {
        setDOMException(exec, TYPE_MISMATCH_ERR);
        return jsUndefined();
    }        
 
    Frame* frame = Window::retrieveActive(exec)->impl()->frame();
    if (!frame)
        return jsUndefined();
    
    RefPtr<SQLTransactionCallback> callback(new JSCustomSQLTransactionCallback(object, frame));
    RefPtr<SQLTransactionErrorCallback> errorCallback;
    
    if (args.size() > 1 && !args[1]->isNull()) {
        if (!(object = args[1]->getObject())) {
            setDOMException(exec, TYPE_MISMATCH_ERR);
            return jsUndefined();
        }

        errorCallback = new JSCustomSQLTransactionErrorCallback(object, frame);
    }

    RefPtr<VoidCallback> successCallback;
    if (args.size() > 2 && !args[2]->isNull()) {
        bool ok;
        successCallback = toVoidCallback(exec, args[2], ok);
        if (!ok) {
            setDOMException(exec, TYPE_MISMATCH_ERR);
            return jsUndefined();
        }
    }
    
    m_impl->transaction(callback.release(), errorCallback.release(), successCallback.release());

    return jsUndefined();
}
    
}
