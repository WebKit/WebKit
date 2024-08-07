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

#include "config.h"
#include "TrustedTypePolicy.h"

#include "TrustedHTML.h"
#include "TrustedScript.h"
#include "TrustedScriptURL.h"
#include "TrustedType.h"
#include "TrustedTypePolicyOptions.h"
#include "WebCoreOpaqueRoot.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(TrustedTypePolicy);

Ref<TrustedTypePolicy> TrustedTypePolicy::create(const String& name, const TrustedTypePolicyOptions& options)
{
    return adoptRef(*new TrustedTypePolicy(name, options));
}

TrustedTypePolicy::TrustedTypePolicy(const String& name, const TrustedTypePolicyOptions& options)
    : m_name(name)
    , m_options(options)
{ }

ExceptionOr<Ref<TrustedHTML>> TrustedTypePolicy::createHTML(const String& input, FixedVector<JSC::Strong<JSC::Unknown>>&& arguments)
{
    auto policyValue = getPolicyValue(TrustedType::TrustedHTML, input, WTFMove(arguments));

    if (policyValue.hasException())
        return policyValue.releaseException();

    return TrustedHTML::create(policyValue.releaseReturnValue());
}

ExceptionOr<Ref<TrustedScript>> TrustedTypePolicy::createScript(const String& input, FixedVector<JSC::Strong<JSC::Unknown>>&& arguments)
{
    auto policyValue = getPolicyValue(TrustedType::TrustedScript, input, WTFMove(arguments));

    if (policyValue.hasException())
        return policyValue.releaseException();

    return TrustedScript::create(policyValue.releaseReturnValue());
}

ExceptionOr<Ref<TrustedScriptURL>> TrustedTypePolicy::createScriptURL(const String& input, FixedVector<JSC::Strong<JSC::Unknown>>&& arguments)
{
    auto policyValue = getPolicyValue(TrustedType::TrustedScriptURL, input, WTFMove(arguments));

    if (policyValue.hasException())
        return policyValue.releaseException();

    return TrustedScriptURL::create(policyValue.releaseReturnValue());
}

// https://w3c.github.io/trusted-types/dist/spec/#get-trusted-type-policy-value-algorithm
ExceptionOr<String> TrustedTypePolicy::getPolicyValue(TrustedType trustedTypeName, const String& input, FixedVector<JSC::Strong<JSC::Unknown>>&& arguments, IfMissing ifMissing)
{
    CallbackResult<String> policyValue(CallbackResultType::UnableToExecute);
    if (trustedTypeName == TrustedType::TrustedHTML) {
        RefPtr<CreateHTMLCallback> protectedCreateHTML;
        {
            Locker locker { lock() };
            protectedCreateHTML = m_options.createHTML;
        }
        if (protectedCreateHTML && protectedCreateHTML->hasCallback())
            policyValue = protectedCreateHTML->handleEvent(input, WTFMove(arguments));
    } else if (trustedTypeName == TrustedType::TrustedScript) {
        RefPtr<CreateScriptCallback> protectedCreateScript;
        {
            Locker locker { lock() };
            protectedCreateScript = m_options.createScript;
        }
        if (protectedCreateScript && protectedCreateScript->hasCallback())
            policyValue = protectedCreateScript->handleEvent(input, WTFMove(arguments));
    } else if (trustedTypeName == TrustedType::TrustedScriptURL) {
        RefPtr<CreateScriptURLCallback> protectedCreateScriptURL;
        {
            Locker locker { lock() };
            protectedCreateScriptURL = m_options.createScriptURL;
        }
        if (protectedCreateScriptURL && protectedCreateScriptURL->hasCallback())
            policyValue = protectedCreateScriptURL->handleEvent(input, WTFMove(arguments));
    } else {
        ASSERT_NOT_REACHED();
        return Exception { ExceptionCode::TypeError };
    }

    if (policyValue.type() == CallbackResultType::Success)
        return policyValue.releaseReturnValue();
    if (policyValue.type() == CallbackResultType::ExceptionThrown)
        return Exception { ExceptionCode::ExistingExceptionError };

    if (ifMissing == IfMissing::Throw) {
        return Exception {
            ExceptionCode::TypeError,
            makeString("Policy "_s, m_name,
                "'s TrustedTypePolicyOptions did not specify a '"_s, trustedTypeToCallbackName(trustedTypeName), "' member."_s)
        };
    }

    return String(nullString());
}

WebCoreOpaqueRoot root(TrustedTypePolicy* policy)
{
    return WebCoreOpaqueRoot { policy };
}

} // namespace WebCore
