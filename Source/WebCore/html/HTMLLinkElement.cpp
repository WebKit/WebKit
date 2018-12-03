/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Rob Buis (rwlbuis@gmail.com)
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
#include "HTMLLinkElement.h"

#include "Attribute.h"
#include "CachedCSSStyleSheet.h"
#include "CachedResource.h"
#include "CachedResourceLoader.h"
#include "CachedResourceRequest.h"
#include "ContentSecurityPolicy.h"
#include "CrossOriginAccessControl.h"
#include "DOMTokenList.h"
#include "Document.h"
#include "Event.h"
#include "EventNames.h"
#include "EventSender.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "HTMLAnchorElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "Logging.h"
#include "MediaList.h"
#include "MediaQueryEvaluator.h"
#include "MediaQueryParser.h"
#include "MouseEvent.h"
#include "RenderStyle.h"
#include "RuntimeEnabledFeatures.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "StyleInheritedData.h"
#include "StyleResolveForDocument.h"
#include "StyleScope.h"
#include "StyleSheetContents.h"
#include "SubresourceIntegrity.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/Ref.h>
#include <wtf/SetForScope.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLLinkElement);

using namespace HTMLNames;

static LinkEventSender& linkLoadEventSender()
{
    static NeverDestroyed<LinkEventSender> sharedLoadEventSender(eventNames().loadEvent);
    return sharedLoadEventSender;
}

static LinkEventSender& linkErrorEventSender()
{
    static NeverDestroyed<LinkEventSender> sharedErrorEventSender(eventNames().errorEvent);
    return sharedErrorEventSender;
}

inline HTMLLinkElement::HTMLLinkElement(const QualifiedName& tagName, Document& document, bool createdByParser)
    : HTMLElement(tagName, document)
    , m_linkLoader(*this)
    , m_disabledState(Unset)
    , m_loading(false)
    , m_createdByParser(createdByParser)
    , m_firedLoad(false)
    , m_loadedResource(false)
    , m_pendingSheetType(Unknown)
{
    ASSERT(hasTagName(linkTag));
}

Ref<HTMLLinkElement> HTMLLinkElement::create(const QualifiedName& tagName, Document& document, bool createdByParser)
{
    return adoptRef(*new HTMLLinkElement(tagName, document, createdByParser));
}

HTMLLinkElement::~HTMLLinkElement()
{
    if (m_sheet)
        m_sheet->clearOwnerNode();

    if (m_cachedSheet)
        m_cachedSheet->removeClient(*this);

    if (m_styleScope)
        m_styleScope->removeStyleSheetCandidateNode(*this);

    linkLoadEventSender().cancelEvent(*this);
    linkErrorEventSender().cancelEvent(*this);
}

void HTMLLinkElement::setDisabledState(bool disabled)
{
    DisabledState oldDisabledState = m_disabledState;
    m_disabledState = disabled ? Disabled : EnabledViaScript;
    if (oldDisabledState == m_disabledState)
        return;

    ASSERT(isConnected() || !styleSheetIsLoading());
    if (!isConnected())
        return;

    // If we change the disabled state while the sheet is still loading, then we have to
    // perform three checks:
    if (styleSheetIsLoading()) {
        // Check #1: The sheet becomes disabled while loading.
        if (m_disabledState == Disabled)
            removePendingSheet();

        // Check #2: An alternate sheet becomes enabled while it is still loading.
        if (m_relAttribute.isAlternate && m_disabledState == EnabledViaScript)
            addPendingSheet(ActiveSheet);

        // Check #3: A main sheet becomes enabled while it was still loading and
        // after it was disabled via script. It takes really terrible code to make this
        // happen (a double toggle for no reason essentially). This happens on
        // virtualplastic.net, which manages to do about 12 enable/disables on only 3
        // sheets. :)
        if (!m_relAttribute.isAlternate && m_disabledState == EnabledViaScript && oldDisabledState == Disabled)
            addPendingSheet(ActiveSheet);

        // If the sheet is already loading just bail.
        return;
    }

    // Load the sheet, since it's never been loaded before.
    if (!m_sheet && m_disabledState == EnabledViaScript)
        process();
    else {
        ASSERT(m_styleScope);
        m_styleScope->didChangeActiveStyleSheetCandidates();
    }
}

void HTMLLinkElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == relAttr) {
        m_relAttribute = LinkRelAttribute(document(), value);
        if (m_relList)
            m_relList->associatedAttributeValueChanged(value);
        process();
        return;
    }
    if (name == hrefAttr) {
        bool wasLink = isLink();
        setIsLink(!value.isNull() && !shouldProhibitLinks(this));
        if (wasLink != isLink())
            invalidateStyleForSubtree();
        process();
        return;
    }
    if (name == typeAttr) {
        m_type = value;
        process();
        return;
    }
    if (name == sizesAttr) {
        if (m_sizes)
            m_sizes->associatedAttributeValueChanged(value);
        process();
        return;
    }
    if (name == mediaAttr) {
        m_media = value.string().convertToASCIILowercase();
        process();
        if (m_sheet && !isDisabled())
            m_styleScope->didChangeActiveStyleSheetCandidates();
        return;
    }
    if (name == disabledAttr) {
        setDisabledState(!value.isNull());
        return;
    }
    if (name == titleAttr) {
        if (m_sheet && !isInShadowTree())
            m_sheet->setTitle(value);
        return;
    }
    HTMLElement::parseAttribute(name, value);
}

bool HTMLLinkElement::shouldLoadLink()
{
    Ref<Document> originalDocument = document();
    if (!dispatchBeforeLoadEvent(getNonEmptyURLAttribute(hrefAttr)))
        return false;
    // A beforeload handler might have removed us from the document or changed the document.
    if (!isConnected() || &document() != originalDocument.ptr())
        return false;
    return true;
}

void HTMLLinkElement::setCrossOrigin(const AtomicString& value)
{
    setAttributeWithoutSynchronization(crossoriginAttr, value);
}

String HTMLLinkElement::crossOrigin() const
{
    return parseCORSSettingsAttribute(attributeWithoutSynchronization(crossoriginAttr));
}

void HTMLLinkElement::setAs(const AtomicString& value)
{
    setAttributeWithoutSynchronization(asAttr, value);
}

String HTMLLinkElement::as() const
{
    String as = attributeWithoutSynchronization(asAttr);
    if (equalLettersIgnoringASCIICase(as, "fetch")
        || equalLettersIgnoringASCIICase(as, "image")
        || equalLettersIgnoringASCIICase(as, "script")
        || equalLettersIgnoringASCIICase(as, "style")
        || (RuntimeEnabledFeatures::sharedFeatures().mediaPreloadingEnabled()
            && (equalLettersIgnoringASCIICase(as, "video")
                || equalLettersIgnoringASCIICase(as, "audio")))
#if ENABLE(VIDEO_TRACK)
        || equalLettersIgnoringASCIICase(as, "track")
#endif
        || equalLettersIgnoringASCIICase(as, "font"))
        return as;
    return String();
}

