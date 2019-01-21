/*
 * Copyright (C) 2008-2017 Apple Inc. All rights reserved.
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
 *
 */

#include "config.h"
#include "HTMLPlugInImageElement.h"

#include "Chrome.h"
#include "ChromeClient.h"
#include "CommonVM.h"
#include "ContentSecurityPolicy.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameLoaderClient.h"
#include "HTMLImageLoader.h"
#include "JSDOMConvertBoolean.h"
#include "JSDOMConvertInterface.h"
#include "JSDOMConvertStrings.h"
#include "JSShadowRoot.h"
#include "LocalizedStrings.h"
#include "Logging.h"
#include "MouseEvent.h"
#include "Page.h"
#include "PlatformMouseEvent.h"
#include "PlugInClient.h"
#include "PluginViewBase.h"
#include "RenderImage.h"
#include "RenderSnapshottedPlugIn.h"
#include "RenderTreeUpdater.h"
#include "SchemeRegistry.h"
#include "ScriptController.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "StyleTreeResolver.h"
#include "SubframeLoader.h"
#include "TypedElementDescendantIterator.h"
#include "UserGestureIndicator.h"
#include <JavaScriptCore/CatchScope.h>
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLPlugInImageElement);

static const int sizingTinyDimensionThreshold = 40;
static const float sizingFullPageAreaRatioThreshold = 0.96;
static const Seconds autostartSoonAfterUserGestureThreshold = 5_s;

// This delay should not exceed the snapshot delay in PluginView.cpp
static const Seconds simulatedMouseClickTimerDelay { 750_ms };

#if PLATFORM(COCOA)
static const Seconds removeSnapshotTimerDelay { 1500_ms };
#endif

static const String titleText(Page& page, const String& mimeType)
{
    if (mimeType.isEmpty())
        return snapshottedPlugInLabelTitle();

    // FIXME: It's not consistent to get a string from the page's chrome client, but then cache it globally.
    // If it's global, it should come from elsewhere. If it's per-page then it should be cached per page.
    static NeverDestroyed<HashMap<String, String>> mimeTypeToLabelTitleMap;
    return mimeTypeToLabelTitleMap.get().ensure(mimeType, [&] {
        auto title = page.chrome().client().plugInStartLabelTitle(mimeType);
        if (!title.isEmpty())
            return title;
        return snapshottedPlugInLabelTitle();
    }).iterator->value;
};

static const String subtitleText(Page& page, const String& mimeType)
{
    if (mimeType.isEmpty())
        return snapshottedPlugInLabelSubtitle();

    // FIXME: It's not consistent to get a string from the page's chrome client, but then cache it globally.
    // If it's global, it should come from elsewhere. If it's per-page then it should be cached per page.
    static NeverDestroyed<HashMap<String, String>> mimeTypeToLabelSubtitleMap;
    return mimeTypeToLabelSubtitleMap.get().ensure(mimeType, [&] {
        auto subtitle = page.chrome().client().plugInStartLabelSubtitle(mimeType);
        if (!subtitle.isEmpty())
            return subtitle;
        return snapshottedPlugInLabelSubtitle();
    }).iterator->value;
};

HTMLPlugInImageElement::HTMLPlugInImageElement(const QualifiedName& tagName, Document& document)
    : HTMLPlugInElement(tagName, document)
    , m_simulatedMouseClickTimer(*this, &HTMLPlugInImageElement::simulatedMouseClickTimerFired, simulatedMouseClickTimerDelay)
    , m_removeSnapshotTimer(*this, &HTMLPlugInImageElement::removeSnapshotTimerFired)
    , m_createdDuringUserGesture(UserGestureIndicator::processingUserGesture())
{
    setHasCustomStyleResolveCallbacks();
}

void HTMLPlugInImageElement::finishCreating()
{
    scheduleUpdateForAfterStyleResolution();
}

HTMLPlugInImageElement::~HTMLPlugInImageElement()
{
    if (m_needsDocumentActivationCallbacks)
        document().unregisterForDocumentSuspensionCallbacks(*this);
}

