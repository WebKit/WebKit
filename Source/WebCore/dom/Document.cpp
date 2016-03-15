/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004-2015 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2008, 2009, 2011, 2012 Google Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) Research In Motion Limited 2010-2011. All rights reserved.
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
#include "Document.h"

#include "AXObjectCache.h"
#include "AnimationController.h"
#include "Attr.h"
#include "AuthorStyleSheets.h"
#include "CDATASection.h"
#include "CSSFontSelector.h"
#include "CSSStyleDeclaration.h"
#include "CSSStyleSheet.h"
#include "CachedCSSStyleSheet.h"
#include "CachedResourceLoader.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "Comment.h"
#include "CompositionEvent.h"
#include "ContentSecurityPolicy.h"
#include "CookieJar.h"
#include "CustomElementDefinitions.h"
#include "CustomEvent.h"
#include "DOMImplementation.h"
#include "DOMNamedFlowCollection.h"
#include "DOMWindow.h"
#include "DateComponents.h"
#include "DebugPageOverlays.h"
#include "Dictionary.h"
#include "DocumentLoader.h"
#include "DocumentMarkerController.h"
#include "DocumentSharedObjectPool.h"
#include "DocumentType.h"
#include "Editor.h"
#include "ElementIterator.h"
#include "EntityReference.h"
#include "EventHandler.h"
#include "ExtensionStyleSheets.h"
#include "FocusController.h"
#include "FontFaceSet.h"
#include "FormController.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "FrameView.h"
#include "GenericCachedHTMLCollection.h"
#include "HTMLAllCollection.h"
#include "HTMLAnchorElement.h"
#include "HTMLBaseElement.h"
#include "HTMLBodyElement.h"
#include "HTMLCanvasElement.h"
#include "HTMLDocument.h"
#include "HTMLElementFactory.h"
#include "HTMLFormControlElement.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLFrameSetElement.h"
#include "HTMLHeadElement.h"
#include "HTMLIFrameElement.h"
#include "HTMLImageElement.h"
#include "HTMLLinkElement.h"
#include "HTMLMediaElement.h"
#include "HTMLNameCollection.h"
#include "HTMLParserIdioms.h"
#include "HTMLPictureElement.h"
#include "HTMLPlugInElement.h"
#include "HTMLScriptElement.h"
#include "HTMLStyleElement.h"
#include "HTMLTitleElement.h"
#include "HTMLUnknownElement.h"
#include "HTTPHeaderNames.h"
#include "HTTPParsers.h"
#include "HashChangeEvent.h"
#include "History.h"
#include "HitTestResult.h"
#include "IconController.h"
#include "ImageLoader.h"
#include "InspectorInstrumentation.h"
#include "JSLazyEventListener.h"
#include "JSModuleLoader.h"
#include "KeyboardEvent.h"
#include "Language.h"
#include "LifecycleCallbackQueue.h"
#include "LoaderStrategy.h"
#include "Logging.h"
#include "MainFrame.h"
#include "MediaCanStartListener.h"
#include "MediaProducer.h"
#include "MediaQueryList.h"
#include "MediaQueryMatcher.h"
#include "MessageEvent.h"
#include "MouseEventWithHitTestResults.h"
#include "MutationEvent.h"
#include "NameNodeList.h"
#include "NestingLevelIncrementer.h"
#include "NodeIterator.h"
#include "NodeRareData.h"
#include "NodeWithIndex.h"
#include "OverflowEvent.h"
#include "PageConsoleClient.h"
#include "PageGroup.h"
#include "PageTransitionEvent.h"
#include "PlatformLocale.h"
#include "PlatformMediaSessionManager.h"
#include "PlatformStrategies.h"
#include "PlugInsResources.h"
#include "PluginDocument.h"
#include "PointerLockController.h"
#include "PopStateEvent.h"
#include "ProcessingInstruction.h"
#include "RenderChildIterator.h"
#include "RenderLayerCompositor.h"
#include "RenderView.h"
#include "RenderWidget.h"
#include "ResourceLoadObserver.h"
#include "RuntimeEnabledFeatures.h"
#include "SVGDocumentExtensions.h"
#include "SVGElement.h"
#include "SVGElementFactory.h"
#include "SVGNames.h"
#include "SVGTitleElement.h"
#include "SVGZoomEvent.h"
#include "SchemeRegistry.h"
#include "ScopedEventQueue.h"
#include "ScriptController.h"
#include "ScriptRunner.h"
#include "ScriptSourceCode.h"
#include "ScrollingCoordinator.h"
#include "SecurityOrigin.h"
#include "SecurityOriginPolicy.h"
#include "SecurityPolicy.h"
#include "SegmentedString.h"
#include "SelectorQuery.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "StorageEvent.h"
#include "StyleProperties.h"
#include "StyleResolveForDocument.h"
#include "StyleResolver.h"
#include "StyleSheetContents.h"
#include "StyleSheetList.h"
#include "StyleTreeResolver.h"
#include "SubresourceLoader.h"
#include "TextEvent.h"
#include "TextNodeTraversal.h"
#include "TransformSource.h"
#include "TreeWalker.h"
#include "VisitedLinkState.h"
#include "WheelEvent.h"
#include "WindowFeatures.h"
#include "XMLDocument.h"
#include "XMLDocumentParser.h"
#include "XMLNSNames.h"
#include "XMLNames.h"
#include "XPathEvaluator.h"
#include "XPathExpression.h"
#include "XPathNSResolver.h"
#include "XPathResult.h"
#include "htmlediting.h"
#include <JavaScriptCore/Profile.h>
#include <ctime>
#include <inspector/ScriptCallStack.h>
#include <wtf/CurrentTime.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/SystemTracing.h>
#include <wtf/TemporaryChange.h>
#include <wtf/text/StringBuffer.h>
#include <yarr/RegularExpression.h>

#if ENABLE(DEVICE_ORIENTATION)
#include "DeviceMotionEvent.h"
#include "DeviceOrientationEvent.h"
#endif

#if ENABLE(FULLSCREEN_API)
#include "RenderFullScreen.h"
#endif

#if PLATFORM(IOS)
#include "CSSFontSelector.h"
#include "DeviceMotionClientIOS.h"
#include "DeviceMotionController.h"
#include "DeviceOrientationClientIOS.h"
#include "DeviceOrientationController.h"
#include "Geolocation.h"
#include "Navigator.h"
#include "NavigatorGeolocation.h"
#include "WKContentObservation.h"
#include "WebCoreSystemInterface.h"
#endif

#if ENABLE(IOS_GESTURE_EVENTS)
#include "GestureEvent.h"
#endif

#if ENABLE(IOS_TEXT_AUTOSIZING)
#include "TextAutoSizing.h"
#endif

#if ENABLE(MATHML)
#include "MathMLElement.h"
#include "MathMLElementFactory.h"
#include "MathMLNames.h"
#endif

#if ENABLE(MEDIA_SESSION)
#include "MediaSession.h"
#endif

#if ENABLE(REQUEST_ANIMATION_FRAME)
#include "RequestAnimationFrameCallback.h"
#include "ScriptedAnimationController.h"
#endif

#if ENABLE(TEXT_AUTOSIZING)
#include "TextAutosizer.h"
#endif

#if ENABLE(TOUCH_EVENTS)
#include "TouchEvent.h"
#include "TouchList.h"
#endif

#if ENABLE(VIDEO_TRACK)
#include "CaptionUserPreferences.h"
#endif

#if ENABLE(WEB_REPLAY)
#include "WebReplayInputs.h"
#include <replay/EmptyInputCursor.h>
#include <replay/InputCursor.h>
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
#include "MediaPlaybackTargetClient.h"
#endif

#if ENABLE(XSLT)
#include "XSLTProcessor.h"
#endif

using namespace WTF;
using namespace Unicode;

namespace WebCore {

using namespace HTMLNames;

// #define INSTRUMENT_LAYOUT_SCHEDULING 1

static const unsigned cMaxWriteRecursionDepth = 21;

// DOM Level 2 says (letters added):
//
// a) Name start characters must have one of the categories Ll, Lu, Lo, Lt, Nl.
// b) Name characters other than Name-start characters must have one of the categories Mc, Me, Mn, Lm, or Nd.
// c) Characters in the compatibility area (i.e. with character code greater than #xF900 and less than #xFFFE) are not allowed in XML names.
// d) Characters which have a font or compatibility decomposition (i.e. those with a "compatibility formatting tag" in field 5 of the database -- marked by field 5 beginning with a "<") are not allowed.
// e) The following characters are treated as name-start characters rather than name characters, because the property file classifies them as Alphabetic: [#x02BB-#x02C1], #x0559, #x06E5, #x06E6.
// f) Characters #x20DD-#x20E0 are excluded (in accordance with Unicode, section 5.14).
// g) Character #x00B7 is classified as an extender, because the property list so identifies it.
// h) Character #x0387 is added as a name character, because #x00B7 is its canonical equivalent.
// i) Characters ':' and '_' are allowed as name-start characters.
// j) Characters '-' and '.' are allowed as name characters.
//
// It also contains complete tables. If we decide it's better, we could include those instead of the following code.

static inline bool isValidNameStart(UChar32 c)
{
    // rule (e) above
    if ((c >= 0x02BB && c <= 0x02C1) || c == 0x559 || c == 0x6E5 || c == 0x6E6)
        return true;

    // rule (i) above
    if (c == ':' || c == '_')
        return true;

    // rules (a) and (f) above
    if (!(U_GET_GC_MASK(c) & (U_GC_LL_MASK | U_GC_LU_MASK | U_GC_LO_MASK | U_GC_LT_MASK | U_GC_NL_MASK)))
        return false;

    // rule (c) above
    if (c >= 0xF900 && c < 0xFFFE)
        return false;

    // rule (d) above
    int type = u_getIntPropertyValue(c, UCHAR_DECOMPOSITION_TYPE);
    if (type == U_DT_FONT || type == U_DT_COMPAT)
        return false;

    return true;
}

static inline bool isValidNamePart(UChar32 c)
{
    // rules (a), (e), and (i) above
    if (isValidNameStart(c))
        return true;

    // rules (g) and (h) above
    if (c == 0x00B7 || c == 0x0387)
        return true;

    // rule (j) above
    if (c == '-' || c == '.')
        return true;

    // rules (b) and (f) above
    if (!(U_GET_GC_MASK(c) & (U_GC_M_MASK | U_GC_LM_MASK | U_GC_ND_MASK)))
        return false;

    // rule (c) above
    if (c >= 0xF900 && c < 0xFFFE)
        return false;

    // rule (d) above
    int type = u_getIntPropertyValue(c, UCHAR_DECOMPOSITION_TYPE);
    if (type == U_DT_FONT || type == U_DT_COMPAT)
        return false;

    return true;
}

static bool shouldInheritSecurityOriginFromOwner(const URL& url)
{
    // http://www.whatwg.org/specs/web-apps/current-work/#origin-0
    //
    // If a Document has the address "about:blank"
    //     The origin of the Document is the origin it was assigned when its browsing context was created.
    //
    // Note: We generalize this to all "blank" URLs and invalid URLs because we
    // treat all of these URLs as about:blank.
    //
    return url.isEmpty() || url.isBlankURL();
}

static Widget* widgetForElement(Element* focusedElement)
{
    if (!focusedElement)
        return nullptr;
    auto* renderer = focusedElement->renderer();
    if (!is<RenderWidget>(renderer))
        return nullptr;
    return downcast<RenderWidget>(*renderer).widget();
}

static bool acceptsEditingFocus(Node* node)
{
    ASSERT(node);
    ASSERT(node->hasEditableStyle());

    Node* root = node->rootEditableElement();
    Frame* frame = node->document().frame();
    if (!frame || !root)
        return false;

    return frame->editor().shouldBeginEditing(rangeOfContents(*root).ptr());
}

static bool canAccessAncestor(const SecurityOrigin* activeSecurityOrigin, Frame* targetFrame)
{
    // targetFrame can be 0 when we're trying to navigate a top-level frame
    // that has a 0 opener.
    if (!targetFrame)
        return false;

    const bool isLocalActiveOrigin = activeSecurityOrigin->isLocal();
    for (Frame* ancestorFrame = targetFrame; ancestorFrame; ancestorFrame = ancestorFrame->tree().parent()) {
        Document* ancestorDocument = ancestorFrame->document();
        // FIXME: Should be an ASSERT? Frames should alway have documents.
        if (!ancestorDocument)
            return true;

        const SecurityOrigin* ancestorSecurityOrigin = ancestorDocument->securityOrigin();
        if (activeSecurityOrigin->canAccess(ancestorSecurityOrigin))
            return true;
        
        // Allow file URL descendant navigation even when allowFileAccessFromFileURLs is false.
        // FIXME: It's a bit strange to special-case local origins here. Should we be doing
        // something more general instead?
        if (isLocalActiveOrigin && ancestorSecurityOrigin->isLocal())
            return true;
    }

    return false;
}

static void printNavigationErrorMessage(Frame* frame, const URL& activeURL, const char* reason)
{
    String message = "Unsafe JavaScript attempt to initiate navigation for frame with URL '" + frame->document()->url().string() + "' from frame with URL '" + activeURL.string() + "'. " + reason + "\n";

    // FIXME: should we print to the console of the document performing the navigation instead?
    frame->document()->domWindow()->printErrorMessage(message);
}

uint64_t Document::s_globalTreeVersion = 0;

#if ENABLE(IOS_TEXT_AUTOSIZING)
void TextAutoSizingTraits::constructDeletedValue(TextAutoSizingKey& slot)
{
    new (&slot) TextAutoSizingKey(TextAutoSizingKey::deletedKeyStyle(), TextAutoSizingKey::deletedKeyDoc());
}

bool TextAutoSizingTraits::isDeletedValue(const TextAutoSizingKey& value)
{
    return value.style() == TextAutoSizingKey::deletedKeyStyle() && value.doc() == TextAutoSizingKey::deletedKeyDoc();
}
#endif

HashSet<Document*>& Document::allDocuments()
{
    static NeverDestroyed<HashSet<Document*>> documents;
    return documents;
}

Document::Document(Frame* frame, const URL& url, unsigned documentClasses, unsigned constructionFlags)
    : ContainerNode(*this, CreateDocument)
    , TreeScope(*this)
#if ENABLE(IOS_TOUCH_EVENTS)
    , m_handlingTouchEvent(false)
    , m_touchEventRegionsDirty(false)
    , m_touchEventsChangedTimer(*this, &Document::touchEventsChangedTimerFired)
#endif
    , m_referencingNodeCount(0)
    , m_didCalculateStyleResolver(false)
    , m_hasNodesWithPlaceholderStyle(false)
    , m_needsNotifyRemoveAllPendingStylesheet(false)
    , m_ignorePendingStylesheets(false)
    , m_pendingSheetLayout(NoLayoutWithPendingSheets)
    , m_frame(frame)
    , m_cachedResourceLoader(m_frame ? Ref<CachedResourceLoader>(m_frame->loader().activeDocumentLoader()->cachedResourceLoader()) : CachedResourceLoader::create(nullptr))
    , m_activeParserCount(0)
    , m_wellFormed(false)
    , m_printing(false)
    , m_paginatedForScreen(false)
    , m_compatibilityMode(DocumentCompatibilityMode::NoQuirksMode)
    , m_compatibilityModeLocked(false)
    , m_textColor(Color::black)
    , m_domTreeVersion(++s_globalTreeVersion)
    , m_listenerTypes(0)
    , m_mutationObserverTypes(0)
    , m_authorStyleSheets(std::make_unique<AuthorStyleSheets>(*this))
    , m_extensionStyleSheets(std::make_unique<ExtensionStyleSheets>(*this))
    , m_visitedLinkState(std::make_unique<VisitedLinkState>(*this))
    , m_visuallyOrdered(false)
    , m_readyState(Complete)
    , m_bParsing(false)
    , m_optimizedStyleSheetUpdateTimer(*this, &Document::optimizedStyleSheetUpdateTimerFired)
    , m_styleRecalcTimer(*this, &Document::updateStyleIfNeeded)
    , m_pendingStyleRecalcShouldForce(false)
    , m_inStyleRecalc(false)
    , m_closeAfterStyleRecalc(false)
    , m_gotoAnchorNeededAfterStylesheetsLoad(false)
    , m_frameElementsShouldIgnoreScrolling(false)
    , m_updateFocusAppearanceRestoresSelection(SelectionRestorationMode::SetDefault)
    , m_markers(std::make_unique<DocumentMarkerController>(*this))
    , m_updateFocusAppearanceTimer(*this, &Document::updateFocusAppearanceTimerFired)
    , m_cssTarget(nullptr)
    , m_processingLoadEvent(false)
    , m_loadEventFinished(false)
    , m_startTime(std::chrono::steady_clock::now())
    , m_overMinimumLayoutThreshold(false)
    , m_scriptRunner(std::make_unique<ScriptRunner>(*this))
    , m_moduleLoader(std::make_unique<JSModuleLoader>(*this))
    , m_xmlVersion(ASCIILiteral("1.0"))
    , m_xmlStandalone(StandaloneUnspecified)
    , m_hasXMLDeclaration(false)
    , m_designMode(inherit)
#if !ASSERT_DISABLED
    , m_inInvalidateNodeListAndCollectionCaches(false)
#endif
#if ENABLE(DASHBOARD_SUPPORT)
    , m_hasAnnotatedRegions(false)
    , m_annotatedRegionsDirty(false)
#endif
    , m_createRenderers(true)
    , m_inPageCache(false)
    , m_accessKeyMapValid(false)
    , m_documentClasses(documentClasses)
    , m_isSynthesized(constructionFlags & Synthesized)
    , m_isNonRenderedPlaceholder(constructionFlags & NonRenderedPlaceholder)
    , m_sawElementsInKnownNamespaces(false)
    , m_isSrcdocDocument(false)
    , m_eventQueue(*this)
    , m_weakFactory(this)
#if ENABLE(FULLSCREEN_API)
    , m_areKeysEnabledInFullScreen(0)
    , m_fullScreenRenderer(nullptr)
    , m_fullScreenChangeDelayTimer(*this, &Document::fullScreenChangeDelayTimerFired)
    , m_isAnimatingFullScreen(false)
#endif
    , m_loadEventDelayCount(0)
    , m_loadEventDelayTimer(*this, &Document::loadEventDelayTimerFired)
    , m_referrerPolicy(ReferrerPolicyDefault)
    , m_writeRecursionIsTooDeep(false)
    , m_writeRecursionDepth(0)
    , m_lastHandledUserGestureTimestamp(0)
#if PLATFORM(IOS)
#if ENABLE(DEVICE_ORIENTATION)
    , m_deviceMotionClient(std::make_unique<DeviceMotionClientIOS>())
    , m_deviceMotionController(std::make_unique<DeviceMotionController>(m_deviceMotionClient.get()))
    , m_deviceOrientationClient(std::make_unique<DeviceOrientationClientIOS>())
    , m_deviceOrientationController(std::make_unique<DeviceOrientationController>(m_deviceOrientationClient.get()))
#endif
#endif
#if ENABLE(TELEPHONE_NUMBER_DETECTION)
    , m_isTelephoneNumberParsingAllowed(true)
#endif
    , m_pendingTasksTimer(*this, &Document::pendingTasksTimerFired)
    , m_scheduledTasksAreSuspended(false)
    , m_visualUpdatesAllowed(true)
    , m_visualUpdatesSuppressionTimer(*this, &Document::visualUpdatesSuppressionTimerFired)
    , m_sharedObjectPoolClearTimer(*this, &Document::clearSharedObjectPool)
#ifndef NDEBUG
    , m_didDispatchViewportPropertiesChanged(false)
#endif
#if ENABLE(TEMPLATE_ELEMENT)
    , m_templateDocumentHost(nullptr)
#endif
#if ENABLE(WEB_REPLAY)
    , m_inputCursor(EmptyInputCursor::create())
#endif
    , m_didAssociateFormControlsTimer(*this, &Document::didAssociateFormControlsTimerFired)
    , m_cookieCacheExpiryTimer(*this, &Document::invalidateDOMCookieCache)
    , m_disabledFieldsetElementsCount(0)
    , m_hasInjectedPlugInsScript(false)
    , m_renderTreeBeingDestroyed(false)
    , m_hasPreparedForDestruction(false)
    , m_hasStyleWithViewportUnits(false)
{
    allDocuments().add(this);

    // We depend on the url getting immediately set in subframes, but we
    // also depend on the url NOT getting immediately set in opened windows.
    // See fast/dom/early-frame-url.html
    // and fast/dom/location-new-window-no-crash.html, respectively.
    // FIXME: Can/should we unify this behavior?
    if ((frame && frame->ownerElement()) || !url.isEmpty())
        setURL(url);

    m_cachedResourceLoader->setDocument(this);

#if ENABLE(TEXT_AUTOSIZING)
    m_textAutosizer = std::make_unique<TextAutosizer>(this);
#endif

    resetLinkColor();
    resetVisitedLinkColor();
    resetActiveLinkColor();

    initSecurityContext();
    initDNSPrefetch();

    for (auto& nodeListAndCollectionCount : m_nodeListAndCollectionCounts)
        nodeListAndCollectionCount = 0;
}

#if ENABLE(FULLSCREEN_API)
static bool isAttributeOnAllOwners(const WebCore::QualifiedName& attribute, const WebCore::QualifiedName& prefixedAttribute, const HTMLFrameOwnerElement* owner)
{
    if (!owner)
        return true;
    do {
        if (!(owner->hasAttribute(attribute) || owner->hasAttribute(prefixedAttribute)))
            return false;
    } while ((owner = owner->document().ownerElement()));
    return true;
}
#endif

Ref<Document> Document::create(ScriptExecutionContext& context)
{
    Ref<Document> document = adoptRef(*new Document(nullptr, URL()));
    document->setSecurityOriginPolicy(context.securityOriginPolicy());

    return document;
}

Document::~Document()
{
    allDocuments().remove(this);

    ASSERT(!renderView());
    ASSERT(!m_inPageCache);
    ASSERT(m_ranges.isEmpty());
    ASSERT(!m_parentTreeScope);
    ASSERT(!m_disabledFieldsetElementsCount);

#if ENABLE(DEVICE_ORIENTATION) && PLATFORM(IOS)
    m_deviceMotionClient->deviceMotionControllerDestroyed();
    m_deviceOrientationClient->deviceOrientationControllerDestroyed();
#endif
    
#if ENABLE(TEMPLATE_ELEMENT)
    if (m_templateDocument)
        m_templateDocument->setTemplateDocumentHost(nullptr); // balanced in templateDocument().
#endif

    // FIXME: Should we reset m_domWindow when we detach from the Frame?
    if (m_domWindow)
        m_domWindow->resetUnlessSuspendedForDocumentSuspension();

    m_scriptRunner = nullptr;
    m_moduleLoader = nullptr;

    removeAllEventListeners();

    // Currently we believe that Document can never outlive the parser.
    // Although the Document may be replaced synchronously, DocumentParsers
    // generally keep at least one reference to an Element which would in turn
    // has a reference to the Document.  If you hit this ASSERT, then that
    // assumption is wrong.  DocumentParser::detach() should ensure that even
    // if the DocumentParser outlives the Document it won't cause badness.
    ASSERT(!m_parser || m_parser->refCount() == 1);
    detachParser();

    if (this == &topDocument())
        clearAXObjectCache();

    m_decoder = nullptr;

    if (m_styleSheetList)
        m_styleSheetList->detachFromDocument();

    if (m_elementSheet)
        m_elementSheet->detachFromDocument();
    extensionStyleSheets().detachFromDocument();

    clearStyleResolver(); // We need to destroy CSSFontSelector before destroying m_cachedResourceLoader.

    // It's possible for multiple Documents to end up referencing the same CachedResourceLoader (e.g., SVGImages
    // load the initial empty document and the SVGDocument with the same DocumentLoader).
    if (m_cachedResourceLoader->document() == this)
        m_cachedResourceLoader->setDocument(nullptr);

#if ENABLE(VIDEO)
    if (auto* platformMediaSessionManager = PlatformMediaSessionManager::sharedManagerIfExists())
        platformMediaSessionManager->stopAllMediaPlaybackForDocument(this);
#endif
    
    // We must call clearRareData() here since a Document class inherits TreeScope
    // as well as Node. See a comment on TreeScope.h for the reason.
    if (hasRareData())
        clearRareData();

    ASSERT(!m_listsInvalidatedAtDocument.size());
    ASSERT(!m_collectionsInvalidatedAtDocument.size());

    for (unsigned i = 0; i < WTF_ARRAY_LENGTH(m_nodeListAndCollectionCounts); ++i)
        ASSERT(!m_nodeListAndCollectionCounts[i]);
}

void Document::removedLastRef()
{
    ASSERT(!m_deletionHasBegun);
    if (m_referencingNodeCount) {
        // If removing a child removes the last node reference, we don't want the scope to be destroyed
        // until after removeDetachedChildren returns, so we protect ourselves.
        incrementReferencingNodeCount();

        // We must make sure not to be retaining any of our children through
        // these extra pointers or we will create a reference cycle.
        m_focusedElement = nullptr;
        m_hoveredElement = nullptr;
        m_activeElement = nullptr;
        m_titleElement = nullptr;
        m_documentElement = nullptr;
        m_userActionElements.documentDidRemoveLastRef();
#if ENABLE(FULLSCREEN_API)
        m_fullScreenElement = nullptr;
        m_fullScreenElementStack.clear();
#endif

        detachParser();

        // removeDetachedChildren() doesn't always unregister IDs,
        // so tear down scope information up front to avoid having
        // stale references in the map.

        destroyTreeScopeData();
        removeDetachedChildren();
        m_formController = nullptr;
        
        m_markers->detach();
        
        m_cssCanvasElements.clear();
        
        commonTeardown();

#ifndef NDEBUG
        // We need to do this right now since selfOnlyDeref() can delete this.
        m_inRemovedLastRefFunction = false;
#endif
        decrementReferencingNodeCount();
    } else {
#ifndef NDEBUG
        m_inRemovedLastRefFunction = false;
        m_deletionHasBegun = true;
#endif
        delete this;
    }
}

void Document::commonTeardown()
{
    if (svgExtensions())
        accessSVGExtensions().pauseAnimations();

#if ENABLE(REQUEST_ANIMATION_FRAME)
    clearScriptedAnimationController();
#endif
}

Element* Document::getElementByAccessKey(const String& key)
{
    if (key.isEmpty())
        return nullptr;
    if (!m_accessKeyMapValid) {
        buildAccessKeyMap(this);
        m_accessKeyMapValid = true;
    }
    return m_elementsByAccessKey.get(key.impl());
}

void Document::buildAccessKeyMap(TreeScope* scope)
{
    ASSERT(scope);
    for (auto& element : descendantsOfType<Element>(scope->rootNode())) {
        const AtomicString& accessKey = element.fastGetAttribute(accesskeyAttr);
        if (!accessKey.isEmpty())
            m_elementsByAccessKey.set(accessKey.impl(), &element);

        if (ShadowRoot* root = element.shadowRoot())
            buildAccessKeyMap(root);
    }
}

void Document::invalidateAccessKeyMap()
{
    m_accessKeyMapValid = false;
    m_elementsByAccessKey.clear();
}

void Document::addImageElementByCaseFoldedUsemap(const AtomicStringImpl& name, HTMLImageElement& element)
{
    return m_imagesByUsemap.add(name, element, *this);
}

void Document::removeImageElementByCaseFoldedUsemap(const AtomicStringImpl& name, HTMLImageElement& element)
{
    return m_imagesByUsemap.remove(name, element);
}

HTMLImageElement* Document::imageElementByCaseFoldedUsemap(const AtomicStringImpl& name) const
{
    return m_imagesByUsemap.getElementByCaseFoldedUsemap(name, *this);
}

SelectorQuery* Document::selectorQueryForString(const String& selectorString, ExceptionCode& ec)
{
    if (selectorString.isEmpty()) {
        ec = SYNTAX_ERR;
        return nullptr;
    }

    if (!m_selectorQueryCache)
        m_selectorQueryCache = std::make_unique<SelectorQueryCache>();
    return m_selectorQueryCache->add(selectorString, *this, ec);
}

void Document::clearSelectorQueryCache()
{
    m_selectorQueryCache = nullptr;
}

MediaQueryMatcher& Document::mediaQueryMatcher()
{
    if (!m_mediaQueryMatcher)
        m_mediaQueryMatcher = MediaQueryMatcher::create(this);
    return *m_mediaQueryMatcher;
}

void Document::setCompatibilityMode(DocumentCompatibilityMode mode)
{
    if (m_compatibilityModeLocked || mode == m_compatibilityMode)
        return;
    bool wasInQuirksMode = inQuirksMode();
    m_compatibilityMode = mode;

    clearSelectorQueryCache();

    if (inQuirksMode() != wasInQuirksMode) {
        // All user stylesheets have to reparse using the different mode.
        extensionStyleSheets().clearPageUserSheet();
        extensionStyleSheets().invalidateInjectedStyleSheetCache();
    }
}

String Document::compatMode() const
{
    return inQuirksMode() ? "BackCompat" : "CSS1Compat";
}

void Document::resetLinkColor()
{
    m_linkColor = Color(0, 0, 238);
}

void Document::resetVisitedLinkColor()
{
    m_visitedLinkColor = Color(85, 26, 139);    
}

void Document::resetActiveLinkColor()
{
    m_activeLinkColor.setNamedColor("red");
}

DOMImplementation& Document::implementation()
{
    if (!m_implementation)
        m_implementation = std::make_unique<DOMImplementation>(*this);
    return *m_implementation;
}

bool Document::hasManifest() const
{
    return documentElement() && documentElement()->hasTagName(htmlTag) && documentElement()->fastHasAttribute(manifestAttr);
}

DocumentType* Document::doctype() const
{
    for (Node* node = firstChild(); node; node = node->nextSibling()) {
        if (node->isDocumentTypeNode())
            return static_cast<DocumentType*>(node);
    }
    return nullptr;
}

void Document::childrenChanged(const ChildChange& change)
{
    ContainerNode::childrenChanged(change);

#if PLATFORM(IOS)
    // FIXME: Chrome::didReceiveDocType() used to be called only when the doctype changed. We need to check the
    // impact of calling this systematically. If the overhead is negligible, we need to rename didReceiveDocType,
    // otherwise, we need to detect the doc type changes before updating the viewport.
    if (Page* page = this->page())
        page->chrome().didReceiveDocType(frame());
#endif

    Element* newDocumentElement = childrenOfType<Element>(*this).first();
    if (newDocumentElement == m_documentElement)
        return;
    m_documentElement = newDocumentElement;
    // The root style used for media query matching depends on the document element.
    clearStyleResolver();
}

static RefPtr<Element> createHTMLElementWithNameValidation(Document& document, const AtomicString& localName, ExceptionCode& ec)
{
    RefPtr<HTMLElement> element = HTMLElementFactory::createKnownElement(localName, document);
    if (LIKELY(element))
        return element;

#if ENABLE(CUSTOM_ELEMENTS)
    auto* definitions = document.customElementDefinitions();
    if (UNLIKELY(definitions)) {
        if (auto* interface = definitions->findInterface(localName))
            return interface->constructElement(localName, JSCustomElementInterface::ShouldClearException::DoNotClear);
    }
#endif

    if (UNLIKELY(!Document::isValidName(localName))) {
        ec = INVALID_CHARACTER_ERR;
        return nullptr;
    }

    QualifiedName qualifiedName(nullAtom, localName, xhtmlNamespaceURI);

#if ENABLE(CUSTOM_ELEMENTS)
    if (CustomElementDefinitions::checkName(localName) == CustomElementDefinitions::NameStatus::Valid) {
        Ref<HTMLElement> element = HTMLElement::create(qualifiedName, document);
        element->setIsUnresolvedCustomElement();
        document.ensureCustomElementDefinitions().addUpgradeCandidate(element.get());
        return WTFMove(element);
    }
#endif

    return HTMLUnknownElement::create(qualifiedName, document);
}

RefPtr<Element> Document::createElementForBindings(const AtomicString& name, ExceptionCode& ec)
{
    if (isHTMLDocument())
        return createHTMLElementWithNameValidation(*this, name.convertToASCIILowercase(), ec);

    if (isXHTMLDocument())
        return createHTMLElementWithNameValidation(*this, name, ec);

    if (!isValidName(name)) {
        ec = INVALID_CHARACTER_ERR;
        return nullptr;
    }

    return createElement(QualifiedName(nullAtom, name, nullAtom), false);
}

Ref<DocumentFragment> Document::createDocumentFragment()
{
    return DocumentFragment::create(document());
}

Ref<Text> Document::createTextNode(const String& data)
{
    return Text::create(*this, data);
}

Ref<Comment> Document::createComment(const String& data)
{
    return Comment::create(*this, data);
}

RefPtr<CDATASection> Document::createCDATASection(const String& data, ExceptionCode& ec)
{
    if (isHTMLDocument()) {
        ec = NOT_SUPPORTED_ERR;
        return nullptr;
    }
    return CDATASection::create(*this, data);
}

RefPtr<ProcessingInstruction> Document::createProcessingInstruction(const String& target, const String& data, ExceptionCode& ec)
{
    if (!isValidName(target)) {
        ec = INVALID_CHARACTER_ERR;
        return nullptr;
    }

    if (data.contains("?>")) {
        ec = INVALID_CHARACTER_ERR;
        return nullptr;
    }

    return ProcessingInstruction::create(*this, target, data);
}

RefPtr<EntityReference> Document::createEntityReference(const String&, ExceptionCode& ec)
{
    ec = NOT_SUPPORTED_ERR;
    return nullptr;
}

Ref<Text> Document::createEditingTextNode(const String& text)
{
    return Text::createEditingText(*this, text);
}

Ref<CSSStyleDeclaration> Document::createCSSStyleDeclaration()
{
    Ref<MutableStyleProperties> propertySet(MutableStyleProperties::create());
    return *propertySet->ensureCSSStyleDeclaration();
}

RefPtr<Node> Document::importNode(Node* importedNode, bool deep, ExceptionCode& ec)
{
    if (!importedNode) {
        ec = NOT_SUPPORTED_ERR;
        return nullptr;
    }

    switch (importedNode->nodeType()) {
    case DOCUMENT_FRAGMENT_NODE:
        if (importedNode->isShadowRoot())
            break;
        FALLTHROUGH;
    case ELEMENT_NODE:
    case TEXT_NODE:
    case CDATA_SECTION_NODE:
    case PROCESSING_INSTRUCTION_NODE:
    case COMMENT_NODE:
        return importedNode->cloneNodeInternal(document(), deep ? CloningOperation::Everything : CloningOperation::OnlySelf);

    case ATTRIBUTE_NODE:
        // FIXME: This will "Attr::normalize" child nodes of Attr.
        return Attr::create(*this, QualifiedName(nullAtom, downcast<Attr>(*importedNode).name(), nullAtom), downcast<Attr>(*importedNode).value());

    case DOCUMENT_NODE: // Can't import a document into another document.
    case DOCUMENT_TYPE_NODE: // FIXME: Support cloning a DocumentType node per DOM4.
        break;
    }

    ec = NOT_SUPPORTED_ERR;
    return nullptr;
}


RefPtr<Node> Document::adoptNode(Node* source, ExceptionCode& ec)
{
    if (!source) {
        ec = NOT_SUPPORTED_ERR;
        return nullptr;
    }

    EventQueueScope scope;

    switch (source->nodeType()) {
    case DOCUMENT_NODE:
        ec = NOT_SUPPORTED_ERR;
        return nullptr;
    case ATTRIBUTE_NODE: {                   
        Attr& attr = downcast<Attr>(*source);
        if (attr.ownerElement())
            attr.ownerElement()->removeAttributeNode(&attr, ec);
        break;
    }       
    default:
        if (source->isShadowRoot()) {
            // ShadowRoot cannot disconnect itself from the host node.
            ec = HIERARCHY_REQUEST_ERR;
            return nullptr;
        }
        if (is<HTMLFrameOwnerElement>(*source)) {
            HTMLFrameOwnerElement& frameOwnerElement = downcast<HTMLFrameOwnerElement>(*source);
            if (frame() && frame()->tree().isDescendantOf(frameOwnerElement.contentFrame())) {
                ec = HIERARCHY_REQUEST_ERR;
                return nullptr;
            }
        }
        if (source->parentNode()) {
            source->parentNode()->removeChild(*source, ec);
            if (ec)
                return nullptr;
        }
    }

    adoptIfNeeded(source);

    return source;
}

bool Document::hasValidNamespaceForElements(const QualifiedName& qName)
{
    // These checks are from DOM Core Level 2, createElementNS
    // http://www.w3.org/TR/DOM-Level-2-Core/core.html#ID-DocCrElNS
    if (!qName.prefix().isEmpty() && qName.namespaceURI().isNull()) // createElementNS(null, "html:div")
        return false;
    if (qName.prefix() == xmlAtom && qName.namespaceURI() != XMLNames::xmlNamespaceURI) // createElementNS("http://www.example.com", "xml:lang")
        return false;

    // Required by DOM Level 3 Core and unspecified by DOM Level 2 Core:
    // http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core.html#ID-DocCrElNS
    // createElementNS("http://www.w3.org/2000/xmlns/", "foo:bar"), createElementNS(null, "xmlns:bar"), createElementNS(null, "xmlns")
    if (qName.prefix() == xmlnsAtom || (qName.prefix().isEmpty() && qName.localName() == xmlnsAtom))
        return qName.namespaceURI() == XMLNSNames::xmlnsNamespaceURI;
    return qName.namespaceURI() != XMLNSNames::xmlnsNamespaceURI;
}

bool Document::hasValidNamespaceForAttributes(const QualifiedName& qName)
{
    return hasValidNamespaceForElements(qName);
}

static Ref<HTMLElement> createFallbackHTMLElement(Document& document, const QualifiedName& name)
{
#if ENABLE(CUSTOM_ELEMENTS)
    auto* definitions = document.customElementDefinitions();
    if (UNLIKELY(definitions)) {
        if (auto* interface = definitions->findInterface(name)) {
            Ref<HTMLElement> element = HTMLElement::create(name, document);
            element->setIsUnresolvedCustomElement();
            LifecycleCallbackQueue::enqueueElementUpgrade(element.get(), *interface);
            return element;
        }
    }
    // FIXME: Should we also check the equality of prefix between the custom element and name?
    if (CustomElementDefinitions::checkName(name.localName()) == CustomElementDefinitions::NameStatus::Valid) {
        Ref<HTMLElement> element = HTMLElement::create(name, document);
        element->setIsUnresolvedCustomElement();
        document.ensureCustomElementDefinitions().addUpgradeCandidate(element.get());
        return element;
    }
#endif
    return HTMLUnknownElement::create(name, document);
}

// FIXME: This should really be in a possible ElementFactory class.
Ref<Element> Document::createElement(const QualifiedName& name, bool createdByParser)
{
    RefPtr<Element> element;

    // FIXME: Use registered namespaces and look up in a hash to find the right factory.
    if (name.namespaceURI() == xhtmlNamespaceURI) {
        element = HTMLElementFactory::createKnownElement(name, *this, nullptr, createdByParser);
        if (UNLIKELY(!element))
            element = createFallbackHTMLElement(*this, name);
    } else if (name.namespaceURI() == SVGNames::svgNamespaceURI)
        element = SVGElementFactory::createElement(name, *this, createdByParser);
#if ENABLE(MATHML)
    else if (name.namespaceURI() == MathMLNames::mathmlNamespaceURI)
        element = MathMLElementFactory::createElement(name, *this, createdByParser);
#endif

    if (element)
        m_sawElementsInKnownNamespaces = true;
    else
        element = Element::create(name, document());

    // <image> uses imgTag so we need a special rule.
    ASSERT((name.matches(imageTag) && element->tagQName().matches(imgTag) && element->tagQName().prefix() == name.prefix()) || name == element->tagQName());

    return element.releaseNonNull();
}

bool Document::cssRegionsEnabled() const
{
    return RuntimeEnabledFeatures::sharedFeatures().cssRegionsEnabled(); 
}

bool Document::cssCompositingEnabled() const
{
    return RuntimeEnabledFeatures::sharedFeatures().cssCompositingEnabled();
}

#if ENABLE(CSS_REGIONS)

RefPtr<DOMNamedFlowCollection> Document::webkitGetNamedFlows()
{
    if (!cssRegionsEnabled() || !renderView())
        return nullptr;

    updateStyleIfNeeded();

    return namedFlows().createCSSOMSnapshot();
}

#endif

NamedFlowCollection& Document::namedFlows()
{
    if (!m_namedFlows)
        m_namedFlows = NamedFlowCollection::create(this);

    return *m_namedFlows;
}

RefPtr<Element> Document::createElementNS(const String& namespaceURI, const String& qualifiedName, ExceptionCode& ec)
{
    String prefix, localName;
    if (!parseQualifiedName(qualifiedName, prefix, localName, ec))
        return nullptr;

    QualifiedName qName(prefix, localName, namespaceURI);
    if (!hasValidNamespaceForElements(qName)) {
        ec = NAMESPACE_ERR;
        return nullptr;
    }

    return createElement(qName, false);
}

String Document::readyState() const
{
    static NeverDestroyed<const String> loading(ASCIILiteral("loading"));
    static NeverDestroyed<const String> interactive(ASCIILiteral("interactive"));
    static NeverDestroyed<const String> complete(ASCIILiteral("complete"));

    switch (m_readyState) {
    case Loading:
        return loading;
    case Interactive:
        return interactive;
    case Complete:
        return complete;
    }

    ASSERT_NOT_REACHED();
    return String();
}

void Document::setReadyState(ReadyState readyState)
{
    if (readyState == m_readyState)
        return;

#if ENABLE(WEB_TIMING)
    switch (readyState) {
    case Loading:
        if (!m_documentTiming.domLoading)
            m_documentTiming.domLoading = monotonicallyIncreasingTime();
        break;
    case Interactive:
        if (!m_documentTiming.domInteractive)
            m_documentTiming.domInteractive = monotonicallyIncreasingTime();
        break;
    case Complete:
        if (!m_documentTiming.domComplete)
            m_documentTiming.domComplete = monotonicallyIncreasingTime();
        break;
    }
#endif

    m_readyState = readyState;
    dispatchEvent(Event::create(eventNames().readystatechangeEvent, false, false));
    
    if (settings() && settings()->suppressesIncrementalRendering())
        setVisualUpdatesAllowed(readyState);
}

void Document::setVisualUpdatesAllowed(ReadyState readyState)
{
    ASSERT(settings() && settings()->suppressesIncrementalRendering());
    switch (readyState) {
    case Loading:
        ASSERT(!m_visualUpdatesSuppressionTimer.isActive());
        ASSERT(m_visualUpdatesAllowed);
        setVisualUpdatesAllowed(false);
        break;
    case Interactive:
        ASSERT(m_visualUpdatesSuppressionTimer.isActive() || m_visualUpdatesAllowed);
        break;
    case Complete:
        if (m_visualUpdatesSuppressionTimer.isActive()) {
            ASSERT(!m_visualUpdatesAllowed);

            if (!view()->visualUpdatesAllowedByClient())
                return;

            setVisualUpdatesAllowed(true);
        } else
            ASSERT(m_visualUpdatesAllowed);
        break;
    }
}
    
void Document::setVisualUpdatesAllowed(bool visualUpdatesAllowed)
{
    if (m_visualUpdatesAllowed == visualUpdatesAllowed)
        return;

    m_visualUpdatesAllowed = visualUpdatesAllowed;

    if (visualUpdatesAllowed)
        m_visualUpdatesSuppressionTimer.stop();
    else
        m_visualUpdatesSuppressionTimer.startOneShot(settings()->incrementalRenderingSuppressionTimeoutInSeconds());

    if (!visualUpdatesAllowed)
        return;

    FrameView* frameView = view();
    bool needsLayout = frameView && renderView() && (frameView->layoutPending() || renderView()->needsLayout());
    if (needsLayout)
        updateLayout();

    if (Page* page = this->page()) {
        if (frame()->isMainFrame()) {
            frameView->addPaintPendingMilestones(DidFirstPaintAfterSuppressedIncrementalRendering);
            if (page->requestedLayoutMilestones() & DidFirstLayoutAfterSuppressedIncrementalRendering)
                frame()->loader().didLayout(DidFirstLayoutAfterSuppressedIncrementalRendering);
        }
    }

    if (view())
        view()->updateCompositingLayersAfterLayout();

    if (RenderView* renderView = this->renderView())
        renderView->repaintViewAndCompositedLayers();

    if (Frame* frame = this->frame())
        frame->loader().forcePageTransitionIfNeeded();
}

void Document::visualUpdatesSuppressionTimerFired()
{
    ASSERT(!m_visualUpdatesAllowed);

    // If the client is extending the visual update suppression period explicitly, the
    // watchdog should not re-enable visual updates itself, but should wait for the client.
    if (!view()->visualUpdatesAllowedByClient())
        return;

    setVisualUpdatesAllowed(true);
}

void Document::setVisualUpdatesAllowedByClient(bool visualUpdatesAllowedByClient)
{
    // We should only re-enable visual updates if ReadyState is Completed or the watchdog timer has fired,
    // both of which we can determine by looking at the timer.

    if (visualUpdatesAllowedByClient && !m_visualUpdatesSuppressionTimer.isActive() && !visualUpdatesAllowed())
        setVisualUpdatesAllowed(true);
}

String Document::characterSetWithUTF8Fallback() const
{
    AtomicString name = encoding();
    if (!name.isNull())
        return name;
    return UTF8Encoding().domName();
}

String Document::defaultCharset() const
{
    if (Settings* settings = this->settings())
        return settings->defaultTextEncodingName();
    return UTF8Encoding().domName();
}

void Document::setCharset(const String& charset)
{
    if (!decoder())
        return;
    decoder()->setEncoding(charset, TextResourceDecoder::UserChosenEncoding);
}

void Document::setContentLanguage(const String& language)
{
    if (m_contentLanguage == language)
        return;
    m_contentLanguage = language;

    // Recalculate style so language is used when selecting the initial font.
    styleResolverChanged(DeferRecalcStyle);
}

void Document::setXMLVersion(const String& version, ExceptionCode& ec)
{
    if (!implementation().hasFeature("XML", String())) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }

    if (!XMLDocumentParser::supportsXMLVersion(version)) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }

    m_xmlVersion = version;
}

void Document::setXMLStandalone(bool standalone, ExceptionCode& ec)
{
    if (!implementation().hasFeature("XML", String())) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }

    m_xmlStandalone = standalone ? Standalone : NotStandalone;
}

void Document::setDocumentURI(const String& uri)
{
    // This property is read-only from JavaScript, but writable from Objective-C.
    m_documentURI = uri;
    updateBaseURL();
}

void Document::setContent(const String& content)
{
    open();
    // FIXME: This should probably use insert(), but that's (intentionally)
    // not implemented for the XML parser as it's normally synonymous with
    // document.write(). append() will end up yielding, but close() will
    // pump the tokenizer syncrhonously and finish the parse.
    m_parser->append(content.impl());
    close();
}

String Document::suggestedMIMEType() const
{
    if (isXHTMLDocument())
        return ASCIILiteral("application/xhtml+xml");
    if (isSVGDocument())
        return ASCIILiteral("image/svg+xml");
    if (xmlStandalone())
        return ASCIILiteral("text/xml");
    if (isHTMLDocument())
        return ASCIILiteral("text/html");
    if (DocumentLoader* loader = this->loader())
        return loader->responseMIMEType();
    return String();
}

void Document::overrideMIMEType(const String& mimeType)
{
    m_overriddenMIMEType = mimeType;
}

String Document::contentType() const
{
    if (!m_overriddenMIMEType.isNull())
        return m_overriddenMIMEType;

    if (DocumentLoader* documentLoader = loader())
        return documentLoader->currentContentType();

    String mimeType = suggestedMIMEType();
    if (!mimeType.isNull())
        return mimeType;

    return ASCIILiteral("application/xml");
}

Node* Document::nodeFromPoint(const LayoutPoint& clientPoint, LayoutPoint* localPoint)
{
    if (!frame() || !view())
        return nullptr;
    
    Frame& frame = *this->frame();
    
    float scaleFactor = frame.pageZoomFactor() * frame.frameScaleFactor();

    LayoutPoint contentsPoint = clientPoint;
    contentsPoint.scale(scaleFactor, scaleFactor);
    contentsPoint.moveBy(view()->contentsScrollPosition());

    LayoutRect visibleRect;
#if PLATFORM(IOS)
    visibleRect = view()->unobscuredContentRect();
#else
    visibleRect = view()->visibleContentRect();
#endif
    if (!visibleRect.contains(contentsPoint))
        return nullptr;

    HitTestResult result(contentsPoint);
    renderView()->hitTest(HitTestRequest(), result);

    if (localPoint)
        *localPoint = result.localPoint();

    return result.innerNode();
}

Element* Document::elementFromPoint(const LayoutPoint& clientPoint)
{
    if (!hasLivingRenderTree())
        return nullptr;

    Node* node = nodeFromPoint(clientPoint);
    while (node && !is<Element>(*node))
        node = node->parentNode();

    if (node)
        node = ancestorInThisScope(node);

    return downcast<Element>(node);
}

RefPtr<Range> Document::caretRangeFromPoint(int x, int y)
{
    return caretRangeFromPoint(LayoutPoint(x, y));
}

RefPtr<Range> Document::caretRangeFromPoint(const LayoutPoint& clientPoint)
{
    if (!hasLivingRenderTree())
        return nullptr;

    LayoutPoint localPoint;
    Node* node = nodeFromPoint(clientPoint, &localPoint);
    if (!node)
        return nullptr;

    Node* shadowAncestorNode = ancestorInThisScope(node);
    if (shadowAncestorNode != node) {
        unsigned offset = shadowAncestorNode->computeNodeIndex();
        ContainerNode* container = shadowAncestorNode->parentNode();
        return Range::create(*this, container, offset, container, offset);
    }

    RenderObject* renderer = node->renderer();
    if (!renderer)
        return nullptr;
    VisiblePosition visiblePosition = renderer->positionForPoint(localPoint, nullptr);
    if (visiblePosition.isNull())
        return nullptr;

    Position rangeCompliantPosition = visiblePosition.deepEquivalent().parentAnchoredEquivalent();
    return Range::create(*this, rangeCompliantPosition, rangeCompliantPosition);
}

Element* Document::scrollingElement()
{
    // FIXME: When we fix https://bugs.webkit.org/show_bug.cgi?id=106133, this should be replaced with the full implementation
    // of Document.scrollingElement() as specified at http://dev.w3.org/csswg/cssom-view/#dom-document-scrollingelement.

    return body();
}

/*
 * Performs three operations:
 *  1. Convert control characters to spaces
 *  2. Trim leading and trailing spaces
 *  3. Collapse internal whitespace.
 */
template <typename CharacterType>
static inline StringWithDirection canonicalizedTitle(Document* document, const StringWithDirection& titleWithDirection)
{
    const String& title = titleWithDirection.string();
    const CharacterType* characters = title.characters<CharacterType>();
    unsigned length = title.length();
    unsigned i;

    StringBuffer<CharacterType> buffer(length);
    unsigned builderIndex = 0;

    // Skip leading spaces and leading characters that would convert to spaces
    for (i = 0; i < length; ++i) {
        CharacterType c = characters[i];
        if (!(c <= 0x20 || c == 0x7F))
            break;
    }

    if (i == length)
        return StringWithDirection();

    // Replace control characters with spaces, and backslashes with currency symbols, and collapse whitespace.
    bool previousCharWasWS = false;
    for (; i < length; ++i) {
        CharacterType c = characters[i];
        if (c <= 0x20 || c == 0x7F || (U_GET_GC_MASK(c) & (U_GC_ZL_MASK | U_GC_ZP_MASK))) {
            if (previousCharWasWS)
                continue;
            buffer[builderIndex++] = ' ';
            previousCharWasWS = true;
        } else {
            buffer[builderIndex++] = c;
            previousCharWasWS = false;
        }
    }

    // Strip trailing spaces
    while (builderIndex > 0) {
        --builderIndex;
        if (buffer[builderIndex] != ' ')
            break;
    }

    if (!builderIndex && buffer[builderIndex] == ' ')
        return StringWithDirection();

    buffer.shrink(builderIndex + 1);

    // Replace the backslashes with currency symbols if the encoding requires it.
    document->displayBufferModifiedByEncoding(buffer.characters(), buffer.length());
    
    return StringWithDirection(String::adopt(buffer), titleWithDirection.direction());
}

void Document::updateTitle(const StringWithDirection& title)
{
    if (m_rawTitle == title)
        return;

    m_rawTitle = title;

    if (m_rawTitle.string().isEmpty())
        m_title = StringWithDirection();
    else {
        if (m_rawTitle.string().is8Bit())
            m_title = canonicalizedTitle<LChar>(this, m_rawTitle);
        else
            m_title = canonicalizedTitle<UChar>(this, m_rawTitle);
    }
    if (DocumentLoader* loader = this->loader())
        loader->setTitle(m_title);
}

void Document::updateTitleFromTitleElement()
{
    if (!m_titleElement) {
        updateTitle(StringWithDirection());
        return;
    }

    if (is<HTMLTitleElement>(*m_titleElement))
        updateTitle(downcast<HTMLTitleElement>(*m_titleElement).textWithDirection());
    else if (is<SVGTitleElement>(*m_titleElement)) {
        // FIXME: does SVG have a title text direction?
        updateTitle(StringWithDirection(downcast<SVGTitleElement>(*m_titleElement).textContent(), LTR));
    }
}

void Document::setTitle(const String& title)
{
    if (!isHTMLDocument() && !isXHTMLDocument())
        m_titleElement = nullptr;
    else if (!m_titleElement) {
        auto* headElement = head();
        if (!headElement)
            return;
        m_titleElement = createElement(titleTag, false);
        headElement->appendChild(*m_titleElement, ASSERT_NO_EXCEPTION);
    }

    // The DOM API has no method of specifying direction, so assume LTR.
    updateTitle(StringWithDirection(title, LTR));

    if (is<HTMLTitleElement>(m_titleElement.get()))
        downcast<HTMLTitleElement>(*m_titleElement).setText(title);
}

void Document::updateTitleElement(Element* newTitleElement)
{
    // Only allow the first title element in tree order to change the title -- others have no effect.
    if (m_titleElement) {
        if (isHTMLDocument() || isXHTMLDocument())
            m_titleElement = descendantsOfType<HTMLTitleElement>(*this).first();
        else if (isSVGDocument())
            m_titleElement = descendantsOfType<SVGTitleElement>(*this).first();
    } else
        m_titleElement = newTitleElement;

    updateTitleFromTitleElement();
}

void Document::titleElementAdded(Element& titleElement)
{
    if (m_titleElement == &titleElement)
        return;

    updateTitleElement(&titleElement);
}

void Document::titleElementRemoved(Element& titleElement)
{
    if (m_titleElement != &titleElement)
        return;

    updateTitleElement(nullptr);
}

void Document::titleElementTextChanged(Element& titleElement)
{
    if (m_titleElement != &titleElement)
        return;

    updateTitleFromTitleElement();
}

void Document::registerForVisibilityStateChangedCallbacks(Element* element)
{
    m_visibilityStateCallbackElements.add(element);
}

void Document::unregisterForVisibilityStateChangedCallbacks(Element* element)
{
    m_visibilityStateCallbackElements.remove(element);
}

void Document::visibilityStateChanged()
{
    dispatchEvent(Event::create(eventNames().visibilitychangeEvent, false, false));
    for (auto* element : m_visibilityStateCallbackElements)
        element->visibilityStateChanged();
}

PageVisibilityState Document::pageVisibilityState() const
{
    // The visibility of the document is inherited from the visibility of the
    // page. If there is no page associated with the document, we will assume
    // that the page is hidden, as specified by the spec:
    // http://dvcs.w3.org/hg/webperf/raw-file/tip/specs/PageVisibility/Overview.html#dom-document-hidden
    if (!m_frame || !m_frame->page())
        return PageVisibilityStateHidden;
    return m_frame->page()->visibilityState();
}

String Document::visibilityState() const
{
    return pageVisibilityStateString(pageVisibilityState());
}

bool Document::hidden() const
{
    return pageVisibilityState() != PageVisibilityStateVisible;
}


#if ENABLE(VIDEO)
void Document::registerForAllowsMediaDocumentInlinePlaybackChangedCallbacks(HTMLMediaElement& element)
{
    m_allowsMediaDocumentInlinePlaybackElements.add(&element);
}

void Document::unregisterForAllowsMediaDocumentInlinePlaybackChangedCallbacks(HTMLMediaElement& element)
{
    m_allowsMediaDocumentInlinePlaybackElements.remove(&element);
}

void Document::allowsMediaDocumentInlinePlaybackChanged()
{
    for (auto* element : m_allowsMediaDocumentInlinePlaybackElements)
        element->allowsMediaDocumentInlinePlaybackChanged();
}
#endif

String Document::nodeName() const
{
    return "#document";
}

Node::NodeType Document::nodeType() const
{
    return DOCUMENT_NODE;
}

FormController& Document::formController()
{
    if (!m_formController)
        m_formController = std::make_unique<FormController>();
    return *m_formController;
}

Vector<String> Document::formElementsState() const
{
    if (!m_formController)
        return Vector<String>();
    return m_formController->formElementsState();
}

void Document::setStateForNewFormElements(const Vector<String>& stateVector)
{
    if (!stateVector.size() && !m_formController)
        return;
    formController().setStateForNewFormElements(stateVector);
}

FrameView* Document::view() const
{
    return m_frame ? m_frame->view() : nullptr;
}

Page* Document::page() const
{
    return m_frame ? m_frame->page() : nullptr;
}

Settings* Document::settings() const
{
    return m_frame ? &m_frame->settings() : nullptr;
}

Ref<Range> Document::createRange()
{
    return Range::create(*this);
}

RefPtr<NodeIterator> Document::createNodeIterator(Node* root, unsigned long whatToShow, RefPtr<NodeFilter>&& filter, bool, ExceptionCode& ec)
{
    return createNodeIterator(root, whatToShow, WTFMove(filter), ec);
}

RefPtr<NodeIterator> Document::createNodeIterator(Node* root, unsigned long whatToShow, RefPtr<NodeFilter>&& filter, ExceptionCode& ec)
{
    if (!root) {
        ec = TypeError;
        return nullptr;
    }
    return NodeIterator::create(root, whatToShow, WTFMove(filter));
}

RefPtr<NodeIterator> Document::createNodeIterator(Node* root, unsigned long whatToShow, ExceptionCode& ec)
{
    return createNodeIterator(root, whatToShow, nullptr, ec);
}

RefPtr<NodeIterator> Document::createNodeIterator(Node* root, ExceptionCode& ec)
{
    return createNodeIterator(root, 0xFFFFFFFF, nullptr, ec);
}

RefPtr<TreeWalker> Document::createTreeWalker(Node* root, unsigned long whatToShow, RefPtr<NodeFilter>&& filter, bool, ExceptionCode& ec)
{
    return createTreeWalker(root, whatToShow, WTFMove(filter), ec);
}

