/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2006, 2007, 2008, 2009, 2010, 2014 Apple Inc. All rights reserved.
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
#include "AuthorStyleSheets.h"
#include "CachedCSSStyleSheet.h"
#include "CachedResource.h"
#include "CachedResourceLoader.h"
#include "CachedResourceRequest.h"
#include "ContentSecurityPolicy.h"
#include "DOMTokenList.h"
#include "Document.h"
#include "Event.h"
#include "EventSender.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "HTMLAnchorElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "MediaList.h"
#include "MediaQueryEvaluator.h"
#include "MouseEvent.h"
#include "Page.h"
#include "RenderStyle.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "StyleInheritedData.h"
#include "StyleResolveForDocument.h"
#include "StyleSheetContents.h"
#include <wtf/Ref.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

using namespace HTMLNames;

static LinkEventSender& linkLoadEventSender()
{
    static NeverDestroyed<LinkEventSender> sharedLoadEventSender(eventNames().loadEvent);
    return sharedLoadEventSender;
}

inline HTMLLinkElement::HTMLLinkElement(const QualifiedName& tagName, Document& document, bool createdByParser)
    : HTMLElement(tagName, document)
    , m_linkLoader(*this)
    , m_disabledState(Unset)
    , m_loading(false)
    , m_createdByParser(createdByParser)
    , m_isInShadowTree(false)
    , m_firedLoad(false)
    , m_loadedSheet(false)
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
        m_cachedSheet->removeClient(this);

    if (inDocument())
        document().authorStyleSheets().removeStyleSheetCandidateNode(*this);

    linkLoadEventSender().cancelEvent(*this);
}

void HTMLLinkElement::setDisabledState(bool disabled)
{
    DisabledState oldDisabledState = m_disabledState;
    m_disabledState = disabled ? Disabled : EnabledViaScript;
    if (oldDisabledState != m_disabledState) {
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
        else
            document().styleResolverChanged(DeferRecalcStyle); // Update the style selector.
    }
}

void HTMLLinkElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == relAttr) {
        m_relAttribute = LinkRelAttribute(value);
        if (m_relList)
            m_relList->associatedAttributeValueChanged(value);
        process();
        return;
    }
    if (name == hrefAttr) {
        bool wasLink = isLink();
        setIsLink(!value.isNull() && !shouldProhibitLinks(this));
        if (wasLink != isLink())
            setNeedsStyleRecalc();
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
            document().styleResolverChanged(DeferRecalcStyle);
        return;
    }
    if (name == disabledAttr) {
        setDisabledState(!value.isNull());
        return;
    }
    if (name == titleAttr) {
        if (m_sheet)
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
    if (!inDocument() || &document() != originalDocument.ptr())
        return false;
    return true;
}