void HTMLPlugInImageElement::setDisplayState(DisplayState state)
{
#if PLATFORM(COCOA)
    if (state == RestartingWithPendingMouseClick || state == Restarting) {
        m_isRestartedPlugin = true;
        m_snapshotDecision = NeverSnapshot;
        invalidateStyleAndLayerComposition();
        if (displayState() == DisplayingSnapshot)
            m_removeSnapshotTimer.startOneShot(removeSnapshotTimerDelay);
    }
#endif

    HTMLPlugInElement::setDisplayState(state);
}

RenderEmbeddedObject* HTMLPlugInImageElement::renderEmbeddedObject() const
{
    // HTMLObjectElement and HTMLEmbedElement may return arbitrary renderers when using fallback content.
    return is<RenderEmbeddedObject>(renderer()) ? downcast<RenderEmbeddedObject>(renderer()) : nullptr;
}

bool HTMLPlugInImageElement::isImageType()
{
    if (m_serviceType.isEmpty() && protocolIs(m_url, "data"))
        m_serviceType = mimeTypeFromDataURL(m_url);

    if (auto frame = makeRefPtr(document().frame()))
        return frame->loader().client().objectContentType(document().completeURL(m_url), m_serviceType) == ObjectContentType::Image;

    return Image::supportsType(m_serviceType);
}

// We don't use m_url, as it may not be the final URL that the object loads, depending on <param> values.
bool HTMLPlugInImageElement::allowedToLoadFrameURL(const String& url)
{
    URL completeURL = document().completeURL(url);
    if (contentFrame() && WTF::protocolIsJavaScript(completeURL) && !document().securityOrigin().canAccess(contentDocument()->securityOrigin()))
        return false;
    return document().frame()->isURLAllowed(completeURL);
}

// We don't use m_url, or m_serviceType as they may not be the final values
// that <object> uses depending on <param> values.
bool HTMLPlugInImageElement::wouldLoadAsPlugIn(const String& url, const String& serviceType)
{
    ASSERT(document().frame());
    URL completedURL;
    if (!url.isEmpty())
        completedURL = document().completeURL(url);
    return document().frame()->loader().client().objectContentType(completedURL, serviceType) == ObjectContentType::PlugIn;
}

RenderPtr<RenderElement> HTMLPlugInImageElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition& insertionPosition)
{
    ASSERT(document().pageCacheState() == Document::NotInPageCache);

    if (displayState() >= PreparingPluginReplacement)
        return HTMLPlugInElement::createElementRenderer(WTFMove(style), insertionPosition);

    // Once a plug-in element creates its renderer, it needs to be told when the document goes
    // inactive or reactivates so it can clear the renderer before going into the page cache.
    if (!m_needsDocumentActivationCallbacks) {
        m_needsDocumentActivationCallbacks = true;
        document().registerForDocumentSuspensionCallbacks(*this);
    }

    if (displayState() == DisplayingSnapshot) {
        auto renderSnapshottedPlugIn = createRenderer<RenderSnapshottedPlugIn>(*this, WTFMove(style));
        renderSnapshottedPlugIn->updateSnapshot(m_snapshotImage.get());
        return WTFMove(renderSnapshottedPlugIn);
    }

    if (useFallbackContent())
        return RenderElement::createFor(*this, WTFMove(style));

    if (isImageType())
        return createRenderer<RenderImage>(*this, WTFMove(style));

    return HTMLPlugInElement::createElementRenderer(WTFMove(style), insertionPosition);
}

bool HTMLPlugInImageElement::childShouldCreateRenderer(const Node& child) const
{
    if (is<RenderSnapshottedPlugIn>(renderer()) && !hasShadowRootParent(child))
        return false;

    return HTMLPlugInElement::childShouldCreateRenderer(child);
}

void HTMLPlugInImageElement::willRecalcStyle(Style::Change change)
{
    // Make sure style recalcs scheduled by a child shadow tree don't trigger reconstruction and cause flicker.
    if (change == Style::NoChange && styleValidity() == Style::Validity::Valid)
        return;

    // FIXME: There shoudn't be need to force render tree reconstruction here.
    // It is only done because loading and load event dispatching is tied to render tree construction.
    if (!useFallbackContent() && needsWidgetUpdate() && renderer() && !isImageType() && displayState() != DisplayingSnapshot)
        invalidateStyleAndRenderersForSubtree();
}

