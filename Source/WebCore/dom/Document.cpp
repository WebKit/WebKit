/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004-2020 Apple Inc. All rights reserved.
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
#include "Attr.h"
#include "BeforeUnloadEvent.h"
#include "CDATASection.h"
#include "CSSAnimationController.h"
#include "CSSFontSelector.h"
#include "CSSStyleDeclaration.h"
#include "CSSStyleSheet.h"
#include "CachedCSSStyleSheet.h"
#include "CachedFrame.h"
#include "CachedResourceLoader.h"
#include "CanvasRenderingContext2D.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "Comment.h"
#include "CommonVM.h"
#include "ComposedTreeIterator.h"
#include "CompositionEvent.h"
#include "ConstantPropertyMap.h"
#include "ContentSecurityPolicy.h"
#include "ContentfulPaintChecker.h"
#include "CookieJar.h"
#include "CustomElementReactionQueue.h"
#include "CustomElementRegistry.h"
#include "CustomEvent.h"
#include "DOMImplementation.h"
#include "DOMWindow.h"
#include "DateComponents.h"
#include "DebugPageOverlays.h"
#include "DeprecatedGlobalSettings.h"
#include "DocumentLoader.h"
#include "DocumentMarkerController.h"
#include "DocumentSharedObjectPool.h"
#include "DocumentTimeline.h"
#include "DocumentTimelinesController.h"
#include "DocumentType.h"
#include "DragEvent.h"
#include "Editing.h"
#include "Editor.h"
#include "ElementIterator.h"
#include "EventHandler.h"
#include "ExtensionStyleSheets.h"
#include "FocusController.h"
#include "FocusEvent.h"
#include "FontFaceSet.h"
#include "FormController.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "FrameView.h"
#include "FullscreenManager.h"
#include "GCReachableRef.h"
#include "GenericCachedHTMLCollection.h"
#include "HTMLAllCollection.h"
#include "HTMLAnchorElement.h"
#include "HTMLAttachmentElement.h"
#include "HTMLBaseElement.h"
#include "HTMLBodyElement.h"
#include "HTMLCanvasElement.h"
#include "HTMLDocument.h"
#include "HTMLElementFactory.h"
#include "HTMLFormControlElement.h"
#include "HTMLFrameElement.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLFrameSetElement.h"
#include "HTMLHeadElement.h"
#include "HTMLHtmlElement.h"
#include "HTMLIFrameElement.h"
#include "HTMLImageElement.h"
#include "HTMLInputElement.h"
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
#include "HighlightMap.h"
#include "History.h"
#include "HitTestResult.h"
#include "IdleCallbackController.h"
#include "ImageBitmapRenderingContext.h"
#include "ImageLoader.h"
#include "InspectorInstrumentation.h"
#include "IntersectionObserver.h"
#include "JSCustomElementInterface.h"
#include "JSLazyEventListener.h"
#include "KeyboardEvent.h"
#include "KeyframeEffect.h"
#include "LayoutDisallowedScope.h"
#include "LazyLoadImageObserver.h"
#include "LegacySchemeRegistry.h"
#include "LibWebRTCProvider.h"
#include "LoaderStrategy.h"
#include "Logging.h"
#include "MediaCanStartListener.h"
#include "MediaProducer.h"
#include "MediaQueryList.h"
#include "MediaQueryMatcher.h"
#include "MediaStream.h"
#include "MessageEvent.h"
#include "MouseEventWithHitTestResults.h"
#include "MutationEvent.h"
#include "NameNodeList.h"
#include "NavigationDisabler.h"
#include "NavigationScheduler.h"
#include "NestingLevelIncrementer.h"
#include "NodeIterator.h"
#include "NodeRareData.h"
#include "NodeWithIndex.h"
#include "OverflowEvent.h"
#include "PageConsoleClient.h"
#include "PageGroup.h"
#include "PageTransitionEvent.h"
#include "PaintWorkletGlobalScope.h"
#include "Performance.h"
#include "PlatformLocale.h"
#include "PlatformMediaSessionManager.h"
#include "PlatformScreen.h"
#include "PlatformStrategies.h"
#include "PlugInsResources.h"
#include "PluginDocument.h"
#include "PointerCaptureController.h"
#include "PointerLockController.h"
#include "PolicyChecker.h"
#include "PopStateEvent.h"
#include "ProcessingInstruction.h"
#include "PublicSuffix.h"
#include "Quirks.h"
#include "RealtimeMediaSourceCenter.h"
#include "RenderChildIterator.h"
#include "RenderInline.h"
#include "RenderLayerCompositor.h"
#include "RenderLineBreak.h"
#include "RenderTreeUpdater.h"
#include "RenderView.h"
#include "RenderWidget.h"
#include "RequestAnimationFrameCallback.h"
#include "ResizeObserver.h"
#include "ResourceLoadObserver.h"
#include "RuntimeApplicationChecks.h"
#include "RuntimeEnabledFeatures.h"
#include "SVGDocumentExtensions.h"
#include "SVGElement.h"
#include "SVGElementFactory.h"
#include "SVGNames.h"
#include "SVGSVGElement.h"
#include "SVGTitleElement.h"
#include "SVGUseElement.h"
#include "SVGZoomEvent.h"
#include "SWClientConnection.h"
#include "ScopedEventQueue.h"
#include "ScriptController.h"
#include "ScriptDisallowedScope.h"
#include "ScriptModuleLoader.h"
#include "ScriptRunner.h"
#include "ScriptSourceCode.h"
#include "ScriptState.h"
#include "ScriptedAnimationController.h"
#include "ScrollbarTheme.h"
#include "ScrollingCoordinator.h"
#include "SecurityOrigin.h"
#include "SecurityOriginData.h"
#include "SecurityOriginPolicy.h"
#include "SecurityPolicy.h"
#include "SegmentedString.h"
#include "SelectorQuery.h"
#include "ServiceWorkerClientData.h"
#include "ServiceWorkerContainer.h"
#include "ServiceWorkerProvider.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "SocketProvider.h"
#include "StorageEvent.h"
#include "StringCallback.h"
#include "StyleAdjuster.h"
#include "StyleColor.h"
#include "StyleProperties.h"
#include "StyleResolveForDocument.h"
#include "StyleResolver.h"
#include "StyleScope.h"
#include "StyleSheetContents.h"
#include "StyleSheetList.h"
#include "StyleTreeResolver.h"
#include "SubresourceLoader.h"
#include "TextAutoSizing.h"
#include "TextEvent.h"
#include "TextManipulationController.h"
#include "TextNodeTraversal.h"
#include "TouchAction.h"
#include "TransformSource.h"
#include "TreeWalker.h"
#include "UndoManager.h"
#include "UserGestureIndicator.h"
#include "ValidationMessageClient.h"
#include "VisibilityChangeClient.h"
#include "VisitedLinkState.h"
#include "VisualViewport.h"
#include "WebAnimation.h"
#include "WebAnimationUtilities.h"
#include "WheelEvent.h"
#include "WindowEventLoop.h"
#include "WindowFeatures.h"
#include "Worklet.h"
#include "XMLDocument.h"
#include "XMLDocumentParser.h"
#include "XMLNSNames.h"
#include "XMLNames.h"
#include "XPathEvaluator.h"
#include "XPathExpression.h"
#include "XPathNSResolver.h"
#include "XPathResult.h"
#include <JavaScriptCore/ConsoleMessage.h>
#include <JavaScriptCore/RegularExpression.h>
#include <JavaScriptCore/ScriptCallStack.h>
#include <JavaScriptCore/VM.h>
#include <ctime>
#include <wtf/IsoMallocInlines.h>
#include <wtf/Language.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/SetForScope.h>
#include <wtf/SystemTracing.h>
#include <wtf/UUID.h>
#include <wtf/text/StringBuffer.h>
#include <wtf/text/TextStream.h>

#if ENABLE(DEVICE_ORIENTATION)
#include "DeviceMotionEvent.h"
#include "DeviceOrientationAndMotionAccessController.h"
#include "DeviceOrientationEvent.h"
#endif

#if ENABLE(FULLSCREEN_API)
#include "RenderFullScreen.h"
#endif

#if ENABLE(INDEXED_DATABASE)
#include "IDBConnectionProxy.h"
#include "IDBOpenDBRequest.h"
#endif

#if PLATFORM(IOS_FAMILY)
#include "ContentChangeObserver.h"
#include "CSSFontSelector.h"
#include "DOMTimerHoldingTank.h"
#include "DeviceMotionClientIOS.h"
#include "DeviceMotionController.h"
#include "DeviceOrientationClientIOS.h"
#include "DeviceOrientationController.h"
#include "Geolocation.h"
#include "Navigator.h"
#include "NavigatorGeolocation.h"
#endif

#if ENABLE(IOS_GESTURE_EVENTS)
#include "GestureEvent.h"
#endif

#if ENABLE(MATHML)
#include "MathMLElement.h"
#include "MathMLElementFactory.h"
#include "MathMLNames.h"
#endif

#if ENABLE(MEDIA_SESSION)
#include "MediaSession.h"
#endif

#if USE(QUICK_LOOK)
#include "QuickLook.h"
#endif

#if ENABLE(TOUCH_EVENTS)
#include "TouchEvent.h"
#endif

#if ENABLE(VIDEO)
#include "CaptionUserPreferences.h"
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
#include "MediaPlaybackTargetClient.h"
#endif

#if ENABLE(XSLT)
#include "XSLTProcessor.h"
#endif

#if ENABLE(WEBGL)
#include "WebGLRenderingContext.h"
#endif
#if ENABLE(WEBGL2)
#include "WebGL2RenderingContext.h"
#endif
#if ENABLE(WEBGPU)
#include "GPUCanvasContext.h"
#endif

#if ENABLE(PICTURE_IN_PICTURE_API)
#include "HTMLVideoElement.h"
#endif

#define RELEASE_LOG_IF_ALLOWED(channel, fmt, ...) RELEASE_LOG_IF(isAlwaysOnLoggingAllowed(), channel, "%p - [pageID=%" PRIu64 ", frameID=%" PRIu64 ", main=%d] Document::" fmt, this, pageID().valueOr(PageIdentifier { }).toUInt64(), frameID().valueOr(FrameIdentifier { }).toUInt64(), this == &topDocument(), ##__VA_ARGS__)
#define RELEASE_LOG_ERROR_IF_ALLOWED(channel, fmt, ...) RELEASE_LOG_ERROR_IF(isAlwaysOnLoggingAllowed(), channel, "%p - [pageID=%" PRIu64 ", frameID=%" PRIu64 ", main=%d] Document::" fmt, this, pageID().valueOr(PageIdentifier { }).toUInt64(), frameID().valueOr(FrameIdentifier { }).toUInt64(), this == &topDocument(), ##__VA_ARGS__)

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(Document);

using namespace HTMLNames;
using namespace PAL;
using namespace WTF::Unicode;

static const unsigned cMaxWriteRecursionDepth = 21;
bool Document::hasEverCreatedAnAXObjectCache = false;
static const Seconds maxIntervalForUserGestureForwardingAfterMediaFinishesPlaying { 1_s };

struct FrameFlatteningLayoutDisallower {
    FrameFlatteningLayoutDisallower(FrameView& frameView)
        : m_frameView(frameView)
        , m_disallowLayout(frameView.effectiveFrameFlattening() != FrameFlattening::Disabled)
    {
        if (m_disallowLayout)
            m_frameView.startDisallowingLayout();
    }

    ~FrameFlatteningLayoutDisallower()
    {
        if (m_disallowLayout)
            m_frameView.endDisallowingLayout();
    }

private:
    FrameView& m_frameView;
    bool m_disallowLayout { false };
};

// Defined here to avoid including GCReachableRef.h in Document.h
struct Document::PendingScrollEventTargetList {
    WTF_MAKE_FAST_ALLOCATED;

public:
    Vector<GCReachableRef<ContainerNode>> targets;
};

#if ENABLE(INTERSECTION_OBSERVER)
static const Seconds intersectionObserversInitialUpdateDelay { 2000_ms };
#endif

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

static Widget* widgetForElement(Element* focusedElement)
{
    if (!focusedElement)
        return nullptr;
    auto* renderer = focusedElement->renderer();
    if (!is<RenderWidget>(renderer))
        return nullptr;
    return downcast<RenderWidget>(*renderer).widget();
}

static bool acceptsEditingFocus(const Element& element)
{
    ASSERT(element.hasEditableStyle());

    auto* root = element.rootEditableElement();
    Frame* frame = element.document().frame();
    if (!frame || !root)
        return false;

    return frame->editor().shouldBeginEditing(rangeOfContents(*root).ptr());
}

static bool canAccessAncestor(const SecurityOrigin& activeSecurityOrigin, Frame* targetFrame)
{
    // targetFrame can be 0 when we're trying to navigate a top-level frame
    // that has a 0 opener.
    if (!targetFrame)
        return false;

    const bool isLocalActiveOrigin = activeSecurityOrigin.isLocal();
    for (Frame* ancestorFrame = targetFrame; ancestorFrame; ancestorFrame = ancestorFrame->tree().parent()) {
        Document* ancestorDocument = ancestorFrame->document();
        // FIXME: Should be an ASSERT? Frames should alway have documents.
        if (!ancestorDocument)
            return true;

        const SecurityOrigin& ancestorSecurityOrigin = ancestorDocument->securityOrigin();
        if (activeSecurityOrigin.canAccess(ancestorSecurityOrigin))
            return true;
        
        // Allow file URL descendant navigation even when allowFileAccessFromFileURLs is false.
        // FIXME: It's a bit strange to special-case local origins here. Should we be doing
        // something more general instead?
        if (isLocalActiveOrigin && ancestorSecurityOrigin.isLocal())
            return true;
    }

    return false;
}

static void printNavigationErrorMessage(Frame& frame, const URL& activeURL, const char* reason)
{
    String message = "Unsafe JavaScript attempt to initiate navigation for frame with URL '" + frame.document()->url().string() + "' from frame with URL '" + activeURL.string() + "'. " + reason + "\n";

    // FIXME: should we print to the console of the document performing the navigation instead?
    frame.document()->domWindow()->printErrorMessage(message);
}

uint64_t Document::s_globalTreeVersion = 0;

auto Document::allDocumentsMap() -> DocumentsMap&
{
    static NeverDestroyed<DocumentsMap> documents;
    return documents;
}

auto Document::allDocuments() -> DocumentsMap::ValuesIteratorRange
{
    return allDocumentsMap().values();
}

static inline int currentOrientation(Frame* frame)
{
#if ENABLE(ORIENTATION_EVENTS)
    if (frame)
        return frame->orientation();
#else
    UNUSED_PARAM(frame);
#endif
    return 0;
}

Document::Document(Frame* frame, const URL& url, unsigned documentClasses, unsigned constructionFlags)
    : ContainerNode(*this, CreateDocument)
    , TreeScope(*this)
    , FrameDestructionObserver(frame)
    , m_settings(frame ? Ref<Settings>(frame->settings()) : Settings::create(nullptr))
    , m_quirks(makeUniqueRef<Quirks>(*this))
    , m_cachedResourceLoader(m_frame ? Ref<CachedResourceLoader>(m_frame->loader().activeDocumentLoader()->cachedResourceLoader()) : CachedResourceLoader::create(nullptr))
    , m_domTreeVersion(++s_globalTreeVersion)
    , m_styleScope(makeUnique<Style::Scope>(*this))
    , m_extensionStyleSheets(makeUnique<ExtensionStyleSheets>(*this))
    , m_visitedLinkState(makeUnique<VisitedLinkState>(*this))
    , m_markers(makeUnique<DocumentMarkerController>(*this))
    , m_styleRecalcTimer([this] { updateStyleIfNeeded(); })
#if !LOG_DISABLED
    , m_documentCreationTime(MonotonicTime::now())
#endif
    , m_scriptRunner(makeUnique<ScriptRunner>(*this))
    , m_moduleLoader(makeUnique<ScriptModuleLoader>(*this))
#if ENABLE(XSLT)
    , m_applyPendingXSLTransformsTimer(*this, &Document::applyPendingXSLTransformsTimerFired)
#endif
    , m_xmlVersion("1.0"_s)
    , m_constantPropertyMap(makeUnique<ConstantPropertyMap>(*this))
    , m_documentClasses(documentClasses)
#if ENABLE(FULLSCREEN_API)
    , m_fullscreenManager { makeUniqueRef<FullscreenManager>(*this) }
#endif
#if ENABLE(INTERSECTION_OBSERVER)
    , m_intersectionObserversInitialUpdateTimer(*this, &Document::scheduleTimedRenderingUpdate)
#endif
    , m_loadEventDelayTimer(*this, &Document::loadEventDelayTimerFired)
#if PLATFORM(IOS_FAMILY) && ENABLE(DEVICE_ORIENTATION)
    , m_deviceMotionClient(makeUnique<DeviceMotionClientIOS>(page() ? page()->deviceOrientationUpdateProvider() : nullptr))
    , m_deviceMotionController(makeUnique<DeviceMotionController>(*m_deviceMotionClient))
    , m_deviceOrientationClient(makeUnique<DeviceOrientationClientIOS>(page() ? page()->deviceOrientationUpdateProvider() : nullptr))
    , m_deviceOrientationController(makeUnique<DeviceOrientationController>(*m_deviceOrientationClient))
#endif
    , m_pendingTasksTimer(*this, &Document::pendingTasksTimerFired)
    , m_visualUpdatesSuppressionTimer(*this, &Document::visualUpdatesSuppressionTimerFired)
    , m_sharedObjectPoolClearTimer(*this, &Document::clearSharedObjectPool)
    , m_fontSelector(CSSFontSelector::create(*this))
    , m_didAssociateFormControlsTimer(*this, &Document::didAssociateFormControlsTimerFired)
    , m_cookieCacheExpiryTimer(*this, &Document::invalidateDOMCookieCache)
    , m_socketProvider(page() ? &page()->socketProvider() : nullptr)
    , m_isSynthesized(constructionFlags & Synthesized)
    , m_isNonRenderedPlaceholder(constructionFlags & NonRenderedPlaceholder)
    , m_orientationNotifier(currentOrientation(frame))
    , m_identifier(DocumentIdentifier::generate())
    , m_undoManager(UndoManager::create(*this))
    , m_editor(makeUniqueRef<Editor>(*this))
    , m_selection(makeUniqueRef<FrameSelection>(this))
{
    auto addResult = allDocumentsMap().add(m_identifier, this);
    ASSERT_UNUSED(addResult, addResult.isNewEntry);

    // We depend on the url getting immediately set in subframes, but we
    // also depend on the url NOT getting immediately set in opened windows.
    // See fast/dom/early-frame-url.html
    // and fast/dom/location-new-window-no-crash.html, respectively.
    // FIXME: Can/should we unify this behavior?
    if ((frame && frame->ownerElement()) || !url.isEmpty())
        setURL(url);

    m_cachedResourceLoader->setDocument(this);

    resetLinkColor();
    resetVisitedLinkColor();
    resetActiveLinkColor();

    initSecurityContext();
    initDNSPrefetch();

    m_fontSelector->registerForInvalidationCallbacks(*this);

    for (auto& nodeListAndCollectionCount : m_nodeListAndCollectionCounts)
        nodeListAndCollectionCount = 0;

    InspectorInstrumentation::addEventListenersToNode(*this);
#if ENABLE(MEDIA_STREAM)
    m_settings->setLegacyGetUserMediaEnabled(quirks().shouldEnableLegacyGetUserMedia());
#endif
}

Ref<Document> Document::create(Document& contextDocument)
{
    auto document = adoptRef(*new Document(nullptr, URL()));
    document->setContextDocument(contextDocument);
    document->setSecurityOriginPolicy(contextDocument.securityOriginPolicy());
    return document;
}

Ref<Document> Document::createNonRenderedPlaceholder(Frame& frame, const URL& url)
{
    return adoptRef(*new Document(&frame, url, DefaultDocumentClass, NonRenderedPlaceholder));
}

Document::~Document()
{
    ASSERT(activeDOMObjectsAreStopped());

    if (m_logger)
        m_logger->removeObserver(*this);

#if ENABLE(INTERSECTION_OBSERVER)
    if (m_intersectionObserverData) {
        for (const auto& observer : m_intersectionObserverData->observers) {
            if (observer)
                observer->rootDestroyed();
        }
        m_intersectionObserverData->observers.clear();
        // Document cannot be a target.
        ASSERT(m_intersectionObserverData->registrations.isEmpty());
    }
#endif

    ASSERT(allDocumentsMap().contains(m_identifier));
    allDocumentsMap().remove(m_identifier);
    // We need to remove from the contexts map very early in the destructor so that calling postTask() on this Document from another thread is safe.
    removeFromContextsMap();

    ASSERT(!renderView());
    ASSERT(m_backForwardCacheState != InBackForwardCache);
    ASSERT(m_ranges.isEmpty());
    ASSERT(!m_parentTreeScope);
    ASSERT(!m_disabledFieldsetElementsCount);
    ASSERT(m_inDocumentShadowRoots.isEmpty());

#if ENABLE(DEVICE_ORIENTATION) && PLATFORM(IOS_FAMILY)
    m_deviceMotionClient->deviceMotionControllerDestroyed();
    m_deviceOrientationClient->deviceOrientationControllerDestroyed();
#endif

    if (m_templateDocument)
        m_templateDocument->setTemplateDocumentHost(nullptr); // balanced in templateDocument().

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
        m_styleSheetList->detach();

    extensionStyleSheets().detachFromDocument();

    styleScope().clearResolver(); // We need to destroy CSSFontSelector before destroying m_cachedResourceLoader.
    m_fontSelector->clearDocument();
    m_fontSelector->unregisterForInvalidationCallbacks(*this);

    // It's possible for multiple Documents to end up referencing the same CachedResourceLoader (e.g., SVGImages
    // load the initial empty document and the SVGDocument with the same DocumentLoader).
    if (m_cachedResourceLoader->document() == this)
        m_cachedResourceLoader->setDocument(nullptr);

#if ENABLE(VIDEO)
    stopAllMediaPlayback();
#endif

    // We must call clearRareData() here since a Document class inherits TreeScope
    // as well as Node. See a comment on TreeScope.h for the reason.
    if (hasRareData())
        clearRareData();

    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(m_listsInvalidatedAtDocument.isEmpty());
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(m_collectionsInvalidatedAtDocument.isEmpty());
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(m_svgUseElements.isEmpty());

    for (unsigned count : m_nodeListAndCollectionCounts)
        ASSERT_UNUSED(count, !count);
}

void Document::removedLastRef()
{
    ASSERT(!m_deletionHasBegun);
    if (m_referencingNodeCount) {
        // Node::removedLastRef doesn't set refCount() to zero because it's not observable.
        // But we need to remember that our refCount reached zero in subsequent calls to decrementReferencingNodeCount().
        m_refCountAndParentBit = 0;

        // If removing a child removes the last node reference, we don't want the scope to be destroyed
        // until after removeDetachedChildren returns, so we protect ourselves.
        incrementReferencingNodeCount();

        RELEASE_ASSERT(!hasLivingRenderTree());
        // We must make sure not to be retaining any of our children through
        // these extra pointers or we will create a reference cycle.
        m_focusedElement = nullptr;
        m_hoveredElement = nullptr;
        m_activeElement = nullptr;
        m_titleElement = nullptr;
        m_documentElement = nullptr;
        m_focusNavigationStartingNode = nullptr;
        m_userActionElements.clear();
#if ENABLE(FULLSCREEN_API)
        m_fullscreenManager->clear();
#endif
        m_associatedFormControls.clear();

        m_fontSelector->clearDocument();
        m_fontSelector->unregisterForInvalidationCallbacks(*this);

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
        commonTeardown();
#ifndef NDEBUG
        m_inRemovedLastRefFunction = false;
        m_deletionHasBegun = true;
#endif
        delete this;
    }
}

void Document::commonTeardown()
{
    stopActiveDOMObjects();

#if ENABLE(FULLSCREEN_API)
    m_fullscreenManager->emptyEventQueue();
#endif

    if (svgExtensions())
        accessSVGExtensions().pauseAnimations();

    clearScriptedAnimationController();
    
    if (m_highlightMap)
        m_highlightMap->clear();

    m_pendingScrollEventTargetList = nullptr;

    if (m_timelinesController)
        m_timelinesController->detachFromDocument();

    m_timeline = nullptr;
}

Element* Document::elementForAccessKey(const String& key)
{
    if (key.isEmpty())
        return nullptr;
    if (!m_accessKeyCache)
        buildAccessKeyCache();
    return m_accessKeyCache->get(key);
}

void Document::buildAccessKeyCache()
{
    m_accessKeyCache = makeUnique<HashMap<String, Element*, ASCIICaseInsensitiveHash>>([this] {
        HashMap<String, Element*, ASCIICaseInsensitiveHash> map;
        for (auto& node : composedTreeDescendants(*this)) {
            if (!is<Element>(node))
                continue;
            auto& element = downcast<Element>(node);
            auto& key = element.attributeWithoutSynchronization(accesskeyAttr);
            if (key.isEmpty())
                continue;
            map.add(key, &element);
        }
        return map;
    }());
}

void Document::invalidateAccessKeyCacheSlowCase()
{
    m_accessKeyCache = nullptr;
}

ExceptionOr<SelectorQuery&> Document::selectorQueryForString(const String& selectorString)
{
    if (selectorString.isEmpty())
        return Exception { SyntaxError };
    if (!m_selectorQueryCache)
        m_selectorQueryCache = makeUnique<SelectorQueryCache>();
    return m_selectorQueryCache->add(selectorString, *this);
}

void Document::clearSelectorQueryCache()
{
    m_selectorQueryCache = nullptr;
}

void Document::setReferrerPolicy(ReferrerPolicy referrerPolicy)
{
    // Do not override existing referrer policy with the "empty string" one as the "empty string" means we should use
    // the policy defined elsewhere.
    if (m_referrerPolicy && referrerPolicy == ReferrerPolicy::EmptyString)
        return;

    m_referrerPolicy = referrerPolicy;
}

MediaQueryMatcher& Document::mediaQueryMatcher()
{
    if (!m_mediaQueryMatcher)
        m_mediaQueryMatcher = MediaQueryMatcher::create(*this);
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
    m_linkColor = StyleColor::colorFromKeyword(CSSValueWebkitLink, styleColorOptions(nullptr));
}

void Document::resetVisitedLinkColor()
{
    m_visitedLinkColor = StyleColor::colorFromKeyword(CSSValueWebkitLink, styleColorOptions(nullptr) | StyleColor::Options::ForVisitedLink);
}

void Document::resetActiveLinkColor()
{
    m_activeLinkColor = StyleColor::colorFromKeyword(CSSValueWebkitActivelink, styleColorOptions(nullptr));
}

DOMImplementation& Document::implementation()
{
    if (!m_implementation)
        m_implementation = makeUnique<DOMImplementation>(*this);
    return *m_implementation;
}

bool Document::hasManifest() const
{
    return documentElement() && documentElement()->hasTagName(htmlTag) && documentElement()->hasAttributeWithoutSynchronization(manifestAttr);
}

DocumentType* Document::doctype() const
{
    for (Node* node = firstChild(); node; node = node->nextSibling()) {
        if (is<DocumentType>(node))
            return downcast<DocumentType>(node);
    }
    return nullptr;
}

void Document::childrenChanged(const ChildChange& change)
{
    ContainerNode::childrenChanged(change);

    // FIXME: Chrome::didReceiveDocType() used to be called only when the doctype changed. We need to check the
    // impact of calling this systematically. If the overhead is negligible, we need to rename didReceiveDocType,
    // otherwise, we need to detect the doc type changes before updating the viewport.
    if (Page* page = this->page())
        page->chrome().didReceiveDocType(*frame());

    Element* newDocumentElement = childrenOfType<Element>(*this).first();
    if (newDocumentElement == m_documentElement)
        return;
    m_documentElement = newDocumentElement;
    // The root style used for media query matching depends on the document element.
    styleScope().clearResolver();
}

static ALWAYS_INLINE Ref<HTMLElement> createUpgradeCandidateElement(Document& document, const QualifiedName& name)
{
    if (!RuntimeEnabledFeatures::sharedFeatures().customElementsEnabled()
        || Document::validateCustomElementName(name.localName()) != CustomElementNameValidationStatus::Valid)
        return HTMLUnknownElement::create(name, document);

    auto element = HTMLElement::create(name, document);
    element->setIsCustomElementUpgradeCandidate();
    return element;
}

static ALWAYS_INLINE Ref<HTMLElement> createUpgradeCandidateElement(Document& document, const AtomString& localName)
{
    return createUpgradeCandidateElement(document, QualifiedName { nullAtom(), localName, xhtmlNamespaceURI });
}

static inline bool isValidHTMLElementName(const AtomString& localName)
{
    return Document::isValidName(localName);
}

static inline bool isValidHTMLElementName(const QualifiedName& name)
{
    return Document::isValidName(name.localName());
}

template<typename NameType>
static ExceptionOr<Ref<Element>> createHTMLElementWithNameValidation(Document& document, const NameType& name)
{
    auto element = HTMLElementFactory::createKnownElement(name, document);
    if (LIKELY(element))
        return Ref<Element> { element.releaseNonNull() };

    if (auto* window = document.domWindow()) {
        auto* registry = window->customElementRegistry();
        if (UNLIKELY(registry)) {
            if (auto* elementInterface = registry->findInterface(name))
                return elementInterface->constructElementWithFallback(document, name);
        }
    }

    if (UNLIKELY(!isValidHTMLElementName(name)))
        return Exception { InvalidCharacterError };

    return Ref<Element> { createUpgradeCandidateElement(document, name) };
}