void HTMLLinkElement::process()
{
    if (!inDocument() || m_isInShadowTree) {
        ASSERT(!m_sheet);
        return;
    }

    URL url = getNonEmptyURLAttribute(hrefAttr);

    if (!m_linkLoader.loadLink(m_relAttribute, url, document()))
        return;

    bool treatAsStyleSheet = m_relAttribute.isStyleSheet
        || (document().settings() && document().settings()->treatsAnyTextCSSLinkAsStylesheet() && m_type.containsIgnoringASCIICase("text/css"));

    if (m_disabledState != Disabled && treatAsStyleSheet && document().frame() && url.isValid()) {
        AtomicString charset = fastGetAttribute(charsetAttr);
        if (charset.isEmpty() && document().frame())
            charset = document().charset();
        
        if (m_cachedSheet) {
            removePendingSheet();
            m_cachedSheet->removeClient(this);
            m_cachedSheet = 0;
        }

        if (!shouldLoadLink())
            return;

        m_loading = true;

        bool mediaQueryMatches = true;
        if (!m_media.isEmpty()) {
            RefPtr<RenderStyle> documentStyle;
            if (document().hasLivingRenderTree())
                documentStyle = Style::resolveForDocument(document());
            RefPtr<MediaQuerySet> media = MediaQuerySet::createAllowingDescriptionSyntax(m_media);
            MediaQueryEvaluator evaluator(document().frame()->view()->mediaType(), document().frame(), documentStyle.get());
            mediaQueryMatches = evaluator.eval(media.get());
        }

        // Don't hold up render tree construction and script execution on stylesheets
        // that are not needed for the rendering at the moment.
        bool isActive = mediaQueryMatches && !isAlternate();
        addPendingSheet(isActive ? ActiveSheet : InactiveSheet);

        // Load stylesheets that are not needed for the rendering immediately with low priority.
        Optional<ResourceLoadPriority> priority;
        if (!isActive)
            priority = ResourceLoadPriority::VeryLow;
        CachedResourceRequest request(ResourceRequest(document().completeURL(url)), charset, priority);
        request.setInitiator(this);

        if (document().contentSecurityPolicy()->allowStyleWithNonce(fastGetAttribute(HTMLNames::nonceAttr))) {
            ResourceLoaderOptions options = CachedResourceLoader::defaultCachedResourceOptions();
            options.setContentSecurityPolicyImposition(ContentSecurityPolicyImposition::SkipPolicyCheck);
            request.setOptions(options);
        }

        m_cachedSheet = document().cachedResourceLoader().requestCSSStyleSheet(request);
        
        if (m_cachedSheet)
            m_cachedSheet->addClient(this);
        else {
            // The request may have been denied if (for example) the stylesheet is local and the document is remote.
            m_loading = false;
            removePendingSheet();
        }
    } else if (m_sheet) {
        // we no longer contain a stylesheet, e.g. perhaps rel or type was changed
        clearSheet();
        document().styleResolverChanged(DeferRecalcStyle);
    }
}

void HTMLLinkElement::clearSheet()
{
    ASSERT(m_sheet);
    ASSERT(m_sheet->ownerNode() == this);
    m_sheet->clearOwnerNode();
    m_sheet = nullptr;
}

Node::InsertionNotificationRequest HTMLLinkElement::insertedInto(ContainerNode& insertionPoint)
{
    HTMLElement::insertedInto(insertionPoint);
    if (!insertionPoint.inDocument())
        return InsertionDone;

    m_isInShadowTree = isInShadowTree();
    if (m_isInShadowTree)
        return InsertionDone;

    document().authorStyleSheets().addStyleSheetCandidateNode(*this, m_createdByParser);

    process();
    return InsertionDone;
}

void HTMLLinkElement::removedFrom(ContainerNode& insertionPoint)
{
    HTMLElement::removedFrom(insertionPoint);
    if (!insertionPoint.inDocument())
        return;

    if (m_isInShadowTree) {
        ASSERT(!m_sheet);
        return;
    }
    document().authorStyleSheets().removeStyleSheetCandidateNode(*this);

    if (m_sheet)
        clearSheet();

    if (styleSheetIsLoading())
        removePendingSheet(RemovePendingSheetNotifyLater);

    if (document().hasLivingRenderTree())
        document().styleResolverChanged(DeferRecalcStyleIfNeeded);
}

void HTMLLinkElement::finishParsingChildren()
{
    m_createdByParser = false;
    HTMLElement::finishParsingChildren();
}

void HTMLLinkElement::setCSSStyleSheet(const String& href, const URL& baseURL, const String& charset, const CachedCSSStyleSheet* cachedStyleSheet)
{
    if (!inDocument()) {
        ASSERT(!m_sheet);
        return;
    }
    auto* frame = document().frame();
    if (!frame)
        return;

    // Completing the sheet load may cause scripts to execute.
    Ref<HTMLLinkElement> protect(*this);

    CSSParserContext parserContext(document(), baseURL, charset);
    auto cachePolicy = frame->loader().subresourceCachePolicy();

    if (RefPtr<StyleSheetContents> restoredSheet = const_cast<CachedCSSStyleSheet*>(cachedStyleSheet)->restoreParsedStyleSheet(parserContext, cachePolicy)) {
        ASSERT(restoredSheet->isCacheable());
        ASSERT(!restoredSheet->isLoading());

        m_sheet = CSSStyleSheet::create(restoredSheet.releaseNonNull(), this);
        m_sheet->setMediaQueries(MediaQuerySet::createAllowingDescriptionSyntax(m_media));
        m_sheet->setTitle(title());

        m_loading = false;
        sheetLoaded();
        notifyLoadedSheetAndAllCriticalSubresources(false);
        return;
    }

    Ref<StyleSheetContents> styleSheet(StyleSheetContents::create(href, parserContext));
    m_sheet = CSSStyleSheet::create(styleSheet.copyRef(), this);
    m_sheet->setMediaQueries(MediaQuerySet::createAllowingDescriptionSyntax(m_media));
    m_sheet->setTitle(title());

    styleSheet.get().parseAuthorStyleSheet(cachedStyleSheet, document().securityOrigin());

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
    dispatchEvent(Event::create(eventNames().loadEvent, false, false));
}