void HTMLPlugInImageElement::didRecalcStyle(Style::Change styleChange)
{
    scheduleUpdateForAfterStyleResolution();

    HTMLPlugInElement::didRecalcStyle(styleChange);
}

void HTMLPlugInImageElement::didAttachRenderers()
{
    m_needsWidgetUpdate = true;
    scheduleUpdateForAfterStyleResolution();

    // Update the RenderImageResource of the associated RenderImage.
    if (m_imageLoader && is<RenderImage>(renderer())) {
        auto& renderImageResource = downcast<RenderImage>(*renderer()).imageResource();
        if (!renderImageResource.cachedImage())
            renderImageResource.setCachedImage(m_imageLoader->image());
    }

    HTMLPlugInElement::didAttachRenderers();
}

void HTMLPlugInImageElement::willDetachRenderers()
{
    auto widget = makeRefPtr(pluginWidget(PluginLoadingPolicy::DoNotLoad));
    if (is<PluginViewBase>(widget))
        downcast<PluginViewBase>(*widget).willDetachRenderer();

    HTMLPlugInElement::willDetachRenderers();
}

void HTMLPlugInImageElement::scheduleUpdateForAfterStyleResolution()
{
    if (m_hasUpdateScheduledForAfterStyleResolution)
        return;

    document().incrementLoadEventDelayCount();

    m_hasUpdateScheduledForAfterStyleResolution = true;

    Style::queuePostResolutionCallback([protectedThis = makeRef(*this)] {
        protectedThis->updateAfterStyleResolution();
    });
}

void HTMLPlugInImageElement::updateAfterStyleResolution()
{
    m_hasUpdateScheduledForAfterStyleResolution = false;

    // Do this after style resolution, since the image or widget load might complete synchronously
    // and cause us to re-enter otherwise. Also, we can't really answer the question "do I have a renderer"
    // accurately until after style resolution.

    if (renderer() && !useFallbackContent()) {
        if (isImageType()) {
            if (!m_imageLoader)
                m_imageLoader = std::make_unique<HTMLImageLoader>(*this);
            if (m_needsImageReload)
                m_imageLoader->updateFromElementIgnoringPreviousError();
            else
                m_imageLoader->updateFromElement();
        } else {
            if (needsWidgetUpdate() && renderEmbeddedObject() && !renderEmbeddedObject()->isPluginUnavailable())
                updateWidget(CreatePlugins::No);
        }
    }

    // Either we reloaded the image just now, or we had some reason not to.
    // Either way, clear the flag now, since we don't need to remember to try again.
    m_needsImageReload = false;

    document().decrementLoadEventDelayCount();
}

void HTMLPlugInImageElement::didMoveToNewDocument(Document& oldDocument, Document& newDocument)
{
    ASSERT_WITH_SECURITY_IMPLICATION(&document() == &newDocument);
    if (m_needsDocumentActivationCallbacks) {
        oldDocument.unregisterForDocumentSuspensionCallbacks(*this);
        newDocument.registerForDocumentSuspensionCallbacks(*this);
    }

    if (m_imageLoader)
        m_imageLoader->elementDidMoveToNewDocument();

    if (m_hasUpdateScheduledForAfterStyleResolution) {
        oldDocument.decrementLoadEventDelayCount();
        newDocument.incrementLoadEventDelayCount();
    }

    HTMLPlugInElement::didMoveToNewDocument(oldDocument, newDocument);
}

void HTMLPlugInImageElement::prepareForDocumentSuspension()
{
    if (renderer())
        RenderTreeUpdater::tearDownRenderers(*this);

    HTMLPlugInElement::prepareForDocumentSuspension();
}

void HTMLPlugInImageElement::resumeFromDocumentSuspension()
{
    scheduleUpdateForAfterStyleResolution();
    invalidateStyleAndRenderersForSubtree();

    HTMLPlugInElement::resumeFromDocumentSuspension();
}

