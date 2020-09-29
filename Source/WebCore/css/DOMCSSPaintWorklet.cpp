/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DOMCSSPaintWorklet.h"

#if ENABLE(CSS_PAINTING_API)

#include "DOMCSSNamespace.h"
#include "Document.h"
#include "PaintWorkletGlobalScope.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

PaintWorklet& DOMCSSPaintWorklet::ensurePaintWorklet(Document& document)
{
    return document.ensurePaintWorklet();
}

DOMCSSPaintWorklet* DOMCSSPaintWorklet::from(DOMCSSNamespace& css)
{
    auto* supplement = static_cast<DOMCSSPaintWorklet*>(Supplement<DOMCSSNamespace>::from(&css, supplementName()));
    if (!supplement) {
        auto newSupplement = makeUnique<DOMCSSPaintWorklet>(css);
        supplement = newSupplement.get();
        provideTo(&css, supplementName(), WTFMove(newSupplement));
    }
    return supplement;
}

const char* DOMCSSPaintWorklet::supplementName()
{
    return "DOMCSSPaintWorklet";
}

void PaintWorklet::addModule(Document& document, const String& moduleURL, WorkletOptions&&, DOMPromiseDeferred<void>&& promise)
{
    // FIXME: We should download the source from the URL
    // https://bugs.webkit.org/show_bug.cgi?id=191136
    auto maybeContext = PaintWorkletGlobalScope::tryCreate(document, ScriptSourceCode(moduleURL));
    if (UNLIKELY(!maybeContext)) {
        promise.reject(Exception { OutOfMemoryError });
        return;
    }
    auto context = maybeContext.releaseNonNull();
    context->evaluate();

    auto locker = holdLock(context->paintDefinitionLock());
    for (auto& name : context->paintDefinitionMap().keys())
        document.setPaintWorkletGlobalScopeForName(name, makeRef(context.get()));
    promise.resolve();
}

}
#endif