RefPtr<TreeWalker> Document::createTreeWalker(Node* root, unsigned long whatToShow, RefPtr<NodeFilter>&& filter, ExceptionCode& ec)
{
    if (!root) {
        ec = TypeError;
        return nullptr;
    }
    return TreeWalker::create(*root, whatToShow, WTFMove(filter));
}

RefPtr<TreeWalker> Document::createTreeWalker(Node* root, unsigned long whatToShow, ExceptionCode& ec)
{
    return createTreeWalker(root, whatToShow, nullptr, ec);
}

RefPtr<TreeWalker> Document::createTreeWalker(Node* root, ExceptionCode& ec)
{
    return createTreeWalker(root, 0xFFFFFFFF, nullptr, ec);
}

void Document::scheduleForcedStyleRecalc()
{
    m_pendingStyleRecalcShouldForce = true;
    scheduleStyleRecalc();
}

void Document::scheduleStyleRecalc()
{
    if (m_styleRecalcTimer.isActive() || inPageCache())
        return;

    ASSERT(childNeedsStyleRecalc() || m_pendingStyleRecalcShouldForce);

    // FIXME: Why on earth is this here? This is clearly misplaced.
    invalidateAccessKeyMap();
    
    m_styleRecalcTimer.startOneShot(0);

    InspectorInstrumentation::didScheduleStyleRecalculation(*this);
}

void Document::unscheduleStyleRecalc()
{
    ASSERT(!childNeedsStyleRecalc());

    m_styleRecalcTimer.stop();
    m_pendingStyleRecalcShouldForce = false;
}

bool Document::hasPendingStyleRecalc() const
{
    return m_styleRecalcTimer.isActive() && !m_inStyleRecalc;
}

bool Document::hasPendingForcedStyleRecalc() const
{
    return m_styleRecalcTimer.isActive() && m_pendingStyleRecalcShouldForce;
}

void Document::recalcStyle(Style::Change change)
{
    ASSERT(!view() || !view()->isPainting());

    // NOTE: XSL code seems to be the only client stumbling in here without a RenderView.
    if (!m_renderView)
        return;

    FrameView& frameView = m_renderView->frameView();
    Ref<FrameView> protect(frameView);
    if (frameView.isPainting())
        return;
    
    if (m_inStyleRecalc)
        return; // Guard against re-entrancy. -dwh

    TraceScope tracingScope(StyleRecalcStart, StyleRecalcEnd);

    RenderView::RepaintRegionAccumulator repaintRegionAccumulator(renderView());
    AnimationUpdateBlock animationUpdateBlock(&m_frame->animation());

    // FIXME: We should update style on our ancestor chain before proceeding (especially for seamless),
    // however doing so currently causes several tests to crash, as Frame::setDocument calls Document::attach
    // before setting the DOMWindow on the Frame, or the SecurityOrigin on the document. The attach, in turn
    // resolves style (here) and then when we resolve style on the parent chain, we may end up
    // re-attaching our containing iframe, which when asked HTMLFrameElementBase::isURLAllowed
    // hits a null-dereference due to security code always assuming the document has a SecurityOrigin.

    authorStyleSheets().flushPendingUpdates();

    frameView.willRecalcStyle();

    InspectorInstrumentationCookie cookie = InspectorInstrumentation::willRecalculateStyle(*this);

    // FIXME: We never reset this flags.
    if (m_elementSheet && m_elementSheet->contents().usesRemUnits())
        authorStyleSheets().setUsesRemUnit(true);
    // We don't call setUsesStyleBasedEditability here because the whole point of the flag is to avoid style recalc.
    // i.e. updating the flag here would be too late.

    m_inStyleRecalc = true;
    bool updatedCompositingLayers = false;
    {
        Style::PostResolutionCallbackDisabler disabler(*this);
        WidgetHierarchyUpdatesSuspensionScope suspendWidgetHierarchyUpdates;

        if (m_pendingStyleRecalcShouldForce)
            change = Style::Force;

        if (change == Style::Force) {
            // This may get set again during style resolve.
            m_hasNodesWithPlaceholderStyle = false;

            auto documentStyle = Style::resolveForDocument(*this);

            // Inserting the pictograph font at the end of the font fallback list is done by the
            // font selector, so set a font selector if needed.
            if (Settings* settings = this->settings()) {
                if (settings->fontFallbackPrefersPictographs())
                    documentStyle.get().fontCascade().update(&fontSelector());
            }

            auto documentChange = Style::determineChange(documentStyle.get(), m_renderView->style());
            if (documentChange != Style::NoChange)
                renderView()->setStyle(WTFMove(documentStyle));
        }

        Style::TreeResolver resolver(*this);
        resolver.resolve(change);

        updatedCompositingLayers = frameView.updateCompositingLayersAfterStyleChange();

        clearNeedsStyleRecalc();
        clearChildNeedsStyleRecalc();
        unscheduleStyleRecalc();

        m_inStyleRecalc = false;
    }

    // If we wanted to call implicitClose() during recalcStyle, do so now that we're finished.
    if (m_closeAfterStyleRecalc) {
        m_closeAfterStyleRecalc = false;
        implicitClose();
    }
    
    ++m_styleRecalcCount;

    InspectorInstrumentation::didRecalculateStyle(cookie);

    // Some animated images may now be inside the viewport due to style recalc,
    // resume them if necessary if there is no layout pending. Otherwise, we'll
    // check if they need to be resumed after layout.
    if (updatedCompositingLayers && !frameView.needsLayout())
        frameView.viewportContentsChanged();

    if (!frameView.needsLayout())
        frameView.frame().selection().updateAppearanceAfterLayout();

    // As a result of the style recalculation, the currently hovered element might have been
    // detached (for example, by setting display:none in the :hover style), schedule another mouseMove event
    // to check if any other elements ended up under the mouse pointer due to re-layout.
    if (m_hoveredElement && !m_hoveredElement->renderer())
        frameView.frame().mainFrame().eventHandler().dispatchFakeMouseMoveEventSoon();
}

void Document::updateStyleIfNeeded()
{
    ASSERT(isMainThread());
    ASSERT(!view() || !view()->isPainting());

    if (!view() || view()->isInRenderTreeLayout())
        return;

    if (m_optimizedStyleSheetUpdateTimer.isActive())
        styleResolverChanged(RecalcStyleIfNeeded);

    if (!needsStyleRecalc())
        return;

    recalcStyle(Style::NoChange);
}

void Document::updateLayout()
{
    ASSERT(isMainThread());

    FrameView* frameView = view();
    if (frameView && frameView->isInRenderTreeLayout()) {
        // View layout should not be re-entrant.
        ASSERT_NOT_REACHED();
        return;
    }

    RenderView::RepaintRegionAccumulator repaintRegionAccumulator(renderView());

    if (HTMLFrameOwnerElement* owner = ownerElement())
        owner->document().updateLayout();

    updateStyleIfNeeded();

    StackStats::LayoutCheckPoint layoutCheckPoint;

    // Only do a layout if changes have occurred that make it necessary.      
    if (frameView && renderView() && (frameView->layoutPending() || renderView()->needsLayout()))
        frameView->layout();
}

// FIXME: This is a bad idea and needs to be removed eventually.
// Other browsers load stylesheets before they continue parsing the web page.
// Since we don't, we can run JavaScript code that needs answers before the
// stylesheets are loaded. Doing a layout ignoring the pending stylesheets
// lets us get reasonable answers. The long term solution to this problem is
// to instead suspend JavaScript execution.
void Document::updateLayoutIgnorePendingStylesheets(Document::RunPostLayoutTasks runPostLayoutTasks)
{
    bool oldIgnore = m_ignorePendingStylesheets;

    if (!haveStylesheetsLoaded()) {
        m_ignorePendingStylesheets = true;
        // FIXME: We are willing to attempt to suppress painting with outdated style info only once.  Our assumption is that it would be
        // dangerous to try to stop it a second time, after page content has already been loaded and displayed
        // with accurate style information.  (Our suppression involves blanking the whole page at the
        // moment.  If it were more refined, we might be able to do something better.)
        // It's worth noting though that this entire method is a hack, since what we really want to do is
        // suspend JS instead of doing a layout with inaccurate information.
        HTMLElement* bodyElement = bodyOrFrameset();
        if (bodyElement && !bodyElement->renderer() && m_pendingSheetLayout == NoLayoutWithPendingSheets) {
            m_pendingSheetLayout = DidLayoutWithPendingSheets;
            styleResolverChanged(RecalcStyleImmediately);
        } else if (m_hasNodesWithPlaceholderStyle)
            // If new nodes have been added or style recalc has been done with style sheets still pending, some nodes 
            // may not have had their real style calculated yet. Normally this gets cleaned when style sheets arrive 
            // but here we need up-to-date style immediately.
            recalcStyle(Style::Force);
    }

    updateLayout();

    if (runPostLayoutTasks == RunPostLayoutTasks::Synchronously && view())
        view()->flushAnyPendingPostLayoutTasks();

    m_ignorePendingStylesheets = oldIgnore;
}

Ref<RenderStyle> Document::styleForElementIgnoringPendingStylesheets(Element& element, RenderStyle* parentStyle)
{
    ASSERT(&element.document() == this);

    // On iOS request delegates called during styleForElement may result in re-entering WebKit and killing the style resolver.
    ResourceLoadSuspender suspender;

    TemporaryChange<bool> change(m_ignorePendingStylesheets, true);
    auto elementStyle = element.resolveStyle(parentStyle);

    Style::commitRelationsToDocument(WTFMove(elementStyle.relations));

    return WTFMove(elementStyle.renderStyle);
}

bool Document::updateLayoutIfDimensionsOutOfDate(Element& element, DimensionsCheck dimensionsCheck)
{
    ASSERT(isMainThread());
    
    // If the stylesheets haven't loaded, just give up and do a full layout ignoring pending stylesheets.
    if (!haveStylesheetsLoaded()) {
        updateLayoutIgnorePendingStylesheets();
        return true;
    }
    
    // Check for re-entrancy and assert (same code that is in updateLayout()).
    FrameView* frameView = view();
    if (frameView && frameView->isInRenderTreeLayout()) {
        // View layout should not be re-entrant.
        ASSERT_NOT_REACHED();
        return true;
    }
    
    RenderView::RepaintRegionAccumulator repaintRegionAccumulator(renderView());
    
    // Mimic the structure of updateLayout(), but at each step, see if we have been forced into doing a full
    // layout.
    bool requireFullLayout = false;
    if (HTMLFrameOwnerElement* owner = ownerElement()) {
        if (owner->document().updateLayoutIfDimensionsOutOfDate(*owner))
            requireFullLayout = true;
    }
    
    updateStyleIfNeeded();

    RenderObject* renderer = element.renderer();
    if (!renderer || renderer->needsLayout() || element.renderNamedFlowFragment()) {
        // If we don't have a renderer or if the renderer needs layout for any reason, give up.
        // Named flows can have auto height, so don't try to enforce the optimization in this case.
        // The 2-pass nature of auto height named flow layout means the region may not be dirty yet.
        requireFullLayout = true;
    }

    bool isVertical = renderer && !renderer->isHorizontalWritingMode();
    bool checkingLogicalWidth = ((dimensionsCheck & WidthDimensionsCheck) && !isVertical) || ((dimensionsCheck & HeightDimensionsCheck) && isVertical);
    bool checkingLogicalHeight = ((dimensionsCheck & HeightDimensionsCheck) && !isVertical) || ((dimensionsCheck & WidthDimensionsCheck) && isVertical);
    bool hasSpecifiedLogicalHeight = renderer && renderer->style().logicalMinHeight() == Length(0, Fixed) && renderer->style().logicalHeight().isFixed() && renderer->style().logicalMaxHeight().isAuto();
    
    if (!requireFullLayout) {
        RenderBox* previousBox = nullptr;
        RenderBox* currentBox = nullptr;
        
        // Check our containing block chain. If anything in the chain needs a layout, then require a full layout.
        for (RenderObject* currRenderer = element.renderer(); currRenderer && !currRenderer->isRenderView(); currRenderer = currRenderer->container()) {
            
            // Require the entire container chain to be boxes.
            if (!is<RenderBox>(currRenderer)) {
                requireFullLayout = true;
                break;
            }
            
            previousBox = currentBox;
            currentBox = downcast<RenderBox>(currRenderer);
            
            // If a box needs layout for itself or if a box has changed children and sizes its width to
            // its content, then require a full layout.
            if (currentBox->selfNeedsLayout() ||
                (checkingLogicalWidth && currRenderer->needsLayout() && currentBox->sizesLogicalWidthToFitContent(MainOrPreferredSize))) {
                requireFullLayout = true;
                break;
            }
            
            // If a block contains floats and the child's height isn't specified, then
            // give up also, since our height could end up being influenced by the floats.
            if (checkingLogicalHeight && !hasSpecifiedLogicalHeight && currentBox->isRenderBlockFlow()) {
                RenderBlockFlow* currentBlockFlow = downcast<RenderBlockFlow>(currentBox);
                if (currentBlockFlow->containsFloats() && previousBox && !previousBox->isFloatingOrOutOfFlowPositioned()) {
                    requireFullLayout = true;
                    break;
                }
            }
            
            if (!currentBox->isRenderBlockFlow() || currentBox->flowThreadContainingBlock() || currentBox->isWritingModeRoot()) {
                // FIXME: For now require only block flows all the way back to the root. This limits the optimization
                // for now, and we'll expand it in future patches to apply to more and more scenarios.
                // Disallow regions/columns from having the optimization.
                // Give up if the writing mode changes at all in the containing block chain.
                requireFullLayout = true;
                break;
            }
            
            if (currRenderer == frameView->layoutRoot())
                break;
        }
    }
    
    StackStats::LayoutCheckPoint layoutCheckPoint;

    // Only do a layout if changes have occurred that make it necessary.      
    if (requireFullLayout && frameView && renderView() && (frameView->layoutPending() || renderView()->needsLayout()))
        frameView->layout();
    
    return requireFullLayout;
}

bool Document::isPageBoxVisible(int pageIndex)
{
    Ref<RenderStyle> pageStyle(ensureStyleResolver().styleForPage(pageIndex));
    return pageStyle->visibility() != HIDDEN; // display property doesn't apply to @page.
}

void Document::pageSizeAndMarginsInPixels(int pageIndex, IntSize& pageSize, int& marginTop, int& marginRight, int& marginBottom, int& marginLeft)
{
    RefPtr<RenderStyle> style = ensureStyleResolver().styleForPage(pageIndex);

    int width = pageSize.width();
    int height = pageSize.height();
    switch (style->pageSizeType()) {
    case PAGE_SIZE_AUTO:
        break;
    case PAGE_SIZE_AUTO_LANDSCAPE:
        if (width < height)
            std::swap(width, height);
        break;
    case PAGE_SIZE_AUTO_PORTRAIT:
        if (width > height)
            std::swap(width, height);
        break;
    case PAGE_SIZE_RESOLVED: {
        LengthSize size = style->pageSize();
        ASSERT(size.width().isFixed());
        ASSERT(size.height().isFixed());
        width = valueForLength(size.width(), 0);
        height = valueForLength(size.height(), 0);
        break;
    }
    default:
        ASSERT_NOT_REACHED();
    }
    pageSize = IntSize(width, height);

    // The percentage is calculated with respect to the width even for margin top and bottom.
    // http://www.w3.org/TR/CSS2/box.html#margin-properties
    marginTop = style->marginTop().isAuto() ? marginTop : intValueForLength(style->marginTop(), width);
    marginRight = style->marginRight().isAuto() ? marginRight : intValueForLength(style->marginRight(), width);
    marginBottom = style->marginBottom().isAuto() ? marginBottom : intValueForLength(style->marginBottom(), width);
    marginLeft = style->marginLeft().isAuto() ? marginLeft : intValueForLength(style->marginLeft(), width);
}

void Document::createStyleResolver()
{
    m_styleResolver = std::make_unique<StyleResolver>(*this);
    m_styleResolver->appendAuthorStyleSheets(authorStyleSheets().activeStyleSheets());
}

StyleResolver& Document::userAgentShadowTreeStyleResolver()
{
    if (!m_userAgentShadowTreeStyleResolver) {
        m_userAgentShadowTreeStyleResolver = std::make_unique<StyleResolver>(*this);

        // FIXME: Filter out shadow pseudo elements we don't want to expose to authors.
        auto& documentAuthorStyle = *ensureStyleResolver().ruleSets().authorStyle();
        if (documentAuthorStyle.hasShadowPseudoElementRules())
            m_userAgentShadowTreeStyleResolver->ruleSets().authorStyle()->copyShadowPseudoElementRulesFrom(documentAuthorStyle);
    }

    return *m_userAgentShadowTreeStyleResolver;
}

void Document::fontsNeedUpdate(FontSelector&)
{
    if (m_styleResolver)
        m_styleResolver->invalidateMatchedPropertiesCache();
    if (inPageCache() || !renderView())
        return;
    scheduleForcedStyleRecalc();
}

CSSFontSelector& Document::fontSelector()
{
    if (!m_fontSelector) {
        m_fontSelector = CSSFontSelector::create(*this);
        m_fontSelector->registerForInvalidationCallbacks(*this);
    }
    return *m_fontSelector;
}

void Document::clearStyleResolver()
{
    m_styleResolver = nullptr;
    m_userAgentShadowTreeStyleResolver = nullptr;

    // FIXME: It would be better if the FontSelector could survive this operation.
    if (m_fontSelector) {
        m_fontSelector->clearDocument();
        m_fontSelector->unregisterForInvalidationCallbacks(*this);
        m_fontSelector = nullptr;
    }
}

void Document::createRenderTree()
{
    ASSERT(!renderView());
    ASSERT(!m_inPageCache);
    ASSERT(!m_axObjectCache || this != &topDocument());

    if (m_isNonRenderedPlaceholder)
        return;

    // FIXME: It would be better if we could pass the resolved document style directly here.
    m_renderView = createRenderer<RenderView>(*this, RenderStyle::create());
    Node::setRenderer(m_renderView.get());

    renderView()->setIsInWindow(true);

    recalcStyle(Style::Force);
}

void Document::didBecomeCurrentDocumentInFrame()
{
    // FIXME: Are there cases where the document can be dislodged from the frame during the event handling below?
    // If so, then m_frame could become 0, and we need to do something about that.

    m_frame->script().updateDocument();

    if (!hasLivingRenderTree())
        createRenderTree();

    updateViewportArguments();

    // FIXME: Doing this only for the main frame is insufficient.
    // Changing a subframe can also change the wheel event handler count.
    // FIXME: Doing this only when a document goes into the frame is insufficient.
    // Removing a document can also change the wheel event handler count.
    // FIXME: Doing this every time is a waste. If the current document and its
    // subframes' documents have no wheel event handlers, then the count did not change,
    // unless the documents they are replacing had wheel event handlers.
    if (page() && m_frame->isMainFrame())
        wheelEventHandlersChanged();

#if ENABLE(TOUCH_EVENTS)
    // FIXME: Doing this only for the main frame is insufficient.
    // A subframe could have touch event handlers.
    if (hasTouchEventHandlers() && page() && m_frame->isMainFrame())
        page()->chrome().client().needTouchEvents(true);
#endif

    // Ensure that the scheduled task state of the document matches the DOM suspension state of the frame. It can
    // be out of sync if the DOM suspension state changed while the document was not in the frame (possibly in the
    // page cache, or simply newly created).
    if (m_frame->activeDOMObjectsAndAnimationsSuspended()) {
        m_frame->animation().suspendAnimationsForDocument(this);
        suspendScheduledTasks(ActiveDOMObject::PageWillBeSuspended);
    } else {
        resumeScheduledTasks(ActiveDOMObject::PageWillBeSuspended);
        m_frame->animation().resumeAnimationsForDocument(this);
    }
}

void Document::disconnectFromFrame()
{
    m_frame = nullptr;
}

void Document::destroyRenderTree()
{
    ASSERT(hasLivingRenderTree());
    ASSERT(!m_inPageCache);

    TemporaryChange<bool> change(m_renderTreeBeingDestroyed, true);

    if (this == &topDocument())
        clearAXObjectCache();

    documentWillBecomeInactive();

    if (FrameView* frameView = view())
        frameView->detachCustomScrollbars();

#if ENABLE(FULLSCREEN_API)
    if (m_fullScreenRenderer)
        setFullScreenRenderer(nullptr);
#endif

    m_hoveredElement = nullptr;
    m_focusedElement = nullptr;
    m_activeElement = nullptr;

    if (m_documentElement)
        Style::detachRenderTree(*m_documentElement);

    clearChildNeedsStyleRecalc();

    unscheduleStyleRecalc();

    m_renderView = nullptr;
    Node::setRenderer(nullptr);

#if ENABLE(IOS_TEXT_AUTOSIZING)
    // Do this before the arena is cleared, which is needed to deref the RenderStyle on TextAutoSizingKey.
    m_textAutoSizedNodes.clear();
#endif
}

void Document::prepareForDestruction()
{
    if (m_hasPreparedForDestruction)
        return;

#if ENABLE(IOS_TOUCH_EVENTS)
    clearTouchEventListeners();
#endif

#if HAVE(ACCESSIBILITY)
    // Sub-frames need to cleanup Nodes in the text marker cache when the Document disappears.
    if (this != &topDocument()) {
        if (AXObjectCache* cache = existingAXObjectCache())
            cache->clearTextMarkerNodesInUse(this);
    }
#endif
    
    disconnectDescendantFrames();
    if (m_domWindow && m_frame)
        m_domWindow->willDetachDocumentFromFrame();

    if (hasLivingRenderTree())
        destroyRenderTree();

    if (is<PluginDocument>(*this))
        downcast<PluginDocument>(*this).detachFromPluginElement();

#if ENABLE(POINTER_LOCK)
    if (page())
        page()->pointerLockController().documentDetached(this);
#endif

    InspectorInstrumentation::documentDetached(*this);

    stopActiveDOMObjects();
    m_eventQueue.close();
#if ENABLE(FULLSCREEN_API)
    m_fullScreenChangeEventTargetQueue.clear();
    m_fullScreenErrorEventTargetQueue.clear();
#endif

    commonTeardown();

#if ENABLE(TOUCH_EVENTS)
    if (m_touchEventTargets && m_touchEventTargets->size() && parentDocument())
        parentDocument()->didRemoveEventTargetNode(*this);
#endif

    if (m_wheelEventTargets && m_wheelEventTargets->size() && parentDocument())
        parentDocument()->didRemoveEventTargetNode(*this);

    if (m_mediaQueryMatcher)
        m_mediaQueryMatcher->documentDestroyed();

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    if (!m_clientToIDMap.isEmpty() && page()) {
        Vector<WebCore::MediaPlaybackTargetClient*> clients;
        copyKeysToVector(m_clientToIDMap, clients);
        for (auto* client : clients)
            removePlaybackTargetPickerClient(*client);
    }
#endif

    disconnectFromFrame();

    m_hasPreparedForDestruction = true;
}

void Document::removeAllEventListeners()
{
    EventTarget::removeAllEventListeners();

    if (m_domWindow)
        m_domWindow->removeAllEventListeners();
#if ENABLE(IOS_TOUCH_EVENTS)
    clearTouchEventListeners();
#endif
    for (Node* node = firstChild(); node; node = NodeTraversal::next(*node))
        node->removeAllEventListeners();
}

void Document::suspendDeviceMotionAndOrientationUpdates()
{
    if (m_areDeviceMotionAndOrientationUpdatesSuspended)
        return;
    m_areDeviceMotionAndOrientationUpdatesSuspended = true;
#if ENABLE(DEVICE_ORIENTATION) && PLATFORM(IOS)
    if (m_deviceMotionController)
        m_deviceMotionController->suspendUpdates();
    if (m_deviceOrientationController)
        m_deviceOrientationController->suspendUpdates();
#endif
}

void Document::resumeDeviceMotionAndOrientationUpdates()
{
    if (!m_areDeviceMotionAndOrientationUpdatesSuspended)
        return;
    m_areDeviceMotionAndOrientationUpdatesSuspended = false;
#if ENABLE(DEVICE_ORIENTATION) && PLATFORM(IOS)
    if (m_deviceMotionController)
        m_deviceMotionController->resumeUpdates();
    if (m_deviceOrientationController)
        m_deviceOrientationController->resumeUpdates();
#endif
}

bool Document::shouldBypassMainWorldContentSecurityPolicy() const
{
    JSC::CallFrame* callFrame = JSDOMWindow::commonVM().topCallFrame;
    if (callFrame == JSC::CallFrame::noCaller())
        return false;
    DOMWrapperWorld& domWrapperWorld = currentWorld(callFrame);
    if (domWrapperWorld.isNormal())
        return false;
    return true;
}

void Document::platformSuspendOrStopActiveDOMObjects()
{
#if PLATFORM(IOS)
    if (WebThreadCountOfObservedContentModifiers() > 0) {
        Frame* frame = this->frame();
        if (Page* page = frame ? frame->page() : nullptr)
            page->chrome().client().clearContentChangeObservers(frame);
    }
#endif
}

void Document::suspendActiveDOMObjects(ActiveDOMObject::ReasonForSuspension why)
{
    ScriptExecutionContext::suspendActiveDOMObjects(why);
    suspendDeviceMotionAndOrientationUpdates();
    platformSuspendOrStopActiveDOMObjects();
}

void Document::resumeActiveDOMObjects(ActiveDOMObject::ReasonForSuspension why)
{
    ScriptExecutionContext::resumeActiveDOMObjects(why);
    resumeDeviceMotionAndOrientationUpdates();
    // FIXME: For iOS, do we need to add content change observers that were removed in Document::suspendActiveDOMObjects()?
}

void Document::stopActiveDOMObjects()
{
    ScriptExecutionContext::stopActiveDOMObjects();
    platformSuspendOrStopActiveDOMObjects();
}

void Document::clearAXObjectCache()
{
    ASSERT(&topDocument() == this);
    // Clear the cache member variable before calling delete because attempts
    // are made to access it during destruction.
    m_axObjectCache = nullptr;
}

AXObjectCache* Document::existingAXObjectCache() const
{
    Document& topDocument = this->topDocument();
    if (!topDocument.hasLivingRenderTree())
        return nullptr;
    return topDocument.m_axObjectCache.get();
}

AXObjectCache* Document::axObjectCache() const
{
    if (!AXObjectCache::accessibilityEnabled())
        return nullptr;
    
    // The only document that actually has a AXObjectCache is the top-level
    // document.  This is because we need to be able to get from any WebCoreAXObject
    // to any other WebCoreAXObject on the same page.  Using a single cache allows
    // lookups across nested webareas (i.e. multiple documents).
    Document& topDocument = this->topDocument();

    // If the document has already been detached, do not make a new axObjectCache.
    if (!topDocument.hasLivingRenderTree())
        return nullptr;

    ASSERT(&topDocument == this || !m_axObjectCache);
    if (!topDocument.m_axObjectCache)
        topDocument.m_axObjectCache = std::make_unique<AXObjectCache>(topDocument);
    return topDocument.m_axObjectCache.get();
}

void Document::setVisuallyOrdered()
{
    m_visuallyOrdered = true;
    if (renderView())
        renderView()->style().setRTLOrdering(VisualOrder);
}

Ref<DocumentParser> Document::createParser()
{
    // FIXME: this should probably pass the frame instead
    return XMLDocumentParser::create(*this, view());
}

ScriptableDocumentParser* Document::scriptableDocumentParser() const
{
    return parser() ? parser()->asScriptableDocumentParser() : nullptr;
}

void Document::open(Document* ownerDocument)
{
    if (m_ignoreOpensDuringUnloadCount)
        return;

    if (ownerDocument) {
        setURL(ownerDocument->url());
        setCookieURL(ownerDocument->cookieURL());
        setSecurityOriginPolicy(ownerDocument->securityOriginPolicy());
    }

    if (m_frame) {
        if (ScriptableDocumentParser* parser = scriptableDocumentParser()) {
            if (parser->isParsing()) {
                // FIXME: HTML5 doesn't tell us to check this, it might not be correct.
                if (parser->isExecutingScript())
                    return;

                if (!parser->wasCreatedByScript() && parser->hasInsertionPoint())
                    return;
            }
        }

        if (m_frame->loader().state() == FrameStateProvisional)
            m_frame->loader().stopAllLoaders();
    }

    removeAllEventListeners();
    implicitOpen();
    if (ScriptableDocumentParser* parser = scriptableDocumentParser())
        parser->setWasCreatedByScript(true);

    if (m_frame)
        m_frame->loader().didExplicitOpen();
}

void Document::detachParser()
{
    if (!m_parser)
        return;
    m_parser->detach();
    m_parser = nullptr;
}

void Document::cancelParsing()
{
    if (!m_parser)
        return;

    // We have to clear the parser to avoid possibly triggering
    // the onload handler when closing as a side effect of a cancel-style
    // change, such as opening a new document or closing the window while
    // still parsing
    detachParser();
    explicitClose();
}

void Document::implicitOpen()
{
    cancelParsing();

    removeChildren();

    setCompatibilityMode(DocumentCompatibilityMode::NoQuirksMode);

    m_parser = createParser();
    setParsing(true);
    setReadyState(Loading);
}

HTMLBodyElement* Document::body() const
{
    auto* element = documentElement();
    if (!element)
        return nullptr;
    return childrenOfType<HTMLBodyElement>(*element).first();
}

HTMLElement* Document::bodyOrFrameset() const
{
    // Return the first body or frameset child of the html element.
    auto* element = documentElement();
    if (!element)
        return nullptr;
    for (auto& child : childrenOfType<HTMLElement>(*element)) {
        if (is<HTMLBodyElement>(child) || is<HTMLFrameSetElement>(child))
            return &child;
    }
    return nullptr;
}

