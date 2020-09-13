/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann (hausmann@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2019 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Ericsson AB. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "HTMLIFrameElement.h"

#include "CSSPropertyNames.h"
#include "DOMTokenList.h"
#include "Frame.h"
#include "HTMLNames.h"
#include "LazyLoadFrameObserver.h"
#include "RenderIFrame.h"
#include "ScriptableDocumentParser.h"
#include "Settings.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLIFrameElement);

using namespace HTMLNames;

inline HTMLIFrameElement::HTMLIFrameElement(const QualifiedName& tagName, Document& document)
    : HTMLFrameElementBase(tagName, document)
{
    ASSERT(hasTagName(iframeTag));
}

Ref<HTMLIFrameElement> HTMLIFrameElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new HTMLIFrameElement(tagName, document));
}

int HTMLIFrameElement::defaultTabIndex() const
{
    return 0;
}

DOMTokenList& HTMLIFrameElement::sandbox()
{
    if (!m_sandbox) {
        m_sandbox = makeUnique<DOMTokenList>(*this, sandboxAttr, [](Document&, StringView token) {
            return SecurityContext::isSupportedSandboxPolicy(token);
        });
    }
    return *m_sandbox;
}

bool HTMLIFrameElement::isPresentationAttribute(const QualifiedName& name) const
{
    if (name == widthAttr || name == heightAttr || name == frameborderAttr)
        return true;
    return HTMLFrameElementBase::isPresentationAttribute(name);
}

void HTMLIFrameElement::collectStyleForPresentationAttribute(const QualifiedName& name, const AtomString& value, MutableStyleProperties& style)
{
    if (name == widthAttr)
        addHTMLLengthToStyle(style, CSSPropertyWidth, value);
    else if (name == heightAttr)
        addHTMLLengthToStyle(style, CSSPropertyHeight, value);
    else if (name == alignAttr)
        applyAlignmentAttributeToStyle(value, style);
    else if (name == frameborderAttr) {
        // Frame border doesn't really match the HTML4 spec definition for iframes. It simply adds
        // a presentational hint that the border should be off if set to zero.
        if (!value.toInt()) {
            // Add a rule that nulls out our border width.
            addPropertyToPresentationAttributeStyle(style, CSSPropertyBorderWidth, 0, CSSUnitType::CSS_PX);
        }
    } else
        HTMLFrameElementBase::collectStyleForPresentationAttribute(name, value, style);
}

void HTMLIFrameElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == sandboxAttr) {
        if (m_sandbox)
            m_sandbox->associatedAttributeValueChanged(value);

        String invalidTokens;
        setSandboxFlags(value.isNull() ? SandboxNone : SecurityContext::parseSandboxPolicy(value, invalidTokens));
        if (!invalidTokens.isNull())
            document().addConsoleMessage(MessageSource::Other, MessageLevel::Error, "Error while parsing the 'sandbox' attribute: " + invalidTokens);
    } else if (name == allowAttr || name == allowfullscreenAttr || name == webkitallowfullscreenAttr) {
        m_featurePolicy = WTF::nullopt;
    } else if (name == loadingAttr) {
        // Allow loading=eager to load the frame immediately if the lazy load was started, but
        // do not allow the reverse situation since the eager load is already started.
        if (m_lazyLoadFrameObserver && !equalLettersIgnoringASCIICase(value, "lazy")) {
            m_lazyLoadFrameObserver->unobserve();
            loadDeferredFrame();
        }
    } else
        HTMLFrameElementBase::parseAttribute(name, value);
}

bool HTMLIFrameElement::rendererIsNeeded(const RenderStyle& style)
{
    return style.display() != DisplayType::None && canLoad();
}

RenderPtr<RenderElement> HTMLIFrameElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return createRenderer<RenderIFrame>(*this, WTFMove(style));
}

void HTMLIFrameElement::setReferrerPolicyForBindings(const AtomString& value)
{
    setAttributeWithoutSynchronization(referrerpolicyAttr, value);
}

String HTMLIFrameElement::referrerPolicyForBindings() const
{
    return referrerPolicyToString(referrerPolicy());
}

ReferrerPolicy HTMLIFrameElement::referrerPolicy() const
{
    if (m_lazyLoadFrameObserver)
        return m_lazyLoadFrameObserver->referrerPolicy();
    if (document().settings().referrerPolicyAttributeEnabled())
        return parseReferrerPolicy(attributeWithoutSynchronization(referrerpolicyAttr), ReferrerPolicySource::ReferrerPolicyAttribute).valueOr(ReferrerPolicy::EmptyString);
    return ReferrerPolicy::EmptyString;
}

const FeaturePolicy& HTMLIFrameElement::featurePolicy() const
{
    if (!m_featurePolicy)
        m_featurePolicy = FeaturePolicy::parse(document(), *this, attributeWithoutSynchronization(allowAttr));
    return *m_featurePolicy;
}

const AtomString& HTMLIFrameElement::loadingForBindings() const
{
    static MainThreadNeverDestroyed<const AtomString> eager("eager", AtomString::ConstructFromLiteral);
    static MainThreadNeverDestroyed<const AtomString> lazy("lazy", AtomString::ConstructFromLiteral);
    return equalLettersIgnoringASCIICase(attributeWithoutSynchronization(HTMLNames::loadingAttr), "lazy") ? lazy : eager;
}

void HTMLIFrameElement::setLoadingForBindings(const AtomString& value)
{
    setAttributeWithoutSynchronization(loadingAttr, value);
}

static bool isFrameLazyLoadable(const URL& completeURL, const AtomString& loadingAttributeValue)
{
    if (!completeURL.protocolIsInHTTPFamily())
        return false;

    return equalLettersIgnoringASCIICase(loadingAttributeValue, "lazy");
}

bool HTMLIFrameElement::shouldLoadFrameLazily()
{
    if (!m_lazyLoadFrameObserver && document().settings().lazyIframeLoadingEnabled()) {
        URL completeURL = document().completeURL(frameURL());
        if (isFrameLazyLoadable(completeURL, attributeWithoutSynchronization(HTMLNames::loadingAttr))) {
            auto currentReferrerPolicy = referrerPolicy();
            lazyLoadFrameObserver().observe(completeURL.string(), currentReferrerPolicy);
            return true;
        }
    }
    return false;
}

bool HTMLIFrameElement::isLazyLoadObserverActive() const
{
    return !!m_lazyLoadFrameObserver;
}

void HTMLIFrameElement::loadDeferredFrame()
{
    AtomString currentURL = frameURL();
    setFrameURL(m_lazyLoadFrameObserver->frameURL());
    openURL();
    setFrameURL(currentURL);
    m_lazyLoadFrameObserver = nullptr;
}

LazyLoadFrameObserver& HTMLIFrameElement::lazyLoadFrameObserver()
{
    if (!m_lazyLoadFrameObserver)
        m_lazyLoadFrameObserver = makeUnique<LazyLoadFrameObserver>(*this);
    return *m_lazyLoadFrameObserver;
}

}