void HTMLLinkElement::process()
{
    if (!isConnected()) {
        ASSERT(!m_sheet);
        return;
    }

    // Prevent recursive loading of link.
    if (m_isHandlingBeforeLoad)
        return;

    URL url = getNonEmptyURLAttribute(hrefAttr);

    if (!m_linkLoader.loadLink(m_relAttribute, url, attributeWithoutSynchronization(asAttr), attributeWithoutSynchronization(mediaAttr), attributeWithoutSynchronization(typeAttr), attributeWithoutSynchronization(crossoriginAttr), document()))
        return;

    bool treatAsStyleSheet = m_relAttribute.isStyleSheet
        || (document().settings().treatsAnyTextCSSLinkAsStylesheet() && m_type.containsIgnoringASCIICase("text/css"));

    if (m_disabledState != Disabled && treatAsStyleSheet && document().frame() && url.isValid()) {
        String charset = attributeWithoutSynchronization(charsetAttr);
        if (charset.isEmpty())
            charset = document().charset();

        if (m_cachedSheet) {
            removePendingSheet();
            m_cachedSheet->removeClient(*this);
            m_cachedSheet = nullptr;
        }

        {
        SetForScope<bool> change(m_isHandlingBeforeLoad, true);
        if (!shouldLoadLink())
            return;
        }

        m_loading = true;

        bool mediaQueryMatches = true;
        if (!m_media.isEmpty()) {
            std::optional<RenderStyle> documentStyle;
            if (document().hasLivingRenderTree())
                documentStyle = Style::resolveForDocument(document());
            auto media = MediaQuerySet::create(m_media, MediaQueryParserContext(document()));
            LOG(MediaQueries, "HTMLLinkElement::process evaluating queries");
            mediaQueryMatches = MediaQueryEvaluator { document().frame()->view()->mediaType(), document(), documentStyle ? &*documentStyle : nullptr }.evaluate(media.get());
        }

        // Don't hold up render tree construction and script execution on stylesheets
        // that are not needed for the rendering at the moment.
        bool isActive = mediaQueryMatches && !isAlternate();
        addPendingSheet(isActive ? ActiveSheet : InactiveSheet);

        // Load stylesheets that are not needed for the rendering immediately with low priority.
        std::optional<ResourceLoadPriority> priority;
        if (!isActive)
            priority = ResourceLoadPriority::VeryLow;

        if (document().settings().subresourceIntegrityEnabled())
            m_integrityMetadataForPendingSheetRequest = attributeWithoutSynchronization(HTMLNames::integrityAttr);

        ResourceLoaderOptions options = CachedResourceLoader::defaultCachedResourceOptions();
        options.sameOriginDataURLFlag = SameOriginDataURLFlag::Set;
        if (document().contentSecurityPolicy()->allowStyleWithNonce(attributeWithoutSynchronization(HTMLNames::nonceAttr)))
            options.contentSecurityPolicyImposition = ContentSecurityPolicyImposition::SkipPolicyCheck;
        options.integrity = m_integrityMetadataForPendingSheetRequest;

        auto request = createPotentialAccessControlRequest(WTFMove(url), document(), crossOrigin(), WTFMove(options));
        request.setPriority(WTFMove(priority));
        request.setCharset(WTFMove(charset));
        request.setInitiator(*this);

        ASSERT_WITH_SECURITY_IMPLICATION(!m_cachedSheet);
        m_cachedSheet = document().cachedResourceLoader().requestCSSStyleSheet(WTFMove(request)).value_or(nullptr);

        if (m_cachedSheet)
            m_cachedSheet->addClient(*this);
        else {
            // The request may have been denied if (for example) the stylesheet is local and the document is remote.
            m_loading = false;
            sheetLoaded();
            notifyLoadedSheetAndAllCriticalSubresources(false);
        }
    } else if (m_sheet) {
        // we no longer contain a stylesheet, e.g. perhaps rel or type was changed
        clearSheet();
        m_styleScope->didChangeActiveStyleSheetCandidates();
    }
}

void HTMLLinkElement::clearSheet()
{
    ASSERT(m_sheet);
    ASSERT(m_sheet->ownerNode() == this);
    m_sheet->clearOwnerNode();
    m_sheet = nullptr;
}

Node::InsertedIntoAncestorResult HTMLLinkElement::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    HTMLElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);
    if (!insertionType.connectedToDocument)
        return InsertedIntoAncestorResult::Done;

    m_styleScope = &Style::Scope::forNode(*this);
    m_styleScope->addStyleSheetCandidateNode(*this, m_createdByParser);

    return InsertedIntoAncestorResult::NeedsPostInsertionCallback;
}

void HTMLLinkElement::didFinishInsertingNode()
{
    process();
}

void HTMLLinkElement::removedFromAncestor(RemovalType removalType, ContainerNode& oldParentOfRemovedTree)
{
    HTMLElement::removedFromAncestor(removalType, oldParentOfRemovedTree);
    if (!removalType.disconnectedFromDocument)
        return;

    m_linkLoader.cancelLoad();

    bool wasLoading = styleSheetIsLoading();

    if (m_sheet)
        clearSheet();

    if (wasLoading)
        removePendingSheet();
    
    if (m_styleScope) {
        m_styleScope->removeStyleSheetCandidateNode(*this);
        m_styleScope = nullptr;
    }
}

void HTMLLinkElement::finishParsingChildren()
{
    m_createdByParser = false;
    HTMLElement::finishParsingChildren();
}

