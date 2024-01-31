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
    if (m_createHTMLCallback) {
        auto callbackResult = m_createHTMLCallback->handleEvent(input, WTFMove(arguments));

        if (callbackResult.type() == CallbackResultType::Success) {
            auto contents = callbackResult.releaseReturnValue();

            return TrustedHTML::create(contents);
        }
        if (callbackResult.type() == CallbackResultType::ExceptionThrown)
            return Exception { ExceptionCode::ExistingExceptionError };
    }

    return Exception {
        ExceptionCode::TypeError,
        makeString("Policy "_s, m_name,
        "'s TrustedTypePolicyOptions did not specify a 'createHTML' member."_s)
    };
}

ExceptionOr<Ref<TrustedScript>> TrustedTypePolicy::createScript(const String& input, FixedVector<JSC::Strong<JSC::Unknown>>&& arguments)
{
    if (m_createScriptCallback) {
        auto callbackResult = m_createScriptCallback->handleEvent(input, WTFMove(arguments));

        if (callbackResult.type() == CallbackResultType::Success) {
            auto contents = callbackResult.releaseReturnValue();

            return TrustedScript::create(contents);
        }
        if (callbackResult.type() == CallbackResultType::ExceptionThrown)
            return Exception { ExceptionCode::ExistingExceptionError };
    }

    return Exception {
        ExceptionCode::TypeError,
        makeString("Policy "_s, m_name,
        "'s TrustedTypePolicyOptions did not specify a 'createScript' member."_s)
    };
}

ExceptionOr<Ref<TrustedScriptURL>> TrustedTypePolicy::createScriptURL(const String& input, FixedVector<JSC::Strong<JSC::Unknown>>&& arguments)
{
    if (m_createScriptURLCallback) {
        auto callbackResult = m_createScriptURLCallback->handleEvent(input, WTFMove(arguments));

        if (callbackResult.type() == CallbackResultType::Success) {
            auto contents = callbackResult.releaseReturnValue();

            return TrustedScriptURL::create(contents);
        }
        if (callbackResult.type() == CallbackResultType::ExceptionThrown)
            return Exception { ExceptionCode::ExistingExceptionError };
    }

    return Exception {
        ExceptionCode::TypeError,
        makeString("Policy "_s, m_name,
        "'s TrustedTypePolicyOptions did not specify a 'createScriptURL' member."_s)
    };
}

} // namespace WebCore
