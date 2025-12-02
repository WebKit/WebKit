/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Stefan Schimanski (1Stein@gmx.de)
 * Copyright (C) 2004-2019 Apple Inc. All rights reserved.
 * Copyright (C) 2016 Google Inc. All rights reserved.
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
#include "HTMLPlugInElement.h"

#include "BridgeJSC.h"
#include "CSSPropertyNames.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "CommonVM.h"
#include "ContainerNodeInlines.h"
#include "ContentSecurityPolicy.h"
#include "DocumentEventLoop.h"
#include "DocumentLoader.h"
#include "DocumentPage.h"
#include "DocumentSecurityOrigin.h"
#include "DocumentView.h"
#include "ElementInlines.h"
#include "Event.h"
#include "EventHandler.h"
#include "EventLoop.h"
#include "EventNames.h"
#include "FrameDestructionObserverInlines.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "GCReachableRef.h"
#include "HTMLImageLoader.h"
#include "HTMLNames.h"
#include "HitTestResult.h"
#include "JSDOMConvertBoolean.h"
#include "JSDOMConvertInterface.h"
#include "JSDOMConvertStrings.h"
#include "JSShadowRoot.h"
#include "LegacySchemeRegistry.h"
#include "LocalFrame.h"
#include "LocalFrameLoaderClient.h"
#include "LocalizedStrings.h"
#include "Logging.h"
#include "MIMETypeRegistry.h"
#include "MouseEvent.h"
#include "NodeName.h"
#include "Page.h"
#include "PlatformMouseEvent.h"
#include "PluginData.h"
#include "PluginReplacement.h"
#include "PluginViewBase.h"
#include "RemoteFrame.h"
#include "RenderEmbeddedObject.h"
#include "RenderImage.h"
#include "RenderLayer.h"
#include "RenderStyleInlines.h"
#include "RenderTreeBuilder.h"
#include "RenderTreeUpdater.h"
#include "RenderView.h"
#include "RenderWidget.h"
#include "ScriptController.h"
#include "ScriptDisallowedScope.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "StyleTreeResolver.h"
#include "SubframeLoader.h"
#include "TypedElementDescendantIteratorInlines.h"
#include "UserGestureIndicator.h"
#include "VoidCallback.h"
#include "Widget.h"
#include <JavaScriptCore/CatchScope.h>
#include <JavaScriptCore/JSGlobalObjectInlines.h>
#include <wtf/TZoneMallocInlines.h>

#if PLATFORM(COCOA)
#include "YouTubePluginReplacement.h"
#endif

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(HTMLPlugInElement);

using namespace HTMLNames;

constexpr auto HTMLPlugInElement::pluginElementTypeFlags() -> OptionSet<TypeFlag>
{
    using enum TypeFlag;
    return { HasCustomStyleResolveCallbacks, HasDidMoveToNewDocument };
}

HTMLPlugInElement::HTMLPlugInElement(const QualifiedName& tagName, Document& document, OptionSet<TypeFlag> typeFlags)
    : HTMLFrameOwnerElement(tagName, document, typeFlags | pluginElementTypeFlags())
    , m_swapRendererTimer(*this, &HTMLPlugInElement::swapRendererTimerFired)
{
}

HTMLPlugInElement::~HTMLPlugInElement()
{
    ASSERT(!m_instance); // cleared in detach()
    ASSERT(!m_pendingPDFTestCallback);

    if (m_needsDocumentActivationCallbacks)
        protectedDocument()->unregisterForDocumentSuspensionCallbacks(*this);
}

bool HTMLPlugInElement::willRespondToMouseClickEventsWithEditability(Editability) const
{
    if (isDisabledFormControl())
        return false;
    return is<RenderWidget>(renderer());
}

void HTMLPlugInElement::willDetachRenderers()
{
    if (RefPtr widget = pluginWidget(PluginLoadingPolicy::DoNotLoad))
        widget->willDetachRenderer();

    m_instance = nullptr;

    if (m_isCapturingMouseEvents) {
        if (RefPtr frame = document().frame())
            frame->eventHandler().setCapturingMouseEventsElement(nullptr);
        m_isCapturingMouseEvents = false;
    }
}

