/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#include "JSInternals.h"

#include <runtime/Error.h>

using namespace JSC;

namespace WebCore {

JSValue JSInternals::userPreferredLanguages(ExecState* exec) const
{
    Internals* imp = static_cast<Internals*>(impl());
    const Vector<String> languages = imp->userPreferredLanguages();
    if (languages.isEmpty())
        return jsNull();
    
    MarkedArgumentBuffer array;
    Vector<String>::const_iterator end = languages.end();
    for (Vector<String>::const_iterator it = languages.begin(); it != end; ++it)
        array.append(jsString(exec, stringToUString(*it)));
    return constructArray(exec, globalObject(), array);
}

void JSInternals::setUserPreferredLanguages(ExecState* exec, JSValue value)
{
    if (!isJSArray(value)) {
        throwError(exec, createSyntaxError(exec, "setUserPreferredLanguages: Expected Array"));
        return;
    }
    
    Vector<String> languages;
    JSArray* array = asArray(value);
    for (unsigned i = 0; i < array->length(); ++i) {
        String language = ustringToString(array->getIndex(i).toString(exec));
        languages.append(language);
    }
    
    Internals* imp = static_cast<Internals*>(impl());
    imp->setUserPreferredLanguages(languages);
}
    
} // namespace WebCore