ExceptionOr<Ref<Element>> Document::createElementForBindings(const AtomString& name)
{
    if (isHTMLDocument())
        return createHTMLElementWithNameValidation(*this, name.convertToASCIILowercase());

    if (isXHTMLDocument())
        return createHTMLElementWithNameValidation(*this, name);

    if (!isValidName(name))
        return Exception { InvalidCharacterError };

    return createElement(QualifiedName(nullAtom(), name, nullAtom()), false);
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

ExceptionOr<Ref<CDATASection>> Document::createCDATASection(const String& data)
{
    if (isHTMLDocument())
        return Exception { NotSupportedError };

    if (data.contains("]]>"))
        return Exception { InvalidCharacterError };

    return CDATASection::create(*this, data);
}

ExceptionOr<Ref<ProcessingInstruction>> Document::createProcessingInstruction(const String& target, const String& data)
{
    if (!isValidName(target))
        return Exception { InvalidCharacterError };

    if (data.contains("?>"))
        return Exception { InvalidCharacterError };

    return ProcessingInstruction::create(*this, target, data);
}

Ref<Text> Document::createEditingTextNode(const String& text)
{
    return Text::createEditingText(*this, text);
}

Ref<CSSStyleDeclaration> Document::createCSSStyleDeclaration()
{
    Ref<MutableStyleProperties> propertySet(MutableStyleProperties::create());
    return propertySet->ensureCSSStyleDeclaration();
}

ExceptionOr<Ref<Node>> Document::importNode(Node& nodeToImport, bool deep)
{
    switch (nodeToImport.nodeType()) {
    case DOCUMENT_FRAGMENT_NODE:
        if (nodeToImport.isShadowRoot())
            break;
        FALLTHROUGH;
    case ELEMENT_NODE:
    case TEXT_NODE:
    case CDATA_SECTION_NODE:
    case PROCESSING_INSTRUCTION_NODE:
    case COMMENT_NODE:
        return nodeToImport.cloneNodeInternal(document(), deep ? CloningOperation::Everything : CloningOperation::OnlySelf);

    case ATTRIBUTE_NODE: {
        auto& attribute = downcast<Attr>(nodeToImport);
        return Ref<Node> { Attr::create(*this, attribute.qualifiedName(), attribute.value()) };
    }
    case DOCUMENT_NODE: // Can't import a document into another document.
    case DOCUMENT_TYPE_NODE: // FIXME: Support cloning a DocumentType node per DOM4.
        break;
    }

    return Exception { NotSupportedError };
}


ExceptionOr<Ref<Node>> Document::adoptNode(Node& source)
{
    EventQueueScope scope;

    switch (source.nodeType()) {
    case DOCUMENT_NODE:
        return Exception { NotSupportedError };
    case ATTRIBUTE_NODE: {
        auto& attr = downcast<Attr>(source);
        if (auto* element = attr.ownerElement()) {
            auto result = element->removeAttributeNode(attr);
            if (result.hasException())
                return result.releaseException();
        }
        break;
    }       
    default:
        if (source.isShadowRoot()) {
            // ShadowRoot cannot disconnect itself from the host node.
            return Exception { HierarchyRequestError };
        }
        if (is<HTMLFrameOwnerElement>(source)) {
            auto& frameOwnerElement = downcast<HTMLFrameOwnerElement>(source);
            if (frame() && frame()->tree().isDescendantOf(frameOwnerElement.contentFrame()))
                return Exception { HierarchyRequestError };
        }
        auto result = source.remove();
        if (result.hasException())
            return result.releaseException();
        RELEASE_ASSERT(!source.isConnected());
        RELEASE_ASSERT(!source.parentNode());
    }

    source.setTreeScopeRecursively(*this);

    return Ref<Node> { source };
}

bool Document::hasValidNamespaceForElements(const QualifiedName& qName)
{
    // These checks are from DOM Core Level 2, createElementNS
    // http://www.w3.org/TR/DOM-Level-2-Core/core.html#ID-DocCrElNS
    if (!qName.prefix().isEmpty() && qName.namespaceURI().isNull()) // createElementNS(null, "html:div")
        return false;
    if (qName.prefix() == xmlAtom() && qName.namespaceURI() != XMLNames::xmlNamespaceURI) // createElementNS("http://www.example.com", "xml:lang")
        return false;

    // Required by DOM Level 3 Core and unspecified by DOM Level 2 Core:
    // http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core.html#ID-DocCrElNS
    // createElementNS("http://www.w3.org/2000/xmlns/", "foo:bar"), createElementNS(null, "xmlns:bar"), createElementNS(null, "xmlns")
    if (qName.prefix() == xmlnsAtom() || (qName.prefix().isEmpty() && qName.localName() == xmlnsAtom()))
        return qName.namespaceURI() == XMLNSNames::xmlnsNamespaceURI;
    return qName.namespaceURI() != XMLNSNames::xmlnsNamespaceURI;
}

bool Document::hasValidNamespaceForAttributes(const QualifiedName& qName)
{
    return hasValidNamespaceForElements(qName);
}

static Ref<HTMLElement> createFallbackHTMLElement(Document& document, const QualifiedName& name)
{
    if (auto* window = document.domWindow()) {
        auto* registry = window->customElementRegistry();
        if (UNLIKELY(registry)) {
            if (auto* elementInterface = registry->findInterface(name)) {
                auto element = HTMLElement::create(name, document);
                element->enqueueToUpgrade(*elementInterface);
                return element;
            }
        }
    }
    // FIXME: Should we also check the equality of prefix between the custom element and name?
    return createUpgradeCandidateElement(document, name);
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

// https://html.spec.whatwg.org/#valid-custom-element-name

struct UnicodeCodePointRange {
    UChar32 minimum;
    UChar32 maximum;
};

#if ASSERT_ENABLED

static inline bool operator<(const UnicodeCodePointRange& a, const UnicodeCodePointRange& b)
{
    ASSERT(a.minimum <= a.maximum);
    ASSERT(b.minimum <= b.maximum);
    return a.maximum < b.minimum;
}

#endif // ASSERT_ENABLED

static inline bool operator<(const UnicodeCodePointRange& a, UChar32 b)
{
    ASSERT(a.minimum <= a.maximum);
    return a.maximum < b;
}

static inline bool operator<(UChar32 a, const UnicodeCodePointRange& b)
{
    ASSERT(b.minimum <= b.maximum);
    return a < b.minimum;
}

static inline bool isPotentialCustomElementNameCharacter(UChar32 character)
{
    static const UnicodeCodePointRange ranges[] = {
        { '-', '.' },
        { '0', '9' },
        { '_', '_' },
        { 'a', 'z' },
        { 0xB7, 0xB7 },
        { 0xC0, 0xD6 },
        { 0xD8, 0xF6 },
        { 0xF8, 0x37D },
        { 0x37F, 0x1FFF },
        { 0x200C, 0x200D },
        { 0x203F, 0x2040 },
        { 0x2070, 0x218F },
        { 0x2C00, 0x2FEF },
        { 0x3001, 0xD7FF },
        { 0xF900, 0xFDCF },
        { 0xFDF0, 0xFFFD },
        { 0x10000, 0xEFFFF },
    };

    ASSERT(std::is_sorted(std::begin(ranges), std::end(ranges)));
    return std::binary_search(std::begin(ranges), std::end(ranges), character);
}

CustomElementNameValidationStatus Document::validateCustomElementName(const AtomString& localName)
{
    if (!isASCIILower(localName[0]))
        return CustomElementNameValidationStatus::FirstCharacterIsNotLowercaseASCIILetter;

    bool containsHyphen = false;
    for (auto character : StringView(localName).codePoints()) {
        if (isASCIIUpper(character))
            return CustomElementNameValidationStatus::ContainsUppercaseASCIILetter;
        if (!isPotentialCustomElementNameCharacter(character))
            return CustomElementNameValidationStatus::ContainsDisallowedCharacter;
        if (character == '-')
            containsHyphen = true;
    }

    if (!containsHyphen)
        return CustomElementNameValidationStatus::ContainsNoHyphen;

#if ENABLE(MATHML)
    const auto& annotationXmlLocalName = MathMLNames::annotation_xmlTag->localName();
#else
    static MainThreadNeverDestroyed<const AtomString> annotationXmlLocalName("annotation-xml", AtomString::ConstructFromLiteral);
#endif

    if (localName == SVGNames::color_profileTag->localName()
        || localName == SVGNames::font_faceTag->localName()
        || localName == SVGNames::font_face_formatTag->localName()
        || localName == SVGNames::font_face_nameTag->localName()
        || localName == SVGNames::font_face_srcTag->localName()
        || localName == SVGNames::font_face_uriTag->localName()
        || localName == SVGNames::missing_glyphTag->localName()
        || localName == annotationXmlLocalName)
        return CustomElementNameValidationStatus::ConflictsWithStandardElementName;

    return CustomElementNameValidationStatus::Valid;
}

ExceptionOr<Ref<Element>> Document::createElementNS(const AtomString& namespaceURI, const String& qualifiedName)
{
    auto parseResult = parseQualifiedName(namespaceURI, qualifiedName);
    if (parseResult.hasException())
        return parseResult.releaseException();
    QualifiedName parsedName { parseResult.releaseReturnValue() };
    if (!hasValidNamespaceForElements(parsedName))
        return Exception { NamespaceError };

    if (parsedName.namespaceURI() == xhtmlNamespaceURI)
        return createHTMLElementWithNameValidation(*this, parsedName);

    return createElement(parsedName, false);
}

void Document::setReadyState(ReadyState readyState)
{
    if (readyState == m_readyState)
        return;

    switch (readyState) {
    case Loading:
        if (!m_documentTiming.domLoading)
            m_documentTiming.domLoading = MonotonicTime::now();
        break;
    case Interactive:
        if (!m_documentTiming.domInteractive)
            m_documentTiming.domInteractive = MonotonicTime::now();
        break;
    case Complete:
        if (!m_documentTiming.domComplete)
            m_documentTiming.domComplete = MonotonicTime::now();
        break;
    }

    m_readyState = readyState;
    dispatchEvent(Event::create(eventNames().readystatechangeEvent, Event::CanBubble::No, Event::IsCancelable::No));

    if (settings().suppressesIncrementalRendering())
        setVisualUpdatesAllowed(readyState);
}

void Document::setVisualUpdatesAllowed(ReadyState readyState)
{
    ASSERT(settings().suppressesIncrementalRendering());
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

            if (view() && !view()->visualUpdatesAllowedByClient())
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
        m_visualUpdatesSuppressionTimer.startOneShot(1_s * settings().incrementalRenderingSuppressionTimeoutInSeconds());

    if (!visualUpdatesAllowed)
        return;

    RefPtr<FrameView> frameView = view();
    bool needsLayout = frameView && renderView() && (frameView->layoutContext().isLayoutPending() || renderView()->needsLayout());
    if (needsLayout)
        updateLayout();

    if (Page* page = this->page()) {
        if (frame()->isMainFrame()) {
            frameView->addPaintPendingMilestones(DidFirstPaintAfterSuppressedIncrementalRendering);
            if (page->requestedLayoutMilestones() & DidFirstLayoutAfterSuppressedIncrementalRendering)
                frame()->loader().didReachLayoutMilestone(DidFirstLayoutAfterSuppressedIncrementalRendering);
        }
    }

    if (frameView)
        frameView->updateCompositingLayersAfterLayout();

    if (RenderView* renderView = this->renderView())
        renderView->repaintViewAndCompositedLayers();

    if (Frame* frame = this->frame())
        frame->loader().completePageTransitionIfNeeded();
}

void Document::visualUpdatesSuppressionTimerFired()
{
    ASSERT(!m_visualUpdatesAllowed);

    // If the client is extending the visual update suppression period explicitly, the
    // watchdog should not re-enable visual updates itself, but should wait for the client.
    if (view() && !view()->visualUpdatesAllowedByClient())
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
    AtomString name = encoding();
    if (!name.isNull())
        return name;
    return UTF8Encoding().domName();
}

String Document::defaultCharsetForLegacyBindings() const
{
    if (!frame())
        UTF8Encoding().domName();
    return settings().defaultTextEncodingName();
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
    m_styleScope->didChangeStyleSheetEnvironment();
}

ExceptionOr<void> Document::setXMLVersion(const String& version)
{
    if (!XMLDocumentParser::supportsXMLVersion(version))
        return Exception { NotSupportedError };

    m_xmlVersion = version;
    return { };
}

void Document::setXMLStandalone(bool standalone)
{
    m_xmlStandalone = standalone ? StandaloneStatus::Standalone : StandaloneStatus::NotStandalone;
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
        return "application/xhtml+xml"_s;
    if (isSVGDocument())
        return "image/svg+xml"_s;
    if (xmlStandalone())
        return "text/xml"_s;
    if (isHTMLDocument())
        return "text/html"_s;
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

    return "application/xml"_s;
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
    auto node = nodeFromPoint(clientPoint, &localPoint);
    if (!node)
        return nullptr;

    auto* renderer = node->renderer();
    if (!renderer)
        return nullptr;
    auto rangeCompliantPosition = renderer->positionForPoint(localPoint).parentAnchoredEquivalent();
    if (rangeCompliantPosition.isNull())
        return nullptr;

    auto offset = rangeCompliantPosition.offsetInContainerNode();
    node = retargetToScope(*rangeCompliantPosition.containerNode());
    if (node != rangeCompliantPosition.containerNode())
        offset = 0;

    return Range::create(*this, node.get(), offset, node.get(), offset);
}

bool Document::isBodyPotentiallyScrollable(HTMLBodyElement& body)
{
    // See https://www.w3.org/TR/cssom-view-1/#potentially-scrollable.
    // An element is potentially scrollable if all of the following conditions are true:
    // - The element has an associated CSS layout box.
    // - The element is not the HTML body element, or it is and the root element's used value of the
    //   overflow-x or overflow-y properties is not visible.
    // - The element's used value of the overflow-x or overflow-y properties is not visible.
    //
    // FIXME: We should use RenderObject::hasOverflowClip() instead of Element::computedStyle() but
    // the used values are currently not correctly updated. See https://webkit.org/b/182292.
    return body.renderer()
        && documentElement()->computedStyle()
        && !documentElement()->computedStyle()->isOverflowVisible()
        && body.computedStyle()
        && !body.computedStyle()->isOverflowVisible();
}

Element* Document::scrollingElementForAPI()
{
    if (inQuirksMode() && settings().CSSOMViewScrollingAPIEnabled())
        updateLayoutIgnorePendingStylesheets();
    return scrollingElement();
}

Element* Document::scrollingElement()
{
    if (settings().CSSOMViewScrollingAPIEnabled()) {
        // See https://drafts.csswg.org/cssom-view/#dom-document-scrollingelement.
        // The scrollingElement attribute, on getting, must run these steps:
        // 1. If the Document is in quirks mode, follow these substeps:
        if (inQuirksMode()) {
            auto* firstBody = body();
            // 1. If the HTML body element exists, and it is not potentially scrollable, return the
            // HTML body element and abort these steps.
            if (firstBody && !isBodyPotentiallyScrollable(*firstBody))
                return firstBody;

            // 2. Return null and abort these steps.
            return nullptr;
        }

        // 2. If there is a root element, return the root element and abort these steps.
        // 3. Return null.
        return documentElement();
    }

    return body();
}

static String canonicalizedTitle(Document& document, const String& title)
{
    // Collapse runs of HTML spaces into single space characters.
    // Strip leading and trailing spaces.
    // Replace backslashes with currency symbols.

    StringBuilder builder;

    auto* decoder = document.decoder();
    auto backslashAsCurrencySymbol = decoder ? decoder->encoding().backslashAsCurrencySymbol() : '\\';

    bool previousCharacterWasHTMLSpace = false;
    for (auto character : StringView { title }.codeUnits()) {
        if (isHTMLSpace(character))
            previousCharacterWasHTMLSpace = true;
        else {
            if (character == '\\')
                character = backslashAsCurrencySymbol;
            if (previousCharacterWasHTMLSpace && !builder.isEmpty())
                builder.append(' ');
            builder.append(character);
            previousCharacterWasHTMLSpace = false;
        }
    }

    return builder == title ? title : builder.toString();
}

void Document::updateTitle(const StringWithDirection& title)
{
    if (m_rawTitle == title)
        return;

    m_rawTitle = title;

    m_title.string = canonicalizedTitle(*this, title.string);
    m_title.direction = title.direction;

    if (auto* loader = this->loader())
        loader->setTitle(m_title);
}

void Document::updateTitleFromTitleElement()
{
    if (!m_titleElement) {
        updateTitle({ });
        return;
    }

    if (is<HTMLTitleElement>(*m_titleElement))
        updateTitle(downcast<HTMLTitleElement>(*m_titleElement).textWithDirection());
    else if (is<SVGTitleElement>(*m_titleElement)) {
        // FIXME: Does the SVG title element have a text direction?
        updateTitle({ downcast<SVGTitleElement>(*m_titleElement).textContent(), TextDirection::LTR });
    }
}

void Document::setTitle(const String& title)
{
    auto* element = documentElement();
    if (is<SVGSVGElement>(element)) {
        if (!m_titleElement) {
            m_titleElement = SVGTitleElement::create(SVGNames::titleTag, *this);
            element->insertBefore(*m_titleElement, element->firstChild());
        }
        // insertBefore above may have ran scripts which removed m_titleElement.
        if (m_titleElement)
            m_titleElement->setTextContent(title);
    } else if (is<HTMLElement>(element)) {
        if (!m_titleElement) {
            auto* headElement = head();
            if (!headElement)
                return;
            m_titleElement = HTMLTitleElement::create(HTMLNames::titleTag, *this);
            headElement->appendChild(*m_titleElement);
        }
        // appendChild above may have ran scripts which removed m_titleElement.
        if (m_titleElement)
            m_titleElement->setTextContent(title);
    }
}

template<typename> struct TitleTraits;

template<> struct TitleTraits<HTMLTitleElement> {
    static bool isInEligibleLocation(HTMLTitleElement& element) { return element.isConnected() && !element.isInShadowTree(); }
    static HTMLTitleElement* findTitleElement(Document& document) { return descendantsOfType<HTMLTitleElement>(document).first(); }
};

template<> struct TitleTraits<SVGTitleElement> {
    static bool isInEligibleLocation(SVGTitleElement& element) { return element.parentNode() == element.document().documentElement(); }
    static SVGTitleElement* findTitleElement(Document& document) { return childrenOfType<SVGTitleElement>(*document.documentElement()).first(); }
};

template<typename TitleElement> Element* selectNewTitleElement(Document& document, Element* oldTitleElement, Element& changingTitleElement)
{
    using Traits = TitleTraits<TitleElement>;

    if (!is<TitleElement>(changingTitleElement)) {
        ASSERT(oldTitleElement == Traits::findTitleElement(document));
        return oldTitleElement;
    }

    if (oldTitleElement)
        return Traits::findTitleElement(document);

    // Optimized common case: We have no title element yet.
    // We can figure out which title element should be used without searching.
    bool isEligible = Traits::isInEligibleLocation(downcast<TitleElement>(changingTitleElement));
    auto* newTitleElement = isEligible ? &changingTitleElement : nullptr;
    ASSERT(newTitleElement == Traits::findTitleElement(document));
    return newTitleElement;
}

void Document::updateTitleElement(Element& changingTitleElement)
{
    // Most documents use HTML title rules.
    // Documents with SVG document elements use SVG title rules.
    auto selectTitleElement = is<SVGSVGElement>(documentElement())
        ? selectNewTitleElement<SVGTitleElement> : selectNewTitleElement<HTMLTitleElement>;
    auto newTitleElement = selectTitleElement(*this, m_titleElement.get(), changingTitleElement);
    if (m_titleElement == newTitleElement)
        return;
    m_titleElement = newTitleElement;
    updateTitleFromTitleElement();
}

void Document::titleElementAdded(Element& titleElement)
{
    if (m_titleElement == &titleElement)
        return;

    updateTitleElement(titleElement);
}

void Document::titleElementRemoved(Element& titleElement)
{
    if (m_titleElement != &titleElement)
        return;

    updateTitleElement(titleElement);
}

void Document::titleElementTextChanged(Element& titleElement)
{
    if (m_titleElement != &titleElement)
        return;

    updateTitleFromTitleElement();
}

void Document::registerForVisibilityStateChangedCallbacks(VisibilityChangeClient& client)
{
    m_visibilityStateCallbackClients.add(&client);
}

void Document::unregisterForVisibilityStateChangedCallbacks(VisibilityChangeClient& client)
{
    m_visibilityStateCallbackClients.remove(&client);
}

void Document::visibilityStateChanged()
{
    // https://w3c.github.io/page-visibility/#reacting-to-visibilitychange-changes
    queueTaskToDispatchEvent(TaskSource::UserInteraction, Event::create(eventNames().visibilitychangeEvent, Event::CanBubble::Yes, Event::IsCancelable::No));
    for (auto* client : m_visibilityStateCallbackClients)
        client->visibilityStateChanged();

#if ENABLE(MEDIA_STREAM) && PLATFORM(IOS_FAMILY)
    if (!PlatformMediaSessionManager::sharedManager().isInterrupted())
        MediaStreamTrack::updateCaptureAccordingToMutedState(*this);
#endif
}

VisibilityState Document::visibilityState() const
{
    // The visibility of the document is inherited from the visibility of the
    // page. If there is no page associated with the document, we will assume
    // that the page is hidden, as specified by the spec:
    // https://w3c.github.io/page-visibility/#visibilitystate-attribute
    if (!m_frame || !m_frame->page())
        return VisibilityState::Hidden;
    return m_frame->page()->visibilityState();
}

bool Document::hidden() const
{
    return visibilityState() != VisibilityState::Visible;
}

#if ENABLE(VIDEO)

void Document::registerMediaElement(HTMLMediaElement& element)
{
    m_mediaElements.add(&element);
}

void Document::unregisterMediaElement(HTMLMediaElement& element)
{
    m_mediaElements.remove(&element);
}

void Document::forEachMediaElement(const Function<void(HTMLMediaElement&)>& function)
{
    Vector<Ref<HTMLMediaElement>> elements;
    for (auto* element : m_mediaElements)
        elements.append(*element);
    for (auto& element : elements)
        function(element);
}

void Document::stopAllMediaPlayback()
{
    if (auto* platformMediaSessionManager = PlatformMediaSessionManager::sharedManagerIfExists())
        platformMediaSessionManager->stopAllMediaPlaybackForDocument(identifier());
}

void Document::suspendAllMediaPlayback()
{
    if (auto* platformMediaSessionManager = PlatformMediaSessionManager::sharedManagerIfExists())
        platformMediaSessionManager->suspendAllMediaPlaybackForDocument(identifier());
}

void Document::resumeAllMediaPlayback()
{
    if (auto* platformMediaSessionManager = PlatformMediaSessionManager::sharedManagerIfExists())
        platformMediaSessionManager->resumeAllMediaPlaybackForDocument(identifier());
}

void Document::suspendAllMediaBuffering()
{
    if (auto* platformMediaSessionManager = PlatformMediaSessionManager::sharedManagerIfExists())
        platformMediaSessionManager->suspendAllMediaBufferingForDocument(identifier());
}

void Document::resumeAllMediaBuffering()
{
    if (auto* platformMediaSessionManager = PlatformMediaSessionManager::sharedManagerIfExists())
        platformMediaSessionManager->resumeAllMediaBufferingForDocument(identifier());
}

#endif

String Document::nodeName() const
{
    return "#document"_s;
}

Node::NodeType Document::nodeType() const
{
    return DOCUMENT_NODE;
}

FormController& Document::formController()
{
    if (!m_formController)
        m_formController = makeUnique<FormController>();
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

Ref<Range> Document::createRange()
{
    return Range::create(*this);
}

Ref<NodeIterator> Document::createNodeIterator(Node& root, unsigned long whatToShow, RefPtr<NodeFilter>&& filter, bool)
{
    return NodeIterator::create(root, whatToShow, WTFMove(filter));
}

Ref<TreeWalker> Document::createTreeWalker(Node& root, unsigned long whatToShow, RefPtr<NodeFilter>&& filter, bool)
{
    return TreeWalker::create(root, whatToShow, WTFMove(filter));
}

void Document::scheduleFullStyleRebuild()
{
    m_needsFullStyleRebuild = true;
    scheduleStyleRecalc();
}

void Document::scheduleStyleRecalc()
{
    ASSERT(!m_renderView || !inHitTesting());

    if (m_styleRecalcTimer.isActive() || backForwardCacheState() != NotInBackForwardCache)
        return;

    ASSERT(childNeedsStyleRecalc() || m_needsFullStyleRebuild);

    m_styleRecalcTimer.startOneShot(0_s);

    InspectorInstrumentation::didScheduleStyleRecalculation(*this);
}

void Document::unscheduleStyleRecalc()
{
    ASSERT(!childNeedsStyleRecalc());

    m_styleRecalcTimer.stop();
    m_needsFullStyleRebuild = false;
}

bool Document::hasPendingStyleRecalc() const
{
    return needsStyleRecalc() && !m_inStyleRecalc;
}

bool Document::hasPendingFullStyleRebuild() const
{
    return hasPendingStyleRecalc() && m_needsFullStyleRebuild;
}

void Document::updateRenderTree(std::unique_ptr<const Style::Update> styleUpdate)
{
    ASSERT(!inRenderTreeUpdate());

    Style::PostResolutionCallbackDisabler callbackDisabler(*this);
    {
        SetForScope<bool> inRenderTreeUpdate(m_inRenderTreeUpdate, true);
        {
            RenderTreeUpdater updater(*this, callbackDisabler);
            updater.commit(WTFMove(styleUpdate));
        }
    }
}

void Document::resolveStyle(ResolveStyleType type)
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
    AnimationUpdateBlock animationUpdateBlock(&m_frame->legacyAnimation());

    // FIXME: Do this update per tree scope.
    {
        auto elements = copyToVectorOf<RefPtr<SVGUseElement>>(m_svgUseElements);
        // We can't clear m_svgUseElements here because updateShadowTree may end up executing arbitrary scripts
        // which may insert new SVG use elements or remove existing ones inside sync IPC via ImageLoader::updateFromElement.
        for (auto& element : elements)
            element->updateShadowTree();
    }

    // FIXME: We should update style on our ancestor chain before proceeding, however doing so at
    // the time this comment was originally written caused several tests to crash.

    {
        ScriptDisallowedScope::InMainThread scriptDisallowedScope;
        styleScope().flushPendingUpdate();
        frameView.willRecalcStyle();
    }

    InspectorInstrumentation::willRecalculateStyle(*this);

    bool updatedCompositingLayers = false;
    {
        Style::PostResolutionCallbackDisabler disabler(*this);
        WidgetHierarchyUpdatesSuspensionScope suspendWidgetHierarchyUpdates;
        ScriptDisallowedScope::InMainThread scriptDisallowedScope;

        m_inStyleRecalc = true;

        if (m_needsFullStyleRebuild)
            type = ResolveStyleType::Rebuild;

        if (type == ResolveStyleType::Rebuild) {
            // This may get set again during style resolve.
            m_hasNodesWithNonFinalStyle = false;
            m_hasNodesWithMissingStyle = false;

            auto documentStyle = Style::resolveForDocument(*this);

            // Inserting the pictograph font at the end of the font fallback list is done by the
            // font selector, so set a font selector if needed.
            if (settings().fontFallbackPrefersPictographs())
                documentStyle.fontCascade().update(&fontSelector());

            auto documentChange = Style::determineChange(documentStyle, m_renderView->style());
            if (documentChange != Style::NoChange)
                renderView()->setStyle(WTFMove(documentStyle));

            if (auto* documentElement = this->documentElement())
                documentElement->invalidateStyleForSubtree();
        }

        Style::TreeResolver resolver(*this);
        auto styleUpdate = resolver.resolve();

        m_lastStyleUpdateSizeForTesting = styleUpdate ? styleUpdate->size() : 0;

        setHasValidStyle();
        clearChildNeedsStyleRecalc();
        unscheduleStyleRecalc();

        m_inStyleRecalc = false;

        if (styleUpdate) {
            updateRenderTree(WTFMove(styleUpdate));
            frameView.styleAndRenderTreeDidChange();
        }

        updatedCompositingLayers = frameView.updateCompositingLayersAfterStyleChange();

        if (m_renderView->needsLayout())
            frameView.layoutContext().scheduleLayout();

        // Usually this is handled by post-layout.
        if (!frameView.needsLayout())
            frameView.frame().selection().scheduleAppearanceUpdateAfterStyleChange();

        // As a result of the style recalculation, the currently hovered element might have been
        // detached (for example, by setting display:none in the :hover style), schedule another mouseMove event
        // to check if any other elements ended up under the mouse pointer due to re-layout.
        if (m_hoveredElement && !m_hoveredElement->renderer())
            frameView.frame().mainFrame().eventHandler().dispatchFakeMouseMoveEventSoon();

        ++m_styleRecalcCount;
        // FIXME: Assert ASSERT(!needsStyleRecalc()) here. Do we still have some cases where it's not true?
    }

    InspectorInstrumentation::didRecalculateStyle(*this);

    // Some animated images may now be inside the viewport due to style recalc,
    // resume them if necessary if there is no layout pending. Otherwise, we'll
    // check if they need to be resumed after layout.
    if (updatedCompositingLayers && !frameView.needsLayout())
        frameView.viewportContentsChanged();
}

void Document::updateTextRenderer(Text& text, unsigned offsetOfReplacedText, unsigned lengthOfReplacedText)
{
    auto textUpdate = makeUnique<Style::Update>(*this);
    textUpdate->addText(text, { offsetOfReplacedText, lengthOfReplacedText, WTF::nullopt });

    updateRenderTree(WTFMove(textUpdate));
}

bool Document::needsStyleRecalc() const
{
    if (backForwardCacheState() != NotInBackForwardCache)
        return false;

    if (m_needsFullStyleRebuild)
        return true;

    if (childNeedsStyleRecalc())
        return true;

    if (styleScope().hasPendingUpdate())
        return true;

    return false;
}

static bool isSafeToUpdateStyleOrLayout(const Document& document)
{
    bool isSafeToExecuteScript = ScriptDisallowedScope::InMainThread::isScriptAllowed();
    auto* frameView = document.view();
    bool isInFrameFlattening = frameView && frameView->isInChildFrameWithFrameFlattening();
    return isSafeToExecuteScript || isInFrameFlattening || !isInWebProcess();
}

bool Document::updateStyleIfNeeded()
{
    RefPtr<FrameView> frameView = view();
    {
        ScriptDisallowedScope::InMainThread scriptDisallowedScope;
        ASSERT(isMainThread());
        ASSERT(!frameView || !frameView->isPainting());

        if (!frameView || frameView->layoutContext().isInRenderTreeLayout())
            return false;

        styleScope().flushPendingUpdate();

        if (!needsStyleRecalc())
            return false;
    }

#if PLATFORM(IOS_FAMILY)
    ContentChangeObserver::StyleRecalcScope observingScope(*this);
#endif
    // The early exit above for !needsStyleRecalc() is needed when updateWidgetPositions() is called in runOrScheduleAsynchronousTasks().
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(isSafeToUpdateStyleOrLayout(*this));
    resolveStyle();
    return true;
}

void Document::updateLayout()
{
    ASSERT(isMainThread());

    RefPtr<FrameView> frameView = view();
    if (frameView && frameView->layoutContext().isInRenderTreeLayout()) {
        // View layout should not be re-entrant.
        ASSERT_NOT_REACHED();
        return;
    }
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(isSafeToUpdateStyleOrLayout(*this));

    RenderView::RepaintRegionAccumulator repaintRegionAccumulator(renderView());

    if (HTMLFrameOwnerElement* owner = ownerElement())
        owner->document().updateLayout();

    updateStyleIfNeeded();

    StackStats::LayoutCheckPoint layoutCheckPoint;

    // Only do a layout if changes have occurred that make it necessary.      
    if (frameView && renderView() && (frameView->layoutContext().isLayoutPending() || renderView()->needsLayout()))
        frameView->layoutContext().layout();
}

void Document::updateLayoutIgnorePendingStylesheets(Document::RunPostLayoutTasks runPostLayoutTasks)
{
    bool oldIgnore = m_ignorePendingStylesheets;

    if (!haveStylesheetsLoaded()) {
        m_ignorePendingStylesheets = true;
        // FIXME: This should just invalidate elements with missing styles.
        if (m_hasNodesWithMissingStyle)
            scheduleFullStyleRebuild();
    }

    updateLayout();

    if (runPostLayoutTasks == RunPostLayoutTasks::Synchronously && view())
        view()->flushAnyPendingPostLayoutTasks();

    m_ignorePendingStylesheets = oldIgnore;
}

std::unique_ptr<RenderStyle> Document::styleForElementIgnoringPendingStylesheets(Element& element, const RenderStyle* parentStyle, PseudoId pseudoElementSpecifier)
{
    ASSERT(&element.document() == this);
    ASSERT(!element.isPseudoElement() || pseudoElementSpecifier == PseudoId::None);
    ASSERT(pseudoElementSpecifier == PseudoId::None || parentStyle);

    // On iOS request delegates called during styleForElement may result in re-entering WebKit and killing the style resolver.
    Style::PostResolutionCallbackDisabler disabler(*this, Style::PostResolutionCallbackDisabler::DrainCallbacks::No);

    SetForScope<bool> change(m_ignorePendingStylesheets, true);
    auto& resolver = element.styleResolver();

    if (pseudoElementSpecifier != PseudoId::None)
        return resolver.pseudoStyleForElement(element, { pseudoElementSpecifier }, *parentStyle);

    auto elementStyle = resolver.styleForElement(element, parentStyle);
    if (elementStyle.relations) {
        Style::Update emptyUpdate(*this);
        Style::commitRelations(WTFMove(elementStyle.relations), emptyUpdate);
    }

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
    RefPtr<FrameView> frameView = view();
    if (frameView && frameView->layoutContext().isInRenderTreeLayout()) {
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
    if (!renderer || renderer->needsLayout()) {
        // If we don't have a renderer or if the renderer needs layout for any reason, give up.
        requireFullLayout = true;
    }

    // Turn off this optimization for input elements with shadow content.
    if (is<HTMLInputElement>(element))
        requireFullLayout = true;

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
            
            if (!currentBox->isRenderBlockFlow() || currentBox->enclosingFragmentedFlow() || currentBox->isWritingModeRoot()) {
                // FIXME: For now require only block flows all the way back to the root. This limits the optimization
                // for now, and we'll expand it in future patches to apply to more and more scenarios.
                // Disallow columns from having the optimization.
                // Give up if the writing mode changes at all in the containing block chain.
                requireFullLayout = true;
                break;
            }
            
            if (currRenderer == frameView->layoutContext().subtreeLayoutRoot())
                break;
        }
    }
    
    StackStats::LayoutCheckPoint layoutCheckPoint;

    // Only do a layout if changes have occurred that make it necessary.      
    if (requireFullLayout && frameView && renderView() && (frameView->layoutContext().isLayoutPending() || renderView()->needsLayout()))
        frameView->layoutContext().layout();
    
    return requireFullLayout;
}

bool Document::isPageBoxVisible(int pageIndex)
{
    updateStyleIfNeeded();
    std::unique_ptr<RenderStyle> pageStyle(styleScope().resolver().styleForPage(pageIndex));
    return pageStyle->visibility() != Visibility::Hidden; // display property doesn't apply to @page.
}

void Document::pageSizeAndMarginsInPixels(int pageIndex, IntSize& pageSize, int& marginTop, int& marginRight, int& marginBottom, int& marginLeft)
{
    updateStyleIfNeeded();
    auto style = styleScope().resolver().styleForPage(pageIndex);

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
        auto& size = style->pageSize();
        ASSERT(size.width.isFixed());
        ASSERT(size.height.isFixed());
        width = valueForLength(size.width, 0);
        height = valueForLength(size.height, 0);
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

Style::Resolver& Document::userAgentShadowTreeStyleResolver()
{
    if (!m_userAgentShadowTreeStyleResolver)
        m_userAgentShadowTreeStyleResolver = makeUnique<Style::Resolver>(*this);
    return *m_userAgentShadowTreeStyleResolver;
}

void Document::fontsNeedUpdate(FontSelector&)
{
    invalidateMatchedPropertiesCacheAndForceStyleRecalc();
}

void Document::invalidateMatchedPropertiesCacheAndForceStyleRecalc()
{
    styleScope().invalidateMatchedDeclarationsCache();

    if (backForwardCacheState() != NotInBackForwardCache || !renderView())
        return;
    scheduleFullStyleRebuild();
}

void Document::didClearStyleResolver()
{
    m_userAgentShadowTreeStyleResolver = nullptr;
}

void Document::setIsResolvingTreeStyle(bool value)
{
    RELEASE_ASSERT(value != m_isResolvingTreeStyle);
    m_isResolvingTreeStyle = value;
}

void Document::createRenderTree()
{
    ASSERT(!renderView());
    ASSERT(m_backForwardCacheState != InBackForwardCache);
    ASSERT(!m_axObjectCache || this != &topDocument());

    if (m_isNonRenderedPlaceholder)
        return;

    // FIXME: It would be better if we could pass the resolved document style directly here.
    m_renderView = createRenderer<RenderView>(*this, RenderStyle::create());
    Node::setRenderer(m_renderView.get());

    renderView()->setIsInWindow(true);

    resolveStyle(ResolveStyleType::Rebuild);
}

void Document::didBecomeCurrentDocumentInFrame()
{
    // FIXME: Are there cases where the document can be dislodged from the frame during the event handling below?
    // If so, then m_frame could become 0, and we need to do something about that.

    m_frame->script().updateDocument();

    if (!hasLivingRenderTree())
        createRenderTree();

    dispatchDisabledAdaptationsDidChangeForMainFrame();
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

    // Ensure that the scheduled task state of the document matches the DOM suspension state of the frame. It can
    // be out of sync if the DOM suspension state changed while the document was not in the frame (possibly in the
    // back/forward cache, or simply newly created).
    if (m_frame->activeDOMObjectsAndAnimationsSuspended()) {
        if (RuntimeEnabledFeatures::sharedFeatures().webAnimationsCSSIntegrationEnabled()) {
            if (m_timelinesController)
                m_timelinesController->suspendAnimations();
        } else
            m_frame->legacyAnimation().suspendAnimationsForDocument(this);
        suspendScheduledTasks(ReasonForSuspension::PageWillBeSuspended);
    } else {
        resumeScheduledTasks(ReasonForSuspension::PageWillBeSuspended);
        if (RuntimeEnabledFeatures::sharedFeatures().webAnimationsCSSIntegrationEnabled()) {
            if (m_timelinesController)
                m_timelinesController->resumeAnimations();
        } else
            m_frame->legacyAnimation().resumeAnimationsForDocument(this);
    }
}

void Document::frameDestroyed()
{
    // detachFromFrame() must be called before destroying the Frame.
    RELEASE_ASSERT(!m_frame);

    if (auto* window = domWindow())
        window->frameDestroyed();

    FrameDestructionObserver::frameDestroyed();
}

void Document::willDetachPage()
{
    FrameDestructionObserver::willDetachPage();
#if PLATFORM(IOS_FAMILY)
    contentChangeObserver().willDetachPage();
#endif
    if (domWindow() && frame())
        InspectorInstrumentation::frameWindowDiscarded(*frame(), domWindow());
}

void Document::attachToCachedFrame(CachedFrameBase& cachedFrame)
{
    RELEASE_ASSERT(cachedFrame.document() == this);
    ASSERT(cachedFrame.view());
    ASSERT(m_backForwardCacheState == Document::InBackForwardCache);
    observeFrame(&cachedFrame.view()->frame());
}

void Document::detachFromCachedFrame(CachedFrameBase& cachedFrame)
{
    ASSERT_UNUSED(cachedFrame, cachedFrame.view());
    RELEASE_ASSERT(cachedFrame.document() == this);
    ASSERT(m_frame == &cachedFrame.view()->frame());
    ASSERT(m_backForwardCacheState == Document::InBackForwardCache);
    detachFromFrame();
}

void Document::destroyRenderTree()
{
    ASSERT(hasLivingRenderTree());
    ASSERT(frame());
    ASSERT(frame()->document() == this);
    ASSERT(page());

    // Prevent Widget tree changes from committing until the RenderView is dead and gone.
    WidgetHierarchyUpdatesSuspensionScope suspendWidgetHierarchyUpdates;

    SetForScope<bool> change(m_renderTreeBeingDestroyed, true);

    if (this == &topDocument())
        clearAXObjectCache();

    documentWillBecomeInactive();

    if (view())
        view()->willDestroyRenderTree();

    if (m_documentElement)
        RenderTreeUpdater::tearDownRenderers(*m_documentElement);

    clearChildNeedsStyleRecalc();

    unscheduleStyleRecalc();

    // FIXME: RenderObject::view() uses m_renderView and we can't null it before destruction is completed
    {
        RenderTreeBuilder builder(*m_renderView);
        // FIXME: This is a workaround for leftover content (see webkit.org/b/182547).
        while (m_renderView->firstChild())
            builder.destroy(*m_renderView->firstChild());
        m_renderView->destroy();
    }
    m_renderView.release();

    Node::setRenderer(nullptr);

#if ENABLE(TEXT_AUTOSIZING)
    m_textAutoSizing = nullptr;
#endif

    if (view())
        view()->didDestroyRenderTree();
}

void Document::willBeRemovedFromFrame()
{
    if (m_hasPreparedForDestruction)
        return;

    if (m_frame)
        m_frame->legacyAnimation().detachFromDocument(this);

#if USE(LIBWEBRTC)
    // FIXME: This should be moved to Modules/mediastream.
    if (LibWebRTCProvider::webRTCAvailable()) {
        if (auto* page = this->page())
            page->libWebRTCProvider().unregisterMDNSNames(identifier());
    }
#endif

#if ENABLE(SERVICE_WORKER)
    setActiveServiceWorker(nullptr);
    setServiceWorkerConnection(nullptr);
#endif

#if ENABLE(IOS_TOUCH_EVENTS)
    clearTouchEventHandlersAndListeners();
#endif

    m_undoManager->removeAllItems();

    m_textManipulationController = nullptr; // Free nodes kept alive by TextManipulationController.

#if ENABLE(ACCESSIBILITY)
    if (this != &topDocument()) {
        // Let the ax cache know that this subframe goes out of scope.
        if (auto* cache = existingAXObjectCache())
            cache->prepareForDocumentDestruction(*this);
    }
#endif

    {
        NavigationDisabler navigationDisabler(m_frame);
        disconnectDescendantFrames();
    }
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(!m_frame || !m_frame->tree().childCount());

    if (m_domWindow && m_frame)
        m_domWindow->willDetachDocumentFromFrame();

    styleScope().clearResolver();

    if (hasLivingRenderTree())
        destroyRenderTree();

    if (is<PluginDocument>(*this))
        downcast<PluginDocument>(*this).detachFromPluginElement();

#if ENABLE(POINTER_LOCK)
    if (page())
        page()->pointerLockController().documentDetached(*this);
#endif

    if (auto* page = this->page()) {
        if (auto* validationMessageClient = page->validationMessageClient())
            validationMessageClient->documentDetached(*this);
    }

    InspectorInstrumentation::documentDetached(*this);

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
        for (auto* client : copyToVector(m_clientToIDMap.keys()))
            removePlaybackTargetPickerClient(*client);
    }
#endif

    m_cachedResourceLoader->stopUnusedPreloadsTimer();

    if (page() && m_mediaState != MediaProducer::IsNotPlaying) {
        m_mediaState = MediaProducer::IsNotPlaying;
        page()->updateIsPlayingMedia(HTMLMediaElementInvalidID);
    }

    selection().willBeRemovedFromFrame();
    editor().clear();
    detachFromFrame();

#if ENABLE(CSS_PAINTING_API)
    for (auto& scope : m_paintWorkletGlobalScopes.values())
        scope->prepareForDestruction();
    m_paintWorkletGlobalScopes.clear();
#endif

    m_hasPreparedForDestruction = true;

    // Note that m_backForwardCacheState can be Document::AboutToEnterBackForwardCache if our frame
    // was removed in an onpagehide event handler fired when the top-level frame is
    // about to enter the back/forward cache.
    RELEASE_ASSERT(m_backForwardCacheState != Document::InBackForwardCache);
}

void Document::removeAllEventListeners()
{
    EventTarget::removeAllEventListeners();

    if (m_domWindow)
        m_domWindow->removeAllEventListeners();

#if ENABLE(IOS_TOUCH_EVENTS)
    clearTouchEventHandlersAndListeners();
#endif
    for (Node* node = firstChild(); node; node = NodeTraversal::next(*node))
        node->removeAllEventListeners();

#if ENABLE(TOUCH_EVENTS)
    m_touchEventTargets = nullptr;
#endif
    m_wheelEventTargets = nullptr;
}

void Document::suspendDeviceMotionAndOrientationUpdates()
{
    if (m_areDeviceMotionAndOrientationUpdatesSuspended)
        return;
    m_areDeviceMotionAndOrientationUpdatesSuspended = true;
#if ENABLE(DEVICE_ORIENTATION) && PLATFORM(IOS_FAMILY)
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
#if ENABLE(DEVICE_ORIENTATION) && PLATFORM(IOS_FAMILY)
    if (m_deviceMotionController)
        m_deviceMotionController->resumeUpdates();
    if (m_deviceOrientationController)
        m_deviceOrientationController->resumeUpdates();
#endif
}

bool Document::shouldBypassMainWorldContentSecurityPolicy() const
{
    // Bypass this policy when the world is known, and it not the normal world.
    JSC::VM& vm = commonVM();
    auto& callFrame = *vm.topCallFrame;
    return &callFrame != JSC::CallFrame::noCaller() && !currentWorld(*callFrame.lexicalGlobalObject(vm)).isNormal();
}

void Document::platformSuspendOrStopActiveDOMObjects()
{
#if PLATFORM(IOS_FAMILY)
    contentChangeObserver().didSuspendActiveDOMObjects();
#endif
}

void Document::suspendActiveDOMObjects(ReasonForSuspension why)
{
    if (m_documentTaskGroup)
        m_documentTaskGroup->suspend();
    ScriptExecutionContext::suspendActiveDOMObjects(why);
    suspendDeviceMotionAndOrientationUpdates();
    platformSuspendOrStopActiveDOMObjects();
}

void Document::resumeActiveDOMObjects(ReasonForSuspension why)
{
    if (m_documentTaskGroup)
        m_documentTaskGroup->resume();
    ScriptExecutionContext::resumeActiveDOMObjects(why);
    resumeDeviceMotionAndOrientationUpdates();
    // FIXME: For iOS, do we need to add content change observers that were removed in Document::suspendActiveDOMObjects()?
}

void Document::stopActiveDOMObjects()
{
    if (m_documentTaskGroup)
        m_documentTaskGroup->stopAndDiscardAllTasks();
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

AXObjectCache* Document::existingAXObjectCacheSlow() const
{
    ASSERT(hasEverCreatedAnAXObjectCache);
    return topDocument().m_axObjectCache.get();
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
    if (!topDocument.m_axObjectCache) {
        topDocument.m_axObjectCache = makeUnique<AXObjectCache>(topDocument);
        hasEverCreatedAnAXObjectCache = true;
    }
    return topDocument.m_axObjectCache.get();
}

void Document::setVisuallyOrdered()
{
    m_visuallyOrdered = true;
    if (renderView())
        renderView()->mutableStyle().setRTLOrdering(Order::Visual);
}

Ref<DocumentParser> Document::createParser()
{
    // FIXME: this should probably pass the frame instead
    return XMLDocumentParser::create(*this, view());
}

HighlightMap& Document::highlightMap()
{
    if (!m_highlightMap)
        m_highlightMap = HighlightMap::create();
    return *m_highlightMap;
}

void Document::updateHighlightPositions()
{
    Vector<WeakPtr<HighlightRangeData>> rangesData;
    if (m_highlightMap) {
        for (auto& highlight : m_highlightMap->map()) {
            for (auto& rangeData : highlight.value->rangesData()) {
                if (rangeData->startPosition && rangeData->endPosition)
                    continue;
                if (&rangeData->range->startContainer().treeScope() != &rangeData->range->endContainer().treeScope())
                    continue;
                rangesData.append(makeWeakPtr(rangeData.ptr()));
            }
        }
    }
    
    for (auto& weakRangeData : rangesData) {
        if (auto* rangeData = weakRangeData.get()) {
            VisibleSelection visibleSelection(rangeData->range);
            Position startPosition;
            Position endPosition;
            if (!rangeData->startPosition.hasValue())
                startPosition = visibleSelection.visibleStart().deepEquivalent();
            if (!rangeData->endPosition.hasValue())
                endPosition = visibleSelection.visibleEnd().deepEquivalent(); // <MMG> switch to END
            if (!weakRangeData.get())
                continue;
            
            rangeData->startPosition = startPosition;
            rangeData->endPosition = endPosition;
        }
    }
}

ScriptableDocumentParser* Document::scriptableDocumentParser() const
{
    return parser() ? parser()->asScriptableDocumentParser() : nullptr;
}

ExceptionOr<RefPtr<WindowProxy>> Document::openForBindings(DOMWindow& activeWindow, DOMWindow& firstWindow, const String& url, const AtomString& name, const String& features)
{
    if (!m_domWindow)
        return Exception { InvalidAccessError };

    return m_domWindow->open(activeWindow, firstWindow, url, name, features);
}

ExceptionOr<Document&> Document::openForBindings(Document* responsibleDocument, const String&, const String&)
{
    if (!isHTMLDocument() || m_throwOnDynamicMarkupInsertionCount)
        return Exception { InvalidStateError };

    auto result = open(responsibleDocument);
    if (UNLIKELY(result.hasException()))
        return result.releaseException();

    return *this;
}

ExceptionOr<void> Document::open(Document* responsibleDocument)
{
    if (responsibleDocument && !responsibleDocument->securityOrigin().isSameOriginAs(securityOrigin()))
        return Exception { SecurityError };

    if (m_ignoreOpensDuringUnloadCount)
        return { };

    if (m_activeParserWasAborted)
        return { };

    if (m_frame) {
        if (ScriptableDocumentParser* parser = scriptableDocumentParser()) {
            if (parser->isParsing()) {
                // FIXME: HTML5 doesn't tell us to check this, it might not be correct.
                if (parser->isExecutingScript())
                    return { };

                if (!parser->wasCreatedByScript() && parser->hasInsertionPoint())
                    return { };
            }
        }

        if (m_frame->loader().policyChecker().delegateIsDecidingNavigationPolicy())
            m_frame->loader().policyChecker().stopCheck();
        if (m_frame && m_frame->loader().state() == FrameStateProvisional)
            m_frame->loader().stopAllLoaders();
    }

    removeAllEventListeners();

    if (responsibleDocument && isFullyActive()) {
        auto newURL = responsibleDocument->url();
        if (responsibleDocument != this)
            newURL.removeFragmentIdentifier();
        setURL(newURL);
        auto newCookieURL = responsibleDocument->cookieURL();
        if (responsibleDocument != this)
            newCookieURL.removeFragmentIdentifier();
        setCookieURL(newCookieURL);
        setSecurityOriginPolicy(responsibleDocument->securityOriginPolicy());
    }

    implicitOpen();
    if (ScriptableDocumentParser* parser = scriptableDocumentParser())
        parser->setWasCreatedByScript(true);

    if (m_frame)
        m_frame->loader().didExplicitOpen();

    return { };
}

// https://html.spec.whatwg.org/#fully-active
bool Document::isFullyActive() const
{
    auto* frame = this->frame();
    if (!frame || frame->document() != this)
        return false;

    if (frame->isMainFrame())
        return true;

    auto* parentFrame = frame->tree().parent();
    return parentFrame && parentFrame->document() && parentFrame->document()->isFullyActive();
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

    if (m_parser->processingData())
        m_activeParserWasAborted = true;

    // We have to clear the parser to avoid possibly triggering
    // the onload handler when closing as a side effect of a cancel-style
    // change, such as opening a new document or closing the window while
    // still parsing
    detachParser();
    explicitClose();
}

void Document::implicitOpen()
{
    removeChildren();

    setCompatibilityMode(DocumentCompatibilityMode::NoQuirksMode);

    detachParser();
    m_parser = createParser();

    if (hasActiveParserYieldToken())
        m_parser->didBeginYieldingParser();

    setParsing(true);
    setReadyState(Loading);
}

HTMLBodyElement* Document::body() const
{
    auto* element = documentElement();
    if (!is<HTMLHtmlElement>(element))
        return nullptr;
    return childrenOfType<HTMLBodyElement>(*element).first();
}

HTMLElement* Document::bodyOrFrameset() const
{
    // Return the first body or frameset child of the html element.
    auto* element = documentElement();
    if (!is<HTMLHtmlElement>(element))
        return nullptr;
    for (auto& child : childrenOfType<HTMLElement>(*element)) {
        if (is<HTMLBodyElement>(child) || is<HTMLFrameSetElement>(child))
            return &child;
    }
    return nullptr;
}

ExceptionOr<void> Document::setBodyOrFrameset(RefPtr<HTMLElement>&& newBody)
{
    if (!is<HTMLBodyElement>(newBody) && !is<HTMLFrameSetElement>(newBody))
        return Exception { HierarchyRequestError };

    auto* currentBody = bodyOrFrameset();
    if (newBody == currentBody)
        return { };

    if (!m_documentElement)
        return Exception { HierarchyRequestError };

    if (currentBody)
        return m_documentElement->replaceChild(*newBody, *currentBody);
    return m_documentElement->appendChild(*newBody);
}

Location* Document::location() const
{
    auto* window = domWindow();
    return window ? &window->location() : nullptr;
}

HTMLHeadElement* Document::head()
{
    if (auto element = documentElement())
        return childrenOfType<HTMLHeadElement>(*element).first();
    return nullptr;
}

ExceptionOr<void> Document::closeForBindings()
{
    // FIXME: We should follow the specification more closely:
    //        http://www.whatwg.org/specs/web-apps/current-work/#dom-document-close

    if (!isHTMLDocument() || m_throwOnDynamicMarkupInsertionCount)
        return Exception { InvalidStateError };

    close();
    return { };
}

void Document::close()
{
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

    checkCompleted();
}

void Document::implicitClose()
{
    RELEASE_ASSERT(!m_inStyleRecalc);
    bool wasLocationChangePending = frame() && frame()->navigationScheduler().locationChangePending();
    bool doload = !parsing() && m_parser && !m_processingLoadEvent && !wasLocationChangePending;
    
    if (!doload)
        return;

    // Call to dispatchWindowLoadEvent can blow us from underneath.
    Ref<Document> protectedThis(*this);

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
    RefPtr<Frame> f = frame();
    if (f) {
#if ENABLE(XSLT)
        // Apply XSL transforms before load events so that event handlers can access the transformed DOM tree.
        applyPendingXSLTransformsNowIfScheduled();
#endif

        if (auto* documentLoader = loader())
            documentLoader->startIconLoading();

        f->legacyAnimation().startAnimationsIfNotSuspended(this);

        // FIXME: We shouldn't be dispatching pending events globally on all Documents here.
        // For now, only do this when there is a Frame, otherwise this could cause JS reentrancy
        // below SVG font parsing, for example. <https://webkit.org/b/136269>
        if (auto* currentPage = page()) {
            ImageLoader::dispatchPendingBeforeLoadEvents(currentPage);
            ImageLoader::dispatchPendingLoadEvents(currentPage);
            ImageLoader::dispatchPendingErrorEvents(currentPage);
            HTMLLinkElement::dispatchPendingLoadEvents(currentPage);
            HTMLStyleElement::dispatchPendingLoadEvents(currentPage);
        }

        if (svgExtensions())
            accessSVGExtensions().dispatchLoadEventToOutermostSVGElements();
    }

    dispatchWindowLoadEvent();
    dispatchPageshowEvent(PageshowEventNotPersisted);
    if (m_pendingStateObject)
        dispatchPopstateEvent(WTFMove(m_pendingStateObject));

    if (f)
        f->loader().dispatchOnloadEvents();

    // An event handler may have removed the frame
    if (!frame()) {
        m_processingLoadEvent = false;
        return;
    }

    frame()->loader().checkCallImplicitClose();
    
    // We used to force a synchronous display and flush here. This really isn't
    // necessary and can in fact be actively harmful if pages are loading at a rate of > 60fps
    // (if your platform is syncing flushes and limiting them to 60fps).
    if (!ownerElement() || (ownerElement()->renderer() && !ownerElement()->renderer()->needsLayout())) {
        updateStyleIfNeeded();
        
        // Always do a layout after loading if needed.
        if (view() && renderView() && (!renderView()->firstChild() || renderView()->needsLayout()))
            view()->layoutContext().layout();
    }

    m_processingLoadEvent = false;

    if (auto fontFaceSet = makeRefPtr(fontSelector().fontFaceSetIfExists()))
        fontFaceSet->documentDidFinishLoading();

#if PLATFORM(COCOA) || PLATFORM(WIN) || PLATFORM(GTK)
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
        m_sharedObjectPool = makeUnique<DocumentSharedObjectPool>();

    if (!m_bParsing && view() && !view()->needsLayout())
        view()->fireLayoutRelatedMilestonesIfNeeded();
}

bool Document::shouldScheduleLayout() const
{
    if (!documentElement())
        return false;
    if (!is<HTMLHtmlElement>(*documentElement()))
        return true;
    if (!bodyOrFrameset())
        return false;
    if (styleScope().hasPendingSheetsBeforeBody())
        return false;
    if (view() && !view()->isVisuallyNonEmpty())
        return false;
    return true;
}
    
bool Document::isLayoutTimerActive() const
{
    return view() && view()->layoutContext().isLayoutPending();
}

bool Document::supportsPaintTiming() const
{
    return RuntimeEnabledFeatures::sharedFeatures().paintTimingEnabled() && securityOrigin().canAccess(topOrigin());
}

// https://w3c.github.io/paint-timing/#ref-for-mark-paint-timing
void Document::enqueuePaintTimingEntryIfNeeded()
{
    if (m_didEnqueueFirstContentfulPaint)
        return;

    if (!supportsPaintTiming())
        return;

    if (!domWindow() || !view())
        return;

    // To make sure we don't report paint while the layer tree is still frozen.
    if (!view()->isVisuallyNonEmpty() || view()->needsLayout())
        return;

    if (!ContentfulPaintChecker::qualifiesForContentfulPaint(*view()))
        return;

    domWindow()->performance().reportFirstContentfulPaint();
    m_didEnqueueFirstContentfulPaint = true;
}

ExceptionOr<void> Document::write(Document* responsibleDocument, SegmentedString&& text)
{
    if (m_activeParserWasAborted)
        return { };

    NestingLevelIncrementer nestingLevelIncrementer(m_writeRecursionDepth);

    m_writeRecursionIsTooDeep = (m_writeRecursionDepth > 1) && m_writeRecursionIsTooDeep;
    m_writeRecursionIsTooDeep = (m_writeRecursionDepth > cMaxWriteRecursionDepth) || m_writeRecursionIsTooDeep;

    if (m_writeRecursionIsTooDeep)
        return { };

    bool hasInsertionPoint = m_parser && m_parser->hasInsertionPoint();
    if (!hasInsertionPoint && (m_ignoreOpensDuringUnloadCount || m_ignoreDestructiveWriteCount))
        return { };

    if (!hasInsertionPoint) {
        auto result = open(responsibleDocument);
        if (UNLIKELY(result.hasException()))
            return result.releaseException();
    }

    ASSERT(m_parser);
    m_parser->insert(WTFMove(text));
    return { };
}

ExceptionOr<void> Document::write(Document* responsibleDocument, Vector<String>&& strings)
{
    if (!isHTMLDocument() || m_throwOnDynamicMarkupInsertionCount)
        return Exception { InvalidStateError };

    SegmentedString text;
    for (auto& string : strings)
        text.append(WTFMove(string));

    return write(responsibleDocument, WTFMove(text));
}

ExceptionOr<void> Document::writeln(Document* responsibleDocument, Vector<String>&& strings)
{
    if (!isHTMLDocument() || m_throwOnDynamicMarkupInsertionCount)
        return Exception { InvalidStateError };

    SegmentedString text;
    for (auto& string : strings)
        text.append(WTFMove(string));

    text.append("\n"_s);
    return write(responsibleDocument, WTFMove(text));
}

Seconds Document::minimumDOMTimerInterval() const
{
    auto* page = this->page();
    if (!page)
        return ScriptExecutionContext::minimumDOMTimerInterval();
    return page->settings().minimumDOMTimerInterval();
}

void Document::setTimerThrottlingEnabled(bool shouldThrottle)
{
    if (m_isTimerThrottlingEnabled == shouldThrottle)
        return;

    m_isTimerThrottlingEnabled = shouldThrottle;
    didChangeTimerAlignmentInterval();
}

Seconds Document::domTimerAlignmentInterval(bool hasReachedMaxNestingLevel) const
{
    auto alignmentInterval = ScriptExecutionContext::domTimerAlignmentInterval(hasReachedMaxNestingLevel);
    if (!hasReachedMaxNestingLevel)
        return alignmentInterval;

    // Apply Document-level DOMTimer throttling only if timers have reached their maximum nesting level as the Page may still be visible.
    if (m_isTimerThrottlingEnabled)
        alignmentInterval = std::max(alignmentInterval, DOMTimer::hiddenPageAlignmentInterval());

    if (Page* page = this->page())
        alignmentInterval = std::max(alignmentInterval, page->domTimerAlignmentInterval());

    if (!topOrigin().canAccess(securityOrigin()) && !hasHadUserInteraction())
        alignmentInterval = std::max(alignmentInterval, DOMTimer::nonInteractedCrossOriginFrameAlignmentInterval());

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
    const URL& newURL = url.isEmpty() ? aboutBlankURL() : url;
    if (newURL == m_url)
        return;

    m_url = newURL;
    if (SecurityOrigin::shouldIgnoreHost(m_url))
        m_url.setHostAndPort({ });

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
        m_baseURL = URL({ }, documentURI());
    }

    clearSelectorQueryCache();

    if (!m_baseURL.isValid())
        m_baseURL = URL();
}

void Document::setBaseURLOverride(const URL& url)
{
    m_baseURLOverride = url;
    updateBaseURL();
}

void Document::processBaseElement()
{
    // Find the first href attribute in a base element and the first target attribute in a base element.
    const AtomString* href = nullptr;
    const AtomString* target = nullptr;
    auto baseDescendants = descendantsOfType<HTMLBaseElement>(*this);
    for (auto& base : baseDescendants) {
        if (!href) {
            const AtomString& value = base.attributeWithoutSynchronization(hrefAttr);
            if (!value.isNull()) {
                href = &value;
                if (target)
                    break;
            }
        }
        if (!target) {
            const AtomString& value = base.attributeWithoutSynchronization(targetAttr);
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
        if (settings().shouldRestrictBaseURLSchemes() && !SecurityPolicy::isBaseURLSchemeAllowed(baseElementURL))
            addConsoleMessage(MessageSource::Security, MessageLevel::Error, "Blocked setting " + baseElementURL.stringCenterEllipsizedToLength() + " as the base URL because it does not have an allowed scheme.");
        else {
            m_baseElementURL = baseElementURL;
            updateBaseURL();
        }
    }

    m_baseTarget = target ? *target : nullAtom();
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

void Document::disableWebAssembly(const String& errorMessage)
{
    if (!frame())
        return;

    frame()->script().disableWebAssembly(errorMessage);
}

#if ENABLE(INDEXED_DATABASE)
IDBClient::IDBConnectionProxy* Document::idbConnectionProxy()
{
    if (!m_idbConnectionProxy) {
        Page* currentPage = page();
        if (!currentPage)
            return nullptr;
        m_idbConnectionProxy = &currentPage->idbConnection().proxy();
    }
    return m_idbConnectionProxy.get();
}
#endif

SocketProvider* Document::socketProvider()
{
    return m_socketProvider.get();
}
    
bool Document::canNavigate(Frame* targetFrame, const URL& destinationURL)
{
    if (!m_frame)
        return false;

    // FIXME: We shouldn't call this function without a target frame, but
    // fast/forms/submit-to-blank-multiple-times.html depends on this function
    // returning true when supplied with a 0 targetFrame.
    if (!targetFrame)
        return true;

    if (!canNavigateInternal(*targetFrame))
        return false;

    if (isNavigationBlockedByThirdPartyIFrameRedirectBlocking(*targetFrame, destinationURL)) {
        printNavigationErrorMessage(*targetFrame, url(), "The frame attempting navigation of the top-level window is cross-origin or untrusted and the user has never interacted with the frame."_s);
        RELEASE_LOG_ERROR_IF_ALLOWED(Loading, "Navigation was prevented because it was triggered by a cross-origin or untrusted iframe");
        return false;
    }

    return true;
}

bool Document::canNavigateInternal(Frame& targetFrame)
{
    ASSERT(m_frame);

    // Cases (i), (ii) and (iii) pass the tests from the specifications but might not pass the "security origin" tests.

    // i. A frame can navigate its top ancestor when its 'allow-top-navigation' flag is set (sometimes known as 'frame-busting').
    if (!isSandboxed(SandboxTopNavigation) && &targetFrame == &m_frame->tree().top())
        return true;

    // ii. A frame can navigate its top ancestor when its 'allow-top-navigation-by-user-activation' flag is set and navigation is triggered by user activation.
    if (!isSandboxed(SandboxTopNavigationByUserActivation) && UserGestureIndicator::processingUserGesture() && &targetFrame == &m_frame->tree().top())
        return true;

    // iii. A sandboxed frame can always navigate its descendants.
    if (isSandboxed(SandboxNavigation) && targetFrame.tree().isDescendantOf(m_frame))
        return true;

    // From https://html.spec.whatwg.org/multipage/browsers.html#allowed-to-navigate.
    // 1. If A is not the same browsing context as B, and A is not one of the ancestor browsing contexts of B, and B is not a top-level browsing context, and A's active document's active sandboxing
    // flag set has its sandboxed navigation browsing context flag set, then abort these steps negatively.
    if (m_frame != &targetFrame && isSandboxed(SandboxNavigation) && targetFrame.tree().parent() && !targetFrame.tree().isDescendantOf(m_frame)) {
        printNavigationErrorMessage(targetFrame, url(), "The frame attempting navigation is sandboxed, and is therefore disallowed from navigating its ancestors."_s);
        return false;
    }

    // 2. Otherwise, if B is a top-level browsing context, and is one of the ancestor browsing contexts of A, then:
    if (m_frame != &targetFrame && &targetFrame == &m_frame->tree().top()) {
        bool triggeredByUserActivation = UserGestureIndicator::processingUserGesture();
        // 1. If this algorithm is triggered by user activation and A's active document's active sandboxing flag set has its sandboxed top-level navigation with user activation browsing context flag set, then abort these steps negatively.
        if (triggeredByUserActivation && isSandboxed(SandboxTopNavigationByUserActivation)) {
            printNavigationErrorMessage(targetFrame, url(), "The frame attempting navigation of the top-level window is sandboxed, but the 'allow-top-navigation-by-user-activation' flag is not set and navigation is not triggered by user activation."_s);
            return false;
        }
        // 2. Otherwise, If this algorithm is not triggered by user activation and A's active document's active sandboxing flag set has its sandboxed top-level navigation without user activation browsing context flag set, then abort these steps negatively.
        if (!triggeredByUserActivation && isSandboxed(SandboxTopNavigation)) {
            printNavigationErrorMessage(targetFrame, url(), "The frame attempting navigation of the top-level window is sandboxed, but the 'allow-top-navigation' flag is not set."_s);
            return false;
        }
    }

    // 3. Otherwise, if B is a top-level browsing context, and is neither A nor one of the ancestor browsing contexts of A, and A's Document's active sandboxing flag set has its
    // sandboxed navigation browsing context flag set, and A is not the one permitted sandboxed navigator of B, then abort these steps negatively.
    if (!targetFrame.tree().parent() && m_frame != &targetFrame && &targetFrame != &m_frame->tree().top() && isSandboxed(SandboxNavigation) && targetFrame.loader().opener() != m_frame) {
        printNavigationErrorMessage(targetFrame, url(), "The frame attempting navigation is sandboxed, and is not allowed to navigate this popup."_s);
        return false;
    }

    // 4. Otherwise, terminate positively!

    // This is the normal case. A document can navigate its descendant frames,
    // or, more generally, a document can navigate a frame if the document is
    // in the same origin as any of that frame's ancestors (in the frame
    // hierarchy).
    //
    // See http://www.adambarth.com/papers/2008/barth-jackson-mitchell.pdf for
    // historical information about this security check.
    if (canAccessAncestor(securityOrigin(), &targetFrame))
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
    if (!targetFrame.tree().parent()) {
        if (&targetFrame == m_frame->loader().opener())
            return true;

        if (canAccessAncestor(securityOrigin(), targetFrame.loader().opener()))
            return true;
    }

    printNavigationErrorMessage(targetFrame, url(), "The frame attempting navigation is neither same-origin with the target, nor is it the target's parent or opener.");
    return false;
}

void Document::willLoadScriptElement(const URL& scriptURL)
{
    m_hasLoadedThirdPartyScript = m_hasLoadedThirdPartyScript || !securityOrigin().isSameOriginAs(SecurityOrigin::create(scriptURL));
}

void Document::willLoadFrameElement(const URL& frameURL)
{
    m_hasLoadedThirdPartyFrame = m_hasLoadedThirdPartyFrame || !securityOrigin().isSameOriginAs(SecurityOrigin::create(frameURL));
}

// Prevent cross-site top-level redirects from third-party iframes unless the user has ever interacted with the frame.
bool Document::isNavigationBlockedByThirdPartyIFrameRedirectBlocking(Frame& targetFrame, const URL& destinationURL)
{
    if (!settings().thirdPartyIframeRedirectBlockingEnabled())
        return false;

    // Only prevent top frame navigations by subframes.
    if (m_frame == &targetFrame || &targetFrame != &m_frame->tree().top())
        return false;

    // Only prevent navigations by subframes that the user has not interacted with.
    if (m_frame->hasHadUserInteraction())
        return false;

    // Only prevent navigations by unsandboxed iframes. Such navigations by unsandboxed iframes would have already been blocked unless
    // "allow-top-navigation" / "allow-top-navigation-by-user-activation" was explicitly specified.
    if (sandboxFlags() != SandboxNone)
        return false;

    // Only prevent navigations by third-party iframes or untrusted first-party iframes.
    bool isUntrustedIframe = m_hasLoadedThirdPartyScript && m_hasLoadedThirdPartyFrame;
    if (canAccessAncestor(securityOrigin(), &targetFrame) && !isUntrustedIframe)
        return false;

    // Only prevent cross-site navigations.
    auto* targetDocument = targetFrame.document();
    if (targetDocument && (targetDocument->securityOrigin().canAccess(SecurityOrigin::create(destinationURL)) || areRegistrableDomainsEqual(targetDocument->url(), destinationURL)))
        return false;

    return true;
}

void Document::didRemoveAllPendingStylesheet()
{
    if (auto* parser = scriptableDocumentParser())
        parser->executeScriptsWaitingForStylesheetsSoon();

    if (m_gotoAnchorNeededAfterStylesheetsLoad) {
        // https://html.spec.whatwg.org/multipage/browsing-the-web.html#try-to-scroll-to-the-fragment
        eventLoop().queueTask(TaskSource::Networking, [protectedThis = makeRef(*this), this] {
            auto frameView = makeRefPtr(view());
            if (!frameView)
                return;
            if (!haveStylesheetsLoaded()) {
                m_gotoAnchorNeededAfterStylesheetsLoad = true;
                return;
            }
            frameView->scrollToFragment(m_url);
        });
    }
}

bool Document::usesStyleBasedEditability() const
{
    if (m_hasElementUsingStyleBasedEditability)
        return true;

    ASSERT(!m_renderView || !m_renderView->frameView().isPainting());
    ASSERT(!m_inStyleRecalc);

    auto& styleScope = const_cast<Style::Scope&>(this->styleScope());
    styleScope.flushPendingUpdate();
    return styleScope.usesStyleBasedEditability();
}

void Document::setHasElementUsingStyleBasedEditability()
{
    m_hasElementUsingStyleBasedEditability = true;
}

void Document::processHttpEquiv(const String& equiv, const String& content, bool isInDocumentHead)
{
    ASSERT(!equiv.isNull());
    ASSERT(!content.isNull());

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
    auto* documentLoader = frame ? frame->loader().documentLoader() : nullptr;
    auto httpStatusCode = documentLoader ? documentLoader->response().httpStatusCode() : 0;

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
        styleScope().setPreferredStylesheetSetName(content);
        break;

    case HTTPHeaderName::Refresh: {
        double delay = 0;
        String urlString;
        if (frame && parseMetaHTTPEquivRefresh(content, delay, urlString)) {
            URL completedURL;
            if (urlString.isEmpty())
                completedURL = m_url;
            else
                completedURL = completeURL(urlString);
            if (!completedURL.protocolIsJavaScript())
                frame->navigationScheduler().scheduleRedirect(*this, delay, completedURL);
            else {
                String message = "Refused to refresh " + m_url.stringCenterEllipsizedToLength() + " to a javascript: URL";
                addConsoleMessage(MessageSource::Security, MessageLevel::Error, message);
            }
        }

        break;
    }

    case HTTPHeaderName::SetCookie:
        if (isHTMLDocument())
            addConsoleMessage(MessageSource::Security, MessageLevel::Error, "The Set-Cookie meta tag is obsolete and was ignored. Use the HTTP header Set-Cookie or document.cookie instead."_s);
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

            String message = "The X-Frame-Option '" + content + "' supplied in a <meta> element was ignored. X-Frame-Options may only be provided by an HTTP header sent with the document.";
            addConsoleMessage(MessageSource::Security, MessageLevel::Error, message, requestIdentifier);
        }
        break;

    case HTTPHeaderName::ContentSecurityPolicy:
        if (isInDocumentHead)
            contentSecurityPolicy()->didReceiveHeader(content, ContentSecurityPolicyHeaderType::Enforce, ContentSecurityPolicy::PolicyFrom::HTTPEquivMeta, referrer(), httpStatusCode);
        break;

    default:
        break;
    }
}

void Document::processDisabledAdaptations(const String& disabledAdaptationsString)
{
    auto disabledAdaptations = parseDisabledAdaptations(disabledAdaptationsString);
    if (m_disabledAdaptations == disabledAdaptations)
        return;

    m_disabledAdaptations = disabledAdaptations;
    dispatchDisabledAdaptationsDidChangeForMainFrame();
}

void Document::dispatchDisabledAdaptationsDidChangeForMainFrame()
{
    if (!frame()->isMainFrame())
        return;

    if (!page())
        return;

    page()->chrome().dispatchDisabledAdaptationsDidChange(m_disabledAdaptations);
}

void Document::processViewport(const String& features, ViewportArguments::Type origin)
{
    ASSERT(!features.isNull());

    LOG_WITH_STREAM(Viewports, stream << "Document::processViewport " << features);

    if (origin < m_viewportArguments.type)
        return;

    m_viewportArguments = ViewportArguments(origin);

    LOG_WITH_STREAM(Viewports, stream  << " resolved to " << m_viewportArguments);

    processFeaturesString(features, FeatureMode::Viewport, [this](StringView key, StringView value) {
        setViewportFeature(m_viewportArguments, *this, key, value);
    });

    updateViewportArguments();
}

ViewportArguments Document::viewportArguments() const
{
    auto* page = this->page();
    if (!page)
        return m_viewportArguments;
    return page->overrideViewportArguments().valueOr(m_viewportArguments);
}

void Document::updateViewportArguments()
{
    if (page() && frame()->isMainFrame()) {
#if ASSERT_ENABLED
        m_didDispatchViewportPropertiesChanged = true;
#endif
        page()->chrome().dispatchViewportPropertiesDidChange(viewportArguments());
        page()->chrome().didReceiveDocType(*frame());
    }
}

#if ENABLE(DARK_MODE_CSS)
static void processColorSchemeString(StringView colorScheme, const WTF::Function<void(StringView key)>& callback)
{
    unsigned length = colorScheme.length();
    for (unsigned i = 0; i < length; ) {
        // Skip to first non-separator.
        while (i < length && isHTMLSpace(colorScheme[i]))
            ++i;
        unsigned keyBegin = i;

        // Skip to first separator.
        while (i < length && !isHTMLSpace(colorScheme[i]))
            ++i;
        unsigned keyEnd = i;

        if (keyBegin == keyEnd)
            continue;

        callback(colorScheme.substring(keyBegin, keyEnd - keyBegin));
    }
}

void Document::processColorScheme(const String& colorSchemeString)
{
    OptionSet<ColorScheme> colorScheme;
    bool allowsTransformations = true;
    bool autoEncountered = false;

    processColorSchemeString(colorSchemeString, [&] (StringView key) {
        if (equalLettersIgnoringASCIICase(key, "auto")) {
            colorScheme = { };
            allowsTransformations = true;
            autoEncountered = true;
            return;
        }

        if (autoEncountered)
            return;

        if (equalLettersIgnoringASCIICase(key, "light"))
            colorScheme.add(ColorScheme::Light);
        else if (equalLettersIgnoringASCIICase(key, "dark"))
            colorScheme.add(ColorScheme::Dark);
        else if (equalLettersIgnoringASCIICase(key, "only"))
            allowsTransformations = false;
    });

    // If the value was just "only", that is synonymous for "only light".
    if (colorScheme.isEmpty() && !allowsTransformations)
        colorScheme.add(ColorScheme::Light);

    m_colorScheme = colorScheme;
    m_allowsColorSchemeTransformations = allowsTransformations;

    if (auto* frameView = view())
        frameView->recalculateBaseBackgroundColor();

    if (auto* page = this->page())
        page->updateStyleAfterChangeInEnvironment();
}
#endif

#if PLATFORM(IOS_FAMILY)

void Document::processFormatDetection(const String& features)
{
    // FIXME: Find a better place for this function.
    processFeaturesString(features, FeatureMode::Viewport, [this](StringView key, StringView value) {
        if (equalLettersIgnoringASCIICase(key, "telephone") && equalLettersIgnoringASCIICase(value, "no"))
            m_isTelephoneNumberParsingAllowed = false;
    });
}

void Document::processWebAppOrientations()
{
    if (Page* page = this->page())
        page->chrome().client().webAppOrientationsUpdated();
}

#endif

void Document::processReferrerPolicy(const String& policy, ReferrerPolicySource source)
{
    ASSERT(!policy.isNull());

    // Documents in a Content-Disposition: attachment sandbox should never send a Referer header,
    // even if the document has a meta tag saying otherwise.
    if (shouldEnforceContentDispositionAttachmentSandbox())
        return;

#if USE(QUICK_LOOK)
    if (shouldEnforceQuickLookSandbox())
        return;
#endif
    
    auto referrerPolicy = parseReferrerPolicy(policy, source);
    if (!referrerPolicy) {
        // Unknown policy values are ignored (https://w3c.github.io/webappsec-referrer-policy/#unknown-policy-values).
        addConsoleMessage(MessageSource::Rendering, MessageLevel::Error, "Failed to set referrer policy: The value '" + policy + "' is not one of 'no-referrer', 'no-referrer-when-downgrade', 'same-origin', 'origin', 'strict-origin', 'origin-when-cross-origin', 'strict-origin-when-cross-origin' or 'unsafe-url'.");
        return;
    }
    setReferrerPolicy(referrerPolicy.value());
}

MouseEventWithHitTestResults Document::prepareMouseEvent(const HitTestRequest& request, const LayoutPoint& documentPoint, const PlatformMouseEvent& event)
{
    if (!hasLivingRenderTree())
        return MouseEventWithHitTestResults(event, HitTestResult(LayoutPoint()));

    HitTestResult result(documentPoint);
    hitTest(request, result);

    auto captureElementChanged = CaptureChange::No;
    if (!request.readOnly()) {
        auto targetElement = makeRefPtr(result.targetElement());
        if (auto* page = this->page()) {
            // Before we dispatch a new mouse event, we must run the Process Pending Capture Element steps as defined
            // in https://w3c.github.io/pointerevents/#process-pending-pointer-capture.
            auto& pointerCaptureController = page->pointerCaptureController();
            auto* previousCaptureElement = pointerCaptureController.pointerCaptureElement(this, event.pointerId());
            pointerCaptureController.processPendingPointerCapture(event.pointerId());
            auto* captureElement = pointerCaptureController.pointerCaptureElement(this, event.pointerId());
            // If the capture element has changed while running the Process Pending Capture Element steps then
            // we need to indicate that when calling updateHoverActiveState to be sure that the :active and :hover
            // element chains are updated.
            if (previousCaptureElement != captureElement)
                captureElementChanged = CaptureChange::Yes;
            // If we have a capture element, we must target it instead of what would normally hit-test for this event.
            if (captureElement)
                targetElement = captureElement;
        }
        updateHoverActiveState(request, targetElement.get(), captureElementChanged);
    }

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
    if (operation == AcceptChildOperation::Replace && refChild->parentNode() == this && refChild->nodeType() == newChild.nodeType())
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
    return clone;
}

Ref<Document> Document::cloneDocumentWithoutChildren() const
{
    if (isXMLDocument()) {
        if (isXHTMLDocument())
            return XMLDocument::createXHTML(nullptr, url());
        return XMLDocument::create(nullptr, url());
    }
    return create(url());
}

void Document::cloneDataFromDocument(const Document& other)
{
    ASSERT(m_url == other.url());
    m_baseURL = other.baseURL();
    m_baseURLOverride = other.baseURLOverride();
    m_documentURI = other.documentURI();

    setCompatibilityMode(other.m_compatibilityMode);
    setContextDocument(other.contextDocument());
    setSecurityOriginPolicy(other.securityOriginPolicy());
    overrideMIMEType(other.contentType());
    setDecoder(other.decoder());
}

StyleSheetList& Document::styleSheets()
{
    if (!m_styleSheetList)
        m_styleSheetList = StyleSheetList::create(*this);
    return *m_styleSheetList;
}

void Document::updateElementsAffectedByMediaQueries()
{
    ScriptDisallowedScope::InMainThread scriptDisallowedScope;

    // FIXME: copyToVector doesn't work with WeakHashSet
    Vector<Ref<HTMLImageElement>> images;
    images.reserveInitialCapacity(m_dynamicMediaQueryDependentImages.computeSize());
    for (auto& image : m_dynamicMediaQueryDependentImages)
        images.append(image);

    for (auto& image : images)
        image->evaluateDynamicMediaQueryDependencies();
}

void Document::evaluateMediaQueriesAndReportChanges()
{
    if (!m_mediaQueryMatcher)
        return;

    m_mediaQueryMatcher->evaluateAll();
}

void Document::updateViewportUnitsOnResize()
{
    if (!hasStyleWithViewportUnits())
        return;

    styleScope().resolver().clearCachedDeclarationsAffectedByViewportUnits();

    // FIXME: Ideally, we should save the list of elements that have viewport units and only iterate over those.
    for (Element* element = ElementTraversal::firstWithin(rootNode()); element; element = ElementTraversal::nextIncludingPseudo(*element)) {
        auto* renderer = element->renderer();
        if (renderer && renderer->style().hasViewportUnits())
            element->invalidateStyle();
    }
}

void Document::setNeedsDOMWindowResizeEvent()
{
    m_needsDOMWindowResizeEvent = true;
    scheduleTimedRenderingUpdate();
}

void Document::setNeedsVisualViewportResize()
{
    m_needsVisualViewportResizeEvent = true;
    scheduleTimedRenderingUpdate();
}

// https://drafts.csswg.org/cssom-view/#run-the-resize-steps
void Document::runResizeSteps()
{
    // FIXME: The order of dispatching is not specified: https://github.com/WICG/visual-viewport/issues/65.
    if (m_needsDOMWindowResizeEvent) {
        LOG_WITH_STREAM(Events, stream << "Document" << this << "sending resize events to window");
        m_needsDOMWindowResizeEvent = false;
        dispatchWindowEvent(Event::create(eventNames().resizeEvent, Event::CanBubble::No, Event::IsCancelable::No));
    }
    if (m_needsVisualViewportResizeEvent) {
        LOG_WITH_STREAM(Events, stream << "Document" << this << "sending resize events to visualViewport");
        m_needsVisualViewportResizeEvent = false;
        if (auto* window = domWindow())
            window->visualViewport().dispatchEvent(Event::create(eventNames().resizeEvent, Event::CanBubble::No, Event::IsCancelable::No));
    }
}

void Document::addPendingScrollEventTarget(ContainerNode& target)
{
    if (!m_pendingScrollEventTargetList)
        m_pendingScrollEventTargetList = makeUnique<PendingScrollEventTargetList>();

    auto& targets = m_pendingScrollEventTargetList->targets;
    if (targets.findMatching([&] (auto& entry) { return entry.ptr() == &target; }) != notFound)
        return;

    if (targets.isEmpty())
        scheduleTimedRenderingUpdate();

    targets.append(target);
}

void Document::setNeedsVisualViewportScrollEvent()
{
    if (!m_needsVisualViewportScrollEvent)
        scheduleTimedRenderingUpdate();
    m_needsVisualViewportScrollEvent = true;
}

// https://drafts.csswg.org/cssom-view/#run-the-scroll-steps
void Document::runScrollSteps()
{
    // FIXME: The order of dispatching is not specified: https://github.com/WICG/visual-viewport/issues/66.
    if (m_pendingScrollEventTargetList && !m_pendingScrollEventTargetList->targets.isEmpty()) {
        LOG_WITH_STREAM(Events, stream << "Document" << this << "sending scroll events to pending scroll event targets");
        auto currentTargets = WTFMove(m_pendingScrollEventTargetList->targets);
        for (auto& target : currentTargets) {
            auto bubbles = target->isDocumentNode() ? Event::CanBubble::Yes : Event::CanBubble::No;
            target->dispatchEvent(Event::create(eventNames().scrollEvent, bubbles, Event::IsCancelable::No));
        }
    }
    if (m_needsVisualViewportScrollEvent) {
        LOG_WITH_STREAM(Events, stream << "Document" << this << "sending scroll events to visualViewport");
        m_needsVisualViewportResizeEvent = false;
        if (auto* window = domWindow())
            window->visualViewport().dispatchEvent(Event::create(eventNames().scrollEvent, Event::CanBubble::No, Event::IsCancelable::No));
    }
}

void Document::addAudioProducer(MediaProducer& audioProducer)
{
    m_audioProducers.add(audioProducer);
    updateIsPlayingMedia();
}

void Document::removeAudioProducer(MediaProducer& audioProducer)
{
    RELEASE_ASSERT(isMainThread());
    m_audioProducers.remove(audioProducer);
    updateIsPlayingMedia();
}

void Document::noteUserInteractionWithMediaElement()
{
    if (m_userHasInteractedWithMediaElement)
        return;

    if (!topDocument().userDidInteractWithPage())
        return;

    m_userHasInteractedWithMediaElement = true;
    updateIsPlayingMedia();
}

void Document::updateIsPlayingMedia(uint64_t sourceElementID)
{
    ASSERT(!m_audioProducers.hasNullReferences());
    MediaProducer::MediaStateFlags state = MediaProducer::IsNotPlaying;
    for (auto& audioProducer : m_audioProducers)
        state |= audioProducer.mediaState();

#if ENABLE(MEDIA_STREAM)
    state |= MediaStreamTrack::captureState(*this);
#endif

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

    if (m_userHasInteractedWithMediaElement)
        state |= MediaProducer::HasUserInteractedWithMediaElement;

    if (state == m_mediaState)
        return;

#if ENABLE(MEDIA_STREAM)
    bool captureStateChanged = MediaProducer::isCapturing(m_mediaState) != MediaProducer::isCapturing(state);
#endif
    
    m_mediaState = state;

    if (page())
        page()->updateIsPlayingMedia(sourceElementID);

#if ENABLE(MEDIA_STREAM)
    if (captureStateChanged)
        mediaStreamCaptureStateChanged();
#endif
}

void Document::pageMutedStateDidChange()
{
    for (auto& audioProducer : m_audioProducers)
        audioProducer.pageMutedStateDidChange();

#if ENABLE(MEDIA_STREAM)
    MediaStreamTrack::updateCaptureAccordingToMutedState(*this);
#endif
}

static bool isNodeInSubtree(Node& node, Node& container, Document::NodeRemoval nodeRemoval)
{
    if (nodeRemoval == Document::NodeRemoval::ChildrenOfNode)
        return node.isDescendantOf(container);

    return &node == &container || node.isDescendantOf(container);
}

void Document::adjustFocusedNodeOnNodeRemoval(Node& node, NodeRemoval nodeRemoval)
{
    if (!m_focusedElement || backForwardCacheState() != NotInBackForwardCache) // If the document is in the back/forward cache, then we don't need to clear out the focused node.
        return;

    Element* focusedElement = node.treeScope().focusedElementInScope();
    if (!focusedElement)
        return;

    if (isNodeInSubtree(*focusedElement, node, nodeRemoval)) {
        // FIXME: We should avoid synchronously updating the style inside setFocusedElement.
        // FIXME: Object elements should avoid loading a frame synchronously in a post style recalc callback.
        SubframeLoadingDisabler disabler(is<ContainerNode>(node) ? &downcast<ContainerNode>(node) : nullptr);
        setFocusedElement(nullptr, FocusDirectionNone, FocusRemovalEventsMode::DoNotDispatch);
        // Set the focus navigation starting node to the previous focused element so that
        // we can fallback to the siblings or parent node for the next search.
        // Also we need to call removeFocusNavigationNodeOfSubtree after this function because
        // setFocusedElement(nullptr) will reset m_focusNavigationStartingNode.
        setFocusNavigationStartingNode(focusedElement);
    }
}

void Document::hoveredElementDidDetach(Element& element)
{
    if (!m_hoveredElement || &element != m_hoveredElement)
        return;

    m_hoveredElement = element.parentElement();
    while (m_hoveredElement && !m_hoveredElement->renderer())
        m_hoveredElement = m_hoveredElement->parentElement();
    if (frame())
        frame()->eventHandler().scheduleHoverStateUpdate();
}

void Document::elementInActiveChainDidDetach(Element& element)
{
    if (!m_activeElement || &element != m_activeElement)
        return;

    m_activeElement = element.parentElement();
    while (m_activeElement && !m_activeElement->renderer())
        m_activeElement = m_activeElement->parentElement();
}

void Document::invalidateEventRegionsForFrame(HTMLFrameOwnerElement& element)
{
    auto* renderer = element.renderer();
    if (!renderer)
        return;
    if (auto* layer = renderer->enclosingLayer()) {
        if (layer->invalidateEventRegion(RenderLayer::EventRegionInvalidationReason::NonCompositedFrame))
            return;
    }
    if (auto* ownerElement = this->ownerElement())
        ownerElement->document().invalidateEventRegionsForFrame(*ownerElement);
}

void Document::invalidateEventListenerRegions()
{
    if (!renderView() || !documentElement())
        return;

    // We don't track style validity for Document and full rebuild is too big of a hammer.
    // Instead just mutate the style directly and trigger a minimal style update.
    auto& rootStyle = renderView()->mutableStyle();
    Style::Adjuster::adjustEventListenerRegionTypesForRootStyle(rootStyle, *this);

    documentElement()->invalidateStyleInternal();
}

void Document::invalidateRenderingDependentRegions()
{
#if PLATFORM(IOS_FAMILY) && ENABLE(TOUCH_EVENTS)
    setTouchEventRegionsNeedUpdate();
#endif

#if PLATFORM(IOS_FAMILY)
    if (auto* page = this->page()) {
        if (auto* frameView = view()) {
            if (auto* scrollingCoordinator = page->scrollingCoordinator())
                scrollingCoordinator->frameViewEventTrackingRegionsChanged(*frameView);
        }
    }
#endif
}

bool Document::setFocusedElement(Element* element, FocusDirection direction, FocusRemovalEventsMode eventsMode)
{
    RefPtr<Element> newFocusedElement = element;
    // Make sure newFocusedElement is actually in this document
    if (newFocusedElement && (&newFocusedElement->document() != this))
        return true;

    if (m_focusedElement == newFocusedElement)
        return true;

    if (backForwardCacheState() != NotInBackForwardCache)
        return false;

    RefPtr<Element> oldFocusedElement = WTFMove(m_focusedElement);

    // Remove focus from the existing focus node (if any)
    if (oldFocusedElement) {
        bool focusChangeBlocked = false;

        oldFocusedElement->setFocus(false);
        setFocusNavigationStartingNode(nullptr);

        if (eventsMode == FocusRemovalEventsMode::Dispatch) {
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
        } else {
            // Match the order in HTMLTextFormControlElement::dispatchBlurEvent.
            if (is<HTMLInputElement>(*oldFocusedElement))
                downcast<HTMLInputElement>(*oldFocusedElement).endEditing();
            if (page())
                page()->chrome().client().elementDidBlur(*oldFocusedElement);
            ASSERT(!m_focusedElement);
        }

        if (oldFocusedElement->isRootEditableElement())
            editor().didEndEditing();

        if (view()) {
            if (Widget* oldWidget = widgetForElement(oldFocusedElement.get()))
                oldWidget->setFocus(false);
            else
                view()->setFocus(false);
        }

        if (is<HTMLInputElement>(oldFocusedElement)) {
            // HTMLInputElement::didBlur just scrolls text fields back to the beginning.
            // FIXME: This could be done asynchronusly.
            downcast<HTMLInputElement>(*oldFocusedElement).didBlur();
        }

        if (focusChangeBlocked)
            return false;
    }

    if (newFocusedElement && newFocusedElement->isFocusable()) {
        if (&newFocusedElement->document() != this) {
            // Bluring oldFocusedElement may have moved newFocusedElement across documents.
            return false;
        }
        if (newFocusedElement->isRootEditableElement() && !acceptsEditingFocus(*newFocusedElement)) {
            // delegate blocks focus change
            return false;
        }
        // Set focus on the new node
        m_focusedElement = newFocusedElement;
        setFocusNavigationStartingNode(m_focusedElement.get());

        // Dispatch the focus event and let the node do any other focus related activities (important for text fields)
        m_focusedElement->dispatchFocusEvent(oldFocusedElement.copyRef(), direction);

        if (m_focusedElement != newFocusedElement) {
            // handler shifted focus
            return false;
        }

        m_focusedElement->dispatchFocusInEvent(eventNames().focusinEvent, oldFocusedElement.copyRef()); // DOM level 3 bubbling focus event.

        if (m_focusedElement != newFocusedElement) {
            // handler shifted focus
            return false;
        }

        // FIXME: We should remove firing DOMFocusInEvent event when we are sure no content depends
        // on it, probably when <rdar://problem/8503958> is m.
        m_focusedElement->dispatchFocusInEvent(eventNames().DOMFocusInEvent, oldFocusedElement.copyRef()); // DOM level 2 for compatibility.

        if (m_focusedElement != newFocusedElement) {
            // handler shifted focus
            return false;
        }

        m_focusedElement->setFocus(true);

        // The setFocus call triggers a blur and a focus event. Event handlers could cause the focused element to be cleared.
        if (m_focusedElement != newFocusedElement) {
            // handler shifted focus
            return false;
        }

        if (m_focusedElement->isRootEditableElement())
            editor().didBeginEditing();

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
            else if (auto* frameView = view())
                frameView->setFocus(true);
        }
    }

    if (m_focusedElement) {
        // Create the AXObject cache in a focus change because GTK relies on it.
        if (AXObjectCache* cache = axObjectCache())
            cache->deferFocusedUIElementChangeIfNeeded(oldFocusedElement.get(), newFocusedElement.get());
    }

    if (page())
        page()->chrome().focusedElementChanged(m_focusedElement.get());

    return true;
}