void HTMLPlugInElement::resetInstance()
{
    m_instance = nullptr;
}

JSC::Bindings::Instance* HTMLPlugInElement::bindingsInstance()
{
    RefPtr frame = document().frame();
    if (!frame)
        return nullptr;

    // If the host dynamically turns off JavaScript (or Java) we will still return
    // the cached allocated Bindings::Instance.  Not supporting this edge-case is OK.

    if (!m_instance) {
        if (RefPtr widget = pluginWidget())
            m_instance = frame->checkedScript()->createScriptInstanceForWidget(widget.get());
    }
    return m_instance.get();
}

PluginViewBase* HTMLPlugInElement::pluginWidget(PluginLoadingPolicy loadPolicy) const
{
    CheckedPtr renderWidget = loadPolicy == PluginLoadingPolicy::Load ? renderWidgetLoadingPlugin() : this->renderWidget();
    if (!renderWidget)
        return nullptr;

    return dynamicDowncast<PluginViewBase>(renderWidget->widget());
}

CheckedPtr<RenderWidget> HTMLPlugInElement::renderWidgetLoadingPlugin() const
{
    Ref document = this->document();
    RefPtr view = document->view();
    if (!view || (!view->inUpdateEmbeddedObjects() && !view->layoutContext().isInLayout() && !view->isPainting())) {
        // Needs to load the plugin immediatedly because this function is called
        // when JavaScript code accesses the plugin.
        // FIXME: <rdar://16893708> Check if dispatching events here is safe.
        document->updateLayout({ LayoutOptions::IgnorePendingStylesheets, LayoutOptions::RunPostLayoutTasksSynchronously });
    }
    return renderWidget(); // This will return nullptr if the renderer is not a RenderWidget.
}

bool HTMLPlugInElement::hasPresentationalHintsForAttribute(const QualifiedName& name) const
{
    switch (name.nodeName()) {
    case AttributeNames::widthAttr:
    case AttributeNames::heightAttr:
    case AttributeNames::vspaceAttr:
    case AttributeNames::hspaceAttr:
    case AttributeNames::alignAttr:
        return true;
    default:
        break;
    }
    return HTMLFrameOwnerElement::hasPresentationalHintsForAttribute(name);
}

void HTMLPlugInElement::collectPresentationalHintsForAttribute(const QualifiedName& name, const AtomString& value, MutableStyleProperties& style)
{
    switch (name.nodeName()) {
    case AttributeNames::widthAttr:
        addHTMLLengthToStyle(style, CSSPropertyWidth, value);
        break;
    case AttributeNames::heightAttr:
        addHTMLLengthToStyle(style, CSSPropertyHeight, value);
        break;
    case AttributeNames::vspaceAttr:
        addHTMLLengthToStyle(style, CSSPropertyMarginTop, value);
        addHTMLLengthToStyle(style, CSSPropertyMarginBottom, value);
        break;
    case AttributeNames::hspaceAttr:
        addHTMLLengthToStyle(style, CSSPropertyMarginLeft, value);
        addHTMLLengthToStyle(style, CSSPropertyMarginRight, value);
        break;
    case AttributeNames::alignAttr:
        applyAlignmentAttributeToStyle(value, style);
        break;
    default:
        HTMLFrameOwnerElement::collectPresentationalHintsForAttribute(name, value, style);
        break;
    }
}

Node::InsertedIntoAncestorResult HTMLPlugInElement::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    auto result = HTMLFrameOwnerElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);
    if (insertionType.connectedToDocument)
        document().didConnectPluginElement();
    return result;
}

void HTMLPlugInElement::removedFromAncestor(RemovalType removalType, ContainerNode& oldParentOfRemovedTree)
{
    HTMLFrameOwnerElement::removedFromAncestor(removalType, oldParentOfRemovedTree);
    if (removalType.disconnectedFromDocument)
        document().didDisconnectPluginElement();
}

