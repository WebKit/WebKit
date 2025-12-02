/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include "config.h"
#include "DocumentMediaElement.h"

#if ENABLE(VIDEO)

#include "CommonVM.h"
#include "DOMWrapperWorld.h"
#include "Document.h"
#include "FrameDestructionObserverInlines.h"
#include "RenderTheme.h"
#include "ScriptController.h"
#include "ScriptSourceCode.h"
#include <JavaScriptCore/CatchScope.h>
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/JSLock.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(DocumentMediaElement);

DocumentMediaElement& DocumentMediaElement::from(Document& document)
{
    auto* supplement = downcast<DocumentMediaElement>(Supplement<Document>::from(&document, supplementName()));
    if (!supplement) {
        auto newSupplement = makeUnique<DocumentMediaElement>(document);
        supplement = newSupplement.get();
        provideTo(&document, supplementName(), WTFMove(newSupplement));
    }
    return *supplement;
}

DocumentMediaElement::DocumentMediaElement(Document& document)
    : m_document { document }
{
}

Document& DocumentMediaElement::document() const
{
    return m_document;
}

ASCIILiteral DocumentMediaElement::supplementName()
{
    return "DocumentMediaElement"_s;
};

bool DocumentMediaElement::setupAndCallMediaControlsJS(NOESCAPE const JSSetupFunction& task)
{
    if (!ensureMediaControlsScript())
        return false;

    return setupAndCallJS(task);
}

DOMWrapperWorld& DocumentMediaElement::ensureIsolatedWorld()
{
    if (!m_isolatedWorld) {
        m_isolatedWorld = DOMWrapperWorld::create(Ref { commonVM() }, DOMWrapperWorld::Type::Internal, "Media Controls (Document)"_s);
        m_isolatedWorld->setIsMediaControls();
    }
    return *m_isolatedWorld;
}

bool DocumentMediaElement::ensureMediaControlsScript()
{
    if (m_haveParsedMediaControlsScript)
        return true;

    Ref document = this->document();
    auto mediaControlsScripts = RenderTheme::singleton().mediaControlsScripts();
    if (mediaControlsScripts.isEmpty() || document->activeDOMObjectsAreSuspended() || document->activeDOMObjectsAreStopped())
        return false;

    m_haveParsedMediaControlsScript = setupAndCallJS([mediaControlsScripts = WTFMove(mediaControlsScripts)](JSDOMGlobalObject& globalObject, JSC::JSGlobalObject&, ScriptController& scriptController, DOMWrapperWorld& world) {
        auto& vm = globalObject.vm();
        auto scope = DECLARE_THROW_SCOPE(vm);

        for (auto& mediaControlsScript : mediaControlsScripts) {
            if (mediaControlsScript.isEmpty())
                continue;
            scriptController.evaluateInWorldIgnoringException(ScriptSourceCode(mediaControlsScript, JSC::SourceTaintedOrigin::Untainted), world);
            RETURN_IF_EXCEPTION(scope, false);
        }

        return true;
    });
    return m_haveParsedMediaControlsScript;
}

bool DocumentMediaElement::setupAndCallJS(NOESCAPE const JSSetupFunction& task)
{
    Ref world = ensureIsolatedWorld();
    Ref protectedDocument = this->document();
    RefPtr protectedFrame = protectedDocument->frame();
    if (!protectedFrame)
        return false;
    CheckedRef scriptController = protectedFrame->script();
    auto* globalObject = JSC::jsCast<JSDOMGlobalObject*>(scriptController->globalObject(world));
    if (!globalObject)
        return false;
    auto& vm = globalObject->vm();
    JSC::JSLockHolder lock(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);
    auto* lexicalGlobalObject = globalObject;

    auto reportExceptionAndReturnFalse = [&] () -> bool {
        auto* exception = scope.exception();
        scope.clearException();
        reportException(globalObject, exception);
        return false;
    };

    auto result = task(*globalObject, *lexicalGlobalObject, scriptController, world);
    RETURN_IF_EXCEPTION(scope, reportExceptionAndReturnFalse());
    return result;
}

}

#endif