static bool shouldResetFocusNavigationStartingNode(Node& node)
{
    // Setting focus navigation starting node to the following nodes means that we should start
    // the search from the beginning of the document.
    return is<HTMLHtmlElement>(node) || is<HTMLDocument>(node);
}

void Document::setFocusNavigationStartingNode(Node* node)
{
    if (!m_frame)
        return;

    m_focusNavigationStartingNodeIsRemoved = false;
    if (!node || shouldResetFocusNavigationStartingNode(*node)) {
        m_focusNavigationStartingNode = nullptr;
        return;
    }

    ASSERT(!node || node != this);
    m_focusNavigationStartingNode = node;
}

Element* Document::focusNavigationStartingNode(FocusDirection direction) const
{
    if (m_focusedElement) {
        if (!m_focusNavigationStartingNode || !m_focusNavigationStartingNode->isDescendantOf(m_focusedElement.get()))
            return m_focusedElement.get();
    }

    if (!m_focusNavigationStartingNode)
        return nullptr;

    Node* node = m_focusNavigationStartingNode.get();
    
    // When the node was removed from the document tree. This case is not specified in the spec:
    // https://html.spec.whatwg.org/multipage/interaction.html#sequential-focus-navigation-starting-point
    // Current behaivor is to move the sequential navigation node to / after (based on the focus direction)
    // the previous sibling of the removed node.
    if (m_focusNavigationStartingNodeIsRemoved) {
        Node* nextNode = NodeTraversal::next(*node);
        if (!nextNode)
            nextNode = node;
        if (direction == FocusDirectionForward)
            return ElementTraversal::previous(*nextNode);
        if (is<Element>(*nextNode))
            return downcast<Element>(nextNode);
        return ElementTraversal::next(*nextNode);
    }

    if (is<Element>(*node))
        return downcast<Element>(node);
    if (Element* elementBeforeNextFocusableElement = direction == FocusDirectionForward ? ElementTraversal::previous(*node) : ElementTraversal::next(*node))
        return elementBeforeNextFocusableElement;
    return node->parentOrShadowHostElement();
}

