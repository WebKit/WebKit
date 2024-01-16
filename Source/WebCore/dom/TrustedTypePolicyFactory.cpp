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
#include "TrustedTypePolicyFactory.h"

#include "JSDOMConvertObject.h"
#include "JSTrustedHTML.h"
#include "JSTrustedScript.h"
#include "JSTrustedScriptURL.h"
#include "TrustedHTML.h"
#include "TrustedScript.h"
#include "TrustedScriptURL.h"
#include "TrustedTypePolicyOptions.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(TrustedTypePolicyFactory);

// static
Ref<TrustedTypePolicyFactory> TrustedTypePolicyFactory::create()
{
    return adoptRef(*new TrustedTypePolicyFactory());
}

Ref<TrustedTypePolicy> TrustedTypePolicyFactory::createPolicy(const String& policyName, const TrustedTypePolicyOptions& options)
{
    auto policy = TrustedTypePolicy::create(policyName, options);
    if (policyName == "default"_s)
        m_defaultPolicy = policy.ptr();

    m_createdPolicyNames.add(policyName);
    return policy;
}

bool TrustedTypePolicyFactory::isHTML(JSC::JSValue value)
{
    return JSC::jsDynamicCast<JSTrustedHTML*>(value);
}

bool TrustedTypePolicyFactory::isScript(JSC::JSValue value)
{
    return JSC::jsDynamicCast<JSTrustedScript*>(value);
}

bool TrustedTypePolicyFactory::isScriptURL(JSC::JSValue value)
{
    return JSC::jsDynamicCast<JSTrustedScriptURL*>(value);
}

Ref<TrustedHTML> TrustedTypePolicyFactory::emptyHTML() const
{
    return TrustedHTML::create(""_s);
}

Ref<TrustedScript> TrustedTypePolicyFactory::emptyScript() const
{
    return TrustedScript::create(""_s);
}

String TrustedTypePolicyFactory::getAttributeType(const String& tagName, const String& attribute, const String& elementNs, const String& attrNs)
{
    UNUSED_PARAM(tagName);
    UNUSED_PARAM(attribute);
    UNUSED_PARAM(elementNs);
    UNUSED_PARAM(attrNs);
    return String();
}

String TrustedTypePolicyFactory::getPropertyType(const String& tagName, const String& attribute, const String& elementNs)
{
    UNUSED_PARAM(tagName);
    UNUSED_PARAM(attribute);
    UNUSED_PARAM(elementNs);
    return String();
}

}