void HTMLLinkElement::initializeStyleSheet(Ref<StyleSheetContents>&& styleSheet, const CachedCSSStyleSheet& cachedStyleSheet, MediaQueryParserContext context)
{
    // FIXME: originClean should be turned to false except if fetch mode is CORS.
    std::optional<bool> originClean;
    if (cachedStyleSheet.options().mode == FetchOptions::Mode::Cors)
        originClean = cachedStyleSheet.isCORSSameOrigin();

    m_sheet = CSSStyleSheet::create(WTFMove(styleSheet), *this, originClean);
    m_sheet->setMediaQueries(MediaQuerySet::create(m_media, context));
    if (!isInShadowTree())
        m_sheet->setTitle(title());

    if (!m_sheet->canAccessRules())
        m_sheet->contents().setAsOpaque();
}

void HTMLLinkElement::setCSSStyleSheet(const String& href, const URL& baseURL, const String& charset, const CachedCSSStyleSheet* cachedStyleSheet)
{
    if (!isConnected()) {
        ASSERT(!m_sheet);
        return;
    }
    auto frame = makeRefPtr(document().frame());
    if (!frame)
        return;

    // Completing the sheet load may cause scripts to execute.
    Ref<HTMLLinkElement> protectedThis(*this);

    if (!cachedStyleSheet->errorOccurred() && !matchIntegrityMetadata(*cachedStyleSheet, m_integrityMetadataForPendingSheetRequest)) {
        document().addConsoleMessage(MessageSource::Security, MessageLevel::Error, makeString("Cannot load stylesheet ", cachedStyleSheet->url().stringCenterEllipsizedToLength(), ". Failed integrity metadata check."));

        m_loading = false;
        sheetLoaded();
        notifyLoadedSheetAndAllCriticalSubresources(true);
        return;
    }

    CSSParserContext parserContext(document(), baseURL, charset);
    auto cachePolicy = frame->loader().subresourceCachePolicy(baseURL);

    if (auto restoredSheet = const_cast<CachedCSSStyleSheet*>(cachedStyleSheet)->restoreParsedStyleSheet(parserContext, cachePolicy, frame->loader())) {
        ASSERT(restoredSheet->isCacheable());
        ASSERT(!restoredSheet->isLoading());
        initializeStyleSheet(restoredSheet.releaseNonNull(), *cachedStyleSheet, MediaQueryParserContext(document()));

        m_loading = false;
        sheetLoaded();
        notifyLoadedSheetAndAllCriticalSubresources(false);
        return;
    }

    auto styleSheet = StyleSheetContents::create(href, parserContext);
    initializeStyleSheet(styleSheet.copyRef(), *cachedStyleSheet, MediaQueryParserContext(document()));

    // FIXME: Set the visibility option based on m_sheet being clean or not.
    // Best approach might be to set it on the style sheet content itself or its context parser otherwise.
    styleSheet.get().parseAuthorStyleSheet(cachedStyleSheet, &document().securityOrigin());

    m_loading = false;
    styleSheet.get().notifyLoadedSheet(cachedStyleSheet);
    styleSheet.get().checkLoaded();

    if (styleSheet.get().isCacheable())
        const_cast<CachedCSSStyleSheet*>(cachedStyleSheet)->saveParsedStyleSheet(WTFMove(styleSheet));
}

bool HTMLLinkElement::styleSheetIsLoading() const
{
    if (m_loading)
        return true;
    if (!m_sheet)
        return false;
    return m_sheet->contents().isLoading();
}

DOMTokenList& HTMLLinkElement::sizes()
{
    if (!m_sizes)
        m_sizes = std::make_unique<DOMTokenList>(*this, sizesAttr);
    return *m_sizes;
}

void HTMLLinkElement::linkLoaded()
{
    m_loadedResource = true;
    linkLoadEventSender().dispatchEventSoon(*this);
}

void HTMLLinkElement::linkLoadingErrored()
{
    linkErrorEventSender().dispatchEventSoon(*this);
}

bool HTMLLinkElement::sheetLoaded()
{
    if (!styleSheetIsLoading()) {
        removePendingSheet();
        return true;
    }
    return false;
}

void HTMLLinkElement::dispatchPendingLoadEvents()
{
    linkLoadEventSender().dispatchPendingEvents();
}

