/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ScriptObjectQuarantine.h"

#include "Document.h"
#include "Frame.h"
#include "JSDOMBinding.h"
#include "JSInspectedObjectWrapper.h"
#include "JSNode.h"
#include "ScriptObject.h"
#include "ScriptValue.h"

#include <runtime/JSLock.h>

#if ENABLE(DATABASE)
#include "Database.h"
#include "JSDatabase.h"
#endif

#if ENABLE(DOM_STORAGE)
#include "JSStorage.h"
#endif

using namespace JSC;

namespace WebCore {

ScriptValue quarantineValue(ScriptState* scriptState, const ScriptValue& value)
{
    JSLock lock(false);
    return ScriptValue(JSInspectedObjectWrapper::wrap(scriptState, value.jsValue()));
}

#if ENABLE(DATABASE)
bool getQuarantinedScriptObject(Database* database, ScriptObject& quarantinedObject)
{
    ASSERT(database);

    Frame* frame = database->document()->frame();
    if (!frame)
        return false;

    ExecState* exec = toJSDOMWindow(frame)->globalExec();

    JSLock lock(false);
    quarantinedObject = ScriptObject(asObject(JSInspectedObjectWrapper::wrap(exec, toJS(exec, database))));

    return true;
}
#endif

#if ENABLE(DOM_STORAGE)
bool getQuarantinedScriptObject(Frame* frame, Storage* storage, ScriptObject& quarantinedObject)
{
    ASSERT(frame);
    ASSERT(storage);

    ExecState* exec = toJSDOMWindow(frame)->globalExec();

    JSLock lock(false);
    quarantinedObject = ScriptObject(asObject(JSInspectedObjectWrapper::wrap(exec, toJS(exec, storage))));

    return true;
}
#endif

bool getQuarantinedScriptObject(Node* node, ScriptObject& quarantinedObject)
{
    ExecState* exec = scriptStateFromNode(node);
    if (!exec)
        return false;

    JSLock lock(false);
    quarantinedObject = ScriptObject(asObject(JSInspectedObjectWrapper::wrap(exec, toJS(exec, node))));

    return true;
}

bool getQuarantinedScriptObject(DOMWindow* domWindow, ScriptObject& quarantinedObject)
{
    ASSERT(domWindow);

    JSDOMWindow* window = toJSDOMWindow(domWindow->frame());
    ExecState* exec = window->globalExec();

    JSLock lock(false);
    quarantinedObject = ScriptObject(asObject(JSInspectedObjectWrapper::wrap(exec, window)));

    return true;
}


} // namespace WebCore
