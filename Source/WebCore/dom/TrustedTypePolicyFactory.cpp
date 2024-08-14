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

#include "ContentSecurityPolicy.h"
#include "ContextDestructionObserver.h"
#include "HTMLNames.h"
#include "JSDOMConvertObject.h"
#include "JSTrustedHTML.h"
#include "JSTrustedScript.h"
#include "JSTrustedScriptURL.h"
#include "SVGNames.h"
#include "ScriptExecutionContext.h"
#include "TrustedType.h"
#include "TrustedTypePolicyOptions.h"
#include "XLinkNames.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(TrustedTypePolicyFactory);

Ref<TrustedTypePolicyFactory> TrustedTypePolicyFactory::create(ScriptExecutionContext& context)
{
    return adoptRef(*new TrustedTypePolicyFactory(context));
}

TrustedTypePolicyFactory::TrustedTypePolicyFactory(ScriptExecutionContext& context)
    : ContextDestructionObserver(&context)
{ }

ExceptionOr<Ref<TrustedTypePolicy>> TrustedTypePolicyFactory::createPolicy(ScriptExecutionContext& context, const String& policyName, const TrustedTypePolicyOptions& options)
{
    auto csp = context.checkedContentSecurityPolicy();
    ASSERT(csp);

    AllowTrustedTypePolicy policyAllowed = csp->allowTrustedTypesPolicy(policyName, m_createdPolicyNames.contains(policyName));

    switch (policyAllowed) {
    case AllowTrustedTypePolicy::DisallowedName:
        return Exception {
            ExceptionCode::TypeError,
            makeString("Failed to execute 'createPolicy': Policy with name '"_s, policyName, "' disallowed."_s)
        };
    case AllowTrustedTypePolicy::DisallowedDuplicateName:
        return Exception {
            ExceptionCode::TypeError,
            makeString("Failed to execute 'createPolicy': Policy with name '"_s, policyName, "' already exists."_s)
        };
    default:
        auto policy = TrustedTypePolicy::create(policyName, options);
        if (policyName == "default"_s)
            m_defaultPolicy = policy.ptr();

        m_createdPolicyNames.add(policyName);
        return policy;
    }
}

bool TrustedTypePolicyFactory::isHTML(JSC::JSValue value) const
{
    return JSC::jsDynamicCast<JSTrustedHTML*>(value);
}

bool TrustedTypePolicyFactory::isScript(JSC::JSValue value) const
{
    return JSC::jsDynamicCast<JSTrustedScript*>(value);
}

bool TrustedTypePolicyFactory::isScriptURL(JSC::JSValue value) const
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

String TrustedTypePolicyFactory::getAttributeType(const String& tagName, const String& attributeParameter, const String& elementNamespace, const String& attributeNamespace) const
{
    return trustedTypeForAttribute(tagName, attributeParameter.convertToASCIILowercase(), elementNamespace, attributeNamespace).attributeType;
}

String TrustedTypePolicyFactory::getPropertyType(const String& tagName, const String& property, const String& elementNamespace) const
{
    auto localName = tagName.convertToASCIILowercase();
    AtomString elementNS = elementNamespace.isEmpty() ? HTMLNames::xhtmlNamespaceURI : AtomString(elementNamespace);

    if (property == "innerHTML"_s || property == "outerHTML"_s)
        return trustedTypeToString(TrustedType::TrustedHTML);

    const QualifiedName element(nullAtom(), AtomString(localName), elementNS);

    if (element.matches(HTMLNames::iframeTag) && property == "srcdoc"_s)
        return trustedTypeToString(TrustedType::TrustedHTML);
    if (element.matches(HTMLNames::scriptTag) && property == "src"_s)
        return trustedTypeToString(TrustedType::TrustedScriptURL);
    if (element.matches(HTMLNames::scriptTag) && (property == "innerText"_s || property == "textContent"_s || property == "text"_s))
        return trustedTypeToString(TrustedType::TrustedScript);

    return nullString();
}

}