void HTMLPlugInImageElement::updateSnapshot(Image* image)
{
    if (displayState() > DisplayingSnapshot)
        return;

    m_snapshotImage = image;

    auto* renderer = this->renderer();
    if (!renderer)
        return;

    if (is<RenderSnapshottedPlugIn>(*renderer)) {
        downcast<RenderSnapshottedPlugIn>(*renderer).updateSnapshot(image);
        return;
    }

    if (is<RenderEmbeddedObject>(*renderer))
        renderer->repaint();
}

static DOMWrapperWorld& plugInImageElementIsolatedWorld()
{
    static auto& isolatedWorld = DOMWrapperWorld::create(commonVM()).leakRef();
    return isolatedWorld;
}

void HTMLPlugInImageElement::didAddUserAgentShadowRoot(ShadowRoot& root)
{
    HTMLPlugInElement::didAddUserAgentShadowRoot(root);
    if (displayState() >= PreparingPluginReplacement)
        return;

    auto* page = document().page();
    if (!page)
        return;

    // Reset any author styles that may apply as we only want explicit
    // styles defined in the injected user agents stylesheets to specify
    // the look-and-feel of the snapshotted plug-in overlay. 
    root.setResetStyleInheritance(true);
    
    String mimeType = serviceType();

    auto& isolatedWorld = plugInImageElementIsolatedWorld();
    document().ensurePlugInsInjectedScript(isolatedWorld);

    auto& scriptController = document().frame()->script();
    auto& globalObject = *JSC::jsCast<JSDOMGlobalObject*>(scriptController.globalObject(isolatedWorld));

    auto& vm = globalObject.vm();
    JSC::JSLockHolder lock(vm);
    auto scope = DECLARE_CATCH_SCOPE(vm);
    auto& state = *globalObject.globalExec();

    JSC::MarkedArgumentBuffer argList;
    argList.append(toJS<IDLInterface<ShadowRoot>>(state, globalObject, root));
    argList.append(toJS<IDLDOMString>(state, titleText(*page, mimeType)));
    argList.append(toJS<IDLDOMString>(state, subtitleText(*page, mimeType)));
    
    // This parameter determines whether or not the snapshot overlay should always be visible over the plugin snapshot.
    // If no snapshot was found then we want the overlay to be visible.
    argList.append(toJS<IDLBoolean>(!m_snapshotImage));
    ASSERT(!argList.hasOverflowed());

    // It is expected the JS file provides a createOverlay(shadowRoot, title, subtitle) function.
    auto* overlay = globalObject.get(&state, JSC::Identifier::fromString(&state, "createOverlay")).toObject(&state);
    ASSERT(!overlay == !!scope.exception());
    if (!overlay) {
        scope.clearException();
        return;
    }
    JSC::CallData callData;
    auto callType = overlay->methodTable(vm)->getCallData(overlay, callData);
    if (callType == JSC::CallType::None)
        return;

    call(&state, overlay, callType, callData, &globalObject, argList);
    scope.clearException();
}

bool HTMLPlugInImageElement::partOfSnapshotOverlay(const EventTarget* target) const
{
    static NeverDestroyed<AtomicString> selector(".snapshot-overlay", AtomicString::ConstructFromLiteral);
    auto shadow = userAgentShadowRoot();
    if (!shadow)
        return false;
    if (!is<Node>(target))
        return false;
    auto queryResult = shadow->querySelector(selector.get());
    if (queryResult.hasException())
        return false;
    auto snapshotLabel = makeRefPtr(queryResult.releaseReturnValue());
    return snapshotLabel && snapshotLabel->contains(downcast<Node>(target));
}

void HTMLPlugInImageElement::removeSnapshotTimerFired()
{
    m_snapshotImage = nullptr;
    m_isRestartedPlugin = false;
    invalidateStyleAndLayerComposition();
    if (renderer())
        renderer()->repaint();
}