void HTMLPlugInElement::defaultEventHandler(Event& event)
{
    // Firefox seems to use a fake event listener to dispatch events to plug-in (tested with mouse events only).
    // This is observable via different order of events - in Firefox, event listeners specified in HTML attributes fires first, then an event
    // gets dispatched to plug-in, and only then other event listeners fire. Hopefully, this difference does not matter in practice.

    // FIXME: Mouse down and scroll events are passed down to plug-in via custom code in EventHandler; these code paths should be united.

    {
        CheckedPtr renderer = dynamicDowncast<RenderWidget>(this->renderer());
        if (!renderer)
            return;

        if (CheckedPtr renderEmbedded = dynamicDowncast<RenderEmbeddedObject>(*renderer); renderEmbedded && renderEmbedded->isPluginUnavailable())
            renderEmbedded->handleUnavailablePluginIndicatorEvent(&event);

        if (RefPtr widget = renderer->widget()) {
            renderer = nullptr;
            widget->handleEvent(event);
        }
    }
    if (event.defaultHandled())
        return;

    HTMLFrameOwnerElement::defaultEventHandler(event);
}

bool HTMLPlugInElement::isKeyboardFocusable(const FocusEventData& focusEventData) const
{
    if (HTMLFrameOwnerElement::isKeyboardFocusable(focusEventData))
        return true;
    return false;
}

bool HTMLPlugInElement::isPluginElement() const
{
    return true;
}

bool HTMLPlugInElement::supportsFocus() const
{
    if (HTMLFrameOwnerElement::supportsFocus())
        return true;

    if (useFallbackContent())
        return false;

    CheckedPtr renderer = dynamicDowncast<RenderEmbeddedObject>(this->renderer());
    return renderer && !renderer->isPluginUnavailable();
}

RenderPtr<RenderElement> HTMLPlugInElement::createPluginRenderer(RenderStyle&& style, const RenderTreePosition& insertionPosition)
{
    if (m_pluginReplacement && m_pluginReplacement->willCreateRenderer()) {
        RenderPtr<RenderElement> renderer = m_pluginReplacement->createElementRenderer(*this, WTFMove(style), insertionPosition);
        if (renderer)
            renderer->markIsYouTubeReplacement();
        return renderer;
    }

    return createRenderer<RenderEmbeddedObject>(*this, WTFMove(style));
}

RenderPtr<RenderElement> HTMLPlugInElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition& insertionPosition)
{
    ASSERT(document().backForwardCacheState() == Document::NotInBackForwardCache);

    if (displayState() >= DisplayState::PreparingPluginReplacement)
        return createPluginRenderer(WTFMove(style), insertionPosition);

    // Once a plug-in element creates its renderer, it needs to be told when the document goes
    // inactive or reactivates so it can clear the renderer before going into the back/forward cache.
    if (!m_needsDocumentActivationCallbacks) {
        m_needsDocumentActivationCallbacks = true;
        protectedDocument()->registerForDocumentSuspensionCallbacks(*this);
    }

    if (useFallbackContent())
        return RenderElement::createFor(*this, WTFMove(style));

    if (isImageType())
        return createRenderer<RenderImage>(RenderObject::Type::Image, *this, WTFMove(style));

    return createPluginRenderer(WTFMove(style), insertionPosition);
}

bool HTMLPlugInElement::isReplaced(const RenderStyle*) const
{
    return !m_pluginReplacement || !m_pluginReplacement->willCreateRenderer();
}

void HTMLPlugInElement::swapRendererTimerFired()
{
    ASSERT(displayState() == DisplayState::PreparingPluginReplacement);
    if (userAgentShadowRoot())
        return;
    
    // Create a shadow root, which will trigger the code to add a snapshot container
    // and reattach, thus making a new Renderer.
    ensureUserAgentShadowRoot();
}

void HTMLPlugInElement::setDisplayState(DisplayState state)
{
    if (state == m_displayState)
        return;

    m_displayState = state;

    m_swapRendererTimer.stop();
    if (displayState() == DisplayState::PreparingPluginReplacement)
        m_swapRendererTimer.startOneShot(0_s);
}

