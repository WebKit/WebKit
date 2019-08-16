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

#pragma once

#if ENABLE(CSS_PAINTING_API)

#include "CSSPaintCallback.h"
#include "WorkletGlobalScope.h"
#include <JavaScriptCore/JSObject.h>
#include <JavaScriptCore/Strong.h>

namespace JSC {
class JSObject;
} // namespace JSC

namespace WebCore {
class JSDOMGlobalObject;

class PaintWorkletGlobalScope : public WorkletGlobalScope {
    WTF_MAKE_ISO_ALLOCATED(PaintWorkletGlobalScope);
public:
    static Ref<PaintWorkletGlobalScope> create(Document&, ScriptSourceCode&&);

    ExceptionOr<void> registerPaint(JSC::ExecState&, JSDOMGlobalObject&, const String& name, JSC::Strong<JSC::JSObject> paintConstructor);
    double devicePixelRatio() const;

    // All paint definitions must be destroyed before the vm is destroyed, because otherwise they will point to freed memory.
    struct PaintDefinition : public CanMakeWeakPtr<PaintDefinition> {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;
        PaintDefinition(const AtomString& name, JSC::JSObject* paintConstructor, Ref<CSSPaintCallback>&&, Vector<String>&& inputProperties, Vector<String>&& inputArguments);

        const AtomString name;
        const JSC::JSObject* const paintConstructor;
        const Ref<CSSPaintCallback> paintCallback;
        const Vector<String> inputProperties;
        const Vector<String> inputArguments;
    };

    HashMap<String, std::unique_ptr<PaintDefinition>>& paintDefinitionMap() { ASSERT(m_paintDefinitionLock.isLocked()); return m_paintDefinitionMap; }
    Lock& paintDefinitionLock() { return m_paintDefinitionLock; }

    void prepareForDestruction() final
    {
        {
            auto locker = holdLock(paintDefinitionLock());
            paintDefinitionMap().clear();
        }
        WorkletGlobalScope::prepareForDestruction();
    }

private:
    PaintWorkletGlobalScope(Document&, ScriptSourceCode&&);

    ~PaintWorkletGlobalScope()
    {
#if !ASSERT_DISABLED
        auto locker = holdLock(paintDefinitionLock());
        ASSERT(paintDefinitionMap().isEmpty());
#endif
    }

    bool isPaintWorkletGlobalScope() const final { return true; }

    HashMap<String, std::unique_ptr<PaintDefinition>> m_paintDefinitionMap;
    Lock m_paintDefinitionLock;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::PaintWorkletGlobalScope)
static bool isType(const WebCore::ScriptExecutionContext& context) { return is<WebCore::WorkletGlobalScope>(context) && downcast<WebCore::WorkletGlobalScope>(context).isPaintWorkletGlobalScope(); }
static bool isType(const WebCore::WorkletGlobalScope& context) { return context.isPaintWorkletGlobalScope(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(CSS_PAINTING_API)
