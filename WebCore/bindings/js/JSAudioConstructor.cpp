/*
 * Copyright (C) 2007, 2008, 2010 Apple Inc. All rights reserved.
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
#include "JSHTMLAudioElement.h"
#include <runtime/Error.h>

using namespace JSC;

namespace WebCore {

const ClassInfo JSAudioConstructor::s_info = { "AudioConstructor", 0, 0, 0 };

JSAudioConstructor::JSAudioConstructor(ExecState* exec, JSDOMGlobalObject* globalObject)
    : DOMConstructorWithDocument(JSAudioConstructor::createStructure(globalObject->objectPrototype()), globalObject)
{
    putDirect(exec->propertyNames().prototype, JSHTMLAudioElementPrototype::self(exec, globalObject), None);
    putDirect(exec->propertyNames().length, jsNumber(exec, 1), ReadOnly | DontDelete | DontEnum);
}

static JSObject* constructAudio(ExecState* exec, JSObject* constructor, const ArgList& args)
{
    JSAudioConstructor* jsConstructor = static_cast<JSAudioConstructor*>(constructor);

    Document* document = jsConstructor->document();
    if (!document)
        return throwError(exec, ReferenceError, "Audio constructor associated document is unavailable");

    // Calling toJS on the document causes the JS document wrapper to be
    // added to the window object. This is done to ensure that JSDocument::markChildren
    // will be called, which will cause the audio element to be marked if necessary.
    toJS(exec, jsConstructor->globalObject(), document);

    // FIXME: This converts an undefined argument to the string "undefined", but possibly we
    // should treat it as if no argument was passed instead, by checking the value of args.at
    // rather than looking at args.size.
    String src;
    if (args.size() > 0)
        src = ustringToString(args.at(0).toString(exec));
    return asObject(toJS(exec, jsConstructor->globalObject(),
        HTMLAudioElement::createForJSConstructor(document, src)));
}

ConstructType JSAudioConstructor::getConstructData(ConstructData& constructData)
{
    constructData.native.function = constructAudio;
    return ConstructTypeHost;
}

} // namespace WebCore

#endif // ENABLE(VIDEO)