void Document::setBodyOrFrameset(RefPtr<HTMLElement>&& newBody, ExceptionCode& ec)
{
    // FIXME: This does not support setting a <frameset> Element, only a <body>. This does
    // not match the HTML specification:
    // https://html.spec.whatwg.org/multipage/dom.html#dom-document-body
    if (!newBody || !documentElement() || !newBody->hasTagName(bodyTag)) { 
        ec = HIERARCHY_REQUEST_ERR;
        return;
    }

    if (&newBody->document() != this) {
        ec = 0;
        RefPtr<Node> node = importNode(newBody.get(), true, ec);
        if (ec)
            return;
        
        newBody = downcast<HTMLElement>(node.get());
    }

    if (auto* body = bodyOrFrameset())
        documentElement()->replaceChild(newBody.releaseNonNull(), *body, ec);
    else
        documentElement()->appendChild(newBody.releaseNonNull(), ec);
}

Location* Document::location() const
{
    auto* window = domWindow();
    if (!window)
        return nullptr;

    return window->location();
}

HTMLHeadElement* Document::head()
{
    if (auto element = documentElement())
        return childrenOfType<HTMLHeadElement>(*element).first();
    return nullptr;
}

void Document::close()
{
    // FIXME: We should follow the specification more closely:
    //        http://www.whatwg.org/specs/web-apps/current-work/#dom-document-close

    if (!scriptableDocumentParser() || !scriptableDocumentParser()->wasCreatedByScript() || !scriptableDocumentParser()->isParsing())
        return;

    explicitClose();
}

void Document::explicitClose()
{
    if (RefPtr<DocumentParser> parser = m_parser)
        parser->finish();

    if (!m_frame) {
        // Because we have no frame, we don't know if all loading has completed,
        // so we just call implicitClose() immediately. FIXME: This might fire
        // the load event prematurely <http://bugs.webkit.org/show_bug.cgi?id=14568>.
        implicitClose();
        return;
    }

    m_frame->loader().checkCompleted();
}

void Document::implicitClose()
{
    // If we're in the middle of recalcStyle, we need to defer the close until the style information is accurate and all elements are re-attached.
    if (m_inStyleRecalc) {
        m_closeAfterStyleRecalc = true;
        return;
    }

    bool wasLocationChangePending = frame() && frame()->navigationScheduler().locationChangePending();
    bool doload = !parsing() && m_parser && !m_processingLoadEvent && !wasLocationChangePending;
    
    if (!doload)
        return;

    // Call to dispatchWindowLoadEvent can blow us from underneath.
    Ref<Document> protect(*this);

    m_processingLoadEvent = true;

    ScriptableDocumentParser* parser = scriptableDocumentParser();
    m_wellFormed = parser && parser->wellFormed();

    // We have to clear the parser, in case someone document.write()s from the
    // onLoad event handler, as in Radar 3206524.
    detachParser();

    // FIXME: We kick off the icon loader when the Document is done parsing.
    // There are earlier opportunities we could start it:
    //  -When the <head> finishes parsing
    //  -When any new HTMLLinkElement is inserted into the document
    // But those add a dynamic component to the favicon that has UI 
    // ramifications, and we need to decide what is the Right Thing To Do(tm)
    Frame* f = frame();
    if (f) {
        f->loader().icon().startLoader();
        f->animation().startAnimationsIfNotSuspended(this);

        // FIXME: We shouldn't be dispatching pending events globally on all Documents here.
        // For now, only do this when there is a Frame, otherwise this could cause JS reentrancy
        // below SVG font parsing, for example. <https://webkit.org/b/136269>
        ImageLoader::dispatchPendingBeforeLoadEvents();
        ImageLoader::dispatchPendingLoadEvents();
        ImageLoader::dispatchPendingErrorEvents();
        HTMLLinkElement::dispatchPendingLoadEvents();
        HTMLStyleElement::dispatchPendingLoadEvents();
    }

    // To align the HTML load event and the SVGLoad event for the outermost <svg> element, fire it from
    // here, instead of doing it from SVGElement::finishedParsingChildren (if externalResourcesRequired="false",
    // which is the default, for ='true' its fired at a later time, once all external resources finished loading).
    if (svgExtensions())
        accessSVGExtensions().dispatchSVGLoadEventToOutermostSVGElements();

    dispatchWindowLoadEvent();
    enqueuePageshowEvent(PageshowEventNotPersisted);
    if (m_pendingStateObject)
        enqueuePopstateEvent(WTFMove(m_pendingStateObject));

    if (f)
        f->loader().dispatchOnloadEvents();
#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (!ownerElement())
        printf("onload fired at %lld\n", elapsedTime().count());
#endif

    // An event handler may have removed the frame
    if (!frame()) {
        m_processingLoadEvent = false;
        return;
    }

    // Make sure both the initial layout and reflow happen after the onload
    // fires. This will improve onload scores, and other browsers do it.
    // If they wanna cheat, we can too. -dwh

    if (frame()->navigationScheduler().locationChangePending() && elapsedTime() < settings()->layoutInterval()) {
        // Just bail out. Before or during the onload we were shifted to another page.
        // The old i-Bench suite does this. When this happens don't bother painting or laying out.        
        m_processingLoadEvent = false;
        view()->unscheduleRelayout();
        return;
    }

    frame()->loader().checkCallImplicitClose();
    
    // We used to force a synchronous display and flush here.  This really isn't
    // necessary and can in fact be actively harmful if pages are loading at a rate of > 60fps
    // (if your platform is syncing flushes and limiting them to 60fps).
    m_overMinimumLayoutThreshold = true;
    if (!ownerElement() || (ownerElement()->renderer() && !ownerElement()->renderer()->needsLayout())) {
        updateStyleIfNeeded();
        
        // Always do a layout after loading if needed.
        if (view() && renderView() && (!renderView()->firstChild() || renderView()->needsLayout()))
            view()->layout();
    }

    m_processingLoadEvent = false;

#if PLATFORM(COCOA) || PLATFORM(WIN) || PLATFORM(GTK) || PLATFORM(EFL)
    if (f && hasLivingRenderTree() && AXObjectCache::accessibilityEnabled()) {
        // The AX cache may have been cleared at this point, but we need to make sure it contains an
        // AX object to send the notification to. getOrCreate will make sure that an valid AX object
        // exists in the cache (we ignore the return value because we don't need it here). This is 
        // only safe to call when a layout is not in progress, so it can not be used in postNotification.
        //
        // This notification is now called AXNewDocumentLoadComplete because there are other handlers that will
        // catch new AND page history loads, and that uses AXLoadComplete
        
        axObjectCache()->getOrCreate(renderView());
        if (this == &topDocument())
            axObjectCache()->postNotification(renderView(), AXObjectCache::AXNewDocumentLoadComplete);
        else {
            // AXLoadComplete can only be posted on the top document, so if it's a document
            // in an iframe that just finished loading, post AXLayoutComplete instead.
            axObjectCache()->postNotification(renderView(), AXObjectCache::AXLayoutComplete);
        }
    }
#endif

    if (svgExtensions())
        accessSVGExtensions().startAnimations();
}

void Document::setParsing(bool b)
{
    m_bParsing = b;

    if (m_bParsing && !m_sharedObjectPool)
        m_sharedObjectPool = std::make_unique<DocumentSharedObjectPool>();

    if (!m_bParsing && view() && !view()->needsLayout())
        view()->fireLayoutRelatedMilestonesIfNeeded();

#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (!ownerElement() && !m_bParsing)
        printf("Parsing finished at %lld\n", elapsedTime().count());
#endif
}

bool Document::shouldScheduleLayout()
{
    // This function will only be called when FrameView thinks a layout is needed.
    // This enforces a couple extra rules.
    //
    //    (a) Only schedule a layout once the stylesheets are loaded.
    //    (b) Only schedule layout once we have a body element.

    return (haveStylesheetsLoaded() && bodyOrFrameset())
        || (documentElement() && !is<HTMLHtmlElement>(*documentElement()));
}
    
bool Document::isLayoutTimerActive()
{
    return view() && view()->layoutPending() && !minimumLayoutDelay().count();
}

std::chrono::milliseconds Document::minimumLayoutDelay()
{
    if (m_overMinimumLayoutThreshold)
        return std::chrono::milliseconds(0);
    
    std::chrono::milliseconds elapsed = elapsedTime();
    m_overMinimumLayoutThreshold = elapsed > settings()->layoutInterval();

    // We'll want to schedule the timer to fire at the minimum layout threshold.
    return std::max(std::chrono::milliseconds(0), settings()->layoutInterval() - elapsed);
}

std::chrono::milliseconds Document::elapsedTime() const
{
    auto elapsedTime = std::chrono::steady_clock::now() - m_startTime;

    return std::chrono::duration_cast<std::chrono::milliseconds>(elapsedTime);
}

void Document::write(const SegmentedString& text, Document* ownerDocument)
{
    NestingLevelIncrementer nestingLevelIncrementer(m_writeRecursionDepth);

    m_writeRecursionIsTooDeep = (m_writeRecursionDepth > 1) && m_writeRecursionIsTooDeep;
    m_writeRecursionIsTooDeep = (m_writeRecursionDepth > cMaxWriteRecursionDepth) || m_writeRecursionIsTooDeep;

    if (m_writeRecursionIsTooDeep)
       return;

#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (!ownerElement())
        printf("Beginning a document.write at %lld\n", elapsedTime().count());
#endif

    bool hasInsertionPoint = m_parser && m_parser->hasInsertionPoint();
    if (!hasInsertionPoint && (m_ignoreOpensDuringUnloadCount || m_ignoreDestructiveWriteCount))
        return;

    if (!hasInsertionPoint)
        open(ownerDocument);

    ASSERT(m_parser);
    m_parser->insert(text);

#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (!ownerElement())
        printf("Ending a document.write at %lld\n", elapsedTime().count());
#endif    
}

void Document::write(const String& text, Document* ownerDocument)
{
    write(SegmentedString(text), ownerDocument);
}

void Document::writeln(const String& text, Document* ownerDocument)
{
    write(text, ownerDocument);
    write("\n", ownerDocument);
}

std::chrono::milliseconds Document::minimumTimerInterval() const
{
    auto* page = this->page();
    if (!page)
        return ScriptExecutionContext::minimumTimerInterval();
    return page->settings().minimumDOMTimerInterval();
}

void Document::setTimerThrottlingEnabled(bool shouldThrottle)
{
    if (m_isTimerThrottlingEnabled == shouldThrottle)
        return;

    m_isTimerThrottlingEnabled = shouldThrottle;
    didChangeTimerAlignmentInterval();
}

std::chrono::milliseconds Document::timerAlignmentInterval(bool hasReachedMaxNestingLevel) const
{
    auto alignmentInterval = ScriptExecutionContext::timerAlignmentInterval(hasReachedMaxNestingLevel);

    // Apply Document-level DOMTimer throttling only if timers have reached their maximum nesting level as the Page may still be visible.
    if (m_isTimerThrottlingEnabled && hasReachedMaxNestingLevel)
        alignmentInterval = std::max(alignmentInterval, DOMTimer::hiddenPageAlignmentInterval());

    if (Page* page = this->page())
        alignmentInterval = std::max(alignmentInterval, page->domTimerAlignmentInterval());

    return alignmentInterval;
}

EventTarget* Document::errorEventTarget()
{
    return m_domWindow.get();
}

void Document::logExceptionToConsole(const String& errorMessage, const String& sourceURL, int lineNumber, int columnNumber, RefPtr<Inspector::ScriptCallStack>&& callStack)
{
    addMessage(MessageSource::JS, MessageLevel::Error, errorMessage, sourceURL, lineNumber, columnNumber, WTFMove(callStack));
}

void Document::setURL(const URL& url)
{
    const URL& newURL = url.isEmpty() ? blankURL() : url;
    if (newURL == m_url)
        return;

    m_url = newURL;
    m_documentURI = m_url.string();
    updateBaseURL();
}

void Document::updateBaseURL()
{
    URL oldBaseURL = m_baseURL;
    // DOM 3 Core: When the Document supports the feature "HTML" [DOM Level 2 HTML], the base URI is computed using
    // first the value of the href attribute of the HTML BASE element if any, and the value of the documentURI attribute
    // from the Document interface otherwise.
    if (!m_baseElementURL.isEmpty())
        m_baseURL = m_baseElementURL;
    else if (!m_baseURLOverride.isEmpty())
        m_baseURL = m_baseURLOverride;
    else {
        // The documentURI attribute is read-only from JavaScript, but writable from Objective C, so we need to retain
        // this fallback behavior. We use a null base URL, since the documentURI attribute is an arbitrary string
        // and DOM 3 Core does not specify how it should be resolved.
        m_baseURL = URL(ParsedURLString, documentURI());
    }

    clearSelectorQueryCache();

    if (!m_baseURL.isValid())
        m_baseURL = URL();

    if (m_elementSheet) {
        // Element sheet is silly. It never contains anything.
        ASSERT(!m_elementSheet->contents().ruleCount());
        bool usesRemUnits = m_elementSheet->contents().usesRemUnits();
        bool usesStyleBasedEditability = m_elementSheet->contents().usesStyleBasedEditability();
        m_elementSheet = CSSStyleSheet::createInline(*this, m_baseURL);
        // FIXME: So we are not really the parser. The right fix is to eliminate the element sheet completely.
        if (usesRemUnits)
            m_elementSheet->contents().parserSetUsesRemUnits();
        if (usesStyleBasedEditability)
            m_elementSheet->contents().parserSetUsesStyleBasedEditability();
    }

    if (!equalIgnoringFragmentIdentifier(oldBaseURL, m_baseURL)) {
        // Base URL change changes any relative visited links.
        // FIXME: There are other URLs in the tree that would need to be re-evaluated on dynamic base URL change. Style should be invalidated too.
        for (auto& anchor : descendantsOfType<HTMLAnchorElement>(*this))
            anchor.invalidateCachedVisitedLinkHash();
    }
}

void Document::setBaseURLOverride(const URL& url)
{
    m_baseURLOverride = url;
    updateBaseURL();
}

void Document::processBaseElement()
{
    // Find the first href attribute in a base element and the first target attribute in a base element.
    const AtomicString* href = nullptr;
    const AtomicString* target = nullptr;
    auto baseDescendants = descendantsOfType<HTMLBaseElement>(*this);
    for (auto& base : baseDescendants) {
        if (!href) {
            const AtomicString& value = base.fastGetAttribute(hrefAttr);
            if (!value.isNull()) {
                href = &value;
                if (target)
                    break;
            }
        }
        if (!target) {
            const AtomicString& value = base.fastGetAttribute(targetAttr);
            if (!value.isNull()) {
                target = &value;
                if (href)
                    break;
            }
        }
    }

    // FIXME: Since this doesn't share code with completeURL it may not handle encodings correctly.
    URL baseElementURL;
    if (href) {
        String strippedHref = stripLeadingAndTrailingHTMLSpaces(*href);
        if (!strippedHref.isEmpty())
            baseElementURL = URL(url(), strippedHref);
    }
    if (m_baseElementURL != baseElementURL && contentSecurityPolicy()->allowBaseURI(baseElementURL)) {
        m_baseElementURL = baseElementURL;
        updateBaseURL();
    }

    m_baseTarget = target ? *target : nullAtom;
}

String Document::userAgent(const URL& url) const
{
    return frame() ? frame()->loader().userAgent(url) : String();
}

void Document::disableEval(const String& errorMessage)
{
    if (!frame())
        return;

    frame()->script().disableEval(errorMessage);
}

bool Document::canNavigate(Frame* targetFrame)
{
    if (!m_frame)
        return false;

    // FIXME: We shouldn't call this function without a target frame, but
    // fast/forms/submit-to-blank-multiple-times.html depends on this function
    // returning true when supplied with a 0 targetFrame.
    if (!targetFrame)
        return true;

    // Frame-busting is generally allowed, but blocked for sandboxed frames lacking the 'allow-top-navigation' flag.
    if (!isSandboxed(SandboxTopNavigation) && targetFrame == &m_frame->tree().top())
        return true;

    if (isSandboxed(SandboxNavigation)) {
        if (targetFrame->tree().isDescendantOf(m_frame))
            return true;

        const char* reason = "The frame attempting navigation is sandboxed, and is therefore disallowed from navigating its ancestors.";
        if (isSandboxed(SandboxTopNavigation) && targetFrame == &m_frame->tree().top())
            reason = "The frame attempting navigation of the top-level window is sandboxed, but the 'allow-top-navigation' flag is not set.";

        printNavigationErrorMessage(targetFrame, url(), reason);
        return false;
    }

    // This is the normal case. A document can navigate its decendant frames,
    // or, more generally, a document can navigate a frame if the document is
    // in the same origin as any of that frame's ancestors (in the frame
    // hierarchy).
    //
    // See http://www.adambarth.com/papers/2008/barth-jackson-mitchell.pdf for
    // historical information about this security check.
    if (canAccessAncestor(securityOrigin(), targetFrame))
        return true;

    // Top-level frames are easier to navigate than other frames because they
    // display their URLs in the address bar (in most browsers). However, there
    // are still some restrictions on navigation to avoid nuisance attacks.
    // Specifically, a document can navigate a top-level frame if that frame
    // opened the document or if the document is the same-origin with any of
    // the top-level frame's opener's ancestors (in the frame hierarchy).
    //
    // In both of these cases, the document performing the navigation is in
    // some way related to the frame being navigate (e.g., by the "opener"
    // and/or "parent" relation). Requiring some sort of relation prevents a
    // document from navigating arbitrary, unrelated top-level frames.
    if (!targetFrame->tree().parent()) {
        if (targetFrame == m_frame->loader().opener())
            return true;

        if (canAccessAncestor(securityOrigin(), targetFrame->loader().opener()))
            return true;
    }

    printNavigationErrorMessage(targetFrame, url(), "The frame attempting navigation is neither same-origin with the target, nor is it the target's parent or opener.");
    return false;
}

Frame* Document::findUnsafeParentScrollPropagationBoundary()
{
    Frame* currentFrame = m_frame;
    if (!currentFrame)
        return nullptr;

    Frame* ancestorFrame = currentFrame->tree().parent();

    while (ancestorFrame) {
        if (!ancestorFrame->document()->securityOrigin()->canAccess(securityOrigin()))
            return currentFrame;
        currentFrame = ancestorFrame;
        ancestorFrame = ancestorFrame->tree().parent();
    }
    return nullptr;
}

void Document::didRemoveAllPendingStylesheet()
{
    m_needsNotifyRemoveAllPendingStylesheet = false;

    styleResolverChanged(DeferRecalcStyleIfNeeded);

    if (m_pendingSheetLayout == DidLayoutWithPendingSheets) {
        m_pendingSheetLayout = IgnoreLayoutWithPendingSheets;
        if (renderView())
            renderView()->repaintViewAndCompositedLayers();
    }

    if (ScriptableDocumentParser* parser = scriptableDocumentParser())
        parser->executeScriptsWaitingForStylesheets();

    if (m_gotoAnchorNeededAfterStylesheetsLoad && view())
        view()->scrollToFragment(m_url);
}

CSSStyleSheet& Document::elementSheet()
{
    if (!m_elementSheet)
        m_elementSheet = CSSStyleSheet::createInline(*this, m_baseURL);
    return *m_elementSheet;
}

bool Document::usesStyleBasedEditability() const
{
    if (m_elementSheet && m_elementSheet->contents().usesStyleBasedEditability())
        return true;

    ASSERT(!m_renderView || !m_renderView->frameView().isPainting());
    ASSERT(!m_inStyleRecalc);

    auto& authorSheets = const_cast<AuthorStyleSheets&>(authorStyleSheets());
    authorSheets.flushPendingUpdates();
    return authorSheets.usesStyleBasedEditability();
}

void Document::processHttpEquiv(const String& equiv, const String& content)
{
    ASSERT(!equiv.isNull() && !content.isNull());

    HttpEquivPolicy policy = httpEquivPolicy();
    if (policy != HttpEquivPolicy::Enabled) {
        String reason;
        switch (policy) {
        case HttpEquivPolicy::Enabled:
            ASSERT_NOT_REACHED();
            break;
        case HttpEquivPolicy::DisabledBySettings:
            reason = "by the embedder.";
            break;
        case HttpEquivPolicy::DisabledByContentDispositionAttachmentSandbox:
            reason = "for documents with Content-Disposition: attachment.";
            break;
        }
        String message = "http-equiv '" + equiv + "' is disabled " + reason;
        addConsoleMessage(MessageSource::Security, MessageLevel::Error, message);
        return;
    }

    Frame* frame = this->frame();

    HTTPHeaderName headerName;
    if (!findHTTPHeaderName(equiv, headerName))
        return;

    switch (headerName) {
    case HTTPHeaderName::DefaultStyle:
        // The preferred style set has been overridden as per section
        // 14.3.2 of the HTML4.0 specification.  We need to update the
        // sheet used variable and then update our style selector.
        // For more info, see the test at:
        // http://www.hixie.ch/tests/evil/css/import/main/preferred.html
        // -dwh
        authorStyleSheets().setSelectedStylesheetSetName(content);
        authorStyleSheets().setPreferredStylesheetSetName(content);
        styleResolverChanged(DeferRecalcStyle);
        break;

    case HTTPHeaderName::Refresh: {
        double delay;
        String urlString;
        if (frame && parseHTTPRefresh(content, true, delay, urlString)) {
            URL completedURL;
            if (urlString.isEmpty())
                completedURL = m_url;
            else
                completedURL = completeURL(urlString);
            if (!protocolIsJavaScript(completedURL))
                frame->navigationScheduler().scheduleRedirect(this, delay, completedURL);
            else {
                String message = "Refused to refresh " + m_url.stringCenterEllipsizedToLength() + " to a javascript: URL";
                addConsoleMessage(MessageSource::Security, MessageLevel::Error, message);
            }
        }

        break;
    }

    case HTTPHeaderName::SetCookie:
        // FIXME: make setCookie work on XML documents too; e.g. in case of <html:meta .....>
        if (is<HTMLDocument>(*this)) {
            // Exception (for sandboxed documents) ignored.
            downcast<HTMLDocument>(*this).setCookie(content, IGNORE_EXCEPTION);
        }
        break;

    case HTTPHeaderName::ContentLanguage:
        setContentLanguage(content);
        break;

    case HTTPHeaderName::XDNSPrefetchControl:
        parseDNSPrefetchControlHeader(content);
        break;

    case HTTPHeaderName::XFrameOptions:
        if (frame) {
            FrameLoader& frameLoader = frame->loader();
            unsigned long requestIdentifier = 0;
            if (frameLoader.activeDocumentLoader() && frameLoader.activeDocumentLoader()->mainResourceLoader())
                requestIdentifier = frameLoader.activeDocumentLoader()->mainResourceLoader()->identifier();
            if (frameLoader.shouldInterruptLoadForXFrameOptions(content, url(), requestIdentifier)) {
                String message = "Refused to display '" + url().stringCenterEllipsizedToLength() + "' in a frame because it set 'X-Frame-Options' to '" + content + "'.";
                frameLoader.stopAllLoaders();
                // Stopping the loader isn't enough, as we're already parsing the document; to honor the header's
                // intent, we must navigate away from the possibly partially-rendered document to a location that
                // doesn't inherit the parent's SecurityOrigin.
                frame->navigationScheduler().scheduleLocationChange(this, securityOrigin(), SecurityOrigin::urlWithUniqueSecurityOrigin(), String());
                addConsoleMessage(MessageSource::Security, MessageLevel::Error, message, requestIdentifier);
            }
        }
        break;

    case HTTPHeaderName::ContentSecurityPolicy:
        contentSecurityPolicy()->processHTTPEquiv(content, ContentSecurityPolicyHeaderType::Enforce);
        break;

    case HTTPHeaderName::ContentSecurityPolicyReportOnly:
        contentSecurityPolicy()->processHTTPEquiv(content, ContentSecurityPolicyHeaderType::Report);
        break;

    case HTTPHeaderName::XWebKitCSP:
        contentSecurityPolicy()->processHTTPEquiv(content, ContentSecurityPolicyHeaderType::PrefixedEnforce);
        break;

    case HTTPHeaderName::XWebKitCSPReportOnly:
        contentSecurityPolicy()->processHTTPEquiv(content, ContentSecurityPolicyHeaderType::PrefixedReport);
        break;

    default:
        break;
    }
}

void Document::processViewport(const String& features, ViewportArguments::Type origin)
{
    ASSERT(!features.isNull());

    if (origin < m_viewportArguments.type)
        return;

    m_viewportArguments = ViewportArguments(origin);

    processFeaturesString(features, [this](StringView key, StringView value) {
        setViewportFeature(m_viewportArguments, *this, key, value);
    });

    updateViewportArguments();
}

void Document::updateViewportArguments()
{
    if (page() && frame()->isMainFrame()) {
#ifndef NDEBUG
        m_didDispatchViewportPropertiesChanged = true;
#endif
        page()->chrome().dispatchViewportPropertiesDidChange(m_viewportArguments);
#if PLATFORM(IOS)
        page()->chrome().didReceiveDocType(frame());
#endif
    }
}

#if PLATFORM(IOS)

void Document::processFormatDetection(const String& features)
{
    // FIXME: Find a better place for this function.
    processFeaturesString(features, [this](StringView key, StringView value) {
        if (equalLettersIgnoringASCIICase(key, "telephone") && equalLettersIgnoringASCIICase(value, "no"))
            setIsTelephoneNumberParsingAllowed(false);
    });
}

void Document::processWebAppOrientations()
{
    if (Page* page = this->page())
        page->chrome().client().webAppOrientationsUpdated();
}

#endif

void Document::processReferrerPolicy(const String& policy)
{
    ASSERT(!policy.isNull());

    // Documents in a Content-Disposition: attachment sandbox should never send a Referer header,
    // even if the document has a meta tag saying otherwise.
    if (shouldEnforceContentDispositionAttachmentSandbox())
        return;

    // Note that we're supporting both the standard and legacy keywords for referrer
    // policies, as defined by http://www.w3.org/TR/referrer-policy/#referrer-policy-delivery-meta
    if (equalLettersIgnoringASCIICase(policy, "no-referrer") || equalLettersIgnoringASCIICase(policy, "never"))
        setReferrerPolicy(ReferrerPolicyNever);
    else if (equalLettersIgnoringASCIICase(policy, "unsafe-url") || equalLettersIgnoringASCIICase(policy, "always"))
        setReferrerPolicy(ReferrerPolicyAlways);
    else if (equalLettersIgnoringASCIICase(policy, "origin"))
        setReferrerPolicy(ReferrerPolicyOrigin);
    else if (equalLettersIgnoringASCIICase(policy, "no-referrer-when-downgrade") || equalLettersIgnoringASCIICase(policy, "default"))
        setReferrerPolicy(ReferrerPolicyDefault);
    else {
        addConsoleMessage(MessageSource::Rendering, MessageLevel::Error, "Failed to set referrer policy: The value '" + policy + "' is not one of 'no-referrer', 'origin', 'no-referrer-when-downgrade', or 'unsafe-url'. Defaulting to 'no-referrer'.");
        setReferrerPolicy(ReferrerPolicyNever);
    }
}

MouseEventWithHitTestResults Document::prepareMouseEvent(const HitTestRequest& request, const LayoutPoint& documentPoint, const PlatformMouseEvent& event)
{
    if (!hasLivingRenderTree())
        return MouseEventWithHitTestResults(event, HitTestResult(LayoutPoint()));

    HitTestResult result(documentPoint);
    renderView()->hitTest(request, result);

    if (!request.readOnly())
        updateHoverActiveState(request, result.innerElement());

    return MouseEventWithHitTestResults(event, result);
}

// DOM Section 1.1.1
bool Document::childTypeAllowed(NodeType type) const
{
    switch (type) {
    case ATTRIBUTE_NODE:
    case CDATA_SECTION_NODE:
    case DOCUMENT_FRAGMENT_NODE:
    case DOCUMENT_NODE:
    case TEXT_NODE:
        return false;
    case COMMENT_NODE:
    case PROCESSING_INSTRUCTION_NODE:
        return true;
    case DOCUMENT_TYPE_NODE:
    case ELEMENT_NODE:
        // Documents may contain no more than one of each of these.
        // (One Element and one DocumentType.)
        for (Node* c = firstChild(); c; c = c->nextSibling())
            if (c->nodeType() == type)
                return false;
        return true;
    }
    return false;
}

bool Document::canAcceptChild(const Node& newChild, const Node* refChild, AcceptChildOperation operation) const
{
    if (operation == AcceptChildOperation::Replace && refChild->nodeType() == newChild.nodeType())
        return true;

    switch (newChild.nodeType()) {
    case ATTRIBUTE_NODE:
    case CDATA_SECTION_NODE:
    case DOCUMENT_NODE:
    case TEXT_NODE:
        return false;
    case COMMENT_NODE:
    case PROCESSING_INSTRUCTION_NODE:
        return true;
    case DOCUMENT_FRAGMENT_NODE: {
        bool hasSeenElementChild = false;
        for (auto* node = downcast<DocumentFragment>(newChild).firstChild(); node; node = node->nextSibling()) {
            if (is<Element>(*node)) {
                if (hasSeenElementChild)
                    return false;
                hasSeenElementChild = true;
            }
            if (!canAcceptChild(*node, refChild, operation))
                return false;
        }
        break;
    }
    case DOCUMENT_TYPE_NODE: {
        auto* existingDocType = childrenOfType<DocumentType>(*this).first();
        if (operation == AcceptChildOperation::Replace) {
            //  parent has a doctype child that is not child, or an element is preceding child.
            if (existingDocType && existingDocType != refChild)
                return false;
            if (refChild->previousElementSibling())
                return false;
        } else {
            ASSERT(operation == AcceptChildOperation::InsertOrAdd);
            if (existingDocType)
                return false;
            if ((refChild && refChild->previousElementSibling()) || (!refChild && firstElementChild()))
                return false;
        }
        break;
    }
    case ELEMENT_NODE: {
        auto* existingElementChild = firstElementChild();
        if (operation == AcceptChildOperation::Replace) {
            if (existingElementChild && existingElementChild != refChild)
                return false;
            for (auto* child = refChild->nextSibling(); child; child = child->nextSibling()) {
                if (is<DocumentType>(*child))
                    return false;
            }
        } else {
            ASSERT(operation == AcceptChildOperation::InsertOrAdd);
            if (existingElementChild)
                return false;
            for (auto* child = refChild; child; child = child->nextSibling()) {
                if (is<DocumentType>(*child))
                    return false;
            }
        }
        break;
    }
    }
    return true;
}

