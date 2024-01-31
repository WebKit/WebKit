/*
 * Copyright (C) 2024 Igalia S.L. All rights reserved.
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

#include "CreateHTMLCallback.h"
#include "CreateScriptCallback.h"
#include "CreateScriptURLCallback.h"
#include "ExceptionOr.h"
#include "ScriptWrappable.h"
#include <JavaScriptCore/Strong.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class TrustedHTML;
class TrustedScript;
class TrustedScriptURL;
struct TrustedTypePolicyOptions;

class TrustedTypePolicy : public ScriptWrappable, public RefCounted<TrustedTypePolicy> {
    WTF_MAKE_ISO_ALLOCATED(TrustedTypePolicy);
public:
    static Ref<TrustedTypePolicy> create(const String&, const TrustedTypePolicyOptions&);
    ~TrustedTypePolicy() = default;
    ExceptionOr<Ref<TrustedHTML>> createHTML(const String& input, FixedVector<JSC::Strong<JSC::Unknown>>&&);
    ExceptionOr<Ref<TrustedScript>> createScript(const String& input, FixedVector<JSC::Strong<JSC::Unknown>>&&);
    ExceptionOr<Ref<TrustedScriptURL>> createScriptURL(const String& input, FixedVector<JSC::Strong<JSC::Unknown>>&&);
    const String name() const { return m_name; }

private:
    TrustedTypePolicy(const String&, const TrustedTypePolicyOptions&);

    String m_name;
    RefPtr<CreateHTMLCallback> m_createHTMLCallback;
    RefPtr<CreateScriptCallback> m_createScriptCallback;
    RefPtr<CreateScriptURLCallback> m_createScriptURLCallback;
};

} // namespace WebCore