void HTMLPlugInImageElement::restartSimilarPlugIns()
{
    // Restart any other snapshotted plugins in the page with the same origin. Note that they
    // may be in different frames, so traverse from the top of the document.

    auto plugInOrigin = m_loadedUrl.host();
    String mimeType = serviceType();
    Vector<Ref<HTMLPlugInImageElement>> similarPlugins;

    if (!document().page())
        return;

    for (RefPtr<Frame> frame = &document().page()->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (!frame->loader().subframeLoader().containsPlugins())
            continue;
        
        if (!frame->document())
            continue;

        for (auto& element : descendantsOfType<HTMLPlugInImageElement>(*frame->document())) {
            if (plugInOrigin == element.loadedUrl().host() && mimeType == element.serviceType())
                similarPlugins.append(element);
        }
    }

    for (auto& plugInToRestart : similarPlugins) {
        if (plugInToRestart->displayState() <= HTMLPlugInElement::DisplayingSnapshot) {
            LOG(Plugins, "%p Plug-in looks similar to a restarted plug-in. Restart.", plugInToRestart.ptr());
            plugInToRestart->restartSnapshottedPlugIn();
        }
        plugInToRestart->m_snapshotDecision = NeverSnapshot;
    }
}

void HTMLPlugInImageElement::userDidClickSnapshot(MouseEvent& event, bool forwardEvent)
{
    if (forwardEvent)
        m_pendingClickEventFromSnapshot = &event;

    auto plugInOrigin = m_loadedUrl.host();
    if (document().page() && !SchemeRegistry::shouldTreatURLSchemeAsLocal(document().page()->mainFrame().document()->baseURL().protocol().toStringWithoutCopying()) && document().page()->settings().autostartOriginPlugInSnapshottingEnabled())
        document().page()->plugInClient()->didStartFromOrigin(document().page()->mainFrame().document()->baseURL().host().toString(), plugInOrigin.toString(), serviceType(), document().page()->sessionID());

    LOG(Plugins, "%p User clicked on snapshotted plug-in. Restart.", this);
    restartSnapshottedPlugIn();
    if (forwardEvent)
        setDisplayState(RestartingWithPendingMouseClick);
    restartSimilarPlugIns();
}

void HTMLPlugInImageElement::setIsPrimarySnapshottedPlugIn(bool isPrimarySnapshottedPlugIn)
{
    if (!document().page() || !document().page()->settings().primaryPlugInSnapshotDetectionEnabled() || document().page()->settings().snapshotAllPlugIns())
        return;

    if (isPrimarySnapshottedPlugIn) {
        if (m_plugInWasCreated) {
            LOG(Plugins, "%p Plug-in was detected as the primary element in the page. Restart.", this);
            restartSnapshottedPlugIn();
            restartSimilarPlugIns();
        } else {
            LOG(Plugins, "%p Plug-in was detected as the primary element in the page, but is not yet created. Will restart later.", this);
            m_deferredPromotionToPrimaryPlugIn = true;
        }
    }
}

void HTMLPlugInImageElement::restartSnapshottedPlugIn()
{
    if (displayState() >= RestartingWithPendingMouseClick)
        return;

    setDisplayState(Restarting);
    invalidateStyleAndRenderersForSubtree();
}

void HTMLPlugInImageElement::dispatchPendingMouseClick()
{
    ASSERT(!m_simulatedMouseClickTimer.isActive());
    m_simulatedMouseClickTimer.restart();
}

void HTMLPlugInImageElement::simulatedMouseClickTimerFired()
{
    ASSERT(displayState() == RestartingWithPendingMouseClick);
    ASSERT(m_pendingClickEventFromSnapshot);

    setDisplayState(Playing);
    dispatchSimulatedClick(m_pendingClickEventFromSnapshot.get(), SendMouseOverUpDownEvents, DoNotShowPressedLook);

    m_pendingClickEventFromSnapshot = nullptr;
}

static bool documentHadRecentUserGesture(Document& document)
{
    MonotonicTime lastKnownUserGestureTimestamp = document.lastHandledUserGestureTimestamp();
    if (document.frame() != &document.page()->mainFrame() && document.page()->mainFrame().document())
        lastKnownUserGestureTimestamp = std::max(lastKnownUserGestureTimestamp, document.page()->mainFrame().document()->lastHandledUserGestureTimestamp());

    return MonotonicTime::now() - lastKnownUserGestureTimestamp < autostartSoonAfterUserGestureThreshold;
}

