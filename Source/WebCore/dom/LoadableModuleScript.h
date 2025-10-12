/*
 * Copyright (C) 2016-2025 Apple, Inc. All rights reserved.
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

#include <WebCore/LoadableScript.h>
#include <WebCore/LoadableScriptError.h>
#include <wtf/TypeCasts.h>

namespace WebCore {

class ScriptSourceCode;
class ModuleFetchParameters;

class LoadableModuleScript final : public LoadableScript {
public:
    virtual ~LoadableModuleScript();

    enum class IsInline : bool { No, Yes };
    static Ref<LoadableModuleScript> create(IsInline, const AtomString& nonce, const AtomString& integrity, ReferrerPolicy, RequestPriority, const AtomString& crossOriginMode, const AtomString& charset, const AtomString& initiatorType, bool isInUserAgentShadowTree);

    bool isLoaded() const final;
    bool hasError() const final;
    std::optional<Error> takeError() final;
    bool wasCanceled() const final;
    bool isInlineModule() const final { return m_isInline; }

    ScriptType scriptType() const final { return ScriptType::Module; }

    void execute(ScriptElement&) final;

    void notifyLoadCompleted(UniquedStringImpl&);
    void notifyLoadFailed(LoadableScript::Error&&);
    void notifyLoadWasCanceled();

    UniquedStringImpl* moduleKey() const { return m_moduleKey.get(); }
    RefPtr<UniquedStringImpl> protectedModuleKey() const { return m_moduleKey; }

    ModuleFetchParameters& parameters() { return m_parameters.get(); }

private:
    LoadableModuleScript(IsInline, const AtomString& nonce, const AtomString& integrity, ReferrerPolicy, RequestPriority, const AtomString& crossOriginMode, const AtomString& charset, const AtomString& initiatorType, bool isInUserAgentShadowTree);

    const Ref<ModuleFetchParameters> m_parameters;
    RefPtr<UniquedStringImpl> m_moduleKey;
    std::optional<LoadableScript::Error> m_error;
    bool m_wasCanceled { false };
    bool m_isLoaded { false };
    bool m_isInline { false };
};

}

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::LoadableModuleScript)
    static bool isType(const WebCore::LoadableScript& script) { return script.isModuleScript(); }
SPECIALIZE_TYPE_TRAITS_END()