void HTMLLinkElement::dispatchPendingEvent(LinkEventSender* eventSender)
{
    ASSERT_UNUSED(eventSender, eventSender == &linkLoadEventSender() || eventSender == &linkErrorEventSender());
    if (m_loadedResource)
        dispatchEvent(Event::create(eventNames().loadEvent, Event::CanBubble::No, Event::IsCancelable::No));
    else
        dispatchEvent(Event::create(eventNames().errorEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

DOMTokenList& HTMLLinkElement::relList()
{
    if (!m_relList) 
        m_relList = std::make_unique<DOMTokenList>(*this, HTMLNames::relAttr, [](Document& document, StringView token) {
            return LinkRelAttribute::isSupported(document, token);
        });
    return *m_relList;
}

void HTMLLinkElement::notifyLoadedSheetAndAllCriticalSubresources(bool errorOccurred)
{
    if (m_firedLoad)
        return;
    m_loadedResource = !errorOccurred;
    linkLoadEventSender().dispatchEventSoon(*this);
    m_firedLoad = true;
}

void HTMLLinkElement::startLoadingDynamicSheet()
{
    // We don't support multiple active sheets.
    ASSERT(m_pendingSheetType < ActiveSheet);
    addPendingSheet(ActiveSheet);
}

bool HTMLLinkElement::isURLAttribute(const Attribute& attribute) const
{
    return attribute.name().localName() == hrefAttr || HTMLElement::isURLAttribute(attribute);
}

void HTMLLinkElement::defaultEventHandler(Event& event)
{
    if (MouseEvent::canTriggerActivationBehavior(event)) {
        handleClick(event);
        return;
    }
    HTMLElement::defaultEventHandler(event);
}

void HTMLLinkElement::handleClick(Event& event)
{
    event.setDefaultHandled();
    URL url = href();
    if (url.isNull())
        return;
    RefPtr<Frame> frame = document().frame();
    if (!frame)
        return;
    frame->loader().urlSelected(url, target(), &event, LockHistory::No, LockBackForwardList::No, MaybeSendReferrer, document().shouldOpenExternalURLsPolicyToPropagate());
}

URL HTMLLinkElement::href() const
{
    return document().completeURL(attributeWithoutSynchronization(hrefAttr));
}

const AtomicString& HTMLLinkElement::rel() const
{
    return attributeWithoutSynchronization(relAttr);
}

String HTMLLinkElement::target() const
{
    return attributeWithoutSynchronization(targetAttr);
}

const AtomicString& HTMLLinkElement::type() const
{
    return attributeWithoutSynchronization(typeAttr);
}

std::optional<LinkIconType> HTMLLinkElement::iconType() const
{
    return m_relAttribute.iconType;
}

void HTMLLinkElement::addSubresourceAttributeURLs(ListHashSet<URL>& urls) const
{
    HTMLElement::addSubresourceAttributeURLs(urls);

    // Favicons are handled by a special case in LegacyWebArchive::create()
    if (m_relAttribute.iconType)
        return;

    if (!m_relAttribute.isStyleSheet)
        return;
    
    // Append the URL of this link element.
    addSubresourceURL(urls, href());

    if (auto styleSheet = makeRefPtr(this->sheet())) {
        styleSheet->contents().traverseSubresources([&] (auto& resource) {
            urls.add(resource.url());
            return false;
        });
    }
}

void HTMLLinkElement::addPendingSheet(PendingSheetType type)
{
    if (type <= m_pendingSheetType)
        return;
    m_pendingSheetType = type;

    if (m_pendingSheetType == InactiveSheet)
        return;
    ASSERT(m_styleScope);
    m_styleScope->addPendingSheet(*this);
}

void HTMLLinkElement::removePendingSheet()
{
    PendingSheetType type = m_pendingSheetType;
    m_pendingSheetType = Unknown;

    if (type == Unknown)
        return;

    ASSERT(m_styleScope);
    if (type == InactiveSheet) {
        // Document just needs to know about the sheet for exposure through document.styleSheets
        m_styleScope->didChangeActiveStyleSheetCandidates();
        return;
    }

    m_styleScope->removePendingSheet(*this);
}

} // namespace WebCore