void HTMLLinkElement::linkLoadingErrored()
{
    dispatchEvent(Event::create(eventNames().errorEvent, false, false));
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
    ASSERT_UNUSED(eventSender, eventSender == &linkLoadEventSender());
    if (m_loadedSheet)
        linkLoaded();
    else
        linkLoadingErrored();
}

DOMTokenList& HTMLLinkElement::relList()
{
    if (!m_relList) 
        m_relList = std::make_unique<DOMTokenList>(*this, HTMLNames::relAttr);
    return *m_relList;
}

void HTMLLinkElement::notifyLoadedSheetAndAllCriticalSubresources(bool errorOccurred)
{
    if (m_firedLoad)
        return;
    m_loadedSheet = !errorOccurred;
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

void HTMLLinkElement::defaultEventHandler(Event* event)
{
    ASSERT(event);
    if (MouseEvent::canTriggerActivationBehavior(*event)) {
        handleClick(*event);
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
    Frame* frame = document().frame();
    if (!frame)
        return;
    frame->loader().urlSelected(url, target(), &event, LockHistory::No, LockBackForwardList::No, MaybeSendReferrer, document().shouldOpenExternalURLsPolicyToPropagate());
}

URL HTMLLinkElement::href() const
{
    return document().completeURL(getAttribute(hrefAttr));
}

const AtomicString& HTMLLinkElement::rel() const
{
    return fastGetAttribute(relAttr);
}

String HTMLLinkElement::target() const
{
    return getAttribute(targetAttr);
}

const AtomicString& HTMLLinkElement::type() const
{
    return getAttribute(typeAttr);
}

IconType HTMLLinkElement::iconType() const
{
    return m_relAttribute.iconType;
}

String HTMLLinkElement::iconSizes()
{
    return sizes().toString();
}

void HTMLLinkElement::addSubresourceAttributeURLs(ListHashSet<URL>& urls) const
{
    HTMLElement::addSubresourceAttributeURLs(urls);

    // Favicons are handled by a special case in LegacyWebArchive::create()
    if (m_relAttribute.iconType != InvalidIcon)
        return;

    if (!m_relAttribute.isStyleSheet)
        return;
    
    // Append the URL of this link element.
    addSubresourceURL(urls, href());
    
    // Walk the URLs linked by the linked-to stylesheet.
    if (CSSStyleSheet* styleSheet = const_cast<HTMLLinkElement*>(this)->sheet())
        styleSheet->contents().addSubresourceStyleURLs(urls);
}

void HTMLLinkElement::addPendingSheet(PendingSheetType type)
{
    if (type <= m_pendingSheetType)
        return;
    m_pendingSheetType = type;

    if (m_pendingSheetType == InactiveSheet)
        return;
    document().authorStyleSheets().addPendingSheet();
}

void HTMLLinkElement::removePendingSheet(RemovePendingSheetNotificationType notification)
{
    PendingSheetType type = m_pendingSheetType;
    m_pendingSheetType = Unknown;

    if (type == Unknown)
        return;

    if (type == InactiveSheet) {
        // Document just needs to know about the sheet for exposure through document.styleSheets
        document().authorStyleSheets().updateActiveStyleSheets(AuthorStyleSheets::OptimizedUpdate);
        return;
    }

    document().authorStyleSheets().removePendingSheet(
        notification == RemovePendingSheetNotifyImmediately
        ? AuthorStyleSheets::RemovePendingSheetNotifyImmediately
        : AuthorStyleSheets::RemovePendingSheetNotifyLater);
}

} // namespace WebCore