void Document::setCSSTarget(Element* targetNode)
{
    if (m_cssTarget)
        m_cssTarget->invalidateStyleForSubtree();
    m_cssTarget = targetNode;
    if (targetNode)
        targetNode->invalidateStyleForSubtree();
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
    ASSERT(m_listsInvalidatedAtDocument.contains(&list));
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

void Document::attachNodeIterator(NodeIterator& iterator)
{
    m_nodeIterators.add(&iterator);
}

void Document::detachNodeIterator(NodeIterator& iterator)
{
    // The node iterator can be detached without having been attached if its root node didn't have a document
    // when the iterator was created, but has it now.
    m_nodeIterators.remove(&iterator);
}

void Document::moveNodeIteratorsToNewDocumentSlowCase(Node& node, Document& newDocument)
{
    ASSERT(!m_nodeIterators.isEmpty());
    for (auto* iterator : copyToVector(m_nodeIterators)) {
        if (&iterator->root() == &node) {
            detachNodeIterator(*iterator);
            newDocument.attachNodeIterator(*iterator);
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
    ASSERT(ScriptDisallowedScope::InMainThread::hasDisallowedScope());

    adjustFocusedNodeOnNodeRemoval(container, NodeRemoval::ChildrenOfNode);
    adjustFocusNavigationNodeOnNodeRemoval(container, NodeRemoval::ChildrenOfNode);

#if ENABLE(FULLSCREEN_API)
    m_fullscreenManager->adjustFullscreenElementOnNodeRemoval(container, NodeRemoval::ChildrenOfNode);
#endif

    for (auto* range : m_ranges)
        range->nodeChildrenWillBeRemoved(container);

    for (auto* it : m_nodeIterators) {
        for (Node* n = container.firstChild(); n; n = n->nextSibling())
            it->nodeWillBeRemoved(*n);
    }

    if (RefPtr<Frame> frame = this->frame()) {
        for (Node* n = container.firstChild(); n; n = n->nextSibling()) {
            frame->eventHandler().nodeWillBeRemoved(*n);
            frame->selection().nodeWillBeRemoved(*n);
            frame->page()->dragCaretController().nodeWillBeRemoved(*n);
        }
    }

    if (m_markers->hasMarkers()) {
        for (Text* textNode = TextNodeTraversal::firstChild(container); textNode; textNode = TextNodeTraversal::nextSibling(*textNode))
            m_markers->removeMarkers(*textNode);
    }
}

void Document::nodeWillBeRemoved(Node& node)
{
    ASSERT(ScriptDisallowedScope::InMainThread::hasDisallowedScope());

    adjustFocusedNodeOnNodeRemoval(node);
    adjustFocusNavigationNodeOnNodeRemoval(node);

#if ENABLE(FULLSCREEN_API)
    m_fullscreenManager->adjustFullscreenElementOnNodeRemoval(node, NodeRemoval::Node);
#endif

    for (auto* it : m_nodeIterators)
        it->nodeWillBeRemoved(node);

    for (auto* range : m_ranges)
        range->nodeWillBeRemoved(node);

    if (RefPtr<Frame> frame = this->frame()) {
        frame->eventHandler().nodeWillBeRemoved(node);
        frame->selection().nodeWillBeRemoved(node);
        frame->page()->dragCaretController().nodeWillBeRemoved(node);
    }

    if (is<Text>(node))
        m_markers->removeMarkers(node);
}

void Document::parentlessNodeMovedToNewDocument(Node& node)
{
    Vector<Range*, 5> rangesAffected;

    for (auto* range : m_ranges) {
        if (range->parentlessNodeMovedToNewDocumentAffectsRange(node))
            rangesAffected.append(range);
    }

    for (auto* range : rangesAffected)
        range->updateRangeForParentlessNodeMovedToNewDocument(node);
}

static Node* fallbackFocusNavigationStartingNodeAfterRemoval(Node& node)
{
    return node.previousSibling() ? node.previousSibling() : node.parentNode();
}

void Document::adjustFocusNavigationNodeOnNodeRemoval(Node& node, NodeRemoval nodeRemoval)
{
    if (!m_focusNavigationStartingNode)
        return;

    if (isNodeInSubtree(*m_focusNavigationStartingNode, node, nodeRemoval)) {
        auto* newNode = (nodeRemoval == NodeRemoval::ChildrenOfNode) ? &node : fallbackFocusNavigationStartingNodeAfterRemoval(node);
        m_focusNavigationStartingNode = (newNode != this) ? newNode : nullptr;
        m_focusNavigationStartingNodeIsRemoved = true;
    }
}

void Document::textInserted(Node& text, unsigned offset, unsigned length)
{
    if (!m_ranges.isEmpty()) {
        for (auto* range : m_ranges)
            range->textInserted(text, offset, length);
    }

    // Update the markers for spelling and grammar checking.
    m_markers->shiftMarkers(text, offset, length);

#if ENABLE(PLATFORM_DRIVEN_TEXT_CHECKING)
    // Freshly inserted text is expected to not inherit PlatformTextChecking markers.
    m_markers->removeMarkers(text, { offset, offset + length }, DocumentMarker::PlatformTextChecking);
#endif
}

void Document::textRemoved(Node& text, unsigned offset, unsigned length)
{
    if (!m_ranges.isEmpty()) {
        for (auto* range : m_ranges)
            range->textRemoved(text, offset, length);
    }

    // Update the markers for spelling and grammar checking.
    m_markers->removeMarkers(text, { offset, offset + length });
    m_markers->shiftMarkers(text, offset + length, 0 - length);
}

void Document::textNodesMerged(Text& oldNode, unsigned offset)
{
    if (!m_ranges.isEmpty()) {
        NodeWithIndex oldNodeWithIndex(&oldNode);
        for (auto* range : m_ranges)
            range->textNodesMerged(oldNodeWithIndex, offset);
    }

    // FIXME: This should update markers for spelling and grammar checking.
}

void Document::textNodeSplit(Text& oldNode)
{
    for (auto* range : m_ranges)
        range->textNodeSplit(oldNode);

    // FIXME: This should update markers for spelling and grammar checking.
}

void Document::createDOMWindow()
{
    ASSERT(m_frame);
    ASSERT(!m_domWindow);

    m_domWindow = DOMWindow::create(*this);

    ASSERT(m_domWindow->document() == this);
    ASSERT(m_domWindow->frame() == m_frame);

    m_frame->loader().client().didCreateWindow(*m_domWindow);
}

void Document::takeDOMWindowFrom(Document& document)
{
    ASSERT(m_frame);
    ASSERT(!m_domWindow);
    ASSERT(document.m_domWindow);
    // A valid DOMWindow is needed by CachedFrame for its documents.
    ASSERT(backForwardCacheState() == NotInBackForwardCache);

    m_domWindow = WTFMove(document.m_domWindow);
    m_domWindow->didSecureTransitionTo(*this);

    ASSERT(m_domWindow->document() == this);
    ASSERT(m_domWindow->frame() == m_frame);
}

WindowProxy* Document::windowProxy() const
{
    if (!m_frame)
        return nullptr;
    return &m_frame->windowProxy();
}

Document& Document::contextDocument() const
{
    if (m_contextDocument)
        return *m_contextDocument.get();
    return const_cast<Document&>(*this);
}

void Document::setAttributeEventListener(const AtomString& eventType, const QualifiedName& attributeName, const AtomString& attributeValue, DOMWrapperWorld& isolatedWorld)
{
    setAttributeEventListener(eventType, JSLazyEventListener::create(*this, attributeName, attributeValue), isolatedWorld);
}

void Document::setWindowAttributeEventListener(const AtomString& eventType, RefPtr<EventListener>&& listener, DOMWrapperWorld& isolatedWorld)
{
    if (!m_domWindow)
        return;
    m_domWindow->setAttributeEventListener(eventType, WTFMove(listener), isolatedWorld);
}

void Document::setWindowAttributeEventListener(const AtomString& eventType, const QualifiedName& attributeName, const AtomString& attributeValue, DOMWrapperWorld& isolatedWorld)
{
    if (!m_domWindow)
        return;
    if (!m_domWindow->frame())
        return;
    setWindowAttributeEventListener(eventType, JSLazyEventListener::create(*m_domWindow, attributeName, attributeValue), isolatedWorld);
}

EventListener* Document::getWindowAttributeEventListener(const AtomString& eventType, DOMWrapperWorld& isolatedWorld)
{
    if (!m_domWindow)
        return nullptr;
    return m_domWindow->attributeEventListener(eventType, isolatedWorld);
}

void Document::dispatchWindowEvent(Event& event, EventTarget* target)
{
    ASSERT_WITH_SECURITY_IMPLICATION(ScriptDisallowedScope::InMainThread::isScriptAllowed());
    if (!m_domWindow)
        return;
    m_domWindow->dispatchEvent(event, target);
}

void Document::dispatchWindowLoadEvent()
{
    ASSERT_WITH_SECURITY_IMPLICATION(ScriptDisallowedScope::InMainThread::isScriptAllowed());
    if (!m_domWindow)
        return;
    m_domWindow->dispatchLoadEvent();
    m_loadEventFinished = true;
    m_cachedResourceLoader->documentDidFinishLoadEvent();
}

void Document::queueTaskToDispatchEvent(TaskSource source, Ref<Event>&& event)
{
    eventLoop().queueTask(source, [document = makeRef(*this), event = WTFMove(event)] {
        document->dispatchEvent(event);
    });
}

void Document::queueTaskToDispatchEventOnWindow(TaskSource source, Ref<Event>&& event)
{
    eventLoop().queueTask(source, [this, protectedThis = makeRef(*this), event = WTFMove(event)] {
        if (!m_domWindow)
            return;
        m_domWindow->dispatchEvent(event);
    });
}

void Document::enqueueOverflowEvent(Ref<Event>&& event)
{
    // https://developer.mozilla.org/en-US/docs/Web/API/Element/overflow_event
    // FIXME: This event is totally unspecified.
    auto* target = event->target();
    RELEASE_ASSERT(target);
    RELEASE_ASSERT(is<Node>(target));
    eventLoop().queueTask(TaskSource::DOMManipulation, [protectedTarget = GCReachableRef<Node>(downcast<Node>(*target)), event = WTFMove(event)] {
        protectedTarget->dispatchEvent(event);
    });
}

ExceptionOr<Ref<Event>> Document::createEvent(const String& type)
{
    // Please do *not* add new event classes to this function unless they are required
    // for compatibility with the DOM specification or some actual legacy web content.

    // This mechanism is superceded by use of event constructors.
    // That is what we should use for any new event classes.

    // The following strings are the ones from the DOM specification
    // <https://dom.spec.whatwg.org/#dom-document-createevent>.

    if (equalLettersIgnoringASCIICase(type, "beforeunloadevent"))
        return Ref<Event> { BeforeUnloadEvent::createForBindings() };
    if (equalLettersIgnoringASCIICase(type, "compositionevent"))
        return Ref<Event> { CompositionEvent::createForBindings() };
    if (equalLettersIgnoringASCIICase(type, "customevent"))
        return Ref<Event> { CustomEvent::create() };
    if (equalLettersIgnoringASCIICase(type, "dragevent"))
        return Ref<Event> { DragEvent::createForBindings() };
    if (equalLettersIgnoringASCIICase(type, "event") || equalLettersIgnoringASCIICase(type, "events") || equalLettersIgnoringASCIICase(type, "htmlevents") || equalLettersIgnoringASCIICase(type, "svgevents"))
        return Event::createForBindings();
    if (equalLettersIgnoringASCIICase(type, "focusevent"))
        return Ref<Event> { FocusEvent::createForBindings() };
    if (equalLettersIgnoringASCIICase(type, "hashchangeevent"))
        return Ref<Event> { HashChangeEvent::createForBindings() };
    if (equalLettersIgnoringASCIICase(type, "keyboardevent"))
        return Ref<Event> { KeyboardEvent::createForBindings() };
    if (equalLettersIgnoringASCIICase(type, "messageevent"))
        return Ref<Event> { MessageEvent::createForBindings() };
    if (equalLettersIgnoringASCIICase(type, "storageevent"))
        return Ref<Event> { StorageEvent::createForBindings() };
    if (equalLettersIgnoringASCIICase(type, "mouseevent") || equalLettersIgnoringASCIICase(type, "mouseevents"))
        return Ref<Event> { MouseEvent::createForBindings() };
    if (equalLettersIgnoringASCIICase(type, "textevent"))
        return Ref<Event> { TextEvent::createForBindings() }; // FIXME: HTML specification says this should create a CompositionEvent, not a TextEvent.
    if (equalLettersIgnoringASCIICase(type, "uievent") || equalLettersIgnoringASCIICase(type, "uievents"))
        return Ref<Event> { UIEvent::createForBindings() };

    // FIXME: Consider including support for these event classes even when device orientation
    // support is not enabled.
#if ENABLE(DEVICE_ORIENTATION)
    if (equalLettersIgnoringASCIICase(type, "devicemotionevent"))
        return Ref<Event> { DeviceMotionEvent::createForBindings() };
    if (equalLettersIgnoringASCIICase(type, "deviceorientationevent"))
        return Ref<Event> { DeviceOrientationEvent::createForBindings() };
#endif

#if ENABLE(TOUCH_EVENTS)
    if (equalLettersIgnoringASCIICase(type, "touchevent"))
        return Ref<Event> { TouchEvent::createForBindings() };
#endif

    // FIXME: Add support for "dragevent", which the DOM specification calls for.

    // The following string comes from the SVG specification
    // <http://www.w3.org/TR/SVG/script.html#InterfaceSVGZoomEvent>
    // However, since there is no provision for initializing the event once it is created,
    // there is no practical value in this feature.
    // FIXME: Confirm there is no content depending on this and remove it.

    if (equalLettersIgnoringASCIICase(type, "svgzoomevents"))
        return Ref<Event> { SVGZoomEvent::createForBindings() };

    // The following strings are not part of the DOM specification and we would like to eliminate them.
    // However, we currently include them until we resolve any issues with backward compatibility.
    // FIXME: For each of the strings below, confirm that there is no content depending on it and remove
    // the string, remove the createForBindings function, and also consider removing the corresponding
    // init function for that class.

    if (equalLettersIgnoringASCIICase(type, "keyboardevents"))
        return Ref<Event> { KeyboardEvent::createForBindings() };
    if (equalLettersIgnoringASCIICase(type, "mutationevent") || equalLettersIgnoringASCIICase(type, "mutationevents"))
        return Ref<Event> { MutationEvent::createForBindings() };
    if (equalLettersIgnoringASCIICase(type, "overflowevent"))
        return Ref<Event> { OverflowEvent::createForBindings() };
    if (equalLettersIgnoringASCIICase(type, "popstateevent"))
        return Ref<Event> { PopStateEvent::createForBindings() };
    if (equalLettersIgnoringASCIICase(type, "wheelevent"))
        return Ref<Event> { WheelEvent::createForBindings() };

    return Exception { NotSupportedError };
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

void Document::addListenerTypeIfNeeded(const AtomString& eventType)
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
    else if (eventType == eventNames().resizeEvent)
        addListenerType(RESIZE_LISTENER);
}

HTMLFrameOwnerElement* Document::ownerElement() const
{
    if (!frame())
        return nullptr;
    return frame()->ownerElement();
}

// https://html.spec.whatwg.org/#cookie-averse-document-object
bool Document::isCookieAverse() const
{
    // A Document that has no browsing context is cookie-averse.
    if (!frame())
        return true;

    URL cookieURL = this->cookieURL();

    // This is not part of the specification but we have historically allowed cookies over file protocol
    // and some developers rely on this for testing.
    if (cookieURL.isLocalFile())
        return false;

    // A Document whose URL's scheme is not a network scheme is cookie-averse (https://fetch.spec.whatwg.org/#network-scheme).
    return !cookieURL.protocolIsInHTTPFamily() && !cookieURL.protocolIs("ftp");
}

ExceptionOr<String> Document::cookie()
{
    if (page() && !page()->settings().cookieEnabled())
        return String();

    if (isCookieAverse())
        return String();

    if (!securityOrigin().canAccessCookies())
        return Exception { SecurityError };

    URL cookieURL = this->cookieURL();
    if (cookieURL.isEmpty())
        return String();

    if (!isDOMCookieCacheValid() && page())
        setCachedDOMCookies(page()->cookieJar().cookies(*this, cookieURL));

    return String { cachedDOMCookies() };
}

ExceptionOr<void> Document::setCookie(const String& value)
{
    if (page() && !page()->settings().cookieEnabled())
        return { };

    if (isCookieAverse())
        return { };

    if (!securityOrigin().canAccessCookies())
        return Exception { SecurityError };

    URL cookieURL = this->cookieURL();
    if (cookieURL.isEmpty())
        return { };

    invalidateDOMCookieCache();
    if (page())
        page()->cookieJar().setCookies(*this, cookieURL, value);
    return { };
}

String Document::referrer()
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    if (!m_referrerOverride.isEmpty())
        return m_referrerOverride;
    if (DeprecatedGlobalSettings::resourceLoadStatisticsEnabled() && frame()) {
        auto referrerStr = frame()->loader().referrer();
        if (!referrerStr.isEmpty()) {
            URL referrerURL { URL(), referrerStr };
            RegistrableDomain referrerRegistrableDomain { referrerURL };
            if (!referrerRegistrableDomain.matches(securityOrigin().data())) {
                m_referrerOverride = referrerURL.protocolHostAndPort();
                return m_referrerOverride;
            }
        }
    }
#endif
    if (frame())
        return frame()->loader().referrer();
    return String();
}

String Document::domain() const
{
    return securityOrigin().domain();
}

ExceptionOr<void> Document::setDomain(const String& newDomain)
{
    if (!frame())
        return Exception { SecurityError, "A browsing context is required to set a domain." };

    if (isSandboxed(SandboxDocumentDomain))
        return Exception { SecurityError, "Assignment is forbidden for sandboxed iframes." };

    if (LegacySchemeRegistry::isDomainRelaxationForbiddenForURLScheme(securityOrigin().protocol()))
        return Exception { SecurityError };

    // FIXME: We should add logging indicating why a domain was not allowed.

    const String& effectiveDomain = domain();
    if (effectiveDomain.isEmpty())
        return Exception { SecurityError, "The document has a null effectiveDomain." };

    if (!securityOrigin().isMatchingRegistrableDomainSuffix(newDomain, settings().treatIPAddressAsDomain()))
        return Exception { SecurityError, "Attempted to use a non-registrable domain." };

    securityOrigin().setDomainFromDOM(newDomain);
    return { };
}

void Document::overrideLastModified(const Optional<WallTime>& lastModified)
{
    m_overrideLastModified = lastModified;
}

// http://www.whatwg.org/specs/web-apps/current-work/#dom-document-lastmodified
String Document::lastModified() const
{
    Optional<WallTime> dateTime;
    if (m_overrideLastModified)
        dateTime = m_overrideLastModified;
    else if (loader())
        dateTime = loader()->response().lastModified();

    // FIXME: If this document came from the file system, the HTML specification tells
    // us to read the last modification date from the file system.
    if (!dateTime)
        dateTime = WallTime::now();

    auto ctime = dateTime.value().secondsSinceEpoch().secondsAs<time_t>();
    auto localDateTime = std::localtime(&ctime);
    return makeString(pad('0', 2, localDateTime->tm_mon + 1), '/',
        pad('0', 2, localDateTime->tm_mday), '/',
        pad('0', 4, 1900 + localDateTime->tm_year), ' ',
        pad('0', 2, localDateTime->tm_hour), ':',
        pad('0', 2, localDateTime->tm_min), ':',
        pad('0', 2, localDateTime->tm_sec));
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
    U16_NEXT(characters, i, length, c);
    if (!isValidNameStart(c))
        return false;

    while (i < length) {
        U16_NEXT(characters, i, length, c);
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

ExceptionOr<std::pair<AtomString, AtomString>> Document::parseQualifiedName(const String& qualifiedName)
{
    unsigned length = qualifiedName.length();

    if (!length)
        return Exception { InvalidCharacterError };

    bool nameStart = true;
    bool sawColon = false;
    unsigned colonPosition = 0;

    for (unsigned i = 0; i < length; ) {
        UChar32 c;
        U16_NEXT(qualifiedName, i, length, c);
        if (c == ':') {
            if (sawColon)
                return Exception { InvalidCharacterError };
            nameStart = true;
            sawColon = true;
            colonPosition = i - 1;
        } else if (nameStart) {
            if (!isValidNameStart(c))
                return Exception { InvalidCharacterError };
            nameStart = false;
        } else {
            if (!isValidNamePart(c))
                return Exception { InvalidCharacterError };
        }
    }

    if (!sawColon)
        return std::pair<AtomString, AtomString> { { }, { qualifiedName } };

    if (!colonPosition || length - colonPosition <= 1)
        return Exception { InvalidCharacterError };

    return std::pair<AtomString, AtomString> { StringView { qualifiedName }.substring(0, colonPosition).toAtomString(), StringView { qualifiedName }.substring(colonPosition + 1).toAtomString() };
}

ExceptionOr<QualifiedName> Document::parseQualifiedName(const AtomString& namespaceURI, const String& qualifiedName)
{
    auto parseResult = parseQualifiedName(qualifiedName);
    if (parseResult.hasException())
        return parseResult.releaseException();
    auto parsedPieces = parseResult.releaseReturnValue();
    return QualifiedName { parsedPieces.first, parsedPieces.second, namespaceURI };
}

void Document::setDecoder(RefPtr<TextResourceDecoder>&& decoder)
{
    m_decoder = WTFMove(decoder);
}

URL Document::completeURL(const String& url, const URL& baseURLOverride, ForceUTF8 forceUTF8) const
{
    // Always return a null URL when passed a null string.
    // FIXME: Should we change the URL constructor to have this behavior?
    // See also [CSS]StyleSheet::completeURL(const String&)
    if (url.isNull())
        return URL();
    const URL& baseURL = ((baseURLOverride.isEmpty() || baseURLOverride == aboutBlankURL()) && parentDocument()) ? parentDocument()->baseURL() : baseURLOverride;
    if (!m_decoder || forceUTF8 == ForceUTF8::Yes)
        return URL(baseURL, url);
    return URL(baseURL, url, m_decoder->encodingForURLParsing());
}

URL Document::completeURL(const String& url, ForceUTF8 forceUTF8) const
{
    return completeURL(url, m_baseURL, forceUTF8);
}

void Document::setBackForwardCacheState(BackForwardCacheState state)
{
    if (m_backForwardCacheState == state)
        return;

    m_backForwardCacheState = state;

    FrameView* v = view();
    Page* page = this->page();

    switch (state) {
    case InBackForwardCache:
        if (v) {
            // FIXME: There is some scrolling related work that needs to happen whenever a page goes into the
            // back/forward cache and similar work that needs to occur when it comes out. This is where we do the work
            // that needs to happen when we enter, and the work that needs to happen when we exit is in
            // HistoryController::restoreScrollPositionAndViewState(). It can't be here because this function is
            // called too early on in the process of a page exiting the cache for that work to be possible in this
            // function. It would be nice if there was more symmetry here.
            // https://bugs.webkit.org/show_bug.cgi?id=98698
            v->cacheCurrentScrollPosition();
            if (page && m_frame->isMainFrame()) {
                v->resetScrollbarsAndClearContentsSize();
                if (ScrollingCoordinator* scrollingCoordinator = page->scrollingCoordinator())
                    scrollingCoordinator->clearAllNodes();
            }
        }

#if ENABLE(POINTER_LOCK)
        exitPointerLock();
#endif

        styleScope().clearResolver();
        clearSelectorQueryCache();
        m_styleRecalcTimer.stop();

        clearSharedObjectPool();

#if ENABLE(INDEXED_DATABASE)
        if (m_idbConnectionProxy)
            m_idbConnectionProxy->setContextSuspended(*scriptExecutionContext(), true);
#endif
        break;
    case NotInBackForwardCache:
        if (childNeedsStyleRecalc())
            scheduleStyleRecalc();
#if ENABLE(INDEXED_DATABASE)
        if (m_idbConnectionProxy)
            m_idbConnectionProxy->setContextSuspended(*scriptExecutionContext(), false);
#endif
        break;
    case AboutToEnterBackForwardCache:
        break;
    }
}

void Document::documentWillBecomeInactive()
{
    if (renderView())
        renderView()->setIsInWindow(false);
}

void Document::suspend(ReasonForSuspension reason)
{
    if (m_isSuspended)
        return;

    documentWillBecomeInactive();

    for (auto* element : m_documentSuspensionCallbackElements)
        element->prepareForDocumentSuspension();

#if ASSERT_ENABLED
    // Clear the update flag to be able to check if the viewport arguments update
    // is dispatched, after the document is restored from the back/forward cache.
    m_didDispatchViewportPropertiesChanged = false;
#endif

    ASSERT(page());
    page()->lockAllOverlayScrollbarsToHidden(true);

    if (RenderView* view = renderView()) {
        if (view->usesCompositing())
            view->compositor().cancelCompositingLayerUpdate();
    }

#if USE(LIBWEBRTC)
    // FIXME: This should be moved to Modules/mediastream.
    if (LibWebRTCProvider::webRTCAvailable()) {
        if (auto* page = this->page())
            page->libWebRTCProvider().unregisterMDNSNames(identifier());
    }
#endif

#if ENABLE(SERVICE_WORKER)
    if (RuntimeEnabledFeatures::sharedFeatures().serviceWorkerEnabled() && reason == ReasonForSuspension::BackForwardCache)
        setServiceWorkerConnection(nullptr);
#endif

    suspendScheduledTasks(reason);

    ASSERT(m_frame);
    m_frame->clearTimers();

    m_visualUpdatesAllowed = false;
    m_visualUpdatesSuppressionTimer.stop();

    m_isSuspended = true;
}

void Document::resume(ReasonForSuspension reason)
{
    if (!m_isSuspended)
        return;

    for (auto* element : copyToVector(m_documentSuspensionCallbackElements))
        element->resumeFromDocumentSuspension();

    if (renderView())
        renderView()->setIsInWindow(true);

    ASSERT(page());
    page()->lockAllOverlayScrollbarsToHidden(false);

    ASSERT(m_frame);

    if (RuntimeEnabledFeatures::sharedFeatures().webAnimationsCSSIntegrationEnabled()) {
        if (m_timelinesController)
            m_timelinesController->resumeAnimations();  
    } else
        m_frame->legacyAnimation().resumeAnimationsForDocument(this);

    resumeScheduledTasks(reason);

    m_visualUpdatesAllowed = true;

    m_isSuspended = false;

#if ENABLE(SERVICE_WORKER)
    if (RuntimeEnabledFeatures::sharedFeatures().serviceWorkerEnabled() && reason == ReasonForSuspension::BackForwardCache)
        setServiceWorkerConnection(&ServiceWorkerProvider::singleton().serviceWorkerConnection());
#endif
}

void Document::registerForDocumentSuspensionCallbacks(Element& element)
{
    m_documentSuspensionCallbackElements.add(&element);
}

void Document::unregisterForDocumentSuspensionCallbacks(Element& element)
{
    m_documentSuspensionCallbackElements.remove(&element);
}

bool Document::audioPlaybackRequiresUserGesture() const
{
    if (DocumentLoader* loader = this->loader()) {
        // If an audio playback policy was set during navigation, use it. If not, use the global settings.
        AutoplayPolicy policy = loader->autoplayPolicy();
        if (policy != AutoplayPolicy::Default)
            return policy == AutoplayPolicy::AllowWithoutSound || policy == AutoplayPolicy::Deny;
    }

    return settings().audioPlaybackRequiresUserGesture();
}

bool Document::videoPlaybackRequiresUserGesture() const
{
    if (DocumentLoader* loader = this->loader()) {
        // If a video playback policy was set during navigation, use it. If not, use the global settings.
        AutoplayPolicy policy = loader->autoplayPolicy();
        if (policy != AutoplayPolicy::Default)
            return policy == AutoplayPolicy::Deny;
    }

    return settings().videoPlaybackRequiresUserGesture();
}

bool Document::mediaDataLoadsAutomatically() const
{
    if (auto* loader = this->loader()) {
        AutoplayPolicy policy = loader->autoplayPolicy();
        if (policy != AutoplayPolicy::Default)
            return policy != AutoplayPolicy::Deny;
    }

    return settings().mediaDataLoadsAutomatically();
}

void Document::storageBlockingStateDidChange()
{
    securityOrigin().setStorageBlockingPolicy(settings().storageBlockingPolicy());
}

// Used only by WebKitLegacy.
void Document::privateBrowsingStateDidChange(PAL::SessionID sessionID)
{
    if (m_logger)
        m_logger->setEnabled(this, sessionID.isAlwaysOnLoggingAllowed());

#if ENABLE(VIDEO)
    forEachMediaElement([sessionID] (HTMLMediaElement& element) {
        element.privateBrowsingStateDidChange(sessionID);
    });
#endif
}

#if ENABLE(VIDEO)

void Document::registerForCaptionPreferencesChangedCallbacks(HTMLMediaElement& element)
{
    if (page())
        page()->group().captionPreferences().setInterestedInCaptionPreferenceChanges();

    m_captionPreferencesChangedElements.add(&element);
}

void Document::unregisterForCaptionPreferencesChangedCallbacks(HTMLMediaElement& element)
{
    m_captionPreferencesChangedElements.remove(&element);
}

void Document::captionPreferencesChanged()
{
    for (auto* element : m_captionPreferencesChangedElements)
        element->captionPreferencesChanged();
}

void Document::setMediaElementShowingTextTrack(const HTMLMediaElement& element)
{
    m_mediaElementShowingTextTrack = makeWeakPtr(element);
}

void Document::clearMediaElementShowingTextTrack()
{
    m_mediaElementShowingTextTrack = nullptr;
}

void Document::updateTextTrackRepresentationImageIfNeeded()
{
    if (m_mediaElementShowingTextTrack)
        m_mediaElementShowingTextTrack->updateTextTrackRepresentationImageIfNeeded();
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
    auto protectedDocument = makeRef(*document);

    document->updateStyleIfNeeded();

    auto frame = makeRefPtr(document->frame());

    if (!frame || frame->document() != document)
        return Editor::Command();

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
    return command(this, commandName).state() == TriState::Indeterminate;
}

bool Document::queryCommandState(const String& commandName)
{
    return command(this, commandName).state() == TriState::True;
}

bool Document::queryCommandSupported(const String& commandName)
{
    return command(this, commandName).isSupported();
}

String Document::queryCommandValue(const String& commandName)
{
    return command(this, commandName).value();
}

void Document::pushCurrentScript(Element* newCurrentScript)
{
    m_currentScriptStack.append(newCurrentScript);
}

void Document::popCurrentScript()
{
    ASSERT(!m_currentScriptStack.isEmpty());
    m_currentScriptStack.removeLast();
}

bool Document::shouldDeferAsynchronousScriptsUntilParsingFinishes() const
{
    if (!settings().shouldDeferAsynchronousScriptsUntilAfterDocumentLoadOrFirstPaint())
        return false;

    if (quirks().shouldBypassAsyncScriptDeferring())
        return false;

    return parsing() && !(view() && view()->hasEverPainted());
}

#if ENABLE(XSLT)

void Document::scheduleToApplyXSLTransforms()
{
    m_hasPendingXSLTransforms = true;
    if (!m_applyPendingXSLTransformsTimer.isActive())
        m_applyPendingXSLTransformsTimer.startOneShot(0_s);
}

void Document::applyPendingXSLTransformsNowIfScheduled()
{
    if (!m_hasPendingXSLTransforms)
        return;
    m_applyPendingXSLTransformsTimer.stop();
    applyPendingXSLTransformsTimerFired();
}

void Document::applyPendingXSLTransformsTimerFired()
{
    if (parsing())
        return;

    m_hasPendingXSLTransforms = false;
    ASSERT_WITH_SECURITY_IMPLICATION(ScriptDisallowedScope::InMainThread::isScriptAllowed());
    for (auto& processingInstruction : styleScope().collectXSLTransforms()) {
        ASSERT(processingInstruction->isXSL());

        // Don't apply XSL transforms to already transformed documents -- <rdar://problem/4132806>
        if (transformSourceDocument() || !processingInstruction->sheet())
            return;

        // If the Document has already been detached from the frame, or the frame is currently in the process of
        // changing to a new document, don't attempt to create a new Document from the XSLT.
        if (!frame() || frame()->documentIsBeingReplaced())
            return;

        auto processor = XSLTProcessor::create();
        processor->setXSLStyleSheet(downcast<XSLStyleSheet>(processingInstruction->sheet()));
        String resultMIMEType;
        String newSource;
        String resultEncoding;
        if (!processor->transformToString(*this, resultMIMEType, newSource, resultEncoding))
            continue;
        // FIXME: If the transform failed we should probably report an error (like Mozilla does).
        processor->createDocumentFromSource(newSource, resultEncoding, resultMIMEType, this, frame());
    }
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
        frame->document()->scheduleFullStyleRebuild();
}

String Document::designMode() const
{
    return inDesignMode() ? "on"_s : "off"_s;
}

void Document::setDesignMode(const String& value)
{
    InheritedBool mode;
    if (equalLettersIgnoringASCIICase(value, "on"))
        mode = on;
    else if (equalLettersIgnoringASCIICase(value, "off"))
        mode = off;
    else
        mode = inherit;
    setDesignMode(mode);
}

auto Document::getDesignMode() const -> InheritedBool
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
    if (backForwardCacheState() == NotInBackForwardCache && !m_renderTreeBeingDestroyed) {
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

ExceptionOr<Ref<Attr>> Document::createAttribute(const String& localName)
{
    if (!isValidName(localName))
        return Exception { InvalidCharacterError };
    return Attr::create(*this, QualifiedName { nullAtom(), isHTMLDocument() ? localName.convertToASCIILowercase() : localName, nullAtom() }, emptyString());
}

ExceptionOr<Ref<Attr>> Document::createAttributeNS(const AtomString& namespaceURI, const String& qualifiedName, bool shouldIgnoreNamespaceChecks)
{
    auto parseResult = parseQualifiedName(namespaceURI, qualifiedName);
    if (parseResult.hasException())
        return parseResult.releaseException();
    QualifiedName parsedName { parseResult.releaseReturnValue() };
    if (!shouldIgnoreNamespaceChecks && !hasValidNamespaceForAttributes(parsedName))
        return Exception { NamespaceError };
    return Attr::create(*this, parsedName, emptyString());
}

const SVGDocumentExtensions* Document::svgExtensions()
{
    return m_svgExtensions.get();
}

SVGDocumentExtensions& Document::accessSVGExtensions()
{
    if (!m_svgExtensions)
        m_svgExtensions = makeUnique<SVGDocumentExtensions>(*this);
    return *m_svgExtensions;
}

void Document::addSVGUseElement(SVGUseElement& element)
{
    auto result = m_svgUseElements.add(&element);
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(result.isNewEntry);
}

void Document::removeSVGUseElement(SVGUseElement& element)
{
    m_svgUseElements.remove(&element);
    // FIXME: Assert that element was in m_svgUseElements once re-entrancy to update style and layout have been removed.
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

Ref<HTMLCollection> Document::allFilteredByName(const AtomString& name)
{
    return ensureRareData().ensureNodeLists().addCachedCollection<HTMLAllNamedSubCollection>(*this, DocumentAllNamedItems, name);
}

Ref<HTMLCollection> Document::windowNamedItems(const AtomString& name)
{
    return ensureRareData().ensureNodeLists().addCachedCollection<WindowNameCollection>(*this, WindowNamedItems, name);
}

Ref<HTMLCollection> Document::documentNamedItems(const AtomString& name)
{
    return ensureRareData().ensureNodeLists().addCachedCollection<DocumentNameCollection>(*this, DocumentNamedItems, name);
}

void Document::finishedParsing()
{
    ASSERT(!scriptableDocumentParser() || !m_parser->isParsing());
    ASSERT(!scriptableDocumentParser() || m_readyState != Loading);
    setParsing(false);

    Ref<Document> protectedThis(*this);

    scriptRunner().documentFinishedParsing();

    if (!m_documentTiming.domContentLoadedEventStart)
        m_documentTiming.domContentLoadedEventStart = MonotonicTime::now();

    // FIXME: Schedule a task to fire DOMContentLoaded event instead. See webkit.org/b/82931
    eventLoop().performMicrotaskCheckpoint();
    dispatchEvent(Event::create(eventNames().DOMContentLoadedEvent, Event::CanBubble::Yes, Event::IsCancelable::No));

    if (!m_documentTiming.domContentLoadedEventEnd)
        m_documentTiming.domContentLoadedEventEnd = MonotonicTime::now();

    if (RefPtr<Frame> frame = this->frame()) {
#if ENABLE(XSLT)
        applyPendingXSLTransformsNowIfScheduled();
#endif

        // FrameLoader::finishedParsing() might end up calling Document::implicitClose() if all
        // resource loads are complete. HTMLObjectElements can start loading their resources from
        // post attach callbacks triggered by resolveStyle(). This means if we parse out an <object>
        // tag and then reach the end of the document without updating styles, we might not have yet
        // started the resource load and might fire the window load event too early. To avoid this
        // we force the styles to be up to date before calling FrameLoader::finishedParsing().
        // See https://bugs.webkit.org/show_bug.cgi?id=36864 starting around comment 35.
        updateStyleIfNeeded();

        frame->loader().finishedParsing();
        InspectorInstrumentation::domContentLoadedEventFired(*frame);
    }

    // Schedule dropping of the DocumentSharedObjectPool. We keep it alive for a while after parsing finishes
    // so that dynamically inserted content can also benefit from sharing optimizations.
    // Note that we don't refresh the timer on pool access since that could lead to huge caches being kept
    // alive indefinitely by something innocuous like JS setting .innerHTML repeatedly on a timer.
    static const Seconds timeToKeepSharedObjectPoolAliveAfterParsingFinished { 10_s };
    m_sharedObjectPoolClearTimer.startOneShot(timeToKeepSharedObjectPoolAliveAfterParsingFinished);

    // Parser should have picked up all speculative preloads by now
    m_cachedResourceLoader->clearPreloads(CachedResourceLoader::ClearPreloadsMode::ClearSpeculativePreloads);

#if ENABLE(SERVICE_WORKER)
    if (RuntimeEnabledFeatures::sharedFeatures().serviceWorkerEnabled()) {
        // Stop queuing service worker client messages now that the DOMContentLoaded event has been fired.
        if (auto* serviceWorkerContainer = this->serviceWorkerContainer())
            serviceWorkerContainer->startMessages();
    }
#endif
}

void Document::clearSharedObjectPool()
{
    m_sharedObjectPool = nullptr;
    m_sharedObjectPoolClearTimer.stop();
}

#if ENABLE(TELEPHONE_NUMBER_DETECTION)

// FIXME: Find a better place for this code.

bool Document::isTelephoneNumberParsingEnabled() const
{
    return settings().telephoneNumberParsingEnabled() && m_isTelephoneNumberParsingAllowed;
}

bool Document::isTelephoneNumberParsingAllowed() const
{
    return m_isTelephoneNumberParsingAllowed;
}

#endif

String Document::originIdentifierForPasteboard() const
{
    auto origin = securityOrigin().toString();
    if (origin != "null")
        return origin;
    if (!m_uniqueIdentifier)
        m_uniqueIdentifier = "null:" + createCanonicalUUIDString();
    return m_uniqueIdentifier;
}

ExceptionOr<Ref<XPathExpression>> Document::createExpression(const String& expression, RefPtr<XPathNSResolver>&& resolver)
{
    if (!m_xpathEvaluator)
        m_xpathEvaluator = XPathEvaluator::create();
    return m_xpathEvaluator->createExpression(expression, WTFMove(resolver));
}

Ref<XPathNSResolver> Document::createNSResolver(Node& nodeResolver)
{
    if (!m_xpathEvaluator)
        m_xpathEvaluator = XPathEvaluator::create();
    return m_xpathEvaluator->createNSResolver(nodeResolver);
}

ExceptionOr<Ref<XPathResult>> Document::evaluate(const String& expression, Node& contextNode, RefPtr<XPathNSResolver>&& resolver, unsigned short type, XPathResult* result)
{
    if (!m_xpathEvaluator)
        m_xpathEvaluator = XPathEvaluator::create();
    return m_xpathEvaluator->evaluate(expression, contextNode, WTFMove(resolver), type, result);
}

void Document::initSecurityContext()
{
    if (haveInitializedSecurityOrigin()) {
        ASSERT(SecurityContext::securityOrigin());
        return;
    }

    if (!m_frame) {
        // No source for a security context.
        // This can occur via document.implementation.createDocument().
        setCookieURL(URL({ }, emptyString()));
        setSecurityOriginPolicy(SecurityOriginPolicy::create(SecurityOrigin::createUnique()));
        setContentSecurityPolicy(makeUnique<ContentSecurityPolicy>(URL { { }, emptyString() }, *this));
        return;
    }

    // In the common case, create the security context from the currently
    // loading URL with a fresh content security policy.
    setCookieURL(m_url);
    enforceSandboxFlags(m_frame->loader().effectiveSandboxFlags());
    setReferrerPolicy(m_frame->loader().effectiveReferrerPolicy());

    if (shouldEnforceContentDispositionAttachmentSandbox())
        applyContentDispositionAttachmentSandbox();

    auto* documentLoader = m_frame->loader().documentLoader();
    bool isSecurityOriginUnique = isSandboxed(SandboxOrigin);
    if (!isSecurityOriginUnique)
        isSecurityOriginUnique = documentLoader && documentLoader->response().tainting() == ResourceResponse::Tainting::Opaque;

    setSecurityOriginPolicy(SecurityOriginPolicy::create(isSecurityOriginUnique ? SecurityOrigin::createUnique() : SecurityOrigin::create(m_url)));
    setContentSecurityPolicy(makeUnique<ContentSecurityPolicy>(URL { m_url }, *this));

    String overrideContentSecurityPolicy = m_frame->loader().client().overrideContentSecurityPolicy();
    if (!overrideContentSecurityPolicy.isNull())
        contentSecurityPolicy()->didReceiveHeader(overrideContentSecurityPolicy, ContentSecurityPolicyHeaderType::Enforce, ContentSecurityPolicy::PolicyFrom::API, referrer(), documentLoader ? documentLoader->response().httpStatusCode() : 0);

#if USE(QUICK_LOOK)
    if (shouldEnforceQuickLookSandbox())
        applyQuickLookSandbox();
#endif

    if (shouldEnforceHTTP09Sandbox()) {
        String message = makeString("Sandboxing '", m_url.stringCenterEllipsizedToLength(), "' because it is using HTTP/0.9.");
        addConsoleMessage(MessageSource::Security, MessageLevel::Error, message);
        enforceSandboxFlags(SandboxScripts | SandboxPlugins);
    }

    if (settings().needsStorageAccessFromFileURLsQuirk())
        securityOrigin().grantStorageAccessFromFileURLsQuirk();
    if (!settings().webSecurityEnabled()) {
        // Web security is turned off. We should let this document access every other document. This is used primary by testing
        // harnesses for web sites.
        securityOrigin().grantUniversalAccess();
    } else if (securityOrigin().isLocal()) {
        if (settings().allowUniversalAccessFromFileURLs() || m_frame->loader().client().shouldForceUniversalAccessFromLocalURL(m_url)) {
            // Some clients want local URLs to have universal access, but that setting is dangerous for other clients.
            securityOrigin().grantUniversalAccess();
        } else if (!settings().allowFileAccessFromFileURLs()) {
            // Some clients want local URLs to have even tighter restrictions by default, and not be able to access other local files.
            // FIXME 81578: The naming of this is confusing. Files with restricted access to other local files
            // still can have other privileges that can be remembered, thereby not making them unique origins.
            securityOrigin().setEnforcesFilePathSeparation();
        }
    }
    securityOrigin().setStorageBlockingPolicy(settings().storageBlockingPolicy());

    Document* parentDocument = ownerElement() ? &ownerElement()->document() : nullptr;
    if (parentDocument && m_frame->loader().shouldTreatURLAsSrcdocDocument(url())) {
        m_isSrcdocDocument = true;
        setBaseURLOverride(parentDocument->baseURL());
    }
    if (parentDocument)
        setStrictMixedContentMode(parentDocument->isStrictMixedContentMode());

    if (!SecurityPolicy::shouldInheritSecurityOriginFromOwner(m_url))
        return;

    // If we do not obtain a meaningful origin from the URL, then we try to
    // find one via the frame hierarchy.
    Frame* parentFrame = m_frame->tree().parent();
    Frame* openerFrame = m_frame->loader().opener();

    Frame* ownerFrame = parentFrame;
    if (!ownerFrame)
        ownerFrame = openerFrame;

    if (!ownerFrame) {
        didFailToInitializeSecurityOrigin();
        return;
    }

    contentSecurityPolicy()->copyStateFrom(ownerFrame->document()->contentSecurityPolicy());
    contentSecurityPolicy()->updateSourceSelf(ownerFrame->document()->securityOrigin());

    // Per <http://www.w3.org/TR/upgrade-insecure-requests/>, new browsing contexts must inherit from an
    // ongoing set of upgraded requests. When opening a new browsing context, we need to capture its
    // existing upgrade request. Nested browsing contexts are handled during DocumentWriter::begin.
    if (auto* openerDocument = openerFrame ? openerFrame->document() : nullptr)
        contentSecurityPolicy()->inheritInsecureNavigationRequestsToUpgradeFromOpener(*openerDocument->contentSecurityPolicy());

    if (isSandboxed(SandboxOrigin)) {
        // If we're supposed to inherit our security origin from our owner,
        // but we're also sandboxed, the only thing we inherit is the ability
        // to load local resources. This lets about:blank iframes in file://
        // URL documents load images and other resources from the file system.
        if (ownerFrame->document()->securityOrigin().canLoadLocalResources())
            securityOrigin().grantLoadLocalResources();
        return;
    }

    setCookieURL(ownerFrame->document()->cookieURL());
    // We alias the SecurityOrigins to match Firefox, see Bug 15313
    // https://bugs.webkit.org/show_bug.cgi?id=15313
    setSecurityOriginPolicy(ownerFrame->document()->securityOriginPolicy());
}

void Document::initContentSecurityPolicy()
{
    auto* parentFrame = m_frame->tree().parent();
    if (parentFrame)
        contentSecurityPolicy()->copyUpgradeInsecureRequestStateFrom(*parentFrame->document()->contentSecurityPolicy());

    // FIXME: Remove this special plugin document logic. We are stricter than the CSP 3 spec. with regards to plugins: we prefer to
    // inherit the full policy unless the plugin document is opened in a new window. The CSP 3 spec. implies that only plugin documents
    // delivered with a local scheme (e.g. blob, file, data) should inherit a policy.
    if (!isPluginDocument())
        return;
    auto* openerFrame = m_frame->loader().opener();
    bool shouldInhert = parentFrame || (openerFrame && openerFrame->document()->securityOrigin().canAccess(securityOrigin()));
    if (!shouldInhert)
        return;
    setContentSecurityPolicy(makeUnique<ContentSecurityPolicy>(URL { m_url }, *this));
    if (openerFrame)
        contentSecurityPolicy()->createPolicyForPluginDocumentFrom(*openerFrame->document()->contentSecurityPolicy());
    else
        contentSecurityPolicy()->copyStateFrom(parentFrame->document()->contentSecurityPolicy());
}

bool Document::isContextThread() const
{
    return isMainThread();
}

// https://w3c.github.io/webappsec-secure-contexts/#is-url-trustworthy
static bool isURLPotentiallyTrustworthy(const URL& url)
{
    if (url.protocolIsAbout())
        return url.isAboutBlank() || url.isAboutSrcDoc();
    if (url.protocolIsData())
        return true;
    return SecurityOrigin::create(url)->isPotentiallyTrustworthy();
}

// https://w3c.github.io/webappsec-secure-contexts/#is-settings-object-contextually-secure step 5.3 and 5.4
static inline bool isDocumentSecure(const Document& document)
{
    if (document.isSandboxed(SandboxOrigin))
        return isURLPotentiallyTrustworthy(document.url());
    return document.securityOrigin().isPotentiallyTrustworthy();
}

// https://w3c.github.io/webappsec-secure-contexts/#is-settings-object-contextually-secure
bool Document::isSecureContext() const
{
    if (!m_frame)
        return true;
    if (!RuntimeEnabledFeatures::sharedFeatures().secureContextChecksEnabled())
        return true;

    for (auto* frame = m_frame->tree().parent(); frame; frame = frame->tree().parent()) {
        if (!isDocumentSecure(*frame->document()))
            return false;
    }

    return isDocumentSecure(*this);
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

void Document::statePopped(Ref<SerializedScriptValue>&& stateObject)
{
    if (!frame())
        return;
    
    // Per step 11 of section 6.5.9 (history traversal) of the HTML5 spec, we 
    // defer firing of popstate until we're in the complete state.
    if (m_readyState == Complete)
        dispatchPopstateEvent(WTFMove(stateObject));
    else
        m_pendingStateObject = WTFMove(stateObject);
}

void Document::attachRange(Range& range)
{
    ASSERT(!m_ranges.contains(&range));
    m_ranges.add(&range);
}

void Document::detachRange(Range& range)
{
    // We don't ASSERT m_ranges.contains(&range) to allow us to call this
    // unconditionally to fix: https://bugs.webkit.org/show_bug.cgi?id=26044
    m_ranges.remove(&range);
}

Optional<RenderingContext> Document::getCSSCanvasContext(const String& type, const String& name, int width, int height)
{
    HTMLCanvasElement* element = getCSSCanvasElement(name);
    if (!element)
        return WTF::nullopt;
    element->setSize({ width, height });
    auto context = element->getContext(type);
    if (!context)
        return WTF::nullopt;

#if ENABLE(WEBGL)
    if (is<WebGLRenderingContext>(*context))
        return RenderingContext { RefPtr<WebGLRenderingContext> { &downcast<WebGLRenderingContext>(*context) } };
#endif
#if ENABLE(WEBGL2)
    if (is<WebGL2RenderingContext>(*context))
        return RenderingContext { RefPtr<WebGL2RenderingContext> { &downcast<WebGL2RenderingContext>(*context) } };
#endif
#if ENABLE(WEBGPU)
    if (is<GPUCanvasContext>(*context))
        return RenderingContext { RefPtr<GPUCanvasContext> { &downcast<GPUCanvasContext>(*context) } };
#endif

    return RenderingContext { RefPtr<CanvasRenderingContext2D> { &downcast<CanvasRenderingContext2D>(*context) } };
}

HTMLCanvasElement* Document::getCSSCanvasElement(const String& name)
{
    RefPtr<HTMLCanvasElement>& element = m_cssCanvasElements.add(name, nullptr).iterator->value;
    if (!element)
        element = HTMLCanvasElement::create(*this);
    return element.get();
}

String Document::nameForCSSCanvasElement(const HTMLCanvasElement& canvasElement) const
{
    for (const auto& entry : m_cssCanvasElements) {
        if (entry.value.get() == &canvasElement)
            return entry.key;
    }
    return String();
}

#if ENABLE(TEXT_AUTOSIZING)
TextAutoSizing& Document::textAutoSizing()
{
    if (!m_textAutoSizing)
        m_textAutoSizing = makeUnique<TextAutoSizing>();
    return *m_textAutoSizing;
}
#endif // ENABLE(TEXT_AUTOSIZING)

void Document::initDNSPrefetch()
{
    m_haveExplicitlyDisabledDNSPrefetch = false;
    m_isDNSPrefetchEnabled = settings().dnsPrefetchingEnabled() && securityOrigin().protocol() == "http";

    // Inherit DNS prefetch opt-out from parent frame    
    if (Document* parent = parentDocument()) {
        if (!parent->isDNSPrefetchEnabled())
            m_isDNSPrefetchEnabled = false;
    }
}

void Document::parseDNSPrefetchControlHeader(const String& dnsPrefetchControl)
{
    if (!settings().dnsPrefetchingEnabled())
        return;

    if (equalLettersIgnoringASCIICase(dnsPrefetchControl, "on") && !m_haveExplicitlyDisabledDNSPrefetch) {
        m_isDNSPrefetchEnabled = true;
        return;
    }

    m_isDNSPrefetchEnabled = false;
    m_haveExplicitlyDisabledDNSPrefetch = true;
}

void Document::getParserLocation(String& completedURL, unsigned& line, unsigned& column) const
{
    // We definitely cannot associate the message with a location being parsed if we are not even parsing.
    if (!parsing())
        return;

    ScriptableDocumentParser* parser = scriptableDocumentParser();
    if (!parser)
        return;

    // When the parser waits for scripts, any messages must be coming from some other source, and are not related to the location of the script element that made the parser wait.
    if (!parser->shouldAssociateConsoleMessagesWithTextPosition())
        return;

    completedURL = url().string();
    TextPosition position = parser->textPosition();
    line = position.m_line.oneBasedInt();
    column = position.m_column.oneBasedInt();
}

void Document::addConsoleMessage(std::unique_ptr<Inspector::ConsoleMessage>&& consoleMessage)
{
    if (!isContextThread()) {
        postTask(AddConsoleMessageTask(WTFMove(consoleMessage)));
        return;
    }

    if (Page* page = this->page())
        page->console().addMessage(WTFMove(consoleMessage));
}

void Document::addConsoleMessage(MessageSource source, MessageLevel level, const String& message, unsigned long requestIdentifier)
{
    if (!isContextThread()) {
        postTask(AddConsoleMessageTask(source, level, message));
        return;
    }

    if (Page* page = this->page())
        page->console().addMessage(source, level, message, requestIdentifier, this);

    if (m_consoleMessageListener)
        m_consoleMessageListener->scheduleCallback(*this, message);
}

void Document::addMessage(MessageSource source, MessageLevel level, const String& message, const String& sourceURL, unsigned lineNumber, unsigned columnNumber, RefPtr<Inspector::ScriptCallStack>&& callStack, JSC::JSGlobalObject* state, unsigned long requestIdentifier)
{
    if (!isContextThread()) {
        postTask(AddConsoleMessageTask(source, level, message));
        return;
    }

    if (Page* page = this->page())
        page->console().addMessage(source, level, message, sourceURL, lineNumber, columnNumber, WTFMove(callStack), state, requestIdentifier);
}

void Document::postTask(Task&& task)
{
    callOnMainThread([documentID = identifier(), task = WTFMove(task)]() mutable {
        ASSERT(isMainThread());

        auto* document = allDocumentsMap().get(documentID);
        if (!document)
            return;

        Page* page = document->page();
        if ((page && page->defersLoading() && document->activeDOMObjectsAreSuspended()) || !document->m_pendingTasks.isEmpty())
            document->m_pendingTasks.append(WTFMove(task));
        else
            task.performTask(*document);
    });
}

void Document::pendingTasksTimerFired()
{
    Vector<Task> pendingTasks = WTFMove(m_pendingTasks);
    for (auto& task : pendingTasks)
        task.performTask(*this);
}

EventLoopTaskGroup& Document::eventLoop()
{
    ASSERT(isMainThread());
    if (UNLIKELY(!m_documentTaskGroup)) {
        m_documentTaskGroup = makeUnique<EventLoopTaskGroup>(windowEventLoop());
        if (activeDOMObjectsAreStopped())
            m_documentTaskGroup->stopAndDiscardAllTasks();
        else if (activeDOMObjectsAreSuspended())
            m_documentTaskGroup->suspend();
    }
    return *m_documentTaskGroup;
}

WindowEventLoop& Document::windowEventLoop()
{
    ASSERT(isMainThread());
    if (UNLIKELY(!m_eventLoop))
        m_eventLoop = WindowEventLoop::eventLoopForSecurityOrigin(securityOrigin());
    return *m_eventLoop;
}

void Document::suspendScheduledTasks(ReasonForSuspension reason)
{
    if (m_scheduledTasksAreSuspended) {
        // A page may subsequently suspend DOM objects, say as part of handling a scroll or zoom gesture, after the
        // embedding client requested the page be suspended. We ignore such requests so long as the embedding client
        // requested the suspension first. See <rdar://problem/13754896> for more details.
        ASSERT(reasonForSuspendingActiveDOMObjects() == ReasonForSuspension::PageWillBeSuspended);
        return;
    }

    suspendScriptedAnimationControllerCallbacks();
    suspendActiveDOMObjects(reason);
    scriptRunner().suspend();
    m_pendingTasksTimer.stop();

#if ENABLE(XSLT)
    m_applyPendingXSLTransformsTimer.stop();
#endif

    // Deferring loading and suspending parser is necessary when we need to prevent re-entrant JavaScript execution
    // (e.g. while displaying an alert).
    // It is not currently possible to suspend parser unless loading is deferred, because new data arriving from network
    // will trigger parsing, and leave the scheduler in an inconsistent state where it doesn't know whether it's suspended or not.
    if (reason == ReasonForSuspension::WillDeferLoading && m_parser)
        m_parser->suspendScheduledTasks();

    m_scheduledTasksAreSuspended = true;
}

void Document::resumeScheduledTasks(ReasonForSuspension reason)
{
    if (reasonForSuspendingActiveDOMObjects() != reason)
        return;

    ASSERT(m_scheduledTasksAreSuspended);

    if (reason == ReasonForSuspension::WillDeferLoading && m_parser)
        m_parser->resumeScheduledTasks();

#if ENABLE(XSLT)
    if (m_hasPendingXSLTransforms)
        m_applyPendingXSLTransformsTimer.startOneShot(0_s);
#endif

    if (!m_pendingTasks.isEmpty())
        m_pendingTasksTimer.startOneShot(0_s);
    scriptRunner().resume();
    resumeActiveDOMObjects(reason);
    resumeScriptedAnimationControllerCallbacks();
    
    m_scheduledTasksAreSuspended = false;
}

void Document::suspendScriptedAnimationControllerCallbacks()
{
    if (m_scriptedAnimationController)
        m_scriptedAnimationController->suspend();
}

void Document::resumeScriptedAnimationControllerCallbacks()
{
    if (m_scriptedAnimationController)
        m_scriptedAnimationController->resume();
}

void Document::serviceRequestAnimationFrameCallbacks(ReducedResolutionSeconds timestamp)
{
    if (m_scriptedAnimationController)
        m_scriptedAnimationController->serviceRequestAnimationFrameCallbacks(timestamp);
}

void Document::windowScreenDidChange(PlatformDisplayID displayID)
{
    if (RenderView* view = renderView()) {
        if (view->usesCompositing())
            view->compositor().windowScreenDidChange(displayID);
    }
}

String Document::displayStringModifiedByEncoding(const String& string) const
{
    if (!m_decoder)
        return string;
    return String { string }.replace('\\', m_decoder->encoding().backslashAsCurrencySymbol());
}

void Document::dispatchPageshowEvent(PageshowEventPersistence persisted)
{
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=36334 Pageshow event needs to fire asynchronously.
    dispatchWindowEvent(PageTransitionEvent::create(eventNames().pageshowEvent, persisted), this);
}

void Document::enqueueSecurityPolicyViolationEvent(SecurityPolicyViolationEvent::Init&& eventInit)
{
    queueTaskToDispatchEvent(TaskSource::DOMManipulation, SecurityPolicyViolationEvent::create(eventNames().securitypolicyviolationEvent, WTFMove(eventInit), Event::IsTrusted::Yes));
}

void Document::enqueueHashchangeEvent(const String& oldURL, const String& newURL)
{
    // FIXME: popstate event and hashchange event are supposed to fire in a single task.
    queueTaskToDispatchEventOnWindow(TaskSource::DOMManipulation, HashChangeEvent::create(oldURL, newURL));
}

void Document::dispatchPopstateEvent(RefPtr<SerializedScriptValue>&& stateObject)
{
    dispatchWindowEvent(PopStateEvent::create(WTFMove(stateObject), m_domWindow ? &m_domWindow->history() : nullptr));
}

void Document::addMediaCanStartListener(MediaCanStartListener& listener)
{
    ASSERT(!m_mediaCanStartListeners.contains(listener));
    m_mediaCanStartListeners.add(listener);
}

void Document::removeMediaCanStartListener(MediaCanStartListener& listener)
{
    ASSERT(m_mediaCanStartListeners.contains(listener));
    m_mediaCanStartListeners.remove(listener);
}

MediaCanStartListener* Document::takeAnyMediaCanStartListener()
{
    if (m_mediaCanStartListeners.computesEmpty())
        return nullptr;

    MediaCanStartListener* listener = m_mediaCanStartListeners.begin().get();
    m_mediaCanStartListeners.remove(*listener);

    return listener;
}

#if ENABLE(DEVICE_ORIENTATION) && PLATFORM(IOS_FAMILY)

DeviceMotionController& Document::deviceMotionController() const
{
    return *m_deviceMotionController;
}

DeviceOrientationController& Document::deviceOrientationController() const
{
    return *m_deviceOrientationController;
}

void Document::simulateDeviceOrientationChange(double alpha, double beta, double gamma)
{
    auto orientation = DeviceOrientationData::create(alpha, beta, gamma, WTF::nullopt, WTF::nullopt);
    deviceOrientationController().didChangeDeviceOrientation(orientation.ptr());
}

#endif

#if ENABLE(POINTER_LOCK)

void Document::exitPointerLock()
{
    Page* page = this->page();
    if (!page)
        return;
    if (auto* target = page->pointerLockController().element()) {
        if (&target->document() != this)
            return;
    }
    page->pointerLockController().requestPointerUnlock();
}

#endif

void Document::decrementLoadEventDelayCount()
{
    ASSERT(m_loadEventDelayCount);
    --m_loadEventDelayCount;

    if (frame() && !m_loadEventDelayCount && !m_loadEventDelayTimer.isActive())
        m_loadEventDelayTimer.startOneShot(0_s);
}

void Document::loadEventDelayTimerFired()
{
    // FIXME: Should the call to FrameLoader::checkLoadComplete be moved inside Document::checkCompleted?
    // FIXME: Should this also call DocumentLoader::checkLoadComplete?
    // FIXME: Not obvious why checkCompleted needs to go first. The order these are called is
    // visible to WebKit clients, but it's more like a race than a well-defined relationship.
    Ref<Document> protectedThis(*this);
    checkCompleted();
    if (auto* frame = this->frame())
        frame->loader().checkLoadComplete();
}

void Document::checkCompleted()
{
    if (auto* frame = this->frame())
        frame->loader().checkCompleted();
}

double Document::monotonicTimestamp() const
{
    auto* loader = this->loader();
    if (!loader)
        return 0;

    return loader->timing().secondsSinceStartTime(MonotonicTime::now()).seconds();
}

int Document::requestAnimationFrame(Ref<RequestAnimationFrameCallback>&& callback)
{
    if (!m_scriptedAnimationController) {
        m_scriptedAnimationController = ScriptedAnimationController::create(*this);

        // It's possible that the Page may have suspended scripted animations before
        // we were created. We need to make sure that we don't start up the animation
        // controller on a background tab, for example.
        if (!page() || page()->scriptedAnimationsSuspended())
            m_scriptedAnimationController->suspend();

        if (!topOrigin().canAccess(securityOrigin()) && !hasHadUserInteraction())
            m_scriptedAnimationController->addThrottlingReason(ThrottlingReason::NonInteractedCrossOriginFrame);
    }

    return m_scriptedAnimationController->registerCallback(WTFMove(callback));
}

void Document::cancelAnimationFrame(int id)
{
    if (!m_scriptedAnimationController)
        return;
    m_scriptedAnimationController->cancelCallback(id);
}

void Document::clearScriptedAnimationController()
{
    // FIXME: consider using ActiveDOMObject.
    if (m_scriptedAnimationController)
        m_scriptedAnimationController->clearDocumentPointer();
    m_scriptedAnimationController = nullptr;
}

int Document::requestIdleCallback(Ref<IdleRequestCallback>&& callback, Seconds timeout)
{
    if (!m_idleCallbackController)
        m_idleCallbackController = makeUnique<IdleCallbackController>(*this);
    return m_idleCallbackController->queueIdleCallback(WTFMove(callback), timeout);
}

void Document::cancelIdleCallback(int id)
{
    if (!m_idleCallbackController)
        return;
    m_idleCallbackController->removeIdleCallback(id);
}

void Document::wheelEventHandlersChanged()
{
    Page* page = this->page();
    if (!page)
        return;

    if (FrameView* frameView = view()) {
        if (ScrollingCoordinator* scrollingCoordinator = page->scrollingCoordinator())
            scrollingCoordinator->frameViewEventTrackingRegionsChanged(*frameView);
    }

    bool haveHandlers = m_wheelEventTargets && !m_wheelEventTargets->isEmpty();
    page->chrome().client().wheelEventHandlersChanged(haveHandlers);
}

void Document::didAddWheelEventHandler(Node& node)
{
    if (!m_wheelEventTargets)
        m_wheelEventTargets = makeUnique<EventTargetSet>();

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
        m_touchEventTargets = makeUnique<EventTargetSet>();

    m_touchEventTargets->add(&handler);

    if (Document* parent = parentDocument()) {
        parent->didAddTouchEventHandler(*this);
        return;
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

    if (Document* parent = parentDocument())
        parent->didRemoveTouchEventHandler(*this);
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

LayoutRect Document::absoluteEventHandlerBounds(bool& includesFixedPositionElements)
{
    includesFixedPositionElements = false;
    if (RenderView* renderView = this->renderView())
        return renderView->documentRect();
    
    return LayoutRect();
}

Document::RegionFixedPair Document::absoluteEventRegionForNode(Node& node)
{
    Region region;
    LayoutRect rootRelativeBounds;
    bool insideFixedPosition = false;

    if (is<Document>(node)) {
        auto& document = downcast<Document>(node);
        if (&document == this)
            rootRelativeBounds = absoluteEventHandlerBounds(insideFixedPosition);
        else if (Element* element = document.ownerElement())
            rootRelativeBounds = element->absoluteEventHandlerBounds(insideFixedPosition);
    } else if (is<Element>(node)) {
        auto& element = downcast<Element>(node);
        if (is<HTMLBodyElement>(element)) {
            // For the body, just use the document bounds.
            // The body may not cover this whole area, but it's OK for this region to be an overestimate.
            rootRelativeBounds = absoluteEventHandlerBounds(insideFixedPosition);
        } else
            rootRelativeBounds = element.absoluteEventHandlerBounds(insideFixedPosition);
    }

    if (!rootRelativeBounds.isEmpty())
        region.unite(Region(enclosingIntRect(rootRelativeBounds)));

    return RegionFixedPair(region, insideFixedPosition);
}

Document::RegionFixedPair Document::absoluteRegionForEventTargets(const EventTargetSet* targets)
{
    LayoutDisallowedScope layoutDisallowedScope(LayoutDisallowedScope::Reason::ReentrancyAvoidance);

    if (!targets)
        return RegionFixedPair(Region(), false);

    Region targetRegion;
    bool insideFixedPosition = false;

    for (auto& keyValuePair : *targets) {
        if (auto* node = keyValuePair.key) {
            auto targetRegionFixedPair = absoluteEventRegionForNode(*node);
            targetRegion.unite(targetRegionFixedPair.first);
            insideFixedPosition |= targetRegionFixedPair.second;
        }
    }

    return RegionFixedPair(targetRegion, insideFixedPosition);
}

void Document::updateLastHandledUserGestureTimestamp(MonotonicTime time)
{
    m_lastHandledUserGestureTimestamp = time;

    if (static_cast<bool>(time) && m_scriptedAnimationController) {
        // It's OK to always remove NonInteractedCrossOriginFrame even if this frame isn't cross-origin.
        m_scriptedAnimationController->removeThrottlingReason(ThrottlingReason::NonInteractedCrossOriginFrame);
    }

    // DOM Timer alignment may depend on the user having interacted with the document.
    didChangeTimerAlignmentInterval();
    
    if (HTMLFrameOwnerElement* element = ownerElement())
        element->document().updateLastHandledUserGestureTimestamp(time);
}

bool Document::processingUserGestureForMedia() const
{
    if (UserGestureIndicator::processingUserGestureForMedia())
        return true;

    if (m_userActivatedMediaFinishedPlayingTimestamp + maxIntervalForUserGestureForwardingAfterMediaFinishesPlaying >= MonotonicTime::now())
        return true;

    if (settings().mediaUserGestureInheritsFromDocument())
        return topDocument().hasHadUserInteraction();

    auto* loader = this->loader();
    if (loader && loader->allowedAutoplayQuirks().contains(AutoplayQuirk::InheritedUserGestures))
        return topDocument().hasHadUserInteraction();

    return false;
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

bool Document::allowsContentJavaScript() const
{
    // FIXME: Get all SPI clients off of this potentially dangerous Setting.
    if (!settings().scriptMarkupEnabled())
        return false;

    if (!m_frame || m_frame->document() != this) {
        // If this Document is frameless or in the wrong frame, its context document
        // must allow for it to run content JavaScript.
        return m_contextDocument && m_contextDocument->allowsContentJavaScript();
    }

    return m_frame->loader().client().allowsContentJavaScriptFromMostRecentNavigation() == AllowsContentJavaScript::Yes;
}

Element* eventTargetElementForDocument(Document* document)
{
    if (!document)
        return nullptr;
    Element* element = document->focusedElement();
    if (!element && is<PluginDocument>(*document))
        element = downcast<PluginDocument>(*document).pluginElement();
    if (!element && document->isHTMLDocument())
        element = document->bodyOrFrameset();
    if (!element)
        element = document->documentElement();
    return element;
}

void Document::convertAbsoluteToClientQuads(Vector<FloatQuad>& quads, const RenderStyle& style)
{
    if (!view())
        return;

    const auto& frameView = *view();
    float inverseFrameScale = frameView.absoluteToDocumentScaleFactor(style.effectiveZoom());
    auto documentToClientOffset = frameView.documentToClientOffset();

    for (auto& quad : quads) {
        if (inverseFrameScale != 1)
            quad.scale(inverseFrameScale);

        quad.move(documentToClientOffset);
    }
}
    
void Document::convertAbsoluteToClientRects(Vector<FloatRect>& rects, const RenderStyle& style)
{
    if (!view())
        return;
    
    auto& frameView = *view();
    float inverseFrameScale = frameView.absoluteToDocumentScaleFactor(style.effectiveZoom());
    auto documentToClientOffset = frameView.documentToClientOffset();
    
    for (auto& rect : rects) {
        if (inverseFrameScale != 1)
            rect.scale(inverseFrameScale);
        
        rect.move(documentToClientOffset);
    }
}

void Document::convertAbsoluteToClientRect(FloatRect& rect, const RenderStyle& style)
{
    if (!view())
        return;

    const auto& frameView = *view();
    rect = frameView.absoluteToDocumentRect(rect, style.effectiveZoom()); 
    rect = frameView.documentToClientRect(rect);
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

    // FIXME: We should call DocumentLoader::checkLoadComplete as well here,
    // but it seems to cause http/tests/security/feed-urls-from-remote.html
    // to timeout on Mac WK1; see http://webkit.org/b/110554 and http://webkit.org/b/110401.
    frame()->loader().checkLoadComplete();
}

DocumentParserYieldToken::DocumentParserYieldToken(Document& document)
    : m_document(makeWeakPtr(document))
{
    if (++document.m_parserYieldTokenCount != 1)
        return;

    document.scriptRunner().didBeginYieldingParser();
    if (auto* parser = document.parser())
        parser->didBeginYieldingParser();
}

DocumentParserYieldToken::~DocumentParserYieldToken()
{
    if (!m_document)
        return;

    ASSERT(m_document->m_parserYieldTokenCount);
    if (--m_document->m_parserYieldTokenCount)
        return;

    m_document->scriptRunner().didEndYieldingParser();
    if (auto* parser = m_document->parser())
        parser->didEndYieldingParser();
}

static Element* findNearestCommonComposedAncestor(Element* elementA, Element* elementB)
{
    if (!elementA || !elementB)
        return nullptr;

    if (elementA == elementB)
        return elementA;

    HashSet<Element*> ancestorChain;
    for (auto* element = elementA; element; element = element->parentElementInComposedTree())
        ancestorChain.add(element);

    for (auto* element = elementB; element; element = element->parentElementInComposedTree()) {
        if (ancestorChain.contains(element))
            return element;
    }
    return nullptr;
}

void Document::updateHoverActiveState(const HitTestRequest& request, Element* innerElement, CaptureChange captureElementChanged)
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
        for (Element* currentElement = oldActiveElement; currentElement; currentElement = currentElement->parentElementInComposedTree()) {
            currentElement->setActive(false);
            m_userActionElements.setInActiveChain(*currentElement, false);
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
                m_userActionElements.setInActiveChain(*element, true);
            }

            m_activeElement = newActiveElement;
        }
    }
    // If the mouse has just been pressed, set :active on the chain. Those (and only those)
    // nodes should remain :active until the mouse is released.
    bool allowActiveChanges = !oldActiveElement && m_activeElement;

    // If the mouse is down and if this is a mouse move event, we want to restrict changes in
    // :hover/:active to only apply to elements that are in the :active chain that we froze
    // at the time the mouse went down, unless the capture element changed.
    bool mustBeInActiveChain = request.active() && request.move() && captureElementChanged == CaptureChange::No;

    RefPtr<Element> oldHoveredElement = WTFMove(m_hoveredElement);

    // A touch release does not set a new hover target; clearing the element we're working with
    // will clear the chain of hovered elements all the way to the top of the tree.
    if (request.touchRelease())
        innerElementInDocument = nullptr;

    // Check to see if the hovered Element has changed.
    // If it hasn't, we do not need to do anything.
    Element* newHoveredElement = innerElementInDocument;
    while (newHoveredElement && !newHoveredElement->renderer())
        newHoveredElement = newHoveredElement->parentElementInComposedTree();

    m_hoveredElement = newHoveredElement;

    auto* commonAncestor = findNearestCommonComposedAncestor(oldHoveredElement.get(), newHoveredElement);

    Vector<RefPtr<Element>, 32> elementsToRemoveFromChain;
    Vector<RefPtr<Element>, 32> elementsToAddToChain;

    if (oldHoveredElement != newHoveredElement) {
        for (auto* element = oldHoveredElement.get(); element; element = element->parentElementInComposedTree()) {
            if (element == commonAncestor)
                break;
            if (!mustBeInActiveChain || element->isInActiveChain())
                elementsToRemoveFromChain.append(element);
        }
        // Unset hovered nodes in sub frame documents if the old hovered node was a frame owner.
        if (is<HTMLFrameOwnerElement>(oldHoveredElement)) {
            if (auto* contentDocument = downcast<HTMLFrameOwnerElement>(*oldHoveredElement).contentDocument())
                contentDocument->updateHoverActiveState(request, nullptr);
        }
    }

    for (auto* element = newHoveredElement; element; element = element->parentElementInComposedTree()) {
        if (!mustBeInActiveChain || element->isInActiveChain())
            elementsToAddToChain.append(element);
    }

    for (auto& element : elementsToRemoveFromChain)
        element->setHovered(false);

    bool sawCommonAncestor = false;
    for (auto& element : elementsToAddToChain) {
        if (allowActiveChanges)
            element->setActive(true);
        if (element == commonAncestor)
            sawCommonAncestor = true;
        if (!sawCommonAncestor) {
            // Elements after the common hover ancestor does not change hover state, but are iterated over because they may change active state.
            element->setHovered(true);
        }
    }
}

bool Document::haveStylesheetsLoaded() const
{
    return !styleScope().hasPendingSheets() || m_ignorePendingStylesheets;
}

Locale& Document::getCachedLocale(const AtomString& locale)
{
    AtomString localeKey = locale;
    if (locale.isEmpty() || !settings().langAttributeAwareFormControlUIEnabled())
        localeKey = defaultLanguage();
    LocaleIdentifierToLocaleMap::AddResult result = m_localeCache.add(localeKey, nullptr);
    if (result.isNewEntry)
        result.iterator->value = Locale::create(localeKey);
    return *(result.iterator->value);
}

Document& Document::ensureTemplateDocument()
{
    if (const Document* document = templateDocument())
        return const_cast<Document&>(*document);

    if (isHTMLDocument())
        m_templateDocument = HTMLDocument::create(nullptr, aboutBlankURL());
    else
        m_templateDocument = create(aboutBlankURL());

    m_templateDocument->setContextDocument(contextDocument());
    m_templateDocument->setTemplateDocumentHost(this); // balanced in dtor.

    return *m_templateDocument;
}

Ref<FontFaceSet> Document::fonts()
{
    updateStyleIfNeeded();
    return fontSelector().fontFaceSet();
}
    
EditingBehavior Document::editingBehavior() const
{
    return EditingBehavior { settings().editingBehaviorType() };
}

float Document::deviceScaleFactor() const
{
    float deviceScaleFactor = 1.0;
    if (Page* documentPage = page())
        deviceScaleFactor = documentPage->deviceScaleFactor();
    return deviceScaleFactor;
}

bool Document::useSystemAppearance() const
{
    if (auto* documentPage = page())
        return documentPage->useSystemAppearance();
    return false;
}

bool Document::useDarkAppearance(const RenderStyle* style) const
{
#if ENABLE(DARK_MODE_CSS)
    OptionSet<ColorScheme> colorScheme;

    // Use the style's supported color schemes, if supplied.
    if (style)
        colorScheme = style->colorScheme().colorScheme();

    // Fallback to the document's supported color schemes if style was empty (auto).
    if (colorScheme.isEmpty())
        colorScheme = m_colorScheme;

    if (colorScheme.contains(ColorScheme::Dark) && !colorScheme.contains(ColorScheme::Light))
        return true;
#else
    UNUSED_PARAM(style);
#endif

    bool pageUsesDarkAppearance = false;
    if (Page* documentPage = page())
        pageUsesDarkAppearance = documentPage->useDarkAppearance();

    if (useSystemAppearance())
        return pageUsesDarkAppearance;

#if ENABLE(DARK_MODE_CSS)
    if (colorScheme.contains(ColorScheme::Dark))
        return pageUsesDarkAppearance;
#endif

    return false;
}

bool Document::useElevatedUserInterfaceLevel() const
{
    if (auto* documentPage = page())
        return documentPage->useElevatedUserInterfaceLevel();
    return false;
}

OptionSet<StyleColor::Options> Document::styleColorOptions(const RenderStyle* style) const
{
    OptionSet<StyleColor::Options> options;
    if (useSystemAppearance())
        options.add(StyleColor::Options::UseSystemAppearance);
    if (useDarkAppearance(style))
        options.add(StyleColor::Options::UseDarkAppearance);
    if (useElevatedUserInterfaceLevel())
        options.add(StyleColor::Options::UseElevatedUserInterfaceLevel);
    return options;
}

CompositeOperator Document::compositeOperatorForBackgroundColor(const Color& color, const RenderObject& renderer) const
{
    if (LIKELY(!settings().punchOutWhiteBackgroundsInDarkMode() || !Color::isWhiteColor(color) || !renderer.useDarkAppearance()))
        return CompositeOperator::SourceOver;

    auto* frameView = view();
    if (!frameView)
        return CompositeOperator::SourceOver;

    // Mail on macOS uses a transparent view, and on iOS it is an opaque view. We need to
    // use different composite modes to get the right results in this case.
    return frameView->isTransparent() ? CompositeOperator::DestinationOut : CompositeOperator::DestinationIn;
}

void Document::didAssociateFormControl(Element& element)
{
    auto* page = this->page();
    if (!page || !page->chrome().client().shouldNotifyOnFormChanges())
        return;
    m_associatedFormControls.add(&element);
    if (!m_didAssociateFormControlsTimer.isActive())
        m_didAssociateFormControlsTimer.startOneShot(0_s);
}

void Document::didAssociateFormControlsTimerFired()
{
    auto vector = copyToVector(m_associatedFormControls);
    m_associatedFormControls.clear();
    if (auto* page = this->page()) {
        ASSERT(m_frame);
        page->chrome().client().didAssociateFormControls(vector, *m_frame);
    }
}

void Document::setCachedDOMCookies(const String& cookies)
{
    ASSERT(!isDOMCookieCacheValid());
    m_cachedDOMCookies = cookies;
    // The cookie cache is valid at most until we go back to the event loop.
    m_cookieCacheExpiryTimer.startOneShot(0_s);
}

void Document::invalidateDOMCookieCache()
{
    m_cookieCacheExpiryTimer.stop();
    m_cachedDOMCookies = String();
}

void Document::didLoadResourceSynchronously(const URL& url)
{
    // Synchronous resources loading can set cookies so we invalidate the cookies cache
    // in this case, to be safe.
    invalidateDOMCookieCache();

    if (auto* page = this->page())
        page->cookieJar().clearCacheForHost(url.host().toString());
}

void Document::ensurePlugInsInjectedScript(DOMWrapperWorld& world)
{
    if (m_hasInjectedPlugInsScript)
        return;

    auto& scriptController = frame()->script();

    // Use the JS file provided by the Chrome client, or fallback to the default one.
    String jsString = page()->chrome().client().plugInExtraScript();
    if (!jsString || !scriptController.shouldAllowUserAgentScripts(*this))
        jsString = String(plugInsJavaScript, sizeof(plugInsJavaScript));

    setHasEvaluatedUserAgentScripts();
    scriptController.evaluateInWorldIgnoringException(ScriptSourceCode(jsString), world);

    m_hasInjectedPlugInsScript = true;
}

#if ENABLE(WEB_CRYPTO)

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

#endif // ENABLE(WEB_CRYPTO)

Element* Document::activeElement()
{
    if (Element* element = treeScope().focusedElementInScope())
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

#if ENABLE(WIRELESS_PLAYBACK_TARGET)

void Document::addPlaybackTargetPickerClient(MediaPlaybackTargetClient& client)
{
    Page* page = this->page();
    if (!page)
        return;

    // FIXME: change this back to an ASSERT once https://webkit.org/b/144970 is fixed.
    if (m_clientToIDMap.contains(&client))
        return;

    auto contextId = PlaybackTargetClientContextIdentifier::generate();
    m_clientToIDMap.add(&client, contextId);
    m_idToClientMap.add(contextId, &client);
    page->addPlaybackTargetPickerClient(contextId);
}

void Document::removePlaybackTargetPickerClient(MediaPlaybackTargetClient& client)
{
    auto it = m_clientToIDMap.find(&client);
    if (it == m_clientToIDMap.end())
        return;

    auto clientId = it->value;
    m_idToClientMap.remove(clientId);
    m_clientToIDMap.remove(it);

    Page* page = this->page();
    if (!page)
        return;
    page->removePlaybackTargetPickerClient(clientId);
}

void Document::showPlaybackTargetPicker(MediaPlaybackTargetClient& client, bool isVideo, RouteSharingPolicy routeSharingPolicy, const String& routingContextUID)
{
    Page* page = this->page();
    if (!page)
        return;

    if (!frame())
        return;

    auto it = m_clientToIDMap.find(&client);
    if (it == m_clientToIDMap.end())
        return;

    // FIXME: This is probably wrong for subframes.
    auto position = frame()->eventHandler().lastKnownMousePosition();
    page->showPlaybackTargetPicker(it->value, position, isVideo, routeSharingPolicy, routingContextUID);
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

void Document::playbackTargetAvailabilityDidChange(PlaybackTargetClientContextIdentifier contextId, bool available)
{
    auto it = m_idToClientMap.find(contextId);
    if (it == m_idToClientMap.end())
        return;

    it->value->externalOutputDeviceAvailableDidChange(available);
}

void Document::setPlaybackTarget(PlaybackTargetClientContextIdentifier contextId, Ref<MediaPlaybackTarget>&& target)
{
    auto it = m_idToClientMap.find(contextId);
    if (it == m_idToClientMap.end())
        return;

    it->value->setPlaybackTarget(target.copyRef());
}

void Document::setShouldPlayToPlaybackTarget(PlaybackTargetClientContextIdentifier contextId, bool shouldPlay)
{
    auto it = m_idToClientMap.find(contextId);
    if (it == m_idToClientMap.end())
        return;

    it->value->setShouldPlayToPlaybackTarget(shouldPlay);
}

void Document::playbackTargetPickerWasDismissed(PlaybackTargetClientContextIdentifier contextId)
{
    auto it = m_idToClientMap.find(contextId);
    if (it == m_idToClientMap.end())
        return;

    it->value->playbackTargetPickerWasDismissed();
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

bool Document::shouldEnforceHTTP09Sandbox() const
{
    if (m_isSynthesized || !m_frame)
        return false;
    DocumentLoader* documentLoader = m_frame->loader().activeDocumentLoader();
    return documentLoader && documentLoader->response().isHTTP09();
}

#if USE(QUICK_LOOK)

bool Document::shouldEnforceQuickLookSandbox() const
{
    if (m_isSynthesized || !m_frame)
        return false;
    DocumentLoader* documentLoader = m_frame->loader().activeDocumentLoader();
    return documentLoader && documentLoader->response().isQuickLook();
}

void Document::applyQuickLookSandbox()
{
    auto& documentLoader = *m_frame->loader().activeDocumentLoader();
    auto documentURL = documentLoader.documentURL();
    auto& responseURL = documentLoader.responseURL();
    ASSERT(!documentURL.protocolIs(QLPreviewProtocol));
    ASSERT(responseURL.protocolIs(QLPreviewProtocol));

    auto securityOrigin = SecurityOrigin::createNonLocalWithAllowedFilePath(responseURL, documentURL.fileSystemPath());
    securityOrigin->setStorageBlockingPolicy(SecurityOrigin::BlockAllStorage);
    setSecurityOriginPolicy(SecurityOriginPolicy::create(WTFMove(securityOrigin)));

    static NeverDestroyed<String> quickLookCSP = makeString("default-src ", QLPreviewProtocol, ": 'unsafe-inline'; base-uri 'none'; sandbox allow-same-origin allow-scripts");
    RELEASE_ASSERT(contentSecurityPolicy());
    // The sandbox directive is only allowed if the policy is from an HTTP header.
    contentSecurityPolicy()->didReceiveHeader(quickLookCSP, ContentSecurityPolicyHeaderType::Enforce, ContentSecurityPolicy::PolicyFrom::HTTPHeader, referrer());

    disableSandboxFlags(SandboxNavigation);

    setReferrerPolicy(ReferrerPolicy::NoReferrer);
}

#endif

bool Document::shouldEnforceContentDispositionAttachmentSandbox() const
{
    if (!settings().contentDispositionAttachmentSandboxEnabled())
        return false;

    if (m_isSynthesized)
        return false;

    if (auto* documentLoader = m_frame ? m_frame->loader().activeDocumentLoader() : nullptr)
        return documentLoader->response().isAttachment();
    return false;
}

void Document::applyContentDispositionAttachmentSandbox()
{
    ASSERT(shouldEnforceContentDispositionAttachmentSandbox());

    setReferrerPolicy(ReferrerPolicy::NoReferrer);
    if (!isMediaDocument())
        enforceSandboxFlags(SandboxAll);
    else
        enforceSandboxFlags(SandboxOrigin);
}

void Document::addDynamicMediaQueryDependentImage(HTMLImageElement& element)
{
    m_dynamicMediaQueryDependentImages.add(element);
}

void Document::removeDynamicMediaQueryDependentImage(HTMLImageElement& element)
{
    m_dynamicMediaQueryDependentImages.remove(element);
}

void Document::scheduleTimedRenderingUpdate()
{
#if ENABLE(INTERSECTION_OBSERVER)
    m_intersectionObserversInitialUpdateTimer.stop();
#endif
    if (auto page = this->page())
        page->renderingUpdateScheduler().scheduleTimedRenderingUpdate();
}

#if ENABLE(INTERSECTION_OBSERVER)

void Document::addIntersectionObserver(IntersectionObserver& observer)
{
    ASSERT(m_intersectionObservers.find(&observer) == notFound);
    m_intersectionObservers.append(makeWeakPtr(&observer));
}

void Document::removeIntersectionObserver(IntersectionObserver& observer)
{
    m_intersectionObservers.removeFirst(&observer);
}

static void expandRootBoundsWithRootMargin(FloatRect& localRootBounds, const LengthBox& rootMargin, float zoomFactor)
{
    FloatBoxExtent rootMarginFloatBox(
        floatValueForLength(rootMargin.top(), localRootBounds.height()) * zoomFactor,
        floatValueForLength(rootMargin.right(), localRootBounds.width()) * zoomFactor,
        floatValueForLength(rootMargin.bottom(), localRootBounds.height()) * zoomFactor,
        floatValueForLength(rootMargin.left(), localRootBounds.width()) * zoomFactor
    );

    localRootBounds.expand(rootMarginFloatBox);
}

static Optional<LayoutRect> computeClippedRectInRootContentsSpace(const LayoutRect& rect, const RenderElement* renderer)
{
    OptionSet<RenderObject::VisibleRectContextOption> visibleRectOptions = { RenderObject::VisibleRectContextOption::UseEdgeInclusiveIntersection, RenderObject::VisibleRectContextOption::ApplyCompositedClips, RenderObject::VisibleRectContextOption::ApplyCompositedContainerScrolls };
    Optional<LayoutRect> rectInFrameAbsoluteSpace = renderer->computeVisibleRectInContainer(rect, &renderer->view(),  {false /* hasPositionFixedDescendant */, false /* dirtyRectIsFlipped */, visibleRectOptions });
    if (!rectInFrameAbsoluteSpace || renderer->frame().isMainFrame())
        return rectInFrameAbsoluteSpace;

    bool intersects = rectInFrameAbsoluteSpace->edgeInclusiveIntersect(renderer->view().frameView().layoutViewportRect());
    if (!intersects)
        return WTF::nullopt;

    LayoutRect rectInFrameViewSpace(renderer->view().frameView().contentsToView(snappedIntRect(*rectInFrameAbsoluteSpace)));
    auto* ownerRenderer = renderer->frame().ownerRenderer();
    if (!ownerRenderer)
        return WTF::nullopt;

    rectInFrameViewSpace.moveBy(ownerRenderer->contentBoxLocation());
    return computeClippedRectInRootContentsSpace(rectInFrameViewSpace, ownerRenderer);
}

struct IntersectionObservationState {
    FloatRect absoluteTargetRect;
    FloatRect absoluteRootBounds;
    FloatRect absoluteIntersectionRect;
    bool isIntersecting { false };
};

static Optional<IntersectionObservationState> computeIntersectionState(FrameView& frameView, const IntersectionObserver& observer, Element& target, bool applyRootMargin)
{
    auto* targetRenderer = target.renderer();
    if (!targetRenderer)
        return WTF::nullopt;

    FloatRect localRootBounds;
    RenderBlock* rootRenderer;
    if (observer.root()) {
        if (observer.trackingDocument() != &target.document())
            return WTF::nullopt;

        if (!observer.root()->renderer() || !is<RenderBlock>(observer.root()->renderer()))
            return WTF::nullopt;

        rootRenderer = downcast<RenderBlock>(observer.root()->renderer());
        if (!rootRenderer->isContainingBlockAncestorFor(*targetRenderer))
            return WTF::nullopt;

        if (rootRenderer->hasOverflowClip())
            localRootBounds = rootRenderer->contentBoxRect();
        else
            localRootBounds = { FloatPoint(), rootRenderer->size() };
    } else {
        ASSERT(frameView.frame().isMainFrame());
        // FIXME: Handle the case of an implicit-root observer that has a target in a different frame tree.
        if (&targetRenderer->frame().mainFrame() != &frameView.frame())
            return WTF::nullopt;
        rootRenderer = frameView.renderView();
        localRootBounds = frameView.layoutViewportRect();
    }

    if (applyRootMargin)
        expandRootBoundsWithRootMargin(localRootBounds, observer.rootMarginBox(), rootRenderer->style().effectiveZoom());

    LayoutRect localTargetBounds;
    if (is<RenderBox>(*targetRenderer))
        localTargetBounds = downcast<RenderBox>(targetRenderer)->borderBoundingBox();
    else if (is<RenderInline>(targetRenderer)) {
        auto pair = target.boundingAbsoluteRectWithoutLayout();
        if (pair) {
            FloatRect absoluteTargetBounds = pair->second;
            localTargetBounds = enclosingLayoutRect(targetRenderer->absoluteToLocalQuad(absoluteTargetBounds).boundingBox());
        }
    } else if (is<RenderLineBreak>(targetRenderer))
        localTargetBounds = downcast<RenderLineBreak>(targetRenderer)->linesBoundingBox();

    Optional<LayoutRect> rootLocalTargetRect;
    if (observer.root()) {
        OptionSet<RenderObject::VisibleRectContextOption> visibleRectOptions = { RenderObject::VisibleRectContextOption::UseEdgeInclusiveIntersection, RenderObject::VisibleRectContextOption::ApplyCompositedClips, RenderObject::VisibleRectContextOption::ApplyCompositedContainerScrolls };
        rootLocalTargetRect = targetRenderer->computeVisibleRectInContainer(localTargetBounds, rootRenderer, { false /* hasPositionFixedDescendant */, false /* dirtyRectIsFlipped */, visibleRectOptions });
    } else
        rootLocalTargetRect = computeClippedRectInRootContentsSpace(localTargetBounds, targetRenderer);

    FloatRect rootLocalIntersectionRect = localRootBounds;

    IntersectionObservationState intersectionState;
    intersectionState.isIntersecting = rootLocalTargetRect && rootLocalIntersectionRect.edgeInclusiveIntersect(*rootLocalTargetRect);
    intersectionState.absoluteTargetRect = targetRenderer->localToAbsoluteQuad(FloatRect(localTargetBounds)).boundingBox();
    intersectionState.absoluteRootBounds = rootRenderer->localToAbsoluteQuad(localRootBounds).boundingBox();

    if (intersectionState.isIntersecting) {
        FloatRect rootAbsoluteIntersectionRect = rootRenderer->localToAbsoluteQuad(rootLocalIntersectionRect).boundingBox();
        if (&targetRenderer->frame() == &rootRenderer->frame())
            intersectionState.absoluteIntersectionRect = rootAbsoluteIntersectionRect;
        else {
            FloatRect rootViewIntersectionRect = frameView.contentsToView(rootAbsoluteIntersectionRect);
            intersectionState.absoluteIntersectionRect = targetRenderer->view().frameView().rootViewToContents(rootViewIntersectionRect);
        }
        intersectionState.isIntersecting = intersectionState.absoluteIntersectionRect.edgeInclusiveIntersect(intersectionState.absoluteTargetRect);
    }

    return intersectionState;
}

void Document::updateIntersectionObservations()
{
    auto* frameView = view();
    if (!frameView)
        return;

    bool needsLayout = frameView->layoutContext().isLayoutPending() || (renderView() && renderView()->needsLayout());
    if (needsLayout || hasPendingStyleRecalc())
        return;

    Vector<WeakPtr<IntersectionObserver>> intersectionObserversWithPendingNotifications;

    for (const auto& observer : m_intersectionObservers) {
        bool needNotify = false;
        auto timestamp = observer->nowTimestamp();
        if (!timestamp)
            continue;
        for (Element* target : observer->observationTargets()) {
            auto& targetRegistrations = target->intersectionObserverDataIfExists()->registrations;
            auto index = targetRegistrations.findMatching([observer](auto& registration) {
                return registration.observer.get() == observer;
            });
            ASSERT(index != notFound);
            auto& registration = targetRegistrations[index];

            bool isSameOriginObservation = &target->document() == this || target->document().securityOrigin().canAccess(securityOrigin());
            auto intersectionState = computeIntersectionState(*frameView, *observer, *target, isSameOriginObservation);

            float intersectionRatio = 0;
            size_t thresholdIndex = 0;
            if (intersectionState) {
                if (intersectionState->isIntersecting) {
                    float absTargetArea = intersectionState->absoluteTargetRect.area();
                    if (absTargetArea)
                        intersectionRatio = intersectionState->absoluteIntersectionRect.area() / absTargetArea;
                    else
                        intersectionRatio = 1;

                    for (auto threshold : observer->thresholds()) {
                        if (!(threshold <= intersectionRatio || WTF::areEssentiallyEqual<float>(threshold, intersectionRatio)))
                            break;
                        ++thresholdIndex;
                    }
                }
            }

            if (!registration.previousThresholdIndex || thresholdIndex != registration.previousThresholdIndex) {
                FloatRect targetBoundingClientRect;
                FloatRect clientIntersectionRect;
                FloatRect clientRootBounds;
                if (intersectionState) {
                    auto* targetFrameView = target->document().view();
                    targetBoundingClientRect = targetFrameView->absoluteToClientRect(intersectionState->absoluteTargetRect, target->renderer()->style().effectiveZoom());
                    auto* rootRenderer = observer->root() ? observer->root()->renderer() : frameView->renderView();
                    clientRootBounds = frameView->absoluteToClientRect(intersectionState->absoluteRootBounds, rootRenderer->style().effectiveZoom());
                    if (intersectionState->isIntersecting)
                        clientIntersectionRect = targetFrameView->absoluteToClientRect(intersectionState->absoluteIntersectionRect, target->renderer()->style().effectiveZoom());
                }

                Optional<DOMRectInit> reportedRootBounds;
                if (isSameOriginObservation) {
                    reportedRootBounds = DOMRectInit({
                        clientRootBounds.x(),
                        clientRootBounds.y(),
                        clientRootBounds.width(),
                        clientRootBounds.height()
                    });
                }

                observer->appendQueuedEntry(IntersectionObserverEntry::create({
                    timestamp->milliseconds(),
                    reportedRootBounds,
                    { targetBoundingClientRect.x(), targetBoundingClientRect.y(), targetBoundingClientRect.width(), targetBoundingClientRect.height() },
                    { clientIntersectionRect.x(), clientIntersectionRect.y(), clientIntersectionRect.width(), clientIntersectionRect.height() },
                    intersectionRatio,
                    target,
                    thresholdIndex > 0,
                }));
                needNotify = true;
                registration.previousThresholdIndex = thresholdIndex;
            }
        }
        if (needNotify)
            intersectionObserversWithPendingNotifications.append(makeWeakPtr(observer.get()));
    }

    for (const auto& observer : intersectionObserversWithPendingNotifications) {
        if (observer)
            observer->notify();
    }
}

void Document::scheduleInitialIntersectionObservationUpdate()
{
    if (m_readyState == Complete)
        scheduleTimedRenderingUpdate();
    else if (!m_intersectionObserversInitialUpdateTimer.isActive())
        m_intersectionObserversInitialUpdateTimer.startOneShot(intersectionObserversInitialUpdateDelay);
}

IntersectionObserverData& Document::ensureIntersectionObserverData()
{
    if (!m_intersectionObserverData)
        m_intersectionObserverData = makeUnique<IntersectionObserverData>();
    return *m_intersectionObserverData;
}

#endif

#if ENABLE(RESIZE_OBSERVER)

void Document::addResizeObserver(ResizeObserver& observer)
{
    if (!m_resizeObservers.contains(&observer))
        m_resizeObservers.append(makeWeakPtr(&observer));
}

void Document::removeResizeObserver(ResizeObserver& observer)
{
    m_resizeObservers.removeFirst(&observer);
}

bool Document::hasResizeObservers()
{
    return !m_resizeObservers.isEmpty();
}

size_t Document::gatherResizeObservations(size_t deeperThan)
{
    size_t minDepth = ResizeObserver::maxElementDepth();
    for (const auto& observer : m_resizeObservers) {
        if (!observer->hasObservations())
            continue;
        auto depth = observer->gatherObservations(deeperThan);
        minDepth = std::min(minDepth, depth);
    }
    return minDepth;
}

void Document::deliverResizeObservations()
{
    auto observersToNotify = m_resizeObservers;
    for (const auto& observer : observersToNotify) {
        if (!observer || !observer->hasActiveObservations())
            continue;
        observer->deliverObservations();
    }
}

bool Document::hasSkippedResizeObservations() const
{
    for (const auto& observer : m_resizeObservers) {
        if (observer->hasSkippedObservations())
            return true;
    }
    return false;
}

void Document::setHasSkippedResizeObservations(bool skipped)
{
    for (const auto& observer : m_resizeObservers)
        observer->setHasSkippedObservations(skipped);
}

void Document::updateResizeObservations(Page& page)
{
    if (!hasResizeObservers())
        return;

    // We need layout the whole frame tree here. Because ResizeObserver could observe element in other frame,
    // and it could change other frame in deliverResizeObservations().
    page.layoutIfNeeded();

    // Start check resize obervers;
    for (size_t depth = gatherResizeObservations(0); depth != ResizeObserver::maxElementDepth(); depth = gatherResizeObservations(depth)) {
        deliverResizeObservations();
        page.layoutIfNeeded();
    }

    if (hasSkippedResizeObservations()) {
        setHasSkippedResizeObservations(false);
        String url;
        unsigned line = 0;
        unsigned column = 0;
        getParserLocation(url, line, column);
        reportException("ResizeObserver loop completed with undelivered notifications.", line, column, url, nullptr, nullptr);
        // Starting a new schedule the next round of notify.
        scheduleTimedRenderingUpdate();
    }
}

#endif

const AtomString& Document::dir() const
{
    auto* documentElement = this->documentElement();
    if (!is<HTMLHtmlElement>(documentElement))
        return nullAtom();
    return downcast<HTMLHtmlElement>(*documentElement).dir();
}

void Document::setDir(const AtomString& value)
{
    auto* documentElement = this->documentElement();
    if (is<HTMLHtmlElement>(documentElement))
        downcast<HTMLHtmlElement>(*documentElement).setDir(value);
}

DOMSelection* Document::getSelection()
{
    return m_domWindow ? m_domWindow->getSelection() : nullptr;
}

void Document::didInsertInDocumentShadowRoot(ShadowRoot& shadowRoot)
{
    ASSERT(shadowRoot.isConnected());
    ASSERT(!m_inDocumentShadowRoots.contains(&shadowRoot));
    m_inDocumentShadowRoots.add(&shadowRoot);
}

void Document::didRemoveInDocumentShadowRoot(ShadowRoot& shadowRoot)
{
    ASSERT(m_inDocumentShadowRoots.contains(&shadowRoot));
    m_inDocumentShadowRoots.remove(&shadowRoot);
}

void Document::orientationChanged(int orientation)
{
    LOG(Events, "Document %p orientationChanged - orientation %d", this, orientation);
    dispatchWindowEvent(Event::create(eventNames().orientationchangeEvent, Event::CanBubble::No, Event::IsCancelable::No));
    m_orientationNotifier.orientationChanged(orientation);
}

#if ENABLE(MEDIA_STREAM)

void Document::stopMediaCapture()
{
    MediaStreamTrack::endCapture(*this);
}

void Document::mediaStreamCaptureStateChanged()
{
    if (!MediaProducer::isCapturing(m_mediaState))
        return;

    forEachMediaElement([] (HTMLMediaElement& element) {
        element.mediaStreamCaptureStarted();
    });
}

void Document::setDeviceIDHashSalt(const String& salt)
{
    ASSERT(m_idHashSalt.isEmpty() || m_idHashSalt == salt);
    m_idHashSalt = salt;
}

#endif

const AtomString& Document::bgColor() const
{
    auto* bodyElement = body();
    if (!bodyElement)
        return emptyAtom();
    return bodyElement->attributeWithoutSynchronization(bgcolorAttr);
}

void Document::setBgColor(const String& value)
{
    if (auto* bodyElement = body())
        bodyElement->setAttributeWithoutSynchronization(bgcolorAttr, value);
}

const AtomString& Document::fgColor() const
{
    auto* bodyElement = body();
    if (!bodyElement)
        return emptyAtom();
    return bodyElement->attributeWithoutSynchronization(textAttr);
}

void Document::setFgColor(const String& value)
{
    if (auto* bodyElement = body())
        bodyElement->setAttributeWithoutSynchronization(textAttr, value);
}

const AtomString& Document::alinkColor() const
{
    auto* bodyElement = body();
    if (!bodyElement)
        return emptyAtom();
    return bodyElement->attributeWithoutSynchronization(alinkAttr);
}

void Document::setAlinkColor(const String& value)
{
    if (auto* bodyElement = body())
        bodyElement->setAttributeWithoutSynchronization(alinkAttr, value);
}

const AtomString& Document::linkColorForBindings() const
{
    auto* bodyElement = body();
    if (!bodyElement)
        return emptyAtom();
    return bodyElement->attributeWithoutSynchronization(linkAttr);
}

void Document::setLinkColorForBindings(const String& value)
{
    if (auto* bodyElement = body())
        bodyElement->setAttributeWithoutSynchronization(linkAttr, value);
}

const AtomString& Document::vlinkColor() const
{
    auto* bodyElement = body();
    if (!bodyElement)
        return emptyAtom();
    return bodyElement->attributeWithoutSynchronization(vlinkAttr);
}

void Document::setVlinkColor(const String& value)
{
    if (auto* bodyElement = body())
        bodyElement->setAttributeWithoutSynchronization(vlinkAttr, value);
}

Logger& Document::logger()
{
    if (!m_logger) {
        m_logger = Logger::create(this);
        auto* page = this->page();
        m_logger->setEnabled(this, page && page->sessionID().isAlwaysOnLoggingAllowed());
        m_logger->addObserver(*this);
    }

    return *m_logger;
}
    
Optional<PageIdentifier> Document::pageID() const
{
    return m_frame->loader().pageID();
}

Optional<FrameIdentifier> Document::frameID() const
{
    return m_frame->loader().frameID();
}

void Document::registerArticleElement(Element& article)
{
    m_articleElements.add(&article);
}

void Document::unregisterArticleElement(Element& article)
{
    m_articleElements.remove(&article);
    if (m_mainArticleElement == &article)
        m_mainArticleElement = nullptr;
}

void Document::updateMainArticleElementAfterLayout()
{
    ASSERT(page() && page()->requestedLayoutMilestones().contains(DidRenderSignificantAmountOfText));

    // If there are too many article elements on the page, don't consider any one of them to be "main content".
    const unsigned maxNumberOfArticlesBeforeIgnoringMainContentArticle = 10;

    // We consider an article to be main content if it is either:
    // 1. The only article element in the document.
    // 2. Much taller than the next tallest article, and also much larger than the viewport.
    const float minimumSecondTallestArticleHeightFactor = 4;
    const float minimumViewportAreaFactor = 5;

    m_mainArticleElement = nullptr;

    auto numberOfArticles = m_articleElements.size();
    if (!numberOfArticles || numberOfArticles > maxNumberOfArticlesBeforeIgnoringMainContentArticle)
        return;

    Element* tallestArticle = nullptr;
    float tallestArticleHeight = 0;
    float tallestArticleWidth = 0;
    float secondTallestArticleHeight = 0;

    for (auto* article : m_articleElements) {
        auto* box = article->renderBox();
        float height = box ? box->height().toFloat() : 0;
        if (height >= tallestArticleHeight) {
            secondTallestArticleHeight = tallestArticleHeight;
            tallestArticleHeight = height;
            tallestArticleWidth = box ? box->width().toFloat() : 0;
            tallestArticle = article;
        } else if (height >= secondTallestArticleHeight)
            secondTallestArticleHeight = height;
    }

    if (numberOfArticles == 1) {
        m_mainArticleElement = tallestArticle;
        return;
    }

    if (tallestArticleHeight < minimumSecondTallestArticleHeightFactor * secondTallestArticleHeight)
        return;

    if (!view())
        return;

    auto viewportSize = view()->layoutViewportRect().size();
    if (tallestArticleWidth * tallestArticleHeight < minimumViewportAreaFactor * (viewportSize.width() * viewportSize.height()).toFloat())
        return;

    m_mainArticleElement = tallestArticle;
}

#if ENABLE(RESOURCE_LOAD_STATISTICS)
bool Document::hasRequestedPageSpecificStorageAccessWithUserInteraction(const RegistrableDomain& domain)
{
    return m_registrableDomainRequestedPageSpecificStorageAccessWithUserInteraction == domain;
}

void Document::setHasRequestedPageSpecificStorageAccessWithUserInteraction(const RegistrableDomain& domain)
{
    m_registrableDomainRequestedPageSpecificStorageAccessWithUserInteraction = domain;
}

void Document::wasLoadedWithDataTransferFromPrevalentResource()
{
    downgradeReferrerToRegistrableDomain();
}

void Document::downgradeReferrerToRegistrableDomain()
{
    auto referrerStr = referrer();
    if (referrerStr.isEmpty())
        return;

    URL referrerURL { URL(), referrerStr };
    auto referrerPort = referrerURL.port();
    RegistrableDomain referrerRegistrableDomain { referrerURL };
    auto referrerRegistrableDomainStr = referrerRegistrableDomain.string();
    if (referrerRegistrableDomainStr.isEmpty())
        return;

    StringBuilder builder;
    builder.append(referrerURL.protocol());
    builder.appendLiteral("://");
    builder.append(referrerRegistrableDomainStr);
    if (referrerPort) {
        builder.append(':');
        builder.appendNumber(*referrerPort);
    }
    builder.append('/');

    m_referrerOverride = builder.toString();
}
#endif

void Document::setConsoleMessageListener(RefPtr<StringCallback>&& listener)
{
    m_consoleMessageListener = listener;
}

DocumentTimelinesController& Document::ensureTimelinesController()
{
    if (!m_timelinesController)
        m_timelinesController = makeUnique<DocumentTimelinesController>(*this);
    return *m_timelinesController.get();
}

DocumentTimeline& Document::timeline()
{
    if (!m_timeline)
        m_timeline = DocumentTimeline::create(*this);

    return *m_timeline;
}

Vector<RefPtr<WebAnimation>> Document::getAnimations()
{
    return matchingAnimations([] (Element& target) -> bool {
        return !target.containingShadowRoot();
    });
}

Vector<RefPtr<WebAnimation>> Document::matchingAnimations(const WTF::Function<bool(Element&)>& function)
{
    // For the list of animations to be current, we need to account for any pending CSS changes,
    // such as updates to CSS Animations and CSS Transitions.
    updateStyleIfNeeded();

    Vector<RefPtr<WebAnimation>> animations;
    for (auto* animation : WebAnimation::instances()) {
        if (!animation || !animation->isRelevant() || !is<KeyframeEffect>(animation->effect()))
            continue;

        auto* target = downcast<KeyframeEffect>(*animation->effect()).targetElementOrPseudoElement();
        if (target && target->isConnected() && &target->document() == this && function(*target))
            animations.append(animation);
    }

    std::stable_sort(animations.begin(), animations.end(), [](auto& lhs, auto& rhs) {
        return compareAnimationsByCompositeOrder(*lhs, *rhs);
    });

    return animations;
}

#if ENABLE(ATTACHMENT_ELEMENT)

void Document::registerAttachmentIdentifier(const String& identifier)
{
    // FIXME: Can this null check for Frame be removed?
    if (frame())
        editor().registerAttachmentIdentifier(identifier);
}

void Document::didInsertAttachmentElement(HTMLAttachmentElement& attachment)
{
    auto identifier = attachment.uniqueIdentifier();
    auto previousIdentifier = identifier;
    bool previousIdentifierIsNotUnique = !previousIdentifier.isEmpty() && m_attachmentIdentifierToElementMap.contains(previousIdentifier);
    if (identifier.isEmpty() || previousIdentifierIsNotUnique) {
        previousIdentifier = identifier;
        identifier = createCanonicalUUIDString();
        attachment.setUniqueIdentifier(identifier);
    }

    m_attachmentIdentifierToElementMap.set(identifier, attachment);

    // FIXME: Can this null check for Frame be removed?
    if (frame()) {
        if (previousIdentifierIsNotUnique)
            editor().cloneAttachmentData(previousIdentifier, identifier);
        editor().didInsertAttachmentElement(attachment);
    }
}

void Document::didRemoveAttachmentElement(HTMLAttachmentElement& attachment)
{
    auto identifier = attachment.uniqueIdentifier();
    if (!identifier)
        return;

    m_attachmentIdentifierToElementMap.remove(identifier);

    // FIXME: Can this null check for Frame be removed?
    if (frame())
        editor().didRemoveAttachmentElement(attachment);
}

RefPtr<HTMLAttachmentElement> Document::attachmentForIdentifier(const String& identifier) const
{
    return m_attachmentIdentifierToElementMap.get(identifier);
}

#endif // ENABLE(ATTACHMENT_ELEMENT)

static MessageSource messageSourceForWTFLogChannel(const WTFLogChannel& channel)
{
    static const NeverDestroyed<String> mediaChannel = MAKE_STATIC_STRING_IMPL("media");
    static const NeverDestroyed<String> webrtcChannel = MAKE_STATIC_STRING_IMPL("webrtc");
    static const NeverDestroyed<String> mediaSourceChannel = MAKE_STATIC_STRING_IMPL("mediasource");

    if (equalIgnoringASCIICase(mediaChannel, channel.name))
        return MessageSource::Media;

    if (equalIgnoringASCIICase(webrtcChannel, channel.name))
        return MessageSource::WebRTC;

    if (equalIgnoringASCIICase(mediaSourceChannel, channel.name))
        return MessageSource::MediaSource;

    return MessageSource::Other;
}

static MessageLevel messageLevelFromWTFLogLevel(WTFLogLevel level)
{
    switch (level) {
    case WTFLogLevel::Always:
        return MessageLevel::Log;
    case WTFLogLevel::Error:
        return MessageLevel::Error;
        break;
    case WTFLogLevel::Warning:
        return MessageLevel::Warning;
        break;
    case WTFLogLevel::Info:
        return MessageLevel::Info;
        break;
    case WTFLogLevel::Debug:
        return MessageLevel::Debug;
        break;
    }

    ASSERT_NOT_REACHED();
    return MessageLevel::Log;
}

static inline Vector<JSONLogValue> crossThreadCopy(Vector<JSONLogValue>&& source)
{
    auto values = WTFMove(source);
    for (auto& value : values)
        value.value = crossThreadCopy(WTFMove(value.value));
    return values;
}

void Document::didLogMessage(const WTFLogChannel& channel, WTFLogLevel level, Vector<JSONLogValue>&& logMessages)
{
    if (!isMainThread()) {
        postTask([this, channel, level, logMessages = crossThreadCopy(WTFMove(logMessages))](auto&) mutable {
            didLogMessage(channel, level, WTFMove(logMessages));
        });
        return;
    }
    auto* page = this->page();
    if (!page)
        return;

    ASSERT(page->sessionID().isAlwaysOnLoggingAllowed());

    auto messageSource = messageSourceForWTFLogChannel(channel);
    if (messageSource == MessageSource::Other)
        return;

    m_logMessageTaskQueue.enqueueTask([this, level, messageSource, logMessages = WTFMove(logMessages)]() mutable {
        if (!this->page())
            return;

        auto messageLevel = messageLevelFromWTFLogLevel(level);
        auto message = makeUnique<Inspector::ConsoleMessage>(messageSource, MessageType::Log, messageLevel, WTFMove(logMessages), mainWorldExecState(frame()));

        addConsoleMessage(WTFMove(message));
    });
}

#if ENABLE(SERVICE_WORKER)
void Document::setServiceWorkerConnection(SWClientConnection* serviceWorkerConnection)
{
    if (m_serviceWorkerConnection == serviceWorkerConnection || m_hasPreparedForDestruction || m_isSuspended)
        return;

    if (m_serviceWorkerConnection)
        m_serviceWorkerConnection->unregisterServiceWorkerClient(identifier());

    m_serviceWorkerConnection = serviceWorkerConnection;

    if (!m_serviceWorkerConnection)
        return;

    auto controllingServiceWorkerRegistrationIdentifier = activeServiceWorker() ? makeOptional<ServiceWorkerRegistrationIdentifier>(activeServiceWorker()->registrationIdentifier()) : WTF::nullopt;
    m_serviceWorkerConnection->registerServiceWorkerClient(topOrigin(), ServiceWorkerClientData::from(*this, *serviceWorkerConnection), controllingServiceWorkerRegistrationIdentifier, userAgent(url()));
}
#endif

String Document::signedPublicKeyAndChallengeString(unsigned keySizeIndex, const String& challengeString, const URL& url)
{
    Page* page = this->page();
    if (!page)
        return emptyString();
    return page->chrome().client().signedPublicKeyAndChallengeString(keySizeIndex, challengeString, url);
}

bool Document::registerCSSProperty(CSSRegisteredCustomProperty&& prop)
{
    return m_CSSRegisteredPropertySet.add(prop.name, makeUnique<CSSRegisteredCustomProperty>(WTFMove(prop))).isNewEntry;
}

void Document::detachFromFrame()
{
    // Assertion to help pinpint rdar://problem/49877867. If this hits, the crash trace should tell us
    // which piece of code is detaching the document from its frame while constructing the CachedFrames.
    RELEASE_ASSERT(m_mayBeDetachedFromFrame);

    observeFrame(nullptr);
}

void Document::frameWasDisconnectedFromOwner()
{
    if (!frame())
        return;

    if (auto* window = domWindow())
        window->willDetachDocumentFromFrame();

    detachFromFrame();
}

bool Document::hitTest(const HitTestRequest& request, HitTestResult& result)
{
    return hitTest(request, result.hitTestLocation(), result);
}

bool Document::hitTest(const HitTestRequest& request, const HitTestLocation& location, HitTestResult& result)
{
    Ref<Document> protectedThis(*this);
    updateLayout();
    if (!renderView())
        return false;

#if ASSERT_ENABLED
    SetForScope<bool> hitTestRestorer { m_inHitTesting, true };
#endif

    auto& frameView = renderView()->frameView();
    Ref<FrameView> protector(frameView);

    FrameFlatteningLayoutDisallower disallower(frameView);

    bool resultLayer = renderView()->layer()->hitTest(request, location, result);

    // ScrollView scrollbars are not the same as RenderLayer scrollbars tested by RenderLayer::hitTestOverflowControls,
    // so we need to test ScrollView scrollbars separately here. In case of using overlay scrollbars, the layer hit test
    // will always work so we need to check the ScrollView scrollbars in that case too.
    if (!resultLayer || ScrollbarTheme::theme().usesOverlayScrollbars()) {
        // FIXME: Consider if this test should be done unconditionally.
        if (request.allowsFrameScrollbars()) {
            IntPoint windowPoint = frameView.contentsToWindow(location.roundedPoint());
            if (auto* frameScrollbar = frameView.scrollbarAtPoint(windowPoint)) {
                result.setScrollbar(frameScrollbar);
                return true;
            }
        }
    }
    return resultLayer;
}

ElementIdentifier Document::identifierForElement(Element& element)
{
    ASSERT(&element.document() == this);
    auto result = m_identifiedElementsMap.ensure(&element, [&] {
        return element.createElementIdentifier();
    });
    return result.iterator->value;
}

Element* Document::searchForElementByIdentifier(const ElementIdentifier& identifier)
{
    for (auto it = m_identifiedElementsMap.begin(); it != m_identifiedElementsMap.end(); ++it) {
        if (it->value == identifier)
            return it->key;
    }

    return nullptr;
}

void Document::identifiedElementWasRemovedFromDocument(Element& element)
{
    m_identifiedElementsMap.remove(&element);
}

#if ENABLE(DEVICE_ORIENTATION)

DeviceOrientationAndMotionAccessController& Document::deviceOrientationAndMotionAccessController()
{
    if (&topDocument() != this)
        return topDocument().deviceOrientationAndMotionAccessController();

    if (!m_deviceOrientationAndMotionAccessController)
        m_deviceOrientationAndMotionAccessController = makeUnique<DeviceOrientationAndMotionAccessController>(*this);
    return *m_deviceOrientationAndMotionAccessController;
}

#endif

#if ENABLE(CSS_PAINTING_API)
Worklet& Document::ensurePaintWorklet()
{
    if (!m_paintWorklet)
        m_paintWorklet = Worklet::create();
    return *m_paintWorklet;
}

PaintWorkletGlobalScope* Document::paintWorkletGlobalScopeForName(const String& name)
{
    return m_paintWorkletGlobalScopes.get(name);
}

void Document::setPaintWorkletGlobalScopeForName(const String& name, Ref<PaintWorkletGlobalScope>&& scope)
{
    auto addResult = m_paintWorkletGlobalScopes.add(name, WTFMove(scope));
    ASSERT_UNUSED(addResult, addResult);
}
#endif

#if PLATFORM(IOS_FAMILY)

ContentChangeObserver& Document::contentChangeObserver()
{
    if (!m_contentChangeObserver)
        m_contentChangeObserver = makeUnique<ContentChangeObserver>(*this);
    return *m_contentChangeObserver; 
}

DOMTimerHoldingTank& Document::domTimerHoldingTank()
{
    if (m_domTimerHoldingTank)
        return *m_domTimerHoldingTank;
    m_domTimerHoldingTank = makeUnique<DOMTimerHoldingTank>();
    return *m_domTimerHoldingTank;
}

#endif

bool Document::hasEvaluatedUserAgentScripts() const
{
    auto& top = topDocument();
    return this == &top ? m_hasEvaluatedUserAgentScripts : top.hasEvaluatedUserAgentScripts();
}

bool Document::isRunningUserScripts() const
{
    auto& top = topDocument();
    return this == &top ? m_isRunningUserScripts : top.isRunningUserScripts();
}

void Document::setAsRunningUserScripts()
{
    auto& top = topDocument();
    if (this == &top)
        m_isRunningUserScripts = true;
    else
        top.setAsRunningUserScripts();
}

void Document::setHasEvaluatedUserAgentScripts()
{
    auto& top = topDocument();
    if (this == &top)
        m_hasEvaluatedUserAgentScripts = true;
    else
        top.setHasEvaluatedUserAgentScripts();
}

void Document::didRejectSyncXHRDuringPageDismissal()
{
    ++m_numberOfRejectedSyncXHRs;
    if (m_numberOfRejectedSyncXHRs > 1)
        return;

    postTask([this, weakThis = makeWeakPtr(*this)](auto&) mutable {
        if (weakThis)
            m_numberOfRejectedSyncXHRs = 0;
    });
}

bool Document::shouldIgnoreSyncXHRs() const
{
    const unsigned maxRejectedSyncXHRsPerEventLoopIteration = 5;
    return m_numberOfRejectedSyncXHRs > maxRejectedSyncXHRsPerEventLoopIteration;
}

bool Document::isAlwaysOnLoggingAllowed() const
{
    return !m_frame || m_frame->isAlwaysOnLoggingAllowed();
}

#if ENABLE(APPLE_PAY)

bool Document::isApplePayActive() const
{
    auto& top = topDocument();
    return this == &top ? m_hasStartedApplePaySession : top.isApplePayActive();
}

void Document::setApplePayIsActive()
{
    auto& top = topDocument();
    if (this == &top)
        m_hasStartedApplePaySession = true;
    else
        top.setApplePayIsActive();
}

#endif

MessagePortChannelProvider& Document::messagePortChannelProvider()
{
    return MessagePortChannelProvider::singleton();
}

#if USE(SYSTEM_PREVIEW)
void Document::dispatchSystemPreviewActionEvent(const SystemPreviewInfo& systemPreviewInfo, const String& message)
{
    auto* element = searchForElementByIdentifier(systemPreviewInfo.element.elementIdentifier);
    if (!element)
        return;

    if (!is<HTMLAnchorElement>(element))
        return;

    auto event = MessageEvent::create(message, securityOrigin().toString());
    UserGestureIndicator gestureIndicator(ProcessingUserGesture, this);
    element->dispatchEvent(event);
}
#endif

#if ENABLE(PICTURE_IN_PICTURE_API)
HTMLVideoElement* Document::pictureInPictureElement() const
{
    return m_pictureInPictureElement.get();
};

void Document::setPictureInPictureElement(HTMLVideoElement* element)
{
    m_pictureInPictureElement = makeWeakPtr(element);
}
#endif

TextManipulationController& Document::textManipulationController()
{
    if (!m_textManipulationController)
        m_textManipulationController = makeUnique<TextManipulationController>(*this);
    return *m_textManipulationController;
}

LazyLoadImageObserver& Document::lazyLoadImageObserver()
{
    if (!m_lazyLoadImageObserver)
        m_lazyLoadImageObserver = LazyLoadImageObserver::create();
    return *m_lazyLoadImageObserver;
}

void Document::prepareCanvasesForDisplayIfNeeded()
{
    // Some canvas contexts need to do work when rendering has finished but
    // before their content is composited.
    for (auto* canvas : copyToVector(m_canvasesNeedingDisplayPreparation)) {
        // However, if they are not in the document body, then they won't
        // be composited and thus don't need preparation. Unfortunately they
        // can't tell at the time they were added to the list, since they
        // could be inserted or removed from the document body afterwards.
        if (!canvas->isInTreeScope())
            continue;

        auto protectedCanvas = makeRef(*canvas);
        protectedCanvas->prepareForDisplay();
    }
    m_canvasesNeedingDisplayPreparation.clear();
}

void Document::clearCanvasPreparation(HTMLCanvasElement* canvas)
{
    m_canvasesNeedingDisplayPreparation.remove(canvas);
}

void Document::canvasChanged(CanvasBase& canvasBase, const FloatRect&)
{
    if (is<HTMLCanvasElement>(canvasBase)) {
        auto* canvas = downcast<HTMLCanvasElement>(&canvasBase);
        if (canvas->needsPreparationForDisplay())
            m_canvasesNeedingDisplayPreparation.add(canvas);
    }
}

void Document::canvasDestroyed(CanvasBase& canvasBase)
{
    if (is<HTMLCanvasElement>(canvasBase)) {
        auto* canvas = downcast<HTMLCanvasElement>(&canvasBase);
        m_canvasesNeedingDisplayPreparation.remove(canvas);
    }
}

} // namespace WebCore

#undef RELEASE_LOG_IF_ALLOWED
#undef RELEASE_LOG_ERROR_IF_ALLOWED
