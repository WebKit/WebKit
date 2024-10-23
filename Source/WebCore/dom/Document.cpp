/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004-2024 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2008-2014 Google Inc. All rights reserved.
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
#include "AnimationTimelinesController.h"
#include "ApplicationManifest.h"
#include "Attr.h"
#include "BeforeUnloadEvent.h"
#include "CDATASection.h"
#include "CSSAnimation.h"
#include "CSSFontSelector.h"
#include "CSSParser.h"
#include "CSSPropertyNames.h"
#include "CSSStyleDeclaration.h"
#include "CSSStyleSheet.h"
#include "CSSViewTransitionRule.h"
#include "CachedCSSStyleSheet.h"
#include "CachedFontLoadRequest.h"
#include "CachedFrame.h"
#include "CachedResourceLoader.h"
#include "CanvasRenderingContext2D.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "Comment.h"
#include "CommonAtomStrings.h"
#include "CommonVM.h"
#include "ComposedTreeIterator.h"
#include "CompositionEvent.h"
#include "ConstantPropertyMap.h"
#include "ContentSecurityPolicy.h"
#include "ContentVisibilityDocumentState.h"
#include "ContentfulPaintChecker.h"
#include "CookieJar.h"
#include "CryptoClient.h"
#include "CustomEffect.h"
#include "CustomElementReactionQueue.h"
#include "CustomElementRegistry.h"
#include "CustomEvent.h"
#include "DOMAudioSession.h"
#include "DOMCSSPaintWorklet.h"
#include "DOMImplementation.h"
#include "DateComponents.h"
#include "DebugPageOverlays.h"
#include "DeprecatedGlobalSettings.h"
#include "DocumentFontLoader.h"
#include "DocumentInlines.h"
#include "DocumentLoader.h"
#include "DocumentMarkerController.h"
#include "DocumentSharedObjectPool.h"
#include "DocumentTimeline.h"
#include "DocumentType.h"
#include "DragEvent.h"
#include "Editing.h"
#include "Editor.h"
#include "ElementAncestorIteratorInlines.h"
#include "ElementChildIteratorInlines.h"
#include "ElementIterator.h"
#include "ElementRareData.h"
#include "ElementTraversal.h"
#include "EmptyHTMLCollection.h"
#include "EventHandler.h"
#include "ExtensionStyleSheets.h"
#include "FocusController.h"
#include "FocusEvent.h"
#include "FocusOptions.h"
#include "FontFaceSet.h"
#include "FormController.h"
#include "FrameLoader.h"
#include "FullscreenManager.h"
#include "GCReachableRef.h"
#include "GPUCanvasContext.h"
#include "GenericCachedHTMLCollection.h"
#include "GraphicsTypes.h"
#include "HTMLAllCollection.h"
#include "HTMLAnchorElement.h"
#include "HTMLAttachmentElement.h"
#include "HTMLBaseElement.h"
#include "HTMLBodyElement.h"
#include "HTMLCanvasElement.h"
#include "HTMLCollectionInlines.h"
#include "HTMLConstructionSite.h"
#include "HTMLDialogElement.h"
#include "HTMLDocument.h"
#include "HTMLDocumentParserFastPath.h"
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
#include "HTMLMaybeFormAssociatedCustomElement.h"
#include "HTMLMediaElement.h"
#include "HTMLMetaElement.h"
#include "HTMLNameCollection.h"
#include "HTMLPictureElement.h"
#include "HTMLPlugInElement.h"
#include "HTMLScriptElement.h"
#include "HTMLStyleElement.h"
#include "HTMLTitleElement.h"
#include "HTMLUnknownElement.h"
#include "HTMLVideoElement.h"
#include "HTTPHeaderNames.h"
#include "HTTPParsers.h"
#include "HashChangeEvent.h"
#include "HighlightRegistry.h"
#include "History.h"
#include "HitTestResult.h"
#include "IDBConnectionProxy.h"
#include "IDBOpenDBRequest.h"
#include "IdleCallbackController.h"
#include "ImageBitmapRenderingContext.h"
#include "ImageLoader.h"
#include "ImageOverlayController.h"
#include "InspectorInstrumentation.h"
#include "IntersectionObserver.h"
#include "JSCustomElementInterface.h"
#include "JSDOMWindowCustom.h"
#include "JSLazyEventListener.h"
#include "JSViewTransitionUpdateCallback.h"
#include "KeyboardEvent.h"
#include "KeyframeEffect.h"
#include "LayoutDisallowedScope.h"
#include "LazyLoadImageObserver.h"
#include "LegacySchemeRegistry.h"
#include "LoaderStrategy.h"
#include "LocalDOMWindow.h"
#include "LocalFrame.h"
#include "LocalFrameLoaderClient.h"
#include "LocalFrameView.h"
#include "Logging.h"
#include "MediaCanStartListener.h"
#include "MediaProducer.h"
#include "MediaQueryList.h"
#include "MediaQueryMatcher.h"
#include "MediaSession.h"
#include "MediaStream.h"
#include "MessageEvent.h"
#include "MouseEventWithHitTestResults.h"
#include "MutationEvent.h"
#include "NameNodeList.h"
#include "NavigationActivation.h"
#include "NavigationDisabler.h"
#include "NavigationScheduler.h"
#include "Navigator.h"
#include "NavigatorMediaSession.h"
#include "NestingLevelIncrementer.h"
#include "NodeIterator.h"
#include "NodeRareData.h"
#include "NodeWithIndex.h"
#include "NoiseInjectionPolicy.h"
#include "NotificationController.h"
#include "OpportunisticTaskScheduler.h"
#include "OrientationNotifier.h"
#include "PageConsoleClient.h"
#include "PageGroup.h"
#include "PageRevealEvent.h"
#include "PageSwapEvent.h"
#include "PageTransitionEvent.h"
#include "PaintWorkletGlobalScope.h"
#include "Performance.h"
#include "PerformanceNavigationTiming.h"
#include "PingLoader.h"
#include "PlatformLocale.h"
#include "PlatformMediaSessionManager.h"
#include "PlatformScreen.h"
#include "PlatformStrategies.h"
#include "PlugInsResources.h"
#include "PluginDocument.h"
#include "PointerCaptureController.h"
#include "PointerEvent.h"
#include "PointerLockController.h"
#include "PolicyChecker.h"
#include "PopStateEvent.h"
#include "Position.h"
#include "ProcessingInstruction.h"
#include "PseudoClassChangeInvalidation.h"
#include "PublicSuffixStore.h"
#include "Quirks.h"
#include "RTCNetworkManager.h"
#include "Range.h"
#include "RealtimeMediaSourceCenter.h"
#include "RenderBoxInlines.h"
#include "RenderChildIterator.h"
#include "RenderInline.h"
#include "RenderLayerCompositor.h"
#include "RenderLayoutState.h"
#include "RenderLineBreak.h"
#include "RenderTreeUpdater.h"
#include "RenderView.h"
#include "RenderWidget.h"
#include "ReportingScope.h"
#include "RequestAnimationFrameCallback.h"
#include "ResizeObserver.h"
#include "ResizeObserverEntry.h"
#include "ResolvedStyle.h"
#include "ResourceLoadObserver.h"
#include "SVGDocumentExtensions.h"
#include "SVGElementFactory.h"
#include "SVGElementTypeHelpers.h"
#include "SVGNames.h"
#include "SVGSVGElement.h"
#include "SVGTitleElement.h"
#include "SVGUseElement.h"
#include "SWClientConnection.h"
#include "ScopedEventQueue.h"
#include "ScriptController.h"
#include "ScriptDisallowedScope.h"
#include "ScriptModuleLoader.h"
#include "ScriptRunner.h"
#include "ScriptSourceCode.h"
#include "ScriptTelemetryCategory.h"
#include "ScriptedAnimationController.h"
#include "ScrollAnimator.h"
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
#include "SleepDisabler.h"
#include "SocketProvider.h"
#include "SpeechRecognition.h"
#include "StartViewTransitionOptions.h"
#include "StaticNodeList.h"
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
#include "TextResourceDecoder.h"
#include "TouchAction.h"
#include "TransformSource.h"
#include "TreeWalker.h"
#include "TrustedType.h"
#include "TypedElementDescendantIteratorInlines.h"
#include "UndoManager.h"
#include "UserGestureIndicator.h"
#include "UserMediaController.h"
#include "ValidationMessage.h"
#include "ValidationMessageClient.h"
#include "ViewTransition.h"
#include "ViewTransitionUpdateCallback.h"
#include "ViolationReportType.h"
#include "VisibilityChangeClient.h"
#include "VisibilityState.h"
#include "VisitedLinkState.h"
#include "VisualViewport.h"
#include "WakeLockManager.h"
#include "WebAnimation.h"
#include "WebAnimationUtilities.h"
#include "WebRTCProvider.h"
#include "WheelEvent.h"
#include "WindowEventLoop.h"
#include "WindowFeatures.h"
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
#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/HexNumber.h>
#include <wtf/Language.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RuntimeApplicationChecks.h>
#include <wtf/SetForScope.h>
#include <wtf/SystemTracing.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/UUID.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuffer.h>
#include <wtf/text/TextStream.h>

#if ENABLE(APP_HIGHLIGHTS)
#include "AppHighlightStorage.h"
#endif

#if ENABLE(DEVICE_ORIENTATION)
#include "DeviceMotionEvent.h"
#include "DeviceOrientationAndMotionAccessController.h"
#include "DeviceOrientationEvent.h"
#endif

#if ENABLE(CONTENT_CHANGE_OBSERVER)
#include "ContentChangeObserver.h"
#include "DOMTimerHoldingTank.h"
#endif

#if PLATFORM(IOS_FAMILY)
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

#if ENABLE(MEDIA_STREAM)
#include "MediaStreamTrack.h"
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
#include "WebGL2RenderingContext.h"
#endif

#if ENABLE(PICTURE_IN_PICTURE_API)
#include "HTMLVideoElement.h"
#endif

#if ENABLE(THREADED_ANIMATION_RESOLUTION)
#include "AcceleratedTimeline.h"
#endif

#define DOCUMENT_RELEASE_LOG(channel, fmt, ...) RELEASE_LOG(channel, "%p - [pageID=%" PRIu64 ", frameID=%" PRIu64 ", isMainFrame=%d] Document::" fmt, this, pageID() ? pageID()->toUInt64() : 0, frameID() ? frameID()->object().toUInt64() : 0, this == &topDocument(), ##__VA_ARGS__)
#define DOCUMENT_RELEASE_LOG_ERROR(channel, fmt, ...) RELEASE_LOG_ERROR(channel, "%p - [pageID=%" PRIu64 ", frameID=%" PRIu64 ", isMainFrame=%d] Document::" fmt, this, pageID() ? pageID()->toUInt64() : 0, frameID() ? frameID()->object().toUInt64() : 0, this == &topDocument(), ##__VA_ARGS__)

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(Document);
WTF_MAKE_TZONE_ALLOCATED_IMPL(DocumentParserYieldToken);

using namespace HTMLNames;
using namespace WTF::Unicode;

static const unsigned cMaxWriteRecursionDepth = 21;
bool Document::hasEverCreatedAnAXObjectCache = false;
static const Seconds maxIntervalForUserGestureForwardingAfterMediaFinishesPlaying { 1_s };

// Defined here to avoid including GCReachableRef.h in Document.h
struct Document::PendingScrollEventTargetList {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(PendingScrollEventTargetList);

public:
    Vector<GCReachableRef<ContainerNode>> targets;
};

static const Seconds intersectionObserversInitialUpdateDelay { 2000_ms };

static void CallbackForContainIntrinsicSize(const Vector<Ref<ResizeObserverEntry>>& entries, ResizeObserver& observer)
{
    for (auto& entry : entries) {
        if (RefPtr target = entry->target()) {
            if (!target->isConnected()) {
                observer.unobserve(*target);
                target->clearLastRememberedLogicalWidth();
                target->clearLastRememberedLogicalHeight();
                continue;
            }

            CheckedPtr box = target->renderBox();
            if (!box) {
                observer.unobserve(*target);
                continue;
            }
            ASSERT(!box->isSkippedContentRoot());
            ASSERT(box->style().hasAutoLengthContainIntrinsicSize());

            auto contentBoxSize = entry->contentBoxSize().at(0);
            if (box->style().containIntrinsicLogicalWidthHasAuto())
                target->setLastRememberedLogicalWidth(LayoutUnit(contentBoxSize->inlineSize()));
            if (box->style().containIntrinsicLogicalHeightHasAuto())
                target->setLastRememberedLogicalHeight(LayoutUnit(contentBoxSize->blockSize()));
        }
    }
}

// https://www.w3.org/TR/xml/#NT-NameStartChar
// NameStartChar       ::=       ":" | [A-Z] | "_" | [a-z] | [#xC0-#xD6] | [#xD8-#xF6] | [#xF8-#x2FF] | [#x370-#x37D] | [#x37F-#x1FFF] | [#x200C-#x200D] | [#x2070-#x218F] | [#x2C00-#x2FEF] | [#x3001-#xD7FF] | [#xF900-#xFDCF] | [#xFDF0-#xFFFD] | [#x10000-#xEFFFF]
static inline bool isValidNameStart(char32_t c)
{
    return c == ':' || (c >= 'A' && c <= 'Z') || c == '_' || (c >= 'a' && c <= 'z') || (c >= 0x00C0 && c <= 0x00D6)
        || (c >= 0x00D8 && c <= 0x00F6) || (c >= 0x00F8 && c <= 0x02FF) || (c >= 0x0370 && c <= 0x037D) || (c >= 0x037F && c <= 0x1FFF)
        || (c >= 0x200C && c <= 0x200D) || (c >= 0x2070 && c <= 0x218F) || (c >= 0x2C00 && c <= 0x2FeF) || (c >= 0x3001 && c <= 0xD7FF)
        || (c >= 0xF900 && c <= 0xFDCF) || (c >= 0xFDF0 && c <= 0xFFFD) || (c >= 0x10000 && c <= 0xEFFFF);
}

// https://www.w3.org/TR/xml/#NT-NameChar
// NameChar       ::=       NameStartChar | "-" | "." | [0-9] | #xB7 | [#x0300-#x036F] | [#x203F-#x2040]
static inline bool isValidNamePart(char32_t c)
{
    return isValidNameStart(c) || c == '-' || c == '.' || (c >= '0' && c <= '9') || c == 0x00B7
        || (c >= 0x0300 && c <= 0x036F) || (c >= 0x203F && c <= 0x2040);
}

static Widget* widgetForElement(Element* focusedElement)
{
    auto* renderer = focusedElement ? dynamicDowncast<RenderWidget>(focusedElement->renderer()) : nullptr;
    return renderer ? renderer->widget() : nullptr;
}

static bool acceptsEditingFocus(const Element& element)
{
    ASSERT(element.hasEditableStyle());

    RefPtr root = element.rootEditableElement();
    RefPtr frame = element.document().frame();
    if (!frame || !root)
        return false;

    return frame->editor().shouldBeginEditing(makeRangeSelectingNodeContents(*root));
}

static bool canAccessAncestor(const SecurityOrigin& activeSecurityOrigin, Frame* targetFrame)
{
    // targetFrame can be 0 when we're trying to navigate a top-level frame
    // that has a 0 opener.
    if (!targetFrame)
        return false;

    const bool isLocalActiveOrigin = activeSecurityOrigin.isLocal();
    for (RefPtr<Frame> ancestorFrame = targetFrame; ancestorFrame; ancestorFrame = ancestorFrame->tree().parent()) {
        RefPtr localAncestor = dynamicDowncast<LocalFrame>(ancestorFrame.get());
        if (!localAncestor)
            continue;
        RefPtr ancestorDocument = localAncestor->document();
        // FIXME: Should be an ASSERT? Frames should alway have documents.
        if (!ancestorDocument)
            return true;

        const SecurityOrigin& ancestorSecurityOrigin = ancestorDocument->securityOrigin();
        if (activeSecurityOrigin.isSameOriginDomain(ancestorSecurityOrigin))
            return true;

        // Allow file URL descendant navigation even when allowFileAccessFromFileURLs is false.
        // FIXME: It's a bit strange to special-case local origins here. Should we be doing
        // something more general instead?
        if (isLocalActiveOrigin && ancestorSecurityOrigin.isLocal())
            return true;
    }

    return false;
}

static void printNavigationErrorMessage(Document& document, Frame& frame, const URL& activeURL, ASCIILiteral reason)
{
    frame.documentURLForConsoleLog([window = document.protectedWindow(), activeURL, reason] (const URL& documentURL) {
        window->printErrorMessage(makeString("Unsafe JavaScript attempt to initiate navigation for frame with URL '"_s, documentURL.string(), "' from frame with URL '"_s, activeURL.string(), "'. "_s, reason, '\n'));
    });
}

uint64_t Document::s_globalTreeVersion = 0;

static const void* sharedLoggerOwner()
{
    static uint64_t owner = cryptographicallyRandomNumber<uint32_t>();
    return reinterpret_cast<const void*>(owner);
}

static Logger*& staticSharedLogger()
{
    static Logger* logger;
    return logger;
}

const Logger& Document::sharedLogger()
{
    if (!staticSharedLogger()) {
        staticSharedLogger() = &Logger::create(sharedLoggerOwner()).leakRef();
        configureSharedLogger();
    }

    return *staticSharedLogger();
}

void Document::configureSharedLogger()
{
    auto logger = staticSharedLogger();
    if (!logger)
        return;

    bool alwaysOnLoggingAllowed = !allDocumentsMap().isEmpty() && WTF::allOf(allDocumentsMap().values(), [](auto& document) {
        RefPtr page = document->page();
        return !page || page->sessionID().isAlwaysOnLoggingAllowed();
    });
    logger->setEnabled(sharedLoggerOwner(), alwaysOnLoggingAllowed);
}

void Document::addToDocumentsMap()
{
    auto addResult = allDocumentsMap().add(identifier(), *this);
    ASSERT_UNUSED(addResult, addResult.isNewEntry);

    configureSharedLogger();
}

void Document::removeFromDocumentsMap()
{
    ASSERT(allDocumentsMap().contains(identifier()));
    allDocumentsMap().remove(identifier());
    configureSharedLogger();
}

auto Document::allDocumentsMap() -> DocumentsMap&
{
    static MainThreadNeverDestroyed<DocumentsMap> documents;
    return documents;
}

auto Document::allDocuments() -> DocumentsMap::ValuesIteratorRange
{
    return allDocumentsMap().values();
}

static inline IntDegrees currentOrientation(LocalFrame* frame)
{
#if ENABLE(ORIENTATION_EVENTS)
    if (frame)
        return frame->orientation();
#else
    UNUSED_PARAM(frame);
#endif
    return 0;
}

Document::Document(LocalFrame* frame, const Settings& settings, const URL& url, DocumentClasses documentClasses, OptionSet<ConstructionFlag> constructionFlags, std::optional<ScriptExecutionContextIdentifier> identifier)
    : ContainerNode(*this, DOCUMENT_NODE)
    , TreeScope(*this)
    , ScriptExecutionContext(Type::Document, identifier)
    , FrameDestructionObserver(frame)
    , m_settings(settings)
    , m_parserContentPolicy(DefaultParserContentPolicy)
    , m_creationURL(url)
    , m_domTreeVersion(++s_globalTreeVersion)
    , m_styleScope(makeUniqueRef<Style::Scope>(*this))
    , m_styleRecalcTimer([this] { updateStyleIfNeeded(); })
#if !LOG_DISABLED
    , m_documentCreationTime(MonotonicTime::now())
#endif
#if ENABLE(XSLT)
    , m_applyPendingXSLTransformsTimer(*this, &Document::applyPendingXSLTransformsTimerFired)
#endif
    , m_xmlVersion("1.0"_s)
    , m_intersectionObserversInitialUpdateTimer(*this, &Document::intersectionObserversInitialUpdateTimerFired)
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
    , m_didAssociateFormControlsTimer(*this, &Document::didAssociateFormControlsTimerFired)
    , m_cookieCacheExpiryTimer(*this, &Document::invalidateDOMCookieCache)
    , m_socketProvider(page() ? &page()->socketProvider() : nullptr)
    , m_selection(makeUniqueRef<FrameSelection>(this))
    , m_fragmentDirectiveForBindings(FragmentDirective::create())
    , m_documentClasses(documentClasses)
    , m_latestFocusTrigger { FocusTrigger::Other }
#if ENABLE(DOM_AUDIO_SESSION)
    , m_audioSessionType { DOMAudioSession::Type::Auto }
#endif
    , m_isSynthesized(constructionFlags.contains(ConstructionFlag::Synthesized))
    , m_isNonRenderedPlaceholder(constructionFlags.contains(ConstructionFlag::NonRenderedPlaceholder))
    , m_frameIdentifier(frame ? std::optional(frame->frameID()) : std::nullopt)
{
    setEventTargetFlag(EventTargetFlag::IsConnected);
    addToDocumentsMap();

    // We depend on the url getting immediately set in subframes, but we
    // also depend on the url NOT getting immediately set in opened windows.
    // See fast/dom/early-frame-url.html
    // and fast/dom/location-new-window-no-crash.html, respectively.
    // FIXME: Can/should we unify this behavior?
    if ((frame && frame->ownerElement()) || !url.isEmpty())
        setURL(url);

    initSecurityContext();

    InspectorInstrumentation::addEventListenersToNode(*this);
    setStorageBlockingPolicy(m_settings->storageBlockingPolicy());

    ASSERT(!m_quirks);
    ASSERT(!m_cachedResourceLoader);
    ASSERT(!m_extensionStyleSheets);
    ASSERT(!m_visitedLinkState);
    ASSERT(!m_markers);
    ASSERT(!m_scriptRunner);
    ASSERT(!m_moduleLoader);
#if ENABLE(FULLSCREEN_API)
    ASSERT(!m_fullscreenManager);
#endif
    ASSERT(!m_fontSelector);
    ASSERT(!m_fontLoader);
    ASSERT(!m_undoManager);
    ASSERT(!m_editor);
    ASSERT(!m_reportingScope);
    ASSERT(!hasEmptySecurityOriginPolicyAndContentSecurityPolicy() || !hasInitializedSecurityOriginPolicyOrContentSecurityPolicy());
    m_constructionDidFinish = true;

}

void Document::createNewIdentifier()
{
    removeFromDocumentsMap();
    regenerateIdentifier();
    addToDocumentsMap();
}

Ref<Document> Document::create(Document& contextDocument)
{
    auto document = adoptRef(*new Document(nullptr, contextDocument.m_settings, URL(), { }));
    document->addToContextsMap();
    document->setContextDocument(contextDocument);
    document->setSecurityOriginPolicy(contextDocument.securityOriginPolicy());
    return document;
}

Ref<Document> Document::createNonRenderedPlaceholder(LocalFrame& frame, const URL& url)
{
    auto document = adoptRef(*new Document(&frame, frame.settings(), url, { }, { ConstructionFlag::NonRenderedPlaceholder }));
    document->addToContextsMap();
    return document;
}

Document::~Document()
{
    ASSERT(activeDOMObjectsAreStopped());

    if (m_logger)
        m_logger->removeObserver(*this);

    if (m_intersectionObserverData) {
        for (const auto& observer : m_intersectionObserverData->observers) {
            if (observer)
                observer->rootDestroyed();
        }
        m_intersectionObserverData->observers.clear();
        // Document cannot be a target.
        ASSERT(m_intersectionObserverData->registrations.isEmpty());
    }

    removeFromDocumentsMap();

    // We need to remove from the contexts map very early in the destructor so that calling postTask() on this Document from another thread is safe.
    removeFromContextsMap();

    ASSERT(!renderView());
    ASSERT(m_backForwardCacheState != InBackForwardCache);
    ASSERT(!m_parentTreeScope);
    ASSERT(!m_disabledFieldsetElementsCount);
    ASSERT(m_ranges.isEmpty());
    ASSERT(m_inDocumentShadowRoots.isEmptyIgnoringNullReferences());

#if ENABLE(DEVICE_ORIENTATION) && PLATFORM(IOS_FAMILY)
    m_deviceMotionClient->deviceMotionControllerDestroyed();
    m_deviceOrientationClient->deviceOrientationControllerDestroyed();
#endif

    if (m_templateDocument)
        m_templateDocument->setTemplateDocumentHost(nullptr); // balanced in templateDocument().

    // FIXME: Should we reset m_domWindow when we detach from the Frame?
    if (RefPtr window = m_domWindow)
        window->resetUnlessSuspendedForDocumentSuspension();

    m_scriptRunner = nullptr;
    m_moduleLoader = nullptr;

    removeAllEventListeners();

    if (RefPtr eventLoop = m_eventLoop)
        eventLoop->removeAssociatedContext(*this);

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

    if (CheckedPtr extensionStyleSheets = m_extensionStyleSheets.get())
        extensionStyleSheets->detachFromDocument();

    styleScope().clearResolver(); // We need to destroy CSSFontSelector before destroying m_cachedResourceLoader.
    if (auto* fontLoader = m_fontLoader.get())
        fontLoader->stopLoadingAndClearFonts();

    if (RefPtr fontSelector = m_fontSelector)
        fontSelector->unregisterForInvalidationCallbacks(*this);

    // It's possible for multiple Documents to end up referencing the same CachedResourceLoader (e.g., SVGImages
    // load the initial empty document and the SVGDocument with the same DocumentLoader).
    if (m_cachedResourceLoader && m_cachedResourceLoader->document() == this)
        protectedCachedResourceLoader()->setDocument(nullptr);

    // We must call clearRareData() here since a Document class inherits TreeScope
    // as well as Node. See a comment on TreeScope.h for the reason.
    if (hasRareData())
        clearRareData();

    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(m_listsInvalidatedAtDocument.isEmpty());
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(m_collectionsInvalidatedAtDocument.isEmpty());

    for (unsigned count : m_nodeListAndCollectionCounts)
        ASSERT_UNUSED(count, !count);

    // End the loading signpost here in case loadEventEnd never fired.
    WTFEndSignpost(this, NavigationAndPaintTiming);
}

void Document::removedLastRef()
{
    ScriptDisallowedScope::InMainThread scriptDisallowedScope;
    ASSERT(!deletionHasBegun());
    m_wasRemovedLastRefCalled = true;
    if (m_referencingNodeCount) {
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
        if (CheckedPtr fullscreenManager = m_fullscreenManager.get())
            m_fullscreenManager->clear();
#endif
        m_associatedFormControls.clear();
        m_pendingRenderTreeUpdate = { };

        if (auto* fontLoader = m_fontLoader.get())
            fontLoader->stopLoadingAndClearFonts();

        detachParser();

        RELEASE_ASSERT(m_selection->isNone());

        // removeDetachedChildren() doesn't always unregister IDs,
        // so tear down scope information up front to avoid having
        // stale references in the map.

        destroyTreeScopeData();
        removeDetachedChildren();
        RELEASE_ASSERT(m_topLayerElements.isEmpty());
        m_formController = nullptr;

        if (CheckedPtr markers = m_markers.get())
            markers->detach();

        m_cssCanvasElements.clear();

        commonTeardown();

        // Node::removedLastRef doesn't set refCount() to zero because it's not observable.
        // But we need to remember that our refCount reached zero in subsequent calls to decrementReferencingNodeCount().
        m_refCountAndParentBit = 0;

        decrementReferencingNodeCount();
    } else {
        commonTeardown();
        setStateFlag(StateFlag::HasStartedDeletion);
        delete this;
    }
}

void Document::commonTeardown()
{
    stopActiveDOMObjects();

#if ENABLE(FULLSCREEN_API)
    if (CheckedPtr fullscreenManager = m_fullscreenManager.get())
        fullscreenManager->emptyEventQueue();
#endif

    if (CheckedPtr svgExtensions = svgExtensionsIfExists())
        svgExtensions->pauseAnimations();

    clearScriptedAnimationController();

    m_documentFragmentForInnerOuterHTML = nullptr;

    auto intersectionObservers = m_intersectionObservers;
    for (auto& weakIntersectionObserver : intersectionObservers) {
        if (RefPtr intersectionObserver = weakIntersectionObserver.get())
            intersectionObserver->disconnect();
    }

    auto resizeObservers = m_resizeObservers;
    for (auto& weakResizeObserver : resizeObservers) {
        if (RefPtr resizeObserver = weakResizeObserver.get())
            resizeObserver->disconnect();
    }

    scriptRunner().clearPendingScripts();

    if (RefPtr highlightRegistry = m_highlightRegistry)
        highlightRegistry->clear();
    if (RefPtr fragmentHighlightRegistry = m_fragmentHighlightRegistry)
        fragmentHighlightRegistry->clear();
#if ENABLE(APP_HIGHLIGHTS)
    if (RefPtr appHighlightRegistry = m_appHighlightRegistry)
        appHighlightRegistry->clear();
#endif
    m_pendingScrollEventTargetList = nullptr;

    if (m_timelinesController)
        m_timelinesController->detachFromDocument();

    m_timeline = nullptr;
    m_associatedFormControls.clear();
    m_didAssociateFormControlsTimer.stop();

#if ENABLE(MEDIA_STREAM)
    auto captureSources = std::exchange(m_captureSources, { });
    for (auto& source : captureSources)
        source->endImmediatly();
#endif

#if ENABLE(WEB_RTC)
    if (RefPtr rtcNetworkManager = std::exchange(m_rtcNetworkManager, nullptr))
        rtcNetworkManager->close();
#endif
}

Quirks& Document::ensureQuirks()
{
    ASSERT(m_constructionDidFinish);
    ASSERT(!m_quirks);
    m_quirks = makeUnique<Quirks>(*this);
    return *m_quirks;
}

CachedResourceLoader& Document::ensureCachedResourceLoader()
{
    ASSERT(m_constructionDidFinish);
    ASSERT(!m_cachedResourceLoader);
    m_cachedResourceLoader = [&]() -> Ref<CachedResourceLoader> {
        if (RefPtr frame = this->frame()) {
            if (RefPtr loader = frame->loader().activeDocumentLoader())
                return loader->cachedResourceLoader();
        }
        return CachedResourceLoader::create(nullptr);
    }();
    m_cachedResourceLoader->setDocument(this);
    return *m_cachedResourceLoader;
}

ExtensionStyleSheets& Document::ensureExtensionStyleSheets()
{
    ASSERT(m_constructionDidFinish);
    ASSERT(!m_extensionStyleSheets);
    m_extensionStyleSheets = makeUnique<ExtensionStyleSheets>(*this);
    return *m_extensionStyleSheets;
}

DocumentMarkerController& Document::ensureMarkers()
{
    ASSERT(m_constructionDidFinish);
    ASSERT(!m_markers);
    m_markers = makeUnique<DocumentMarkerController>(*this);
    return *m_markers;
}

VisitedLinkState& Document::ensureVisitedLinkState()
{
    ASSERT(m_constructionDidFinish);
    ASSERT(!m_visitedLinkState);
    m_visitedLinkState = makeUnique<VisitedLinkState>(*this);
    return *m_visitedLinkState;
}

#if ENABLE(FULLSCREEN_API)
FullscreenManager& Document::ensureFullscreenManager()
{
    ASSERT(m_constructionDidFinish);
    ASSERT(!m_fullscreenManager);
    m_fullscreenManager = makeUnique<FullscreenManager>(*this);
    return *m_fullscreenManager;
}
#endif

Ref<SecurityOrigin> Document::protectedTopOrigin() const
{
    return topOrigin();
}

SecurityOrigin& Document::topOrigin() const
{
    // Keep exact pre-site-isolation behavior to avoid risking changing behavior when site isolation is not enabled.
    if (!settings().siteIsolationEnabled())
        return topDocument().securityOrigin();

    RefPtr frame = this->frame();
    if (RefPtr localMainFrame = frame ? dynamicDowncast<LocalFrame>(frame->mainFrame()) : nullptr) {
        if (RefPtr mainFrameDocument = localMainFrame->document())
            return mainFrameDocument->securityOrigin();
    }
    if (RefPtr page = this->page())
        return page->mainFrameOrigin();
    return SecurityOrigin::opaqueOrigin();
}

inline DocumentFontLoader& Document::fontLoader()
{
    ASSERT(m_constructionDidFinish);
    if (!m_fontLoader)
        return ensureFontLoader();
    return *m_fontLoader;
}

DocumentFontLoader& Document::ensureFontLoader()
{
    ASSERT(m_constructionDidFinish);
    ASSERT(!m_fontLoader);
    m_fontLoader = makeUnique<DocumentFontLoader>(*this);
    return *m_fontLoader;
}

CSSFontSelector* Document::cssFontSelector()
{
    return &fontSelector();
}

CSSFontSelector& Document::ensureFontSelector()
{
    ASSERT(m_constructionDidFinish);
    ASSERT(!m_fontSelector);
    m_fontSelector = CSSFontSelector::create(*this);
    m_fontSelector->registerForInvalidationCallbacks(*this);
    return *m_fontSelector;
}

UndoManager& Document::ensureUndoManager()
{
    ASSERT(m_constructionDidFinish);
    ASSERT(!m_undoManager);
    m_undoManager = UndoManager::create(*this);
    return *m_undoManager;
}

Editor& Document::editor()
{
    if (!m_editor)
        return ensureEditor();
    return *m_editor;
}

const Editor& Document::editor() const
{
    if (!m_editor)
        return const_cast<Document&>(*this).ensureEditor();
    return *m_editor;
}

CheckedRef<Editor> Document::checkedEditor()
{
    return editor();
}

CheckedRef<const Editor> Document::checkedEditor() const
{
    return editor();
}

Editor& Document::ensureEditor()
{
    ASSERT(m_constructionDidFinish);
    ASSERT(!m_editor);
    m_editor = makeUnique<Editor>(*this);
    return *m_editor;
}

ReportingScope& Document::ensureReportingScope()
{
    ASSERT(m_constructionDidFinish);
    ASSERT(!m_reportingScope);
    m_reportingScope = ReportingScope::create(*this);
    return *m_reportingScope;
}

void Document::setMarkupUnsafe(const String& markup, OptionSet<ParserContentPolicy> parserContentPolicy)
{
    ASSERT(!hasChildNodes());
    auto policy = OptionSet<ParserContentPolicy> { ParserContentPolicy::AllowScriptingContent } | parserContentPolicy;
    setParserContentPolicy(policy);
    if (this->contentType() == textHTMLContentTypeAtom()) {
        auto html = HTMLHtmlElement::create(*this);
        appendChild(html);
        auto body = HTMLBodyElement::create(*this);
        html->appendChild(body);
        body->beginParsingChildren();
        if (LIKELY(tryFastParsingHTMLFragment(StringView { markup }.substring(markup.find(isNotASCIIWhitespace<UChar>)), *this, body, body, policy))) {
            body->finishParsingChildren();
            auto head = HTMLHeadElement::create(*this);
            html->insertBefore(head, body.ptr());
            return;
        }
        html->remove();
    }
    open();
    protectedParser()->appendSynchronously(markup.impl());
    close();
}

ExceptionOr<Ref<Document>> Document::parseHTMLUnsafe(Document& context, std::variant<RefPtr<TrustedHTML>, String>&& html)
{
    auto stringValueHolder = trustedTypeCompliantString(*context.scriptExecutionContext(), WTFMove(html), "Document parseHTMLUnsafe"_s);
    if (stringValueHolder.hasException())
        return stringValueHolder.releaseException();

    Ref document = HTMLDocument::create(nullptr, context.protectedSettings(), URL { });
    document->setMarkupUnsafe(stringValueHolder.releaseReturnValue(), { ParserContentPolicy::AllowDeclarativeShadowRoots });
    return { document };
}

Element* Document::elementForAccessKey(const String& key)
{
    if (key.isEmpty())
        return nullptr;
    if (!m_accessKeyCache)
        buildAccessKeyCache();
    return m_accessKeyCache->get(key).get();
}

void Document::buildAccessKeyCache()
{
    m_accessKeyCache = makeUnique<UncheckedKeyHashMap<String, WeakPtr<Element, WeakPtrImplWithEventTargetData>, ASCIICaseInsensitiveHash>>([this] {
        UncheckedKeyHashMap<String, WeakPtr<Element, WeakPtrImplWithEventTargetData>, ASCIICaseInsensitiveHash> map;
        for (auto& node : composedTreeDescendants(*this)) {
            auto element = dynamicDowncast<Element>(node);
            if (!element)
                continue;
            auto& key = element->attributeWithoutSynchronization(accesskeyAttr);
            if (key.isEmpty())
                continue;
            map.add(key, *element);
        }
        return map;
    }());
}

void Document::invalidateAccessKeyCacheSlowCase()
{
    m_accessKeyCache = nullptr;
}

struct QuerySelectorAllResults {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
public:
    static constexpr unsigned maxSize = 8;

    struct Entry {
        String selector;
        Ref<NodeList> nodeList;
        AtomString classNameToMatch;
    };

    Vector<Entry, maxSize> entries;
};

RefPtr<NodeList> Document::resultForSelectorAll(ContainerNode& context, const String& selectorString)
{
    ASSERT(context.hasValidQuerySelectorAllResults() == m_querySelectorAllResults.contains(context));
    if (!context.hasValidQuerySelectorAllResults())
        return nullptr;
    auto* results = m_querySelectorAllResults.get(context);
    if (!results)
        return nullptr;
    for (auto& entry : results->entries) {
        if (entry.selector == selectorString)
            return entry.nodeList.copyRef();
    }
    return nullptr;
}

void Document::addResultForSelectorAll(ContainerNode& context, const String& selectorString, NodeList& nodeList, const AtomString& classNameToMatch)
{
    auto result = m_querySelectorAllResults.ensure(context, [&]() {
        context.setHasValidQuerySelectorAllResults(true);
        return makeUnique<QuerySelectorAllResults>();
    });
    auto& entries = result.iterator->value->entries;
    if (entries.size() >= QuerySelectorAllResults::maxSize)
        entries.remove(weakRandomNumber<uint32_t>() % entries.size());
    entries.append({ selectorString, nodeList, classNameToMatch });
}

void Document::invalidateQuerySelectorAllResults(Node& startingNode)
{
    if (m_querySelectorAllResults.isEmptyIgnoringNullReferences())
        return;
    for (RefPtr<Node> currentNode = &startingNode; currentNode; currentNode = currentNode->parentNode()) {
        if (!currentNode->hasValidQuerySelectorAllResults())
            continue;
        m_querySelectorAllResults.remove(*currentNode);
        currentNode->setHasValidQuerySelectorAllResults(false);
    }
}

void Document::invalidateQuerySelectorAllResultsForClassAttributeChange(Node& startingNode, const SpaceSplitString& oldClasses, const SpaceSplitString& newClasses)
{
    if (m_querySelectorAllResults.isEmptyIgnoringNullReferences())
        return;
    for (RefPtr<Node> currentNode = &startingNode; currentNode; currentNode = currentNode->parentNode()) {
        if (!currentNode->hasValidQuerySelectorAllResults())
            continue;
        auto it = m_querySelectorAllResults.find(*currentNode);
        ASSERT(it != m_querySelectorAllResults.end());
        if (it == m_querySelectorAllResults.end())
            continue;
        auto& entries = it->value->entries;
        unsigned index = 0;
        while (index < entries.size()) {
            auto& entry = entries[index];
            if (!entry.classNameToMatch.isNull() && oldClasses.contains(entry.classNameToMatch) != newClasses.contains(entry.classNameToMatch))
                entries.remove(index);
            else
                ++index;
        }
        if (!entries.size()) {
            m_querySelectorAllResults.remove(it);
            currentNode->setHasValidQuerySelectorAllResults(false);
        }
    }
}

void Document::clearQuerySelectorAllResults()
{
    m_querySelectorAllResults.clear();
}

ExceptionOr<SelectorQuery&> Document::selectorQueryForString(const String& selectorString)
{
    if (selectorString.isEmpty())
        return Exception { ExceptionCode::SyntaxError, makeString('\'', selectorString, "' is not a valid selector."_s) };

    auto* query = SelectorQueryCache::singleton().add(selectorString, *this);
    if (!query)
        return Exception { ExceptionCode::SyntaxError, makeString('\'', selectorString, "' is not a valid selector."_s) };

    return *query;
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

    if (inQuirksMode() != wasInQuirksMode) {
        // All user stylesheets have to reparse using the different mode.
        if (auto* extensionStyleSheets = extensionStyleSheetsIfExists()) {
            extensionStyleSheets->clearPageUserSheet();
            extensionStyleSheets->invalidateInjectedStyleSheetCache();
        }
    }

    if (CheckedPtr view = renderView())
        view->updateQuirksMode();

    invalidateCachedCSSParserContext();
}

String Document::compatMode() const
{
    return inQuirksMode() ? "BackCompat"_s : "CSS1Compat"_s;
}

const Color& Document::themeColor()
{
    if (!m_cachedThemeColor.isValid()) {
        if (!m_activeThemeColorMetaElement)
            m_activeThemeColorMetaElement = determineActiveThemeColorMetaElement();
        if (m_activeThemeColorMetaElement)
            m_cachedThemeColor = m_activeThemeColorMetaElement->contentColor();

        if (!m_cachedThemeColor.isValid())
            m_cachedThemeColor = m_applicationManifestThemeColor;
    }
    return m_cachedThemeColor;
}

Color Document::linkColor(const RenderStyle& style) const
{
    if (m_linkColor.isValid())
        return m_linkColor;
    return StyleColor::colorFromKeyword(CSSValueWebkitLink, styleColorOptions(&style));
}

Color Document::visitedLinkColor(const RenderStyle& style) const
{
    if (m_visitedLinkColor.isValid())
        return m_visitedLinkColor;
    return StyleColor::colorFromKeyword(CSSValueWebkitLink, styleColorOptions(&style) | StyleColorOptions::ForVisitedLink);
}

Color Document::activeLinkColor(const RenderStyle& style) const
{
    if (m_activeLinkColor.isValid())
        return m_activeLinkColor;
    return StyleColor::colorFromKeyword(CSSValueWebkitActivelink, styleColorOptions(&style));
}

void Document::resetLinkColor()
{
    m_linkColor = { };
}

void Document::resetVisitedLinkColor()
{
    m_visitedLinkColor = { };
}

void Document::resetActiveLinkColor()
{
    m_activeLinkColor = { };
}

DOMImplementation& Document::implementation()
{
    if (!m_implementation)
        m_implementation = makeUniqueWithoutRefCountedCheck<DOMImplementation>(*this);
    return *m_implementation;
}

DocumentType* Document::doctype() const
{
    for (Node* node = firstChild(); node; node = node->nextSibling()) {
        if (auto* documentType = dynamicDowncast<DocumentType>(node))
            return documentType;
    }
    return nullptr;
}

void Document::childrenChanged(const ChildChange& change)
{
    ContainerNode::childrenChanged(change);

    // FIXME: Chrome::didReceiveDocType() used to be called only when the doctype changed. We need to check the
    // impact of calling this systematically. If the overhead is negligible, we need to rename didReceiveDocType,
    // otherwise, we need to detect the doc type changes before updating the viewport.
    if (RefPtr page = this->page())
        page->chrome().didReceiveDocType(*frame());

    RefPtr newDocumentElement = childrenOfType<Element>(*this).first();
    if (newDocumentElement == m_documentElement)
        return;
    m_documentElement = WTFMove(newDocumentElement);
    setDocumentElementLanguage(m_documentElement ? m_documentElement->langFromAttribute() : nullAtom());
    auto* htmlDocumentElement = dynamicDowncast<HTMLElement>(m_documentElement.get());
    setDocumentElementTextDirection(htmlDocumentElement && htmlDocumentElement->usesEffectiveTextDirection()
        ? htmlDocumentElement->effectiveTextDirection() : TextDirection::LTR);
    // The root style used for media query matching depends on the document element.
    styleScope().clearResolver();
}

static ALWAYS_INLINE CustomElementNameValidationStatus validateCustomElementNameWithoutCheckingStandardElementNames(const AtomString&);
static ALWAYS_INLINE bool isStandardElementName(const AtomString& localName);

static ALWAYS_INLINE Ref<HTMLElement> createUpgradeCandidateElement(Document& document, const QualifiedName& name)
{
    ASSERT(!isStandardElementName(name.localName())); // HTMLTagNames.in lists builtin SVG/MathML elements with "-" in their names explicitly as HTMLUnknownElement.
    if (validateCustomElementNameWithoutCheckingStandardElementNames(name.localName()) != CustomElementNameValidationStatus::Valid)
        return HTMLUnknownElement::create(name, document);

    Ref element = HTMLMaybeFormAssociatedCustomElement::create(name, document);
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
    RefPtr element = HTMLElementFactory::createKnownElement(name, document);
    if (LIKELY(element))
        return Ref<Element> { element.releaseNonNull() };

    if (RefPtr window = document.domWindow()) {
        RefPtr registry = window->customElementRegistry();
        if (UNLIKELY(registry)) {
            if (RefPtr elementInterface = registry->findInterface(name))
                return elementInterface->constructElementWithFallback(document, name);
        }
    }

    if (UNLIKELY(!isValidHTMLElementName(name)))
        return Exception { ExceptionCode::InvalidCharacterError };

    return Ref<Element> { createUpgradeCandidateElement(document, name) };
}

ExceptionOr<Ref<Element>> Document::createElementForBindings(const AtomString& name)
{
    if (isHTMLDocument())
        return createHTMLElementWithNameValidation(*this, name.convertToASCIILowercase());

    if (isXHTMLDocument())
        return createHTMLElementWithNameValidation(*this, name);

    if (!isValidName(name))
        return Exception { ExceptionCode::InvalidCharacterError, makeString("Invalid qualified name: '"_s, name, '\'') };

    return createElement(QualifiedName(nullAtom(), name, nullAtom()), false);
}

Ref<DocumentFragment> Document::createDocumentFragment()
{
    return DocumentFragment::create(*this);
}

Ref<Text> Document::createTextNode(String&& data)
{
    return Text::create(*this, WTFMove(data));
}

Ref<Comment> Document::createComment(String&& data)
{
    return Comment::create(*this, WTFMove(data));
}

ExceptionOr<Ref<CDATASection>> Document::createCDATASection(String&& data)
{
    if (isHTMLDocument())
        return Exception { ExceptionCode::NotSupportedError };

    if (data.contains("]]>"_s))
        return Exception { ExceptionCode::InvalidCharacterError };

    return CDATASection::create(*this, WTFMove(data));
}

ExceptionOr<Ref<ProcessingInstruction>> Document::createProcessingInstruction(String&& target, String&& data)
{
    if (!isValidName(target))
        return Exception { ExceptionCode::InvalidCharacterError, makeString("Invalid qualified name: '"_s, target, '\'') };

    if (data.contains("?>"_s))
        return Exception { ExceptionCode::InvalidCharacterError };

    return ProcessingInstruction::create(*this, WTFMove(target), WTFMove(data));
}

Ref<Text> Document::createEditingTextNode(String&& text)
{
    return Text::createEditingText(*this, WTFMove(text));
}

Ref<CSSStyleDeclaration> Document::createCSSStyleDeclaration()
{
    Ref propertySet = MutableStyleProperties::create();
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
        auto& attribute = uncheckedDowncast<Attr>(nodeToImport);
        return Ref<Node> { Attr::create(*this, attribute.qualifiedName(), attribute.value()) };
    }
    case DOCUMENT_NODE: // Can't import a document into another document.
    case DOCUMENT_TYPE_NODE: // FIXME: Support cloning a DocumentType node per DOM4.
        break;
    }

    return Exception { ExceptionCode::NotSupportedError };
}


ExceptionOr<Ref<Node>> Document::adoptNode(Node& source)
{
    EventQueueScope scope;

    switch (source.nodeType()) {
    case DOCUMENT_NODE:
        return Exception { ExceptionCode::NotSupportedError };
    case ATTRIBUTE_NODE: {
        auto& attr = uncheckedDowncast<Attr>(source);
        if (RefPtr element = attr.ownerElement()) {
            auto result = element->removeAttributeNode(attr);
            if (result.hasException())
                return result.releaseException();
        }
        break;
    }
    default:
        if (source.isShadowRoot()) {
            // ShadowRoot cannot disconnect itself from the host node.
            return Exception { ExceptionCode::HierarchyRequestError };
        }
        if (auto* frameOwnerElement = dynamicDowncast<HTMLFrameOwnerElement>(source)) {
            if (frame() && frame()->tree().isDescendantOf(frameOwnerElement->contentFrame()))
                return Exception { ExceptionCode::HierarchyRequestError };
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
    if (RefPtr window = document.domWindow()) {
        RefPtr registry = window->customElementRegistry();
        if (UNLIKELY(registry)) {
            if (RefPtr elementInterface = registry->findInterface(name)) {
                Ref element = elementInterface->createElement(document);
                element->setIsCustomElementUpgradeCandidate();
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
    else if (settings().mathMLEnabled() && name.namespaceURI() == MathMLNames::mathmlNamespaceURI)
        element = MathMLElementFactory::createElement(name, *this, createdByParser);
#endif

    if (element)
        m_sawElementsInKnownNamespaces = true;
    else
        element = Element::create(name, protectedDocument());

    // <image> uses imgTag so we need a special rule.
    ASSERT((name.matches(imageTag) && element->tagQName().matches(imgTag) && element->tagQName().prefix() == name.prefix()) || name == element->tagQName());

    return element.releaseNonNull();
}

// https://html.spec.whatwg.org/#valid-custom-element-name

struct UnicodeCodePointRange {
    char32_t minimum;
    char32_t maximum;
};

#if ASSERT_ENABLED

static inline bool operator<(const UnicodeCodePointRange& a, const UnicodeCodePointRange& b)
{
    ASSERT(a.minimum <= a.maximum);
    ASSERT(b.minimum <= b.maximum);
    return a.maximum < b.minimum;
}

#endif // ASSERT_ENABLED

static inline bool operator<(const UnicodeCodePointRange& a, char32_t b)
{
    ASSERT(a.minimum <= a.maximum);
    return a.maximum < b;
}

static inline bool operator<(char32_t a, const UnicodeCodePointRange& b)
{
    ASSERT(b.minimum <= b.maximum);
    return a < b.minimum;
}

static inline bool isPotentialCustomElementNameCharacter(char32_t character)
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

enum class CustomElementNameCharacterKind : uint8_t {
    Invalid,
    Valid,
    Hyphen,
    Upper,
};

static ALWAYS_INLINE CustomElementNameCharacterKind customElementNameCharacterKind(LChar character)
{
    using Kind = CustomElementNameCharacterKind;
    static const Kind table[] = {
        Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid,
        Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid,
        Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid,
        Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid,
        Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid,
        Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Hyphen,  Kind::Valid,   Kind::Invalid, // -, .
        Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Valid,   // 0-7
        Kind::Valid,   Kind::Valid,   Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, // 8-9
        Kind::Invalid, Kind::Upper,   Kind::Upper,   Kind::Upper,   Kind::Upper,   Kind::Upper,   Kind::Upper,   Kind::Upper,   // @, A-G
        Kind::Upper,   Kind::Upper,   Kind::Upper,   Kind::Upper,   Kind::Upper,   Kind::Upper,   Kind::Upper,   Kind::Upper,   // H-O
        Kind::Upper,   Kind::Upper,   Kind::Upper,   Kind::Upper,   Kind::Upper,   Kind::Upper,   Kind::Upper,   Kind::Upper,   // P-W
        Kind::Upper,   Kind::Upper,   Kind::Upper,   Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Valid,   // X-Z, _
        Kind::Invalid, Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Valid,   // a-g
        Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Valid,   // h-o
        Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Valid,   // p-w
        Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, // x-z
        Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid,
        Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid,
        Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid,
        Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid,
        Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid,
        Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid,
        Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Valid,   // xB7
        Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid, Kind::Invalid,
        Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Valid,   // xC0-xC7
        Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Valid,   // xC8-xCF
        Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Invalid, // xD0-xD6
        Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Valid,   // xD8-xDF
        Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Valid,   // xE0-xE7
        Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Valid,   // xE8-xEF
        Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Invalid, // xF0-xF6
        Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Valid,   Kind::Valid,   // xF8-xFF
    };
    ASSERT(std::size(table) == 256);
    return table[character];
}

static ALWAYS_INLINE CustomElementNameValidationStatus validateCustomElementNameWithoutCheckingStandardElementNames(const AtomString& localName)
{
    if (!isASCIILower(localName[0]))
        return CustomElementNameValidationStatus::FirstCharacterIsNotLowercaseASCIILetter;

    bool containsHyphen = false;
    if (LIKELY(localName.is8Bit())) {
        for (auto character : localName.string().span8()) {
            switch (customElementNameCharacterKind(character)) {
            case CustomElementNameCharacterKind::Invalid:
                return CustomElementNameValidationStatus::ContainsDisallowedCharacter;
            case CustomElementNameCharacterKind::Valid:
                break;
            case CustomElementNameCharacterKind::Hyphen:
                containsHyphen = true;
                break;
            case CustomElementNameCharacterKind::Upper:
                return CustomElementNameValidationStatus::ContainsUppercaseASCIILetter;
            }
        }
    } else {
        for (auto character : StringView(localName).codePoints()) {
            if (isASCIIUpper(character))
                return CustomElementNameValidationStatus::ContainsUppercaseASCIILetter;
            if (!isPotentialCustomElementNameCharacter(character))
                return CustomElementNameValidationStatus::ContainsDisallowedCharacter;
            if (character == '-')
                containsHyphen = true;
        }
    }

    if (!containsHyphen)
        return CustomElementNameValidationStatus::ContainsNoHyphen;

    return CustomElementNameValidationStatus::Valid;
}

static ALWAYS_INLINE bool isStandardElementName(const AtomString& localName)
{
#if ENABLE(MATHML)
    const auto& annotationXmlLocalName = MathMLNames::annotation_xmlTag->localName();
#else
    static MainThreadNeverDestroyed<const AtomString> annotationXmlLocalName("annotation-xml"_s);
#endif
    static MainThreadNeverDestroyed<const AtomString> colorProfileLocalName("color-profile"_s);

    return localName == SVGNames::font_faceTag->localName()
        || localName == SVGNames::font_face_formatTag->localName()
        || localName == SVGNames::font_face_nameTag->localName()
        || localName == SVGNames::font_face_srcTag->localName()
        || localName == SVGNames::font_face_uriTag->localName()
        || localName == SVGNames::missing_glyphTag->localName()
        || localName == annotationXmlLocalName
        || localName == colorProfileLocalName;
}

CustomElementNameValidationStatus Document::validateCustomElementName(const AtomString& localName)
{
    auto result = validateCustomElementNameWithoutCheckingStandardElementNames(localName);
    if (result != CustomElementNameValidationStatus::Valid)
        return result;

    if (isStandardElementName(localName))
        return CustomElementNameValidationStatus::ConflictsWithStandardElementName;

    return CustomElementNameValidationStatus::Valid;
}

ExceptionOr<Ref<Element>> Document::createElementNS(const AtomString& namespaceURI, const AtomString& qualifiedName)
{
    auto opportunisticallyMatchedBuiltinElement = ([&]() -> RefPtr<Element> {
        if (namespaceURI == xhtmlNamespaceURI)
            return HTMLElementFactory::createKnownElement(qualifiedName, *this, nullptr, /* createdByParser */ false);
        if (namespaceURI == SVGNames::svgNamespaceURI)
            return SVGElementFactory::createKnownElement(qualifiedName, *this, /* createdByParser */ false);
#if ENABLE(MATHML)
        if (settings().mathMLEnabled() && namespaceURI == MathMLNames::mathmlNamespaceURI)
            return MathMLElementFactory::createKnownElement(qualifiedName, *this, /* createdByParser */ false);
#endif
        return nullptr;
    })();

    if (LIKELY(opportunisticallyMatchedBuiltinElement))
        return opportunisticallyMatchedBuiltinElement.releaseNonNull();

    auto parseResult = parseQualifiedName(namespaceURI, qualifiedName);
    if (parseResult.hasException())
        return parseResult.releaseException();
    QualifiedName parsedName { parseResult.releaseReturnValue() };
    if (!hasValidNamespaceForElements(parsedName))
        return Exception { ExceptionCode::NamespaceError };

    if (parsedName.namespaceURI() == xhtmlNamespaceURI)
        return createHTMLElementWithNameValidation(*this, parsedName);

    return createElement(parsedName, false);
}

DocumentEventTiming* Document::documentEventTimingFromNavigationTiming()
{
    RefPtr window = domWindow();
    if (!window)
        return nullptr;
    RefPtr navigationTiming = window->performance().navigationTiming();
    return navigationTiming ? &navigationTiming->documentEventTiming() : nullptr;
}

void Document::setReadyState(ReadyState readyState)
{
    if (readyState == m_readyState)
        return;

    switch (readyState) {
    case ReadyState::Loading:
        if (!m_eventTiming.domLoading) {
            auto now = MonotonicTime::now();
            m_eventTiming.domLoading = now;
            if (auto* eventTiming = documentEventTimingFromNavigationTiming())
                eventTiming->domLoading = now;
            // We do this here instead of in the Document constructor because monotonicTimestamp() is 0 when the Document constructor is running.
            if (!url().isEmpty())
                WTFBeginSignpostWithTimeDelta(this, NavigationAndPaintTiming, -Seconds(monotonicTimestamp()), "Loading %" PUBLIC_LOG_STRING " | isMainFrame: %d", url().string().utf8().data(), frame() && frame()->isMainFrame());
            WTFEmitSignpost(this, NavigationAndPaintTiming, "domLoading");
        }
        break;
    case ReadyState::Complete:
        if (!m_eventTiming.domComplete) {
            auto now = MonotonicTime::now();
            m_eventTiming.domComplete = now;
            if (auto* eventTiming = documentEventTimingFromNavigationTiming())
                eventTiming->domComplete = now;
            WTFEmitSignpost(this, NavigationAndPaintTiming, "domComplete");
        }
        FALLTHROUGH;
    case ReadyState::Interactive:
        if (!m_eventTiming.domInteractive) {
            auto now = MonotonicTime::now();
            m_eventTiming.domInteractive = now;
            if (auto* eventTiming = documentEventTimingFromNavigationTiming())
                eventTiming->domInteractive = now;
            WTFEmitSignpost(this, NavigationAndPaintTiming, "domInteractive");
        }
        break;
    }

    m_readyState = readyState;

    if (m_frame)
        dispatchEvent(Event::create(eventNames().readystatechangeEvent, Event::CanBubble::No, Event::IsCancelable::No));

    setVisualUpdatesAllowed(readyState);
}

void Document::setVisualUpdatesAllowed(ReadyState readyState)
{
    switch (readyState) {
    case ReadyState::Loading:
        if (settings().suppressesIncrementalRendering())
            addVisualUpdatePreventedReason(VisualUpdatesPreventedReason::ReadyState);
        break;
    case ReadyState::Interactive:
        break;
    case ReadyState::Complete:
        removeVisualUpdatePreventedReasons(VisualUpdatesPreventedReason::ReadyState);
        break;
    }
}

void Document::addVisualUpdatePreventedReason(VisualUpdatesPreventedReason reason, CompletePageTransition completePageTransition)
{
    m_visualUpdatesPreventedReasons.add(reason);

    if (visualUpdatePreventRequiresLayoutMilestones().contains(reason))
        m_visualUpdatesAllowedChangeRequiresLayoutMilestones = true;

    if (completePageTransition == CompletePageTransition::Yes)
        m_visualUpdatesAllowedChangeCompletesPageTransition = true;

    if (!m_visualUpdatesSuppressionTimer.isActive() && visualUpdatePreventReasonsClearedByTimer().contains(reason))
        m_visualUpdatesSuppressionTimer.startOneShot(1_s * settings().incrementalRenderingSuppressionTimeoutInSeconds());
}

void Document::removeVisualUpdatePreventedReasons(OptionSet<VisualUpdatesPreventedReason> reasons)
{
    bool wasPrevented = !m_visualUpdatesPreventedReasons.isEmpty();
    m_visualUpdatesPreventedReasons.remove(reasons);

    if (!wasPrevented || !m_visualUpdatesPreventedReasons.isEmpty())
        return;

    m_visualUpdatesSuppressionTimer.stop();

    if (m_visualUpdatesAllowedChangeRequiresLayoutMilestones) {
        RefPtr frameView = view();
        if (RefPtr page = this->page()) {
            if (frame()->isMainFrame()) {
                frameView->addPaintPendingMilestones(LayoutMilestone::DidFirstPaintAfterSuppressedIncrementalRendering);
                if (page->requestedLayoutMilestones() & LayoutMilestone::DidFirstLayoutAfterSuppressedIncrementalRendering)
                    protectedFrame()->protectedLoader()->didReachLayoutMilestone(LayoutMilestone::DidFirstLayoutAfterSuppressedIncrementalRendering);
            }
        }
        m_visualUpdatesAllowedChangeRequiresLayoutMilestones = false;
    }

    if (CheckedPtr renderView = this->renderView())
        renderView->repaintViewAndCompositedLayers();

    if (RefPtr frame = this->frame(); frame && m_visualUpdatesAllowedChangeCompletesPageTransition)
        frame->protectedLoader()->completePageTransitionIfNeeded();
    m_visualUpdatesAllowedChangeCompletesPageTransition = false;
    scheduleRenderingUpdate({ });
}

void Document::visualUpdatesSuppressionTimerFired()
{
    removeVisualUpdatePreventedReasons(visualUpdatePreventReasonsClearedByTimer());
}

void Document::setVisualUpdatesAllowedByClient(bool visualUpdatesAllowedByClient)
{
    if (visualUpdatesAllowedByClient)
        addVisualUpdatePreventedReason(VisualUpdatesPreventedReason::Client);
    else
        removeVisualUpdatePreventedReasons(VisualUpdatesPreventedReason::Client);
}

String Document::characterSetWithUTF8Fallback() const
{
    AtomString name = encoding();
    if (!name.isNull())
        return name;
    return "UTF-8"_s;
}

String Document::defaultCharsetForLegacyBindings() const
{
    if (!frame())
        return "UTF-8"_s;
    return settings().defaultTextEncodingName();
}

void Document::setCharset(const String& charset)
{
    if (RefPtr decoder = this->decoder())
        decoder->setEncoding(charset, TextResourceDecoder::UserChosenEncoding);
}

void Document::setContentLanguage(const AtomString& language)
{
    if (m_contentLanguage == language)
        return;
    m_contentLanguage = language;

    // Recalculate style so language is used when selecting the initial font.
    m_styleScope->didChangeStyleSheetEnvironment();
}

const AtomString& Document::effectiveDocumentElementLanguage() const
{
    if (!m_documentElementLanguage.isNull())
        return m_documentElementLanguage;
    return m_contentLanguage;
}

void Document::setDocumentElementLanguage(const AtomString& language)
{
    if (m_documentElementLanguage == language)
        return;

    auto oldEffectiveDocumentElementLangauge = effectiveDocumentElementLanguage();
    m_documentElementLanguage = language;

    if (oldEffectiveDocumentElementLangauge != effectiveDocumentElementLanguage()) {
        for (Ref element : std::exchange(m_elementsWithLangAttrMatchingDocumentElement, { }))
            element->updateEffectiveLangStateAndPropagateToDescendants();
    }

    if (m_contentLanguage == language)
        return;

    // Recalculate style so language is used when selecting the initial font.
    m_styleScope->didChangeStyleSheetEnvironment();
}

ExceptionOr<void> Document::setXMLVersion(const String& version)
{
    if (!XMLDocumentParser::supportsXMLVersion(version))
        return Exception { ExceptionCode::NotSupportedError };

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

String Document::suggestedMIMEType() const
{
    if (isXHTMLDocument())
        return applicationXHTMLContentTypeAtom();
    if (isSVGDocument())
        return imageSVGContentTypeAtom();
    if (xmlStandalone())
        return textXMLContentTypeAtom();
    if (isHTMLDocument())
        return textHTMLContentTypeAtom();
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

    if (RefPtr documentLoader = loader())
        return documentLoader->currentContentType();

    String mimeType = suggestedMIMEType();
    if (!mimeType.isNull())
        return mimeType;

    return "application/xml"_s;
}

AtomString Document::encoding() const
{
    auto encoding = textEncoding().domName();
    return encoding.isNull() ? nullAtom() : AtomString { encoding };
}

RefPtr<Range> Document::caretRangeFromPoint(int x, int y, HitTestSource source)
{
    auto boundary = caretPositionFromPoint(LayoutPoint(x, y), source);
    if (!boundary)
        return nullptr;
    return createLiveRange({ *boundary, *boundary });
}

std::optional<BoundaryPoint> Document::caretPositionFromPoint(const LayoutPoint& clientPoint, HitTestSource source)
{
    if (!hasLivingRenderTree())
        return std::nullopt;

    LayoutPoint localPoint;
    RefPtr node = nodeFromPoint(clientPoint, &localPoint, source);
    if (!node)
        return std::nullopt;

    updateLayoutIgnorePendingStylesheets();

    CheckedPtr renderer = node->renderer();
    if (!renderer)
        return std::nullopt;

    if (renderer->isSkippedContentRoot())
        return { { *node, 0 } };

    auto rangeCompliantPosition = renderer->positionForPoint(localPoint, source).parentAnchoredEquivalent();
    if (rangeCompliantPosition.isNull())
        return std::nullopt;

    unsigned offset = rangeCompliantPosition.offsetInContainerNode();
    node = retargetToScope(*rangeCompliantPosition.protectedContainerNode());
    if (node != rangeCompliantPosition.containerNode())
        offset = 0;

    return { { node.releaseNonNull(), offset } };
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
    // FIXME: We should use RenderObject::hasNonVisibleOverflow() instead of Element::computedStyle() but
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
            RefPtr firstBody = body();
            // 1. If the HTML body element exists, and it is not potentially scrollable, return the
            // HTML body element and abort these steps.
            if (firstBody && !isBodyPotentiallyScrollable(*firstBody))
                return firstBody.get();

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

    RefPtr decoder = document.decoder();
    auto backslashAsCurrencySymbol = decoder ? decoder->encoding().backslashAsCurrencySymbol() : '\\';

    bool previousCharacterWasHTMLSpace = false;
    for (auto character : StringView { title }.codeUnits()) {
        if (isASCIIWhitespace(character))
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

    if (!m_updateTitleTaskScheduled) {
        eventLoop().queueTask(TaskSource::DOMManipulation, [protectedThis = Ref { *this }, this]() mutable {
            m_updateTitleTaskScheduled = false;
            if (RefPtr documentLoader = loader())
                documentLoader->setTitle(m_title);
        });
        m_updateTitleTaskScheduled = true;
    }

    if (CheckedPtr cache = existingAXObjectCache())
        cache->onTitleChange(*this);
}

void Document::updateTitleFromTitleElement()
{
    if (!m_titleElement) {
        updateTitle({ });
        return;
    }

    if (RefPtr htmlTitle = dynamicDowncast<HTMLTitleElement>(*m_titleElement))
        updateTitle(htmlTitle->textWithDirection());
    else if (RefPtr svgTitle = dynamicDowncast<SVGTitleElement>(*m_titleElement)) {
        // FIXME: Does the SVG title element have a text direction?
        updateTitle({ svgTitle->textContent(), TextDirection::LTR });
    }
}

void Document::setTitle(String&& title)
{
    RefPtr element = documentElement();
    if (RefPtr svgElement = dynamicDowncast<SVGSVGElement>(element.get())) {
        if (!m_titleElement) {
            Ref titleElement = SVGTitleElement::create(SVGNames::titleTag, *this);
            m_titleElement = titleElement.copyRef();
            svgElement->insertBefore(titleElement, svgElement->protectedFirstChild());
        }
        // insertBefore above may have ran scripts which removed m_titleElement.
        if (RefPtr titleElement = m_titleElement)
            titleElement->setTextContent(WTFMove(title));
    } else if (is<HTMLElement>(element)) {
        std::optional<String> oldTitle;
        if (!m_titleElement) {
            RefPtr headElement = head();
            if (!headElement)
                return;
            Ref titleElement = HTMLTitleElement::create(HTMLNames::titleTag, *this);
            m_titleElement = titleElement.copyRef();
            headElement->appendChild(titleElement);
        } else
            oldTitle = protectedTitleElement()->textContent();

        // appendChild above may have run scripts which removed m_titleElement.
        if (!m_titleElement)
            return;

        Ref titleElement = *m_titleElement;
        titleElement->setTextContent(String { title });
        CheckedPtr textManipulationController = textManipulationControllerIfExists();
        if (UNLIKELY(textManipulationController)) {
            if (!oldTitle)
                textManipulationController->didAddOrCreateRendererForNode(titleElement);
            else if (*oldTitle != title)
                textManipulationController->didUpdateContentForNode(titleElement);
        }
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

template<typename TitleElement> Element* selectNewTitleElement(Document& document, Element* oldTitleElement, Element& changingElement)
{
    using Traits = TitleTraits<TitleElement>;

    auto* changingTitleElement = dynamicDowncast<TitleElement>(changingElement);
    if (!changingTitleElement) {
        ASSERT(oldTitleElement == Traits::findTitleElement(document));
        return oldTitleElement;
    }

    if (oldTitleElement)
        return Traits::findTitleElement(document);

    // Optimized common case: We have no title element yet.
    // We can figure out which title element should be used without searching.
    bool isEligible = Traits::isInEligibleLocation(*changingTitleElement);
    auto* newTitleElement = isEligible ? changingTitleElement : nullptr;
    ASSERT(newTitleElement == Traits::findTitleElement(document));
    return newTitleElement;
}

inline RefPtr<Element> Document::protectedTitleElement() const
{
    return m_titleElement;
}

void Document::updateTitleElement(Element& changingTitleElement)
{
    // Most documents use HTML title rules.
    // Documents with SVG document elements use SVG title rules.
    auto selectTitleElement = is<SVGSVGElement>(documentElement())
        ? selectNewTitleElement<SVGTitleElement> : selectNewTitleElement<HTMLTitleElement>;
    RefPtr newTitleElement = selectTitleElement(*this, protectedTitleElement().get(), changingTitleElement);
    if (m_titleElement == newTitleElement)
        return;
    m_titleElement = WTFMove(newTitleElement);
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
    m_visibilityStateCallbackClients.add(client);
}

void Document::unregisterForVisibilityStateChangedCallbacks(VisibilityChangeClient& client)
{
    m_visibilityStateCallbackClients.remove(client);
}

void Document::visibilityStateChanged()
{
    // https://w3c.github.io/page-visibility/#reacting-to-visibilitychange-changes
    queueTaskToDispatchEvent(TaskSource::UserInteraction, Event::create(eventNames().visibilitychangeEvent, Event::CanBubble::Yes, Event::IsCancelable::No));
    m_visibilityStateCallbackClients.forEach([](auto& client) {
        client.visibilityStateChanged();
    });

#if ENABLE(MEDIA_STREAM) && PLATFORM(IOS_FAMILY)
    if (auto mediaSessionManager = PlatformMediaSessionManager::sharedManagerIfExists()) {
        if (!mediaSessionManager->isInterrupted())
            updateCaptureAccordingToMutedState();
    }
#endif

    if (!hidden()) {
        auto callbacks = std::exchange(m_whenIsVisibleHandlers, { });
        for (auto& callback : callbacks)
            callback();
    }
    updateServiceWorkerClientData();

    // https://drafts.csswg.org/css-view-transitions/#page-visibility-change-steps
    if (hidden())
        m_inboundViewTransitionParams = nullptr;
}

VisibilityState Document::visibilityState() const
{
    // The visibility of the document is inherited from the visibility of the
    // page. If there is no page associated with the document, we will assume
    // that the page is hidden, as specified by the spec:
    // https://w3c.github.io/page-visibility/#visibilitystate-attribute
    if (!m_frame || !m_frame->page() || m_visibilityHiddenDueToDismissal)
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
    m_mediaElements.add(element);
}

void Document::unregisterMediaElement(HTMLMediaElement& element)
{
    m_mediaElements.remove(element);
}

void Document::forEachMediaElement(const Function<void(HTMLMediaElement&)>& function)
{
    ASSERT(!m_mediaElements.hasNullReferences());
    m_mediaElements.forEach([&](auto& element) {
        function(Ref { element });
    });
}

#endif

String Document::nodeName() const
{
    return "#document"_s;
}

WakeLockManager& Document::wakeLockManager()
{
    if (!m_wakeLockManager)
        m_wakeLockManager = makeUnique<WakeLockManager>(*this);
    return *m_wakeLockManager;
}

FormController& Document::formController()
{
    if (!m_formController)
        m_formController = makeUnique<FormController>();
    return *m_formController;
}

Vector<AtomString> Document::formElementsState() const
{
    if (!m_formController)
        return { };
    return m_formController->formElementsState(*this);
}

void Document::setStateForNewFormElements(const Vector<AtomString>& stateVector)
{
    if (!stateVector.size() && !m_formController)
        return;
    formController().setStateForNewFormElements(stateVector);
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

void Document::updateRenderTree(std::unique_ptr<Style::Update> styleUpdate)
{
    ASSERT(!inRenderTreeUpdate());

    Style::PostResolutionCallbackDisabler callbackDisabler(*this);
    {
        SetForScope inRenderTreeUpdate(m_inRenderTreeUpdate, true);
        {
            RenderTreeUpdater updater(*this, callbackDisabler);
            updater.commit(WTFMove(styleUpdate));
        }
    }
}

void Document::resolveStyle(ResolveStyleType type)
{
    ScriptDisallowedScope::InMainThread scriptDisallowedScope;

    ASSERT(!view() || !view()->isPainting());

    // NOTE: XSL code seems to be the only client stumbling in here without a RenderView.
    if (!m_renderView)
        return;

    Ref frameView = m_renderView->frameView();

    if (isInWebProcess()) {
        RELEASE_ASSERT(!frameView->isPainting());
        RELEASE_ASSERT(!m_inStyleRecalc);
    } else {
        if (frameView->isPainting())
            return;
        if (m_inStyleRecalc)
            return;
    }

    TraceScope tracingScope(StyleRecalcStart, StyleRecalcEnd);

    RenderView::RepaintRegionAccumulator repaintRegionAccumulator(renderView());

    // FIXME: We should update style on our ancestor chain before proceeding, however doing so at
    // the time this comment was originally written caused several tests to crash.

    // FIXME: Do this user agent shadow tree update per tree scope.
    for (auto& element : copyToVectorOf<Ref<Element>>(m_elementsWithPendingUserAgentShadowTreeUpdates))
        element->updateUserAgentShadowTree();

    styleScope().flushPendingUpdate();
    frameView->willRecalcStyle();

    InspectorInstrumentation::willRecalculateStyle(*this);

    bool updatedCompositingLayers = false;
    {
        Style::PostResolutionCallbackDisabler disabler(*this);
        WidgetHierarchyUpdatesSuspensionScope suspendWidgetHierarchyUpdates;

        m_inStyleRecalc = true;

        if (m_needsFullStyleRebuild)
            type = ResolveStyleType::Rebuild;

        if (type == ResolveStyleType::Rebuild) {
            // This may get set again during style resolve.
            m_hasNodesWithMissingStyle = false;

            auto newStyle = Style::resolveForDocument(*this);

            auto documentChange = m_initialContainingBlockStyle ? Style::determineChange(newStyle, *m_initialContainingBlockStyle) : Style::Change::Renderer;
            if (documentChange != Style::Change::None) {
                m_initialContainingBlockStyle = RenderStyle::clonePtr(newStyle);
                // The used style may end up differing from the computed style due to propagation of properties from elements.
                renderView()->setStyle(WTFMove(newStyle));
            }

            if (RefPtr documentElement = this->documentElement())
                documentElement->invalidateStyleForSubtree();
        }

        // FIXME: Be smarter about invalidation for anchor positioning.
        // This simply repeats the entire anchor-positioning process.
        styleScope().clearAnchorPositioningState();

        Style::TreeResolver resolver(*this, WTFMove(m_pendingRenderTreeUpdate));
        auto styleUpdate = resolver.resolve();

        while (resolver.hasUnresolvedQueryContainers() || resolver.hasUnresolvedAnchorPositionedElements()) {
            if (styleUpdate) {
                SetForScope resolvingContainerQueriesScope(m_isResolvingContainerQueries, resolver.hasUnresolvedQueryContainers());
                SetForScope resolvingAnchorPositionedElementsScope(m_isResolvingAnchorPositionedElements, resolver.hasUnresolvedAnchorPositionedElements());

                updateRenderTree(WTFMove(styleUpdate));

                if (frameView->layoutContext().needsLayout())
                    frameView->layoutContext().layout();
            }

            styleUpdate = resolver.resolve();
        }

        m_lastStyleUpdateSizeForTesting = styleUpdate ? styleUpdate->size() : 0;

        setHasValidStyle();
        clearChildNeedsStyleRecalc();
        unscheduleStyleRecalc();

        m_inStyleRecalc = false;

        if (auto* fontLoader = m_fontLoader.get())
            fontLoader->loadPendingFonts();

        if (styleUpdate) {
            updateRenderTree(WTFMove(styleUpdate));
            frameView->styleAndRenderTreeDidChange();
        }

        updatedCompositingLayers = frameView->updateCompositingLayersAfterStyleChange();

        if (m_renderView->needsLayout())
            frameView->layoutContext().scheduleLayout();

        // As a result of the style recalculation, the currently hovered element might have been
        // detached (for example, by setting display:none in the :hover style), schedule another mouseMove event
        // to check if any other elements ended up under the mouse pointer due to re-layout.
        if (m_hoveredElement && !m_hoveredElement->renderer()) {
            if (RefPtr localMainFrame = dynamicDowncast<LocalFrame>(frameView->frame().mainFrame()))
                localMainFrame->eventHandler().dispatchFakeMouseMoveEventSoon();
        }

        ++m_styleRecalcCount;
        // FIXME: Assert ASSERT(!needsStyleRecalc()) here. fast/events/media-element-focus-tab.html hits this assertion.
    }

    InspectorInstrumentation::didRecalculateStyle(*this);

    // Some animated images may now be inside the viewport due to style recalc,
    // resume them if necessary if there is no layout pending. Otherwise, we'll
    // check if they need to be resumed after layout.
    if (updatedCompositingLayers && !frameView->needsLayout())
        frameView->viewportContentsChanged();
}

void Document::updateTextRenderer(Text& text, unsigned offsetOfReplacedText, unsigned lengthOfReplacedText)
{
    if (!hasLivingRenderTree())
        return;

    ensurePendingRenderTreeUpdate().addText(text, { offsetOfReplacedText, lengthOfReplacedText, std::nullopt });
}

void Document::updateSVGRenderer(SVGElement& element)
{
    if (!hasLivingRenderTree())
        return;

    ensurePendingRenderTreeUpdate().addSVGRendererUpdate(element);
}

Style::Update& Document::ensurePendingRenderTreeUpdate()
{
    ASSERT(hasLivingRenderTree());

    if (!m_pendingRenderTreeUpdate)
        m_pendingRenderTreeUpdate = makeUnique<Style::Update>(*this);

    scheduleRenderingUpdate({ });

    return *m_pendingRenderTreeUpdate;
}

bool Document::needsStyleRecalc() const
{
    if (backForwardCacheState() != NotInBackForwardCache)
        return false;

    if (m_needsFullStyleRebuild)
        return true;

    if (childNeedsStyleRecalc())
        return true;

    if (m_pendingRenderTreeUpdate)
        return true;

    if (styleScope().hasPendingUpdate())
        return true;

    return false;
}

bool Document::updateStyleIfNeeded()
{
    ScriptDisallowedScope::InMainThread scriptDisallowedScope;

    if (isResolvingContainerQueriesForSelfOrAncestor())
        return false;

    RefPtr frameView = view();
    {
        ASSERT(isMainThread());
        ASSERT(!frameView || !frameView->isPainting());

        if (!frameView || frameView->layoutContext().isInRenderTreeLayout())
            return false;

        styleScope().flushPendingUpdate();

        if (!needsStyleRecalc())
            return false;
    }

#if ENABLE(CONTENT_CHANGE_OBSERVER)
    ContentChangeObserver::StyleRecalcScope observingScope(*this);
#endif
    resolveStyle();
    return true;
}

auto Document::updateLayoutIgnorePendingStylesheets(OptionSet<LayoutOptions> layoutOptions, const Element* context) -> UpdateLayoutResult
{
    layoutOptions.add(LayoutOptions::IgnorePendingStylesheets);
    return updateLayout(layoutOptions, context);
}

auto Document::updateLayout(OptionSet<LayoutOptions> layoutOptions, const Element* context) -> UpdateLayoutResult
{
    bool oldIgnore = m_ignorePendingStylesheets;

    if (layoutOptions.contains(LayoutOptions::IgnorePendingStylesheets)) {
        if (!haveStylesheetsLoaded()) {
            m_ignorePendingStylesheets = true;
            // FIXME: This should just invalidate elements with missing styles.
            if (m_hasNodesWithMissingStyle)
                scheduleFullStyleRebuild();
        }
        updateRelevancyOfContentVisibilityElements();
    }

    ASSERT(isMainThread());

    RefPtr frameView = view();
    if (frameView && frameView->layoutContext().isInRenderTreeLayout()) {
        // View layout should not be re-entrant.
        ASSERT_NOT_REACHED();
        return UpdateLayoutResult::NoChange;
    }

    UpdateLayoutResult result = UpdateLayoutResult::NoChange;

    {
        RenderView::RepaintRegionAccumulator repaintRegionAccumulator(renderView());
        ScriptDisallowedScope::InMainThread scriptDisallowedScope;

        if (!layoutOptions.contains(LayoutOptions::DoNotLayoutAncestorDocuments)) {
            if (ownerElement() && ownerElement()->protectedDocument()->updateLayout(layoutOptions, context) == UpdateLayoutResult::ChangesDone)
                result = UpdateLayoutResult::ChangesDone;
        }

        if (updateStyleIfNeeded())
            result = UpdateLayoutResult::ChangesDone;

        StackStats::LayoutCheckPoint layoutCheckPoint;

        if (frameView && renderView()) {
            if (context && layoutOptions.contains(LayoutOptions::ContentVisibilityForceLayout)) {
                if (context->renderer() && context->renderer()->style().hasSkippedContent()) {
                    if (auto wasSkippedDuringLastLayout = context->renderer()->wasSkippedDuringLastLayoutDueToContentVisibility()) {
                        if (*wasSkippedDuringLastLayout)
                            context->renderer()->setNeedsLayout();
                        else
                            context = nullptr;
                    }
                } else
                    context = nullptr;
            }
            if (frameView->layoutContext().needsLayout(layoutOptions)) {
                ContentVisibilityForceLayoutScope scope(*renderView(), context);
                frameView->layoutContext().layout(layoutOptions.contains(LayoutOptions::CanDeferUpdateLayerPositions));
                result = UpdateLayoutResult::ChangesDone;
            }

            if (layoutOptions.contains(LayoutOptions::UpdateCompositingLayers) && frameView->updateCompositingLayersAfterLayoutIfNeeded())
                result = UpdateLayoutResult::ChangesDone;
        }
    }

    if (layoutOptions.contains(LayoutOptions::RunPostLayoutTasksSynchronously) && view())
        protectedView()->flushAnyPendingPostLayoutTasks();

    if (layoutOptions.contains(LayoutOptions::IgnorePendingStylesheets)) {
        if (RefPtr frameView = view())
            frameView->updateScrollAnchoringPositionForScrollableAreas();
    }

    m_ignorePendingStylesheets = oldIgnore;
    return result;
}

std::unique_ptr<RenderStyle> Document::styleForElementIgnoringPendingStylesheets(Element& element, const RenderStyle* parentStyle, const std::optional<Style::PseudoElementIdentifier>& pseudoElementIdentifier)
{
    ASSERT(&element.document() == this);
    ASSERT(!element.isPseudoElement() || !pseudoElementIdentifier);
    ASSERT(!pseudoElementIdentifier || parentStyle);
    ASSERT(Style::postResolutionCallbacksAreSuspended());

    std::optional<RenderStyle> updatedDocumentStyle;
    if (!parentStyle && m_needsFullStyleRebuild && hasLivingRenderTree()) {
        updatedDocumentStyle.emplace(Style::resolveForDocument(*this));
        parentStyle = &*updatedDocumentStyle;
    }

    SetForScope change(m_ignorePendingStylesheets, true);
    Ref resolver = element.styleResolver();

    if (pseudoElementIdentifier) {
        auto style = resolver->styleForPseudoElement(element, { *pseudoElementIdentifier }, { parentStyle });
        if (!style)
            return nullptr;
        return WTFMove(style->style);
    }

    auto elementStyle = resolver->styleForElement(element, { parentStyle });
    if (elementStyle.relations) {
        Style::Update emptyUpdate(*this);
        Style::commitRelations(WTFMove(elementStyle.relations), emptyUpdate);
    }

    return WTFMove(elementStyle.style);
}

bool Document::updateLayoutIfDimensionsOutOfDate(Element& element, OptionSet<DimensionsCheck> dimensionsCheck)
{
    ASSERT(isMainThread());

    // If the stylesheets haven't loaded, just give up and do a full layout ignoring pending stylesheets.
    if (!haveStylesheetsLoaded()) {
        updateLayoutIgnorePendingStylesheets();
        return true;
    }

    // Check for re-entrancy and assert (same code that is in updateLayout()).
    RefPtr frameView = view();
    if (frameView && frameView->layoutContext().isInRenderTreeLayout()) {
        // View layout should not be re-entrant.
        ASSERT_NOT_REACHED();
        return true;
    }

    RenderView::RepaintRegionAccumulator repaintRegionAccumulator(renderView());

    // Mimic the structure of updateLayout(), but at each step, see if we have been forced into doing a full layout.
    if (RefPtr owner = ownerElement()) {
        if (owner->protectedDocument()->updateLayoutIfDimensionsOutOfDate(*owner)) {
            updateLayout({ }, &element);
            return true;
        }
    }

    updateRelevancyOfContentVisibilityElements();

    updateStyleIfNeeded();

    if (!element.renderer() || !frameView->layoutContext().needsLayout()) {
        // Tree is clean, we don't need to walk the ancestor chain to figure out whether we have a sufficiently clean subtree.
        return false;
    }

    // If the renderer needs layout for any reason, give up.
    // Also, turn off this optimization for input elements with shadow content.
    bool requireFullLayout = element.renderer()->needsLayout() || is<HTMLInputElement>(element);
    if (!requireFullLayout) {
        CheckedPtr<RenderBox> previousBox;
        CheckedPtr<RenderBox> currentBox;

        CheckedPtr renderer = element.renderer();
        bool hasSpecifiedLogicalHeight = renderer->style().logicalMinHeight() == Length(0, LengthType::Fixed) && renderer->style().logicalHeight().isFixed() && renderer->style().logicalMaxHeight().isAuto();
        bool isVertical = !renderer->isHorizontalWritingMode();
        bool checkingLogicalWidth = (dimensionsCheck.contains(DimensionsCheck::Width) && !isVertical) || (dimensionsCheck.contains(DimensionsCheck::Height) && isVertical);
        bool checkingLogicalHeight = (dimensionsCheck.contains(DimensionsCheck::Height) && !isVertical) || (dimensionsCheck.contains(DimensionsCheck::Width) && isVertical);

        // Check our containing block chain. If anything in the chain needs a layout, then require a full layout.
        for (CheckedPtr currentRenderer = renderer; currentRenderer && !currentRenderer->isRenderView(); currentRenderer = currentRenderer->container()) {

            // Require the entire container chain to be boxes.
            CheckedPtr currentRendererBox = dynamicDowncast<RenderBox>(*currentRenderer);
            if (!currentRendererBox) {
                requireFullLayout = true;
                break;
            }

            previousBox = std::exchange(currentBox, WTFMove(currentRendererBox));

            if (currentBox->style().containerType() != ContainerType::Normal) {
                requireFullLayout = true;
                break;
            }

            // If a box needs layout for itself or if a box has changed children and sizes its width to
            // its content, then require a full layout.
            if (currentBox->selfNeedsLayout() ||
                (checkingLogicalWidth && currentRenderer->needsLayout() && currentBox->sizesLogicalWidthToFitContent(RenderBox::SizeType::MainOrPreferredSize))) {
                requireFullLayout = true;
                break;
            }

            // If a block contains floats and the child's height isn't specified, then
            // give up also, since our height could end up being influenced by the floats.
            if (checkingLogicalHeight && !hasSpecifiedLogicalHeight) {
                if (CheckedPtr currentBlockFlow = dynamicDowncast<RenderBlockFlow>(*currentBox)) {
                    if (currentBlockFlow->containsFloats() && previousBox && !previousBox->isFloatingOrOutOfFlowPositioned()) {
                        requireFullLayout = true;
                        break;
                    }
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

            if (currentRenderer == frameView->layoutContext().subtreeLayoutRoot())
                break;
        }
    }

    StackStats::LayoutCheckPoint layoutCheckPoint;

    // Only do a layout if changes have occurred that make it necessary.
    if (requireFullLayout)
        updateLayout({ }, &element);

    return requireFullLayout;
}

bool Document::isPageBoxVisible(int pageIndex)
{
    updateStyleIfNeeded();
    std::unique_ptr<RenderStyle> pageStyle(styleScope().resolver().styleForPage(pageIndex));
    return pageStyle->usedVisibility() != Visibility::Hidden; // display property doesn't apply to @page.
}

void Document::pageSizeAndMarginsInPixels(int pageIndex, IntSize& pageSize, int& marginTop, int& marginRight, int& marginBottom, int& marginLeft)
{
    updateStyleIfNeeded();
    auto style = styleScope().resolver().styleForPage(pageIndex);

    int width = pageSize.width();
    int height = pageSize.height();
    switch (style->pageSizeType()) {
    case PageSizeType::Auto:
        break;
    case PageSizeType::AutoLandscape:
        if (width < height)
            std::swap(width, height);
        break;
    case PageSizeType::AutoPortrait:
        if (width > height)
            std::swap(width, height);
        break;
    case PageSizeType::Resolved: {
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

void Document::fontsNeedUpdate(FontSelector&)
{
    ASSERT(!deletionHasBegun());
    invalidateMatchedPropertiesCacheAndForceStyleRecalc();
}

void Document::invalidateMatchedPropertiesCacheAndForceStyleRecalc()
{
    styleScope().invalidateMatchedDeclarationsCache();

    if (backForwardCacheState() != NotInBackForwardCache || !renderView())
        return;
    scheduleFullStyleRebuild();
}

void Document::setIsResolvingTreeStyle(bool value)
{
    RELEASE_ASSERT(value != m_isResolvingTreeStyle);
    m_isResolvingTreeStyle = value;
}

bool Document::isResolvingContainerQueriesForSelfOrAncestor() const
{
    if (m_isResolvingContainerQueries)
        return true;
    if (RefPtr owner = ownerElement())
        return owner->document().isResolvingContainerQueriesForSelfOrAncestor();
    return false;
}

bool Document::isInStyleInterleavedLayoutForSelfOrAncestor() const
{
    if (isInStyleInterleavedLayout())
        return true;
    if (RefPtr owner = ownerElement())
        return owner->document().isInStyleInterleavedLayoutForSelfOrAncestor();
    return false;
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
    CheckedPtr renderView = m_renderView.get();
    Node::setRenderer(renderView.get());

    renderView->setIsInWindow(true);

    resolveStyle(ResolveStyleType::Rebuild);
}

void Document::didBecomeCurrentDocumentInFrame()
{
    protectedFrame()->checkedScript()->updateDocument();

    // Many of these functions have event handlers which can detach the frame synchronously, so we must check repeatedly in this function.
    if (!m_frame)
        return;

    if (!hasLivingRenderTree())
        createRenderTree();
    if (!m_frame)
        return;

    dispatchDisabledAdaptationsDidChangeForMainFrame();
    if (!m_frame)
        return;

    updateViewportArguments();
    if (!m_frame)
        return;

    // FIXME: Doing this only for the main frame is insufficient.
    // Changing a subframe can also change the wheel event handler count.
    // FIXME: Doing this only when a document goes into the frame is insufficient.
    // Removing a document can also change the wheel event handler count.
    // FIXME: Doing this every time is a waste. If the current document and its
    // subframes' documents have no wheel event handlers, then the count did not change,
    // unless the documents they are replacing had wheel event handlers.
    if (page() && m_frame->isMainFrame())
        wheelEventHandlersChanged();
    if (!m_frame)
        return;

    // Ensure that the scheduled task state of the document matches the DOM suspension state of the frame. It can
    // be out of sync if the DOM suspension state changed while the document was not in the frame (possibly in the
    // back/forward cache, or simply newly created).
    if (m_frame->activeDOMObjectsAndAnimationsSuspended()) {
        if (m_timelinesController)
            m_timelinesController->suspendAnimations();
        suspendScheduledTasks(ReasonForSuspension::PageWillBeSuspended);
    } else {
        resumeScheduledTasks(ReasonForSuspension::PageWillBeSuspended);
        if (m_timelinesController)
            m_timelinesController->resumeAnimations();
    }
}

void Document::frameDestroyed()
{
    // detachFromFrame() must be called before destroying the Frame.
    RELEASE_ASSERT(!m_frame);

    if (RefPtr window = domWindow())
        window->frameDestroyed();

    FrameDestructionObserver::frameDestroyed();
}

void Document::willDetachPage()
{
    FrameDestructionObserver::willDetachPage();
#if ENABLE(CONTENT_CHANGE_OBSERVER)
    contentChangeObserver().willDetachPage();
#endif
    if (domWindow() && frame())
        InspectorInstrumentation::frameWindowDiscarded(protectedFrame().releaseNonNull(), protectedWindow().get());
}

void Document::attachToCachedFrame(CachedFrameBase& cachedFrame)
{
    RELEASE_ASSERT(cachedFrame.document() == this);
    ASSERT(cachedFrame.view());
    ASSERT(m_backForwardCacheState == Document::InBackForwardCache);
    if (auto* localFrameView = dynamicDowncast<LocalFrameView>(cachedFrame.view()))
        observeFrame(localFrameView->protectedFrame().ptr());
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

    SetForScope change(m_renderTreeBeingDestroyed, true);

    if (this == &topDocument())
        clearAXObjectCache();

    documentWillBecomeInactive();

    if (RefPtr view = this->view())
        view->willDestroyRenderTree();

    m_pendingRenderTreeUpdate = { };
    m_initialContainingBlockStyle = { };

    if (RefPtr documentElement = m_documentElement)
        RenderTreeUpdater::tearDownRenderers(*documentElement);

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

    if (RefPtr view = this->view())
        view->didDestroyRenderTree();
}

void Document::willBeRemovedFromFrame()
{
    if (m_hasPreparedForDestruction)
        return;

#if ENABLE(WEB_RTC)
    if (RefPtr rtcNetworkManager = m_rtcNetworkManager)
        rtcNetworkManager->unregisterMDNSNames();
#endif

    setActiveServiceWorker(nullptr);
    setServiceWorkerConnection(nullptr);

#if ENABLE(IOS_TOUCH_EVENTS)
    clearTouchEventHandlersAndListeners();
#endif

    protectedUndoManager()->removeAllItems();

    m_textManipulationController = nullptr; // Free nodes kept alive by TextManipulationController.

    if (this != &topDocument()) {
        // Let the ax cache know that this subframe goes out of scope.
        if (CheckedPtr cache = existingAXObjectCache())
            cache->prepareForDocumentDestruction(*this);
    }

    {
        NavigationDisabler navigationDisabler(m_frame.get());
        disconnectDescendantFrames();
    }
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(!m_frame || !m_frame->tree().childCount());

    ScriptDisallowedScope::InMainThread scriptDisallowedScope;

    if (m_domWindow && m_frame)
        protectedWindow()->willDetachDocumentFromFrame();

    styleScope().clearResolver();

    if (hasLivingRenderTree())
        destroyRenderTree();

    if (auto* pluginDocument = dynamicDowncast<PluginDocument>(*this))
        pluginDocument->detachFromPluginElement();

    if (RefPtr<Page> page = this->page()) {
#if ENABLE(POINTER_LOCK)
        page->pointerLockController().documentDetached(*this);
#endif
        if (auto* imageOverlayController = page->imageOverlayControllerIfExists())
            imageOverlayController->documentDetached(*this);
        if (auto* validationMessageClient = page->validationMessageClient())
            validationMessageClient->documentDetached(*this);
    }

    InspectorInstrumentation::documentDetached(*this);

    commonTeardown();

#if ENABLE(TOUCH_EVENTS)
    if (m_touchEventTargets && !m_touchEventTargets->isEmptyIgnoringNullReferences() && parentDocument())
        protectedParentDocument()->didRemoveEventTargetNode(*this);
#endif

    if (m_wheelEventTargets && !m_wheelEventTargets->isEmptyIgnoringNullReferences() && parentDocument())
        protectedParentDocument()->didRemoveEventTargetNode(*this);

    if (RefPtr mediaQueryMatcher = m_mediaQueryMatcher)
        mediaQueryMatcher->documentDestroyed();

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    if (!m_clientToIDMap.isEmpty() && page()) {
        for (auto* client : copyToVector(m_clientToIDMap.keys()))
            removePlaybackTargetPickerClient(*client);
    }
#endif

    protectedCachedResourceLoader()->stopUnusedPreloadsTimer();

    if (page() && !m_mediaState.isEmpty()) {
        m_mediaState = MediaProducer::IsNotPlaying;
        protectedPage()->updateIsPlayingMedia();
    }

    selection().willBeRemovedFromFrame();
    if (CheckedPtr editor = m_editor.get())
        editor->clear();
    detachFromFrame();

    for (auto& scope : m_paintWorkletGlobalScopes.values())
        scope->prepareForDestruction();
    m_paintWorkletGlobalScopes.clear();

    m_hasPreparedForDestruction = true;

    // Note that m_backForwardCacheState can be Document::AboutToEnterBackForwardCache if our frame
    // was removed in an onpagehide event handler fired when the top-level frame is
    // about to enter the back/forward cache.
    RELEASE_ASSERT(m_backForwardCacheState != Document::InBackForwardCache);
}

void Document::removeAllEventListeners()
{
    EventTarget::removeAllEventListeners();

    if (RefPtr window = m_domWindow)
        window->removeAllEventListeners();

    protectedReportingScope()->removeAllObservers();

    // FIXME: What about disconnected nodes.
    for (RefPtr node = firstChild(); node; node = NodeTraversal::next(*node))
        node->removeAllEventListeners();
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

void Document::suspendFontLoading()
{
    if (auto* fontLoader = m_fontLoader.get())
        fontLoader->suspendFontLoading();
}

bool Document::shouldBypassMainWorldContentSecurityPolicy() const
{
    // Bypass this policy when the world is known, and it not the normal world.
    JSC::VM& vm = commonVM();
    auto* callFrame = vm.topCallFrame;
    return callFrame && callFrame != JSC::CallFrame::noCaller() && !currentWorld(*callFrame->lexicalGlobalObject(vm)).isNormal();
}

void Document::platformSuspendOrStopActiveDOMObjects()
{
#if ENABLE(CONTENT_CHANGE_OBSERVER)
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
        m_documentTaskGroup->markAsReadyToStop();
    ScriptExecutionContext::stopActiveDOMObjects();
    platformSuspendOrStopActiveDOMObjects();

    // https://www.w3.org/TR/screen-wake-lock/#handling-document-loss-of-full-activity
    if (m_wakeLockManager)
        m_wakeLockManager->releaseAllLocks(WakeLockType::Screen);
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
    Ref topDocument = this->topDocument();

    // If the document has already been detached, do not make a new axObjectCache.
    if (!topDocument->hasLivingRenderTree())
        return nullptr;

    ASSERT(topDocument.ptr() == this || !m_axObjectCache);
    if (!topDocument->m_axObjectCache) {
        topDocument->m_axObjectCache = makeUnique<AXObjectCache>(topDocument);
        hasEverCreatedAnAXObjectCache = true;
    }
    return topDocument->m_axObjectCache.get();
}

void Document::setVisuallyOrdered()
{
    m_visuallyOrdered = true;
    if (CheckedPtr renderView = this->renderView())
        renderView->mutableStyle().setRTLOrdering(Order::Visual);
}

Ref<DocumentParser> Document::createParser()
{
    // FIXME: this should probably pass the frame instead
    return XMLDocumentParser::create(*this, view() ? XMLDocumentParser::IsInFrameView::Yes : XMLDocumentParser::IsInFrameView::No, m_parserContentPolicy);
}

bool Document::hasHighlight() const
{
    return (m_highlightRegistry && !m_highlightRegistry->isEmpty())
        || (m_fragmentHighlightRegistry && !m_fragmentHighlightRegistry->isEmpty())
#if ENABLE(APP_HIGHLIGHTS)
        || (m_appHighlightRegistry && !m_appHighlightRegistry->isEmpty())
#endif
    ;
}

HighlightRegistry& Document::highlightRegistry()
{
    if (!m_highlightRegistry)
        m_highlightRegistry = HighlightRegistry::create();
    return *m_highlightRegistry;
}

HighlightRegistry& Document::fragmentHighlightRegistry()
{
    if (!m_fragmentHighlightRegistry)
        m_fragmentHighlightRegistry = HighlightRegistry::create();
    return *m_fragmentHighlightRegistry;
}

#if ENABLE(APP_HIGHLIGHTS)
HighlightRegistry& Document::appHighlightRegistry()
{
    if (!m_appHighlightRegistry) {
        m_appHighlightRegistry = HighlightRegistry::create();
        if (RefPtr currentPage = page())
            m_appHighlightRegistry->setHighlightVisibility(currentPage->chrome().client().appHighlightsVisiblility());
    }
    return *m_appHighlightRegistry;
}

AppHighlightStorage& Document::appHighlightStorage()
{
    if (!m_appHighlightStorage)
        m_appHighlightStorage = makeUnique<AppHighlightStorage>(*this);
    return *m_appHighlightStorage;
}
#endif
void Document::collectHighlightRangesFromRegister(Vector<WeakPtr<HighlightRange>>& highlightRanges, const HighlightRegistry& highlightRegistry)
{
    for (auto& highlight : highlightRegistry.map()) {
        for (auto& highlightRange : highlight.value->highlightRanges()) {
            if (highlightRange->startPosition().isNotNull() && highlightRange->endPosition().isNotNull() && !highlightRange->range().isLiveRange())
                continue;

            if (RefPtr liveRange = dynamicDowncast<Range>(highlightRange->range()); liveRange && !liveRange->didChangeForHighlight())
                continue;

            auto simpleRange = makeSimpleRange(highlightRange->range());
            if (&simpleRange.startContainer().treeScope() != &simpleRange.endContainer().treeScope())
                continue;
            highlightRanges.append(highlightRange.get());
        }
    }

    // One range can belong to multiple highlights so resetting a range's flag cannot be done in the loops above.
    for (auto& highlight : highlightRegistry.map()) {
        for (auto& highlightRange : highlight.value->highlightRanges()) {
            if (RefPtr liveRange = dynamicDowncast<Range>(highlightRange->range()); liveRange && liveRange->didChangeForHighlight())
                liveRange->resetDidChangeForHighlight();
        }
    }
}

void Document::updateHighlightPositions()
{
    Vector<WeakPtr<HighlightRange>> highlightRanges;
    if (m_highlightRegistry)
        collectHighlightRangesFromRegister(highlightRanges, *m_highlightRegistry.get());
    if (m_fragmentHighlightRegistry)
        collectHighlightRangesFromRegister(highlightRanges, *m_fragmentHighlightRegistry.get());
#if ENABLE(APP_HIGHLIGHTS)
    if (m_appHighlightRegistry)
        collectHighlightRangesFromRegister(highlightRanges, *m_appHighlightRegistry.get());
#endif

    for (auto& weakRangeData : highlightRanges) {
        if (RefPtr highlightRange = weakRangeData.get()) {
            VisibleSelection visibleSelection(makeSimpleRange(highlightRange->range()));
            auto range = makeSimpleRange(highlightRange->range());

            auto startPosition = visibleSelection.visibleStart().deepEquivalent();
            auto endPosition = visibleSelection.visibleEnd().deepEquivalent();
            if (!weakRangeData.get())
                continue;

            if (auto simpleRange = makeSimpleRange(highlightRange->startPosition(), highlightRange->endPosition()))
                Highlight::repaintRange(StaticRange::create(*simpleRange));

            if (!startPosition.isNull())
                highlightRange->setStartPosition(WTFMove(startPosition));
            if (!endPosition.isNull())
                highlightRange->setEndPosition(WTFMove(endPosition));

            Highlight::repaintRange(highlightRange->range());
        }
    }
}

ScriptableDocumentParser* Document::scriptableDocumentParser() const
{
    return parser() ? parser()->asScriptableDocumentParser() : nullptr;
}

ExceptionOr<RefPtr<WindowProxy>> Document::openForBindings(LocalDOMWindow& activeWindow, LocalDOMWindow& firstWindow, const String& url, const AtomString& name, const String& features)
{
    if (!m_domWindow)
        return Exception { ExceptionCode::InvalidAccessError };

    return protectedWindow()->open(activeWindow, firstWindow, url, name, features);
}

ExceptionOr<Document&> Document::openForBindings(Document* entryDocument, const String&, const String&)
{
    if (!isHTMLDocument() || m_throwOnDynamicMarkupInsertionCount)
        return Exception { ExceptionCode::InvalidStateError };

    auto result = open(entryDocument);
    if (UNLIKELY(result.hasException()))
        return result.releaseException();

    return *this;
}

ExceptionOr<void> Document::open(Document* entryDocument)
{
    if (entryDocument && !entryDocument->securityOrigin().isSameOriginAs(securityOrigin()))
        return Exception { ExceptionCode::SecurityError };

    if (m_unloadCounter)
        return { };

    if (m_activeParserWasAborted)
        return { };

    if (RefPtr frame = this->frame()) {
        if (RefPtr parser = scriptableDocumentParser()) {
            if (parser->isParsing()) {
                // FIXME: HTML5 doesn't tell us to check this, it might not be correct.
                if (parser->isExecutingScript())
                    return { };

                if (!parser->wasCreatedByScript() && parser->hasInsertionPoint())
                    return { };
            }
        }

        bool isNavigating = frame->loader().policyChecker().delegateIsDecidingNavigationPolicy() || frame->loader().state() == FrameState::Provisional || frame->checkedNavigationScheduler()->hasQueuedNavigation();
        if (frame->loader().policyChecker().delegateIsDecidingNavigationPolicy())
            frame->loader().policyChecker().stopCheck();
        // Null-checking m_frame again as `policyChecker().stopCheck()` may have cleared it.
        if (isNavigating && m_frame)
            protectedFrame()->protectedLoader()->stopAllLoaders();
    }

    removeAllEventListeners();

    if (entryDocument && isFullyActive()) {
        auto newURL = entryDocument->url();
        if (entryDocument != this)
            newURL.removeFragmentIdentifier();
        setURL(newURL);
        auto newCookieURL = entryDocument->cookieURL();
        if (entryDocument != this)
            newCookieURL.removeFragmentIdentifier();
        setCookieURL(newCookieURL);
        setSecurityOriginPolicy(entryDocument->securityOriginPolicy());
    }

    implicitOpen();
    if (RefPtr parser = scriptableDocumentParser())
        parser->setWasCreatedByScript(true);

    if (RefPtr frame = this->frame()) {
        // Set document's is initial about:blank to false.
        // https://html.spec.whatwg.org/multipage/dynamic-markup-insertion.html#document-open-steps (step 13)
        Ref frameLoader = frame->loader();
        frameLoader->advanceStatePastInitialEmptyDocument();
        frameLoader->didExplicitOpen();
    }

    return { };
}

// https://html.spec.whatwg.org/#fully-active
bool Document::isFullyActive() const
{
    RefPtr frame = this->frame();
    if (!frame || frame->document() != this)
        return false;

    RefPtr parentFrame = dynamicDowncast<LocalFrame>(frame->tree().parent());
    if (!parentFrame)
        return true;
    return parentFrame->document() && parentFrame->document()->isFullyActive();
}

void Document::detachParser()
{
    if (RefPtr parser = std::exchange(m_parser, nullptr))
        parser->detach();
}

void Document::cancelParsing()
{
    if (!m_parser)
        return;

    if (protectedParser()->processingData())
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
        protectedParser()->didBeginYieldingParser();

    setParsing(true);
    setReadyState(ReadyState::Loading);
}

std::unique_ptr<FontLoadRequest> Document::fontLoadRequest(const String& url, bool isSVG, bool isInitiatingElementInUserAgentShadowTree, LoadedFromOpaqueSource loadedFromOpaqueSource)
{
    CachedResourceHandle cachedFont = fontLoader().cachedFont(completeURL(url), isSVG, isInitiatingElementInUserAgentShadowTree, loadedFromOpaqueSource);
    return cachedFont ? makeUnique<CachedFontLoadRequest>(*cachedFont, *this) : nullptr;
}

void Document::beginLoadingFontSoon(FontLoadRequest& request)
{
    CachedResourceHandle font = downcast<CachedFontLoadRequest>(request).cachedFont();
    fontLoader().beginLoadingFontSoon(*font);
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
        return Exception { ExceptionCode::HierarchyRequestError };

    RefPtr currentBody = bodyOrFrameset();
    if (newBody == currentBody)
        return { };

    if (!m_documentElement)
        return Exception { ExceptionCode::HierarchyRequestError };

    if (currentBody)
        return protectedDocumentElement()->replaceChild(*newBody, *currentBody);
    return protectedDocumentElement()->appendChild(*newBody);
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

RefPtr<HTMLHeadElement> Document::protectedHead()
{
    return head();
}

ExceptionOr<void> Document::closeForBindings()
{
    // FIXME: We should follow the specification more closely:
    //        http://www.whatwg.org/specs/web-apps/current-work/#dom-document-close

    if (!isHTMLDocument() || m_throwOnDynamicMarkupInsertionCount)
        return Exception { ExceptionCode::InvalidStateError };

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
    if (RefPtr parser = m_parser)
        parser->finish();

    if (!m_frame) {
        // Because we have no frame, we don't know if all loading has completed,
        // so we just call implicitClose() immediately. FIXME: This might fire
        // the load event prematurely <http://bugs.webkit.org/show_bug.cgi?id=14568>.
        setReadyState(ReadyState::Complete);
        implicitClose();
        return;
    }

    checkCompleted();
}

void Document::implicitClose()
{
    RELEASE_ASSERT(!m_inStyleRecalc);
    bool wasLocationChangePending = frame() && frame()->checkedNavigationScheduler()->locationChangePending();
    bool doload = !parsing() && m_parser && !m_processingLoadEvent && !wasLocationChangePending;

    if (!doload)
        return;

    // Call to dispatchWindowLoadEvent can blow us from underneath.
    Ref protectedThis { *this };

    m_processingLoadEvent = true;

    RefPtr parser = scriptableDocumentParser();
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
    RefPtr frame = this->frame();
    if (frame) {
#if ENABLE(XSLT)
        // Apply XSL transforms before load events so that event handlers can access the transformed DOM tree.
        applyPendingXSLTransformsNowIfScheduled();
#endif

        if (RefPtr documentLoader = loader())
            documentLoader->startIconLoading();

        // FIXME: We shouldn't be dispatching pending events globally on all Documents here.
        // For now, only do this when there is a Frame, otherwise this could cause JS reentrancy
        // below SVG font parsing, for example. <https://webkit.org/b/136269>
        if (RefPtr currentPage = page()) {
            ImageLoader::dispatchPendingLoadEvents(currentPage.get());
            HTMLLinkElement::dispatchPendingLoadEvents(currentPage.get());
            HTMLStyleElement::dispatchPendingLoadEvents(currentPage.get());
        }

        if (CheckedPtr svgExtensions = svgExtensionsIfExists())
            svgExtensions->dispatchLoadEventToOutermostSVGElements();
    }

    dispatchWindowLoadEvent();
    dispatchPageshowEvent(PageshowEventPersistence::NotPersisted);

    if (frame)
        frame->protectedLoader()->dispatchOnloadEvents();

    // An event handler may have removed the frame
    frame = this->frame();
    if (!frame) {
        m_processingLoadEvent = false;
        return;
    }

    frame->protectedLoader()->checkCallImplicitClose();

    // We used to force a synchronous display and flush here. This really isn't
    // necessary and can in fact be actively harmful if pages are loading at a rate of > 60fps
    // (if your platform is syncing flushes and limiting them to 60fps).
    if (!ownerElement() || (ownerElement()->renderer() && !ownerElement()->renderer()->needsLayout())) {
        updateStyleIfNeeded();

        // Always do a layout after loading if needed.
        if (view() && renderView() && (!renderView()->firstChild() || renderView()->needsLayout())) {
            protectedView()->layoutContext().layout();
            protectedView()->updateCompositingLayersAfterLayoutIfNeeded();
        }
    }

    m_processingLoadEvent = false;

    if (RefPtr fontSelector = fontSelectorIfExists()) {
        if (RefPtr fontFaceSet = fontSelector->fontFaceSetIfExists())
            fontFaceSet->documentDidFinishLoading();
    }

#if PLATFORM(COCOA) || PLATFORM(WIN) || PLATFORM(GTK)
    if (frame && hasLivingRenderTree() && AXObjectCache::accessibilityEnabled()) {
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

    if (CheckedPtr svgExtensions = svgExtensionsIfExists())
        svgExtensions->startAnimations();
}

void Document::setParsing(bool b)
{
    m_bParsing = b;

    if (m_bParsing && !m_sharedObjectPool)
        m_sharedObjectPool = makeUnique<DocumentSharedObjectPool>();

    if (!m_bParsing && view() && !view()->needsLayout())
        protectedView()->fireLayoutRelatedMilestonesIfNeeded();
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

bool Document::isLayoutPending() const
{
    return view() && view()->layoutContext().isLayoutPending();
}

bool Document::supportsPaintTiming() const
{
    return protectedSecurityOrigin()->isSameOriginDomain(topOrigin());
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

    if (!view()->hasContentfulDescendants())
        return;

    if (!ContentfulPaintChecker::qualifiesForContentfulPaint(*view()))
        return;

    WTFEmitSignpost(this, NavigationAndPaintTiming, "firstContentfulPaint");

    protectedWindow()->performance().reportFirstContentfulPaint();
    m_didEnqueueFirstContentfulPaint = true;
}

ExceptionOr<void> Document::write(Document* entryDocument, SegmentedString&& text)
{
    if (!isHTMLDocument() || m_throwOnDynamicMarkupInsertionCount)
        return Exception { ExceptionCode::InvalidStateError };

    if (m_activeParserWasAborted)
        return { };

    NestingLevelIncrementer nestingLevelIncrementer(m_writeRecursionDepth);

    m_writeRecursionIsTooDeep = (m_writeRecursionDepth > 1) && m_writeRecursionIsTooDeep;
    m_writeRecursionIsTooDeep = (m_writeRecursionDepth > cMaxWriteRecursionDepth) || m_writeRecursionIsTooDeep;

    if (m_writeRecursionIsTooDeep)
        return { };

    bool hasInsertionPoint = m_parser && m_parser->hasInsertionPoint();
    if (!hasInsertionPoint && (m_unloadCounter || m_ignoreDestructiveWriteCount))
        return { };

    if (!hasInsertionPoint) {
        auto result = open(entryDocument);
        if (UNLIKELY(result.hasException()))
            return result.releaseException();
    }

    ASSERT(m_parser);
    protectedParser()->insert(WTFMove(text));
    return { };
}

ExceptionOr<void> Document::write(Document* entryDocument, FixedVector<std::variant<RefPtr<TrustedHTML>, String>>&& strings, ASCIILiteral lineFeed)
{
    auto isTrusted = true;
    SegmentedString text;
    for (auto& entry : strings) {
        text.append(WTF::switchOn(WTFMove(entry),
            [&isTrusted](const String& string) {
                isTrusted = false;
                return string;
            },
            [](const RefPtr<TrustedHTML>& html) {
                return html->toString();
            }
        ));
    }

    if (isTrusted || !scriptExecutionContext()->settingsValues().trustedTypesEnabled) {
        text.append(lineFeed);
        return write(entryDocument, WTFMove(text));
    }

    String textString = text.toString();
    auto stringValueHolder = trustedTypeCompliantString(TrustedType::TrustedHTML, *scriptExecutionContext(), textString, lineFeed.isEmpty() ? "Document write"_s : "Document writeln"_s);
    if (stringValueHolder.hasException())
        return stringValueHolder.releaseException();
    SegmentedString trustedText(stringValueHolder.releaseReturnValue());
    trustedText.append(lineFeed);
    return write(entryDocument, WTFMove(trustedText));
}

ExceptionOr<void> Document::write(Document* entryDocument, FixedVector<std::variant<RefPtr<TrustedHTML>, String>>&& strings)
{
    return write(entryDocument, WTFMove(strings), ""_s);
}

ExceptionOr<void> Document::write(Document* entryDocument, FixedVector<String>&& strings)
{
    SegmentedString text;
    for (auto& string : strings)
        text.append(WTFMove(string));
    return write(entryDocument, WTFMove(text));
}

ExceptionOr<void> Document::writeln(Document* entryDocument, FixedVector<std::variant<RefPtr<TrustedHTML>, String>>&& strings)
{
    return write(entryDocument, WTFMove(strings), "\n"_s);
}

ExceptionOr<void> Document::writeln(Document* entryDocument, FixedVector<String>&& strings)
{
    SegmentedString text;
    for (auto& string : strings)
        text.append(WTFMove(string));
    text.append("\n"_s);
    return write(entryDocument, WTFMove(text));
}

Seconds Document::minimumDOMTimerInterval() const
{
    RefPtr page = this->page();
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

// https://html.spec.whatwg.org/#updating-the-document:fire-a-page-transition-event
void Document::clearRevealForReactivation()
{
    m_hasBeenRevealed = false;
}

void Document::setVisibilityHiddenDueToDismissal(bool hiddenDueToDismissal)
{
    if (m_visibilityHiddenDueToDismissal == hiddenDueToDismissal)
        return;

    m_visibilityHiddenDueToDismissal = hiddenDueToDismissal;
    dispatchEvent(Event::create(eventNames().visibilitychangeEvent, Event::CanBubble::Yes, Event::IsCancelable::No));
}

Seconds Document::domTimerAlignmentInterval(bool hasReachedMaxNestingLevel) const
{
    auto alignmentInterval = ScriptExecutionContext::domTimerAlignmentInterval(hasReachedMaxNestingLevel);
    if (!hasReachedMaxNestingLevel)
        return alignmentInterval;

    // Apply Document-level DOMTimer throttling only if timers have reached their maximum nesting level as the Page may still be visible.
    if (m_isTimerThrottlingEnabled)
        alignmentInterval = std::max(alignmentInterval, DOMTimer::hiddenPageAlignmentInterval());

    if (RefPtr page = this->page())
        alignmentInterval = std::max(alignmentInterval, page->domTimerAlignmentInterval());

    if (!topOrigin().isSameOriginDomain(securityOrigin()) && !hasHadUserInteraction())
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

String Document::documentURI() const
{
    return switchOn(m_documentURI,
        [](const String& string) { return string; },
        [](const URL& url) { return url.string(); }
    );
}

void Document::setURL(const URL& url)
{
    URL newURL = url.isEmpty() ? aboutBlankURL() : url;
    if (newURL == m_url)
        return;

    if (RefPtr page = protectedPage())
        m_fragmentDirective = page->mainFrameURLFragment();

    if (m_fragmentDirective.isEmpty())
        m_fragmentDirective = newURL.consumeFragmentDirective();

    if (SecurityOrigin::shouldIgnoreHost(newURL))
        newURL.removeHostAndPort();
    // SecurityContext::securityOrigin may not be initialized at this time if setURL() is called in the constructor, therefore calling topOrigin() is not always safe.
    auto topOrigin = isTopDocument() && !SecurityContext::securityOrigin() ? SecurityOrigin::create(url)->data() : this->topOrigin().data();
    m_url = { WTFMove(newURL), topOrigin };
    if (m_frame)
        m_frame->documentURLDidChange(m_url);

    m_documentURI = m_url.url();
    m_adjustedURL = adjustedURL();
    updateBaseURL();
}

const URL& Document::urlForBindings()
{
    auto shouldAdjustURL = [this] {
        if (m_url.url().isEmpty() || !loader() || !isTopDocument() || !frame())
            return false;

        RefPtr policySourceLoader = topDocument().loader();
        if (policySourceLoader && !policySourceLoader->request().url().hasSpecialScheme() && url().protocolIsInHTTPFamily())
            policySourceLoader = loader();

        if (!policySourceLoader)
            return false;

        auto navigationalProtections = policySourceLoader->navigationalAdvancedPrivacyProtections();
        if (navigationalProtections.isEmpty())
            return false;

        auto preNavigationURL = URL { loader()->originalRequest().httpReferrer() };
        if (preNavigationURL.isEmpty() || RegistrableDomain { preNavigationURL }.matches(securityOrigin().data())) {
            // Only apply the protections below following a cross-origin navigation.
            return false;
        }

        auto areSameSiteIgnoringPublicSuffix = [](StringView domain, StringView otherDomain) {
            auto& publicSuffixStore = PublicSuffixStore::singleton();
            auto domainString = publicSuffixStore.topPrivatelyControlledDomain(domain);
            auto otherDomainString = publicSuffixStore.topPrivatelyControlledDomain(otherDomain);
            auto substringToSeparator = [](const String& string) -> String {
                auto indexOfFirstSeparator = string.find('.');
                if (indexOfFirstSeparator == notFound)
                    return { };
                return string.left(indexOfFirstSeparator);
            };
            auto firstSubstring = substringToSeparator(domainString);
            return !firstSubstring.isEmpty() && firstSubstring == substringToSeparator(otherDomainString);
        };

        auto shouldApplyBaselineProtections = [&] {
            if (!navigationalProtections.contains(AdvancedPrivacyProtections::BaselineProtections))
                return false;

            auto currentHost = securityOrigin().data().host();
            if (areSameSiteIgnoringPublicSuffix(preNavigationURL.host(), currentHost))
                return false;

            if (!m_hasLoadedThirdPartyScript)
                return false;

            if (auto sourceURL = currentSourceURL(); !sourceURL.isEmpty()) {
                if (RegistrableDomain { sourceURL }.matches(securityOrigin().data()))
                    return false;

                if (areSameSiteIgnoringPublicSuffix(sourceURL.host(), currentHost))
                    return false;
            }

            return true;
        };

        auto shouldApplyEnhancedProtections = [&] {
            if (!navigationalProtections.contains(AdvancedPrivacyProtections::ScriptTelemetry))
                return false;

            if (!requiresScriptExecutionTelemetry(ScriptTelemetryCategory::QueryParameters))
                return false;

            return true;
        };

        return shouldApplyBaselineProtections() || shouldApplyEnhancedProtections();
    }();

    if (shouldAdjustURL)
        return m_adjustedURL;

    return m_url.url().isEmpty() ? aboutBlankURL() : m_url.url();
}

URL Document::adjustedURL() const
{
    return page() ? page()->chrome().client().allowedQueryParametersForAdvancedPrivacyProtections(m_url.url()) : m_url.url();
}

// https://html.spec.whatwg.org/#fallback-base-url
URL Document::fallbackBaseURL() const
{
    // The documentURI attribute is read-only from JavaScript, but writable from Objective C, so we need to retain
    // this fallback behavior. We use a null base URL, since the documentURI attribute is an arbitrary string
    // and DOM 3 Core does not specify how it should be resolved.
    auto documentURL = switchOn(m_documentURI,
        [](const String& string) { return URL({ }, string); },
        [](const URL& url) { return url; }
    );

    if (documentURL.isAboutSrcDoc()) {
        if (auto* parent = parentDocument())
            return parent->baseURL();
    }

    if (documentURL.isAboutBlank()) {
        RefPtr creator = parentDocument();
        if (!creator && frame()) {
            if (RefPtr localOpener = dynamicDowncast<LocalFrame>(frame()->opener()))
                creator = localOpener->document();
        }
        if (creator)
            return creator->baseURL();
    }

    return documentURL;
}

void Document::updateBaseURL()
{
    // DOM 3 Core: When the Document supports the feature "HTML" [DOM Level 2 HTML], the base URI is computed using
    // first the value of the href attribute of the HTML BASE element if any, and the value of the documentURI attribute
    // from the Document interface otherwise.
    if (!m_baseElementURL.isEmpty())
        m_baseURL = m_baseElementURL;
    else if (!m_baseURLOverride.isEmpty())
        m_baseURL = m_baseURLOverride;
    else
        m_baseURL = fallbackBaseURL();

    if (!m_baseURL.isValid())
        m_baseURL = URL();

    invalidateCachedCSSParserContext();
}

void Document::setBaseURLOverride(const URL& url)
{
    m_baseURLOverride = url;
    updateBaseURL();
}

HTMLBaseElement* Document::firstBaseElement() const
{
    return m_firstBaseElement.get();
}

void Document::processBaseElement()
{
    // Find the first href attribute in a base element and the first target attribute in a base element.
    AtomString href;
    AtomString target;
    RefPtr<HTMLBaseElement> baseElement;
    auto baseDescendants = descendantsOfType<HTMLBaseElement>(*this);
    for (auto& base : baseDescendants) {
        if (!baseElement)
            baseElement = &base;

        if (href.isNull()) {
            auto& value = base.attributeWithoutSynchronization(hrefAttr);
            if (!value.isNull()) {
                href = value;
                if (!target.isNull())
                    break;
            }
        }
        if (target.isNull()) {
            auto& value = base.attributeWithoutSynchronization(targetAttr);
            if (!value.isNull()) {
                target = value;
                if (!href.isNull())
                    break;
            }
        }
    }

    URL baseElementURL;
    if (!href.isNull())
        baseElementURL = completeURL(href, fallbackBaseURL());
    if (m_baseElementURL != baseElementURL) {
        if (!checkedContentSecurityPolicy()->allowBaseURI(baseElementURL))
            m_baseElementURL = { };
        else if (settings().shouldRestrictBaseURLSchemes() && !SecurityPolicy::isBaseURLSchemeAllowed(baseElementURL)) {
            m_baseElementURL = { };
            addConsoleMessage(MessageSource::Security, MessageLevel::Error, makeString("Blocked setting "_s, baseElementURL.stringCenterEllipsizedToLength(), " as the base URL because it does not have an allowed scheme."_s));
        } else
            m_baseElementURL = WTFMove(baseElementURL);
        updateBaseURL();
    }

    m_baseTarget = WTFMove(target);
    m_firstBaseElement = WTFMove(baseElement);
}

String Document::userAgent(const URL& url) const
{
    RefPtr frame = this->frame();
    return frame ? frame->protectedLoader()->userAgent(url) : String();
}

void Document::disableEval(const String& errorMessage)
{
    RefPtr frame = this->frame();
    if (!frame)
        return;

    frame->checkedScript()->setEvalEnabled(false, errorMessage);
}

void Document::disableWebAssembly(const String& errorMessage)
{
    RefPtr frame = this->frame();
    if (!frame)
        return;

    frame->checkedScript()->setWebAssemblyEnabled(false, errorMessage);
}

void Document::setRequiresTrustedTypes(bool required)
{
    RefPtr frame = this->frame();
    if (!frame)
        return;

    frame->checkedScript()->setRequiresTrustedTypes(required);
}

IDBClient::IDBConnectionProxy* Document::idbConnectionProxy()
{
    if (!m_idbConnectionProxy) {
        RefPtr currentPage = page();
        if (!currentPage)
            return nullptr;
        m_idbConnectionProxy = &currentPage->idbConnection().proxy();
    }
    return m_idbConnectionProxy.get();
}

StorageConnection* Document::storageConnection()
{
    return page() ? &page()->storageConnection() : nullptr;
}

SocketProvider* Document::socketProvider()
{
    return m_socketProvider.get();
}

RefPtr<RTCDataChannelRemoteHandlerConnection> Document::createRTCDataChannelRemoteHandlerConnection()
{
    ASSERT(isMainThread());
    RefPtr page = this->page();
    if (!page)
        return nullptr;
    return page->webRTCProvider().createRTCDataChannelRemoteHandlerConnection();
}

#if ENABLE(WEB_RTC)
void Document::setRTCNetworkManager(Ref<RTCNetworkManager>&& rtcNetworkManager)
{
    m_rtcNetworkManager = WTFMove(rtcNetworkManager);
}

void Document::startGatheringRTCLogs(RTCController::LogCallback&& callback)
{
    if (RefPtr page = this->page())
        page->rtcController().startGatheringLogs(*this, WTFMove(callback));
}

void Document::stopGatheringRTCLogs()
{
    if (RefPtr page = this->page())
        page->rtcController().stopGatheringLogs();
}
#endif

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
        printNavigationErrorMessage(*this, *targetFrame, url(), "The frame attempting navigation of the top-level window is cross-origin or untrusted and the user has never interacted with the frame."_s);
        DOCUMENT_RELEASE_LOG_ERROR(Loading, "Navigation was prevented because it was triggered by a cross-origin or untrusted iframe");
        return false;
    }

    return true;
}

bool Document::canNavigateInternal(Frame& targetFrame)
{
    ASSERT(m_frame);

    // Cases (i), (ii) and (iii) pass the tests from the specifications but might not pass the "security origin" tests.

    // i. A frame can navigate its top ancestor when its 'allow-top-navigation' flag is set (sometimes known as 'frame-busting').
    if (!isSandboxed(SandboxFlag::TopNavigation) && &targetFrame == &m_frame->tree().top())
        return true;

    // The user gesture only relaxes permissions for the purpose of navigating if its impacts the current document.
    bool isProcessingUserGestureForDocument = UserGestureIndicator::processingUserGesture(m_frame->document());

    // ii. A frame can navigate its top ancestor when its 'allow-top-navigation-by-user-activation' flag is set and navigation is triggered by user activation.
    if (!isSandboxed(SandboxFlag::TopNavigationByUserActivation) && isProcessingUserGestureForDocument && &targetFrame == &m_frame->tree().top())
        return true;

    // iii. A sandboxed frame can always navigate its descendants.
    if (isSandboxed(SandboxFlag::Navigation) && targetFrame.tree().isDescendantOf(m_frame.get()))
        return true;

    // From https://html.spec.whatwg.org/multipage/browsers.html#allowed-to-navigate.
    // 1. If A is not the same browsing context as B, and A is not one of the ancestor browsing contexts of B, and B is not a top-level browsing context, and A's active document's active sandboxing
    // flag set has its sandboxed navigation browsing context flag set, then abort these steps negatively.
    if (m_frame != &targetFrame && isSandboxed(SandboxFlag::Navigation) && targetFrame.tree().parent() && !targetFrame.tree().isDescendantOf(m_frame.get())) {
        printNavigationErrorMessage(*this, targetFrame, url(), "The frame attempting navigation is sandboxed, and is therefore disallowed from navigating its ancestors."_s);
        return false;
    }

    // 2. Otherwise, if B is a top-level browsing context, and is one of the ancestor browsing contexts of A, then:
    if (m_frame != &targetFrame && &targetFrame == &m_frame->tree().top()) {
        // 1. If this algorithm is triggered by user activation and A's active document's active sandboxing flag set has its sandboxed top-level navigation with user activation browsing context flag set, then abort these steps negatively.
        if (isProcessingUserGestureForDocument && isSandboxed(SandboxFlag::TopNavigationByUserActivation)) {
            printNavigationErrorMessage(*this, targetFrame, url(), "The frame attempting navigation of the top-level window is sandboxed, but the 'allow-top-navigation-by-user-activation' flag is not set and navigation is not triggered by user activation."_s);
            return false;
        }
        // 2. Otherwise, If this algorithm is not triggered by user activation and A's active document's active sandboxing flag set has its sandboxed top-level navigation without user activation browsing context flag set, then abort these steps negatively.
        if (!isProcessingUserGestureForDocument && isSandboxed(SandboxFlag::TopNavigation)) {
            printNavigationErrorMessage(*this, targetFrame, url(), "The frame attempting navigation of the top-level window is sandboxed, but the 'allow-top-navigation' flag is not set."_s);
            return false;
        }
    }

    // 3. Otherwise, if B is a top-level browsing context, and is neither A nor one of the ancestor browsing contexts of A, and A's Document's active sandboxing flag set has its
    // sandboxed navigation browsing context flag set, and A is not the one permitted sandboxed navigator of B, then abort these steps negatively.
    if (!targetFrame.tree().parent() && m_frame != &targetFrame && &targetFrame != &m_frame->tree().top() && isSandboxed(SandboxFlag::Navigation) && targetFrame.opener() != m_frame) {
        printNavigationErrorMessage(*this, targetFrame, url(), "The frame attempting navigation is sandboxed, and is not allowed to navigate this popup."_s);
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
        if (&targetFrame == m_frame->opener())
            return true;

        if (RefPtr localOpener = dynamicDowncast<LocalFrame>(targetFrame.opener())) {
            if (canAccessAncestor(securityOrigin(), localOpener.get()))
                return true;
        }
    }

    printNavigationErrorMessage(*this, targetFrame, url(), "The frame attempting navigation is neither same-origin with the target, nor is it the target's parent or opener."_s);
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
    // Only prevent top frame navigations by subframes.
    if (m_frame == &targetFrame || &targetFrame != &m_frame->tree().top())
        return false;

    // Only prevent navigations by subframes that the user has not interacted with.
    if (m_frame->hasHadUserInteraction())
        return false;

    // Only prevent navigations by unsandboxed iframes. Such navigations by sandboxed iframes would have already been blocked unless
    // "allow-top-navigation" / "allow-top-navigation-by-user-activation" was explicitly specified.
    // We also want to guard against bypassing this block via an iframe-provided CSP sandbox.
    RefPtr ownerElement = m_frame->ownerElement();
    if ((!ownerElement || ownerElement->sandboxFlags() == sandboxFlags()) && !sandboxFlags().isEmpty()) {
        // Navigation is only allowed if the parent of the sandboxed iframe is first-party.
        RefPtr parentFrame = dynamicDowncast<LocalFrame>(m_frame->tree().parent());
        RefPtr parentDocument = parentFrame ? parentFrame->document() : nullptr;
        if (parentDocument && canAccessAncestor(parentDocument->securityOrigin(), &targetFrame))
            return false;
    }

    // Only prevent navigations by third-party iframes or untrusted first-party iframes.
    bool isUntrustedIframe = m_hasLoadedThirdPartyScript && m_hasLoadedThirdPartyFrame;
    if (canAccessAncestor(securityOrigin(), &targetFrame) && !isUntrustedIframe)
        return false;

    // Only prevent cross-site navigations.
    RefPtr targetLocalFrame = dynamicDowncast<LocalFrame>(targetFrame);
    if (!targetLocalFrame)
        return true;
    RefPtr targetDocument = targetLocalFrame->document();
    if (!targetDocument)
        return true;

    if (targetDocument->securityOrigin().protocol() != destinationURL.protocol())
        return true;

    return !(targetDocument->protectedSecurityOrigin()->isSameOriginDomain(SecurityOrigin::create(destinationURL)) || areRegistrableDomainsEqual(targetDocument->url(), destinationURL));
}

void Document::didRemoveAllPendingStylesheet()
{
    if (RefPtr parser = scriptableDocumentParser())
        parser->executeScriptsWaitingForStylesheetsSoon();

    if (m_gotoAnchorNeededAfterStylesheetsLoad && view()) {
        // https://html.spec.whatwg.org/multipage/browsing-the-web.html#try-to-scroll-to-the-fragment
        eventLoop().queueTask(TaskSource::Networking, [protectedThis = Ref { *this }, this] {
            RefPtr frameView = view();
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

void Document::processMetaHttpEquiv(const String& equiv, const AtomString& content, bool isInDocumentHead)
{
    ASSERT(!equiv.isNull());
    ASSERT(!content.isNull());

    HttpEquivPolicy policy = httpEquivPolicy();
    if (policy != HttpEquivPolicy::Enabled) {
        ASCIILiteral reason = ""_s;
        switch (policy) {
        case HttpEquivPolicy::Enabled:
            ASSERT_NOT_REACHED();
            break;
        case HttpEquivPolicy::DisabledBySettings:
            reason = "by the embedder."_s;
            break;
        case HttpEquivPolicy::DisabledByContentDispositionAttachmentSandbox:
            reason = "for documents with Content-Disposition: attachment."_s;
            break;
        }
        String message = makeString("http-equiv '"_s, equiv, "' is disabled "_s, reason);
        addConsoleMessage(MessageSource::Security, MessageLevel::Error, message);
        return;
    }

    RefPtr frame = this->frame();
    RefPtr documentLoader = frame ? frame->loader().documentLoader() : nullptr;
    auto httpStatusCode = documentLoader ? documentLoader->response().httpStatusCode() : 0;
    auto responseURL = documentLoader ? documentLoader->response().url() : URL();

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

    case HTTPHeaderName::Refresh:
        if (frame)
            frame->protectedLoader()->scheduleRefreshIfNeeded(*this, content, IsMetaRefresh::Yes);
        break;

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
            Ref frameLoader = frame->loader();
            std::optional<ResourceLoaderIdentifier> requestIdentifier;
            if (frameLoader->activeDocumentLoader() && frameLoader->activeDocumentLoader()->mainResourceLoader())
                requestIdentifier = frameLoader->activeDocumentLoader()->mainResourceLoader()->identifier();

            auto message = makeString("The X-Frame-Option '"_s, content, "' supplied in a <meta> element was ignored. X-Frame-Options may only be provided by an HTTP header sent with the document."_s);
            addConsoleMessage(MessageSource::Security, MessageLevel::Error, message, requestIdentifier ? requestIdentifier->toUInt64() : 0);
        }
        break;

    case HTTPHeaderName::ContentSecurityPolicy:
        if (isInDocumentHead)
            checkedContentSecurityPolicy()->didReceiveHeader(content, ContentSecurityPolicyHeaderType::Enforce, ContentSecurityPolicy::PolicyFrom::HTTPEquivMeta, referrer(), httpStatusCode);
        break;

    case HTTPHeaderName::ReportingEndpoints:
        reportingScope().parseReportingEndpoints(content, responseURL);
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

    RefPtr page = this->page();
    if (!page)
        return;

    page->chrome().dispatchDisabledAdaptationsDidChange(m_disabledAdaptations);
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
    RefPtr page = this->page();
    if (!page)
        return m_viewportArguments;
    return page->overrideViewportArguments().value_or(m_viewportArguments);
}

bool Document::isViewportDocument() const
{
    RefPtr page = this->page();
    if (!page)
        return false;

#if ENABLE(FULLSCREEN_API)
    if (RefPtr outermostFullscreenDocument = page->outermostFullscreenDocument())
        return outermostFullscreenDocument == this;
#endif

    if (RefPtr frame = this->frame())
        return frame->isMainFrame();

    return false;
}

void Document::updateViewportArguments()
{
    RefPtr page = this->page();
    if (!page)
        return;

    if (!isViewportDocument())
        return;

#if ASSERT_ENABLED
    m_didDispatchViewportPropertiesChanged = true;
#endif
    page->chrome().dispatchViewportPropertiesDidChange(viewportArguments());
    page->chrome().didReceiveDocType(protectedFrame().releaseNonNull());
}

void Document::metaElementThemeColorChanged(HTMLMetaElement& metaElement)
{
    // If the current content color isn't valid and it wasn't previously in the list of elements
    // with a valid content color, don't bother recalculating `m_metaThemeColorElements`.
    if (!metaElement.contentColor().isValid() && m_metaThemeColorElements && !m_metaThemeColorElements->contains(&metaElement))
        return;

    auto oldThemeColor = std::exchange(m_cachedThemeColor, Color());
    m_metaThemeColorElements = std::nullopt;
    m_activeThemeColorMetaElement = nullptr;
    if (themeColor() == oldThemeColor)
        return;

    themeColorChanged();
}

WeakPtr<HTMLMetaElement, WeakPtrImplWithEventTargetData> Document::determineActiveThemeColorMetaElement()
{
    if (!m_metaThemeColorElements) {
        Vector<WeakPtr<HTMLMetaElement, WeakPtrImplWithEventTargetData>> metaThemeColorElements;
        for (auto& metaElement : descendantsOfType<HTMLMetaElement>(*this)) {
            if (equalLettersIgnoringASCIICase(metaElement.name(), "theme-color"_s) && metaElement.contentColor().isValid())
                metaThemeColorElements.append(metaElement);
        }
        m_metaThemeColorElements = WTFMove(metaThemeColorElements);
    }

    for (auto& metaElement : *m_metaThemeColorElements) {
        if (metaElement && metaElement->contentColor().isValid() && metaElement->mediaAttributeMatches())
            return metaElement;
    }
    return nullptr;
}

void Document::themeColorChanged()
{
    scheduleRenderingUpdate({ });

    if (RefPtr page = this->page())
        page->chrome().client().themeColorChanged();
}

#if ENABLE(DARK_MODE_CSS)
static void processColorSchemeString(StringView colorScheme, const Function<void(StringView key)>& callback)
{
    unsigned length = colorScheme.length();
    for (unsigned i = 0; i < length; ) {
        // Skip to first non-separator.
        while (i < length && isASCIIWhitespace(colorScheme[i]))
            ++i;
        unsigned keyBegin = i;

        // Skip to first separator.
        while (i < length && !isASCIIWhitespace(colorScheme[i]))
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
        if (equalLettersIgnoringASCIICase(key, "auto"_s)) {
            colorScheme = { };
            allowsTransformations = true;
            autoEncountered = true;
            return;
        }

        if (autoEncountered)
            return;

        if (equalLettersIgnoringASCIICase(key, "light"_s))
            colorScheme.add(ColorScheme::Light);
        else if (equalLettersIgnoringASCIICase(key, "dark"_s))
            colorScheme.add(ColorScheme::Dark);
        else if (equalLettersIgnoringASCIICase(key, "only"_s))
            allowsTransformations = false;
    });

    // If the value was just "only", that is synonymous for "only light".
    if (colorScheme.isEmpty() && !allowsTransformations)
        colorScheme.add(ColorScheme::Light);

    m_colorScheme = colorScheme;
    m_allowsColorSchemeTransformations = allowsTransformations;

    if (RefPtr frameView = view())
        frameView->recalculateBaseBackgroundColor();

    if (RefPtr page = this->page())
        page->updateStyleAfterChangeInEnvironment();
}

void Document::metaElementColorSchemeChanged()
{
    RefPtr<CSSValue> colorScheme;
    auto colorSchemeString = emptyString();
    auto cssParserContext = CSSParserContext(document());
    for (auto& metaElement : descendantsOfType<HTMLMetaElement>(rootNode())) {
        const AtomString& nameValue = metaElement.attributeWithoutSynchronization(nameAttr);
        if ((equalLettersIgnoringASCIICase(nameValue, "color-scheme"_s) || equalLettersIgnoringASCIICase(nameValue, "supported-color-schemes"_s)) && (colorScheme = CSSParser::parseSingleValue(CSSPropertyColorScheme, metaElement.attributeWithoutSynchronization(contentAttr), cssParserContext))) {
            colorSchemeString = colorScheme->cssText();
            break;
        }
    }
    processColorScheme(colorSchemeString);
}
#endif

#if PLATFORM(IOS_FAMILY)

void Document::processFormatDetection(const String& features)
{
    // FIXME: Find a better place for this function.
    processFeaturesString(features, FeatureMode::Viewport, [this](StringView key, StringView value) {
        if (equalLettersIgnoringASCIICase(key, "telephone"_s) && equalLettersIgnoringASCIICase(value, "no"_s))
            m_isTelephoneNumberParsingAllowed = false;
    });
}

void Document::processWebAppOrientations()
{
    if (RefPtr page = this->page())
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
        addConsoleMessage(MessageSource::Rendering, MessageLevel::Error, makeString("Failed to set referrer policy: The value '"_s, policy, "' is not one of 'no-referrer', 'no-referrer-when-downgrade', 'same-origin', 'origin', 'strict-origin', 'origin-when-cross-origin', 'strict-origin-when-cross-origin' or 'unsafe-url'."_s));
        return;
    }
    setReferrerPolicy(referrerPolicy.value());
}

#if ENABLE(APPLICATION_MANIFEST)

void Document::processApplicationManifest(const ApplicationManifest& applicationManifest)
{
    auto oldThemeColor = std::exchange(m_cachedThemeColor, Color());
    m_applicationManifestThemeColor = applicationManifest.themeColor;
    if (themeColor() == oldThemeColor)
        return;

    themeColorChanged();
}

#endif // ENABLE(APPLICATION_MANIFEST)

MouseEventWithHitTestResults Document::prepareMouseEvent(const HitTestRequest& request, const LayoutPoint& documentPoint, const PlatformMouseEvent& event)
{
    if (!hasLivingRenderTree())
        return MouseEventWithHitTestResults(event, HitTestResult(LayoutPoint()));

    HitTestResult result(documentPoint);
    hitTest(request, result);

    auto captureElementChanged = CaptureChange::No;
    if (!request.readOnly()) {
        RefPtr targetElement = result.targetElement();
        if (RefPtr page = this->page()) {
            // Before we dispatch a new mouse event, we must run the Process Pending Capture Element steps as defined
            // in https://w3c.github.io/pointerevents/#process-pending-pointer-capture.
            auto& pointerCaptureController = page->pointerCaptureController();
            RefPtr previousCaptureElement = pointerCaptureController.pointerCaptureElement(this, event.pointerId());
            pointerCaptureController.processPendingPointerCapture(event.pointerId());
            RefPtr captureElement = pointerCaptureController.pointerCaptureElement(this, event.pointerId());
            // If the capture element has changed while running the Process Pending Capture Element steps then
            // we need to indicate that when calling updateHoverActiveState to be sure that the :active and :hover
            // element chains are updated.
            if (previousCaptureElement != captureElement)
                captureElementChanged = CaptureChange::Yes;
            // If we have a capture element, we must target it instead of what would normally hit-test for this event.
            if (captureElement)
                targetElement = WTFMove(captureElement);
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
        for (RefPtr node = uncheckedDowncast<DocumentFragment>(newChild).firstChild(); node; node = node->nextSibling()) {
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
        RefPtr existingDocType = childrenOfType<DocumentType>(*this).first();
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
    Ref clone = cloneDocumentWithoutChildren();
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
            return XMLDocument::createXHTML(nullptr, m_settings, url());
        return XMLDocument::create(nullptr, m_settings, url());
    }
    return create(m_settings, url());
}

void Document::cloneDataFromDocument(const Document& other)
{
    ASSERT(m_url == other.url());
    m_baseURL = other.baseURL();
    m_baseURLOverride = other.baseURLOverride();
    m_documentURI = other.m_documentURI;

    setCompatibilityMode(other.m_compatibilityMode);
    setContextDocument(other.protectedContextDocument());
    setSecurityOriginPolicy(other.securityOriginPolicy());
    overrideMIMEType(other.contentType());
    setDecoder(other.protectedDecoder());
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

    if (auto activeThemeColorElement = determineActiveThemeColorMetaElement(); m_activeThemeColorMetaElement != activeThemeColorElement) {
        auto oldThemeColor = std::exchange(m_cachedThemeColor, Color());
        m_activeThemeColorMetaElement = WTFMove(activeThemeColorElement);
        if (themeColor() != oldThemeColor)
            themeColorChanged();
    }

    for (auto& image : copyToVectorOf<Ref<HTMLImageElement>>(m_dynamicMediaQueryDependentImages))
        image->evaluateDynamicMediaQueryDependencies();
}

void Document::evaluateMediaQueriesAndReportChanges()
{
    if (!m_mediaQueryMatcher)
        return;

    m_mediaQueryMatcher->evaluateAll(MediaQueryMatcher::EventMode::DispatchNow);
}

void Document::updateViewportUnitsOnResize()
{
    styleScope().didChangeViewportSize();
}

void Document::setNeedsDOMWindowResizeEvent()
{
#if ENABLE(FULLSCREEN_API)
    if (CheckedPtr fullscreenManager = fullscreenManagerIfExists(); fullscreenManager && fullscreenManager->isAnimatingFullscreen()) {
        fullscreenManager->addPendingScheduledResize(FullscreenManager::ResizeType::DOMWindow);
        return;
    }
#endif

    m_needsDOMWindowResizeEvent = true;
    scheduleRenderingUpdate(RenderingUpdateStep::Resize);
}

void Document::setNeedsVisualViewportResize()
{
#if ENABLE(FULLSCREEN_API)
    if (CheckedPtr fullscreenManager = fullscreenManagerIfExists(); fullscreenManager && fullscreenManager->isAnimatingFullscreen()) {
        fullscreenManager->addPendingScheduledResize(FullscreenManager::ResizeType::VisualViewport);
        return;
    }
#endif

    m_needsVisualViewportResizeEvent = true;
    scheduleRenderingUpdate(RenderingUpdateStep::Resize);
}

// https://drafts.csswg.org/cssom-view/#run-the-resize-steps
void Document::runResizeSteps()
{
    // FIXME: The order of dispatching is not specified: https://github.com/WICG/visual-viewport/issues/65.
    if (m_needsDOMWindowResizeEvent) {
        LOG_WITH_STREAM(Events, stream << "Document " << this << " sending resize events to window");
        m_needsDOMWindowResizeEvent = false;
        dispatchWindowEvent(Event::create(eventNames().resizeEvent, Event::CanBubble::No, Event::IsCancelable::No));
    }
    if (m_needsVisualViewportResizeEvent) {
        LOG_WITH_STREAM(Events, stream << "Document " << this << " sending resize events to visualViewport");
        m_needsVisualViewportResizeEvent = false;
        if (RefPtr window = domWindow())
            window->visualViewport().dispatchEvent(Event::create(eventNames().resizeEvent, Event::CanBubble::No, Event::IsCancelable::No));
    }
}

void Document::addPendingScrollEventTarget(ContainerNode& target)
{
    if (!m_pendingScrollEventTargetList)
        m_pendingScrollEventTargetList = makeUnique<PendingScrollEventTargetList>();

    auto& targets = m_pendingScrollEventTargetList->targets;
    if (targets.findIf([&] (auto& entry) { return entry.ptr() == &target; }) != notFound)
        return;

    if (targets.isEmpty())
        scheduleRenderingUpdate(RenderingUpdateStep::Scroll);

    targets.append(target);
}

void Document::setNeedsVisualViewportScrollEvent()
{
    if (!m_needsVisualViewportScrollEvent)
        scheduleRenderingUpdate(RenderingUpdateStep::Scroll);
    m_needsVisualViewportScrollEvent = true;
}

static bool serviceScrollAnimationForScrollableArea(const ScrollableArea* scrollableArea, MonotonicTime time)
{
    if (!scrollableArea)
        return false;

    if (auto* animator = scrollableArea->existingScrollAnimator())
        return animator->serviceScrollAnimation(time) == ScrollAnimationStatus::Animating;

    return false;
}

// https://drafts.csswg.org/cssom-view/#run-the-scroll-steps
void Document::runScrollSteps()
{
    // Service user scroll animations before scroll event dispatch.
    if (RefPtr frameView = view()) {
        MonotonicTime now = MonotonicTime::now();
        bool scrollAnimationsInProgress = serviceScrollAnimationForScrollableArea(frameView.get(), now);
        HashSet<CheckedPtr<ScrollableArea>> scrollableAreasToUpdate;
        if (auto userScrollableAreas = frameView->scrollableAreas()) {
            for (auto& area : *userScrollableAreas)
                scrollableAreasToUpdate.add(CheckedPtr<ScrollableArea>(area));
        }

        if (auto nonUserScrollableAreas = frameView->scrollableAreasForAnimatedScroll()) {
            for (auto& area : *nonUserScrollableAreas)
                scrollableAreasToUpdate.add(CheckedPtr<ScrollableArea>(area));
        }
        for (auto& scrollableArea : scrollableAreasToUpdate) {
            if (serviceScrollAnimationForScrollableArea(scrollableArea.get(), now))
                scrollAnimationsInProgress = true;
        }
        if (scrollAnimationsInProgress)
            protectedPage()->scheduleRenderingUpdate({ RenderingUpdateStep::Scroll });

        frameView->updateScrollAnchoringElementsForScrollableAreas();
        frameView->updateScrollAnchoringPositionForScrollableAreas();
    }

    // FIXME: The order of dispatching is not specified: https://github.com/WICG/visual-viewport/issues/66.
    if (m_pendingScrollEventTargetList && !m_pendingScrollEventTargetList->targets.isEmpty()) {
        LOG_WITH_STREAM(Events, stream << "Document " << this << " sending scroll events to pending scroll event targets");
        auto currentTargets = WTFMove(m_pendingScrollEventTargetList->targets);
        for (auto& target : currentTargets) {
            auto bubbles = target->isDocumentNode() ? Event::CanBubble::Yes : Event::CanBubble::No;
            target->dispatchEvent(Event::create(eventNames().scrollEvent, bubbles, Event::IsCancelable::No));
        }
    }
    if (m_needsVisualViewportScrollEvent) {
        LOG_WITH_STREAM(Events, stream << "Document " << this << " sending scroll events to visualViewport");
        m_needsVisualViewportScrollEvent = false;
        if (RefPtr window = domWindow())
            window->visualViewport().dispatchEvent(Event::create(eventNames().scrollEvent, Event::CanBubble::No, Event::IsCancelable::No));
    }
}

void Document::invalidateScrollbars()
{
    if (RefPtr frameView = view())
        frameView->invalidateScrollbarsForAllScrollableAreas();
}

void Document::scheduleToAdjustValidationMessagePosition(ValidationMessage& validationMessage)
{
    m_validationMessagesToPosition.add(validationMessage);
    scheduleRenderingUpdate(RenderingUpdateStep::UpdateValidationMessagePositions);
}

void Document::adjustValidationMessagePositions()
{
    for (auto& message : std::exchange(m_validationMessagesToPosition, { }))
        message.adjustBubblePosition();
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

void Document::setActiveSpeechRecognition(SpeechRecognition* speechRecognition)
{
    if (m_activeSpeechRecognition == speechRecognition)
        return;

    m_activeSpeechRecognition = speechRecognition;
    updateIsPlayingMedia();
}

void Document::noteUserInteractionWithMediaElement()
{
    if (m_userHasInteractedWithMediaElement)
        return;

    if (!protectedTopDocument()->userDidInteractWithPage())
        return;

    m_userHasInteractedWithMediaElement = true;
    updateIsPlayingMedia();
}

#if ENABLE(MEDIA_STREAM)
MediaProducerMediaStateFlags Document::computeCaptureState() const
{
    MediaProducerMediaStateFlags state;
    for (auto& source : m_captureSources)
        state.add(MediaStreamTrack::captureState(source.get()));
    return state;
}
#endif // ENABLE(MEDIA_STREAM)

void Document::updateIsPlayingMedia()
{
    ASSERT(!m_audioProducers.hasNullReferences());
    MediaProducerMediaStateFlags state;
    for (auto& audioProducer : m_audioProducers)
        state.add(audioProducer.mediaState());

#if ENABLE(MEDIA_STREAM)
    state.add(computeCaptureState());
    if (m_activeSpeechRecognition)
        state.add(MediaProducerMediaState::HasActiveAudioCaptureDevice);

    m_activeMediaElementsWithMediaStreamCount = 0;
    forEachMediaElement([&](auto& element) {
        if (element.hasMediaStreamSrcObject() && element.isPlaying())
            ++m_activeMediaElementsWithMediaStreamCount;
    });
#endif

    if (m_userHasInteractedWithMediaElement)
        state.add(MediaProducerMediaState::HasUserInteractedWithMediaElement);

    if (state == m_mediaState)
        return;

#if ENABLE(MEDIA_STREAM)
    bool captureStateChanged = MediaProducer::isCapturing(m_mediaState) != MediaProducer::isCapturing(state);
    bool microphoneMutedStateChanged = m_mediaState.containsAny(MediaProducer::IsCapturingAudioMask) != state.containsAny(MediaProducer::IsCapturingAudioMask);
#endif

    m_mediaState = state;

    if (RefPtr page = this->page())
        page->updateIsPlayingMedia();

#if ENABLE(MEDIA_STREAM)
    if (captureStateChanged)
        mediaStreamCaptureStateChanged();

    if (microphoneMutedStateChanged) {
        if (auto* controller = UserMediaController::from(page()))
            controller->checkDocumentForVoiceActivity(this);
    }
#endif
}

void Document::visibilityAdjustmentStateDidChange()
{
    for (auto& audioProducer : m_audioProducers)
        audioProducer.visibilityAdjustmentStateDidChange();
}

#if ENABLE(MEDIA_STREAM) && ENABLE(MEDIA_SESSION)
static bool hasRealtimeMediaSource(const HashSet<Ref<RealtimeMediaSource>>& sources, const Function<bool(const RealtimeMediaSource&)>& filterSource)
{
    for (Ref source : sources) {
        if (!source->isEnded() && filterSource(source.get()))
            return true;
    }
    return false;
}

void Document::processCaptureStateDidChange(Function<bool(const Page&)>&& isPageMutedCallback, Function<bool(const RealtimeMediaSource&)>&& filterSource, MediaSessionAction action)
{
    RefPtr page = this->page();
    if (!page)
        return;

    RefPtr window = domWindow();
    RefPtr mediaSession = window ? NavigatorMediaSession::mediaSessionIfExists(window->protectedNavigator()) : nullptr;
    if (!mediaSession)
        return;

    if (!hasRealtimeMediaSource(m_captureSources, filterSource))
        return;

    eventLoop().queueTask(TaskSource::MediaElement, [weakDocument = WeakPtr { *this }, weakSession = WeakPtr { *mediaSession }, isPageMuted = isPageMutedCallback(*page), filterSource = WTFMove(filterSource), isPageMutedCallback = WTFMove(isPageMutedCallback), action] {
        RefPtr protecteDocument = weakDocument.get();
        if (!protecteDocument)
            return;

        RefPtr page = protecteDocument->page();
        if (!page || isPageMuted != isPageMutedCallback(*page) || !hasRealtimeMediaSource(protecteDocument->m_captureSources, filterSource))
            return;

        if (RefPtr protectedSession = weakSession.get()) {
            MediaSessionActionDetails details;
            details.action = action;
            details.isActivating = !isPageMuted;
            protectedSession->callActionHandler(details);
        }
    });
}

void Document::cameraCaptureStateDidChange()
{
    processCaptureStateDidChange([] (auto& page) {
        return page.mutedState().contains(MediaProducerMutedState::VideoCaptureIsMuted);
    }, [] (auto& source) {
        return source.deviceType() == CaptureDevice::DeviceType::Camera;
    }, MediaSessionAction::Togglecamera);
}

void Document::microphoneCaptureStateDidChange()
{
    processCaptureStateDidChange([] (auto& page) {
        return page.mutedState().contains(MediaProducerMutedState::AudioCaptureIsMuted);
    }, [] (auto& source) {
        return source.deviceType() == CaptureDevice::DeviceType::Microphone;
    }, MediaSessionAction::Togglemicrophone);
}

void Document::screenshareCaptureStateDidChange()
{
    processCaptureStateDidChange([] (auto& page) {
        return page.mutedState().contains(MediaProducerMutedState::ScreenCaptureIsMuted) || page.mutedState().contains(MediaProducerMutedState::WindowCaptureIsMuted);
    }, [] (auto& source) {
        return source.deviceType() == CaptureDevice::DeviceType::Screen || source.deviceType() == CaptureDevice::DeviceType::Window;
    }, MediaSessionAction::Togglescreenshare);
}

bool Document::hasMutedAudioCaptureDevice() const
{
    return mediaState().contains(MediaProducerMediaState::HasMutedAudioCaptureDevice);
}

void Document::setShouldListenToVoiceActivity(bool shouldListen)
{
    if (m_shouldListenToVoiceActivity == shouldListen)
        return;

    m_shouldListenToVoiceActivity = shouldListen;

    if (auto* controller = UserMediaController::from(page()))
        controller->setShouldListenToVoiceActivity(*this, m_shouldListenToVoiceActivity);
}

void Document::voiceActivityDetected()
{
    if (!isFullyActive() || topDocument().hidden() || !hasMutedAudioCaptureDevice())
        return;

    RefPtr window = domWindow();
    RefPtr mediaSession = window ? NavigatorMediaSession::mediaSessionIfExists(window->protectedNavigator()) : nullptr;
    if (!mediaSession)
        return;

    eventLoop().queueTask(TaskSource::MediaElement, [weakDocument = WeakPtr { *this }, weakSession = WeakPtr { *mediaSession }] {
        RefPtr protecteDocument = weakDocument.get();
        if (!protecteDocument || !protecteDocument->hasMutedAudioCaptureDevice())
            return;

        if (RefPtr protectedSession = weakSession.get())
            protectedSession->callActionHandler({ .action = MediaSessionAction:: Voiceactivity });
    });
}
#endif

void Document::pageMutedStateDidChange()
{
    for (auto& audioProducer : m_audioProducers)
        audioProducer.pageMutedStateDidChange();

#if ENABLE(MEDIA_STREAM)
    updateCaptureAccordingToMutedState();
#endif
}

#if ENABLE(MEDIA_STREAM)
static void updateCaptureSourceToPageMutedState(Document& document, Page& page, RealtimeMediaSource& source)
{
    ASSERT(source.isCaptureSource());
    switch (source.deviceType()) {
    case CaptureDevice::DeviceType::Microphone:
#if PLATFORM(IOS_FAMILY)
        if (document.settings().manageCaptureStatusBarInGPUProcessEnabled() && !document.settings().interruptAudioOnPageVisibilityChangeEnabled())
            source.setIsInBackground(document.hidden());
#endif
        source.setMuted(page.mutedState().contains(MediaProducerMutedState::AudioCaptureIsMuted) || (document.hidden() && document.settings().interruptAudioOnPageVisibilityChangeEnabled()));
        break;
    case CaptureDevice::DeviceType::Camera:
        source.setMuted(page.mutedState().contains(MediaProducerMutedState::VideoCaptureIsMuted) || (document.hidden() && document.settings().interruptVideoOnPageVisibilityChangeEnabled()));
        break;
    case CaptureDevice::DeviceType::Screen:
        source.setMuted(page.mutedState().contains(MediaProducerMutedState::ScreenCaptureIsMuted));
        break;
    case CaptureDevice::DeviceType::Window:
        source.setMuted(page.mutedState().contains(MediaProducerMutedState::WindowCaptureIsMuted));
        break;
    case CaptureDevice::DeviceType::SystemAudio:
    case CaptureDevice::DeviceType::Speaker:
    case CaptureDevice::DeviceType::Unknown:
        ASSERT_NOT_REACHED();
        break;
    }
}

void Document::addCaptureSource(Ref<RealtimeMediaSource>&& source)
{
    ASSERT(!source->isEnded());

    RefPtr page = this->page();
    if (!page)
        return;

    updateCaptureSourceToPageMutedState(*this, *page, source.get());
    source->registerOwnerCallback([document = WeakPtr { *this }](auto& source, bool isNewClonedSource) {
        RefPtr protectedDocument = document.get();
        if (!protectedDocument || protectedDocument->m_isUpdatingCaptureAccordingToMutedState)
            return;
        if (source.isEnded())
            protectedDocument->m_captureSources.remove(source);
        else if (isNewClonedSource)
            protectedDocument->m_captureSources.add(source);
        protectedDocument->updateIsPlayingMedia();
    });

    m_captureSources.add(WTFMove(source));
}

void Document::updateCaptureAccordingToMutedState()
{
    RefPtr page = this->page();
    if (!page)
        return;

    m_isUpdatingCaptureAccordingToMutedState = true;
    for (auto& source : m_captureSources)
        updateCaptureSourceToPageMutedState(*this, *page, source.get());
    m_isUpdatingCaptureAccordingToMutedState = false;

    updateIsPlayingMedia();
}
#endif // ENABLE(MEDIA_STREAM)

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

    RefPtr focusedElement = node.treeScope().focusedElementInScope();
    if (!focusedElement)
        return;

    if (isNodeInSubtree(*focusedElement, node, nodeRemoval)) {
        // FIXME: We should avoid synchronously updating the style inside setFocusedElement.
        // FIXME: Object elements should avoid loading a frame synchronously in a post style recalc callback.
        SubframeLoadingDisabler disabler(dynamicDowncast<ContainerNode>(node));
        setFocusedElement(nullptr, { { }, { }, FocusRemovalEventsMode::DoNotDispatch, { }, { } });
        // Set the focus navigation starting node to the previous focused element so that
        // we can fallback to the siblings or parent node for the next search.
        // Also we need to call removeFocusNavigationNodeOfSubtree after this function because
        // setFocusedElement(nullptr) will reset m_focusNavigationStartingNode.
        setFocusNavigationStartingNode(focusedElement.get());
    }
}

void Document::appendAutofocusCandidate(Element& candidate)
{
    ASSERT(isTopDocument());
    ASSERT(!m_isAutofocusProcessed);
    auto it = m_autofocusCandidates.findIf([&candidate](auto& c) {
        return c == &candidate;
    });
    if (it != m_autofocusCandidates.end())
        m_autofocusCandidates.remove(it);
    m_autofocusCandidates.append(candidate);
}

void Document::clearAutofocusCandidates()
{
    m_autofocusCandidates.clear();
}

// https://html.spec.whatwg.org/multipage/interaction.html#flush-autofocus-candidates
void Document::flushAutofocusCandidates()
{
    ASSERT(isTopDocument());
    if (m_isAutofocusProcessed)
        return;

    if (m_autofocusCandidates.isEmpty())
        return;

    if (cssTarget()) {
        m_autofocusCandidates.clear();
        setAutofocusProcessed();
        return;
    }

    while (!m_autofocusCandidates.isEmpty()) {
        RefPtr element = m_autofocusCandidates.first().get();
        if (!element || !element->document().isFullyActive() || &element->document().topDocument() != this) {
            m_autofocusCandidates.removeFirst();
            continue;
        }

        if (RefPtr parser = scriptableDocumentParser(); parser && parser->hasScriptsWaitingForStylesheets())
            break;
        m_autofocusCandidates.removeFirst();

        bool hasAncestorWithCSSTarget = [&] {
            for (auto* document = &element->document(); document && document != this; document = document->parentDocument()) {
                if (document->cssTarget())
                    return true;
            }
            return false;
        }();
        if (hasAncestorWithCSSTarget)
            continue;

        // FIXME: Need to ignore if the inclusive ancestor documents has a target element.
        // FIXME: Use the result of getting the focusable area for element if element is not focusable.
        if (element->isFocusable()) {
            clearAutofocusCandidates();
            setAutofocusProcessed();
            element->runFocusingStepsForAutofocus();
            return;
        }
    }
}

void Document::hoveredElementDidDetach(Element& element)
{
    if (!m_hoveredElement || &element != m_hoveredElement)
        return;

    m_hoveredElement = element.parentElement();
    while (m_hoveredElement && !m_hoveredElement->renderer())
        m_hoveredElement = m_hoveredElement->parentElement();
    if (RefPtr frame = this->frame())
        frame->eventHandler().scheduleHoverStateUpdate();
}

void Document::elementInActiveChainDidDetach(Element& element)
{
    if (!m_activeElement || &element != m_activeElement)
        return;

    m_activeElement = element.parentElement();
    while (m_activeElement && !m_activeElement->renderer())
        m_activeElement = m_activeElement->parentElement();
}

void Document::updateEventRegions()
{
    // FIXME: Move updateTouchEventRegions() here, but it should only happen for the top document.
    if (CheckedPtr view = renderView()) {
        if (view->usesCompositing())
            view->compositor().updateEventRegions();
    }
}

void Document::scheduleDeferredAXObjectCacheUpdate()
{
    if (m_scheduledDeferredAXObjectCacheUpdate)
        return;

    m_scheduledDeferredAXObjectCacheUpdate = true;
    eventLoop().queueTask(TaskSource::InternalAsyncTask, [weakThis = WeakPtr { *this }] {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->flushDeferredAXObjectCacheUpdate();
    });
}

void Document::flushDeferredAXObjectCacheUpdate()
{
    if (!m_scheduledDeferredAXObjectCacheUpdate)
        return;

    m_scheduledDeferredAXObjectCacheUpdate = false;

    if (CheckedPtr cache = existingAXObjectCache())
        cache->performDeferredCacheUpdate(ForceLayout::No);
}

void Document::updateAccessibilityObjectRegions()
{
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (CheckedPtr cache = existingAXObjectCache())
        cache->willUpdateObjectRegions();

    if (CheckedPtr view = renderView())
        view->protectedFrameView()->updateAccessibilityObjectRegions();
#endif
}

void Document::invalidateEventRegionsForFrame(HTMLFrameOwnerElement& element)
{
    CheckedPtr renderer = element.renderer();
    if (!renderer)
        return;
    if (auto* layer = renderer->enclosingLayer()) {
        if (layer->invalidateEventRegion(RenderLayer::EventRegionInvalidationReason::NonCompositedFrame))
            return;
    }
    if (RefPtr ownerElement = this->ownerElement())
        ownerElement->protectedDocument()->invalidateEventRegionsForFrame(*ownerElement);
}

void Document::invalidateEventListenerRegions()
{
    if (!renderView() || !documentElement())
        return;

    // We don't track style validity for Document and full rebuild is too big of a hammer.
    // Instead just mutate the style directly and trigger a minimal style update.
    CheckedPtr renderView = this->renderView();
    auto& rootStyle = renderView->mutableStyle();
    auto changed = Style::Adjuster::adjustEventListenerRegionTypesForRootStyle(rootStyle, *this);

    if (changed)
        scheduleFullStyleRebuild();
    else
        protectedDocumentElement()->invalidateStyleInternal();
}

void Document::invalidateRenderingDependentRegions()
{
#if PLATFORM(IOS_FAMILY) && ENABLE(TOUCH_EVENTS)
    setTouchEventRegionsNeedUpdate();
#endif

#if PLATFORM(IOS_FAMILY)
    if (RefPtr page = this->page()) {
        if (RefPtr frameView = view()) {
            if (RefPtr scrollingCoordinator = page->scrollingCoordinator())
                scrollingCoordinator->frameViewEventTrackingRegionsChanged(*frameView);
        }
    }
#endif
}

bool Document::setFocusedElement(Element* element)
{
    return setFocusedElement(element, { });
}

bool Document::setFocusedElement(Element* newFocusedElement, const FocusOptions& options)
{
    // Make sure newFocusedElement is actually in this document
    if (newFocusedElement && (&newFocusedElement->document() != this))
        return true;

    if (m_focusedElement == newFocusedElement)
        return true;

    if (backForwardCacheState() != NotInBackForwardCache)
        return false;

    RefPtr oldFocusedElement = WTFMove(m_focusedElement);

    // Remove focus from the existing focus node (if any)
    if (oldFocusedElement) {
        bool focusChangeBlocked = false;

        oldFocusedElement->setFocus(false);
        setFocusNavigationStartingNode(nullptr);

        scheduleContentRelevancyUpdate(ContentRelevancy::Focused);

        if (options.removalEventsMode == FocusRemovalEventsMode::Dispatch) {
            // Dispatch a change event for form control elements that have been edited.
            if (RefPtr formControlElement = dynamicDowncast<HTMLFormControlElement>(*oldFocusedElement)) {
                if (formControlElement->wasChangedSinceLastFormControlChangeEvent())
                    formControlElement->dispatchFormControlChangeEvent();
            }

            // Dispatch the blur event and let the node do any other blur related activities (important for text fields)
            oldFocusedElement->dispatchBlurEvent(newFocusedElement);

            if (m_focusedElement) {
                // handler shifted focus
                focusChangeBlocked = true;
                newFocusedElement = nullptr;
            }

            oldFocusedElement->dispatchFocusOutEventIfNeeded(newFocusedElement); // DOM level 3 bubbling blur event.

            if (m_focusedElement) {
                // handler shifted focus
                focusChangeBlocked = true;
                newFocusedElement = nullptr;
            }
        } else {
            // Match the order in HTMLTextFormControlElement::dispatchBlurEvent.
            if (RefPtr inputElement = dynamicDowncast<HTMLInputElement>(*oldFocusedElement))
                inputElement->endEditing();
            if (RefPtr page = this->page())
                page->chrome().client().elementDidBlur(*oldFocusedElement);
            ASSERT(!m_focusedElement);
        }

        if (oldFocusedElement->isRootEditableElement())
            editor().didEndEditing();

        if (RefPtr view = this->view()) {
            if (RefPtr oldWidget = widgetForElement(oldFocusedElement.get()))
                oldWidget->setFocus(false);
            else
                view->setFocus(false);
        }

        if (RefPtr inputElement = dynamicDowncast<HTMLInputElement>(*oldFocusedElement)) {
            // HTMLInputElement::didBlur just scrolls text fields back to the beginning.
            // FIXME: This could be done asynchronusly.
            inputElement->didBlur();
        }

        if (focusChangeBlocked)
            return false;
    }

    auto isNewElementFocusable = [&] {
        if (!newFocusedElement || !newFocusedElement->isConnected())
            return false;
        // Resolving isFocusable() may require matching :focus-within as if the focus was already on the new element.
        newFocusedElement->setHasTentativeFocus(true);
        bool isFocusable = newFocusedElement->isFocusable();
        newFocusedElement->setHasTentativeFocus(false);
        return isFocusable;
    }();

    if (isNewElementFocusable) {
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
        RefPtr focusedElement = m_focusedElement;
        setFocusNavigationStartingNode(focusedElement.get());
        focusedElement->setFocus(true, options.visibility);
        if (options.trigger != FocusTrigger::Bindings)
            m_latestFocusTrigger = options.trigger;

        scheduleContentRelevancyUpdate(ContentRelevancy::Focused);

        // The setFocus call triggers a blur and a focus event. Event handlers could cause the focused element to be cleared.
        if (m_focusedElement != focusedElement) {
            // handler shifted focus
            return false;
        }

        // Dispatch the focus event and let the node do any other focus related activities (important for text fields)
        focusedElement->dispatchFocusEvent(oldFocusedElement.copyRef(), options);

        if (m_focusedElement != focusedElement) {
            // handler shifted focus
            return false;
        }

        focusedElement->dispatchFocusInEventIfNeeded(oldFocusedElement.copyRef()); // DOM level 3 bubbling focus event.

        if (m_focusedElement != focusedElement) {
            // handler shifted focus
            return false;
        }

        if (focusedElement->isRootEditableElement())
            editor().didBeginEditing();

        // eww, I suck. set the qt focus correctly
        // ### find a better place in the code for this
        if (view()) {
            RefPtr focusWidget = widgetForElement(focusedElement.get());
            if (focusWidget) {
                // Make sure a widget has the right size before giving it focus.
                // Otherwise, we are testing edge cases of the Widget code.
                // Specifically, in WebCore this does not work well for text fields.
                updateLayout();
                // Re-get the widget in case updating the layout changed things.
                focusWidget = widgetForElement(focusedElement.get());
            }
            if (focusWidget)
                focusWidget->setFocus(true);
            else if (RefPtr frameView = view())
                frameView->setFocus(true);
        }
    }

    if (m_focusedElement) {
#if PLATFORM(GTK)
        // GTK relies on creating the AXObjectCache when a focus change happens.
        if (CheckedPtr cache = axObjectCache())
#else
        if (CheckedPtr cache = existingAXObjectCache())
#endif
            cache->onFocusChange(oldFocusedElement.get(), newFocusedElement);
    }

    if (RefPtr page = this->page())
        page->chrome().focusedElementChanged(protectedFocusedElement().get());

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
        if (m_focusedElement->isRootEditableElement() && m_focusedElement->contains(m_focusNavigationStartingNode.get()))
            return m_focusedElement.get();
    }

    if (!m_focusNavigationStartingNode)
        return nullptr;

    RefPtr node = m_focusNavigationStartingNode;

    // When the node was removed from the document tree. This case is not specified in the spec:
    // https://html.spec.whatwg.org/multipage/interaction.html#sequential-focus-navigation-starting-point
    // Current behaivor is to move the sequential navigation node to / after (based on the focus direction)
    // the previous sibling of the removed node.
    if (m_focusNavigationStartingNodeIsRemoved) {
        RefPtr nextNode = NodeTraversal::next(*node);
        if (!nextNode)
            nextNode = WTFMove(node);
        if (direction == FocusDirection::Forward)
            return ElementTraversal::previous(*nextNode);
        if (auto* nextElement = dynamicDowncast<Element>(*nextNode))
            return nextElement;
        return ElementTraversal::next(*nextNode);
    }

    if (auto* element = dynamicDowncast<Element>(*node))
        return element;
    if (Element* elementBeforeNextFocusableElement = direction == FocusDirection::Forward ? ElementTraversal::previous(*node) : ElementTraversal::next(*node))
        return elementBeforeNextFocusableElement;
    return node->parentOrShadowHostElement();
}

void Document::setCSSTarget(Element* newTarget)
{
    if (m_cssTarget == newTarget)
        return;

    std::optional<Style::PseudoClassChangeInvalidation> oldInvalidation;
    if (m_cssTarget)
        emplace(oldInvalidation, *m_cssTarget, { { CSSSelector::PseudoClass::Target, false } });

    std::optional<Style::PseudoClassChangeInvalidation> newInvalidation;
    if (newTarget)
        emplace(newInvalidation, *newTarget, { { CSSSelector::PseudoClass::Target, true } });
    m_cssTarget = newTarget;
}

void Document::registerNodeListForInvalidation(LiveNodeList& list)
{
    m_nodeListAndCollectionCounts[static_cast<unsigned>(list.invalidationType())]++;
    if (!list.isRootedAtTreeScope())
        return;
    ASSERT(!list.isRegisteredForInvalidationAtDocument());
    list.setRegisteredForInvalidationAtDocument(true);
    m_listsInvalidatedAtDocument.add(&list);
}

void Document::unregisterNodeListForInvalidation(LiveNodeList& list)
{
    m_nodeListAndCollectionCounts[static_cast<unsigned>(list.invalidationType())]--;
    if (!list.isRegisteredForInvalidationAtDocument())
        return;

    list.setRegisteredForInvalidationAtDocument(false);
    ASSERT(m_listsInvalidatedAtDocument.contains(&list));
    m_listsInvalidatedAtDocument.remove(&list);
}

void Document::registerCollection(HTMLCollection& collection)
{
    m_nodeListAndCollectionCounts[static_cast<unsigned>(collection.invalidationType())]++;
    if (collection.isRootedAtTreeScope())
        m_collectionsInvalidatedAtDocument.add(&collection);
}

void Document::unregisterCollection(HTMLCollection& collection)
{
    ASSERT(m_nodeListAndCollectionCounts[static_cast<unsigned>(collection.invalidationType())]);
    m_nodeListAndCollectionCounts[static_cast<unsigned>(collection.invalidationType())]--;
    if (!collection.isRootedAtTreeScope())
        return;

    m_collectionsInvalidatedAtDocument.remove(&collection);
}

void Document::collectionCachedIdNameMap(const HTMLCollection& collection)
{
    ASSERT_UNUSED(collection, collection.hasNamedElementCache());
    m_nodeListAndCollectionCounts[static_cast<unsigned>(NodeListInvalidationType::InvalidateOnIdNameAttrChange)]++;
}

void Document::collectionWillClearIdNameMap(const HTMLCollection& collection)
{
    ASSERT_UNUSED(collection, collection.hasNamedElementCache());
    ASSERT(m_nodeListAndCollectionCounts[static_cast<unsigned>(NodeListInvalidationType::InvalidateOnIdNameAttrChange)]);
    m_nodeListAndCollectionCounts[static_cast<unsigned>(NodeListInvalidationType::InvalidateOnIdNameAttrChange)]--;
}

void Document::attachNodeIterator(NodeIterator& iterator)
{
    m_nodeIterators.add(iterator);
}

void Document::detachNodeIterator(NodeIterator& iterator)
{
    // The node iterator can be detached without having been attached if its root node didn't have a document
    // when the iterator was created, but has it now.
    m_nodeIterators.remove(iterator);
}

void Document::moveNodeIteratorsToNewDocument(Node& node, Document& newDocument)
{
    for (auto& it : copyToVectorOf<Ref<NodeIterator>>(m_nodeIterators)) {
        if (&it->root() == &node) {
            detachNodeIterator(it.get());
            newDocument.attachNodeIterator(it.get());
        }
    }
}

void Document::updateRangesAfterChildrenChanged(ContainerNode& container)
{
    for (auto& range : m_ranges)
        Ref { range.get() }->nodeChildrenChanged(container);
}

void Document::nodeChildrenWillBeRemoved(ContainerNode& container)
{
    ASSERT(ScriptDisallowedScope::InMainThread::hasDisallowedScope());

    adjustFocusedNodeOnNodeRemoval(container, NodeRemoval::ChildrenOfNode);
    adjustFocusNavigationNodeOnNodeRemoval(container, NodeRemoval::ChildrenOfNode);

    for (auto& range : m_ranges)
        Ref { range.get() }->nodeChildrenWillBeRemoved(container);

    for (auto& it : m_nodeIterators) {
        for (RefPtr n = container.firstChild(); n; n = n->nextSibling())
            it.nodeWillBeRemoved(*n);
    }

    if (RefPtr frame = this->frame()) {
        for (RefPtr n = container.firstChild(); n; n = n->nextSibling()) {
            frame->eventHandler().nodeWillBeRemoved(*n);
            frame->selection().nodeWillBeRemoved(*n);
            frame->page()->dragCaretController().nodeWillBeRemoved(*n);
        }
    }

    if (m_markers && m_markers->hasMarkers()) {
        for (RefPtr textNode = TextNodeTraversal::firstChild(container); textNode; textNode = TextNodeTraversal::nextSibling(*textNode))
            m_markers->removeMarkers(*textNode);
    }
}

void Document::nodeWillBeRemoved(Node& node)
{
    ASSERT(ScriptDisallowedScope::InMainThread::hasDisallowedScope());

    adjustFocusedNodeOnNodeRemoval(node);
    adjustFocusNavigationNodeOnNodeRemoval(node);

    for (Ref nodeIterator : m_nodeIterators)
        nodeIterator->nodeWillBeRemoved(node);

    for (auto& range : m_ranges)
        Ref { range.get() }->nodeWillBeRemoved(node);

    if (RefPtr frame = this->frame()) {
        frame->eventHandler().nodeWillBeRemoved(node);
        frame->selection().nodeWillBeRemoved(node);
        frame->page()->dragCaretController().nodeWillBeRemoved(node);
    }

    if (is<Text>(node) && m_markers)
        m_markers->removeMarkers(node);
}

void Document::parentlessNodeMovedToNewDocument(Node& node)
{
    Vector<Ref<Range>, 5> rangesAffected;

    for (auto& weakRange : m_ranges) {
        Ref range = weakRange.get();
        if (range->parentlessNodeMovedToNewDocumentAffectsRange(node))
            rangesAffected.append(WTFMove(range));
    }

    for (auto& range : rangesAffected)
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
    for (auto& range : m_ranges)
        Ref { range.get() }->textInserted(text, offset, length);

    if (!m_markers)
        return;

    // Update the markers for spelling and grammar checking.
    m_markers->shiftMarkers(text, offset, length);

#if ENABLE(PLATFORM_DRIVEN_TEXT_CHECKING)
    // Freshly inserted text is expected to not inherit PlatformTextChecking markers.
    m_markers->removeMarkers(text, { offset, offset + length }, DocumentMarker::Type::PlatformTextChecking);
#endif
}

void Document::textRemoved(Node& text, unsigned offset, unsigned length)
{
    for (auto& range : m_ranges)
        Ref { range.get() }->textRemoved(text, offset, length);

    if (!m_markers)
        return;

    // Update the markers for spelling and grammar checking.
    m_markers->removeMarkers(text, { offset, offset + length });
    m_markers->shiftMarkers(text, offset + length, 0 - length);
}

void Document::textNodesMerged(Text& oldNode, unsigned offset)
{
    if (!m_ranges.isEmpty()) {
        NodeWithIndex oldNodeWithIndex(oldNode);
        for (auto& range : m_ranges)
            Ref { range.get() }->textNodesMerged(oldNodeWithIndex, offset);
    }

    // FIXME: This should update markers for spelling and grammar checking.
}

void Document::textNodeSplit(Text& oldNode)
{
    for (auto& range : m_ranges)
        Ref { range.get() }->textNodeSplit(oldNode);

    // FIXME: This should update markers for spelling and grammar checking.
}

void Document::createDOMWindow()
{
    ASSERT(m_frame);
    ASSERT(!m_domWindow);

    m_domWindow = LocalDOMWindow::create(*this);

    ASSERT(m_domWindow->document() == this);
    ASSERT(m_domWindow->frame() == m_frame);
}

void Document::takeDOMWindowFrom(Document& document)
{
    ASSERT(m_frame);
    ASSERT(!m_domWindow);
    ASSERT(document.m_domWindow);
    // A valid LocalDOMWindow is needed by CachedFrame for its documents.
    ASSERT(backForwardCacheState() == NotInBackForwardCache);

    m_domWindow = WTFMove(document.m_domWindow);
    protectedWindow()->didSecureTransitionTo(*this);

    ASSERT(m_domWindow->document() == this);
    ASSERT(m_domWindow->frame() == m_frame);
}

WindowProxy* Document::windowProxy() const
{
    if (!m_frame)
        return nullptr;
    return &m_frame->windowProxy();
}

RefPtr<WindowProxy> Document::protectedWindowProxy() const
{
    return windowProxy();
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

void Document::setWindowAttributeEventListener(const AtomString& eventType, const QualifiedName& attributeName, const AtomString& attributeValue, DOMWrapperWorld& isolatedWorld)
{
    if (!m_domWindow)
        return;
    if (!m_domWindow->frame())
        return;
    protectedWindow()->setAttributeEventListener(eventType, JSLazyEventListener::create(*m_domWindow, attributeName, attributeValue), isolatedWorld);
}

void Document::dispatchWindowEvent(Event& event, EventTarget* target)
{
    ASSERT_WITH_SECURITY_IMPLICATION(ScriptDisallowedScope::InMainThread::isScriptAllowed());
    if (RefPtr window = m_domWindow)
        window->dispatchEvent(event, target);
}

void Document::dispatchWindowLoadEvent()
{
    ASSERT_WITH_SECURITY_IMPLICATION(ScriptDisallowedScope::InMainThread::isScriptAllowed());
    if (!m_domWindow)
        return;
    protectedWindow()->dispatchLoadEvent();
    m_loadEventFinished = true;
    protectedCachedResourceLoader()->documentDidFinishLoadEvent();
}

void Document::queueTaskToDispatchEvent(TaskSource source, Ref<Event>&& event)
{
    eventLoop().queueTask(source, [document = Ref { *this }, event = WTFMove(event)] {
        document->dispatchEvent(event);
    });
}

void Document::queueTaskToDispatchEventOnWindow(TaskSource source, Ref<Event>&& event)
{
    eventLoop().queueTask(source, [this, protectedThis = Ref { *this }, event = WTFMove(event)] {
        if (RefPtr window = m_domWindow)
            window->dispatchEvent(event);
    });
}

ExceptionOr<Ref<Event>> Document::createEvent(const String& type)
{
    // Please do *not* add new event classes to this function unless they are required
    // for compatibility with the DOM specification or some actual legacy web content.

    // This mechanism is superseded by use of event constructors.
    // That is what we should use for any new event classes.

    // The following strings are the ones from the DOM specification
    // <https://dom.spec.whatwg.org/#dom-document-createevent>.

    if (equalLettersIgnoringASCIICase(type, "beforeunloadevent"_s))
        return Ref<Event> { BeforeUnloadEvent::createForBindings() };
    if (equalLettersIgnoringASCIICase(type, "compositionevent"_s))
        return Ref<Event> { CompositionEvent::createForBindings() };
    if (equalLettersIgnoringASCIICase(type, "customevent"_s))
        return Ref<Event> { CustomEvent::create() };
    if (equalLettersIgnoringASCIICase(type, "dragevent"_s))
        return Ref<Event> { DragEvent::createForBindings() };
    if (equalLettersIgnoringASCIICase(type, "event"_s) || equalLettersIgnoringASCIICase(type, "events"_s) || equalLettersIgnoringASCIICase(type, "htmlevents"_s) || equalLettersIgnoringASCIICase(type, "svgevents"_s))
        return Event::createForBindings();
    if (equalLettersIgnoringASCIICase(type, "focusevent"_s))
        return Ref<Event> { FocusEvent::createForBindings() };
    if (equalLettersIgnoringASCIICase(type, "hashchangeevent"_s))
        return Ref<Event> { HashChangeEvent::createForBindings() };
    if (equalLettersIgnoringASCIICase(type, "keyboardevent"_s))
        return Ref<Event> { KeyboardEvent::createForBindings() };
    if (equalLettersIgnoringASCIICase(type, "messageevent"_s))
        return Ref<Event> { MessageEvent::createForBindings() };
    if (equalLettersIgnoringASCIICase(type, "storageevent"_s))
        return Ref<Event> { StorageEvent::createForBindings() };
    if (equalLettersIgnoringASCIICase(type, "mouseevent"_s) || equalLettersIgnoringASCIICase(type, "mouseevents"_s))
        return Ref<Event> { MouseEvent::createForBindings() };
    if (equalLettersIgnoringASCIICase(type, "textevent"_s))
        return Ref<Event> { TextEvent::createForBindings() }; // FIXME: HTML specification says this should create a CompositionEvent, not a TextEvent.
    if (equalLettersIgnoringASCIICase(type, "uievent"_s) || equalLettersIgnoringASCIICase(type, "uievents"_s))
        return Ref<Event> { UIEvent::createForBindings() };

    // FIXME: Consider including support for these event classes even when device orientation
    // support is not enabled.
#if ENABLE(DEVICE_ORIENTATION)
    if (equalLettersIgnoringASCIICase(type, "devicemotionevent"_s))
        return Ref<Event> { DeviceMotionEvent::createForBindings() };
    if (equalLettersIgnoringASCIICase(type, "deviceorientationevent"_s))
        return Ref<Event> { DeviceOrientationEvent::createForBindings() };
#endif

#if ENABLE(TOUCH_EVENTS)
    if (equalLettersIgnoringASCIICase(type, "touchevent"_s))
        return Ref<Event> { TouchEvent::createForBindings() };
#endif

    // FIXME: Add support for "dragevent", which the DOM specification calls for.

    // The following strings are not part of the DOM specification and we would like to eliminate them.
    // However, we currently include them until we resolve any issues with backward compatibility.
    // FIXME: For each of the strings below, confirm that there is no content depending on it and remove
    // the string, remove the createForBindings function, and also consider removing the corresponding
    // init function for that class.

    if (equalLettersIgnoringASCIICase(type, "keyboardevents"_s))
        return Ref<Event> { KeyboardEvent::createForBindings() };
    if (equalLettersIgnoringASCIICase(type, "mutationevent"_s) || equalLettersIgnoringASCIICase(type, "mutationevents"_s))
        return Ref<Event> { MutationEvent::createForBindings() };
    if (equalLettersIgnoringASCIICase(type, "popstateevent"_s))
        return Ref<Event> { PopStateEvent::createForBindings() };
    if (equalLettersIgnoringASCIICase(type, "wheelevent"_s))
        return Ref<Event> { WheelEvent::createForBindings() };

    return Exception { ExceptionCode::NotSupportedError };
}

bool Document::hasListenerTypeForEventType(PlatformEvent::Type eventType) const
{
    switch (eventType) {
    case PlatformEvent::Type::MouseForceChanged:
        return m_listenerTypes.contains(Document::ListenerType::ForceChanged);
    case PlatformEvent::Type::MouseForceDown:
        return m_listenerTypes.contains(Document::ListenerType::ForceDown);
    case PlatformEvent::Type::MouseForceUp:
        return m_listenerTypes.contains(Document::ListenerType::ForceUp);
    case PlatformEvent::Type::MouseScroll:
        return m_listenerTypes.contains(Document::ListenerType::Scroll);
    default:
        return false;
    }
}

void Document::addListenerTypeIfNeeded(const AtomString& eventType)
{
    auto& eventNames = WebCore::eventNames();
    auto typeInfo = eventNames.typeInfoForEvent(eventType);
    switch (typeInfo.type()) {
    case EventType::DOMSubtreeModified:
        addListenerType(ListenerType::DOMSubtreeModified);
        break;
    case EventType::DOMNodeInserted:
        addListenerType(ListenerType::DOMNodeInserted);
        break;
    case EventType::DOMNodeRemoved:
        addListenerType(ListenerType::DOMNodeRemoved);
        break;
    case EventType::DOMNodeRemovedFromDocument:
        addListenerType(ListenerType::DOMNodeRemovedFromDocument);
        break;
    case EventType::DOMNodeInsertedIntoDocument:
        addListenerType(ListenerType::DOMNodeInsertedIntoDocument);
        break;
    case EventType::DOMCharacterDataModified:
        addListenerType(ListenerType::DOMCharacterDataModified);
        break;
    case EventType::scroll:
        addListenerType(ListenerType::Scroll);
        break;
    case EventType::webkitmouseforcewillbegin:
        addListenerType(ListenerType::ForceWillBegin);
        break;
    case EventType::webkitmouseforcechanged:
        addListenerType(ListenerType::ForceChanged);
        break;
    case EventType::webkitmouseforcedown:
        addListenerType(ListenerType::ForceDown);
        break;
    case EventType::webkitmouseforceup:
        addListenerType(ListenerType::ForceUp);
        break;
    case EventType::focusin:
        addListenerType(ListenerType::FocusIn);
        break;
    case EventType::focusout:
        addListenerType(ListenerType::FocusOut);
        break;
    default:
        if (typeInfo.isInCategory(EventCategory::CSSTransition))
            addListenerType(ListenerType::CSSTransition);
        else if (typeInfo.isInCategory(EventCategory::CSSAnimation))
            addListenerType(ListenerType::CSSAnimation);
    }
}

void Document::didAddEventListenersOfType(const AtomString& eventType, unsigned count)
{
    ASSERT(count);
    addListenerTypeIfNeeded(eventType);
    auto result = m_eventListenerCounts.fastAdd(eventType, 0);
    result.iterator->value += count;
}

void Document::didRemoveEventListenersOfType(const AtomString& eventType, unsigned count)
{
    ASSERT(count);
    ASSERT(m_eventListenerCounts.contains(eventType));
    auto it = m_eventListenerCounts.find(eventType);
    ASSERT(it->value >= count);
    it->value -= count;
}

HTMLFrameOwnerElement* Document::ownerElement() const
{
    if (!frame())
        return nullptr;
    return frame()->ownerElement();
}

std::optional<OwnerPermissionsPolicyData> Document::ownerPermissionsPolicy() const
{
    if (WeakPtr currentFrame = frame())
        return currentFrame->ownerPermissionsPolicy();

    return std::nullopt;
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
    if (cookieURL.protocolIsFile())
        return false;

    // A Document whose URL's scheme is not a network scheme is cookie-averse (https://fetch.spec.whatwg.org/#network-scheme).
    return !cookieURL.protocolIsInHTTPFamily() && !cookieURL.protocolIs("ftp"_s);
}

ExceptionOr<String> Document::cookie()
{
    if (page() && !page()->settings().cookieEnabled())
        return String();

    if (isCookieAverse())
        return String();

    if (canAccessResource(ScriptExecutionContext::ResourceType::Cookies) == ScriptExecutionContext::HasResourceAccess::No)
        return Exception { ExceptionCode::SecurityError };

    URL cookieURL = this->cookieURL();
    if (cookieURL.isEmpty())
        return String();

    if (!isDOMCookieCacheValid() && page())
        setCachedDOMCookies(protectedPage()->cookieJar().cookies(*this, cookieURL));

    return String { cachedDOMCookies() };
}

ExceptionOr<void> Document::setCookie(const String& value)
{
    if (page() && !page()->settings().cookieEnabled())
        return { };

    if (isCookieAverse())
        return { };

    if (canAccessResource(ScriptExecutionContext::ResourceType::Cookies) == ScriptExecutionContext::HasResourceAccess::No)
        return Exception { ExceptionCode::SecurityError };

    URL cookieURL = this->cookieURL();
    if (cookieURL.isEmpty())
        return { };

    invalidateDOMCookieCache();
    if (RefPtr page = this->page())
        page->cookieJar().setCookies(*this, cookieURL, value);
    return { };
}

String Document::referrer()
{
    if (!m_referrerOverride.isEmpty())
        return m_referrerOverride;
    if (DeprecatedGlobalSettings::trackingPreventionEnabled() && frame()) {
        auto referrerStr = frame()->loader().referrer();
        if (!referrerStr.isEmpty()) {
            URL referrerURL { referrerStr };
            RegistrableDomain referrerRegistrableDomain { referrerURL };
            if (!referrerRegistrableDomain.matches(securityOrigin().data())) {
                m_referrerOverride = URL { referrerURL.protocolHostAndPort() }.string();
                return m_referrerOverride;
            }
        }
    }
    if (frame())
        return frame()->loader().referrer();
    return String();
}

String Document::referrerForBindings()
{
    RefPtr policySourceLoader = topDocument().loader();
    if (!policySourceLoader)
        return referrer();

    if (!policySourceLoader->request().url().hasSpecialScheme() && url().protocolIsInHTTPFamily())
        policySourceLoader = loader();

    if (policySourceLoader && !RegistrableDomain { URL { frame()->loader().referrer() } }.matches(securityOrigin().data())) {
        auto policies = policySourceLoader->navigationalAdvancedPrivacyProtections();
        if (policies.contains(AdvancedPrivacyProtections::BaselineProtections))
            return { };

        if (policies.contains(AdvancedPrivacyProtections::ScriptTelemetry) && requiresScriptExecutionTelemetry(ScriptTelemetryCategory::Referrer))
            return { };
    }

    return referrer();
}

String Document::domain() const
{
    return securityOrigin().domain();
}

ExceptionOr<void> Document::setDomain(const String& newDomain)
{
    if (!frame())
        return Exception { ExceptionCode::SecurityError, "A browsing context is required to set a domain."_s };

    if (isSandboxed(SandboxFlag::DocumentDomain))
        return Exception { ExceptionCode::SecurityError, "Assignment is forbidden for sandboxed iframes."_s };

    if (LegacySchemeRegistry::isDomainRelaxationForbiddenForURLScheme(securityOrigin().protocol()))
        return Exception { ExceptionCode::SecurityError };

    // FIXME: We should add logging indicating why a domain was not allowed.

    const String& effectiveDomain = domain();
    if (effectiveDomain.isEmpty())
        return Exception { ExceptionCode::SecurityError, "The document has a null effectiveDomain."_s };

    if (!securityOrigin().isMatchingRegistrableDomainSuffix(newDomain, settings().treatIPAddressAsDomain()))
        return Exception { ExceptionCode::SecurityError, "Attempted to use a non-registrable domain."_s };

    securityOrigin().setDomainFromDOM(newDomain);
    return { };
}

void Document::overrideLastModified(const std::optional<WallTime>& lastModified)
{
    m_overrideLastModified = lastModified;
}

// http://www.whatwg.org/specs/web-apps/current-work/#dom-document-lastmodified
String Document::lastModified() const
{
    std::optional<WallTime> dateTime;
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
    updateCachedCookiesEnabled();
}

void Document::setFirstPartyForCookies(const URL& url)
{
    if (m_firstPartyForCookies == url)
        return;

    m_firstPartyForCookies = url;
    updateCachedCookiesEnabled();
}

void Document::updateCachedCookiesEnabled()
{
    RefPtr page = this->page();
    if (!page || !page->settings().cookieEnabled() || canAccessResource(ScriptExecutionContext::ResourceType::Cookies) == ScriptExecutionContext::HasResourceAccess::No) {
        setCachedCookiesEnabled(false);
        return;
    }

    page->cookieJar().remoteCookiesEnabled(*this, [weakDocument = WeakPtr { *this }](bool enabled) mutable {
        if (RefPtr document = weakDocument.get())
            document->setCachedCookiesEnabled(enabled);
    });
}

static bool isValidNameNonASCII(std::span<const LChar> characters)
{
    if (!isValidNameStart(characters[0]))
        return false;

    for (size_t i = 1; i < characters.size(); ++i) {
        if (!isValidNamePart(characters[i]))
            return false;
    }

    return true;
}

static bool isValidNameNonASCII(std::span<const UChar> characters)
{
    for (size_t i = 0; i < characters.size();) {
        bool first = !i;
        char32_t c;
        U16_NEXT(characters, i, characters.size(), c); // Increments i.
        if (first ? !isValidNameStart(c) : !isValidNamePart(c))
            return false;
    }

    return true;
}

template<typename CharType>
static inline bool isValidNameASCII(std::span<const CharType> characters)
{
    CharType c = characters[0];
    if (!(isASCIIAlpha(c) || c == ':' || c == '_'))
        return false;

    for (size_t i = 1; i < characters.size(); ++i) {
        c = characters[i];
        if (!(isASCIIAlphanumeric(c) || c == ':' || c == '_' || c == '-' || c == '.'))
            return false;
    }

    return true;
}

static bool isValidNameASCIIWithoutColon(std::span<const LChar> characters)
{
    auto c = characters.front();
    if (!(isASCIIAlpha(c) || c == '_'))
        return false;

    for (size_t i = 1; i < characters.size(); ++i) {
        c = characters[i];
        if (!(isASCIIAlphanumeric(c) || c == '_' || c == '-' || c == '.'))
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
        auto characters = name.span8();

        if (isValidNameASCII(characters))
            return true;

        return isValidNameNonASCII(characters);
    }

    auto characters = name.span16();

    if (isValidNameASCII(characters))
        return true;

    return isValidNameNonASCII(characters);
}

ExceptionOr<std::pair<AtomString, AtomString>> Document::parseQualifiedName(const AtomString& qualifiedName)
{
    unsigned length = qualifiedName.length();

    if (!length)
        return Exception { ExceptionCode::InvalidCharacterError };

    bool nameStart = true;
    bool sawColon = false;
    unsigned colonPosition = 0;

    bool isValidLocalName = qualifiedName.is8Bit() && isValidNameASCIIWithoutColon(qualifiedName.span8());
    if (LIKELY(isValidLocalName))
        return std::pair<AtomString, AtomString> { { }, { qualifiedName } };

    for (unsigned i = 0; i < length; ) {
        char32_t c;
        U16_NEXT(qualifiedName, i, length, c);
        if (c == ':') {
            if (sawColon)
                return Exception { ExceptionCode::InvalidCharacterError, makeString("Unexpected colon in qualified name '"_s, qualifiedName, '\'') };
            nameStart = true;
            sawColon = true;
            colonPosition = i - 1;
        } else if (nameStart) {
            if (!isValidNameStart(c))
                return Exception { ExceptionCode::InvalidCharacterError, makeString("Invalid qualified name start in '"_s, qualifiedName, '\'') };
            nameStart = false;
        } else {
            if (!isValidNamePart(c))
                return Exception { ExceptionCode::InvalidCharacterError, makeString("Invalid qualified name part in '"_s, qualifiedName, '\'') };
        }
    }

    if (!sawColon)
        return std::pair<AtomString, AtomString> { { }, { qualifiedName } };

    if (!colonPosition || length - colonPosition <= 1)
        return Exception { ExceptionCode::InvalidCharacterError, makeString("Namespace in qualified name '"_s, qualifiedName, "' is too short"_s) };

    return std::pair<AtomString, AtomString> { StringView { qualifiedName }.left(colonPosition).toAtomString(), StringView { qualifiedName }.substring(colonPosition + 1).toAtomString() };
}

ExceptionOr<QualifiedName> Document::parseQualifiedName(const AtomString& namespaceURI, const AtomString& qualifiedName)
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

URL Document::baseURLForComplete(const URL& baseURLOverride) const
{
    return ((baseURLOverride.isEmpty() || baseURLOverride == aboutBlankURL()) && parentDocument()) ? parentDocument()->baseURL() : baseURLOverride;
}

URL Document::completeURL(const String& url, const URL& baseURLOverride, ForceUTF8 forceUTF8) const
{
    // See also CSSParserContext::completeURL(const String&)

    // Always return a null URL when passed a null string.
    // FIXME: Should we change the URL constructor to have this behavior?
    if (url.isNull())
        return URL();

    URL baseURL = baseURLForComplete(baseURLOverride);
    // Same logic as openFunc() in XMLDocumentParserLibxml2.cpp. Keep them in sync.
    if (!m_decoder || forceUTF8 == ForceUTF8::Yes)
        return URL(baseURL, url);
    return URL(baseURL, url, m_decoder->encodingForURLParsing());
}

URL Document::completeURL(const String& url, ForceUTF8 forceUTF8) const
{
    return completeURL(url, m_baseURL, forceUTF8);
}

bool Document::shouldMaskURLForBindingsInternal(const URL& urlToMask) const
{
    // Don't mask the URL if it has the same protocol as the document.
    if (urlToMask.protocolIs(url().protocol()))
        return false;

    RefPtr page = this->page();
    if (UNLIKELY(!page))
        return false;

    auto& maskedURLSchemes = page->maskedURLSchemes();
    if (maskedURLSchemes.isEmpty())
        return false;

    return maskedURLSchemes.contains<StringViewHashTranslator>(urlToMask.protocol());
}

static StaticStringImpl maskedURLString { "webkit-masked-url://hidden/" };
StaticStringImpl& Document::maskedURLStringForBindings()
{
    return maskedURLString;
}

const URL& Document::maskedURLForBindings()
{
    // This function can be called from GC heap thread, thus we need to use StaticStringImpl as a source of URL.
    // StaticStringImpl is never converted to AtomString, and it is safe to be used in any threads.
    static LazyNeverDestroyed<URL> url;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [&] {
        url.construct(maskedURLStringForBindings());
        ASSERT(url->string().impl() == &static_cast<StringImpl&>(maskedURLStringForBindings()));
    });
    return url;
}

void Document::setBackForwardCacheState(BackForwardCacheState state)
{
    if (m_backForwardCacheState == state)
        return;

    m_backForwardCacheState = state;

    RefPtr frameView = view();
    RefPtr page = this->page();

    switch (state) {
    case InBackForwardCache:
        if (frameView) {
            // FIXME: There is some scrolling related work that needs to happen whenever a page goes into the
            // back/forward cache and similar work that needs to occur when it comes out. This is where we do the work
            // that needs to happen when we enter, and the work that needs to happen when we exit is in
            // HistoryController::restoreScrollPositionAndViewState(). It can't be here because this function is
            // called too early on in the process of a page exiting the cache for that work to be possible in this
            // function. It would be nice if there was more symmetry here.
            // https://bugs.webkit.org/show_bug.cgi?id=98698
            frameView->cacheCurrentScrollState();
            if (page && m_frame->isMainFrame()) {
                frameView->resetScrollbarsAndClearContentsSize();
                if (RefPtr scrollingCoordinator = page->scrollingCoordinator())
                    scrollingCoordinator->clearAllNodes(m_frame->rootFrame().frameID());
            }
        }

#if ENABLE(POINTER_LOCK)
        exitPointerLock();
#endif

        styleScope().clearResolver();
        m_styleRecalcTimer.stop();

        clearSharedObjectPool();

        if (RefPtr idbConnectionProxy = m_idbConnectionProxy)
            idbConnectionProxy->setContextSuspended(*scriptExecutionContext(), true);
        break;
    case NotInBackForwardCache:
        if (childNeedsStyleRecalc())
            scheduleStyleRecalc();
        if (RefPtr idbConnectionProxy = m_idbConnectionProxy)
            idbConnectionProxy->setContextSuspended(*scriptExecutionContext(), false);
        break;
    case AboutToEnterBackForwardCache:
        break;
    }
}

void Document::documentWillBecomeInactive()
{
    ASSERT_IMPLIES(renderView(), view());
    if (RefPtr frameView = view())
        frameView->setIsInWindow(false);
}

void Document::suspend(ReasonForSuspension reason)
{
    if (m_isSuspended)
        return;

    documentWillBecomeInactive();

    for (Ref element : m_documentSuspensionCallbackElements)
        element->prepareForDocumentSuspension();

#if ASSERT_ENABLED
    // Clear the update flag to be able to check if the viewport arguments update
    // is dispatched, after the document is restored from the back/forward cache.
    m_didDispatchViewportPropertiesChanged = false;
#endif

    if (auto viewTransition = m_activeViewTransition)
        viewTransition->skipTransition();

    if (RefPtr page = this->page())
        page->lockAllOverlayScrollbarsToHidden(true);

    if (CheckedPtr view = renderView()) {
        if (view->usesCompositing())
            view->compositor().cancelCompositingLayerUpdate();
    }

#if ENABLE(WEB_RTC)
    if (RefPtr rtcNetworkManager = m_rtcNetworkManager)
        rtcNetworkManager->unregisterMDNSNames();
#endif

    if (settings().serviceWorkersEnabled() && reason == ReasonForSuspension::BackForwardCache)
        setServiceWorkerConnection(nullptr);

    suspendScheduledTasks(reason);

    ASSERT(m_frame);
    m_frame->clearTimers();

    addVisualUpdatePreventedReason(VisualUpdatesPreventedReason::Suspension);

    if (auto* fontLoader = m_fontLoader.get())
        fontLoader->suspendFontLoading();

    m_isSuspended = true;
}

void Document::resume(ReasonForSuspension reason)
{
    if (!m_isSuspended)
        return;

    for (auto element : copyToVectorOf<Ref<Element>>(m_documentSuspensionCallbackElements))
        element->resumeFromDocumentSuspension();

    if (CheckedPtr view = renderView())
        view->setIsInWindow(true);

    if (RefPtr page = this->page())
        page->lockAllOverlayScrollbarsToHidden(false);

    ASSERT(m_frame);

    if (m_timelinesController)
        m_timelinesController->resumeAnimations();

    resumeScheduledTasks(reason);

    removeVisualUpdatePreventedReasons(VisualUpdatesPreventedReason::Suspension);

    if (auto* fontLoader = m_fontLoader.get())
        fontLoader->resumeFontLoading();

    m_isSuspended = false;

    if (settings().serviceWorkersEnabled() && reason == ReasonForSuspension::BackForwardCache)
        setServiceWorkerConnection(&ServiceWorkerProvider::singleton().serviceWorkerConnection());
}

void Document::registerForDocumentSuspensionCallbacks(Element& element)
{
    m_documentSuspensionCallbackElements.add(element);
}

void Document::unregisterForDocumentSuspensionCallbacks(Element& element)
{
    m_documentSuspensionCallbackElements.remove(element);
}

bool Document::requiresUserGestureForAudioPlayback() const
{
    if (RefPtr loader = this->loader()) {
        // If an audio playback policy was set during navigation, use it. If not, use the global settings.
        AutoplayPolicy policy = loader->autoplayPolicy();
        if (policy != AutoplayPolicy::Default)
            return policy == AutoplayPolicy::AllowWithoutSound || policy == AutoplayPolicy::Deny;
    }

    return settings().requiresUserGestureForAudioPlayback();
}

bool Document::requiresUserGestureForVideoPlayback() const
{
    if (RefPtr loader = this->loader()) {
        // If a video playback policy was set during navigation, use it. If not, use the global settings.
        AutoplayPolicy policy = loader->autoplayPolicy();
        if (policy != AutoplayPolicy::Default)
            return policy == AutoplayPolicy::Deny;
    }

    return settings().requiresUserGestureForVideoPlayback();
}

bool Document::mediaDataLoadsAutomatically() const
{
    if (RefPtr loader = this->loader()) {
        AutoplayPolicy policy = loader->autoplayPolicy();
        if (policy != AutoplayPolicy::Default)
            return policy != AutoplayPolicy::Deny;
    }

    return settings().mediaDataLoadsAutomatically();
}

void Document::storageBlockingStateDidChange()
{
    setStorageBlockingPolicy(settings().storageBlockingPolicy());
}

// Used only by WebKitLegacy.
void Document::privateBrowsingStateDidChange(PAL::SessionID sessionID)
{
    if (RefPtr logger = m_logger)
        logger->setEnabled(this, sessionID.isAlwaysOnLoggingAllowed());

#if ENABLE(VIDEO)
    forEachMediaElement([sessionID] (HTMLMediaElement& element) {
        element.privateBrowsingStateDidChange(sessionID);
    });
#endif
}

#if ENABLE(VIDEO)

void Document::registerForCaptionPreferencesChangedCallbacks(HTMLMediaElement& element)
{
    if (RefPtr page = this->page())
        page->group().ensureCaptionPreferences().setInterestedInCaptionPreferenceChanges();

    m_captionPreferencesChangedElements.add(element);
}

void Document::unregisterForCaptionPreferencesChangedCallbacks(HTMLMediaElement& element)
{
    m_captionPreferencesChangedElements.remove(element);
}

void Document::captionPreferencesChanged()
{
    ASSERT(!m_captionPreferencesChangedElements.hasNullReferences());
    m_captionPreferencesChangedElements.forEach([](HTMLMediaElement& element) {
        Ref { element }->captionPreferencesChanged();
    });
}

void Document::setMediaElementShowingTextTrack(const HTMLMediaElement& element)
{
    m_mediaElementShowingTextTrack = element;
}

void Document::clearMediaElementShowingTextTrack()
{
    m_mediaElementShowingTextTrack = nullptr;
}

void Document::updateTextTrackRepresentationImageIfNeeded()
{
    if (RefPtr mediaElement = m_mediaElementShowingTextTrack.get())
        mediaElement->updateTextTrackRepresentationImageIfNeeded();
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
    Ref protectedDocument { *document };

    document->updateStyleIfNeeded();

    RefPtr frame = document->frame();

    if (!frame || frame->document() != document)
        return Editor::Command();

    return frame->editor().command(commandName,
        userInterface ? EditorCommandSource::DOMWithUserInterface : EditorCommandSource::DOM);
}

ExceptionOr<bool> Document::execCommand(const String& commandName, bool userInterface, const std::variant<String, RefPtr<TrustedHTML>>& value)
{
    if (UNLIKELY(!isHTMLDocument() && !isXHTMLDocument()))
        return Exception { ExceptionCode::InvalidStateError, "execCommand is only supported on HTML documents."_s };

    auto stringValueHolder = WTF::switchOn(value,
        [&commandName, this](const String& str) -> ExceptionOr<String> {
            if (commandName != "insertHTML"_s)
                return String(str);
            return trustedTypeCompliantString(TrustedType::TrustedHTML, *scriptExecutionContext(), str, "Document execCommand"_s);
        },
        [](const RefPtr<TrustedHTML>& trustedHtml) -> ExceptionOr<String> {
            return trustedHtml->toString();
        }
    );

    if (stringValueHolder.hasException())
        return stringValueHolder.releaseException();

    EventQueueScope eventQueueScope;
    return command(this, commandName, userInterface).execute(stringValueHolder.releaseReturnValue());
}

ExceptionOr<bool> Document::queryCommandEnabled(const String& commandName)
{
    if (UNLIKELY(!isHTMLDocument() && !isXHTMLDocument()))
        return Exception { ExceptionCode::InvalidStateError, "queryCommandEnabled is only supported on HTML documents."_s };
    return command(this, commandName).isEnabled();
}

ExceptionOr<bool> Document::queryCommandIndeterm(const String& commandName)
{
    if (UNLIKELY(!isHTMLDocument() && !isXHTMLDocument()))
        return Exception { ExceptionCode::InvalidStateError, "queryCommandIndeterm is only supported on HTML documents."_s };
    return command(this, commandName).state() == TriState::Indeterminate;
}

ExceptionOr<bool> Document::queryCommandState(const String& commandName)
{
    if (UNLIKELY(!isHTMLDocument() && !isXHTMLDocument()))
        return Exception { ExceptionCode::InvalidStateError, "queryCommandState is only supported on HTML documents."_s };
    return command(this, commandName).state() == TriState::True;
}

ExceptionOr<bool> Document::queryCommandSupported(const String& commandName)
{
    if (UNLIKELY(!isHTMLDocument() && !isXHTMLDocument()))
        return Exception { ExceptionCode::InvalidStateError, "queryCommandSupported is only supported on HTML documents."_s };
    return command(this, commandName).isSupported();
}

ExceptionOr<String> Document::queryCommandValue(const String& commandName)
{
    if (UNLIKELY(!isHTMLDocument() && !isXHTMLDocument()))
        return Exception { ExceptionCode::InvalidStateError, "queryCommandValue is only supported on HTML documents."_s };
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

        Ref processor = XSLTProcessor::create();
        processor->setXSLStyleSheet(downcast<XSLStyleSheet>(processingInstruction->sheet()));
        String resultMIMEType;
        String newSource;
        String resultEncoding;
        if (!processor->transformToString(*this, resultMIMEType, newSource, resultEncoding))
            continue;
        // FIXME: If the transform failed we should probably report an error (like Mozilla does).
        processor->createDocumentFromSource(newSource, resultEncoding, resultMIMEType, this, protectedFrame().get());
    }
}

void Document::setTransformSource(std::unique_ptr<TransformSource> source)
{
    m_transformSource = WTFMove(source);
}

#endif

String Document::designMode() const
{
    return inDesignMode() ? onAtom() : offAtom();
}

void Document::setDesignMode(const String& value)
{
    DesignMode mode = equalLettersIgnoringASCIICase(value, "on"_s) ? DesignMode::On : DesignMode::Off;
    m_designMode = mode;
    scheduleFullStyleRebuild();
}

Document* Document::parentDocument() const
{
    if (!m_frame)
        return nullptr;
    auto* parent = dynamicDowncast<LocalFrame>(m_frame->tree().parent());
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
        Document* mainFrameDocument = nullptr;
        if (auto* localFrame = dynamicDowncast<LocalFrame>(m_frame->mainFrame()))
            mainFrameDocument = localFrame->document();
        return mainFrameDocument ? *mainFrameDocument : const_cast<Document&>(*this);
    }

    Document* document = const_cast<Document*>(this);
    while (HTMLFrameOwnerElement* element = document->ownerElement())
        document = &element->document();
    return *document;
}

bool Document::isTopDocument() const
{
    if (!settings().siteIsolationEnabled())
        return isTopDocumentLegacy();

    if (WeakPtr currentFrame = frame()) {
        if (auto localMainFrame = dynamicDowncast<LocalFrame>(currentFrame->mainFrame()))
            return localMainFrame->document() == this;
    }

    return false;
}

ScriptRunner& Document::ensureScriptRunner()
{
    ASSERT(!m_scriptRunner);
    m_scriptRunner = makeUnique<ScriptRunner>(*this);
    return *m_scriptRunner;
}

ScriptModuleLoader& Document::ensureModuleLoader()
{
    ASSERT(!m_moduleLoader);
    m_moduleLoader = makeUnique<ScriptModuleLoader>(this, ScriptModuleLoader::OwnerType::Document);
    return *m_moduleLoader;
}

CheckedRef<ScriptRunner> Document::checkedScriptRunner()
{
    return scriptRunner();
}

ExceptionOr<Ref<Attr>> Document::createAttribute(const AtomString& localName)
{
    if (!isValidName(localName))
        return Exception { ExceptionCode::InvalidCharacterError, makeString("Invalid qualified name: '"_s, localName, '\'') };
    return Attr::create(*this, QualifiedName { nullAtom(), isHTMLDocument() ? localName.convertToASCIILowercase() : localName, nullAtom() }, emptyAtom());
}

ExceptionOr<Ref<Attr>> Document::createAttributeNS(const AtomString& namespaceURI, const AtomString& qualifiedName, bool shouldIgnoreNamespaceChecks)
{
    auto parseResult = parseQualifiedName(namespaceURI, qualifiedName);
    if (parseResult.hasException())
        return parseResult.releaseException();
    QualifiedName parsedName { parseResult.releaseReturnValue() };
    if (!shouldIgnoreNamespaceChecks && !hasValidNamespaceForAttributes(parsedName))
        return Exception { ExceptionCode::NamespaceError };
    return Attr::create(*this, parsedName, emptyAtom());
}

SVGDocumentExtensions& Document::svgExtensions()
{
    if (!m_svgExtensions)
        m_svgExtensions = makeUnique<SVGDocumentExtensions>(*this);
    return *m_svgExtensions;
}

CheckedRef<SVGDocumentExtensions> Document::checkedSVGExtensions()
{
    return svgExtensions();
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
    return ensureCachedCollection<CollectionType::DocImages>();
}

Ref<HTMLCollection> Document::applets()
{
    return ensureRareData().ensureNodeLists().addCachedCollection<EmptyHTMLCollection>(*this, CollectionType::DocEmpty);
}

Ref<HTMLCollection> Document::embeds()
{
    return ensureCachedCollection<CollectionType::DocEmbeds>();
}

Ref<HTMLCollection> Document::scripts()
{
    return ensureCachedCollection<CollectionType::DocScripts>();
}

Ref<HTMLCollection> Document::links()
{
    return ensureCachedCollection<CollectionType::DocLinks>();
}

Ref<HTMLCollection> Document::forms()
{
    return ensureCachedCollection<CollectionType::DocForms>();
}

Ref<HTMLCollection> Document::anchors()
{
    return ensureCachedCollection<CollectionType::DocAnchors>();
}

Ref<HTMLCollection> Document::all()
{
    return ensureRareData().ensureNodeLists().addCachedCollection<HTMLAllCollection>(*this, CollectionType::DocAll);
}

Ref<HTMLCollection> Document::allFilteredByName(const AtomString& name)
{
    return ensureRareData().ensureNodeLists().addCachedCollection<HTMLAllNamedSubCollection>(*this, CollectionType::DocumentAllNamedItems, name);
}

Ref<HTMLCollection> Document::windowNamedItems(const AtomString& name)
{
    return ensureRareData().ensureNodeLists().addCachedCollection<WindowNameCollection>(*this, CollectionType::WindowNamedItems, name);
}

Ref<HTMLCollection> Document::documentNamedItems(const AtomString& name)
{
    return ensureRareData().ensureNodeLists().addCachedCollection<DocumentNameCollection>(*this, CollectionType::DocumentNamedItems, name);
}

Ref<NodeList> Document::getElementsByName(const AtomString& elementName)
{
    return ensureRareData().ensureNodeLists().addCacheWithAtomName<NameNodeList>(*this, elementName);
}

void Document::finishedParsing()
{
    ASSERT(!scriptableDocumentParser() || !m_parser->isParsing());
    ASSERT(!scriptableDocumentParser() || m_readyState != ReadyState::Loading);
    setParsing(false);

    Ref protectedThis { *this };

    if (CheckedPtr scriptRunner = m_scriptRunner.get())
        scriptRunner->documentFinishedParsing();

    if (!m_eventTiming.domContentLoadedEventStart) {
        auto now = MonotonicTime::now();
        m_eventTiming.domContentLoadedEventStart = now;
        if (auto* eventTiming = documentEventTimingFromNavigationTiming())
            eventTiming->domContentLoadedEventStart = now;
        WTFEmitSignpost(this, NavigationAndPaintTiming, "domContentLoadedEventBegin");
    }

    // FIXME: Schedule a task to fire DOMContentLoaded event instead. See webkit.org/b/82931
    RefPtr documentLoader = loader();
    bool isInMiddleOfInitializingIframe = documentLoader && documentLoader->isInFinishedLoadingOfEmptyDocument();
    if (!isInMiddleOfInitializingIframe)
        eventLoop().performMicrotaskCheckpoint();

    dispatchEvent(Event::create(eventNames().DOMContentLoadedEvent, Event::CanBubble::Yes, Event::IsCancelable::No));

    if (!m_eventTiming.domContentLoadedEventEnd) {
        auto now = MonotonicTime::now();
        m_eventTiming.domContentLoadedEventEnd = now;
        if (auto* eventTiming = documentEventTimingFromNavigationTiming())
            eventTiming->domContentLoadedEventEnd = now;
        WTFEmitSignpost(this, NavigationAndPaintTiming, "domContentLoadedEventEnd");
    }

    if (RefPtr frame = this->frame()) {
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

        frame->protectedLoader()->finishedParsing();
        InspectorInstrumentation::domContentLoadedEventFired(*frame);
    }

    // Schedule dropping of the DocumentSharedObjectPool. We keep it alive for a while after parsing finishes
    // so that dynamically inserted content can also benefit from sharing optimizations.
    // Note that we don't refresh the timer on pool access since that could lead to huge caches being kept
    // alive indefinitely by something innocuous like JS setting .innerHTML repeatedly on a timer.
    static const Seconds timeToKeepSharedObjectPoolAliveAfterParsingFinished { 10_s };
    m_sharedObjectPoolClearTimer.startOneShot(timeToKeepSharedObjectPoolAliveAfterParsingFinished);

    // Parser should have picked up all speculative preloads by now
    if (m_cachedResourceLoader)
        m_cachedResourceLoader->clearPreloads(CachedResourceLoader::ClearPreloadsMode::ClearSpeculativePreloads);

    if (settings().serviceWorkersEnabled()) {
        // Stop queuing service worker client messages now that the DOMContentLoaded event has been fired.
        if (RefPtr serviceWorkerContainer = this->serviceWorkerContainer())
            serviceWorkerContainer->startMessages();
    }

#if ENABLE(APP_HIGHLIGHTS)
    if (auto* appHighlightStorage = appHighlightStorageIfExists())
        appHighlightStorage->restoreUnrestoredAppHighlights();
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
    ASSERT(m_constructionDidFinish);
    auto origin = securityOrigin().toString();
    if (origin != "null"_s)
        return origin;
    if (!m_uniqueIdentifier)
        m_uniqueIdentifier = makeString("null:"_s, WTF::UUID::createVersion4Weak());
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
        ASSERT(m_cookieURL.isNull());
        setEmptySecurityOriginPolicyAndContentSecurityPolicy();
        return;
    }

    // In the common case, create the security context from the currently
    // loading URL with a fresh content security policy.
    setCookieURL(m_url);

    // Flags from CSP will be added when the response is received, but should not be carried over to the frame's next document.
    enforceSandboxFlags(m_frame->sandboxFlagsFromSandboxAttributeNotCSP());

    setReferrerPolicy(m_frame->loader().effectiveReferrerPolicy());

    if (shouldEnforceContentDispositionAttachmentSandbox())
        applyContentDispositionAttachmentSandbox();

    RefPtr documentLoader = m_frame->loader().documentLoader();
    bool isSecurityOriginUnique = isSandboxed(SandboxFlag::Origin);
    if (!isSecurityOriginUnique)
        isSecurityOriginUnique = documentLoader && documentLoader->response().tainting() == ResourceResponse::Tainting::Opaque;

    setSecurityOriginPolicy(SecurityOriginPolicy::create(isSecurityOriginUnique ? SecurityOrigin::createOpaque() : SecurityOrigin::create(m_url)));
    setContentSecurityPolicy(makeUnique<ContentSecurityPolicy>(URL { m_url }, *this));

    String overrideContentSecurityPolicy = m_frame->loader().client().overrideContentSecurityPolicy();
    if (!overrideContentSecurityPolicy.isNull())
        checkedContentSecurityPolicy()->didReceiveHeader(overrideContentSecurityPolicy, ContentSecurityPolicyHeaderType::Enforce, ContentSecurityPolicy::PolicyFrom::API, referrer(), documentLoader ? documentLoader->response().httpStatusCode() : 0);

#if USE(QUICK_LOOK)
    if (shouldEnforceQuickLookSandbox())
        applyQuickLookSandbox();
#endif

    if (shouldEnforceHTTP09Sandbox()) {
        auto message = makeString("Sandboxing '"_s, m_url.url().stringCenterEllipsizedToLength(), "' because it is using HTTP/0.9."_s);
        addConsoleMessage(MessageSource::Security, MessageLevel::Error, message);
        enforceSandboxFlags({ SandboxFlag::Scripts, SandboxFlag::Plugins });
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
            // still can have other privileges that can be remembered, thereby not making them opaque origins.
            securityOrigin().setEnforcesFilePathSeparation();
        }
    }

    RefPtr parentDocument = ownerElement() ? &ownerElement()->document() : nullptr;
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
    RefPtr parentFrame = m_frame->tree().parent();
    RefPtr openerFrame = dynamicDowncast<LocalFrame>(m_frame->opener());

    RefPtr ownerFrame = dynamicDowncast<LocalFrame>(parentFrame.get());
    if (!ownerFrame)
        ownerFrame = openerFrame;

    if (!ownerFrame) {
        didFailToInitializeSecurityOrigin();
        return;
    }

    CheckedPtr contentSecurityPolicy = this->contentSecurityPolicy();
    contentSecurityPolicy->copyStateFrom(ownerFrame->protectedDocument()->checkedContentSecurityPolicy().get());
    contentSecurityPolicy->updateSourceSelf(ownerFrame->document()->protectedSecurityOrigin());

    setCrossOriginEmbedderPolicy(ownerFrame->document()->crossOriginEmbedderPolicy());

    // https://html.spec.whatwg.org/multipage/browsers.html#creating-a-new-browsing-context (Step 12)
    // If creator is non-null and creator's origin is same origin with creator's relevant settings object's top-level origin, then set coop
    // to creator's browsing context's top-level browsing context's active document's cross-origin opener policy.
    if (m_frame->isMainFrame() && openerFrame && openerFrame->document() && openerFrame->document()->isSameOriginAsTopDocument())
        setCrossOriginOpenerPolicy(openerFrame->document()->crossOriginOpenerPolicy());

    // Per <http://www.w3.org/TR/upgrade-insecure-requests/>, new browsing contexts must inherit from an
    // ongoing set of upgraded requests. When opening a new browsing context, we need to capture its
    // existing upgrade request. Nested browsing contexts are handled during DocumentWriter::begin.
    if (RefPtr openerDocument = openerFrame ? openerFrame->document() : nullptr)
        contentSecurityPolicy->inheritInsecureNavigationRequestsToUpgradeFromOpener(*openerDocument->checkedContentSecurityPolicy());

    if (isSandboxed(SandboxFlag::Origin)) {
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
    if (!m_frame)
        return;
    RefPtr parentFrame = dynamicDowncast<LocalFrame>(m_frame->tree().parent());
    if (parentFrame)
        checkedContentSecurityPolicy()->copyUpgradeInsecureRequestStateFrom(*parentFrame->protectedDocument()->checkedContentSecurityPolicy());

    // FIXME: Remove this special plugin document logic. We are stricter than the CSP 3 spec. with regards to plugins: we prefer to
    // inherit the full policy unless the plugin document is opened in a new window. The CSP 3 spec. implies that only plugin documents
    // delivered with a local scheme (e.g. blob, file, data) should inherit a policy.
    if (!isPluginDocument())
        return;
    RefPtr openerFrame = dynamicDowncast<LocalFrame>(m_frame->opener());
    bool shouldInhert = parentFrame || (openerFrame && openerFrame->document()->protectedSecurityOrigin()->isSameOriginDomain(securityOrigin()));
    if (!shouldInhert)
        return;
    setContentSecurityPolicy(makeUnique<ContentSecurityPolicy>(URL { m_url }, *this));
    if (openerFrame)
        checkedContentSecurityPolicy()->createPolicyForPluginDocumentFrom(*openerFrame->protectedDocument()->checkedContentSecurityPolicy());
    else
        checkedContentSecurityPolicy()->copyStateFrom(parentFrame->protectedDocument()->checkedContentSecurityPolicy().get());
}

void Document::inheritPolicyContainerFrom(const PolicyContainer& policyContainer)
{
    setContentSecurityPolicy(makeUnique<ContentSecurityPolicy>(URL { m_url }, *this));
    SecurityContext::inheritPolicyContainerFrom(policyContainer);
}

// https://html.spec.whatwg.org/#the-rules-for-choosing-a-browsing-context-given-a-browsing-context-name (Step 8.2)
bool Document::shouldForceNoOpenerBasedOnCOOP() const
{
    if (!settings().crossOriginOpenerPolicyEnabled())
        return false;

    auto COOPValue = topDocument().crossOriginOpenerPolicy().value;
    return (COOPValue == CrossOriginOpenerPolicyValue::SameOrigin || COOPValue == CrossOriginOpenerPolicyValue::SameOriginPlusCOEP) && !isSameOriginAsTopDocument();
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
    if (document.isSandboxed(SandboxFlag::Origin))
        return isURLPotentiallyTrustworthy(document.url());
    return document.securityOrigin().isPotentiallyTrustworthy();
}

// https://w3c.github.io/webappsec-secure-contexts/#is-settings-object-contextually-secure
bool Document::isSecureContext() const
{
    if (!m_frame)
        return true;
    if (!settings().secureContextChecksEnabled())
        return true;
    if (page() && page()->isServiceWorkerPage())
        return true;

    for (auto* frame = m_frame->tree().parent(); frame; frame = frame->tree().parent()) {
        auto* localFrame = dynamicDowncast<LocalFrame>(frame);
        if (!localFrame)
            continue;
        Ref<Document> ancestorDocument = *localFrame->document();
        if (!isDocumentSecure(ancestorDocument))
            return false;
    }

    return isDocumentSecure(*this);
}

void Document::updateURLForPushOrReplaceState(const URL& url)
{
    RefPtr frame = this->frame();
    if (!frame)
        return;

    setURL(url);
    frame->loader().setOutgoingReferrer(url);

    if (RefPtr documentLoader = loader())
        documentLoader->replaceRequestURLForSameDocumentNavigation(url);
}

void Document::statePopped(Ref<SerializedScriptValue>&& stateObject)
{
    if (!frame())
        return;

    dispatchPopstateEvent(WTFMove(stateObject));
}

void Document::attachRange(Range& range)
{
    ASSERT(!m_ranges.contains(range));
    m_ranges.add(range);
}

void Document::detachRange(Range& range)
{
    // We don't ASSERT m_ranges.contains(&range) to allow us to call this
    // unconditionally to fix: https://bugs.webkit.org/show_bug.cgi?id=26044
    m_ranges.remove(range);
}

std::optional<RenderingContext> Document::getCSSCanvasContext(const String& type, const String& name, int width, int height)
{
    RefPtr element = getCSSCanvasElement(name);
    if (!element)
        return std::nullopt;
    element->setSize({ width, height });
    auto context = element->getContext(type);
    if (!context)
        return std::nullopt;

#if ENABLE(WEBGL)
    if (RefPtr renderingContext = dynamicDowncast<WebGLRenderingContext>(*context))
        return RenderingContext { WTFMove(renderingContext) };

    if (RefPtr renderingContext = dynamicDowncast<WebGL2RenderingContext>(*context))
        return RenderingContext { WTFMove(renderingContext) };
#endif

    if (RefPtr renderingContext = dynamicDowncast<ImageBitmapRenderingContext>(*context))
        return RenderingContext { WTFMove(renderingContext) };

    if (RefPtr gpuCanvasContext = dynamicDowncast<GPUCanvasContext>(*context))
        return RenderingContext { WTFMove(gpuCanvasContext) };

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
    m_isDNSPrefetchEnabled = settings().dnsPrefetchingEnabled() && securityOrigin().protocol() == "http"_s ? TriState::True : TriState::False;

    // Inherit DNS prefetch opt-out from parent frame
    if (RefPtr parent = parentDocument()) {
        if (!parent->isDNSPrefetchEnabled())
            m_isDNSPrefetchEnabled = TriState::False;
    }
}

bool Document::isDNSPrefetchEnabled() const
{
    if (m_isDNSPrefetchEnabled == TriState::Indeterminate)
        const_cast<Document&>(*this).initDNSPrefetch();
    return m_isDNSPrefetchEnabled == TriState::True;
}

void Document::parseDNSPrefetchControlHeader(const String& dnsPrefetchControl)
{
    if (!settings().dnsPrefetchingEnabled())
        return;

    if (m_isDNSPrefetchEnabled == TriState::Indeterminate)
        initDNSPrefetch();

    if (equalLettersIgnoringASCIICase(dnsPrefetchControl, "on"_s) && !m_haveExplicitlyDisabledDNSPrefetch) {
        m_isDNSPrefetchEnabled = TriState::True;
        return;
    }

    m_isDNSPrefetchEnabled = TriState::False;
    m_haveExplicitlyDisabledDNSPrefetch = true;
}

void Document::getParserLocation(String& completedURL, unsigned& line, unsigned& column) const
{
    // We definitely cannot associate the message with a location being parsed if we are not even parsing.
    if (!parsing())
        return;

    RefPtr parser = scriptableDocumentParser();
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

    if (RefPtr page = this->page())
        page->console().addMessage(WTFMove(consoleMessage));
}

void Document::addConsoleMessage(MessageSource source, MessageLevel level, const String& message, unsigned long requestIdentifier)
{
    if (!isContextThread()) {
        postTask(AddConsoleMessageTask(source, level, message));
        return;
    }

    if (RefPtr page = this->page())
        page->console().addMessage(source, level, message, requestIdentifier, this);

    if (RefPtr consoleMessageListener = m_consoleMessageListener)
        consoleMessageListener->scheduleCallback(*this, message);
}

void Document::addMessage(MessageSource source, MessageLevel level, const String& message, const String& sourceURL, unsigned lineNumber, unsigned columnNumber, RefPtr<Inspector::ScriptCallStack>&& callStack, JSC::JSGlobalObject* state, unsigned long requestIdentifier)
{
    if (!isContextThread()) {
        postTask(AddConsoleMessageTask(source, level, message));
        return;
    }

    if (RefPtr page = this->page())
        page->console().addMessage(source, level, message, sourceURL, lineNumber, columnNumber, WTFMove(callStack), state, requestIdentifier);
}

void Document::postTask(Task&& task)
{
    callOnMainThread([documentID = identifier(), task = WTFMove(task)]() mutable {
        ASSERT(isMainThread());

        RefPtr document = allDocumentsMap().get(documentID);
        if (!document)
            return;

        RefPtr page = document->page();
        if ((page && page->defersLoading() && document->activeDOMObjectsAreSuspended()) || !document->m_pendingTasks.isEmpty())
            document->m_pendingTasks.append(WTFMove(task));
        else
            task.performTask(*document);
    });
}

void Document::pendingTasksTimerFired()
{
    Ref protectedThis { *this };
    auto pendingTasks = std::exchange(m_pendingTasks, Vector<Task> { });
    for (auto& task : pendingTasks)
        task.performTask(*this);
}

EventLoopTaskGroup& Document::eventLoop()
{
    ASSERT(isMainThread());
    if (UNLIKELY(!m_documentTaskGroup)) {
        m_documentTaskGroup = makeUnique<EventLoopTaskGroup>(windowEventLoop());
        if (activeDOMObjectsAreStopped())
            m_documentTaskGroup->markAsReadyToStop();
        else if (activeDOMObjectsAreSuspended())
            m_documentTaskGroup->suspend();
    }
    return *m_documentTaskGroup;
}

WindowEventLoop& Document::windowEventLoop()
{
    ASSERT(isMainThread());
    if (UNLIKELY(!m_eventLoop)) {
        m_eventLoop = WindowEventLoop::eventLoopForSecurityOrigin(securityOrigin());
        m_eventLoop->addAssociatedContext(*this);
    }
    return *m_eventLoop;
}

Ref<WindowEventLoop> Document::protectedWindowEventLoop()
{
    return windowEventLoop();
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
    if (CheckedPtr scriptRunner = m_scriptRunner.get())
        scriptRunner->suspend();
    m_pendingTasksTimer.stop();

#if ENABLE(XSLT)
    m_applyPendingXSLTransformsTimer.stop();
#endif

    // Deferring loading and suspending parser is necessary when we need to prevent re-entrant JavaScript execution
    // (e.g. while displaying an alert).
    // It is not currently possible to suspend parser unless loading is deferred, because new data arriving from network
    // will trigger parsing, and leave the scheduler in an inconsistent state where it doesn't know whether it's suspended or not.
    if (reason == ReasonForSuspension::WillDeferLoading && m_parser)
        protectedParser()->suspendScheduledTasks();

    m_scheduledTasksAreSuspended = true;
}

void Document::resumeScheduledTasks(ReasonForSuspension reason)
{
    if (reasonForSuspendingActiveDOMObjects() != reason)
        return;

    ASSERT(m_scheduledTasksAreSuspended);

    if (reason == ReasonForSuspension::WillDeferLoading && m_parser)
        protectedParser()->resumeScheduledTasks();

#if ENABLE(XSLT)
    if (m_hasPendingXSLTransforms)
        m_applyPendingXSLTransformsTimer.startOneShot(0_s);
#endif

    if (!m_pendingTasks.isEmpty())
        m_pendingTasksTimer.startOneShot(0_s);
    if (CheckedPtr scriptRunner = m_scriptRunner.get())
        scriptRunner->resume();
    resumeActiveDOMObjects(reason);
    resumeScriptedAnimationControllerCallbacks();

    m_scheduledTasksAreSuspended = false;
}

void Document::suspendScriptedAnimationControllerCallbacks()
{
    if (RefPtr scriptedAnimationController = m_scriptedAnimationController)
        scriptedAnimationController->suspend();
}

void Document::resumeScriptedAnimationControllerCallbacks()
{
    if (RefPtr scriptedAnimationController = m_scriptedAnimationController)
        scriptedAnimationController->resume();
}

void Document::serviceRequestAnimationFrameCallbacks()
{
    RefPtr scriptedAnimationController = m_scriptedAnimationController;
    if (scriptedAnimationController && domWindow())
        scriptedAnimationController->serviceRequestAnimationFrameCallbacks(domWindow()->frozenNowTimestamp());
}

void Document::serviceCaretAnimation()
{
    selection().caretAnimator().serviceCaretAnimation();
}

void Document::serviceRequestVideoFrameCallbacks()
{
#if ENABLE(VIDEO)
    if (!domWindow())
        return;

    bool isServicingRequestVideoFrameCallbacks = false;
    forEachMediaElement([now = domWindow()->frozenNowTimestamp(), &isServicingRequestVideoFrameCallbacks](auto& element) {
        RefPtr videoElement = dynamicDowncast<HTMLVideoElement>(element);
        if (videoElement && videoElement->shouldServiceRequestVideoFrameCallbacks()) {
            isServicingRequestVideoFrameCallbacks = true;
            videoElement->serviceRequestVideoFrameCallbacks(now);
        }
    });

    if (!isServicingRequestVideoFrameCallbacks)
        return;

    if (RefPtr page = this->page())
        page->scheduleRenderingUpdate(RenderingUpdateStep::VideoFrameCallbacks);
#endif
}

void Document::windowScreenDidChange(PlatformDisplayID displayID)
{
    for (auto& observer : copyToVector(m_displayChangedObservers)) {
        if (observer)
            (*observer)(displayID);
    }
}

String Document::displayStringModifiedByEncoding(const String& string) const
{
    if (!m_decoder)
        return string;
    return makeStringByReplacingAll(string, '\\', m_decoder->encoding().backslashAsCurrencySymbol());
}

void Document::dispatchPageshowEvent(PageshowEventPersistence persisted)
{
    // Pausing a page may trigger pagehide and pageshow events. WebCore also implicitly fires these
    // events when closing a WebView. Here we keep track of the state of the page to prevent duplicate,
    // unbalanced events per the definition of the pageshow event:
    // https://html.spec.whatwg.org/multipage/browsing-the-web.html#reactivate-a-document
    if (m_lastPageStatus == PageStatus::Shown)
        return;
    m_lastPageStatus = PageStatus::Shown;
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=36334 Pageshow event needs to fire asynchronously.
    dispatchWindowEvent(PageTransitionEvent::create(eventNames().pageshowEvent, persisted == PageshowEventPersistence::Persisted), this);
}

void Document::dispatchPagehideEvent(PageshowEventPersistence persisted)
{
    // Pausing a page may trigger pagehide and pageshow events. WebCore also implicitly fires these
    // events when closing a WebView. Here we keep track of the state of the page to prevent duplicate,
    // unbalanced events per the definition of the pageshow event:
    // https://html.spec.whatwg.org/multipage/document-lifecycle.html#unload-a-document
    if (m_lastPageStatus == PageStatus::Hidden)
        return;
    m_lastPageStatus = PageStatus::Hidden;
    dispatchWindowEvent(PageTransitionEvent::create(eventNames().pagehideEvent, persisted == PageshowEventPersistence::Persisted), this);
}

// https://www.w3.org/TR/css-view-transitions-2/#vt-rule-algo
std::variant<Document::SkipTransition, Vector<AtomString>> Document::resolveViewTransitionRule()
{
    if (hidden())
        return SkipTransition { };

    auto rule = styleScope().resolver().viewTransitionRule();
    if (rule && rule->computedNavigation() == ViewTransitionNavigation::Auto)
        return rule->types();
    return SkipTransition { };
}

// https://html.spec.whatwg.org/multipage/browsing-the-web.html#revealing-the-document
void Document::reveal()
{
    if (!settings().crossDocumentViewTransitionsEnabled())
        return;

    if (m_hasBeenRevealed)
        return;
    m_hasBeenRevealed = true;

    PageRevealEvent::Init init;

    RefPtr inboundTransition = ViewTransition::resolveInboundCrossDocumentViewTransition(*this, std::exchange(m_inboundViewTransitionParams, nullptr));
    if (inboundTransition)
        init.viewTransition = inboundTransition;

    dispatchWindowEvent(PageRevealEvent::create(eventNames().pagerevealEvent, WTFMove(init)), this);

    if (inboundTransition) {
        // FIXME: Prepare to run script given document.

        inboundTransition->activateViewTransition();

        // FIXME: Clean up after running script given document.
        eventLoop().performMicrotaskCheckpoint();
    }
}

void Document::transferViewTransitionParams(Document& newDocument)
{
    newDocument.m_inboundViewTransitionParams = std::exchange(m_inboundViewTransitionParams, nullptr);
}

void Document::dispatchPageswapEvent(bool canTriggerCrossDocumentViewTransition, RefPtr<NavigationActivation>&& activation)
{
    if (!settings().crossDocumentViewTransitionsEnabled())
        return;

    RefPtr<ViewTransition> oldViewTransition;

    auto startTime = MonotonicTime::now();
    PageSwapEvent::Init swapInit;
    swapInit.activation = WTFMove(activation);
    if (canTriggerCrossDocumentViewTransition && globalObject()) {
        oldViewTransition = ViewTransition::setupCrossDocumentViewTransition(*this);
        swapInit.viewTransition = oldViewTransition;
    }

    dispatchWindowEvent(PageSwapEvent::create(eventNames().pageswapEvent, WTFMove(swapInit)), this);

    // FIXME: This should actually defer the navigation, and run the setupViewTransition
    // (capture the old state) on the next rendering update.
    // https://html.spec.whatwg.org/multipage/browsing-the-web.html#deactivate-a-document-for-a-cross-document-navigation
    if (oldViewTransition && oldViewTransition->phase() != ViewTransitionPhase::Done) {
        oldViewTransition->setupViewTransition();

        // FIXME: This should set the params on the new Document, but it doesn't exist yet.
        // Store it on the old, and we'll call transferViewTransitionParams soon.
        m_inboundViewTransitionParams = oldViewTransition->takeViewTransitionParams().moveToUniquePtr();
        m_inboundViewTransitionParams->startTime = startTime;
    }
}

void Document::enqueueSecurityPolicyViolationEvent(SecurityPolicyViolationEventInit&& eventInit)
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
    auto event = PopStateEvent::create(WTFMove(stateObject), m_domWindow ? &m_domWindow->history() : nullptr);
    event->setHasUAVisualTransition(page() && page()->isInSwipeAnimation());
    dispatchWindowEvent(event);
}

void Document::addMediaCanStartListener(MediaCanStartListener& listener)
{
    m_mediaCanStartListeners.add(listener);
}

void Document::removeMediaCanStartListener(MediaCanStartListener& listener)
{
    ASSERT(m_mediaCanStartListeners.contains(listener));
    m_mediaCanStartListeners.remove(listener);
}

MediaCanStartListener* Document::takeAnyMediaCanStartListener()
{
    if (m_mediaCanStartListeners.isEmptyIgnoringNullReferences())
        return nullptr;

    RefPtr listener = m_mediaCanStartListeners.begin().get();
    m_mediaCanStartListeners.remove(*listener);

    return listener.get();
}

void Document::addDisplayChangedObserver(const DisplayChangedObserver& observer)
{
    ASSERT(!m_displayChangedObservers.contains(observer));
    m_displayChangedObservers.add(observer);
}

#if HAVE(SPATIAL_TRACKING_LABEL)
const String& Document::defaultSpatialTrackingLabel() const
{
    if (RefPtr page = protectedPage())
        return page->defaultSpatialTrackingLabel();
    return emptyString();
}

void Document::defaultSpatialTrackingLabelChanged(const String& defaultSpatialTrackingLabel)
{
    for (auto& observer : copyToVector(m_defaultSpatialTrackingLabelChangedObservers)) {
        if (observer)
            (*observer)(defaultSpatialTrackingLabel);
    }
}

void Document::addDefaultSpatialTrackingLabelChangedObserver(const DefaultSpatialTrackingLabelChangedObserver& observer)
{
    ASSERT(!m_defaultSpatialTrackingLabelChangedObservers.contains(observer));
    m_defaultSpatialTrackingLabelChangedObservers.add(observer);
}
#endif


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
    auto orientation = DeviceOrientationData::create(alpha, beta, gamma, std::nullopt, std::nullopt);
    deviceOrientationController().didChangeDeviceOrientation(orientation.ptr());
}

#endif

#if ENABLE(POINTER_LOCK)

void Document::exitPointerLock()
{
    RefPtr page = this->page();
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
    Ref protectedThis { *this };
    checkCompleted();
    if (RefPtr frame = this->frame())
        frame->protectedLoader()->checkLoadComplete();
}

void Document::checkCompleted()
{
    if (RefPtr frame = this->frame())
        frame->protectedLoader()->checkCompleted();
}

double Document::monotonicTimestamp() const
{
    RefPtr loader = this->loader();
    if (!loader)
        return 0.0;
    return (MonotonicTime::now() - loader->timing().startTime()).seconds();
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

        if (!topOrigin().isSameOriginDomain(securityOrigin()) && !hasHadUserInteraction())
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
    if (RefPtr scriptedAnimationController = std::exchange(m_scriptedAnimationController, nullptr))
        scriptedAnimationController->clearDocumentPointer();
}

// https://html.spec.whatwg.org/multipage/dom.html#render-blocking-mechanism
bool Document::allowsAddingRenderBlockedElements() const
{
    return contentType() == "text/html"_s && !body();
}

bool Document::isRenderBlocked() const
{
    return m_visualUpdatesPreventedReasons.contains(VisualUpdatesPreventedReason::RenderBlocking);
}

void Document::blockRenderingOn(Element& element, ImplicitRenderBlocking implicit)
{
    if (allowsAddingRenderBlockedElements()) {
        m_renderBlockingElements.add(element);

        // Only forcibly complete the page transition when blocking ends (and override any
        // rendering freezing from WebKit) if the document has explictly opted-into render blocking.
        auto completePageTransition = CompletePageTransition::Yes;
        if (implicit == ImplicitRenderBlocking::Yes)
            completePageTransition = CompletePageTransition::No;

        addVisualUpdatePreventedReason(VisualUpdatesPreventedReason::RenderBlocking, completePageTransition);
    }
}

void Document::unblockRenderingOn(Element& element)
{
    m_renderBlockingElements.remove(element);
    if (m_renderBlockingElements.isEmptyIgnoringNullReferences())
        removeVisualUpdatePreventedReasons(VisualUpdatesPreventedReason::RenderBlocking);
}

// https://html.spec.whatwg.org/multipage/links.html#process-internal-resource-links
void Document::processInternalResourceLinks(HTMLAnchorElement* anchor)
{
    auto copy = copyToVector(m_renderBlockingElements);
    for (auto& element : copy) {
        if (RefPtr link = dynamicDowncast<HTMLLinkElement>(element.get()))
            link->processInternalResourceLink(anchor);
    }
}

CheckedRef<FrameSelection> Document::checkedSelection()
{
    return m_selection.get();
}

CheckedRef<const FrameSelection> Document::checkedSelection() const
{
    return m_selection.get();
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

HttpEquivPolicy Document::httpEquivPolicy() const
{
    if (shouldEnforceContentDispositionAttachmentSandbox())
        return HttpEquivPolicy::DisabledByContentDispositionAttachmentSandbox;
    if (page() && !page()->settings().httpEquivEnabled())
        return HttpEquivPolicy::DisabledBySettings;
    return HttpEquivPolicy::Enabled;
}

void Document::wheelEventHandlersChanged(Node* node)
{
    RefPtr page = this->page();
    if (!page)
        return;

    if (RefPtr frameView = view()) {
        if (RefPtr scrollingCoordinator = page->scrollingCoordinator())
            scrollingCoordinator->frameViewEventTrackingRegionsChanged(*frameView);
    }

#if ENABLE(WHEEL_EVENT_REGIONS)
    if (RefPtr<Element> element = dynamicDowncast<Element>(node)) {
        // Style is affected via eventListenerRegionTypes().
        element->invalidateStyle();
    }

    protectedFrame()->invalidateContentEventRegionsIfNeeded(LocalFrame::InvalidateContentEventRegionsReason::EventHandlerChange);
#else
    UNUSED_PARAM(node);
#endif

    bool haveHandlers = m_wheelEventTargets && !m_wheelEventTargets->isEmptyIgnoringNullReferences();
    page->chrome().client().wheelEventHandlersChanged(haveHandlers);
}

void Document::didAddWheelEventHandler(Node& node)
{
    if (!m_wheelEventTargets)
        m_wheelEventTargets = makeUnique<EventTargetSet>();

    m_wheelEventTargets->add(node);
    wheelEventHandlersChanged(&node);

    if (RefPtr frame = this->frame())
        DebugPageOverlays::didChangeEventHandlers(*frame);
}

static bool removeHandlerFromSet(EventTargetSet& handlerSet, Node& node, EventHandlerRemoval removal)
{
    switch (removal) {
    case EventHandlerRemoval::One:
        return handlerSet.remove(node);
    case EventHandlerRemoval::All:
        return handlerSet.removeAll(node);
    }
    return false;
}

void Document::didRemoveWheelEventHandler(Node& node, EventHandlerRemoval removal)
{
    if (!m_wheelEventTargets)
        return;

    if (!removeHandlerFromSet(*m_wheelEventTargets, node, removal))
        return;

    wheelEventHandlersChanged(&node);

    if (RefPtr frame = this->frame())
        DebugPageOverlays::didChangeEventHandlers(*frame);
}

unsigned Document::wheelEventHandlerCount() const
{
    if (!m_wheelEventTargets)
        return 0;

    unsigned count = 0;
    for (auto handler : *m_wheelEventTargets)
        count += handler.value;

    return count;
}

void Document::didAddTouchEventHandler(Node& handler)
{
#if ENABLE(TOUCH_EVENTS)
    if (!m_touchEventTargets)
        m_touchEventTargets = makeUnique<EventTargetSet>();

    m_touchEventTargets->add(handler);

    if (RefPtr parent = parentDocument()) {
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

    if (RefPtr parent = parentDocument())
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
        m_touchEventTargets->removeAll(handler);
        if ((&handler == this || m_touchEventTargets->isEmptyIgnoringNullReferences()) && parentDocument())
            protectedParentDocument()->didRemoveEventTargetNode(*this);
    }
#endif

    if (m_wheelEventTargets) {
        m_wheelEventTargets->removeAll(handler);
        if ((&handler == this || m_wheelEventTargets->isEmptyIgnoringNullReferences()) && parentDocument())
            protectedParentDocument()->didRemoveEventTargetNode(*this);
    }
}

unsigned Document::touchEventHandlerCount() const
{
#if ENABLE(TOUCH_EVENTS)
    if (!m_touchEventTargets)
        return 0;

    unsigned count = 0;
    for (auto handler : *m_touchEventTargets)
        count += handler.value;

    return count;
#else
    return 0;
#endif
}

void Document::didAddOrRemoveMouseEventHandler(Node& node)
{
#if ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)
    if (RefPtr element = dynamicDowncast<Element>(node)) {
        // Style is affected via eventListenerRegionTypes().
        element->invalidateStyle();
    }

    if (RefPtr frame = this->frame())
        frame->invalidateContentEventRegionsIfNeeded(LocalFrame::InvalidateContentEventRegionsReason::EventHandlerChange);
#else
    UNUSED_PARAM(node);
#endif

    if (RefPtr frame = this->frame())
        DebugPageOverlays::didChangeEventHandlers(*frame);
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

    if (auto* document = dynamicDowncast<Document>(node)) {
        if (document == this)
            rootRelativeBounds = absoluteEventHandlerBounds(insideFixedPosition);
        else if (RefPtr element = document->ownerElement())
            rootRelativeBounds = element->absoluteEventHandlerBounds(insideFixedPosition);
    } else if (auto* element = dynamicDowncast<Element>(node)) {
        if (is<HTMLBodyElement>(*element)) {
            // For the body, just use the document bounds.
            // The body may not cover this whole area, but it's OK for this region to be an overestimate.
            rootRelativeBounds = absoluteEventHandlerBounds(insideFixedPosition);
        } else
            rootRelativeBounds = element->absoluteEventHandlerBounds(insideFixedPosition);
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

    for (auto keyValuePair : *targets) {
        Ref node = keyValuePair.key;
        auto targetRegionFixedPair = absoluteEventRegionForNode(node);
        targetRegion.unite(targetRegionFixedPair.first);
        insideFixedPosition |= targetRegionFixedPair.second;
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

    if (RefPtr element = ownerElement())
        element->protectedDocument()->updateLastHandledUserGestureTimestamp(time);
}

bool Document::processingUserGestureForMedia() const
{
    if (UserGestureIndicator::processingUserGestureForMedia())
        return true;

    if (m_domWindow && m_domWindow->hasTransientActivation())
        return true;

    if (m_userActivatedMediaFinishedPlayingTimestamp + maxIntervalForUserGestureForwardingAfterMediaFinishesPlaying >= MonotonicTime::now())
        return true;

    if (settings().mediaUserGestureInheritsFromDocument())
        return topDocument().hasHadUserInteraction();

    RefPtr loader = this->loader();
    if (loader && loader->allowedAutoplayQuirks().contains(AutoplayQuirk::InheritedUserGestures))
        return topDocument().hasHadUserInteraction();

    return false;
}

bool Document::hasRecentUserInteractionForNavigationFromJS() const
{
    if (UserGestureIndicator::processingUserGesture(this))
        return true;

    RefPtr window = domWindow();
    if (!window || window->lastActivationTimestamp().isInfinity())
        return false;

    static constexpr Seconds maximumIntervalForUserActivationForwarding { 10_s };
    return (MonotonicTime::now() - window->lastActivationTimestamp()) <= maximumIntervalForUserActivationForwarding;
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
        return !m_contextDocument || m_contextDocument->allowsContentJavaScript();
    }

    return protectedFrame()->loader().client().allowsContentJavaScriptFromMostRecentNavigation() == AllowsContentJavaScript::Yes;
}

Element* eventTargetElementForDocument(Document* document)
{
    if (!document)
        return nullptr;
#if ENABLE(FULLSCREEN_API)
#if ENABLE(VIDEO)
    if (CheckedPtr fullscreenManager = document->fullscreenManagerIfExists(); fullscreenManager && fullscreenManager->isFullscreen() && is<HTMLVideoElement>(fullscreenManager->currentFullscreenElement()))
        return fullscreenManager->currentFullscreenElement();
#endif
#endif
    Element* element = document->focusedElement();
    if (!element) {
        if (auto* pluginDocument = dynamicDowncast<PluginDocument>(*document))
            element = pluginDocument->pluginElement();
    }
    if (!element && document->isHTMLDocument())
        element = document->bodyOrFrameset();
    if (!element)
        element = document->documentElement();
    return element;
}

void Document::convertAbsoluteToClientQuads(Vector<FloatQuad>& quads, const RenderStyle& style)
{
    RefPtr frameView = view();
    if (!frameView)
        return;

    float inverseFrameScale = frameView->absoluteToDocumentScaleFactor(style.usedZoom());
    auto documentToClientOffset = frameView->documentToClientOffset();

    for (auto& quad : quads) {
        if (inverseFrameScale != 1)
            quad.scale(inverseFrameScale);

        quad.move(documentToClientOffset);
    }
}

void Document::convertAbsoluteToClientRects(Vector<FloatRect>& rects, const RenderStyle& style)
{
    RefPtr frameView = view();
    if (!frameView)
        return;

    float inverseFrameScale = frameView->absoluteToDocumentScaleFactor(style.usedZoom());
    auto documentToClientOffset = frameView->documentToClientOffset();

    for (auto& rect : rects) {
        if (inverseFrameScale != 1)
            rect.scale(inverseFrameScale);

        rect.move(documentToClientOffset);
    }
}

void Document::convertAbsoluteToClientRect(FloatRect& rect, const RenderStyle& style)
{
    RefPtr frameView = view();
    if (!frameView)
        return;

    rect = frameView->absoluteToDocumentRect(rect, style.usedZoom());
    rect = frameView->documentToClientRect(rect);
}

bool Document::hasActiveParser()
{
    return m_activeParserCount || (m_parser && m_parser->processingData());
}

void Document::decrementActiveParserCount()
{
    --m_activeParserCount;
    RefPtr frame = this->frame();
    if (!frame)
        return;

    // FIXME: We should call DocumentLoader::checkLoadComplete as well here,
    // but it seems to cause http/tests/security/feed-urls-from-remote.html
    // to timeout on Mac WK1; see http://webkit.org/b/110554 and http://webkit.org/b/110401.
    frame->protectedLoader()->checkLoadComplete();
}

DocumentParserYieldToken::DocumentParserYieldToken(Document& document)
    : m_document(document)
{
    if (++document.m_parserYieldTokenCount != 1)
        return;

    if (CheckedPtr scriptRunner = document.scriptRunnerIfExists())
        scriptRunner->didBeginYieldingParser();
    if (RefPtr parser = document.parser())
        parser->didBeginYieldingParser();
}

DocumentParserYieldToken::~DocumentParserYieldToken()
{
    if (!m_document)
        return;

    ASSERT(m_document->m_parserYieldTokenCount);
    if (--m_document->m_parserYieldTokenCount)
        return;

    Ref protectedDocument = *m_document;
    if (CheckedPtr scriptRunner = protectedDocument->scriptRunnerIfExists())
        scriptRunner->didEndYieldingParser();
    if (RefPtr parser = protectedDocument->parser())
        parser->didEndYieldingParser();
}

static Element* findNearestCommonComposedAncestor(Element* elementA, Element* elementB)
{
    if (!elementA || !elementB)
        return nullptr;

    if (elementA == elementB)
        return elementA;

    HashSet<Ref<Element>> ancestorChain;
    for (auto* element = elementA; element; element = element->parentElementInComposedTree())
        ancestorChain.add(*element);

    for (auto* element = elementB; element; element = element->parentElementInComposedTree()) {
        if (ancestorChain.contains(*element))
            return element;
    }
    return nullptr;
}

void Document::updateHoverActiveState(const HitTestRequest& request, Element* innerElement, CaptureChange captureElementChanged)
{
    ASSERT(!request.readOnly());

    Vector<RefPtr<Element>, 32> elementsToClearActive;
    Vector<RefPtr<Element>, 32> elementsToSetActive;
    Vector<RefPtr<Element>, 32> elementsToClearHover;
    Vector<RefPtr<Element>, 32> elementsToSetHover;

    RefPtr innerElementInDocument = innerElement;
    while (innerElementInDocument && &innerElementInDocument->document() != this) {
        innerElementInDocument->document().updateHoverActiveState(request, innerElementInDocument.get());
        innerElementInDocument = innerElementInDocument->document().ownerElement();
    }

    RefPtr oldActiveElement = m_activeElement.get();
    if (oldActiveElement && !request.active()) {
        // We are clearing the :active chain because the mouse has been released.
        for (RefPtr currentElement = oldActiveElement; currentElement; currentElement = currentElement->parentElementInComposedTree()) {
            elementsToClearActive.append(currentElement);
            m_userActionElements.setInActiveChain(*currentElement, false);
        }
        m_activeElement = nullptr;
    } else {
        RefPtr newActiveElement = innerElementInDocument;
        if (!oldActiveElement && newActiveElement && request.active() && !request.touchMove()) {
            // We are setting the :active chain and freezing it. If future moves happen, they
            // will need to reference this chain.
            for (CheckedPtr curr = newActiveElement->renderer(); curr; curr = curr->parent()) {
                RefPtr element = curr->element();
                if (!element || curr->isRenderTextOrLineBreak())
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

    RefPtr oldHoveredElement = WTFMove(m_hoveredElement);

    // A touch release does not set a new hover target; clearing the element we're working with
    // will clear the chain of hovered elements all the way to the top of the tree.
    if (request.touchRelease())
        innerElementInDocument = nullptr;

    // Check to see if the hovered Element has changed.
    // If it hasn't, we do not need to do anything.
    RefPtr newHoveredElement = innerElementInDocument;
    while (newHoveredElement && !newHoveredElement->renderer())
        newHoveredElement = newHoveredElement->parentElementInComposedTree();

    m_hoveredElement = newHoveredElement;

    RefPtr commonAncestor = findNearestCommonComposedAncestor(oldHoveredElement.get(), newHoveredElement.get());

    if (oldHoveredElement != newHoveredElement) {
        for (auto* element = oldHoveredElement.get(); element; element = element->parentElementInComposedTree()) {
            if (element == commonAncestor)
                break;
            if (mustBeInActiveChain && !element->isInActiveChain())
                continue;
            elementsToClearHover.append(element);
        }
        // Unset hovered nodes in sub frame documents if the old hovered node was a frame owner.
        if (auto* frameOwnerElement = dynamicDowncast<HTMLFrameOwnerElement>(oldHoveredElement.get())) {
            if (RefPtr contentDocument = frameOwnerElement->contentDocument())
                contentDocument->updateHoverActiveState(request, nullptr);
        }
    }

    bool sawCommonAncestor = false;
    for (RefPtr element = newHoveredElement; element; element = element->parentElementInComposedTree()) {
        if (mustBeInActiveChain && !element->isInActiveChain())
            continue;
        if (allowActiveChanges)
            elementsToSetActive.append(element);
        if (element == commonAncestor)
            sawCommonAncestor = true;
        if (!sawCommonAncestor)
            elementsToSetHover.append(element);
    }

    auto changeState = [](auto& elements, auto pseudoClass, auto value, auto&& setter) {
        if (elements.isEmpty())
            return;

        Style::PseudoClassChangeInvalidation styleInvalidation { *elements.last(), pseudoClass, value, Style::InvalidationScope::Descendants };

        // We need to do descendant invalidation for each shadow tree separately as the style is per-scope.
        Vector<Style::PseudoClassChangeInvalidation> shadowDescendantStyleInvalidations;
        for (auto& element : elements) {
            if (hasShadowRootParent(*element))
                shadowDescendantStyleInvalidations.append({ *element, pseudoClass, value, Style::InvalidationScope::Descendants });
        }

        for (auto& element : elements)
            setter(*element);
    };

    changeState(elementsToClearActive, CSSSelector::PseudoClass::Active, false, [](auto& element) {
        element.setActive(false, Style::InvalidationScope::SelfChildrenAndSiblings);
    });
    changeState(elementsToSetActive, CSSSelector::PseudoClass::Active, true, [](auto& element) {
        element.setActive(true, Style::InvalidationScope::SelfChildrenAndSiblings);
    });
    changeState(elementsToClearHover, CSSSelector::PseudoClass::Hover, false, [request](auto& element) {
        element.setHovered(false, Style::InvalidationScope::SelfChildrenAndSiblings, request);
    });
    changeState(elementsToSetHover, CSSSelector::PseudoClass::Hover, true, [request](auto& element) {
        element.setHovered(true, Style::InvalidationScope::SelfChildrenAndSiblings, request);
    });
}

bool Document::haveStylesheetsLoaded() const
{
    return !styleScope().hasPendingSheets() || m_ignorePendingStylesheets;
}

Locale& Document::getCachedLocale(const AtomString& locale)
{
    AtomString localeKey = locale;
    if (locale.isEmpty() || !settings().langAttributeAwareFormControlUIEnabled())
        localeKey = AtomString { defaultLanguage() };
    LocaleIdentifierToLocaleMap::AddResult result = m_localeCache.add(localeKey, nullptr);
    if (result.isNewEntry)
        result.iterator->value = Locale::create(localeKey);
    return *(result.iterator->value);
}

Document& Document::ensureTemplateDocument()
{
    if (const Document* document = templateDocument())
        return const_cast<Document&>(*document);

    Ref templateDocument = [&]() -> Ref<Document> {
        if (isHTMLDocument())
            return HTMLDocument::create(nullptr, m_settings, aboutBlankURL(), { });
        return create(m_settings, aboutBlankURL());
    }();
    m_templateDocument = templateDocument.copyRef();
    templateDocument->setContextDocument(contextDocument());
    templateDocument->setTemplateDocumentHost(this); // balanced in dtor.

    return *m_templateDocument;
}

Ref<DocumentFragment> Document::documentFragmentForInnerOuterHTML()
{
    if (UNLIKELY(!m_documentFragmentForInnerOuterHTML))
        m_documentFragmentForInnerOuterHTML = DocumentFragment::createForInnerOuterHTML(*this);
    else if (UNLIKELY(m_documentFragmentForInnerOuterHTML->hasChildNodes()))
        Ref { *m_documentFragmentForInnerOuterHTML }->removeChildren();
    return *m_documentFragmentForInnerOuterHTML;
}

Ref<FontFaceSet> Document::fonts()
{
    return protectedFontSelector()->fontFaceSet();
}

EditingBehavior Document::editingBehavior() const
{
    return EditingBehavior { settings().editingBehaviorType() };
}

Ref<Settings> Document::protectedSettings() const
{
    return const_cast<Settings&>(m_settings.get());
}

float Document::deviceScaleFactor() const
{
    float deviceScaleFactor = 1.0;
    if (RefPtr documentPage = page())
        deviceScaleFactor = documentPage->deviceScaleFactor();
    return deviceScaleFactor;
}

bool Document::useSystemAppearance() const
{
    if (RefPtr documentPage = page())
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
    if (RefPtr documentPage = page())
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
    if (RefPtr documentPage = page())
        return documentPage->useElevatedUserInterfaceLevel();
    return false;
}

OptionSet<StyleColorOptions> Document::styleColorOptions(const RenderStyle* style) const
{
    OptionSet<StyleColorOptions> options;
    if (useSystemAppearance())
        options.add(StyleColorOptions::UseSystemAppearance);
    if (useDarkAppearance(style))
        options.add(StyleColorOptions::UseDarkAppearance);
    if (useElevatedUserInterfaceLevel())
        options.add(StyleColorOptions::UseElevatedUserInterfaceLevel);
    return options;
}

CompositeOperator Document::compositeOperatorForBackgroundColor(const Color& color, const RenderObject& renderer) const
{
    if (LIKELY(!settings().punchOutWhiteBackgroundsInDarkMode() || !Color::isWhiteColor(color) || !renderer.useDarkAppearance()))
        return CompositeOperator::SourceOver;

    RefPtr frameView = view();
    if (!frameView)
        return CompositeOperator::SourceOver;

    // Mail on macOS uses a transparent view, and on iOS it is an opaque view. We need to
    // use different composite modes to get the right results in this case.
    return frameView->isTransparent() ? CompositeOperator::DestinationOut : CompositeOperator::DestinationIn;
}

void Document::didAssociateFormControl(Element& element)
{
    RefPtr page = this->page();
    if (!page || !page->chrome().client().shouldNotifyOnFormChanges())
        return;

    auto isNewEntry = m_associatedFormControls.add(element).isNewEntry;
    if (isNewEntry && !m_didAssociateFormControlsTimer.isActive())
        m_didAssociateFormControlsTimer.startOneShot(isTopDocument() || hasHadUserInteraction() ? 0_s : 1_s);
}

void Document::didAssociateFormControlsTimerFired()
{
    auto controls = WTF::compactMap(std::exchange(m_associatedFormControls, { }), [](auto&& element) -> std::optional<RefPtr<Element>> {
        if (element.isConnected())
            return RefPtr { &element };
        return std::nullopt;
    });

    if (RefPtr page = this->page(); page && !controls.isEmpty()) {
        ASSERT(m_frame);
        page->chrome().client().didAssociateFormControls(controls, protectedFrame().releaseNonNull());
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

    if (RefPtr page = this->page())
        page->cookieJar().clearCacheForHost(url.host().toString());
}

void Document::ensurePlugInsInjectedScript(DOMWrapperWorld& world)
{
    if (m_hasInjectedPlugInsScript)
        return;

    RefPtr frame = this->frame();
    auto& scriptController = frame->script();

    // Use the JS file provided by the Chrome client, or fallback to the default one.
    String jsString = page()->chrome().client().plugInExtraScript();
    if (!jsString)
        jsString = StringImpl::createWithoutCopying(plugInsJavaScript);

    scriptController.evaluateInWorldIgnoringException(ScriptSourceCode(jsString, JSC::SourceTaintedOrigin::Untainted), world);

    m_hasInjectedPlugInsScript = true;
}

std::optional<Vector<uint8_t>> Document::wrapCryptoKey(const Vector<uint8_t>& key)
{
    RefPtr page = this->page();
    if (!page)
        return std::nullopt;
    return page->cryptoClient().wrapCryptoKey(key);
}

std::optional<Vector<uint8_t>>Document::unwrapCryptoKey(const Vector<uint8_t>& wrappedKey)
{
    RefPtr page = this->page();
    if (!page)
        return std::nullopt;
    return page->cryptoClient().unwrapCryptoKey(wrappedKey);
}

Element* Document::activeElement()
{
    if (Element* element = treeScope().focusedElementInScope())
        return element;
    return bodyOrFrameset();
}

bool Document::hasFocus() const
{
    RefPtr page = this->page();
    if (!page || !page->focusController().isActive() || !page->focusController().isFocused())
        return false;
    if (RefPtr focusedFrame = page->focusController().focusedFrame()) {
        if (focusedFrame->tree().isDescendantOf(frame()))
            return true;
    }
    return false;
}

#if ENABLE(WIRELESS_PLAYBACK_TARGET)

void Document::addPlaybackTargetPickerClient(MediaPlaybackTargetClient& client)
{
    RefPtr page = this->page();
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

    // Unable to ref the page as it may have started destruction.
    if (WeakPtr page = this->page())
        page->removePlaybackTargetPickerClient(clientId);
}

void Document::showPlaybackTargetPicker(MediaPlaybackTargetClient& client, bool isVideo, RouteSharingPolicy routeSharingPolicy, const String& routingContextUID)
{
    RefPtr page = this->page();
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

void Document::playbackTargetPickerClientStateDidChange(MediaPlaybackTargetClient& client, MediaProducerMediaStateFlags state)
{
    RefPtr page = this->page();
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

ShouldOpenExternalURLsPolicy Document::shouldOpenExternalURLsPolicyToPropagate() const
{
    if (RefPtr documentLoader = loader())
        return documentLoader->shouldOpenExternalURLsPolicyToPropagate();

    return ShouldOpenExternalURLsPolicy::ShouldNotAllow;
}

bool Document::shouldEnforceHTTP09Sandbox() const
{
    if (m_isSynthesized || !m_frame)
        return false;
    RefPtr documentLoader = m_frame->loader().activeDocumentLoader();
    return documentLoader && documentLoader->response().isHTTP09();
}

#if USE(QUICK_LOOK)

bool Document::shouldEnforceQuickLookSandbox() const
{
    if (m_isSynthesized || !m_frame)
        return false;
    RefPtr documentLoader = m_frame->loader().activeDocumentLoader();
    return documentLoader && documentLoader->response().isQuickLook();
}

void Document::applyQuickLookSandbox()
{
    Ref documentLoader = *m_frame->loader().activeDocumentLoader();
    auto documentURL = documentLoader->documentURL();
    auto& responseURL = documentLoader->responseURL();
    ASSERT(!documentURL.protocolIs(QLPreviewProtocol));
    ASSERT(responseURL.protocolIs(QLPreviewProtocol));

    setStorageBlockingPolicy(StorageBlockingPolicy::BlockAll);
    auto securityOrigin = SecurityOrigin::createNonLocalWithAllowedFilePath(responseURL, documentURL.fileSystemPath());
    setSecurityOriginPolicy(SecurityOriginPolicy::create(WTFMove(securityOrigin)));

    static NeverDestroyed<String> quickLookCSP = makeString("default-src "_s, QLPreviewProtocol, ": 'unsafe-inline'; base-uri 'none'; sandbox allow-same-origin allow-scripts"_s);
    RELEASE_ASSERT(contentSecurityPolicy());
    // The sandbox directive is only allowed if the policy is from an HTTP header.
    checkedContentSecurityPolicy()->didReceiveHeader(quickLookCSP, ContentSecurityPolicyHeaderType::Enforce, ContentSecurityPolicy::PolicyFrom::HTTPHeader, referrer());

    disableSandboxFlags(SandboxFlag::Navigation);

    setReferrerPolicy(ReferrerPolicy::NoReferrer);
}

#endif

bool Document::shouldEnforceContentDispositionAttachmentSandbox() const
{
    if (!settings().contentDispositionAttachmentSandboxEnabled())
        return false;

    if (m_isSynthesized)
        return false;

    if (RefPtr documentLoader = m_frame ? m_frame->loader().activeDocumentLoader() : nullptr)
        return documentLoader->response().isAttachment();
    return false;
}

void Document::applyContentDispositionAttachmentSandbox()
{
    ASSERT(shouldEnforceContentDispositionAttachmentSandbox());

    setReferrerPolicy(ReferrerPolicy::NoReferrer);
    if (!isMediaDocument())
        enforceSandboxFlags(SandboxFlags::all());
    else
        enforceSandboxFlags(SandboxFlag::Origin);
}

void Document::addDynamicMediaQueryDependentImage(HTMLImageElement& element)
{
    m_dynamicMediaQueryDependentImages.add(element);
}

void Document::removeDynamicMediaQueryDependentImage(HTMLImageElement& element)
{
    m_dynamicMediaQueryDependentImages.remove(element);
}

void Document::intersectionObserversInitialUpdateTimerFired()
{
    scheduleRenderingUpdate(RenderingUpdateStep::IntersectionObservations);
}

void Document::scheduleRenderingUpdate(OptionSet<RenderingUpdateStep> requestedSteps)
{
    if (m_intersectionObserversInitialUpdateTimer.isActive()) {
        m_intersectionObserversInitialUpdateTimer.stop();
        requestedSteps.add(RenderingUpdateStep::IntersectionObservations);
    }
    if (RefPtr page = this->page())
        page->scheduleRenderingUpdate(requestedSteps);
}

void Document::addIntersectionObserver(IntersectionObserver& observer)
{
    ASSERT(m_intersectionObservers.find(&observer) == notFound);
    m_intersectionObservers.append(observer);
}

void Document::removeIntersectionObserver(IntersectionObserver& observer)
{
    m_intersectionObservers.removeFirst(&observer);
}

void Document::updateIntersectionObservations()
{
    updateIntersectionObservations(m_intersectionObservers);
}

void Document::updateIntersectionObservations(const Vector<WeakPtr<IntersectionObserver>>& intersectionObservers)
{
    RefPtr frameView = view();
    if (!frameView)
        return;

    bool needsLayout = frameView->layoutContext().isLayoutPending() || (renderView() && renderView()->needsLayout());
    if (needsLayout || hasPendingStyleRecalc()) {
        if (!intersectionObservers.isEmpty()) {
            LOG_WITH_STREAM(IntersectionObserver, stream << "Document " << this << " updateIntersectionObservations - needsLayout " << needsLayout << " or has pending style recalc " << hasPendingStyleRecalc() << "; scheduling another update");
            scheduleRenderingUpdate(RenderingUpdateStep::IntersectionObservations);
        }
        return;
    }

    LOG_WITH_STREAM(IntersectionObserver, stream << "Document " << this << " updateIntersectionObservations - notifying observers");

    Vector<WeakPtr<IntersectionObserver>> intersectionObserversWithPendingNotifications;

    for (auto& weakObserver : intersectionObservers) {
        RefPtr observer = weakObserver.get();
        if (!observer)
            continue;

        auto needNotify = observer->updateObservations(*this);
        if (needNotify == IntersectionObserver::NeedNotify::Yes)
            intersectionObserversWithPendingNotifications.append(observer);
    }

    for (auto& weakObserver : intersectionObserversWithPendingNotifications) {
        if (RefPtr observer = weakObserver.get())
            observer->notify();
    }
}

void Document::scheduleInitialIntersectionObservationUpdate()
{
    if (m_readyState == ReadyState::Complete)
        scheduleRenderingUpdate(RenderingUpdateStep::IntersectionObservations);
    else if (!m_intersectionObserversInitialUpdateTimer.isActive())
        m_intersectionObserversInitialUpdateTimer.startOneShot(intersectionObserversInitialUpdateDelay);
}

IntersectionObserverData& Document::ensureIntersectionObserverData()
{
    if (!m_intersectionObserverData)
        m_intersectionObserverData = makeUnique<IntersectionObserverData>();
    return *m_intersectionObserverData;
}

void Document::addResizeObserver(ResizeObserver& observer)
{
    if (!m_resizeObservers.contains(&observer))
        m_resizeObservers.append(observer);
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
    LOG_WITH_STREAM(ResizeObserver, stream << "Document " << *this << " gatherResizeObservations");
    size_t minDepth = ResizeObserver::maxElementDepth();
    for (auto& weakObserver : m_resizeObservers) {
        RefPtr observer = weakObserver.get();
        if (!observer || !observer->hasObservations())
            continue;
        auto depth = observer->gatherObservations(deeperThan);
        minDepth = std::min(minDepth, depth);
    }
    return minDepth;
}

size_t Document::gatherResizeObservationsForContainIntrinsicSize()
{
    if (!m_resizeObserverForContainIntrinsicSize)
        return ResizeObserver::maxElementDepth();

    LOG_WITH_STREAM(ResizeObserver, stream << "Document " << *this << " gatherResizeObservationsForContainIntrinsicSize");
    return m_resizeObserverForContainIntrinsicSize->gatherObservations(0);
}

void Document::deliverResizeObservations()
{
    LOG_WITH_STREAM(ResizeObserver, stream << "Document " << *this << " deliverResizeObservations");
    if (m_resizeObserverForContainIntrinsicSize)
        m_resizeObserverForContainIntrinsicSize->deliverObservations();

    auto observersToNotify = m_resizeObservers;
    for (auto& weakObserver : observersToNotify) {
        RefPtr observer = weakObserver.get();
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
    for (auto& observer : m_resizeObservers)
        Ref { *observer }->setHasSkippedObservations(skipped);
}

void Document::updateResizeObservations(Page& page)
{
    if (quirks().shouldSilenceResizeObservers()) {
        addConsoleMessage(MessageSource::Other, MessageLevel::Info, "ResizeObservers silenced due to: http://webkit.org/b/258597"_s);
        return;
    }
    if (!hasResizeObservers() && !m_resizeObserverForContainIntrinsicSize && !m_contentVisibilityDocumentState)
        return;

    size_t resizeObserverDepth = 0;
    while (true) {
        // We need layout the whole frame tree here. Because ResizeObserver could observe element in other frame,
        // and it could change other frame in deliverResizeObservations().
        page.layoutIfNeeded();
        // If we have determined a change because of visibility we need to get up-to-date layout before reporting any values to resize observers.
        if (m_contentVisibilityDocumentState && m_contentVisibilityDocumentState->determineInitialVisibleContentVisibility() == HadInitialVisibleContentVisibilityDetermination::Yes)
            continue;

        // Start check resize observers;
        if (!resizeObserverDepth && gatherResizeObservationsForContainIntrinsicSize() != ResizeObserver::maxElementDepth())
            deliverResizeObservations();

        resizeObserverDepth = gatherResizeObservations(resizeObserverDepth);
        if (resizeObserverDepth == ResizeObserver::maxElementDepth())
            break;
        deliverResizeObservations();
    }

    if (hasSkippedResizeObservations()) {
        setHasSkippedResizeObservations(false);
        reportException("ResizeObserver loop completed with undelivered notifications."_s, 0, 0, url().string(), nullptr, nullptr);
        // Starting a new schedule the next round of notify.
        scheduleRenderingUpdate(RenderingUpdateStep::ResizeObservations);
    }
}

const AtomString& Document::dir() const
{
    auto* documentElement = dynamicDowncast<HTMLHtmlElement>(this->documentElement());
    return documentElement ? documentElement->dir() : nullAtom();
}

void Document::setDir(const AtomString& value)
{
    if (RefPtr documentElement = dynamicDowncast<HTMLHtmlElement>(this->documentElement()))
        documentElement->setDir(value);
}

DOMSelection* Document::getSelection()
{
    return m_domWindow ? m_domWindow->getSelection() : nullptr;
}

void Document::didInsertInDocumentShadowRoot(ShadowRoot& shadowRoot)
{
    ASSERT(shadowRoot.isConnected());
    ASSERT(!m_inDocumentShadowRoots.contains(shadowRoot));
    m_inDocumentShadowRoots.add(shadowRoot);
}

void Document::didRemoveInDocumentShadowRoot(ShadowRoot& shadowRoot)
{
    ASSERT(m_inDocumentShadowRoots.contains(shadowRoot));
    m_inDocumentShadowRoots.remove(shadowRoot);
}

ConstantPropertyMap& Document::constantProperties() const
{
    if (!m_constantPropertyMap) {
        auto& thisDocument = const_cast<Document&>(*this);
        thisDocument.m_constantPropertyMap = makeUnique<ConstantPropertyMap>(thisDocument);
    }
    return *m_constantPropertyMap;
}

void Document::orientationChanged(IntDegrees orientation)
{
    LOG(Events, "Document %p orientationChanged - orientation %d", this, orientation);
    dispatchWindowEvent(Event::create(eventNames().orientationchangeEvent, Event::CanBubble::No, Event::IsCancelable::No));
    if (CheckedPtr notifier = m_orientationNotifier.get())
        notifier->orientationChanged(orientation);
}

OrientationNotifier& Document::orientationNotifier()
{
    if (!m_orientationNotifier)
        m_orientationNotifier = makeUnique<OrientationNotifier>(currentOrientation(frame()));
    return *m_orientationNotifier;
}

#if ENABLE(MEDIA_STREAM)
static bool isSourceMatchingMediaCaptureKind(const RealtimeMediaSource& source, MediaProducerMediaCaptureKind kind)
{
    switch (kind) {
    case MediaProducerMediaCaptureKind::Microphone:
        return source.deviceType() == CaptureDevice::DeviceType::Microphone;
    case MediaProducerMediaCaptureKind::Camera:
        return source.deviceType() == CaptureDevice::DeviceType::Camera;
    case MediaProducerMediaCaptureKind::Display: {
        auto deviceType = source.deviceType();
        return deviceType == CaptureDevice::DeviceType::Screen || deviceType == CaptureDevice::DeviceType::Window;
    }
    case MediaProducerMediaCaptureKind::SystemAudio:
        return source.deviceType() == CaptureDevice::DeviceType::SystemAudio;
    case MediaProducerMediaCaptureKind::EveryKind:
        return true;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return false;
}

void Document::stopMediaCapture(MediaProducerMediaCaptureKind kind)
{
    bool didEndCapture = false;
    auto captureSources = m_captureSources;
    for (auto& source : captureSources) {
        if (isSourceMatchingMediaCaptureKind(source.get(), kind)) {
            source->endImmediatly();
            didEndCapture = true;
        }
    }
    if (didEndCapture)
        updateIsPlayingMedia();
}

void Document::mediaStreamCaptureStateChanged()
{
    updateSleepDisablerIfNeeded();

    if (!MediaProducer::isCapturing(m_mediaState))
        return;

    forEachMediaElement([] (HTMLMediaElement& element) {
        element.mediaStreamCaptureStarted();
    });
}

void Document::updateVideoCaptureStateForMicrophoneInterruption(bool isMicrophoneInterrupted)
{
    RELEASE_LOG_INFO(WebRTC, "Document::updateVideoCaptureStateForMicrophoneInterruption %d", isMicrophoneInterrupted);

    RefPtr page = this->page();
    if (!page)
        return;

    for (auto& document : allDocuments()) {
        Ref protectedDocument = document.get();
        if (page.get() != document->page())
            continue;
        for (auto& captureSource : document->m_captureSources) {
            if (!captureSource->isEnded() && captureSource->deviceType() == CaptureDevice::DeviceType::Camera)
                captureSource->setMuted(isMicrophoneInterrupted);
        }
    }
}
#endif // ENABLE(MEDIA_STREAM)

const AtomString& Document::bgColor() const
{
    RefPtr bodyElement = body();
    if (!bodyElement)
        return emptyAtom();
    return bodyElement->attributeWithoutSynchronization(bgcolorAttr);
}

void Document::setBgColor(const AtomString& value)
{
    if (RefPtr bodyElement = body())
        bodyElement->setAttributeWithoutSynchronization(bgcolorAttr, value);
}

const AtomString& Document::fgColor() const
{
    RefPtr bodyElement = body();
    return bodyElement ? bodyElement->attributeWithoutSynchronization(textAttr) : emptyAtom();
}

void Document::setFgColor(const AtomString& value)
{
    if (RefPtr bodyElement = body())
        bodyElement->setAttributeWithoutSynchronization(textAttr, value);
}

const AtomString& Document::alinkColor() const
{
    RefPtr bodyElement = body();
    return bodyElement ? bodyElement->attributeWithoutSynchronization(alinkAttr) : emptyAtom();
}

void Document::setAlinkColor(const AtomString& value)
{
    if (RefPtr bodyElement = body())
        bodyElement->setAttributeWithoutSynchronization(alinkAttr, value);
}

const AtomString& Document::linkColorForBindings() const
{
    RefPtr bodyElement = body();
    return bodyElement ? bodyElement->attributeWithoutSynchronization(linkAttr) : emptyAtom();
}

void Document::setLinkColorForBindings(const AtomString& value)
{
    if (RefPtr bodyElement = body())
        bodyElement->setAttributeWithoutSynchronization(linkAttr, value);
}

const AtomString& Document::vlinkColor() const
{
    RefPtr bodyElement = body();
    return bodyElement ? bodyElement->attributeWithoutSynchronization(vlinkAttr) : emptyAtom();
}

void Document::setVlinkColor(const AtomString& value)
{
    if (RefPtr bodyElement = body())
        bodyElement->setAttributeWithoutSynchronization(vlinkAttr, value);
}

Logger& Document::logger()
{
    if (!m_logger) {
        Ref logger = Logger::create(this);
        m_logger = logger.copyRef();
        RefPtr page = this->page();
        logger->setEnabled(this, page && page->sessionID().isAlwaysOnLoggingAllowed());
        logger->addObserver(*this);
    }

    return *m_logger;
}

std::optional<PageIdentifier> Document::pageID() const
{
    if (auto* page = this->page())
        return page->identifier();
    return std::nullopt;
}

std::optional<FrameIdentifier> Document::frameID() const
{
    return m_frameIdentifier;
}

void Document::registerArticleElement(Element& article)
{
    m_articleElements.add(article);
}

void Document::unregisterArticleElement(Element& article)
{
    m_articleElements.remove(article);
    if (m_mainArticleElement == &article)
        m_mainArticleElement = nullptr;
}

void Document::updateMainArticleElementAfterLayout()
{
    ASSERT(page() && page()->requestedLayoutMilestones().contains(LayoutMilestone::DidRenderSignificantAmountOfText));

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

    RefPtr<Element> tallestArticle;
    float tallestArticleHeight = 0;
    float tallestArticleWidth = 0;
    float secondTallestArticleHeight = 0;

    for (auto& article : m_articleElements) {
        auto* box = article->renderBox();
        float height = box ? box->height().toFloat() : 0;
        if (height >= tallestArticleHeight) {
            secondTallestArticleHeight = tallestArticleHeight;
            tallestArticleHeight = height;
            tallestArticleWidth = box ? box->width().toFloat() : 0;
            tallestArticle = article.ptr();
        } else if (height >= secondTallestArticleHeight)
            secondTallestArticleHeight = height;
    }

    if (numberOfArticles == 1) {
        m_mainArticleElement = tallestArticle.get();
        return;
    }

    if (tallestArticleHeight < minimumSecondTallestArticleHeightFactor * secondTallestArticleHeight)
        return;

    if (!view())
        return;

    auto viewportSize = view()->layoutViewportRect().size();
    if (tallestArticleWidth * tallestArticleHeight < minimumViewportAreaFactor * (viewportSize.width() * viewportSize.height()).toFloat())
        return;

    m_mainArticleElement = tallestArticle.get();
}

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
    URL referrerURL { referrer() };
    if (referrerURL.isEmpty())
        return;

    auto domainString = RegistrableDomain { referrerURL }.string();
    if (domainString.isEmpty())
        return;

    if (auto port = referrerURL.port())
        m_referrerOverride = makeString(referrerURL.protocol(), "://"_s, domainString, ':', *port, '/');
    else
        m_referrerOverride = makeString(referrerURL.protocol(), "://"_s, domainString, '/');
}

void Document::setConsoleMessageListener(RefPtr<StringCallback>&& listener)
{
    m_consoleMessageListener = listener;
}

AnimationTimelinesController& Document::ensureTimelinesController()
{
    if (!m_timelinesController)
        m_timelinesController = makeUnique<AnimationTimelinesController>(*this);
    return *m_timelinesController.get();
}

void Document::updateAnimationsAndSendEvents()
{
    RefPtr domWindow = this->domWindow();
    if (!domWindow)
        return;

    if (auto* timelinesController = this->timelinesController())
        timelinesController->updateAnimationsAndSendEvents(domWindow->frozenNowTimestamp());
}

DocumentTimeline& Document::timeline()
{
    if (!m_timeline)
        m_timeline = DocumentTimeline::create(*this);

    return *m_timeline;
}

#if ENABLE(THREADED_ANIMATION_RESOLUTION)
AcceleratedTimeline& Document::acceleratedTimeline()
{
    if (!m_acceleratedTimeline)
        m_acceleratedTimeline = makeUnique<AcceleratedTimeline>(*this);
    return *m_acceleratedTimeline;
}
#endif

Vector<RefPtr<WebAnimation>> Document::getAnimations()
{
    return matchingAnimations([] (Element& target) -> bool {
        return !target.containingShadowRoot();
    });
}

Vector<RefPtr<WebAnimation>> Document::matchingAnimations(const Function<bool(Element&)>& function)
{
    // For the list of animations to be current, we need to account for any pending CSS changes,
    // such as updates to CSS Animations and CSS Transitions. This requires updating layout as
    // well since resolving layout-dependent media queries could yield animations.
    if (RefPtr owner = ownerElement())
        owner->protectedDocument()->updateLayout();
    updateStyleIfNeeded();

    Vector<RefPtr<WebAnimation>> animations;

    auto effectCanBeListed = [&](AnimationEffect* effect) {
        if (is<CustomEffect>(effect))
            return true;

        if (auto* keyframeEffect = dynamicDowncast<KeyframeEffect>(effect)) {
            RefPtr target = keyframeEffect->target();
            return target && target->isConnected() && &target->document() == this && function(*target);
        }

        return false;
    };

    for (auto* animation : WebAnimation::instances()) {
        if (animation->isRelevant() && effectCanBeListed(animation->effect()))
            animations.append(animation);
    }

    std::stable_sort(animations.begin(), animations.end(), [](auto& lhs, auto& rhs) {
        return compareAnimationsByCompositeOrder(*lhs, *rhs);
    });

    return animations;
}

void Document::keyframesRuleDidChange(const String& name)
{
    for (RefPtr animation : WebAnimation::instances()) {
        auto cssAnimation = dynamicDowncast<CSSAnimation>(*animation);
        if (!cssAnimation || !cssAnimation->isRelevant())
            continue;

        if (cssAnimation->animationName() != name)
            continue;

        auto owningElement = cssAnimation->owningElement();
        if (!owningElement || !owningElement->element.isConnected() || &owningElement->element.document() != this)
            continue;

        cssAnimation->keyframesRuleDidChange();
    }
}

void Document::addTopLayerElement(Element& element)
{
    RELEASE_ASSERT(&element.document() == this && !element.isInTopLayer());
    auto result = m_topLayerElements.add(element);
    RELEASE_ASSERT(result.isNewEntry);
    if (auto* candidatePopover = dynamicDowncast<HTMLElement>(element); candidatePopover && candidatePopover->popoverState() == PopoverState::Auto) {
#if ENABLE(FULLSCREEN_API)
        if (candidatePopover->hasFullscreenFlag())
            return;
#endif
        auto* dialogElement = dynamicDowncast<HTMLDialogElement>(*candidatePopover);
        if (dialogElement && dialogElement->isModal())
            return;
        auto result = m_autoPopoverList.add(*candidatePopover);
        RELEASE_ASSERT(result.isNewEntry);
    }
}

void Document::removeTopLayerElement(Element& element)
{
    RELEASE_ASSERT(&element.document() == this && element.isInTopLayer());
    auto didRemove = m_topLayerElements.remove(element);
    RELEASE_ASSERT(didRemove);
    if (auto* candidatePopover = dynamicDowncast<HTMLElement>(element); candidatePopover && candidatePopover->isPopoverShowing() && candidatePopover->popoverState() == PopoverState::Auto) {
        auto didRemove = m_autoPopoverList.remove(*candidatePopover);
        RELEASE_ASSERT(didRemove);
    }
}

HTMLDialogElement* Document::activeModalDialog() const
{
    for (auto& element : makeReversedRange(m_topLayerElements)) {
        if (RefPtr dialog = dynamicDowncast<HTMLDialogElement>(element.get()); dialog && dialog->isModal())
            return dialog.get();
    }

    return nullptr;
}

HTMLElement* Document::topmostAutoPopover() const
{
    if (m_autoPopoverList.isEmpty())
        return nullptr;
    return m_autoPopoverList.last().ptr();
}

// https://html.spec.whatwg.org/#hide-all-popovers-until
void Document::hideAllPopoversUntil(HTMLElement* endpoint, FocusPreviousElement focusPreviousElement, FireEvents fireEvents)
{
    auto closeAllOpenPopovers = [&]() {
        while (RefPtr popover = topmostAutoPopover())
            popover->hidePopoverInternal(focusPreviousElement, fireEvents);
    };
    if (!endpoint)
        return closeAllOpenPopovers();

    bool repeatingHide = false;
    do {
        RefPtr<Element> lastToHide;
        bool foundEndPoint = false;
        for (auto& popover : autoPopoverList()) {
            if (popover.ptr() == endpoint)
                foundEndPoint = true;
            else if (foundEndPoint) {
                lastToHide = popover.ptr();
                break;
            }
        }
        if (!foundEndPoint)
            return closeAllOpenPopovers();
        while (lastToHide && lastToHide->isPopoverShowing()) {
            RefPtr topmostAutoPopover = this->topmostAutoPopover();
            if (!topmostAutoPopover)
                break;
            topmostAutoPopover->hidePopoverInternal(focusPreviousElement, fireEvents);
        }
        repeatingHide = m_autoPopoverList.contains(*endpoint) && topmostAutoPopover() != endpoint;
        if (repeatingHide)
            fireEvents = FireEvents::No;
    } while (repeatingHide);
}

// https://html.spec.whatwg.org/#popover-light-dismiss
void Document::handlePopoverLightDismiss(const PointerEvent& event, Node& target)
{
    ASSERT(event.isTrusted());

    RefPtr topmostAutoPopover = this->topmostAutoPopover();
    if (!topmostAutoPopover)
        return;

    RefPtr popoverToAvoidHiding = [&]() -> HTMLElement* {
        auto* targetElement = dynamicDowncast<Element>(target);
        RefPtr startElement = targetElement ? targetElement : target.parentElement();
        auto [clickedPopover, invokerPopover] = [&]() {
            RefPtr<HTMLElement> clickedPopover;
            RefPtr<HTMLElement> invokerPopover;
            auto isShowingAutoPopover = [](HTMLElement& element) -> bool {
                return element.popoverState() == PopoverState::Auto && element.popoverData()->visibilityState() == PopoverVisibilityState::Showing;
            };
            for (RefPtr element = WTFMove(startElement); element; element = element->parentElementInComposedTree()) {
                if (RefPtr htmlElement = dynamicDowncast<HTMLElement>(*element)) {
                    if (!clickedPopover && isShowingAutoPopover(*htmlElement))
                        clickedPopover = htmlElement;

                    if (!invokerPopover) {
                        if (RefPtr button = dynamicDowncast<HTMLFormControlElement>(*htmlElement)) {
                            if (RefPtr popover = dynamicDowncast<HTMLElement>(button->commandForElement()); popover && isShowingAutoPopover(*popover))
                                invokerPopover = WTFMove(popover);
                            else if (RefPtr popover = button->popoverTargetElement(); popover && isShowingAutoPopover(*popover))
                                invokerPopover = WTFMove(popover);
                        }
                    }
                    if (clickedPopover && invokerPopover)
                        break;
                }
            }
            return std::tuple { WTFMove(clickedPopover), WTFMove(invokerPopover) };
        }();

        auto highestInTopLayer = [&](HTMLElement* first, HTMLElement* second) -> HTMLElement* {
            if (!first || first == second)
                return second;
            if (!second)
                return first;
            for (auto& element : makeReversedRange(m_autoPopoverList)) {
                if (element.ptr() == first || element.ptr() == second)
                    return element.ptr();
            }
            ASSERT_NOT_REACHED();
            return nullptr;
        };
        return highestInTopLayer(clickedPopover.get(), invokerPopover.get());
    }();

    if (event.type() == eventNames().pointerdownEvent) {
        m_popoverPointerDownTarget = popoverToAvoidHiding;
        return;
    }

    ASSERT(event.type() == eventNames().pointerupEvent);
    if (m_popoverPointerDownTarget == popoverToAvoidHiding.get())
        hideAllPopoversUntil(popoverToAvoidHiding.get(), FocusPreviousElement::No, FireEvents::Yes);
    m_popoverPointerDownTarget = nullptr;
}

#if ENABLE(ATTACHMENT_ELEMENT)

void Document::registerAttachmentIdentifier(const String& identifier, const AttachmentAssociatedElement& element)
{
    editor().registerAttachmentIdentifier(identifier, element);
}

void Document::didInsertAttachmentElement(HTMLAttachmentElement& attachment)
{
    auto identifier = attachment.uniqueIdentifier();
    auto previousIdentifier = identifier;
    bool previousIdentifierIsNotUnique = !previousIdentifier.isEmpty() && m_attachmentIdentifierToElementMap.contains(previousIdentifier);
    if (identifier.isEmpty() || previousIdentifierIsNotUnique) {
        previousIdentifier = identifier;
        identifier = createVersion4UUIDStringWeak();
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
    auto channelName = span(channel.name);
    if (equalLettersIgnoringASCIICase(channelName, "media"_s))
        return MessageSource::Media;

    if (equalLettersIgnoringASCIICase(channelName, "webrtc"_s))
        return MessageSource::WebRTC;

    if (equalLettersIgnoringASCIICase(channelName, "mediasource"_s))
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
    RefPtr page = this->page();
    if (!page)
        return;

    ASSERT(page->sessionID().isAlwaysOnLoggingAllowed());

    auto messageSource = messageSourceForWTFLogChannel(channel);
    if (messageSource == MessageSource::Other)
        return;

    eventLoop().queueTask(TaskSource::InternalAsyncTask, [weakThis = WeakPtr<Document, WeakPtrImplWithEventTargetData> { *this }, level, messageSource, logMessages = WTFMove(logMessages)]() mutable {
        RefPtr document = weakThis.get();
        if (!document || !document->page())
            return;

        auto messageLevel = messageLevelFromWTFLogLevel(level);
        auto* globalObject = mainWorldGlobalObject(document->frame());
        auto message = makeUnique<Inspector::ConsoleMessage>(messageSource, MessageType::Log, messageLevel, WTFMove(logMessages), globalObject);
        document->addConsoleMessage(WTFMove(message));
    });
}

void Document::setServiceWorkerConnection(RefPtr<SWClientConnection>&& serviceWorkerConnection)
{
    if (m_serviceWorkerConnection == serviceWorkerConnection || m_hasPreparedForDestruction || m_isSuspended)
        return;

    if (RefPtr oldServiceWorkerConnection = m_serviceWorkerConnection)
        oldServiceWorkerConnection->unregisterServiceWorkerClient(identifier());

    m_serviceWorkerConnection = WTFMove(serviceWorkerConnection);
    updateServiceWorkerClientData();
}

void Document::updateServiceWorkerClientData()
{
    RefPtr serviceWorkerConnection = m_serviceWorkerConnection;
    if (!serviceWorkerConnection)
        return;

    auto controllingServiceWorkerRegistrationIdentifier = activeServiceWorker() ? std::make_optional<ServiceWorkerRegistrationIdentifier>(activeServiceWorker()->registrationIdentifier()) : std::nullopt;
    serviceWorkerConnection->registerServiceWorkerClient(clientOrigin(), ServiceWorkerClientData::from(*this), controllingServiceWorkerRegistrationIdentifier, userAgent(url()));
}

void Document::navigateFromServiceWorker(const URL& url, CompletionHandler<void(ScheduleLocationChangeResult)>&& callback)
{
    if (activeDOMObjectsAreSuspended() || activeDOMObjectsAreStopped()) {
        callback(ScheduleLocationChangeResult::Stopped);
        return;
    }
    eventLoop().queueTask(TaskSource::DOMManipulation, [weakThis = WeakPtr<Document, WeakPtrImplWithEventTargetData> { *this }, url, callback = WTFMove(callback)]() mutable {
        RefPtr frame = weakThis ? weakThis->frame() : nullptr;
        if (!frame) {
            callback(ScheduleLocationChangeResult::Stopped);
            return;
        }
        frame->checkedNavigationScheduler()->scheduleLocationChange(*weakThis, weakThis->securityOrigin(), url, frame->loader().outgoingReferrer(), LockHistory::Yes, LockBackForwardList::No, NavigationHistoryBehavior::Auto, [callback = WTFMove(callback)](auto result) mutable {
            callback(result);
        });
    });
}

const Style::CustomPropertyRegistry& Document::customPropertyRegistry() const
{
    return styleScope().customPropertyRegistry();
}

const CSSCounterStyleRegistry& Document::counterStyleRegistry() const
{
    return styleScope().counterStyleRegistry();
}

CSSCounterStyleRegistry& Document::counterStyleRegistry()
{
    return styleScope().counterStyleRegistry();
}

CSSParserContext Document::cssParserContext() const
{
    if (!m_cachedCSSParserContext)
        m_cachedCSSParserContext = makeUnique<CSSParserContext>(*this, URL { }, emptyString());
    return *m_cachedCSSParserContext;
}

void Document::invalidateCachedCSSParserContext()
{
    m_cachedCSSParserContext = { };
}

const FixedVector<CSSPropertyID>& Document::exposedComputedCSSPropertyIDs()
{
    if (!m_exposedComputedCSSPropertyIDs.has_value()) {
        std::remove_const_t<decltype(computedPropertyIDs)> exposed;
        auto end = std::copy_if(computedPropertyIDs.begin(), computedPropertyIDs.end(), exposed.begin(), [&](auto property) {
            if (!isExposed(property, m_settings.ptr()))
                return false;
            // If the standard property is exposed no need to expose the alias.
            if (auto standard = cascadeAliasProperty(property); standard != property && isExposed(standard, m_settings.ptr()))
                return false;
            return true;
        });
        m_exposedComputedCSSPropertyIDs.emplace(exposed.begin(), end);
    }
    return m_exposedComputedCSSPropertyIDs.value();
}

void Document::detachFromFrame()
{
    observeFrame(nullptr);
}

bool Document::hitTest(const HitTestRequest& request, HitTestResult& result)
{
    return hitTest(request, result.hitTestLocation(), result);
}

bool Document::hitTest(const HitTestRequest& request, const HitTestLocation& location, HitTestResult& result)
{
    Ref protectedThis { *this };

    if (!renderView())
        return false;

    Ref frameView = renderView()->frameView();

    // If hit testing can descend into child frames, then we should make sure those frames have an updated layout
    // before proceeding
    if (request.allowsAnyFrameContent())
        frameView->updateLayoutAndStyleIfNeededRecursive();
    else
        updateLayout();

#if ASSERT_ENABLED
    SetForScope hitTestRestorer { m_inHitTesting, true };
#endif

    bool resultLayer = renderView()->layer()->hitTest(request, location, result);

    // ScrollView scrollbars are not the same as RenderLayer scrollbars tested by RenderLayer::hitTestOverflowControls,
    // so we need to test ScrollView scrollbars separately here. In case of using overlay scrollbars, the layer hit test
    // will always work so we need to check the ScrollView scrollbars in that case too.
    if (!resultLayer || ScrollbarTheme::theme().usesOverlayScrollbars()) {
        // FIXME: Consider if this test should be done unconditionally.
        if (request.allowsFrameScrollbars()) {
            IntPoint windowPoint = frameView->contentsToWindow(location.roundedPoint());
            if (RefPtr frameScrollbar = frameView->scrollbarAtPoint(windowPoint)) {
                result.setScrollbar(WTFMove(frameScrollbar));
                return true;
            }
        }
    }
    return resultLayer;
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

PaintWorklet& Document::ensurePaintWorklet()
{
    if (!m_paintWorklet)
        m_paintWorklet = PaintWorklet::create(*this);
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

#if ENABLE(CONTENT_CHANGE_OBSERVER)

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

bool Document::isRunningUserScripts() const
{
    auto& top = topDocument();
    return this == &top ? m_isRunningUserScripts : top.isRunningUserScripts();
}

void Document::setAsRunningUserScripts()
{
    Ref top = topDocument();
    if (this == top.ptr())
        m_isRunningUserScripts = true;
    else
        top->setAsRunningUserScripts();
}

void Document::didRejectSyncXHRDuringPageDismissal()
{
    ++m_numberOfRejectedSyncXHRs;
    if (m_numberOfRejectedSyncXHRs > 1)
        return;

    postTask([this, weakThis = WeakPtr<Document, WeakPtrImplWithEventTargetData> { *this }](auto&) mutable {
        if (weakThis)
            m_numberOfRejectedSyncXHRs = 0;
    });
}

bool Document::shouldIgnoreSyncXHRs() const
{
    const unsigned maxRejectedSyncXHRsPerEventLoopIteration = 5;
    return m_numberOfRejectedSyncXHRs > maxRejectedSyncXHRsPerEventLoopIteration;
}

MessagePortChannelProvider& Document::messagePortChannelProvider()
{
    return MessagePortChannelProvider::singleton();
}

#if USE(SYSTEM_PREVIEW)
void Document::dispatchSystemPreviewActionEvent(const SystemPreviewInfo& systemPreviewInfo, const String& message)
{
    RefPtr element = systemPreviewInfo.element.elementIdentifier ? Element::fromIdentifier(*systemPreviewInfo.element.elementIdentifier) : nullptr;
    if (!is<HTMLAnchorElement>(element))
        return;

    if (&element->document() != this)
        return;

    auto event = MessageEvent::create(message, securityOrigin().toString());
    UserGestureIndicator gestureIndicator(IsProcessingUserGesture::Yes, this);
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
    RefPtr oldElement = m_pictureInPictureElement.get();
    if (oldElement == element)
        return;

    std::optional<Style::PseudoClassChangeInvalidation> oldInvalidation;
    if (oldElement)
        emplace(oldInvalidation, *oldElement, { { CSSSelector::PseudoClass::PictureInPicture, false } });

    std::optional<Style::PseudoClassChangeInvalidation> newInvalidation;
    if (element)
        emplace(newInvalidation, *element, { { CSSSelector::PseudoClass::PictureInPicture, true } });

    m_pictureInPictureElement = element;
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
        m_lazyLoadImageObserver = makeUnique<LazyLoadImageObserver>();
    return *m_lazyLoadImageObserver;
}

const CrossOriginOpenerPolicy& Document::crossOriginOpenerPolicy() const
{
    if (this != &topDocument())
        return topDocument().crossOriginOpenerPolicy();
    return SecurityContext::crossOriginOpenerPolicy();
}

void Document::prepareCanvasesForDisplayOrFlushIfNeeded()
{
    auto contexts = copyToVectorOf<WeakPtr<CanvasRenderingContext>>(m_canvasContextsToPrepare);
    m_canvasContextsToPrepare.clear();
    for (auto& weakContext : contexts) {
        RefPtr context = weakContext.get();
        if (!context)
            continue;

        context->setIsInPreparationForDisplayOrFlush(false);

        // Some canvas contexts hold memory that should be periodically freed.
        if (context->hasDeferredOperations())
            context->flushDeferredOperations();

        // Some canvases need to do work when rendering has finished but before their content is composited.
        if (RefPtr htmlCanvas = dynamicDowncast<HTMLCanvasElement>(context->canvasBase())) {
            if (htmlCanvas->needsPreparationForDisplay())
                htmlCanvas->prepareForDisplay();
        }
    }
}

void Document::addCanvasNeedingPreparationForDisplayOrFlush(CanvasRenderingContext& context)
{
    if (context.hasDeferredOperations() || context.needsPreparationForDisplay()) {
        bool shouldSchedule = m_canvasContextsToPrepare.isEmptyIgnoringNullReferences();
        m_canvasContextsToPrepare.add(context);
        context.setIsInPreparationForDisplayOrFlush(true);
        if (shouldSchedule)
            scheduleRenderingUpdate(RenderingUpdateStep::PrepareCanvasesForDisplayOrFlush);
    }
}

void Document::removeCanvasNeedingPreparationForDisplayOrFlush(CanvasRenderingContext& context)
{
    m_canvasContextsToPrepare.remove(context);
    context.setIsInPreparationForDisplayOrFlush(false);
}

void Document::updateSleepDisablerIfNeeded()
{
    MediaProducerMediaStateFlags activeVideoCaptureMask { MediaProducerMediaState::HasActiveVideoCaptureDevice, MediaProducerMediaState::HasActiveScreenCaptureDevice, MediaProducerMediaState::HasActiveWindowCaptureDevice };
    if (m_mediaState & activeVideoCaptureMask) {
        m_sleepDisabler = makeUnique<SleepDisabler>("com.apple.WebCore: Document doing camera, screen or window capture"_s, PAL::SleepDisabler::Type::Display, pageID());
        return;
    }
    m_sleepDisabler = nullptr;
}

JSC::VM& Document::vm()
{
    return commonVM();
}

JSC::VM* Document::vmIfExists() const
{
    return commonVMOrNull();
}

String Document::debugDescription() const
{
    StringBuilder builder;

    builder.append("Document 0x"_s, hex(reinterpret_cast<uintptr_t>(this), Lowercase));
    if (frame() && frame()->isMainFrame())
        builder.append(" (main frame)"_s);

    builder.append(' ', documentURI());
    return builder.toString();
}

TextStream& operator<<(TextStream& ts, const Document& document)
{
    ts << document.debugDescription();
    return ts;
}

void Document::whenVisible(Function<void()>&& callback)
{
    if (hidden()) {
        m_whenIsVisibleHandlers.append(WTFMove(callback));
        return;
    }
    callback();
}

NotificationClient* Document::notificationClient()
{
#if ENABLE(NOTIFICATIONS)
    RefPtr page = this->page();
    if (!page)
        return nullptr;

    return &NotificationController::from(page.get())->client();
#else
    return nullptr;
#endif
}

GraphicsClient* Document::graphicsClient()
{
    RefPtr page = this->page();
    if (!page)
        return nullptr;
    return &page->chrome();
}

std::optional<PAL::SessionID> Document::sessionID() const
{
    if (RefPtr page = this->page())
        return page->sessionID();

    return std::nullopt;
}

void Document::addElementWithPendingUserAgentShadowTreeUpdate(Element& element)
{
    ASSERT(&element.document() == this);
    auto result = m_elementsWithPendingUserAgentShadowTreeUpdates.add(element);
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(result.isNewEntry);
}

void Document::removeElementWithPendingUserAgentShadowTreeUpdate(Element& element)
{
    ASSERT(&element.document() == this);
    m_elementsWithPendingUserAgentShadowTreeUpdates.remove(element);
    // FIXME: Assert that element was in m_elementsWithPendingUserAgentShadowTreeUpdates once re-entrancy to update style and layout have been removed.
}

bool Document::hasElementWithPendingUserAgentShadowTreeUpdate(Element& element) const
{
    return m_elementsWithPendingUserAgentShadowTreeUpdates.contains(element);
}

bool Document::isSameSiteForCookies(const URL& url) const
{
    auto domain = isTopDocument() ? RegistrableDomain(securityOrigin().data()) : RegistrableDomain(siteForCookies());
    return domain.matches(url);
}

void Document::notifyReportObservers(Ref<Report>&& reports)
{
    reportingScope().notifyReportObservers(WTFMove(reports));
}

String Document::endpointURIForToken(const String& token) const
{
    return reportingScope().endpointURIForToken(token);
}

String Document::httpUserAgent() const
{
    return userAgent(url());
}

void Document::sendReportToEndpoints(const URL& baseURL, const Vector<String>& endpointURIs, const Vector<String>& endpointTokens, Ref<FormData>&& report, ViolationReportType reportType)
{
    for (auto& url : endpointURIs)
        PingLoader::sendViolationReport(*frame(), URL { baseURL, url }, report.copyRef(), reportType);
    for (auto& token : endpointTokens) {
        if (auto url = endpointURIForToken(token); !url.isEmpty())
            PingLoader::sendViolationReport(*frame(), URL { baseURL, url }, report.copyRef(), reportType);
    }
}

void Document::addElementWithLangAttrMatchingDocumentElement(Element& element)
{
    m_elementsWithLangAttrMatchingDocumentElement.add(element);
}

void Document::removeElementWithLangAttrMatchingDocumentElement(Element& element)
{
    m_elementsWithLangAttrMatchingDocumentElement.remove(element);
}

RefPtr<ResizeObserver> Document::ensureResizeObserverForContainIntrinsicSize()
{
    if (!m_resizeObserverForContainIntrinsicSize)
        m_resizeObserverForContainIntrinsicSize = ResizeObserver::createNativeObserver(*this, CallbackForContainIntrinsicSize);
    return m_resizeObserverForContainIntrinsicSize;
}

void Document::observeForContainIntrinsicSize(Element& element)
{
    RefPtr resizeObserver = ensureResizeObserverForContainIntrinsicSize();
    resizeObserver->observe(element);
}

void Document::unobserveForContainIntrinsicSize(Element& element)
{
    if (RefPtr resizeObserverForContainIntrinsicSize = m_resizeObserverForContainIntrinsicSize)
        resizeObserverForContainIntrinsicSize->unobserve(element);
}

void Document::resetObservationSizeForContainIntrinsicSize(Element& target)
{
    if (RefPtr resizeObserverForContainIntrinsicSize = m_resizeObserverForContainIntrinsicSize)
        resizeObserverForContainIntrinsicSize->resetObservationSize(target);
}

OptionSet<NoiseInjectionPolicy> Document::noiseInjectionPolicies() const
{
    OptionSet<NoiseInjectionPolicy> policies;
    if (advancedPrivacyProtections().contains(AdvancedPrivacyProtections::FingerprintingProtections))
        policies.add(NoiseInjectionPolicy::Minimal);
    if (advancedPrivacyProtections().contains(AdvancedPrivacyProtections::ScriptTelemetry))
        policies.add(NoiseInjectionPolicy::Enhanced);
    return policies;
}

OptionSet<AdvancedPrivacyProtections> Document::advancedPrivacyProtections() const
{
    if (RefPtr loader = topDocument().loader())
        return loader->advancedPrivacyProtections();
    return { };
}

std::optional<uint64_t> Document::noiseInjectionHashSalt() const
{
    if (!page() || !noiseInjectionPolicies())
        return std::nullopt;
    return protectedPage()->noiseInjectionHashSaltForDomain(RegistrableDomain { m_url });
}

ContentVisibilityDocumentState& Document::contentVisibilityDocumentState()
{
    if (!m_contentVisibilityDocumentState)
        m_contentVisibilityDocumentState = makeUnique<ContentVisibilityDocumentState>();
    return *m_contentVisibilityDocumentState;
}

bool Document::isObservingContentVisibilityTargets() const
{
    return m_contentVisibilityDocumentState && m_contentVisibilityDocumentState->hasObservationTargets();
}

void Document::updateRelevancyOfContentVisibilityElements()
{
    if (m_contentRelevancyUpdate.isEmpty() || !isObservingContentVisibilityTargets())
        return;
    if (m_contentVisibilityDocumentState->updateRelevancyOfContentVisibilityElements(m_contentRelevancyUpdate) == DidUpdateAnyContentRelevancy::Yes)
        updateLayoutIgnorePendingStylesheets();
    m_contentRelevancyUpdate = { };
}

void Document::scheduleContentRelevancyUpdate(ContentRelevancy contentRelevancy)
{
    if (!isObservingContentVisibilityTargets())
        return;
    m_contentRelevancyUpdate.add(contentRelevancy);
    scheduleRenderingUpdate(RenderingUpdateStep::UpdateContentRelevancy);
}

void Document::updateContentRelevancyForScrollIfNeeded(const Element& scrollAnchor)
{
    if (!m_contentVisibilityDocumentState)
        return;
    return m_contentVisibilityDocumentState->updateContentRelevancyForScrollIfNeeded(scrollAnchor);
}

ViewTransition* Document::activeViewTransition() const
{
    return m_activeViewTransition.get();
}

bool Document::activeViewTransitionCapturedDocumentElement() const
{
    if (m_activeViewTransition)
        return m_activeViewTransition->documentElementIsCaptured();
    return false;
}

void Document::setActiveViewTransition(RefPtr<ViewTransition>&& viewTransition)
{
    std::optional<Style::PseudoClassChangeInvalidation> activeViewTransitionInvalidation;
    std::optional<Style::PseudoClassChangeInvalidation> activeViewTransitionTypeInvalidation;
    if (documentElement()) {
        activeViewTransitionInvalidation.emplace(*documentElement(), CSSSelector::PseudoClass::ActiveViewTransition, !!viewTransition);
        activeViewTransitionTypeInvalidation.emplace(*documentElement(), CSSSelector::PseudoClass::ActiveViewTransitionType, Style::PseudoClassChangeInvalidation::AnyValue);
    }

    clearRenderingIsSuppressedForViewTransition();
    m_activeViewTransition = WTFMove(viewTransition);
}

bool Document::hasViewTransitionPseudoElementTree() const
{
    return m_hasViewTransitionPseudoElementTree;
}

void Document::setHasViewTransitionPseudoElementTree(bool value)
{
    m_hasViewTransitionPseudoElementTree = value;
}

bool Document::renderingIsSuppressedForViewTransition() const
{
    return m_renderingIsSuppressedForViewTransition;
}

void Document::setRenderingIsSuppressedForViewTransitionAfterUpdateRendering()
{
    m_enableRenderingIsSuppressedForViewTransitionAfterUpdateRendering = true;
}

// FIXME: Remove this once cross-document view transitions capture old state on the subsequent rendering update instead of immediately.
void Document::setRenderingIsSuppressedForViewTransitionImmediately()
{
    m_renderingIsSuppressedForViewTransition = true;
    if (CheckedPtr view = renderView())
        view->compositor().setRenderingIsSuppressed(true);
}

void Document::clearRenderingIsSuppressedForViewTransition()
{
    m_enableRenderingIsSuppressedForViewTransitionAfterUpdateRendering = false;
    if (std::exchange(m_renderingIsSuppressedForViewTransition, false)) {
        if (CheckedPtr view = renderView())
            view->compositor().setRenderingIsSuppressed(false);
    }
}

void Document::flushDeferredRenderingIsSuppressedForViewTransitionChanges()
{
    if (std::exchange(m_enableRenderingIsSuppressedForViewTransitionAfterUpdateRendering, false))
        setRenderingIsSuppressedForViewTransitionImmediately();
}

// https://drafts.csswg.org/css-view-transitions/#ViewTransition-prepare
RefPtr<ViewTransition> Document::startViewTransition(StartViewTransitionCallbackOptions&& callbackOptions)
{
    if (!globalObject())
        return nullptr;

    RefPtr<ViewTransitionUpdateCallback> updateCallback = nullptr;
    Vector<AtomString> activeTypes { };

    if (callbackOptions) {
        WTF::switchOn(*callbackOptions, [&](RefPtr<JSViewTransitionUpdateCallback>& callback) {
            updateCallback = WTFMove(callback);
        }, [&](StartViewTransitionOptions& options) {
            updateCallback = WTFMove(options.update);

            if (options.types) {
                ASSERT(settings().viewTransitionTypesEnabled());
                activeTypes = WTFMove(*options.types);
            }
        });
    }

    Ref viewTransition = ViewTransition::createSamePage(*this, WTFMove(updateCallback), WTFMove(activeTypes));

    if (hidden()) {
        viewTransition->skipViewTransition(Exception { ExceptionCode::InvalidStateError, "View transition was skipped because document visibility state is hidden."_s });
        return viewTransition;
    }

    if (RefPtr activeViewTransition = m_activeViewTransition)
        activeViewTransition->skipViewTransition(Exception { ExceptionCode::AbortError, "Old view transition aborted by new view transition."_s });

    setActiveViewTransition(WTFMove(viewTransition));
    scheduleRenderingUpdate(RenderingUpdateStep::PerformPendingViewTransitions);

    return m_activeViewTransition;
}

void Document::performPendingViewTransitions()
{
    if (!m_activeViewTransition) {
        if (renderingIsSuppressedForViewTransition()) {
            clearRenderingIsSuppressedForViewTransition();
            DOCUMENT_RELEASE_LOG_ERROR(ViewTransitions, "Rendering suppressed enabled without active view transition");
        }
        return;
    }
    Ref activeViewTransition = *m_activeViewTransition;
    if (activeViewTransition->phase() == ViewTransitionPhase::PendingCapture)
        activeViewTransition->setupViewTransition();
    else if (activeViewTransition->phase() == ViewTransitionPhase::Animating)
        activeViewTransition->handleTransitionFrame();

    if (m_activeViewTransition)
        scheduleRenderingUpdate(RenderingUpdateStep::PerformPendingViewTransitions);
}

String Document::mediaKeysStorageDirectory()
{
    RefPtr currentPage = page();
    return currentPage ? currentPage->ensureMediaKeysStorageDirectoryForOrigin(securityOrigin().data()) : emptyString();
}

CheckedRef<Style::Scope> Document::checkedStyleScope()
{
    return m_styleScope.get();
}

CheckedRef<const Style::Scope> Document::checkedStyleScope() const
{
    return m_styleScope.get();
}

RefPtr<LocalFrameView> Document::protectedView() const
{
    return view();
}

CheckedPtr<RenderView> Document::checkedRenderView() const
{
    return m_renderView.get();
}

Ref<CSSFontSelector> Document::protectedFontSelector() const
{
    if (!m_fontSelector)
        return const_cast<Document&>(*this).ensureFontSelector();
    return *m_fontSelector;
}

PermissionsPolicy Document::permissionsPolicy() const
{
    // We create PermissionsPolicy on demand instead of at Document creation time,
    // because Document may not be set on Frame yet, and it would affect the computation
    // of PermissionsPolicy.
    if (!m_permissionsPolicy)
        m_permissionsPolicy = makeUnique<PermissionsPolicy>(*this);

    return *m_permissionsPolicy;
}

void Document::securityOriginDidChange()
{
    m_permissionsPolicy = nullptr;
}

} // namespace WebCore

#undef DOCUMENT_RELEASE_LOG
#undef DOCUMENT_RELEASE_LOG_ERROR
