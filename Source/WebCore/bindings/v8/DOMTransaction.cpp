/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL GOOGLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DOMTransaction.h"

#if ENABLE(UNDO_MANAGER)

#include "Frame.h"
#include "UndoManager.h"
#include "V8DOMTransaction.h"
#include "V8HiddenPropertyName.h"
#include "V8UndoManager.h"

namespace WebCore {

DOMTransaction::DOMTransaction(const WorldContextHandle& worldContext)
    : m_worldContext(worldContext)
    , m_undoManager(0)
{
}

PassRefPtr<DOMTransaction> DOMTransaction::create(const WorldContextHandle& worldContext)
{
    return adoptRef(new DOMTransaction(worldContext));
}

void DOMTransaction::apply()
{
    m_isAutomatic = !getFunction("executeAutomatic").IsEmpty();
    if (!m_isAutomatic)
        callFunction("execute");
}

void DOMTransaction::unapply()
{
    if (!m_isAutomatic)
        callFunction("undo");

    if (m_undoManager)
        m_undoManager->registerRedoStep(this);
}

void DOMTransaction::reapply()
{
    if (!m_isAutomatic)
        callFunction("redo");

    if (m_undoManager)
        m_undoManager->registerUndoStep(this);
}

v8::Handle<v8::Function> DOMTransaction::getFunction(const char* propertyName)
{
    v8::Handle<v8::Object> wrapper = v8::Handle<v8::Object>::Cast(toV8(this));
    if (wrapper.IsEmpty())
        return v8::Handle<v8::Function>();

    v8::Local<v8::Value> data = wrapper->GetHiddenValue(V8HiddenPropertyName::domTransactionData());
    if (data.IsEmpty() || !data->IsObject())
        return v8::Handle<v8::Function>();

    v8::Local<v8::Value> function = v8::Local<v8::Object>::Cast(data)->Get(v8::String::NewSymbol(propertyName));
    if (function.IsEmpty() || !function->IsFunction())
        return v8::Handle<v8::Function>();

    return v8::Handle<v8::Function>::Cast(function);
}

void DOMTransaction::callFunction(const char* propertyName)
{
    if (!m_undoManager || !m_undoManager->undoScopeHost())
        return;

    Document* document = m_undoManager->undoScopeHost()->document();
    if (!document)
        return;

    Frame* frame = document->frame();
    if (!frame || !frame->script()->canExecuteScripts(AboutToExecuteScript))
        return;

    v8::Handle<v8::Function> function = getFunction(propertyName);
    if (function.IsEmpty())
        return;

    v8::Local<v8::Context> v8Context = m_worldContext.adjustedContext(frame->script()->proxy());
    if (v8Context.IsEmpty())
        return;

    v8::Handle<v8::Object> receiver = v8::Handle<v8::Object>::Cast(toV8(m_undoManager));
    if (receiver.IsEmpty())
        return;
    v8::Handle<v8::Value> parameters[0] = { };
    frame->script()->proxy()->callFunction(function, receiver, 0, parameters);
}

}

#endif