void HTMLPlugInImageElement::checkSizeChangeForSnapshotting()
{
    if (!m_needsCheckForSizeChange || m_snapshotDecision != MaySnapshotWhenResized || documentHadRecentUserGesture(document()))
        return;

    m_needsCheckForSizeChange = false;

    auto contentBoxRect = downcast<RenderBox>(*renderer()).contentBoxRect();
    int contentWidth = contentBoxRect.width();
    int contentHeight = contentBoxRect.height();

    if (contentWidth <= sizingTinyDimensionThreshold || contentHeight <= sizingTinyDimensionThreshold)
        return;

    LOG(Plugins, "%p Plug-in originally avoided snapshotting because it was sized %dx%d. Now it is %dx%d. Tell it to snapshot.\n", this, m_sizeWhenSnapshotted.width(), m_sizeWhenSnapshotted.height(), contentWidth, contentHeight);
    setDisplayState(WaitingForSnapshot);
    m_snapshotDecision = Snapshotted;
    auto widget = makeRefPtr(pluginWidget());
    if (is<PluginViewBase>(widget))
        downcast<PluginViewBase>(*widget).beginSnapshottingRunningPlugin();
}

static inline bool is100Percent(Length length)
{
    return length.isPercent() && length.percent() == 100;
}
    
static inline bool isSmallerThanTinySizingThreshold(const RenderEmbeddedObject& renderer)
{
    auto contentRect = renderer.contentBoxRect();
    return contentRect.width() <= sizingTinyDimensionThreshold || contentRect.height() <= sizingTinyDimensionThreshold;
}
    
bool HTMLPlugInImageElement::isTopLevelFullPagePlugin(const RenderEmbeddedObject& renderer) const
{
    ASSERT(document().frame());
    auto& frame = *document().frame();
    if (!frame.isMainFrame())
        return false;
    
    auto& style = renderer.style();
    auto visibleSize = frame.view()->visibleSize();
    auto contentRect = renderer.contentBoxRect();
    float contentWidth = contentRect.width();
    float contentHeight = contentRect.height();
    return is100Percent(style.width()) && is100Percent(style.height()) && contentWidth * contentHeight > visibleSize.area().unsafeGet() * sizingFullPageAreaRatioThreshold;
}

void HTMLPlugInImageElement::checkSnapshotStatus()
{
    if (!is<RenderSnapshottedPlugIn>(*renderer())) {
        if (displayState() == Playing)
            checkSizeChangeForSnapshotting();
        return;
    }
    
    // If width and height styles were previously not set and we've snapshotted the plugin we may need to restart the plugin so that its state can be updated appropriately.
    if (!document().page()->settings().snapshotAllPlugIns() && displayState() <= DisplayingSnapshot && !m_plugInDimensionsSpecified) {
        auto& renderer = downcast<RenderSnapshottedPlugIn>(*this->renderer());
        if (!renderer.style().logicalWidth().isSpecified() && !renderer.style().logicalHeight().isSpecified())
            return;
        
        m_plugInDimensionsSpecified = true;
        if (isTopLevelFullPagePlugin(renderer)) {
            m_snapshotDecision = NeverSnapshot;
            restartSnapshottedPlugIn();
        } else if (isSmallerThanTinySizingThreshold(renderer)) {
            m_snapshotDecision = MaySnapshotWhenResized;
            restartSnapshottedPlugIn();
        }
        return;
    }
    
    // Notify the shadow root that the size changed so that we may update the overlay layout.
    ensureUserAgentShadowRoot().dispatchEvent(Event::create(eventNames().resizeEvent, Event::CanBubble::Yes, Event::IsCancelable::No));
}
    
