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
#include "CommonAtomStrings.h"
#include "DOMTokenList.h"
#include "DocumentQuirks.h"
#include "DocumentView.h"
#include "ElementInlines.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "LazyLoadFrameObserver.h"
#include "LocalFrame.h"
#include "NodeName.h"
#include "RenderIFrame.h"
#include "ScriptController.h"
#include "ScriptableDocumentParser.h"
#include "Settings.h"
#include "TrustedType.h"
#include <JavaScriptCore/ConsoleTypes.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(HTMLIFrameElement);

using namespace HTMLNames;

inline HTMLIFrameElement::HTMLIFrameElement(const QualifiedName& tagName, Document& document)
    : HTMLFrameElementBase(tagName, document)
{
    ASSERT(hasTagName(iframeTag));

#if ENABLE(CONTENT_EXTENSIONS)
    if (document.settings().iFrameResourceMonitoringEnabled())
        setInitiatorSourceURL(document.currentSourceURL());
#endif
}

HTMLIFrameElement::~HTMLIFrameElement() = default;

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
        lazyInitialize(m_sandbox, makeUniqueWithoutRefCountedCheck<DOMTokenList>(*this, sandboxAttr, [](Document&, StringView token) {
            return SecurityContext::isSupportedSandboxPolicy(token);
        }));
    }
    return *m_sandbox;
}

bool HTMLIFrameElement::hasPresentationalHintsForAttribute(const QualifiedName& name) const
{
    switch (name.nodeName()) {
    case AttributeNames::widthAttr:
    case AttributeNames::heightAttr:
    case AttributeNames::frameborderAttr:
        return true;
    default:
        break;
    }
    return HTMLFrameElementBase::hasPresentationalHintsForAttribute(name);
}

void HTMLIFrameElement::collectPresentationalHintsForAttribute(const QualifiedName& name, const AtomString& value, MutableStyleProperties& style)
{
    switch (name.nodeName()) {
    case AttributeNames::widthAttr:
        addHTMLLengthToStyle(style, CSSPropertyWidth, value);
        break;
    case AttributeNames::heightAttr:
        addHTMLLengthToStyle(style, CSSPropertyHeight, value);
        break;
    case AttributeNames::alignAttr:
        applyAlignmentAttributeToStyle(value, style);
        break;
    case AttributeNames::frameborderAttr:
        // Frame border doesn't really match the HTML4 spec definition for iframes. It simply adds
        // a presentational hint that the border should be off if set to zero.
        if (!parseHTMLInteger(value).value_or(0)) {
            // Add a rule that nulls out our border width.
            addPropertyToPresentationalHintStyle(style, CSSPropertyBorderWidth, 0, CSSUnitType::CSS_PX);
        }
        break;
    default:
        HTMLFrameElementBase::collectPresentationalHintsForAttribute(name, value, style);
        break;
    }
}

void HTMLIFrameElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    switch (name.nodeName()) {
    case AttributeNames::sandboxAttr: {
        if (m_sandbox)
            m_sandbox->associatedAttributeValueChanged();

        String invalidTokens;
        setSandboxFlags(newValue.isNull() ? SandboxFlags { } : SecurityContext::parseSandboxPolicy(newValue, invalidTokens));
        if (!invalidTokens.isNull())
            protect(document())->addConsoleMessage(MessageSource::Other, MessageLevel::Error, makeString("Error while parsing the 'sandbox' attribute: "_s, invalidTokens));
        break;
    }
    case AttributeNames::allowAttr:
    case AttributeNames::allowfullscreenAttr:
    case AttributeNames::webkitallowfullscreenAttr:
        break;
    case AttributeNames::loadingAttr:
        // Allow loading=eager to load the frame immediately if the lazy load was started, but
        // do not allow the reverse situation since the eager load is already started.
        if (m_lazyLoadFrameObserver && !equalLettersIgnoringASCIICase(newValue, "lazy"_s)) {
            m_lazyLoadFrameObserver->unobserve();
            loadDeferredFrame();
        }
        break;
    case AttributeNames::srcdocAttr:
    case AttributeNames::srcAttr:
        [[fallthrough]];
    default:
        HTMLFrameElementBase::attributeChanged(name, oldValue, newValue, attributeModificationReason);
        break;
    }
}

bool HTMLIFrameElement::rendererIsNeeded(const RenderStyle& style)
{
    return style.display() != DisplayType::None && canLoad();
}

RenderPtr<RenderElement> HTMLIFrameElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return createRenderer<RenderIFrame>(*this, WTF::move(style));
}

String HTMLIFrameElement::referrerPolicyForBindings() const
{
    return referrerPolicyToString(referrerPolicy());
}

ReferrerPolicy HTMLIFrameElement::referrerPolicy() const
{
    if (m_lazyLoadFrameObserver)
        return m_lazyLoadFrameObserver->referrerPolicy();
    return referrerPolicyFromAttribute();
}

String HTMLIFrameElement::srcdoc() const
{
    return attributeWithoutSynchronization(srcdocAttr);
}

ExceptionOr<void> HTMLIFrameElement::setSrcdoc(Variant<RefPtr<TrustedHTML>, String>&& value, SubstituteData::SessionHistoryVisibility sessionHistoryVisibility)
{
    auto stringValueHolder = trustedTypeCompliantString(protect(protect(document())->contextDocument()), WTF::move(value), "HTMLIFrameElement srcdoc"_s);

    if (stringValueHolder.hasException())
        return stringValueHolder.releaseException();

    setAttributeWithoutSynchronization(srcdocAttr, AtomString { stringValueHolder.releaseReturnValue() });
    m_srcdocSessionHistoryVisibility = sessionHistoryVisibility;
    return { };
}

ReferrerPolicy HTMLIFrameElement::referrerPolicyFromAttribute() const
{
    return parseReferrerPolicy(attributeWithoutSynchronization(referrerpolicyAttr), ReferrerPolicySource::ReferrerPolicyAttribute).value_or(ReferrerPolicy::EmptyString);
}

static bool isFrameLazyLoadable(const Document& document, const URL& url, const AtomString& loadingAttributeValue)
{
    if (!url.isValid() || url.isAboutBlank())
        return false;

    if (RefPtr frame = document.frame(); !frame || !frame->checkedScript()->canExecuteScripts(ReasonForCallingCanExecuteScripts::NotAboutToExecuteScript))
        return false;

    return equalLettersIgnoringASCIICase(loadingAttributeValue, "lazy"_s);
}

bool HTMLIFrameElement::shouldLoadFrameLazily()
{
    Ref document = this->document();
    if (!document->settings().lazyIframeLoadingEnabled() || document->quirks().shouldDisableLazyIframeLoadingQuirk())
        return false;
    URL completeURL = document->completeURL(frameURL());
    auto referrerPolicy = referrerPolicyFromAttribute();
    if (!m_lazyLoadFrameObserver) {
        if (isFrameLazyLoadable(document, completeURL, attributeWithoutSynchronization(HTMLNames::loadingAttr))) {
            lazyLoadFrameObserver().observe(AtomString { completeURL.string() }, referrerPolicy);
            return true;
        }
    } else
        m_lazyLoadFrameObserver->update(AtomString { completeURL.string() }, referrerPolicy);
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
    if (isConnected())
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
