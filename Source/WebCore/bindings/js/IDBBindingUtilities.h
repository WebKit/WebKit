/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2014, 2015, 2016 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(INDEXED_DATABASE)

#include "IDBKeyPath.h"
#include <wtf/Forward.h>

namespace JSC {
class ExecState;
class JSGlobalObject;
class JSValue;
}

namespace WebCore {

class IDBIndexInfo;
class IDBKey;
class IDBKeyData;
class IDBValue;
class IndexKey;
class JSDOMGlobalObject;

RefPtr<IDBKey> maybeCreateIDBKeyFromScriptValueAndKeyPath(JSC::ExecState&, const JSC::JSValue&, const IDBKeyPath&);
bool canInjectIDBKeyIntoScriptValue(JSC::ExecState&, const JSC::JSValue&, const IDBKeyPath&);
bool injectIDBKeyIntoScriptValue(JSC::ExecState&, const IDBKeyData&, JSC::JSValue, const IDBKeyPath&);

void generateIndexKeyForValue(JSC::ExecState&, const IDBIndexInfo&, JSC::JSValue, IndexKey& outKey, const Optional<IDBKeyPath>&, const IDBKeyData&);

Ref<IDBKey> scriptValueToIDBKey(JSC::ExecState&, const JSC::JSValue&);

JSC::JSValue deserializeIDBValueToJSValue(JSC::ExecState&, const IDBValue&, Vector<std::pair<String, String>>&);
JSC::JSValue deserializeIDBValueToJSValue(JSC::ExecState&, const IDBValue&);
JSC::JSValue toJS(JSC::ExecState*, JSDOMGlobalObject*, const IDBValue&);
JSC::JSValue toJS(JSC::ExecState&, JSC::JSGlobalObject&, IDBKey*);
JSC::JSValue toJS(JSC::ExecState*, JSDOMGlobalObject*, const IDBKeyData&);

Optional<JSC::JSValue> deserializeIDBValueWithKeyInjection(JSC::ExecState&, const IDBValue&, const IDBKeyData&, const Optional<IDBKeyPath>&);
}

#endif // ENABLE(INDEXED_DATABASE)
