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
#include "ScriptWrappableInlines.h"
#include "TrustedType.h"
#include "TrustedTypePolicyOptions.h"
#include "XLinkNames.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(TrustedTypePolicyFactory);

Ref<TrustedTypePolicyFactory> TrustedTypePolicyFactory::create(ScriptExecutionContext& context)
{
    return adoptRef(*new TrustedTypePolicyFactory(context));
}

TrustedTypePolicyFactory::TrustedTypePolicyFactory(ScriptExecutionContext& context)
    : ContextDestructionObserver(&context)
{ }

ExceptionOr<Ref<TrustedTypePolicy>> TrustedTypePolicyFactory::createPolicy(ScriptExecutionContext& context, StringImpl* policyName, const TrustedTypePolicyOptions& options)
{
    CheckedPtr csp = context.contentSecurityPolicy();
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
        if (equal(policyName, "default"_s))
            m_defaultPolicy = policy.ptr();

        m_createdPolicyNames.add(policyName);
        return policy;
    }
}

bool TrustedTypePolicyFactory::isHTML(JSC::JSValue value) const
{
    return dynamicDowncast<JSTrustedHTML>(value);
}

bool TrustedTypePolicyFactory::isScript(JSC::JSValue value) const
{
    return dynamicDowncast<JSTrustedScript>(value);
}

bool TrustedTypePolicyFactory::isScriptURL(JSC::JSValue value) const
{
    return dynamicDowncast<JSTrustedScriptURL>(value);
}

Ref<TrustedHTML> TrustedTypePolicyFactory::emptyHTML() const
{
    return TrustedHTML::create(""_si);
}

Ref<TrustedScript> TrustedTypePolicyFactory::emptyScript() const
{
    return TrustedScript::create(""_si);
}

RefPtr<StringImpl> TrustedTypePolicyFactory::getAttributeType(StringImpl* tagName, StringImpl* attributeParameter, StringImpl* elementNamespace, StringImpl* attributeNamespace) const
{
    return trustedTypeForAttribute(tagName, convertToASCIILowercase(attributeParameter), elementNamespace, attributeNamespace).attributeType;
}

RefPtr<StringImpl> TrustedTypePolicyFactory::getPropertyType(StringImpl* tagName, StringImpl* property, StringImpl* elementNamespace) const
{
    auto localName = convertToASCIILowercase(tagName);
    RefPtr<AtomStringImpl> elementNS = isEmpty(elementNamespace) ? HTMLNames::xhtmlNamespaceURI : AtomStringImpl::add(elementNamespace);

    if (equal(property, "innerHTML"_s) || equal(property, "outerHTML"_s))
        return trustedTypeToString(TrustedType::TrustedHTML);

    const QualifiedName element(nullAtom(), AtomStringImpl::add(localName).get(), elementNS);

    if (element.matches(HTMLNames::iframeTag) && equal(property, "srcdoc"_s))
        return trustedTypeToString(TrustedType::TrustedHTML);
    if (element.matches(HTMLNames::scriptTag) && equal(property, "src"_s))
        return trustedTypeToString(TrustedType::TrustedScriptURL);
    if (element.matches(HTMLNames::scriptTag) && (equal(property, "innerText"_s) || equal(property, "textContent"_s) || equal(property, "text"_s)))
        return trustedTypeToString(TrustedType::TrustedScript);

    return nullString();
}

TrustedTypePolicyFactory::~TrustedTypePolicyFactory() = default;

}
