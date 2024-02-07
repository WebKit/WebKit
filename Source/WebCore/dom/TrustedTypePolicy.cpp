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
#include "TrustedTypePolicyOptions.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(TrustedTypePolicy);

Ref<TrustedTypePolicy> TrustedTypePolicy::create(const String& name, const TrustedTypePolicyOptions& options)
{
    return adoptRef(*new TrustedTypePolicy(name, options));
}

TrustedTypePolicy::TrustedTypePolicy(const String& name, const TrustedTypePolicyOptions& options)
    : m_name(name)
    , m_createHTMLCallback(options.createHTML)
    , m_createScriptCallback(options.createScript)
    , m_createScriptURLCallback(options.createScriptURL)
{ }

ExceptionOr<Ref<TrustedHTML>> TrustedTypePolicy::createHTML(const String& input, FixedVector<JSC::Strong<JSC::Unknown>>&& arguments)
{
    auto policyValue = getPolicyValue("TrustedHTML"_s, input, WTFMove(arguments));

    if (policyValue.hasException())
        return policyValue.releaseException();

    return TrustedHTML::create(policyValue.releaseReturnValue());
}

ExceptionOr<Ref<TrustedScript>> TrustedTypePolicy::createScript(const String& input, FixedVector<JSC::Strong<JSC::Unknown>>&& arguments)
{
    auto policyValue = getPolicyValue("TrustedScript"_s, input, WTFMove(arguments));

    if (policyValue.hasException())
        return policyValue.releaseException();

    return TrustedScript::create(policyValue.releaseReturnValue());
}

ExceptionOr<Ref<TrustedScriptURL>> TrustedTypePolicy::createScriptURL(const String& input, FixedVector<JSC::Strong<JSC::Unknown>>&& arguments)
{
    auto policyValue = getPolicyValue("TrustedScriptURL"_s, input, WTFMove(arguments));

    if (policyValue.hasException())
        return policyValue.releaseException();

    return TrustedScriptURL::create(policyValue.releaseReturnValue());
}

// https://w3c.github.io/trusted-types/dist/spec/#get-trusted-type-policy-value-algorithm
ExceptionOr<String> TrustedTypePolicy::getPolicyValue(const String& trustedTypeName, const String& input, FixedVector<JSC::Strong<JSC::Unknown>>&& arguments)
{
    auto callbackName = emptyString();
    CallbackResult<String> callbackResult(CallbackResultType::UnableToExecute);
    if (trustedTypeName == "TrustedHTML"_s) {
        callbackName = "createHTML"_s;
        if (m_createHTMLCallback)
            callbackResult = m_createHTMLCallback->handleEvent(input, WTFMove(arguments));
    } else if (trustedTypeName == "TrustedScript"_s) {
        callbackName = "createScript"_s;
        if (m_createScriptCallback)
            callbackResult = m_createScriptCallback->handleEvent(input, WTFMove(arguments));
    } else if (trustedTypeName == "TrustedScriptURL"_s) {
        callbackName = "createScriptURL"_s;
        if (m_createScriptURLCallback)
            callbackResult = m_createScriptURLCallback->handleEvent(input, WTFMove(arguments));
    } else {
        ASSERT_NOT_REACHED();
        return Exception {
            ExceptionCode::TypeError
        };
    }

    if (callbackResult.type() == CallbackResultType::Success)
        return callbackResult.releaseReturnValue();
    if (callbackResult.type() == CallbackResultType::ExceptionThrown)
        return Exception { ExceptionCode::ExistingExceptionError };

    return Exception {
        ExceptionCode::TypeError,
        makeString("Policy "_s, m_name,
            "'s TrustedTypePolicyOptions did not specify a '"_s, callbackName, "' member."_s)
    };
}

} // namespace WebCore