Ref<Node> Document::cloneNodeInternal(Document&, CloningOperation type)
{
    Ref<Document> clone = cloneDocumentWithoutChildren();
    clone->cloneDataFromDocument(*this);
    switch (type) {
    case CloningOperation::OnlySelf:
    case CloningOperation::SelfWithTemplateContent:
        break;
    case CloningOperation::Everything:
        cloneChildNodes(clone);
        break;
    }
    return WTFMove(clone);
}

Ref<Document> Document::cloneDocumentWithoutChildren() const
{
    if (isXMLDocument()) {
        if (isXHTMLDocument())
            return XMLDocument::createXHTML(nullptr, url());
        return XMLDocument::create(nullptr, url());
    }
    return create(nullptr, url());
}

void Document::cloneDataFromDocument(const Document& other)
{
    ASSERT(m_url == other.url());
    m_baseURL = other.baseURL();
    m_baseURLOverride = other.baseURLOverride();
    m_documentURI = other.documentURI();

    setCompatibilityMode(other.m_compatibilityMode);
    setSecurityOriginPolicy(other.securityOriginPolicy());
    overrideMIMEType(other.contentType());
    setDecoder(other.decoder());
}

StyleSheetList& Document::styleSheets()
{
    if (!m_styleSheetList)
        m_styleSheetList = StyleSheetList::create(this);
    return *m_styleSheetList;
}

String Document::preferredStylesheetSet() const
{
    return authorStyleSheets().preferredStylesheetSetName();
}

String Document::selectedStylesheetSet() const
{
    return authorStyleSheets().selectedStylesheetSetName();
}

void Document::setSelectedStylesheetSet(const String& aString)
{
    authorStyleSheets().setSelectedStylesheetSetName(aString);
    styleResolverChanged(DeferRecalcStyle);
}

void Document::evaluateMediaQueryList()
{
    if (m_mediaQueryMatcher)
        m_mediaQueryMatcher->styleResolverChanged();
    
    checkViewportDependentPictures();
}

void Document::checkViewportDependentPictures()
{
    Vector<HTMLPictureElement*, 16> changedPictures;
    HashSet<HTMLPictureElement*>::iterator end = m_viewportDependentPictures.end();
    for (HashSet<HTMLPictureElement*>::iterator it = m_viewportDependentPictures.begin(); it != end; ++it) {
        if ((*it)->viewportChangeAffectedPicture())
            changedPictures.append(*it);
    }
    for (auto* picture : changedPictures)
        picture->sourcesChanged();
}

void Document::optimizedStyleSheetUpdateTimerFired()
{
    styleResolverChanged(RecalcStyleIfNeeded);
}

void Document::scheduleOptimizedStyleSheetUpdate()
{
    if (m_optimizedStyleSheetUpdateTimer.isActive())
        return;
    authorStyleSheets().setPendingUpdateType(AuthorStyleSheets::OptimizedUpdate);
    m_optimizedStyleSheetUpdateTimer.startOneShot(0);
}

void Document::updateViewportUnitsOnResize()
{
    if (!hasStyleWithViewportUnits())
        return;

    ensureStyleResolver().clearCachedPropertiesAffectedByViewportUnits();

    // FIXME: Ideally, we should save the list of elements that have viewport units and only iterate over those.
    for (Element* element = ElementTraversal::firstWithin(rootNode()); element; element = ElementTraversal::nextIncludingPseudo(*element)) {
        auto* renderer = element->renderer();
        if (renderer && renderer->style().hasViewportUnits())
            element->setNeedsStyleRecalc(InlineStyleChange);
    }
}

void Document::addAudioProducer(MediaProducer* audioProducer)
{
    m_audioProducers.add(audioProducer);
    updateIsPlayingMedia();
}

void Document::removeAudioProducer(MediaProducer* audioProducer)
{
    m_audioProducers.remove(audioProducer);
    updateIsPlayingMedia();
}

void Document::updateIsPlayingMedia(uint64_t sourceElementID)
{
    MediaProducer::MediaStateFlags state = MediaProducer::IsNotPlaying;
    for (auto* audioProducer : m_audioProducers)
        state |= audioProducer->mediaState();

#if ENABLE(MEDIA_SESSION)
    if (HTMLMediaElement* sourceElement = HTMLMediaElement::elementWithID(sourceElementID)) {
        if (sourceElement->isPlaying())
            state |= MediaProducer::IsSourceElementPlaying;

        if (auto* session = sourceElement->session()) {
            if (auto* controls = session->controls()) {
                if (controls->previousTrackEnabled())
                    state |= MediaProducer::IsPreviousTrackControlEnabled;
                if (controls->nextTrackEnabled())
                    state |= MediaProducer::IsNextTrackControlEnabled;
            }
        }
    }
#endif

    if (state == m_mediaState)
        return;

    m_mediaState = state;

    if (page())
        page()->updateIsPlayingMedia(sourceElementID);
}

void Document::pageMutedStateDidChange()
{
    for (auto* audioProducer : m_audioProducers)
        audioProducer->pageMutedStateDidChange();
}

void Document::styleResolverChanged(StyleResolverUpdateFlag updateFlag)
{
    if (m_optimizedStyleSheetUpdateTimer.isActive())
        m_optimizedStyleSheetUpdateTimer.stop();

    // Don't bother updating, since we haven't loaded all our style info yet
    // and haven't calculated the style selector for the first time.
    if (!hasLivingRenderTree() || (!m_didCalculateStyleResolver && !haveStylesheetsLoaded())) {
        m_styleResolver = nullptr;
        return;
    }
    m_didCalculateStyleResolver = true;

#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (!ownerElement())
        printf("Beginning update of style selector at time %lld.\n", elapsedTime().count());
#endif

    auto styleSheetUpdate = (updateFlag == RecalcStyleIfNeeded || updateFlag == DeferRecalcStyleIfNeeded)
        ? AuthorStyleSheets::OptimizedUpdate
        : AuthorStyleSheets::FullUpdate;
    bool stylesheetChangeRequiresStyleRecalc = authorStyleSheets().updateActiveStyleSheets(styleSheetUpdate);

    if (updateFlag == DeferRecalcStyle) {
        scheduleForcedStyleRecalc();
        return;
    }

    if (updateFlag == DeferRecalcStyleIfNeeded) {
        if (stylesheetChangeRequiresStyleRecalc)
            scheduleForcedStyleRecalc();
        return;
    }

    if (!stylesheetChangeRequiresStyleRecalc)
        return;

    // This recalcStyle initiates a new recalc cycle. We need to bracket it to
    // make sure animations get the correct update time
    {
        AnimationUpdateBlock animationUpdateBlock(m_frame ? &m_frame->animation() : nullptr);
        recalcStyle(Style::Force);
    }

#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (!ownerElement())
        printf("Finished update of style selector at time %lld\n", elapsedTime().count());
#endif

    if (renderView()) {
        renderView()->setNeedsLayoutAndPrefWidthsRecalc();
        if (view())
            view()->scheduleRelayout();
    }

    evaluateMediaQueryList();
}

void Document::removeFocusedNodeOfSubtree(Node* node, bool amongChildrenOnly)
{
    if (!m_focusedElement || this->inPageCache()) // If the document is in the page cache, then we don't need to clear out the focused node.
        return;

    Element* focusedElement = node->treeScope().focusedElement();
    if (!focusedElement)
        return;

    bool nodeInSubtree = false;
    if (amongChildrenOnly)
        nodeInSubtree = focusedElement->isDescendantOf(node);
    else
        nodeInSubtree = (focusedElement == node) || focusedElement->isDescendantOf(node);
    
    if (nodeInSubtree)
        setFocusedElement(nullptr);
}

void Document::hoveredElementDidDetach(Element* element)
{
    if (!m_hoveredElement || element != m_hoveredElement)
        return;

    m_hoveredElement = element->parentElement();
    while (m_hoveredElement && !m_hoveredElement->renderer())
        m_hoveredElement = m_hoveredElement->parentElement();
    if (frame())
        frame()->eventHandler().scheduleHoverStateUpdate();
}

void Document::elementInActiveChainDidDetach(Element* element)
{
    if (!m_activeElement || element != m_activeElement)
        return;

    m_activeElement = element->parentElement();
    while (m_activeElement && !m_activeElement->renderer())
        m_activeElement = m_activeElement->parentElement();
}

#if ENABLE(DASHBOARD_SUPPORT)
const Vector<AnnotatedRegionValue>& Document::annotatedRegions() const
{
    return m_annotatedRegions;
}

void Document::setAnnotatedRegions(const Vector<AnnotatedRegionValue>& regions)
{
    m_annotatedRegions = regions;
    setAnnotatedRegionsDirty(false);
}
#endif

bool Document::setFocusedElement(Element* element, FocusDirection direction)
{
    RefPtr<Element> newFocusedElement = element;
    // Make sure newFocusedElement is actually in this document
    if (newFocusedElement && (&newFocusedElement->document() != this))
        return true;

    if (m_focusedElement == newFocusedElement)
        return true;

    if (m_inPageCache)
        return false;

    bool focusChangeBlocked = false;
    RefPtr<Element> oldFocusedElement = m_focusedElement.release();

    // Remove focus from the existing focus node (if any)
    if (oldFocusedElement) {
        if (oldFocusedElement->active())
            oldFocusedElement->setActive(false);

        oldFocusedElement->setFocus(false);

        // Dispatch a change event for form control elements that have been edited.
        if (is<HTMLFormControlElement>(*oldFocusedElement)) {
            HTMLFormControlElement& formControlElement = downcast<HTMLFormControlElement>(*oldFocusedElement);
            if (formControlElement.wasChangedSinceLastFormControlChangeEvent())
                formControlElement.dispatchFormControlChangeEvent();
        }

        // Dispatch the blur event and let the node do any other blur related activities (important for text fields)
        oldFocusedElement->dispatchBlurEvent(newFocusedElement.copyRef());

        if (m_focusedElement) {
            // handler shifted focus
            focusChangeBlocked = true;
            newFocusedElement = nullptr;
        }
        
        oldFocusedElement->dispatchFocusOutEvent(eventNames().focusoutEvent, newFocusedElement.copyRef()); // DOM level 3 name for the bubbling blur event.
        // FIXME: We should remove firing DOMFocusOutEvent event when we are sure no content depends
        // on it, probably when <rdar://problem/8503958> is resolved.
        oldFocusedElement->dispatchFocusOutEvent(eventNames().DOMFocusOutEvent, newFocusedElement.copyRef()); // DOM level 2 name for compatibility.

        if (m_focusedElement) {
            // handler shifted focus
            focusChangeBlocked = true;
            newFocusedElement = nullptr;
        }
            
        if (oldFocusedElement->isRootEditableElement())
            frame()->editor().didEndEditing();

        if (view()) {
            if (Widget* oldWidget = widgetForElement(oldFocusedElement.get()))
                oldWidget->setFocus(false);
            else
                view()->setFocus(false);
        }
    }

    if (newFocusedElement && newFocusedElement->isFocusable()) {
        if (newFocusedElement->isRootEditableElement() && !acceptsEditingFocus(newFocusedElement.get())) {
            // delegate blocks focus change
            focusChangeBlocked = true;
            goto SetFocusedNodeDone;
        }
        // Set focus on the new node
        m_focusedElement = newFocusedElement;

        // Dispatch the focus event and let the node do any other focus related activities (important for text fields)
        m_focusedElement->dispatchFocusEvent(oldFocusedElement.copyRef(), direction);

        if (m_focusedElement != newFocusedElement) {
            // handler shifted focus
            focusChangeBlocked = true;
            goto SetFocusedNodeDone;
        }

        m_focusedElement->dispatchFocusInEvent(eventNames().focusinEvent, oldFocusedElement.copyRef()); // DOM level 3 bubbling focus event.

        if (m_focusedElement != newFocusedElement) {
            // handler shifted focus
            focusChangeBlocked = true;
            goto SetFocusedNodeDone;
        }

        // FIXME: We should remove firing DOMFocusInEvent event when we are sure no content depends
        // on it, probably when <rdar://problem/8503958> is m.
        m_focusedElement->dispatchFocusInEvent(eventNames().DOMFocusInEvent, oldFocusedElement.copyRef()); // DOM level 2 for compatibility.

        if (m_focusedElement != newFocusedElement) {
            // handler shifted focus
            focusChangeBlocked = true;
            goto SetFocusedNodeDone;
        }

        m_focusedElement->setFocus(true);

        if (m_focusedElement->isRootEditableElement())
            frame()->editor().didBeginEditing();

        // eww, I suck. set the qt focus correctly
        // ### find a better place in the code for this
        if (view()) {
            Widget* focusWidget = widgetForElement(m_focusedElement.get());
            if (focusWidget) {
                // Make sure a widget has the right size before giving it focus.
                // Otherwise, we are testing edge cases of the Widget code.
                // Specifically, in WebCore this does not work well for text fields.
                updateLayout();
                // Re-get the widget in case updating the layout changed things.
                focusWidget = widgetForElement(m_focusedElement.get());
            }
            if (focusWidget)
                focusWidget->setFocus(true);
            else
                view()->setFocus(true);
        }
    }

    if (!focusChangeBlocked && m_focusedElement) {
        // Create the AXObject cache in a focus change because GTK relies on it.
        if (AXObjectCache* cache = axObjectCache())
            cache->handleFocusedUIElementChanged(oldFocusedElement.get(), newFocusedElement.get());
    }

    if (!focusChangeBlocked && page())
        page()->chrome().focusedElementChanged(m_focusedElement.get());

SetFocusedNodeDone:
    updateStyleIfNeeded();
    return !focusChangeBlocked;
}

void Document::setCSSTarget(Element* n)
{
    if (m_cssTarget)
        m_cssTarget->setNeedsStyleRecalc();
    m_cssTarget = n;
    if (n)
        n->setNeedsStyleRecalc();
}

void Document::registerNodeListForInvalidation(LiveNodeList& list)
{
    m_nodeListAndCollectionCounts[list.invalidationType()]++;
    if (!list.isRootedAtDocument())
        return;
    ASSERT(!list.isRegisteredForInvalidationAtDocument());
    list.setRegisteredForInvalidationAtDocument(true);
    m_listsInvalidatedAtDocument.add(&list);
}

void Document::unregisterNodeListForInvalidation(LiveNodeList& list)
{
    m_nodeListAndCollectionCounts[list.invalidationType()]--;
    if (!list.isRegisteredForInvalidationAtDocument())
        return;

    list.setRegisteredForInvalidationAtDocument(false);
    ASSERT(m_inInvalidateNodeListAndCollectionCaches
        ? m_listsInvalidatedAtDocument.isEmpty()
        : m_listsInvalidatedAtDocument.contains(&list));
    m_listsInvalidatedAtDocument.remove(&list);
}

void Document::registerCollection(HTMLCollection& collection)
{
    m_nodeListAndCollectionCounts[collection.invalidationType()]++;
    if (collection.isRootedAtDocument())
        m_collectionsInvalidatedAtDocument.add(&collection);
}

void Document::unregisterCollection(HTMLCollection& collection)
{
    ASSERT(m_nodeListAndCollectionCounts[collection.invalidationType()]);
    m_nodeListAndCollectionCounts[collection.invalidationType()]--;
    if (!collection.isRootedAtDocument())
        return;

    m_collectionsInvalidatedAtDocument.remove(&collection);
}

void Document::collectionCachedIdNameMap(const HTMLCollection& collection)
{
    ASSERT_UNUSED(collection, collection.hasNamedElementCache());
    m_nodeListAndCollectionCounts[InvalidateOnIdNameAttrChange]++;
}

void Document::collectionWillClearIdNameMap(const HTMLCollection& collection)
{
    ASSERT_UNUSED(collection, collection.hasNamedElementCache());
    ASSERT(m_nodeListAndCollectionCounts[InvalidateOnIdNameAttrChange]);
    m_nodeListAndCollectionCounts[InvalidateOnIdNameAttrChange]--;
}

void Document::attachNodeIterator(NodeIterator* ni)
{
    m_nodeIterators.add(ni);
}

void Document::detachNodeIterator(NodeIterator* ni)
{
    // The node iterator can be detached without having been attached if its root node didn't have a document
    // when the iterator was created, but has it now.
    m_nodeIterators.remove(ni);
}

void Document::moveNodeIteratorsToNewDocument(Node* node, Document* newDocument)
{
    Vector<NodeIterator*> nodeIterators;
    copyToVector(m_nodeIterators, nodeIterators);
    for (auto* it : nodeIterators) {
        if (it->root() == node) {
            detachNodeIterator(it);
            newDocument->attachNodeIterator(it);
        }
    }
}

void Document::updateRangesAfterChildrenChanged(ContainerNode& container)
{
    for (auto* range : m_ranges)
        range->nodeChildrenChanged(container);
}

void Document::nodeChildrenWillBeRemoved(ContainerNode& container)
{
    for (auto* range : m_ranges)
        range->nodeChildrenWillBeRemoved(container);

    for (auto* it : m_nodeIterators) {
        for (Node* n = container.firstChild(); n; n = n->nextSibling())
            it->nodeWillBeRemoved(*n);
    }

    if (Frame* frame = this->frame()) {
        for (Node* n = container.firstChild(); n; n = n->nextSibling()) {
            frame->eventHandler().nodeWillBeRemoved(*n);
            frame->selection().nodeWillBeRemoved(*n);
            frame->page()->dragCaretController().nodeWillBeRemoved(*n);
        }
    }

    if (m_markers->hasMarkers()) {
        for (Text* textNode = TextNodeTraversal::firstChild(container); textNode; textNode = TextNodeTraversal::nextSibling(*textNode))
            m_markers->removeMarkers(textNode);
    }
}

void Document::nodeWillBeRemoved(Node& n)
{
    for (auto* it : m_nodeIterators)
        it->nodeWillBeRemoved(n);

    for (auto* range : m_ranges)
        range->nodeWillBeRemoved(n);

    if (Frame* frame = this->frame()) {
        frame->eventHandler().nodeWillBeRemoved(n);
        frame->selection().nodeWillBeRemoved(n);
        frame->page()->dragCaretController().nodeWillBeRemoved(n);
    }

    if (is<Text>(n))
        m_markers->removeMarkers(&n);
}

void Document::textInserted(Node* text, unsigned offset, unsigned length)
{
    if (!m_ranges.isEmpty()) {
        for (auto* range : m_ranges)
            range->textInserted(text, offset, length);
    }

    // Update the markers for spelling and grammar checking.
    m_markers->shiftMarkers(text, offset, length);
}

void Document::textRemoved(Node* text, unsigned offset, unsigned length)
{
    if (!m_ranges.isEmpty()) {
        for (auto* range : m_ranges)
            range->textRemoved(text, offset, length);
    }

    // Update the markers for spelling and grammar checking.
    m_markers->removeMarkers(text, offset, length);
    m_markers->shiftMarkers(text, offset + length, 0 - length);
}

void Document::textNodesMerged(Text* oldNode, unsigned offset)
{
    if (!m_ranges.isEmpty()) {
        NodeWithIndex oldNodeWithIndex(oldNode);
        for (auto* range : m_ranges)
            range->textNodesMerged(oldNodeWithIndex, offset);
    }

    // FIXME: This should update markers for spelling and grammar checking.
}

void Document::textNodeSplit(Text* oldNode)
{
    for (auto* range : m_ranges)
        range->textNodeSplit(oldNode);

    // FIXME: This should update markers for spelling and grammar checking.
}

void Document::createDOMWindow()
{
    ASSERT(m_frame);
    ASSERT(!m_domWindow);

    m_domWindow = DOMWindow::create(this);

    ASSERT(m_domWindow->document() == this);
    ASSERT(m_domWindow->frame() == m_frame);
}

void Document::takeDOMWindowFrom(Document* document)
{
    ASSERT(m_frame);
    ASSERT(!m_domWindow);
    ASSERT(document->m_domWindow);
    // A valid DOMWindow is needed by CachedFrame for its documents.
    ASSERT(!document->inPageCache());

    m_domWindow = document->m_domWindow.release();
    m_domWindow->didSecureTransitionTo(this);

    ASSERT(m_domWindow->document() == this);
    ASSERT(m_domWindow->frame() == m_frame);
}

void Document::setAttributeEventListener(const AtomicString& eventType, const QualifiedName& attributeName, const AtomicString& attributeValue)
{
    setAttributeEventListener(eventType, JSLazyEventListener::create(*this, attributeName, attributeValue));
}

void Document::setWindowAttributeEventListener(const AtomicString& eventType, PassRefPtr<EventListener> listener)
{
    if (!m_domWindow)
        return;
    m_domWindow->setAttributeEventListener(eventType, listener);
}

void Document::setWindowAttributeEventListener(const AtomicString& eventType, const QualifiedName& attributeName, const AtomicString& attributeValue)
{
    if (!m_domWindow)
        return;
    setWindowAttributeEventListener(eventType, JSLazyEventListener::create(*m_domWindow, attributeName, attributeValue));
}

EventListener* Document::getWindowAttributeEventListener(const AtomicString& eventType)
{
    if (!m_domWindow)
        return nullptr;
    return m_domWindow->getAttributeEventListener(eventType);
}

void Document::dispatchWindowEvent(Event& event, EventTarget* target)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!NoEventDispatchAssertion::isEventDispatchForbidden());
    if (!m_domWindow)
        return;
    m_domWindow->dispatchEvent(event, target);
}

void Document::dispatchWindowLoadEvent()
{
    ASSERT_WITH_SECURITY_IMPLICATION(!NoEventDispatchAssertion::isEventDispatchForbidden());
    if (!m_domWindow)
        return;
    m_domWindow->dispatchLoadEvent();
    m_loadEventFinished = true;
    m_cachedResourceLoader->documentDidFinishLoadEvent();
}

void Document::enqueueWindowEvent(Ref<Event>&& event)
{
    event->setTarget(m_domWindow.get());
    m_eventQueue.enqueueEvent(WTFMove(event));
}

void Document::enqueueDocumentEvent(Ref<Event>&& event)
{
    event->setTarget(this);
    m_eventQueue.enqueueEvent(WTFMove(event));
}

void Document::enqueueOverflowEvent(Ref<Event>&& event)
{
    m_eventQueue.enqueueEvent(WTFMove(event));
}

void Document::enqueueSlotchangeEvent(Ref<Event>&& event)
{
    m_eventQueue.enqueueEvent(WTFMove(event));
}

RefPtr<Event> Document::createEvent(const String& type, ExceptionCode& ec)
{
    // Please do *not* add new event classes to this function unless they are
    // required for compatibility of some actual legacy web content.

    // This mechanism is superceded by use of event constructors.
    // That is what we should use for any new event classes.

    // The following strings are the ones from the DOM specification
    // <https://dom.spec.whatwg.org/#dom-document-createevent>.

    if (equalLettersIgnoringASCIICase(type, "customevent"))
        return CustomEvent::createForBindings();
    if (equalLettersIgnoringASCIICase(type, "event") || equalLettersIgnoringASCIICase(type, "events") || equalLettersIgnoringASCIICase(type, "htmlevents"))
        return Event::createForBindings();
    if (equalLettersIgnoringASCIICase(type, "keyboardevent") || equalLettersIgnoringASCIICase(type, "keyboardevents"))
        return KeyboardEvent::createForBindings();
    if (equalLettersIgnoringASCIICase(type, "messageevent"))
        return MessageEvent::createForBindings();
    if (equalLettersIgnoringASCIICase(type, "mouseevent") || equalLettersIgnoringASCIICase(type, "mouseevents"))
        return MouseEvent::createForBindings();
    if (equalLettersIgnoringASCIICase(type, "uievent") || equalLettersIgnoringASCIICase(type, "uievents"))
        return UIEvent::createForBindings();

#if ENABLE(TOUCH_EVENTS)
    if (equalLettersIgnoringASCIICase(type, "touchevent"))
        return TouchEvent::createForBindings();
#endif

    // The following string comes from the SVG specifications
    // <http://www.w3.org/TR/SVG/script.html#InterfaceSVGZoomEvent>
    // <http://www.w3.org/TR/SVG2/interact.html#InterfaceSVGZoomEvent>.
    // However, since there is no provision for initializing the event once it is created,
    // there is no practical value in this feature.

    if (equalLettersIgnoringASCIICase(type, "svgzoomevents"))
        return SVGZoomEvent::createForBindings();

    // The following strings are for event classes where WebKit supplies an init function.
    // These strings are not part of the DOM specification and we would like to eliminate them.
    // However, we currently include these because we have concerns about backward compatibility.

    // FIXME: For each of the strings below, prove there is no content depending on it and remove
    // both the string and the corresponding init function for that class.

    if (equalLettersIgnoringASCIICase(type, "compositionevent"))
        return CompositionEvent::createForBindings();
    if (equalLettersIgnoringASCIICase(type, "hashchangeevent"))
        return HashChangeEvent::createForBindings();
    if (equalLettersIgnoringASCIICase(type, "mutationevent") || equalLettersIgnoringASCIICase(type, "mutationevents"))
        return MutationEvent::createForBindings();
    if (equalLettersIgnoringASCIICase(type, "overflowevent"))
        return OverflowEvent::createForBindings();
    if (equalLettersIgnoringASCIICase(type, "storageevent"))
        return StorageEvent::createForBindings();
    if (equalLettersIgnoringASCIICase(type, "textevent"))
        return TextEvent::createForBindings();
    if (equalLettersIgnoringASCIICase(type, "wheelevent"))
        return WheelEvent::createForBindings();

#if ENABLE(DEVICE_ORIENTATION)
    if (equalLettersIgnoringASCIICase(type, "devicemotionevent"))
        return DeviceMotionEvent::createForBindings();
    if (equalLettersIgnoringASCIICase(type, "deviceorientationevent"))
        return DeviceOrientationEvent::createForBindings();
#endif

    ec = NOT_SUPPORTED_ERR;
    return nullptr;
}

bool Document::hasListenerTypeForEventType(PlatformEvent::Type eventType) const
{
    switch (eventType) {
    case PlatformEvent::MouseForceChanged:
        return m_listenerTypes & Document::FORCECHANGED_LISTENER;
    case PlatformEvent::MouseForceDown:
        return m_listenerTypes & Document::FORCEDOWN_LISTENER;
    case PlatformEvent::MouseForceUp:
        return m_listenerTypes & Document::FORCEUP_LISTENER;
    case PlatformEvent::MouseScroll:
        return m_listenerTypes & Document::SCROLL_LISTENER;
    default:
        return false;
    }
}

void Document::addListenerTypeIfNeeded(const AtomicString& eventType)
{
    if (eventType == eventNames().DOMSubtreeModifiedEvent)
        addListenerType(DOMSUBTREEMODIFIED_LISTENER);
    else if (eventType == eventNames().DOMNodeInsertedEvent)
        addListenerType(DOMNODEINSERTED_LISTENER);
    else if (eventType == eventNames().DOMNodeRemovedEvent)
        addListenerType(DOMNODEREMOVED_LISTENER);
    else if (eventType == eventNames().DOMNodeRemovedFromDocumentEvent)
        addListenerType(DOMNODEREMOVEDFROMDOCUMENT_LISTENER);
    else if (eventType == eventNames().DOMNodeInsertedIntoDocumentEvent)
        addListenerType(DOMNODEINSERTEDINTODOCUMENT_LISTENER);
    else if (eventType == eventNames().DOMCharacterDataModifiedEvent)
        addListenerType(DOMCHARACTERDATAMODIFIED_LISTENER);
    else if (eventType == eventNames().overflowchangedEvent)
        addListenerType(OVERFLOWCHANGED_LISTENER);
    else if (eventType == eventNames().webkitAnimationStartEvent || eventType == eventNames().animationstartEvent)
        addListenerType(ANIMATIONSTART_LISTENER);
    else if (eventType == eventNames().webkitAnimationEndEvent || eventType == eventNames().animationendEvent)
        addListenerType(ANIMATIONEND_LISTENER);
    else if (eventType == eventNames().webkitAnimationIterationEvent || eventType == eventNames().animationiterationEvent)
        addListenerType(ANIMATIONITERATION_LISTENER);
    else if (eventType == eventNames().webkitTransitionEndEvent || eventType == eventNames().transitionendEvent)
        addListenerType(TRANSITIONEND_LISTENER);
    else if (eventType == eventNames().beforeloadEvent)
        addListenerType(BEFORELOAD_LISTENER);
    else if (eventType == eventNames().scrollEvent)
        addListenerType(SCROLL_LISTENER);
    else if (eventType == eventNames().webkitmouseforcewillbeginEvent)
        addListenerType(FORCEWILLBEGIN_LISTENER);
    else if (eventType == eventNames().webkitmouseforcechangedEvent)
        addListenerType(FORCECHANGED_LISTENER);
    else if (eventType == eventNames().webkitmouseforcedownEvent)
        addListenerType(FORCEDOWN_LISTENER);
    else if (eventType == eventNames().webkitmouseforceupEvent)
        addListenerType(FORCEUP_LISTENER);
}

CSSStyleDeclaration* Document::getOverrideStyle(Element*, const String&)
{
    return nullptr;
}

HTMLFrameOwnerElement* Document::ownerElement() const
{
    if (!frame())
        return nullptr;
    return frame()->ownerElement();
}