void HTMLPlugInImageElement::subframeLoaderWillCreatePlugIn(const URL& url)
{
    LOG(Plugins, "%p Plug-in URL: %s", this, m_url.utf8().data());
    LOG(Plugins, "   Actual URL: %s", url.string().utf8().data());
    LOG(Plugins, "   MIME type: %s", serviceType().utf8().data());

    m_loadedUrl = url;
    m_plugInWasCreated = false;
    m_deferredPromotionToPrimaryPlugIn = false;

    if (!document().page() || !document().page()->settings().plugInSnapshottingEnabled()) {
        m_snapshotDecision = NeverSnapshot;
        return;
    }

    if (displayState() == Restarting) {
        LOG(Plugins, "%p Plug-in is explicitly restarting", this);
        m_snapshotDecision = NeverSnapshot;
        setDisplayState(Playing);
        return;
    }

    if (displayState() == RestartingWithPendingMouseClick) {
        LOG(Plugins, "%p Plug-in is explicitly restarting but also waiting for a click", this);
        m_snapshotDecision = NeverSnapshot;
        return;
    }

    if (m_snapshotDecision == NeverSnapshot) {
        LOG(Plugins, "%p Plug-in is blessed, allow it to start", this);
        return;
    }

    bool inMainFrame = document().frame()->isMainFrame();

    if (document().isPluginDocument() && inMainFrame) {
        LOG(Plugins, "%p Plug-in document in main frame", this);
        m_snapshotDecision = NeverSnapshot;
        return;
    }

    if (UserGestureIndicator::processingUserGesture()) {
        LOG(Plugins, "%p Script is currently processing user gesture, set to play", this);
        m_snapshotDecision = NeverSnapshot;
        return;
    }

    if (m_createdDuringUserGesture) {
        LOG(Plugins, "%p Plug-in was created when processing user gesture, set to play", this);
        m_snapshotDecision = NeverSnapshot;
        return;
    }

    if (documentHadRecentUserGesture(document())) {
        LOG(Plugins, "%p Plug-in was created shortly after a user gesture, set to play", this);
        m_snapshotDecision = NeverSnapshot;
        return;
    }

    if (document().page()->settings().snapshotAllPlugIns()) {
        LOG(Plugins, "%p Plug-in forced to snapshot by user preference", this);
        m_snapshotDecision = Snapshotted;
        setDisplayState(WaitingForSnapshot);
        return;
    }

    if (document().page()->settings().autostartOriginPlugInSnapshottingEnabled() && document().page()->plugInClient() && document().page()->plugInClient()->shouldAutoStartFromOrigin(document().page()->mainFrame().document()->baseURL().host().toString(), url.host().toString(), serviceType())) {
        LOG(Plugins, "%p Plug-in from (%s, %s) is marked to auto-start, set to play", this, document().page()->mainFrame().document()->baseURL().host().utf8().data(), url.host().utf8().data());
        m_snapshotDecision = NeverSnapshot;
        return;
    }

    if (m_loadedUrl.isEmpty() && !serviceType().isEmpty()) {
        LOG(Plugins, "%p Plug-in has no src URL but does have a valid mime type %s, set to play", this, serviceType().utf8().data());
        m_snapshotDecision = MaySnapshotWhenContentIsSet;
        return;
    }

    if (!SchemeRegistry::shouldTreatURLSchemeAsLocal(m_loadedUrl.protocol().toStringWithoutCopying()) && !m_loadedUrl.host().isEmpty() && m_loadedUrl.host() == document().page()->mainFrame().document()->baseURL().host()) {
        LOG(Plugins, "%p Plug-in is served from page's domain, set to play", this);
        m_snapshotDecision = NeverSnapshot;
        return;
    }
    
    auto& renderer = downcast<RenderEmbeddedObject>(*this->renderer());
    auto contentRect = renderer.contentBoxRect();
    int contentWidth = contentRect.width();
    int contentHeight = contentRect.height();
    
    m_plugInDimensionsSpecified = renderer.style().logicalWidth().isSpecified() || renderer.style().logicalHeight().isSpecified();
    
    if (isTopLevelFullPagePlugin(renderer)) {
        LOG(Plugins, "%p Plug-in is top level full page, set to play", this);
        m_snapshotDecision = NeverSnapshot;
        return;
    }

    if (isSmallerThanTinySizingThreshold(renderer)) {
        LOG(Plugins, "%p Plug-in is very small %dx%d, set to play", this, contentWidth, contentHeight);
        m_sizeWhenSnapshotted = IntSize(contentWidth, contentHeight);
        m_snapshotDecision = MaySnapshotWhenResized;
        return;
    }

    if (!document().page()->plugInClient()) {
        LOG(Plugins, "%p There is no plug-in client. Set to wait for snapshot", this);
        m_snapshotDecision = NeverSnapshot;
        setDisplayState(WaitingForSnapshot);
        return;
    }

    LOG(Plugins, "%p Plug-in from (%s, %s) is not auto-start, sized at %dx%d, set to wait for snapshot", this, document().topDocument().baseURL().host().utf8().data(), url.host().utf8().data(), contentWidth, contentHeight);
    m_snapshotDecision = Snapshotted;
    setDisplayState(WaitingForSnapshot);
}