void HTMLPlugInElement::didAddUserAgentShadowRoot(ShadowRoot& root)
{
    if (!m_pluginReplacement || !document().page() || displayState() != DisplayState::PreparingPluginReplacement)
        return;
    
    m_pluginReplacement->installReplacement(root);

    setDisplayState(DisplayState::DisplayingPluginReplacement);
    invalidateStyleAndRenderersForSubtree();
}

#if PLATFORM(COCOA)
static void registrar(const ReplacementPlugin&);
#endif

static Vector<ReplacementPlugin*>& registeredPluginReplacements()
{
    static NeverDestroyed<Vector<ReplacementPlugin*>> registeredReplacements;
    static bool enginesQueried = false;
    
    if (enginesQueried)
        return registeredReplacements;
    enginesQueried = true;

#if PLATFORM(COCOA)
    YouTubePluginReplacement::registerPluginReplacement(registrar);
#endif
    
    return registeredReplacements;
}

#if PLATFORM(COCOA)
static void registrar(const ReplacementPlugin& replacement)
{
    registeredPluginReplacements().append(new ReplacementPlugin(replacement));
}
#endif

static ReplacementPlugin* pluginReplacementForType(const URL& url, const String& mimeType)
{
    Vector<ReplacementPlugin*>& replacements = registeredPluginReplacements();
    if (replacements.isEmpty())
        return nullptr;

    StringView extension;
    auto lastPathComponent = url.lastPathComponent();
    size_t dotOffset = lastPathComponent.reverseFind('.');
    if (dotOffset != notFound)
        extension = lastPathComponent.substring(dotOffset + 1);

    String type = mimeType;
    if (type.isEmpty() && url.protocolIsData())
        type = mimeTypeFromDataURL(url.string());
    
    if (type.isEmpty() && !extension.isEmpty()) {
        for (auto* replacement : replacements) {
            if (replacement->supportsFileExtension(extension) && replacement->supportsURL(url))
                return replacement;
        }
    }
    
    if (type.isEmpty()) {
        if (extension.isEmpty())
            return nullptr;
        type = MIMETypeRegistry::mediaMIMETypeForExtension(extension);
    }

    if (type.isEmpty())
        return nullptr;

    for (auto* replacement : replacements) {
        if (replacement->supportsType(type) && replacement->supportsURL(url))
            return replacement;
    }

    return nullptr;
}

bool HTMLPlugInElement::requestObject(const String& relativeURL, const String& mimeType, const Vector<AtomString>& paramNames, const Vector<AtomString>& paramValues)
{
    ASSERT(document().frame());

    if (relativeURL.isEmpty() && mimeType.isEmpty())
        return false;

    if (!canLoadPlugInContent(relativeURL, mimeType)) {
        CheckedRef { *renderEmbeddedObject() }->setPluginUnavailabilityReason(PluginUnavailabilityReason::PluginBlockedByContentSecurityPolicy);
        return false;
    }

    if (m_pluginReplacement)
        return true;

    Ref document = this->document();
    URL completedURL;
    if (!relativeURL.isEmpty())
        completedURL = document->completeURL(relativeURL);

    if (ReplacementPlugin* replacement = pluginReplacementForType(completedURL, mimeType)) {
        LOG(Plugins, "%p - Found plug-in replacement for %s.", this, completedURL.string().utf8().data());

        lazyInitialize(m_pluginReplacement, replacement->create(*this, paramNames, paramValues));
        setDisplayState(DisplayState::PreparingPluginReplacement);
        return true;
    }

    if (ScriptDisallowedScope::InMainThread::isScriptAllowed())
        return document->frame()->loader().subframeLoader().requestObject(*this, relativeURL, getNameAttribute(), mimeType, paramNames, paramValues);

    document->checkedEventLoop()->queueTask(TaskSource::Networking, [this, protectedThis = Ref { *this }, relativeURL, nameAttribute = getNameAttribute(), mimeType, paramNames, paramValues, document]() mutable {
        if (!this->isConnected() || &this->document() != document.ptr())
            return;
        RefPtr frame = this->document().frame();
        if (!frame)
            return;
        frame->loader().subframeLoader().requestObject(*this, relativeURL, nameAttribute, mimeType, paramNames, paramValues);
    });
    return true;
}