String Document::cookie(ExceptionCode& ec)
{
    if (page() && !page()->settings().cookieEnabled())
        return String();

    // FIXME: The HTML5 DOM spec states that this attribute can raise an
    // INVALID_STATE_ERR exception on getting if the Document has no
    // browsing context.

    if (!securityOrigin()->canAccessCookies()) {
        ec = SECURITY_ERR;
        return String();
    }

    URL cookieURL = this->cookieURL();
    if (cookieURL.isEmpty())
        return String();

    if (!isDOMCookieCacheValid())
        setCachedDOMCookies(cookies(this, cookieURL));

    return cachedDOMCookies();
}

void Document::setCookie(const String& value, ExceptionCode& ec)
{
    if (page() && !page()->settings().cookieEnabled())
        return;

    // FIXME: The HTML5 DOM spec states that this attribute can raise an
    // INVALID_STATE_ERR exception on setting if the Document has no
    // browsing context.

    if (!securityOrigin()->canAccessCookies()) {
        ec = SECURITY_ERR;
        return;
    }

    URL cookieURL = this->cookieURL();
    if (cookieURL.isEmpty())
        return;

    invalidateDOMCookieCache();
    setCookies(this, cookieURL, value);
}

String Document::referrer() const
{
    if (frame())
        return frame()->loader().referrer();
    return String();
}

String Document::origin() const
{
    return securityOrigin()->databaseIdentifier();
}

String Document::domain() const
{
    return securityOrigin()->domain();
}

void Document::setDomain(const String& newDomain, ExceptionCode& ec)
{
    if (SchemeRegistry::isDomainRelaxationForbiddenForURLScheme(securityOrigin()->protocol())) {
        ec = SECURITY_ERR;
        return;
    }

    // Both NS and IE specify that changing the domain is only allowed when
    // the new domain is a suffix of the old domain.

    // FIXME: We should add logging indicating why a domain was not allowed.

    // If the new domain is the same as the old domain, still call
    // securityOrigin()->setDomainForDOM. This will change the
    // security check behavior. For example, if a page loaded on port 8000
    // assigns its current domain using document.domain, the page will
    // allow other pages loaded on different ports in the same domain that
    // have also assigned to access this page.
    if (equalIgnoringASCIICase(domain(), newDomain)) {
        securityOrigin()->setDomainFromDOM(newDomain);
        return;
    }

    int oldLength = domain().length();
    int newLength = newDomain.length();
    // e.g. newDomain = webkit.org (10) and domain() = www.webkit.org (14)
    if (newLength >= oldLength) {
        ec = SECURITY_ERR;
        return;
    }

    String test = domain();
    // Check that it's a subdomain, not e.g. "ebkit.org"
    if (test[oldLength - newLength - 1] != '.') {
        ec = SECURITY_ERR;
        return;
    }

    // Now test is "webkit.org" from domain()
    // and we check that it's the same thing as newDomain
    test.remove(0, oldLength - newLength);
    if (test != newDomain) {
        ec = SECURITY_ERR;
        return;
    }

    securityOrigin()->setDomainFromDOM(newDomain);
}

// http://www.whatwg.org/specs/web-apps/current-work/#dom-document-lastmodified
String Document::lastModified() const
{
    using namespace std::chrono;
    Optional<system_clock::time_point> dateTime;
    if (m_frame && loader())
        dateTime = loader()->response().lastModified();

    // FIXME: If this document came from the file system, the HTML5
    // specification tells us to read the last modification date from the file
    // system.
    if (!dateTime) {
        dateTime = system_clock::now();
#if ENABLE(WEB_REPLAY)
        InputCursor& cursor = inputCursor();
        if (cursor.isCapturing())
            cursor.appendInput<DocumentLastModifiedDate>(duration_cast<milliseconds>(dateTime.value().time_since_epoch()).count());
        else if (cursor.isReplaying()) {
            if (DocumentLastModifiedDate* input = cursor.fetchInput<DocumentLastModifiedDate>())
                dateTime = system_clock::time_point(milliseconds(static_cast<long long>(input->fallbackValue())));
        }
#endif
    }

    auto ctime = system_clock::to_time_t(dateTime.value());
    auto localDateTime = std::localtime(&ctime);
    return String::format("%02d/%02d/%04d %02d:%02d:%02d", localDateTime->tm_mon + 1, localDateTime->tm_mday, 1900 + localDateTime->tm_year, localDateTime->tm_hour, localDateTime->tm_min, localDateTime->tm_sec);
}

void Document::setCookieURL(const URL& url)
{
    if (m_cookieURL == url)
        return;
    m_cookieURL = url;
    invalidateDOMCookieCache();
}

static bool isValidNameNonASCII(const LChar* characters, unsigned length)
{
    if (!isValidNameStart(characters[0]))
        return false;

    for (unsigned i = 1; i < length; ++i) {
        if (!isValidNamePart(characters[i]))
            return false;
    }

    return true;
}

static bool isValidNameNonASCII(const UChar* characters, unsigned length)
{
    unsigned i = 0;

    UChar32 c;
    U16_NEXT(characters, i, length, c)
    if (!isValidNameStart(c))
        return false;

    while (i < length) {
        U16_NEXT(characters, i, length, c)
        if (!isValidNamePart(c))
            return false;
    }

    return true;
}

template<typename CharType>
static inline bool isValidNameASCII(const CharType* characters, unsigned length)
{
    CharType c = characters[0];
    if (!(isASCIIAlpha(c) || c == ':' || c == '_'))
        return false;

    for (unsigned i = 1; i < length; ++i) {
        c = characters[i];
        if (!(isASCIIAlphanumeric(c) || c == ':' || c == '_' || c == '-' || c == '.'))
            return false;
    }

    return true;
}

bool Document::isValidName(const String& name)
{
    unsigned length = name.length();
    if (!length)
        return false;

    if (name.is8Bit()) {
        const LChar* characters = name.characters8();

        if (isValidNameASCII(characters, length))
            return true;

        return isValidNameNonASCII(characters, length);
    }

    const UChar* characters = name.characters16();

    if (isValidNameASCII(characters, length))
        return true;

    return isValidNameNonASCII(characters, length);
}

bool Document::parseQualifiedName(const String& qualifiedName, String& prefix, String& localName, ExceptionCode& ec)
{
    unsigned length = qualifiedName.length();

    if (!length) {
        ec = INVALID_CHARACTER_ERR;
        return false;
    }

    bool nameStart = true;
    bool sawColon = false;
    int colonPos = 0;

    for (unsigned i = 0; i < length;) {
        UChar32 c;
        U16_NEXT(qualifiedName, i, length, c)
        if (c == ':') {
            if (sawColon) {
                ec = NAMESPACE_ERR;
                return false; // multiple colons: not allowed
            }
            nameStart = true;
            sawColon = true;
            colonPos = i - 1;
        } else if (nameStart) {
            if (!isValidNameStart(c)) {
                ec = INVALID_CHARACTER_ERR;
                return false;
            }
            nameStart = false;
        } else {
            if (!isValidNamePart(c)) {
                ec = INVALID_CHARACTER_ERR;
                return false;
            }
        }
    }

    if (!sawColon) {
        prefix = String();
        localName = qualifiedName;
    } else {
        prefix = qualifiedName.substring(0, colonPos);
        if (prefix.isEmpty()) {
            ec = NAMESPACE_ERR;
            return false;
        }
        localName = qualifiedName.substring(colonPos + 1);
    }

    if (localName.isEmpty()) {
        ec = NAMESPACE_ERR;
        return false;
    }

    return true;
}

void Document::setDecoder(RefPtr<TextResourceDecoder>&& decoder)
{
    m_decoder = WTFMove(decoder);
}

URL Document::completeURL(const String& url, const URL& baseURLOverride) const
{
    // Always return a null URL when passed a null string.
    // FIXME: Should we change the URL constructor to have this behavior?
    // See also [CSS]StyleSheet::completeURL(const String&)
    if (url.isNull())
        return URL();
    const URL& baseURL = ((baseURLOverride.isEmpty() || baseURLOverride == blankURL()) && parentDocument()) ? parentDocument()->baseURL() : baseURLOverride;
    if (!m_decoder)
        return URL(baseURL, url);
    return URL(baseURL, url, m_decoder->encoding());
}

URL Document::completeURL(const String& url) const
{
    return completeURL(url, m_baseURL);
}

void Document::setInPageCache(bool flag)
{
    if (m_inPageCache == flag)
        return;

    m_inPageCache = flag;

    FrameView* v = view();
    Page* page = this->page();

    if (flag) {
        if (v) {
            // FIXME: There is some scrolling related work that needs to happen whenever a page goes into the
            // page cache and similar work that needs to occur when it comes out. This is where we do the work
            // that needs to happen when we enter, and the work that needs to happen when we exit is in
            // HistoryController::restoreScrollPositionAndViewState(). It can't be here because this function is
            // called too early on in the process of a page exiting the cache for that work to be possible in this
            // function. It would be nice if there was more symmetry here.
            // https://bugs.webkit.org/show_bug.cgi?id=98698
            v->cacheCurrentScrollPosition();
            if (page && m_frame->isMainFrame()) {
                v->resetScrollbarsAndClearContentsSize();
                if (ScrollingCoordinator* scrollingCoordinator = page->scrollingCoordinator())
                    scrollingCoordinator->clearStateTree();
            } else
                v->resetScrollbars();
        }
        m_styleRecalcTimer.stop();

        clearStyleResolver();
        clearSelectorQueryCache();
        clearSharedObjectPool();
    } else {
        if (childNeedsStyleRecalc())
            scheduleStyleRecalc();
    }
}

void Document::documentWillBecomeInactive()
{
    if (renderView())
        renderView()->setIsInWindow(false);
}

void Document::suspend(ActiveDOMObject::ReasonForSuspension reason)
{
    if (m_isSuspended)
        return;

    documentWillBecomeInactive();

    for (auto* element : m_documentSuspensionCallbackElements)
        element->prepareForDocumentSuspension();

#ifndef NDEBUG
    // Clear the update flag to be able to check if the viewport arguments update
    // is dispatched, after the document is restored from the page cache.
    m_didDispatchViewportPropertiesChanged = false;
#endif

    ASSERT(page());
    page()->lockAllOverlayScrollbarsToHidden(true);

    if (RenderView* view = renderView()) {
        if (view->usesCompositing())
            view->compositor().cancelCompositingLayerUpdate();
    }

    suspendScriptedAnimationControllerCallbacks();
    suspendActiveDOMObjects(reason);

    ASSERT(m_frame);
    m_frame->clearTimers();

    m_visualUpdatesAllowed = false;
    m_visualUpdatesSuppressionTimer.stop();

    m_isSuspended = true;
}

void Document::resume(ActiveDOMObject::ReasonForSuspension reason)
{
    if (!m_isSuspended)
        return;

    Vector<Element*> elements;
    copyToVector(m_documentSuspensionCallbackElements, elements);
    for (auto* element : elements)
        element->resumeFromDocumentSuspension();

    if (renderView())
        renderView()->setIsInWindow(true);

    ASSERT(page());
    page()->lockAllOverlayScrollbarsToHidden(false);

    ASSERT(m_frame);
    m_frame->loader().client().dispatchDidBecomeFrameset(isFrameSet());
    m_frame->animation().resumeAnimationsForDocument(this);

    resumeActiveDOMObjects(reason);
    resumeScriptedAnimationControllerCallbacks();

    m_visualUpdatesAllowed = true;

    m_isSuspended = false;
}

void Document::registerForDocumentSuspensionCallbacks(Element* e)
{
    m_documentSuspensionCallbackElements.add(e);
}

void Document::unregisterForDocumentSuspensionCallbacks(Element* e)
{
    m_documentSuspensionCallbackElements.remove(e);
}

void Document::mediaVolumeDidChange() 
{
    for (auto* element : m_mediaVolumeCallbackElements)
        element->mediaVolumeDidChange();
}

void Document::registerForMediaVolumeCallbacks(Element* e)
{
    m_mediaVolumeCallbackElements.add(e);
}

void Document::unregisterForMediaVolumeCallbacks(Element* e)
{
    m_mediaVolumeCallbackElements.remove(e);
}

void Document::storageBlockingStateDidChange()
{
    if (Settings* settings = this->settings())
        securityOrigin()->setStorageBlockingPolicy(settings->storageBlockingPolicy());
}

void Document::privateBrowsingStateDidChange() 
{
    for (auto* element : m_privateBrowsingStateChangedElements)
        element->privateBrowsingStateDidChange();
}

void Document::registerForPrivateBrowsingStateChangedCallbacks(Element* e)
{
    m_privateBrowsingStateChangedElements.add(e);
}

void Document::unregisterForPrivateBrowsingStateChangedCallbacks(Element* e)
{
    m_privateBrowsingStateChangedElements.remove(e);
}

#if ENABLE(VIDEO_TRACK)
void Document::registerForCaptionPreferencesChangedCallbacks(Element* e)
{
    if (page())
        page()->group().captionPreferences().setInterestedInCaptionPreferenceChanges();

    m_captionPreferencesChangedElements.add(e);
}

void Document::unregisterForCaptionPreferencesChangedCallbacks(Element* e)
{
    m_captionPreferencesChangedElements.remove(e);
}

void Document::captionPreferencesChanged()
{
    for (auto* element : m_captionPreferencesChangedElements)
        element->captionPreferencesChanged();
}
#endif

#if ENABLE(MEDIA_CONTROLS_SCRIPT)
void Document::registerForPageScaleFactorChangedCallbacks(HTMLMediaElement* element)
{
    m_pageScaleFactorChangedElements.add(element);
}

void Document::unregisterForPageScaleFactorChangedCallbacks(HTMLMediaElement* element)
{
    m_pageScaleFactorChangedElements.remove(element);
}

void Document::pageScaleFactorChangedAndStable()
{
    for (HTMLMediaElement* mediaElement : m_pageScaleFactorChangedElements)
        mediaElement->pageScaleFactorChanged();
}
#endif

void Document::setShouldCreateRenderers(bool f)
{
    m_createRenderers = f;
}

bool Document::shouldCreateRenderers()
{
    return m_createRenderers;
}

// Support for Javascript execCommand, and related methods

static Editor::Command command(Document* document, const String& commandName, bool userInterface = false)
{
    Frame* frame = document->frame();
    if (!frame || frame->document() != document)
        return Editor::Command();

    document->updateStyleIfNeeded();

    return frame->editor().command(commandName,
        userInterface ? CommandFromDOMWithUserInterface : CommandFromDOM);
}

bool Document::execCommand(const String& commandName, bool userInterface, const String& value)
{
    EventQueueScope eventQueueScope;
    return command(this, commandName, userInterface).execute(value);
}

bool Document::queryCommandEnabled(const String& commandName)
{
    return command(this, commandName).isEnabled();
}

bool Document::queryCommandIndeterm(const String& commandName)
{
    return command(this, commandName).state() == MixedTriState;
}

bool Document::queryCommandState(const String& commandName)
{
    return command(this, commandName).state() == TrueTriState;
}

bool Document::queryCommandSupported(const String& commandName)
{
    return command(this, commandName).isSupported();
}

String Document::queryCommandValue(const String& commandName)
{
    return command(this, commandName).value();
}

void Document::pushCurrentScript(HTMLScriptElement* newCurrentScript)
{
    ASSERT(newCurrentScript);
    m_currentScriptStack.append(newCurrentScript);
}

void Document::popCurrentScript()
{
    ASSERT(!m_currentScriptStack.isEmpty());
    m_currentScriptStack.removeLast();
}

#if ENABLE(XSLT)

void Document::applyXSLTransform(ProcessingInstruction* pi)
{
    RefPtr<XSLTProcessor> processor = XSLTProcessor::create();
    processor->setXSLStyleSheet(downcast<XSLStyleSheet>(pi->sheet()));
    String resultMIMEType;
    String newSource;
    String resultEncoding;
    if (!processor->transformToString(*this, resultMIMEType, newSource, resultEncoding))
        return;
    // FIXME: If the transform failed we should probably report an error (like Mozilla does).
    Frame* ownerFrame = frame();
    processor->createDocumentFromSource(newSource, resultEncoding, resultMIMEType, this, ownerFrame);
}

void Document::setTransformSource(std::unique_ptr<TransformSource> source)
{
    m_transformSource = WTFMove(source);
}

#endif

void Document::setDesignMode(InheritedBool value)
{
    m_designMode = value;
    for (Frame* frame = m_frame; frame && frame->document(); frame = frame->tree().traverseNext(m_frame))
        frame->document()->scheduleForcedStyleRecalc();
}

Document::InheritedBool Document::getDesignMode() const
{
    return m_designMode;
}

bool Document::inDesignMode() const
{
    for (const Document* d = this; d; d = d->parentDocument()) {
        if (d->m_designMode != inherit)
            return d->m_designMode;
    }
    return false;
}

Document* Document::parentDocument() const
{
    if (!m_frame)
        return nullptr;
    Frame* parent = m_frame->tree().parent();
    if (!parent)
        return nullptr;
    return parent->document();
}

Document& Document::topDocument() const
{
    // FIXME: This special-casing avoids incorrectly determined top documents during the process
    // of AXObjectCache teardown or notification posting for cached or being-destroyed documents.
    if (!m_inPageCache && !m_renderTreeBeingDestroyed) {
        if (!m_frame)
            return const_cast<Document&>(*this);
        // This should always be non-null.
        Document* mainFrameDocument = m_frame->mainFrame().document();
        return mainFrameDocument ? *mainFrameDocument : const_cast<Document&>(*this);
    }

    Document* document = const_cast<Document*>(this);
    while (HTMLFrameOwnerElement* element = document->ownerElement())
        document = &element->document();
    return *document;
}

RefPtr<Attr> Document::createAttribute(const String& name, ExceptionCode& ec)
{
    return createAttributeNS(String(), isHTMLDocument() ? name.convertToASCIILowercase() : name, ec, true);
}

RefPtr<Attr> Document::createAttributeNS(const String& namespaceURI, const String& qualifiedName, ExceptionCode& ec, bool shouldIgnoreNamespaceChecks)
{
    String prefix, localName;
    if (!parseQualifiedName(qualifiedName, prefix, localName, ec))
        return nullptr;

    QualifiedName qName(prefix, localName, namespaceURI);

    if (!shouldIgnoreNamespaceChecks && !hasValidNamespaceForAttributes(qName)) {
        ec = NAMESPACE_ERR;
        return nullptr;
    }

    return Attr::create(*this, qName, emptyString());
}

const SVGDocumentExtensions* Document::svgExtensions()
{
    return m_svgExtensions.get();
}

SVGDocumentExtensions& Document::accessSVGExtensions()
{
    if (!m_svgExtensions)
        m_svgExtensions = std::make_unique<SVGDocumentExtensions>(this);
    return *m_svgExtensions;
}

bool Document::hasSVGRootNode() const
{
    return documentElement() && documentElement()->hasTagName(SVGNames::svgTag);
}

template <CollectionType collectionType>
Ref<HTMLCollection> Document::ensureCachedCollection()
{
    return ensureRareData().ensureNodeLists().addCachedCollection<GenericCachedHTMLCollection<CollectionTypeTraits<collectionType>::traversalType>>(*this, collectionType);
}

Ref<HTMLCollection> Document::images()
{
    return ensureCachedCollection<DocImages>();
}

Ref<HTMLCollection> Document::applets()
{
    return ensureCachedCollection<DocApplets>();
}

Ref<HTMLCollection> Document::embeds()
{
    return ensureCachedCollection<DocEmbeds>();
}

Ref<HTMLCollection> Document::plugins()
{
    // This is an alias for embeds() required for the JS DOM bindings.
    return ensureCachedCollection<DocEmbeds>();
}

Ref<HTMLCollection> Document::scripts()
{
    return ensureCachedCollection<DocScripts>();
}

Ref<HTMLCollection> Document::links()
{
    return ensureCachedCollection<DocLinks>();
}

Ref<HTMLCollection> Document::forms()
{
    return ensureCachedCollection<DocForms>();
}

Ref<HTMLCollection> Document::anchors()
{
    return ensureCachedCollection<DocAnchors>();
}

Ref<HTMLCollection> Document::all()
{
    return ensureRareData().ensureNodeLists().addCachedCollection<HTMLAllCollection>(*this, DocAll);
}

Ref<HTMLCollection> Document::windowNamedItems(const AtomicString& name)
{
    return ensureRareData().ensureNodeLists().addCachedCollection<WindowNameCollection>(*this, WindowNamedItems, name);
}

Ref<HTMLCollection> Document::documentNamedItems(const AtomicString& name)
{
    return ensureRareData().ensureNodeLists().addCachedCollection<DocumentNameCollection>(*this, DocumentNamedItems, name);
}

void Document::finishedParsing()
{
    ASSERT(!scriptableDocumentParser() || !m_parser->isParsing());
    ASSERT(!scriptableDocumentParser() || m_readyState != Loading);
    setParsing(false);

#if ENABLE(WEB_TIMING)
    if (!m_documentTiming.domContentLoadedEventStart)
        m_documentTiming.domContentLoadedEventStart = monotonicallyIncreasingTime();
#endif

    dispatchEvent(Event::create(eventNames().DOMContentLoadedEvent, true, false));

#if ENABLE(WEB_TIMING)
    if (!m_documentTiming.domContentLoadedEventEnd)
        m_documentTiming.domContentLoadedEventEnd = monotonicallyIncreasingTime();
#endif

    if (RefPtr<Frame> f = frame()) {
        // FrameLoader::finishedParsing() might end up calling Document::implicitClose() if all
        // resource loads are complete. HTMLObjectElements can start loading their resources from
        // post attach callbacks triggered by recalcStyle().  This means if we parse out an <object>
        // tag and then reach the end of the document without updating styles, we might not have yet
        // started the resource load and might fire the window load event too early.  To avoid this
        // we force the styles to be up to date before calling FrameLoader::finishedParsing().
        // See https://bugs.webkit.org/show_bug.cgi?id=36864 starting around comment 35.
        updateStyleIfNeeded();

        f->loader().finishedParsing();

        InspectorInstrumentation::domContentLoadedEventFired(*f);
    }

    // Schedule dropping of the DocumentSharedObjectPool. We keep it alive for a while after parsing finishes
    // so that dynamically inserted content can also benefit from sharing optimizations.
    // Note that we don't refresh the timer on pool access since that could lead to huge caches being kept
    // alive indefinitely by something innocuous like JS setting .innerHTML repeatedly on a timer.
    static const int timeToKeepSharedObjectPoolAliveAfterParsingFinishedInSeconds = 10;
    m_sharedObjectPoolClearTimer.startOneShot(timeToKeepSharedObjectPoolAliveAfterParsingFinishedInSeconds);

    // Parser should have picked up all preloads by now
    m_cachedResourceLoader->clearPreloads();
}

void Document::clearSharedObjectPool()
{
    m_sharedObjectPool = nullptr;
    m_sharedObjectPoolClearTimer.stop();
}

#if ENABLE(TELEPHONE_NUMBER_DETECTION)
// FIXME: Find a better place for this functionality.
bool Document::isTelephoneNumberParsingEnabled() const
{
    Settings* settings = this->settings();
    return settings && settings->telephoneNumberParsingEnabled() && m_isTelephoneNumberParsingAllowed;
}

void Document::setIsTelephoneNumberParsingAllowed(bool isTelephoneNumberParsingAllowed)
{
    m_isTelephoneNumberParsingAllowed = isTelephoneNumberParsingAllowed;
}

bool Document::isTelephoneNumberParsingAllowed() const
{
    return m_isTelephoneNumberParsingAllowed;
}
#endif

RefPtr<XPathExpression> Document::createExpression(const String& expression,
                                                       XPathNSResolver* resolver,
                                                       ExceptionCode& ec)
{
    if (!m_xpathEvaluator)
        m_xpathEvaluator = XPathEvaluator::create();
    return m_xpathEvaluator->createExpression(expression, resolver, ec);
}

RefPtr<XPathNSResolver> Document::createNSResolver(Node* nodeResolver)
{
    if (!m_xpathEvaluator)
        m_xpathEvaluator = XPathEvaluator::create();
    return m_xpathEvaluator->createNSResolver(nodeResolver);
}

RefPtr<XPathResult> Document::evaluate(const String& expression, Node* contextNode, XPathNSResolver* resolver, unsigned short type, XPathResult* result, ExceptionCode& ec)
{
    if (!m_xpathEvaluator)
        m_xpathEvaluator = XPathEvaluator::create();
    return m_xpathEvaluator->evaluate(expression, contextNode, resolver, type, result, ec);
}

void Document::initSecurityContext()
{
    if (haveInitializedSecurityOrigin()) {
        ASSERT(securityOrigin());
        return;
    }

    if (!m_frame) {
        // No source for a security context.
        // This can occur via document.implementation.createDocument().
        setCookieURL(URL(ParsedURLString, emptyString()));
        setSecurityOriginPolicy(SecurityOriginPolicy::create(SecurityOrigin::createUnique()));
        setContentSecurityPolicy(std::make_unique<ContentSecurityPolicy>(*this));
        return;
    }

    // In the common case, create the security context from the currently
    // loading URL with a fresh content security policy.
    setCookieURL(m_url);
    enforceSandboxFlags(m_frame->loader().effectiveSandboxFlags());

    if (shouldEnforceContentDispositionAttachmentSandbox())
        applyContentDispositionAttachmentSandbox();

    setSecurityOriginPolicy(SecurityOriginPolicy::create(isSandboxed(SandboxOrigin) ? SecurityOrigin::createUnique() : SecurityOrigin::create(m_url)));
    setContentSecurityPolicy(std::make_unique<ContentSecurityPolicy>(*this));

    if (Settings* settings = this->settings()) {
        if (!settings->webSecurityEnabled()) {
            // Web security is turned off. We should let this document access every other document. This is used primary by testing
            // harnesses for web sites.
            securityOrigin()->grantUniversalAccess();
        } else if (securityOrigin()->isLocal()) {
            if (settings->allowUniversalAccessFromFileURLs() || m_frame->loader().client().shouldForceUniversalAccessFromLocalURL(m_url)) {
                // Some clients want local URLs to have universal access, but that setting is dangerous for other clients.
                securityOrigin()->grantUniversalAccess();
            } else if (!settings->allowFileAccessFromFileURLs()) {
                // Some clients want local URLs to have even tighter restrictions by default, and not be able to access other local files.
                // FIXME 81578: The naming of this is confusing. Files with restricted access to other local files
                // still can have other privileges that can be remembered, thereby not making them unique origins.
                securityOrigin()->enforceFilePathSeparation();
            }
        }
        securityOrigin()->setStorageBlockingPolicy(settings->storageBlockingPolicy());
    }

    Document* parentDocument = ownerElement() ? &ownerElement()->document() : nullptr;
    if (parentDocument && m_frame->loader().shouldTreatURLAsSrcdocDocument(url())) {
        m_isSrcdocDocument = true;
        setBaseURLOverride(parentDocument->baseURL());
    }

    if (!shouldInheritSecurityOriginFromOwner(m_url))
        return;

    // If we do not obtain a meaningful origin from the URL, then we try to
    // find one via the frame hierarchy.

    Frame* ownerFrame = m_frame->tree().parent();
    if (!ownerFrame)
        ownerFrame = m_frame->loader().opener();

    if (!ownerFrame) {
        didFailToInitializeSecurityOrigin();
        return;
    }

    if (isSandboxed(SandboxOrigin)) {
        // If we're supposed to inherit our security origin from our owner,
        // but we're also sandboxed, the only thing we inherit is the ability
        // to load local resources. This lets about:blank iframes in file://
        // URL documents load images and other resources from the file system.
        if (ownerFrame->document()->securityOrigin()->canLoadLocalResources())
            securityOrigin()->grantLoadLocalResources();
        return;
    }

    setCookieURL(ownerFrame->document()->cookieURL());
    // We alias the SecurityOrigins to match Firefox, see Bug 15313
    // https://bugs.webkit.org/show_bug.cgi?id=15313
    setSecurityOriginPolicy(ownerFrame->document()->securityOriginPolicy());
}

void Document::initContentSecurityPolicy()
{
    if (!m_frame->tree().parent() || (!shouldInheritSecurityOriginFromOwner(m_url) && !isPluginDocument()))
        return;

    contentSecurityPolicy()->copyStateFrom(m_frame->tree().parent()->document()->contentSecurityPolicy());
}

bool Document::isContextThread() const
{
    return isMainThread();
}

void Document::updateURLForPushOrReplaceState(const URL& url)
{
    Frame* f = frame();
    if (!f)
        return;

    setURL(url);
    f->loader().setOutgoingReferrer(url);

    if (DocumentLoader* documentLoader = loader())
        documentLoader->replaceRequestURLForSameDocumentNavigation(url);
}

void Document::statePopped(PassRefPtr<SerializedScriptValue> stateObject)
{
    if (!frame())
        return;
    
    // Per step 11 of section 6.5.9 (history traversal) of the HTML5 spec, we 
    // defer firing of popstate until we're in the complete state.
    if (m_readyState == Complete)
        enqueuePopstateEvent(stateObject);
    else
        m_pendingStateObject = stateObject;
}

void Document::updateFocusAppearanceSoon(SelectionRestorationMode mode)
{
    m_updateFocusAppearanceRestoresSelection = mode;
    if (!m_updateFocusAppearanceTimer.isActive())
        m_updateFocusAppearanceTimer.startOneShot(0);
}

void Document::cancelFocusAppearanceUpdate()
{
    m_updateFocusAppearanceTimer.stop();
}

void Document::updateFocusAppearanceTimerFired()
{
    Element* element = focusedElement();
    if (!element)
        return;

    updateLayout();
    if (element->isFocusable())
        element->updateFocusAppearance(m_updateFocusAppearanceRestoresSelection);
}