void HTMLPlugInImageElement::subframeLoaderDidCreatePlugIn(const Widget& widget)
{
    m_plugInWasCreated = true;

    if (is<PluginViewBase>(widget) && downcast<PluginViewBase>(widget).shouldAlwaysAutoStart()) {
        LOG(Plugins, "%p Plug-in should auto-start, set to play", this);
        m_snapshotDecision = NeverSnapshot;
        setDisplayState(Playing);
        return;
    }

    if (m_deferredPromotionToPrimaryPlugIn) {
        LOG(Plugins, "%p Plug-in was created, previously deferred promotion to primary. Will promote", this);
        setIsPrimarySnapshottedPlugIn(true);
        m_deferredPromotionToPrimaryPlugIn = false;
    }
}

void HTMLPlugInImageElement::defaultEventHandler(Event& event)
{
    if (is<RenderEmbeddedObject>(renderer()) && displayState() == WaitingForSnapshot && is<MouseEvent>(event) && event.type() == eventNames().clickEvent) {
        auto& mouseEvent = downcast<MouseEvent>(event);
        if (mouseEvent.button() == LeftButton) {
            userDidClickSnapshot(mouseEvent, true);
            mouseEvent.setDefaultHandled();
            return;
        }
    }
    HTMLPlugInElement::defaultEventHandler(event);
}

bool HTMLPlugInImageElement::allowedToLoadPluginContent(const String& url, const String& mimeType) const
{
    // Elements in user agent show tree should load whatever the embedding document policy is.
    if (isInUserAgentShadowTree())
        return true;

    URL completedURL;
    if (!url.isEmpty())
        completedURL = document().completeURL(url);

    ASSERT(document().contentSecurityPolicy());
    const ContentSecurityPolicy& contentSecurityPolicy = *document().contentSecurityPolicy();

    contentSecurityPolicy.upgradeInsecureRequestIfNeeded(completedURL, ContentSecurityPolicy::InsecureRequestType::Load);

    if (!contentSecurityPolicy.allowObjectFromSource(completedURL))
        return false;

    auto& declaredMimeType = document().isPluginDocument() && document().ownerElement() ?
        document().ownerElement()->attributeWithoutSynchronization(HTMLNames::typeAttr) : attributeWithoutSynchronization(HTMLNames::typeAttr);
    return contentSecurityPolicy.allowPluginType(mimeType, declaredMimeType, completedURL);
}

bool HTMLPlugInImageElement::requestObject(const String& url, const String& mimeType, const Vector<String>& paramNames, const Vector<String>& paramValues)
{
    ASSERT(document().frame());

    if (url.isEmpty() && mimeType.isEmpty())
        return false;

    if (!allowedToLoadPluginContent(url, mimeType)) {
        renderEmbeddedObject()->setPluginUnavailabilityReason(RenderEmbeddedObject::PluginBlockedByContentSecurityPolicy);
        return false;
    }

    if (HTMLPlugInElement::requestObject(url, mimeType, paramNames, paramValues))
        return true;
    
    return document().frame()->loader().subframeLoader().requestObject(*this, url, getNameAttribute(), mimeType, paramNames, paramValues);
}

void HTMLPlugInImageElement::updateImageLoaderWithNewURLSoon()
{
    if (m_needsImageReload)
        return;

    m_needsImageReload = true;
    scheduleUpdateForAfterStyleResolution();
    invalidateStyle();
}

} // namespace WebCore