bool HTMLPlugInElement::canLoadScriptURL(const URL&) const
{
    // FIXME: Probably want to at least check canAddSubframe.
    return true;
}

void HTMLPlugInElement::pluginDestroyedWithPendingPDFTestCallback(RefPtr<VoidCallback>&& callback)
{
    ASSERT(!m_pendingPDFTestCallback);
    m_pendingPDFTestCallback = WTFMove(callback);
}

RefPtr<VoidCallback> HTMLPlugInElement::takePendingPDFTestCallback()
{
    if (!m_pendingPDFTestCallback)
        return nullptr;
    return WTFMove(m_pendingPDFTestCallback);
}

void HTMLPlugInElement::updateImageLoaderWithNewURLSoon()
{
    if (m_needsImageReload)
        return;

    m_needsImageReload = true;
    if (inRenderedDocument())
        scheduleUpdateForAfterStyleResolution();
    invalidateStyle();
}

void HTMLPlugInElement::scheduleUpdateForAfterStyleResolution()
{
    if (m_hasUpdateScheduledForAfterStyleResolution)
        return;

    Ref document = this->document();
    document->incrementLoadEventDelayCount();

    m_hasUpdateScheduledForAfterStyleResolution = true;

    document->checkedEventLoop()->queueTask(TaskSource::DOMManipulation, [element = GCReachableRef { *this }] {
        element->updateAfterStyleResolution();
    });
}

bool HTMLPlugInElement::shouldBypassCSPForPDFPlugin(const String& contentType) const
{
#if ENABLE(PDF_PLUGIN)
    return document().frame()->loader().client().shouldUsePDFPlugin(contentType, document().url().path());
#else
    UNUSED_PARAM(contentType);
    return false;
#endif
}

RenderEmbeddedObject* HTMLPlugInElement::renderEmbeddedObject() const
{
    // HTMLObjectElement and HTMLEmbedElement may return arbitrary renderers when using fallback content.
    return dynamicDowncast<RenderEmbeddedObject>(renderer());
}

bool HTMLPlugInElement::canLoadURL(const String& relativeURL) const
{
    return canLoadURL(protectedDocument()->completeURL(relativeURL));
}

bool HTMLPlugInElement::canLoadURL(const URL& completeURL) const
{
    if (completeURL.protocolIsJavaScript()) {
        if (is<RemoteFrame>(contentFrame()))
            return false;
        RefPtr contentDocument = this->contentDocument();
        if (contentDocument && !protectedDocument()->protectedSecurityOrigin()->isSameOriginDomain(contentDocument->protectedSecurityOrigin().get()))
            return false;
    }

    return !isProhibitedSelfReference(completeURL);
}

// We don't use m_url, or m_serviceType as they may not be the final values
// that <object> uses depending on <param> values.
bool HTMLPlugInElement::wouldLoadAsPlugIn(const String& relativeURL, const String& serviceType)
{
    Ref document = this->document();
    ASSERT(document->frame());
    URL completedURL;
    if (!relativeURL.isEmpty())
        completedURL = document->completeURL(relativeURL);
    return document->frame()->loader().client().objectContentType(completedURL, serviceType) == ObjectContentType::PlugIn;
}

bool HTMLPlugInElement::isImageType()
{
    if (m_serviceType.isEmpty() && protocolIs(m_url, "data"_s))
        m_serviceType = mimeTypeFromDataURL(m_url);

    Ref document = this->document();
    if (RefPtr frame = document->frame())
        return frame->loader().client().objectContentType(document->completeURL(m_url), m_serviceType) == ObjectContentType::Image;

    return Image::supportsType(m_serviceType);
}