void Document::attachRange(Range* range)
{
    ASSERT(!m_ranges.contains(range));
    m_ranges.add(range);
}

void Document::detachRange(Range* range)
{
    // We don't ASSERT m_ranges.contains(range) to allow us to call this
    // unconditionally to fix: https://bugs.webkit.org/show_bug.cgi?id=26044
    m_ranges.remove(range);
}

CanvasRenderingContext* Document::getCSSCanvasContext(const String& type, const String& name, int width, int height)
{
    HTMLCanvasElement* element = getCSSCanvasElement(name);
    if (!element)
        return nullptr;
    element->setSize(IntSize(width, height));
    return element->getContext(type);
}

HTMLCanvasElement* Document::getCSSCanvasElement(const String& name)
{
    RefPtr<HTMLCanvasElement>& element = m_cssCanvasElements.add(name, nullptr).iterator->value;
    if (!element)
        element = HTMLCanvasElement::create(*this);
    return element.get();
}

#if ENABLE(IOS_TEXT_AUTOSIZING)
void Document::addAutoSizingNode(Node* node, float candidateSize)
{
    TextAutoSizingKey key(&node->renderer()->style(), &document());
    TextAutoSizingMap::AddResult result = m_textAutoSizedNodes.add(key, nullptr);
    if (result.isNewEntry)
        result.iterator->value = TextAutoSizingValue::create();
    result.iterator->value->addNode(node, candidateSize);
}

void Document::validateAutoSizingNodes()
{
    Vector<TextAutoSizingKey> nodesForRemoval;
    for (auto& keyValuePair : m_textAutoSizedNodes) {
        TextAutoSizingValue* value = keyValuePair.value.get();
        // Update all the nodes in the collection to reflect the new
        // candidate size.
        if (!value)
            continue;

        value->adjustNodeSizes();
        if (!value->numNodes())
            nodesForRemoval.append(keyValuePair.key);
    }
    for (auto& key : nodesForRemoval)
        m_textAutoSizedNodes.remove(key);
}
    
void Document::resetAutoSizingNodes()
{
    for (auto& value : m_textAutoSizedNodes.values()) {
        if (value)
            value->reset();
    }
    m_textAutoSizedNodes.clear();
}

#endif // ENABLE(IOS_TEXT_AUTOSIZING)

void Document::initDNSPrefetch()
{
    Settings* settings = this->settings();

    m_haveExplicitlyDisabledDNSPrefetch = false;
    m_isDNSPrefetchEnabled = settings && settings->dnsPrefetchingEnabled() && securityOrigin()->protocol() == "http";

    // Inherit DNS prefetch opt-out from parent frame    
    if (Document* parent = parentDocument()) {
        if (!parent->isDNSPrefetchEnabled())
            m_isDNSPrefetchEnabled = false;
    }
}

void Document::parseDNSPrefetchControlHeader(const String& dnsPrefetchControl)
{
    if (equalLettersIgnoringASCIICase(dnsPrefetchControl, "on") && !m_haveExplicitlyDisabledDNSPrefetch) {
        m_isDNSPrefetchEnabled = true;
        return;
    }

    m_isDNSPrefetchEnabled = false;
    m_haveExplicitlyDisabledDNSPrefetch = true;
}

void Document::addConsoleMessage(MessageSource source, MessageLevel level, const String& message, unsigned long requestIdentifier)
{
    if (!isContextThread()) {
        postTask(AddConsoleMessageTask(source, level, StringCapture(message)));
        return;
    }

    if (Page* page = this->page())
        page->console().addMessage(source, level, message, requestIdentifier, this);
}

void Document::addMessage(MessageSource source, MessageLevel level, const String& message, const String& sourceURL, unsigned lineNumber, unsigned columnNumber, RefPtr<Inspector::ScriptCallStack>&& callStack, JSC::ExecState* state, unsigned long requestIdentifier)
{
    if (!isContextThread()) {
        postTask(AddConsoleMessageTask(source, level, StringCapture(message)));
        return;
    }

    if (Page* page = this->page())
        page->console().addMessage(source, level, message, sourceURL, lineNumber, columnNumber, WTFMove(callStack), state, requestIdentifier);
}

SecurityOrigin* Document::topOrigin() const
{
    return topDocument().securityOrigin();
}

void Document::postTask(Task task)
{
    Task* taskPtr = std::make_unique<Task>(WTFMove(task)).release();
    WeakPtr<Document> documentReference(m_weakFactory.createWeakPtr());

    callOnMainThread([=] {
        ASSERT(isMainThread());
        std::unique_ptr<Task> task(taskPtr);

        Document* document = documentReference.get();
        if (!document)
            return;

        Page* page = document->page();
        if ((page && page->defersLoading() && document->activeDOMObjectsAreSuspended()) || !document->m_pendingTasks.isEmpty())
            document->m_pendingTasks.append(WTFMove(*task.release()));
        else
            task->performTask(*document);
    });
}

void Document::pendingTasksTimerFired()
{
    Vector<Task> pendingTasks = WTFMove(m_pendingTasks);
    for (auto& task : pendingTasks)
        task.performTask(*this);
}

void Document::suspendScheduledTasks(ActiveDOMObject::ReasonForSuspension reason)
{
    if (m_scheduledTasksAreSuspended) {
        // A page may subsequently suspend DOM objects, say as part of handling a scroll or zoom gesture, after the
        // embedding client requested the page be suspended. We ignore such requests so long as the embedding client
        // requested the suspension first. See <rdar://problem/13754896> for more details.
        ASSERT(reasonForSuspendingActiveDOMObjects() == ActiveDOMObject::PageWillBeSuspended);
        return;
    }

    suspendScriptedAnimationControllerCallbacks();
    suspendActiveDOMObjects(reason);
    scriptRunner()->suspend();
    m_pendingTasksTimer.stop();

    // Deferring loading and suspending parser is necessary when we need to prevent re-entrant JavaScript execution
    // (e.g. while displaying an alert).
    // It is not currently possible to suspend parser unless loading is deferred, because new data arriving from network
    // will trigger parsing, and leave the scheduler in an inconsistent state where it doesn't know whether it's suspended or not.
    if (reason == ActiveDOMObject::WillDeferLoading && m_parser)
        m_parser->suspendScheduledTasks();

    m_scheduledTasksAreSuspended = true;
}

void Document::resumeScheduledTasks(ActiveDOMObject::ReasonForSuspension reason)
{
    if (reasonForSuspendingActiveDOMObjects() != reason)
        return;

    ASSERT(m_scheduledTasksAreSuspended);

    if (reason == ActiveDOMObject::WillDeferLoading && m_parser)
        m_parser->resumeScheduledTasks();
    if (!m_pendingTasks.isEmpty())
        m_pendingTasksTimer.startOneShot(0);
    scriptRunner()->resume();
    resumeActiveDOMObjects(reason);
    resumeScriptedAnimationControllerCallbacks();
    
    m_scheduledTasksAreSuspended = false;
}

void Document::suspendScriptedAnimationControllerCallbacks()
{
#if ENABLE(REQUEST_ANIMATION_FRAME)
    if (m_scriptedAnimationController)
        m_scriptedAnimationController->suspend();
#endif
}

void Document::resumeScriptedAnimationControllerCallbacks()
{
#if ENABLE(REQUEST_ANIMATION_FRAME)
    if (m_scriptedAnimationController)
        m_scriptedAnimationController->resume();
#endif
}

void Document::scriptedAnimationControllerSetThrottled(bool isThrottled)
{
#if ENABLE(REQUEST_ANIMATION_FRAME)
    if (m_scriptedAnimationController)
        m_scriptedAnimationController->setThrottled(isThrottled);
#else
    UNUSED_PARAM(isThrottled);
#endif
}

void Document::windowScreenDidChange(PlatformDisplayID displayID)
{
    UNUSED_PARAM(displayID);

#if ENABLE(REQUEST_ANIMATION_FRAME)
    if (m_scriptedAnimationController)
        m_scriptedAnimationController->windowScreenDidChange(displayID);
#endif

    if (RenderView* view = renderView()) {
        if (view->usesCompositing())
            view->compositor().windowScreenDidChange(displayID);
    }
}

String Document::displayStringModifiedByEncoding(const String& str) const
{
    if (m_decoder)
        return m_decoder->encoding().displayString(str.impl());
    return str;
}

RefPtr<StringImpl> Document::displayStringModifiedByEncoding(PassRefPtr<StringImpl> str) const
{
    if (m_decoder)
        return m_decoder->encoding().displayString(str);
    return str;
}

template <typename CharacterType>
void Document::displayBufferModifiedByEncodingInternal(CharacterType* buffer, unsigned len) const
{
    if (m_decoder)
        m_decoder->encoding().displayBuffer(buffer, len);
}

// Generate definitions for both character types
template void Document::displayBufferModifiedByEncodingInternal<LChar>(LChar*, unsigned) const;
template void Document::displayBufferModifiedByEncodingInternal<UChar>(UChar*, unsigned) const;

void Document::enqueuePageshowEvent(PageshowEventPersistence persisted)
{
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=36334 Pageshow event needs to fire asynchronously.
    dispatchWindowEvent(PageTransitionEvent::create(eventNames().pageshowEvent, persisted), this);
}

void Document::enqueueHashchangeEvent(const String& oldURL, const String& newURL)
{
    enqueueWindowEvent(HashChangeEvent::create(oldURL, newURL));
}

void Document::enqueuePopstateEvent(RefPtr<SerializedScriptValue>&& stateObject)
{
    dispatchWindowEvent(PopStateEvent::create(WTFMove(stateObject), m_domWindow ? m_domWindow->history() : nullptr));
}

void Document::addMediaCanStartListener(MediaCanStartListener* listener)
{
    ASSERT(!m_mediaCanStartListeners.contains(listener));
    m_mediaCanStartListeners.add(listener);
}

void Document::removeMediaCanStartListener(MediaCanStartListener* listener)
{
    ASSERT(m_mediaCanStartListeners.contains(listener));
    m_mediaCanStartListeners.remove(listener);
}

MediaCanStartListener* Document::takeAnyMediaCanStartListener()
{
    return m_mediaCanStartListeners.takeAny();
}

#if ENABLE(DEVICE_ORIENTATION) && PLATFORM(IOS)
DeviceMotionController* Document::deviceMotionController() const
{
    return m_deviceMotionController.get();
}

DeviceOrientationController* Document::deviceOrientationController() const
{
    return m_deviceOrientationController.get();
}
#endif

#if ENABLE(FULLSCREEN_API)
bool Document::fullScreenIsAllowedForElement(Element* element) const
{
    ASSERT(element);
    return isAttributeOnAllOwners(allowfullscreenAttr, webkitallowfullscreenAttr, element->document().ownerElement());
}

void Document::requestFullScreenForElement(Element* element, unsigned short flags, FullScreenCheckType checkType)
{
    // The Mozilla Full Screen API <https://wiki.mozilla.org/Gecko:FullScreenAPI> has different requirements
    // for full screen mode, and do not have the concept of a full screen element stack.
    bool inLegacyMozillaMode = (flags & Element::LEGACY_MOZILLA_REQUEST);

    do {
        if (!element)
            element = documentElement();
 
        // 1. If any of the following conditions are true, terminate these steps and queue a task to fire
        // an event named fullscreenerror with its bubbles attribute set to true on the context object's 
        // node document:

        // The context object is not in a document.
        if (!element->inDocument())
            break;

        // The context object's node document, or an ancestor browsing context's document does not have
        // the fullscreen enabled flag set.
        if (checkType == EnforceIFrameAllowFullScreenRequirement && !fullScreenIsAllowedForElement(element))
            break;

        // The context object's node document fullscreen element stack is not empty and its top element
        // is not an ancestor of the context object. (NOTE: Ignore this requirement if the request was
        // made via the legacy Mozilla-style API.)
        if (!m_fullScreenElementStack.isEmpty() && !m_fullScreenElementStack.last()->contains(element) && !inLegacyMozillaMode)
            break;

        // A descendant browsing context's document has a non-empty fullscreen element stack.
        bool descendentHasNonEmptyStack = false;
        for (Frame* descendant = frame() ? frame()->tree().traverseNext() : nullptr; descendant; descendant = descendant->tree().traverseNext()) {
            if (descendant->document()->webkitFullscreenElement()) {
                descendentHasNonEmptyStack = true;
                break;
            }
        }
        if (descendentHasNonEmptyStack && !inLegacyMozillaMode)
            break;

        // This algorithm is not allowed to show a pop-up:
        //   An algorithm is allowed to show a pop-up if, in the task in which the algorithm is running, either:
        //   - an activation behavior is currently being processed whose click event was trusted, or
        //   - the event listener for a trusted click event is being handled.
        if (!ScriptController::processingUserGesture())
            break;

        // There is a previously-established user preference, security risk, or platform limitation.
        if (!page() || !page()->settings().fullScreenEnabled())
            break;

        if (!page()->chrome().client().supportsFullScreenForElement(element, flags & Element::ALLOW_KEYBOARD_INPUT)) {
            // The new full screen API does not accept a "flags" parameter, so fall back to disallowing
            // keyboard input if the chrome client refuses to allow keyboard input.
            if (!inLegacyMozillaMode && flags & Element::ALLOW_KEYBOARD_INPUT) {
                flags &= ~Element::ALLOW_KEYBOARD_INPUT;
                if (!page()->chrome().client().supportsFullScreenForElement(element, false))
                    break;
            } else
                break;
        }

        // 2. Let doc be element's node document. (i.e. "this")
        Document* currentDoc = this;

        // 3. Let docs be all doc's ancestor browsing context's documents (if any) and doc.
        Deque<Document*> docs;

        do {
            docs.prepend(currentDoc);
            currentDoc = currentDoc->ownerElement() ? &currentDoc->ownerElement()->document() : nullptr;
        } while (currentDoc);

        // 4. For each document in docs, run these substeps:
        Deque<Document*>::iterator current = docs.begin(), following = docs.begin();

        do {
            ++following;

            // 1. Let following document be the document after document in docs, or null if there is no
            // such document.
            Document* currentDoc = *current;
            Document* followingDoc = following != docs.end() ? *following : nullptr;

            // 2. If following document is null, push context object on document's fullscreen element
            // stack, and queue a task to fire an event named fullscreenchange with its bubbles attribute
            // set to true on the document.
            if (!followingDoc) {
                currentDoc->pushFullscreenElementStack(element);
                addDocumentToFullScreenChangeEventQueue(currentDoc);
                continue;
            }

            // 3. Otherwise, if document's fullscreen element stack is either empty or its top element
            // is not following document's browsing context container,
            Element* topElement = currentDoc->webkitFullscreenElement();
            if (!topElement || topElement != followingDoc->ownerElement()) {
                // ...push following document's browsing context container on document's fullscreen element
                // stack, and queue a task to fire an event named fullscreenchange with its bubbles attribute
                // set to true on document.
                currentDoc->pushFullscreenElementStack(followingDoc->ownerElement());
                addDocumentToFullScreenChangeEventQueue(currentDoc);
                continue;
            }

            // 4. Otherwise, do nothing for this document. It stays the same.
        } while (++current != docs.end());

        // 5. Return, and run the remaining steps asynchronously.
        // 6. Optionally, perform some animation.
        m_areKeysEnabledInFullScreen = flags & Element::ALLOW_KEYBOARD_INPUT;
        page()->chrome().client().enterFullScreenForElement(element);

        // 7. Optionally, display a message indicating how the user can exit displaying the context object fullscreen.
        return;
    } while (0);

    m_fullScreenErrorEventTargetQueue.append(element ? element : documentElement());
    m_fullScreenChangeDelayTimer.startOneShot(0);
}

void Document::webkitCancelFullScreen()
{
    // The Mozilla "cancelFullScreen()" API behaves like the W3C "fully exit fullscreen" behavior, which
    // is defined as: 
    // "To fully exit fullscreen act as if the exitFullscreen() method was invoked on the top-level browsing
    // context's document and subsequently empty that document's fullscreen element stack."
    Document& topDocument = this->topDocument();
    if (!topDocument.webkitFullscreenElement())
        return;

    // To achieve that aim, remove all the elements from the top document's stack except for the first before
    // calling webkitExitFullscreen():
    Vector<RefPtr<Element>> replacementFullscreenElementStack;
    replacementFullscreenElementStack.append(topDocument.webkitFullscreenElement());
    topDocument.m_fullScreenElementStack.swap(replacementFullscreenElementStack);

    topDocument.webkitExitFullscreen();
}

void Document::webkitExitFullscreen()
{
    // The exitFullscreen() method must run these steps:
    
    // 1. Let doc be the context object. (i.e. "this")
    Document* currentDoc = this;

    // 2. If doc's fullscreen element stack is empty, terminate these steps.
    if (m_fullScreenElementStack.isEmpty())
        return;
    
    // 3. Let descendants be all the doc's descendant browsing context's documents with a non-empty fullscreen
    // element stack (if any), ordered so that the child of the doc is last and the document furthest
    // away from the doc is first.
    Deque<RefPtr<Document>> descendants;
    for (Frame* descendant = frame() ? frame()->tree().traverseNext() : nullptr; descendant; descendant = descendant->tree().traverseNext()) {
        if (descendant->document()->webkitFullscreenElement())
            descendants.prepend(descendant->document());
    }
        
    // 4. For each descendant in descendants, empty descendant's fullscreen element stack, and queue a
    // task to fire an event named fullscreenchange with its bubbles attribute set to true on descendant.
    for (auto& document : descendants) {
        document->clearFullscreenElementStack();
        addDocumentToFullScreenChangeEventQueue(document.get());
    }

    // 5. While doc is not null, run these substeps:
    Element* newTop = nullptr;
    while (currentDoc) {
        // 1. Pop the top element of doc's fullscreen element stack.
        currentDoc->popFullscreenElementStack();

        //    If doc's fullscreen element stack is non-empty and the element now at the top is either
        //    not in a document or its node document is not doc, repeat this substep.
        newTop = currentDoc->webkitFullscreenElement();
        if (newTop && (!newTop->inDocument() || &newTop->document() != currentDoc))
            continue;

        // 2. Queue a task to fire an event named fullscreenchange with its bubbles attribute set to true
        // on doc.
        addDocumentToFullScreenChangeEventQueue(currentDoc);

        // 3. If doc's fullscreen element stack is empty and doc's browsing context has a browsing context
        // container, set doc to that browsing context container's node document.
        if (!newTop && currentDoc->ownerElement()) {
            currentDoc = &currentDoc->ownerElement()->document();
            continue;
        }

        // 4. Otherwise, set doc to null.
        currentDoc = nullptr;
    }

    // 6. Return, and run the remaining steps asynchronously.
    // 7. Optionally, perform some animation.

    if (!page())
        return;

    // Only exit out of full screen window mode if there are no remaining elements in the 
    // full screen stack.
    if (!newTop) {
        page()->chrome().client().exitFullScreenForElement(m_fullScreenElement.get());
        return;
    }

    // Otherwise, notify the chrome of the new full screen element.
    page()->chrome().client().enterFullScreenForElement(newTop);
}

bool Document::webkitFullscreenEnabled() const
{
    // 4. The fullscreenEnabled attribute must return true if the context object and all ancestor
    // browsing context's documents have their fullscreen enabled flag set, or false otherwise.

    // Top-level browsing contexts are implied to have their allowFullScreen attribute set.
    return isAttributeOnAllOwners(allowfullscreenAttr, webkitallowfullscreenAttr, ownerElement());
}

static void unwrapFullScreenRenderer(RenderFullScreen* fullScreenRenderer, Element* fullScreenElement)
{
    if (!fullScreenRenderer)
        return;
    bool requiresRenderTreeRebuild;
    fullScreenRenderer->unwrapRenderer(requiresRenderTreeRebuild);

    if (requiresRenderTreeRebuild && fullScreenElement && fullScreenElement->parentNode())
        fullScreenElement->parentNode()->setNeedsStyleRecalc(ReconstructRenderTree);
}

static bool hostIsYouTube(const String& host)
{
    // Match .youtube.com, youtube.com, youtube.co.uk, and all two-letter country codes.
    static NeverDestroyed<JSC::Yarr::RegularExpression> youtubePattern("(^|\\.)youtube.(com|co.uk|[a-z]{2})$", TextCaseInsensitive);
    ASSERT(youtubePattern.get().isValid());

    return youtubePattern.get().match(host);
}

void Document::webkitWillEnterFullScreenForElement(Element* element)
{
    if (!hasLivingRenderTree() || inPageCache())
        return;

    ASSERT(element);

    // Protect against being called after the document has been removed from the page.
    if (!page())
        return;

    ASSERT(page()->settings().fullScreenEnabled());

    unwrapFullScreenRenderer(m_fullScreenRenderer, m_fullScreenElement.get());

    if (element)
        element->willBecomeFullscreenElement();
    
    m_fullScreenElement = element;

#if USE(NATIVE_FULLSCREEN_VIDEO)
    if (element && element->isMediaElement())
        return;
#endif

    // Create a placeholder block for a the full-screen element, to keep the page from reflowing
    // when the element is removed from the normal flow.  Only do this for a RenderBox, as only 
    // a box will have a frameRect.  The placeholder will be created in setFullScreenRenderer()
    // during layout.
    auto renderer = m_fullScreenElement->renderer();
    bool shouldCreatePlaceholder = is<RenderBox>(renderer);
    if (shouldCreatePlaceholder) {
        m_savedPlaceholderFrameRect = downcast<RenderBox>(*renderer).frameRect();
        m_savedPlaceholderRenderStyle = RenderStyle::clone(&renderer->style());
    }

    if (m_fullScreenElement != documentElement())
        RenderFullScreen::wrapRenderer(renderer, renderer ? renderer->parent() : nullptr, *this);

    m_fullScreenElement->setContainsFullScreenElementOnAncestorsCrossingFrameBoundaries(true);

    recalcStyle(Style::Force);

    if (settings() && settings()->needsSiteSpecificQuirks() && hostIsYouTube(url().host()))
        fullScreenChangeDelayTimerFired();
}

void Document::webkitDidEnterFullScreenForElement(Element*)
{
    if (!m_fullScreenElement)
        return;

    if (!hasLivingRenderTree() || inPageCache())
        return;

    m_fullScreenElement->didBecomeFullscreenElement();

    if (!settings() || !settings()->needsSiteSpecificQuirks() || !hostIsYouTube(url().host()))
        m_fullScreenChangeDelayTimer.startOneShot(0);
}

void Document::webkitWillExitFullScreenForElement(Element*)
{
    if (!m_fullScreenElement)
        return;

    if (!hasLivingRenderTree() || inPageCache())
        return;

    m_fullScreenElement->willStopBeingFullscreenElement();
}

void Document::webkitDidExitFullScreenForElement(Element*)
{
    if (!m_fullScreenElement)
        return;

    if (!hasLivingRenderTree() || inPageCache())
        return;

    m_fullScreenElement->setContainsFullScreenElementOnAncestorsCrossingFrameBoundaries(false);

    m_areKeysEnabledInFullScreen = false;

    unwrapFullScreenRenderer(m_fullScreenRenderer, m_fullScreenElement.get());

    m_fullScreenElement = nullptr;
    scheduleForcedStyleRecalc();

    // When webkitCancelFullScreen is called, we call webkitExitFullScreen on the topDocument(). That
    // means that the events will be queued there. So if we have no events here, start the timer on
    // the exiting document.
    bool eventTargetQueuesEmpty = m_fullScreenChangeEventTargetQueue.isEmpty() && m_fullScreenErrorEventTargetQueue.isEmpty();
    Document& exitingDocument = eventTargetQueuesEmpty ? topDocument() : *this;

    // FIXME(136605): Remove this quirk once YouTube moves to relative widths and heights for
    // fullscreen mode.
    if (settings() && settings()->needsSiteSpecificQuirks() && hostIsYouTube(url().host()))
        exitingDocument.fullScreenChangeDelayTimerFired();
    else
        exitingDocument.m_fullScreenChangeDelayTimer.startOneShot(0);

}

void Document::setFullScreenRenderer(RenderFullScreen* renderer)
{
    if (renderer == m_fullScreenRenderer)
        return;

    if (renderer && m_savedPlaceholderRenderStyle) 
        renderer->createPlaceholder(m_savedPlaceholderRenderStyle.releaseNonNull(), m_savedPlaceholderFrameRect);
    else if (renderer && m_fullScreenRenderer && m_fullScreenRenderer->placeholder()) {
        RenderBlock* placeholder = m_fullScreenRenderer->placeholder();
        renderer->createPlaceholder(RenderStyle::clone(&placeholder->style()), placeholder->frameRect());
    }

    if (m_fullScreenRenderer)
        m_fullScreenRenderer->destroy();
    ASSERT(!m_fullScreenRenderer);

    m_fullScreenRenderer = renderer;
}

void Document::fullScreenRendererDestroyed()
{
    m_fullScreenRenderer = nullptr;
}

void Document::fullScreenChangeDelayTimerFired()
{
    // Since we dispatch events in this function, it's possible that the
    // document will be detached and GC'd. We protect it here to make sure we
    // can finish the function successfully.
    Ref<Document> protect(*this);
    Deque<RefPtr<Node>> changeQueue;
    m_fullScreenChangeEventTargetQueue.swap(changeQueue);
    Deque<RefPtr<Node>> errorQueue;
    m_fullScreenErrorEventTargetQueue.swap(errorQueue);
    dispatchFullScreenChangeOrErrorEvent(changeQueue, eventNames().webkitfullscreenchangeEvent, /* shouldNotifyMediaElement */ true);
    dispatchFullScreenChangeOrErrorEvent(errorQueue, eventNames().webkitfullscreenerrorEvent, /* shouldNotifyMediaElement */ false);
}

void Document::dispatchFullScreenChangeOrErrorEvent(Deque<RefPtr<Node>>& queue, const AtomicString& eventName, bool shouldNotifyMediaElement)
{
    while (!queue.isEmpty()) {
        RefPtr<Node> node = queue.takeFirst();
        if (!node)
            node = documentElement();
        // The dispatchEvent below may have blown away our documentElement.
        if (!node)
            continue;

        // If the element was removed from our tree, also message the documentElement. Since we may
        // have a document hierarchy, check that node isn't in another document.
        if (!node->inDocument())
            queue.append(documentElement());

#if ENABLE(VIDEO)
        if (shouldNotifyMediaElement && is<HTMLMediaElement>(*node))
            downcast<HTMLMediaElement>(*node).enteredOrExitedFullscreen();
#endif
        node->dispatchEvent(Event::create(eventName, true, false));
    }
}

void Document::fullScreenElementRemoved()
{
    m_fullScreenElement->setContainsFullScreenElementOnAncestorsCrossingFrameBoundaries(false);
    webkitCancelFullScreen();
}

void Document::removeFullScreenElementOfSubtree(Node* node, bool amongChildrenOnly)
{
    if (!m_fullScreenElement)
        return;
    
    bool elementInSubtree = false;
    if (amongChildrenOnly)
        elementInSubtree = m_fullScreenElement->isDescendantOf(node);
    else
        elementInSubtree = (m_fullScreenElement == node) || m_fullScreenElement->isDescendantOf(node);
    
    if (elementInSubtree)
        fullScreenElementRemoved();
}

bool Document::isAnimatingFullScreen() const
{
    return m_isAnimatingFullScreen;
}

void Document::setAnimatingFullScreen(bool flag)
{
    if (m_isAnimatingFullScreen == flag)
        return;
    m_isAnimatingFullScreen = flag;

    if (m_fullScreenElement && m_fullScreenElement->isDescendantOf(this)) {
        m_fullScreenElement->setNeedsStyleRecalc();
        scheduleForcedStyleRecalc();
    }
}

void Document::clearFullscreenElementStack()
{
    m_fullScreenElementStack.clear();
}

void Document::popFullscreenElementStack()
{
    if (m_fullScreenElementStack.isEmpty())
        return;

    m_fullScreenElementStack.removeLast();
}

void Document::pushFullscreenElementStack(Element* element)
{
    m_fullScreenElementStack.append(element);
}

void Document::addDocumentToFullScreenChangeEventQueue(Document* doc)
{
    ASSERT(doc);
    Node* target = doc->webkitFullscreenElement();
    if (!target)
        target = doc->webkitCurrentFullScreenElement();
    if (!target)
        target = doc;
    m_fullScreenChangeEventTargetQueue.append(target);
}
#endif

#if ENABLE(POINTER_LOCK)
void Document::exitPointerLock()
{
    if (!page())
        return;
    if (Element* target = page()->pointerLockController().element()) {
        if (&target->document() != this)
            return;
    }
    page()->pointerLockController().requestPointerUnlock();
}

Element* Document::pointerLockElement() const
{
    if (!page() || page()->pointerLockController().lockPending())
        return nullptr;
    if (Element* element = page()->pointerLockController().element()) {
        if (&element->document() == this)
            return element;
    }
    return nullptr;
}
#endif

void Document::decrementLoadEventDelayCount()
{
    ASSERT(m_loadEventDelayCount);
    --m_loadEventDelayCount;

    if (frame() && !m_loadEventDelayCount && !m_loadEventDelayTimer.isActive())
        m_loadEventDelayTimer.startOneShot(0);
}

void Document::loadEventDelayTimerFired()
{
    if (frame())
        frame()->loader().checkCompleted();
}

#if ENABLE(REQUEST_ANIMATION_FRAME)
int Document::requestAnimationFrame(PassRefPtr<RequestAnimationFrameCallback> callback)
{
    if (!m_scriptedAnimationController) {
#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
        m_scriptedAnimationController = ScriptedAnimationController::create(this, page() ? page()->chrome().displayID() : 0);
#else
        m_scriptedAnimationController = ScriptedAnimationController::create(this, 0);
#endif
        // It's possible that the Page may have suspended scripted animations before
        // we were created. We need to make sure that we don't start up the animation
        // controller on a background tab, for example.
        if (!page() || page()->scriptedAnimationsSuspended())
            m_scriptedAnimationController->suspend();
    }

    return m_scriptedAnimationController->registerCallback(callback);
}

