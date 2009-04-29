/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
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
 */

#include "config.h"

#if ENABLE(VIDEO)

#include "JSAudioConstructor.h"

#include "HTMLAudioElement.h"
#include "HTMLNames.h"
#include "JSHTMLAudioElement.h"
#include "ScriptExecutionContext.h"
#include "Text.h"

using namespace JSC;

namespace WebCore {

const ClassInfo JSAudioConstructor::s_info = { "AudioConstructor", 0, 0, 0 };

JSAudioConstructor::JSAudioConstructor(ExecState* exec, JSDOMGlobalObject* globalObject)
    : DOMObject(JSAudioConstructor::createStructure(exec->lexicalGlobalObject()->objectPrototype()))
    , m_globalObject(globalObject)
{
    ASSERT(globalObject->scriptExecutionContext());
    ASSERT(globalObject->scriptExecutionContext()->isDocument());

    putDirect(exec->propertyNames().prototype, JSHTMLAudioElementPrototype::self(exec, exec->lexicalGlobalObject()), None);
    putDirect(exec->propertyNames().length, jsNumber(exec, 1), ReadOnly|DontDelete|DontEnum);
}

Document* JSAudioConstructor::document() const
{
    return static_cast<Document*>(m_globalObject->scriptExecutionContext());
}

static JSObject* constructAudio(ExecState* exec, JSObject* constructor, const ArgList& args)
{
    // FIXME: Why doesn't this need the call toJS on the document like JSImageConstructor?

    Document* document = static_cast<JSAudioConstructor*>(constructor)->document();
    if (!document)
        return throwError(exec, ReferenceError, "Audio constructor associated document is unavailable");

    RefPtr<HTMLAudioElement> audio = new HTMLAudioElement(HTMLNames::audioTag, document);
    if (args.size() > 0) {
        audio->setSrc(args.at(0).toString(exec));
        audio->scheduleLoad();
    }
    return asObject(toJS(exec, audio.release()));
}

ConstructType JSAudioConstructor::getConstructData(ConstructData& constructData)
{
    constructData.native.function = constructAudio;
    return ConstructTypeHost;
}

void JSAudioConstructor::mark()
{
    DOMObject::mark();
    if (!m_globalObject->marked())
        m_globalObject->mark();
}

} // namespace WebCore

#endif // ENABLE(VIDEO)