void HTMLPlugInElement::updateAfterStyleResolution()
{
    m_hasUpdateScheduledForAfterStyleResolution = false;

    // Do this after style resolution, since the image or widget load might complete synchronously
    // and cause us to re-enter otherwise. Also, we can't really answer the question "do I have a renderer"
    // accurately until after style resolution.

    if (renderer() && !useFallbackContent()) {
        if (isImageType()) {
            if (!m_imageLoader)
                lazyInitialize(m_imageLoader, makeUniqueWithoutRefCountedCheck<HTMLImageLoader>(*this));
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

    protectedDocument()->decrementLoadEventDelayCount();
}

void HTMLPlugInElement::didMoveToNewDocument(Document& oldDocument, Document& newDocument)
{
    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(&document() == &newDocument);
    if (m_needsDocumentActivationCallbacks) {
        oldDocument.unregisterForDocumentSuspensionCallbacks(*this);
        newDocument.registerForDocumentSuspensionCallbacks(*this);
    }

    if (m_imageLoader)
        m_imageLoader->elementDidMoveToNewDocument(oldDocument);

    if (m_hasUpdateScheduledForAfterStyleResolution) {
        oldDocument.decrementLoadEventDelayCount();
        newDocument.incrementLoadEventDelayCount();
    }

    HTMLFrameOwnerElement::didMoveToNewDocument(oldDocument, newDocument);
}

bool HTMLPlugInElement::childShouldCreateRenderer(const Node& child) const
{
    return HTMLFrameOwnerElement::childShouldCreateRenderer(child);
}

void HTMLPlugInElement::willRecalcStyle(OptionSet<Style::Change> change)
{
    // Make sure style recalcs scheduled by a child shadow tree don't trigger reconstruction and cause flicker.
    if (!change && styleValidity() == Style::Validity::Valid)
        return;

    // FIXME: There shoudn't be need to force render tree reconstruction here.
    // It is only done because loading and load event dispatching is tied to render tree construction.
    if (!useFallbackContent() && needsWidgetUpdate() && renderer() && !isImageType())
        invalidateStyleAndRenderersForSubtree();
}

void HTMLPlugInElement::didRecalcStyle(OptionSet<Style::Change> styleChange)
{
    scheduleUpdateForAfterStyleResolution();

    HTMLFrameOwnerElement::didRecalcStyle(styleChange);
}

void HTMLPlugInElement::didAttachRenderers()
{
    m_needsWidgetUpdate = true;
    scheduleUpdateForAfterStyleResolution();

    // Update the RenderImageResource of the associated RenderImage.
    if (m_imageLoader) {
        if (CheckedPtr renderImage = dynamicDowncast<RenderImage>(renderer())) {
            CheckedRef renderImageResource = renderImage->imageResource();
            if (!renderImageResource->cachedImage())
                renderImageResource->setCachedImage(m_imageLoader->protectedImage());
        }
    }

    HTMLFrameOwnerElement::didAttachRenderers();
}

void HTMLPlugInElement::prepareForDocumentSuspension()
{
    if (renderer())
        RenderTreeUpdater::tearDownRenderers(*this);

    HTMLFrameOwnerElement::prepareForDocumentSuspension();
}

void HTMLPlugInElement::resumeFromDocumentSuspension()
{
    scheduleUpdateForAfterStyleResolution();
    invalidateStyleAndRenderersForSubtree();

    HTMLFrameOwnerElement::resumeFromDocumentSuspension();
}

bool HTMLPlugInElement::canLoadPlugInContent(const String& relativeURL, const String& mimeType) const
{
    // Elements in user agent show tree should load whatever the embedding document policy is.
    if (isInUserAgentShadowTree())
        return true;

    Ref document = this->document();
    URL completedURL;
    if (!relativeURL.isEmpty())
        completedURL = document->completeURL(relativeURL);

    ASSERT(document->contentSecurityPolicy());
    CheckedRef contentSecurityPolicy = *document->contentSecurityPolicy();

    contentSecurityPolicy->upgradeInsecureRequestIfNeeded(completedURL, ContentSecurityPolicy::InsecureRequestType::Load);

    if (!shouldBypassCSPForPDFPlugin(mimeType) && !contentSecurityPolicy->allowObjectFromSource(completedURL))
        return false;

    RefPtr ownerElement = document->ownerElement();
    auto& declaredMIMEType = document->isPluginDocument() && ownerElement ?
        ownerElement->attributeWithoutSynchronization(HTMLNames::typeAttr) : attributeWithoutSynchronization(HTMLNames::typeAttr);
    return contentSecurityPolicy->allowPluginType(mimeType, declaredMIMEType, completedURL);
}

}