void Document::cancelAnimationFrame(int id)
{
    if (!m_scriptedAnimationController)
        return;
    m_scriptedAnimationController->cancelCallback(id);
}

void Document::serviceScriptedAnimations(double monotonicAnimationStartTime)
{
    if (!m_scriptedAnimationController)
        return;
    m_scriptedAnimationController->serviceScriptedAnimations(monotonicAnimationStartTime);
}

void Document::clearScriptedAnimationController()
{
    // FIXME: consider using ActiveDOMObject.
    if (m_scriptedAnimationController)
        m_scriptedAnimationController->clearDocumentPointer();
    m_scriptedAnimationController = nullptr;
}
#endif
    
void Document::sendWillRevealEdgeEventsIfNeeded(const IntPoint& oldPosition, const IntPoint& newPosition, const IntRect& visibleRect, const IntSize& contentsSize, Element* target)
{
    // For each edge (top, bottom, left and right), send the will reveal edge event for that direction
    // if newPosition is at or beyond the notification point, if the scroll direction is heading in the
    // direction of that edge point, and if oldPosition is before the notification point (which indicates
    // that this is the first moment that we know we crossed the magic line).
    
#if ENABLE(WILL_REVEAL_EDGE_EVENTS)
    // FIXME: broken in RTL documents.
    int willRevealBottomNotificationPoint = std::max(0, contentsSize.height() - 2 *  visibleRect.height());
    int willRevealTopNotificationPoint = visibleRect.height();

    // Bottom edge.
    if (newPosition.y() >= willRevealBottomNotificationPoint && newPosition.y() > oldPosition.y()
        && willRevealBottomNotificationPoint >= oldPosition.y()) {
        Ref<Event> willRevealEvent = Event::create(eventNames().webkitwillrevealbottomEvent, false, false);
        if (!target)
            enqueueWindowEvent(WTFMove(willRevealEvent));
        else {
            willRevealEvent->setTarget(target);
            m_eventQueue.enqueueEvent(WTFMove(willRevealEvent));
        }
    }

    // Top edge.
    if (newPosition.y() <= willRevealTopNotificationPoint && newPosition.y() < oldPosition.y()
        && willRevealTopNotificationPoint <= oldPosition.y()) {
        Ref<Event> willRevealEvent = Event::create(eventNames().webkitwillrevealtopEvent, false, false);
        if (!target)
            enqueueWindowEvent(WTFMove(willRevealEvent));
        else {
            willRevealEvent->setTarget(target);
            m_eventQueue.enqueueEvent(WTFMove(willRevealEvent));
        }
    }

    int willRevealRightNotificationPoint = std::max(0, contentsSize.width() - 2 * visibleRect.width());
    int willRevealLeftNotificationPoint = visibleRect.width();

    // Right edge.
    if (newPosition.x() >= willRevealRightNotificationPoint && newPosition.x() > oldPosition.x()
        && willRevealRightNotificationPoint >= oldPosition.x()) {
        Ref<Event> willRevealEvent = Event::create(eventNames().webkitwillrevealrightEvent, false, false);
        if (!target)
            enqueueWindowEvent(WTFMove(willRevealEvent));
        else {
            willRevealEvent->setTarget(target);
            m_eventQueue.enqueueEvent(WTFMove(willRevealEvent));
        }
    }

    // Left edge.
    if (newPosition.x() <= willRevealLeftNotificationPoint && newPosition.x() < oldPosition.x()
        && willRevealLeftNotificationPoint <= oldPosition.x()) {
        Ref<Event> willRevealEvent = Event::create(eventNames().webkitwillrevealleftEvent, false, false);
        if (!target)
            enqueueWindowEvent(WTFMove(willRevealEvent));
        else {
            willRevealEvent->setTarget(target);
            m_eventQueue.enqueueEvent(WTFMove(willRevealEvent));
        }
    }
#else
    UNUSED_PARAM(oldPosition);
    UNUSED_PARAM(newPosition);
    UNUSED_PARAM(visibleRect);
    UNUSED_PARAM(contentsSize);
    UNUSED_PARAM(target);
#endif
}

#if !PLATFORM(IOS)
#if ENABLE(TOUCH_EVENTS)
RefPtr<Touch> Document::createTouch(DOMWindow* window, EventTarget* target, int identifier, int pageX, int pageY, int screenX, int screenY, int radiusX, int radiusY, float rotationAngle, float force, ExceptionCode&) const
{
    // FIXME: It's not clear from the documentation at
    // http://developer.apple.com/library/safari/#documentation/UserExperience/Reference/DocumentAdditionsReference/DocumentAdditions/DocumentAdditions.html
    // when this method should throw and nor is it by inspection of iOS behavior. It would be nice to verify any cases where it throws under iOS
    // and implement them here. See https://bugs.webkit.org/show_bug.cgi?id=47819
    Frame* frame = window ? window->frame() : this->frame();
    return Touch::create(frame, target, identifier, screenX, screenY, pageX, pageY, radiusX, radiusY, rotationAngle, force);
}
#endif
#endif // !PLATFORM(IOS)

void Document::wheelEventHandlersChanged()
{
    Page* page = this->page();
    if (!page)
        return;

    if (FrameView* frameView = view()) {
        if (ScrollingCoordinator* scrollingCoordinator = page->scrollingCoordinator())
            scrollingCoordinator->frameViewNonFastScrollableRegionChanged(*frameView);
    }

    bool haveHandlers = m_wheelEventTargets && !m_wheelEventTargets->isEmpty();
    page->chrome().client().wheelEventHandlersChanged(haveHandlers);
}

void Document::didAddWheelEventHandler(Node& node)
{
    if (!m_wheelEventTargets)
        m_wheelEventTargets = std::make_unique<EventTargetSet>();

    m_wheelEventTargets->add(&node);

    wheelEventHandlersChanged();

    if (Frame* frame = this->frame())
        DebugPageOverlays::didChangeEventHandlers(*frame);
}

HttpEquivPolicy Document::httpEquivPolicy() const
{
    if (shouldEnforceContentDispositionAttachmentSandbox())
        return HttpEquivPolicy::DisabledByContentDispositionAttachmentSandbox;
    if (page() && !page()->settings().httpEquivEnabled())
        return HttpEquivPolicy::DisabledBySettings;
    return HttpEquivPolicy::Enabled;
}

static bool removeHandlerFromSet(EventTargetSet& handlerSet, Node& node, EventHandlerRemoval removal)
{
    switch (removal) {
    case EventHandlerRemoval::One:
        return handlerSet.remove(&node);
    case EventHandlerRemoval::All:
        return handlerSet.removeAll(&node);
    }
    return false;
}

void Document::didRemoveWheelEventHandler(Node& node, EventHandlerRemoval removal)
{
    if (!m_wheelEventTargets)
        return;

    if (!removeHandlerFromSet(*m_wheelEventTargets, node, removal))
        return;

    wheelEventHandlersChanged();

    if (Frame* frame = this->frame())
        DebugPageOverlays::didChangeEventHandlers(*frame);
}

unsigned Document::wheelEventHandlerCount() const
{
    if (!m_wheelEventTargets)
        return 0;

    unsigned count = 0;
    for (auto& handler : *m_wheelEventTargets)
        count += handler.value;

    return count;
}

void Document::didAddTouchEventHandler(Node& handler)
{
#if ENABLE(TOUCH_EVENTS)
    if (!m_touchEventTargets)
        m_touchEventTargets = std::make_unique<EventTargetSet>();

    m_touchEventTargets->add(&handler);

    if (Document* parent = parentDocument()) {
        parent->didAddTouchEventHandler(*this);
        return;
    }

    if (Page* page = this->page()) {
        if (m_touchEventTargets->size() == 1)
            page->chrome().client().needTouchEvents(true);
    }
#else
    UNUSED_PARAM(handler);
#endif
}

void Document::didRemoveTouchEventHandler(Node& handler, EventHandlerRemoval removal)
{
#if ENABLE(TOUCH_EVENTS)
    if (!m_touchEventTargets)
        return;

    removeHandlerFromSet(*m_touchEventTargets, handler, removal);

    if (Document* parent = parentDocument()) {
        parent->didRemoveTouchEventHandler(*this);
        return;
    }

    Page* page = this->page();
    if (!page)
        return;
    if (m_touchEventTargets->size())
        return;

    // FIXME: why can't we trust m_touchEventTargets?
    for (const Frame* frame = &page->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (frame->document() && frame->document()->hasTouchEventHandlers())
            return;
    }
    page->chrome().client().needTouchEvents(false);
#else
    UNUSED_PARAM(handler);
    UNUSED_PARAM(removal);
#endif
}

void Document::didRemoveEventTargetNode(Node& handler)
{
#if ENABLE(TOUCH_EVENTS)
    if (m_touchEventTargets) {
        m_touchEventTargets->removeAll(&handler);
        if ((&handler == this || m_touchEventTargets->isEmpty()) && parentDocument())
            parentDocument()->didRemoveEventTargetNode(*this);
    }
#endif

    if (m_wheelEventTargets) {
        m_wheelEventTargets->removeAll(&handler);
        if ((&handler == this || m_wheelEventTargets->isEmpty()) && parentDocument())
            parentDocument()->didRemoveEventTargetNode(*this);
    }
}

unsigned Document::touchEventHandlerCount() const
{
#if ENABLE(TOUCH_EVENTS)
    if (!m_touchEventTargets)
        return 0;

    unsigned count = 0;
    for (auto& handler : *m_touchEventTargets)
        count += handler.value;

    return count;
#else
    return 0;
#endif
}

#if ENABLE(CUSTOM_ELEMENTS)
CustomElementDefinitions& Document::ensureCustomElementDefinitions()
{
    if (!m_customElementDefinitions)
        m_customElementDefinitions = std::make_unique<CustomElementDefinitions>();
    return *m_customElementDefinitions;
}
#endif

LayoutRect Document::absoluteEventHandlerBounds(bool& includesFixedPositionElements)
{
    includesFixedPositionElements = false;
    if (RenderView* renderView = this->renderView())
        return renderView->documentRect();
    
    return LayoutRect();
}

Document::RegionFixedPair Document::absoluteRegionForEventTargets(const EventTargetSet* targets)
{
    if (!targets)
        return RegionFixedPair(Region(), false);

    Region targetRegion;
    bool insideFixedPosition = false;

    for (auto& keyValuePair : *targets) {
        LayoutRect rootRelativeBounds;

        if (is<Document>(keyValuePair.key)) {
            Document* document = downcast<Document>(keyValuePair.key);
            if (document == this)
                rootRelativeBounds = absoluteEventHandlerBounds(insideFixedPosition);
            else if (Element* element = document->ownerElement())
                rootRelativeBounds = element->absoluteEventHandlerBounds(insideFixedPosition);
        } else if (is<Element>(keyValuePair.key)) {
            Element* element = downcast<Element>(keyValuePair.key);
            if (is<HTMLBodyElement>(element)) {
                // For the body, just use the document bounds.
                // The body may not cover this whole area, but it's OK for this region to be an overestimate.
                rootRelativeBounds = absoluteEventHandlerBounds(insideFixedPosition);
            } else
                rootRelativeBounds = element->absoluteEventHandlerBounds(insideFixedPosition);
        }
        
        if (!rootRelativeBounds.isEmpty())
            targetRegion.unite(Region(enclosingIntRect(rootRelativeBounds)));
    }

    return RegionFixedPair(targetRegion, insideFixedPosition);
}

void Document::updateLastHandledUserGestureTimestamp()
{
    if (!m_lastHandledUserGestureTimestamp)
        ResourceLoadObserver::sharedObserver().logUserInteraction(*this);

    m_lastHandledUserGestureTimestamp = monotonicallyIncreasingTime();
}

void Document::startTrackingStyleRecalcs()
{
    m_styleRecalcCount = 0;
}

unsigned Document::styleRecalcCount() const
{
    return m_styleRecalcCount;
}

DocumentLoader* Document::loader() const
{
    if (!m_frame)
        return nullptr;
    
    DocumentLoader* loader = m_frame->loader().documentLoader();
    if (!loader)
        return nullptr;
    
    if (m_frame->document() != this)
        return nullptr;
    
    return loader;
}

#if ENABLE(CSS_DEVICE_ADAPTATION)
IntSize Document::initialViewportSize() const
{
    if (!view())
        return IntSize();
    return view()->initialViewportSize();
}
#endif

Element* eventTargetElementForDocument(Document* document)
{
    if (!document)
        return nullptr;
    Element* element = document->focusedElement();
    if (!element && is<PluginDocument>(*document))
        element = downcast<PluginDocument>(*document).pluginElement();
    if (!element && is<HTMLDocument>(*document))
        element = document->bodyOrFrameset();
    if (!element)
        element = document->documentElement();
    return element;
}

void Document::adjustFloatQuadsForScrollAndAbsoluteZoomAndFrameScale(Vector<FloatQuad>& quads, const RenderStyle& style)
{
    if (!view())
        return;

    float zoom = style.effectiveZoom();
    float inverseFrameScale = 1;
    if (frame())
        inverseFrameScale = 1 / frame()->frameScaleFactor();

    LayoutRect visibleContentRect = view()->visibleContentRect();
    for (auto& quad : quads) {
        quad.move(-visibleContentRect.x(), -visibleContentRect.y());
        if (zoom != 1)
            quad.scale(1 / zoom, 1 / zoom);
        if (inverseFrameScale != 1)
            quad.scale(inverseFrameScale, inverseFrameScale);
    }
}

void Document::adjustFloatRectForScrollAndAbsoluteZoomAndFrameScale(FloatRect& rect, const RenderStyle& style)
{
    if (!view())
        return;

    float zoom = style.effectiveZoom();
    float inverseFrameScale = 1;
    if (frame())
        inverseFrameScale = 1 / frame()->frameScaleFactor();

    LayoutRect visibleContentRect = view()->visibleContentRect();
    rect.move(-visibleContentRect.x(), -visibleContentRect.y());
    if (zoom != 1)
        rect.scale(1 / zoom);
    if (inverseFrameScale != 1)
        rect.scale(inverseFrameScale);
}

bool Document::hasActiveParser()
{
    return m_activeParserCount || (m_parser && m_parser->processingData());
}

void Document::decrementActiveParserCount()
{
    --m_activeParserCount;
    if (!frame())
        return;

    // FIXME: We should call loader()->checkLoadComplete() as well here,
    // but it seems to cause http/tests/security/feed-urls-from-remote.html
    // to timeout on Mac WK1; see http://webkit.org/b/110554 and http://webkit.org/b/110401.
    frame()->loader().checkLoadComplete();
}

static RenderElement* nearestCommonHoverAncestor(RenderElement* obj1, RenderElement* obj2)
{
    if (!obj1 || !obj2)
        return nullptr;

    for (RenderElement* currObj1 = obj1; currObj1; currObj1 = currObj1->hoverAncestor()) {
        for (RenderElement* currObj2 = obj2; currObj2; currObj2 = currObj2->hoverAncestor()) {
            if (currObj1 == currObj2)
                return currObj1;
        }
    }

    return nullptr;
}

void Document::updateHoverActiveState(const HitTestRequest& request, Element* innerElement, StyleResolverUpdateFlag updateFlag)
{
    ASSERT(!request.readOnly());

    Element* innerElementInDocument = innerElement;
    while (innerElementInDocument && &innerElementInDocument->document() != this) {
        innerElementInDocument->document().updateHoverActiveState(request, innerElementInDocument);
        innerElementInDocument = innerElementInDocument->document().ownerElement();
    }

    Element* oldActiveElement = m_activeElement.get();
    if (oldActiveElement && !request.active()) {
        // We are clearing the :active chain because the mouse has been released.
        for (Element* curr = oldActiveElement; curr; curr = curr->parentOrShadowHostElement()) {
            curr->setActive(false);
            m_userActionElements.setInActiveChain(curr, false);
        }
        m_activeElement = nullptr;
    } else {
        Element* newActiveElement = innerElementInDocument;
        if (!oldActiveElement && newActiveElement && request.active() && !request.touchMove()) {
            // We are setting the :active chain and freezing it. If future moves happen, they
            // will need to reference this chain.
            for (RenderElement* curr = newActiveElement->renderer(); curr; curr = curr->parent()) {
                Element* element = curr->element();
                if (!element || curr->isTextOrLineBreak())
                    continue;
                m_userActionElements.setInActiveChain(element, true);
            }

            m_activeElement = newActiveElement;
        }
    }
    // If the mouse has just been pressed, set :active on the chain. Those (and only those)
    // nodes should remain :active until the mouse is released.
    bool allowActiveChanges = !oldActiveElement && m_activeElement;

    // If the mouse is down and if this is a mouse move event, we want to restrict changes in
    // :hover/:active to only apply to elements that are in the :active chain that we froze
    // at the time the mouse went down.
    bool mustBeInActiveChain = request.active() && request.move();

    RefPtr<Element> oldHoveredElement = m_hoveredElement.release();

    // A touch release does not set a new hover target; clearing the element we're working with
    // will clear the chain of hovered elements all the way to the top of the tree.
    if (request.touchRelease())
        innerElementInDocument = nullptr;

    // Check to see if the hovered Element has changed.
    // If it hasn't, we do not need to do anything.
    Element* newHoveredElement = innerElementInDocument;
    while (newHoveredElement && !newHoveredElement->renderer())
        newHoveredElement = newHoveredElement->parentOrShadowHostElement();

    m_hoveredElement = newHoveredElement;

    // We have two different objects. Fetch their renderers.
    RenderElement* oldHoverObj = oldHoveredElement ? oldHoveredElement->renderer() : nullptr;
    RenderElement* newHoverObj = newHoveredElement ? newHoveredElement->renderer() : nullptr;

    // Locate the common ancestor render object for the two renderers.
    RenderElement* ancestor = nearestCommonHoverAncestor(oldHoverObj, newHoverObj);

    Vector<RefPtr<Element>, 32> elementsToRemoveFromChain;
    Vector<RefPtr<Element>, 32> elementsToAddToChain;

    if (oldHoverObj != newHoverObj) {
        // If the old hovered element is not nil but it's renderer is, it was probably detached as part of the :hover style
        // (for instance by setting display:none in the :hover pseudo-class). In this case, the old hovered element (and its ancestors)
        // must be updated, to ensure it's normal style is re-applied.
        if (oldHoveredElement && !oldHoverObj) {
            for (Element* element = oldHoveredElement.get(); element; element = element->parentElement()) {
                if (!mustBeInActiveChain || element->inActiveChain())
                    elementsToRemoveFromChain.append(element);
            }
        }

        // The old hover path only needs to be cleared up to (and not including) the common ancestor;
        for (RenderElement* curr = oldHoverObj; curr && curr != ancestor; curr = curr->hoverAncestor()) {
            Element* element = curr->element();
            if (!element)
                continue;
            if (!mustBeInActiveChain || element->inActiveChain())
                elementsToRemoveFromChain.append(element);
        }
        // Unset hovered nodes in sub frame documents if the old hovered node was a frame owner.
        if (is<HTMLFrameOwnerElement>(oldHoveredElement.get())) {
            if (Document* contentDocument = downcast<HTMLFrameOwnerElement>(*oldHoveredElement).contentDocument())
                contentDocument->updateHoverActiveState(request, nullptr);
        }
    }

    // Now set the hover state for our new object up to the root.
    for (RenderElement* curr = newHoverObj; curr; curr = curr->hoverAncestor()) {
        Element* element = curr->element();
        if (!element)
            continue;
        if (!mustBeInActiveChain || element->inActiveChain())
            elementsToAddToChain.append(element);
    }

    for (auto& element : elementsToRemoveFromChain)
        element->setHovered(false);

    bool sawCommonAncestor = false;
    for (auto& element : elementsToAddToChain) {
        if (allowActiveChanges)
            element->setActive(true);
        if (ancestor && element == ancestor->element())
            sawCommonAncestor = true;
        if (!sawCommonAncestor) {
            // Elements after the common hover ancestor does not change hover state, but are iterated over because they may change active state.
            element->setHovered(true);
        }
    }

    ASSERT(updateFlag == RecalcStyleIfNeeded || updateFlag == DeferRecalcStyleIfNeeded);
    if (updateFlag == RecalcStyleIfNeeded)
        updateStyleIfNeeded();
}

bool Document::haveStylesheetsLoaded() const
{
    return !authorStyleSheets().hasPendingSheets() || m_ignorePendingStylesheets;
}

Locale& Document::getCachedLocale(const AtomicString& locale)
{
    AtomicString localeKey = locale;
    if (locale.isEmpty() || !RuntimeEnabledFeatures::sharedFeatures().langAttributeAwareFormControlUIEnabled())
        localeKey = defaultLanguage();
    LocaleIdentifierToLocaleMap::AddResult result = m_localeCache.add(localeKey, nullptr);
    if (result.isNewEntry)
        result.iterator->value = Locale::create(localeKey);
    return *(result.iterator->value);
}

#if ENABLE(TEMPLATE_ELEMENT)
Document& Document::ensureTemplateDocument()
{
    if (const Document* document = templateDocument())
        return const_cast<Document&>(*document);

    if (isHTMLDocument())
        m_templateDocument = HTMLDocument::create(nullptr, blankURL());
    else
        m_templateDocument = Document::create(nullptr, blankURL());

    m_templateDocument->setTemplateDocumentHost(this); // balanced in dtor.

    return *m_templateDocument;
}
#endif

Ref<FontFaceSet> Document::fonts()
{
    updateStyleIfNeeded();
    return fontSelector().fontFaceSet();
}

float Document::deviceScaleFactor() const
{
    float deviceScaleFactor = 1.0;
    if (Page* documentPage = page())
        deviceScaleFactor = documentPage->deviceScaleFactor();
    return deviceScaleFactor;
}
void Document::didAssociateFormControl(Element* element)
{
    if (!frame() || !frame()->page() || !frame()->page()->chrome().client().shouldNotifyOnFormChanges())
        return;
    m_associatedFormControls.add(element);
    if (!m_didAssociateFormControlsTimer.isActive())
        m_didAssociateFormControlsTimer.startOneShot(0);
}

void Document::didAssociateFormControlsTimerFired()
{
    if (!frame() || !frame()->page())
        return;

    Vector<RefPtr<Element>> associatedFormControls;
    copyToVector(m_associatedFormControls, associatedFormControls);

    frame()->page()->chrome().client().didAssociateFormControls(associatedFormControls);
    m_associatedFormControls.clear();
}

void Document::setCachedDOMCookies(const String& cookies)
{
    ASSERT(!isDOMCookieCacheValid());
    m_cachedDOMCookies = cookies;
    // The cookie cache is valid at most until we go back to the event loop.
    m_cookieCacheExpiryTimer.startOneShot(0);
}

void Document::invalidateDOMCookieCache()
{
    m_cookieCacheExpiryTimer.stop();
    m_cachedDOMCookies = String();
}

void Document::didLoadResourceSynchronously(const ResourceRequest&)
{
    // Synchronous resources loading can set cookies so we invalidate the cookies cache
    // in this case, to be safe.
    invalidateDOMCookieCache();
}

void Document::ensurePlugInsInjectedScript(DOMWrapperWorld& world)
{
    if (m_hasInjectedPlugInsScript)
        return;

    // Use the JS file provided by the Chrome client, or fallback to the default one.
    String jsString = page()->chrome().client().plugInExtraScript();
    if (!jsString)
        jsString = String(plugInsJavaScript, sizeof(plugInsJavaScript));

    frame()->script().evaluateInWorld(ScriptSourceCode(jsString), world);

    m_hasInjectedPlugInsScript = true;
}

#if ENABLE(SUBTLE_CRYPTO)
bool Document::wrapCryptoKey(const Vector<uint8_t>& key, Vector<uint8_t>& wrappedKey)
{
    Page* page = this->page();
    if (!page)
        return false;
    return page->chrome().client().wrapCryptoKey(key, wrappedKey);
}

bool Document::unwrapCryptoKey(const Vector<uint8_t>& wrappedKey, Vector<uint8_t>& key)
{
    Page* page = this->page();
    if (!page)
        return false;
    return page->chrome().client().unwrapCryptoKey(wrappedKey, key);
}
#endif // ENABLE(SUBTLE_CRYPTO)

Element* Document::activeElement()
{
    if (Element* element = treeScope().focusedElement())
        return element;
    return bodyOrFrameset();
}

bool Document::hasFocus() const
{
    Page* page = this->page();
    if (!page || !page->focusController().isActive())
        return false;
    if (Frame* focusedFrame = page->focusController().focusedFrame()) {
        if (focusedFrame->tree().isDescendantOf(frame()))
            return true;
    }
    return false;
}

#if ENABLE(WEB_REPLAY)
void Document::setInputCursor(PassRefPtr<InputCursor> cursor)
{
    m_inputCursor = WTFMove(cursor);
}
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
static uint64_t nextPlaybackTargetClientContextId()
{
    static uint64_t contextId = 0;
    return ++contextId;
}

void Document::addPlaybackTargetPickerClient(MediaPlaybackTargetClient& client)
{
    Page* page = this->page();
    if (!page)
        return;

    // FIXME: change this back to an ASSERT once https://webkit.org/b/144970 is fixed.
    if (m_clientToIDMap.contains(&client))
        return;

    uint64_t contextId = nextPlaybackTargetClientContextId();
    m_clientToIDMap.add(&client, contextId);
    m_idToClientMap.add(contextId, &client);
    page->addPlaybackTargetPickerClient(contextId);
}

void Document::removePlaybackTargetPickerClient(MediaPlaybackTargetClient& client)
{
    auto it = m_clientToIDMap.find(&client);
    if (it == m_clientToIDMap.end())
        return;

    uint64_t clientId = it->value;
    m_idToClientMap.remove(clientId);
    m_clientToIDMap.remove(it);

    Page* page = this->page();
    if (!page)
        return;
    page->removePlaybackTargetPickerClient(clientId);
}

void Document::showPlaybackTargetPicker(MediaPlaybackTargetClient& client, bool isVideo, const String& customMenuItemTitle)
{
    Page* page = this->page();
    if (!page)
        return;

    auto it = m_clientToIDMap.find(&client);
    if (it == m_clientToIDMap.end())
        return;

    page->showPlaybackTargetPicker(it->value, view()->lastKnownMousePosition(), isVideo, customMenuItemTitle);
}

void Document::playbackTargetPickerClientStateDidChange(MediaPlaybackTargetClient& client, MediaProducer::MediaStateFlags state)
{
    Page* page = this->page();
    if (!page)
        return;

    auto it = m_clientToIDMap.find(&client);
    if (it == m_clientToIDMap.end())
        return;

    page->playbackTargetPickerClientStateDidChange(it->value, state);
}

void Document::playbackTargetAvailabilityDidChange(uint64_t clientId, bool available)
{
    auto it = m_idToClientMap.find(clientId);
    if (it == m_idToClientMap.end())
        return;

    it->value->externalOutputDeviceAvailableDidChange(available);
}

void Document::setPlaybackTarget(uint64_t clientId, Ref<MediaPlaybackTarget>&& target)
{
    auto it = m_idToClientMap.find(clientId);
    if (it == m_idToClientMap.end())
        return;

    it->value->setPlaybackTarget(target.copyRef());
}

void Document::setShouldPlayToPlaybackTarget(uint64_t clientId, bool shouldPlay)
{
    auto it = m_idToClientMap.find(clientId);
    if (it == m_idToClientMap.end())
        return;

    it->value->setShouldPlayToPlaybackTarget(shouldPlay);
}

void Document::customPlaybackActionSelected(uint64_t clientId)
{
    if (auto* client = m_idToClientMap.get(clientId))
        client->customPlaybackActionSelected();
}
#endif // ENABLE(WIRELESS_PLAYBACK_TARGET)

#if ENABLE(MEDIA_SESSION)
MediaSession& Document::defaultMediaSession()
{
    if (!m_defaultMediaSession)
        m_defaultMediaSession = MediaSession::create(*scriptExecutionContext());

    return *m_defaultMediaSession;
}
#endif

ShouldOpenExternalURLsPolicy Document::shouldOpenExternalURLsPolicyToPropagate() const
{
    if (DocumentLoader* documentLoader = loader())
        return documentLoader->shouldOpenExternalURLsPolicyToPropagate();

    return ShouldOpenExternalURLsPolicy::ShouldNotAllow;
}

bool Document::shouldEnforceContentDispositionAttachmentSandbox() const
{
    if (m_isSynthesized)
        return false;

    bool contentDispositionAttachmentSandboxEnabled = settings() && settings()->contentDispositionAttachmentSandboxEnabled();
    bool responseIsAttachment = false;
    if (DocumentLoader* documentLoader = m_frame ? m_frame->loader().activeDocumentLoader() : nullptr)
        responseIsAttachment = documentLoader->response().isAttachment();

    return contentDispositionAttachmentSandboxEnabled && responseIsAttachment;
}

void Document::applyContentDispositionAttachmentSandbox()
{
    ASSERT(shouldEnforceContentDispositionAttachmentSandbox());

    setReferrerPolicy(ReferrerPolicyNever);
    if (!isMediaDocument())
        enforceSandboxFlags(SandboxAll);
    else
        enforceSandboxFlags(SandboxOrigin);
}

void Document::addViewportDependentPicture(HTMLPictureElement& picture)
{
    m_viewportDependentPictures.add(&picture);
}

void Document::removeViewportDependentPicture(HTMLPictureElement& picture)
{
    m_viewportDependentPictures.remove(&picture);
}

} // namespace WebCore
