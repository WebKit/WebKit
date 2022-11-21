/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Stefan Schimanski (1Stein@gmx.de)
 * Copyright (C) 2004-2019 Apple Inc. All rights reserved.
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
#include "Document.h"
#include "Event.h"
#include "EventHandler.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "HTMLNames.h"
#include "HitTestResult.h"
#include "Logging.h"
#include "MIMETypeRegistry.h"
#include "Page.h"
#include "PluginData.h"
#include "PluginReplacement.h"
#include "PluginViewBase.h"
#include "RenderEmbeddedObject.h"
#include "RenderLayer.h"
#include "RenderView.h"
#include "RenderWidget.h"
#include "ScriptController.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "SubframeLoader.h"
#include "Widget.h"
#include <wtf/IsoMallocInlines.h>

#if PLATFORM(COCOA)
#include "YouTubePluginReplacement.h"
#endif

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLPlugInElement);

using namespace HTMLNames;

HTMLPlugInElement::HTMLPlugInElement(const QualifiedName& tagName, Document& document)
    : HTMLFrameOwnerElement(tagName, document)
    , m_swapRendererTimer(*this, &HTMLPlugInElement::swapRendererTimerFired)
{
    setHasCustomStyleResolveCallbacks();
}

HTMLPlugInElement::~HTMLPlugInElement()
{
    ASSERT(!m_instance); // cleared in detach()
}

bool HTMLPlugInElement::willRespondToMouseClickEventsWithEditability(Editability) const
{
    if (isDisabledFormControl())
        return false;
    auto renderer = this->renderer();
    return renderer && renderer->isWidget();
}

void HTMLPlugInElement::willDetachRenderers()
{
    m_instance = nullptr;

    if (m_isCapturingMouseEvents) {
        if (RefPtr<Frame> frame = document().frame())
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
            m_instance = frame->script().createScriptInstanceForWidget(widget.get());
    }
    return m_instance.get();
}

PluginViewBase* HTMLPlugInElement::pluginWidget(PluginLoadingPolicy loadPolicy) const
{
    RenderWidget* renderWidget = loadPolicy == PluginLoadingPolicy::Load ? renderWidgetLoadingPlugin() : this->renderWidget();
    if (!renderWidget)
        return nullptr;

    return dynamicDowncast<PluginViewBase>(renderWidget->widget());
}

RenderWidget* HTMLPlugInElement::renderWidgetLoadingPlugin() const
{
    RefPtr<FrameView> view = document().view();
    if (!view || (!view->inUpdateEmbeddedObjects() && !view->layoutContext().isInLayout() && !view->isPainting())) {
        // Needs to load the plugin immediatedly because this function is called
        // when JavaScript code accesses the plugin.
        // FIXME: <rdar://16893708> Check if dispatching events here is safe.
        document().updateLayoutIgnorePendingStylesheets(Document::RunPostLayoutTasks::Synchronously);
    }
    return renderWidget(); // This will return nullptr if the renderer is not a RenderWidget.
}

bool HTMLPlugInElement::hasPresentationalHintsForAttribute(const QualifiedName& name) const
{
    if (name == widthAttr || name == heightAttr || name == vspaceAttr || name == hspaceAttr || name == alignAttr)
        return true;
    return HTMLFrameOwnerElement::hasPresentationalHintsForAttribute(name);
}

void HTMLPlugInElement::collectPresentationalHintsForAttribute(const QualifiedName& name, const AtomString& value, MutableStyleProperties& style)
{
    if (name == widthAttr)
        addHTMLLengthToStyle(style, CSSPropertyWidth, value);
    else if (name == heightAttr)
        addHTMLLengthToStyle(style, CSSPropertyHeight, value);
    else if (name == vspaceAttr) {
        addHTMLLengthToStyle(style, CSSPropertyMarginTop, value);
        addHTMLLengthToStyle(style, CSSPropertyMarginBottom, value);
    } else if (name == hspaceAttr) {
        addHTMLLengthToStyle(style, CSSPropertyMarginLeft, value);
        addHTMLLengthToStyle(style, CSSPropertyMarginRight, value);
    } else if (name == alignAttr)
        applyAlignmentAttributeToStyle(value, style);
    else
        HTMLFrameOwnerElement::collectPresentationalHintsForAttribute(name, value, style);
}

void HTMLPlugInElement::defaultEventHandler(Event& event)
{
    // Firefox seems to use a fake event listener to dispatch events to plug-in (tested with mouse events only).
    // This is observable via different order of events - in Firefox, event listeners specified in HTML attributes fires first, then an event
    // gets dispatched to plug-in, and only then other event listeners fire. Hopefully, this difference does not matter in practice.

    // FIXME: Mouse down and scroll events are passed down to plug-in via custom code in EventHandler; these code paths should be united.

    auto renderer = this->renderer();
    if (!is<RenderWidget>(renderer))
        return;

    if (is<RenderEmbeddedObject>(*renderer) && downcast<RenderEmbeddedObject>(*renderer).isPluginUnavailable())
        downcast<RenderEmbeddedObject>(*renderer).handleUnavailablePluginIndicatorEvent(&event);

    // Don't keep the widget alive over the defaultEventHandler call, since that can do things like navigate.
    {
        RefPtr<Widget> widget = downcast<RenderWidget>(*renderer).widget();
        if (widget)
            widget->handleEvent(event);
        if (event.defaultHandled())
            return;
    }
    HTMLFrameOwnerElement::defaultEventHandler(event);
}

bool HTMLPlugInElement::isKeyboardFocusable(KeyboardEvent*) const
{
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

    if (useFallbackContent() || !is<RenderEmbeddedObject>(renderer()))
        return false;
    return !downcast<RenderEmbeddedObject>(*renderer()).isPluginUnavailable();
}

RenderPtr<RenderElement> HTMLPlugInElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition& insertionPosition)
{
    if (m_pluginReplacement && m_pluginReplacement->willCreateRenderer())
        return m_pluginReplacement->createElementRenderer(*this, WTFMove(style), insertionPosition);

    return createRenderer<RenderEmbeddedObject>(*this, WTFMove(style));
}

void HTMLPlugInElement::swapRendererTimerFired()
{
    ASSERT(displayState() == PreparingPluginReplacement);
    if (userAgentShadowRoot())
        return;
    
    // Create a shadow root, which will trigger the code to add a snapshot container
    // and reattach, thus making a new Renderer.
    Ref<HTMLPlugInElement> protectedThis(*this);
    ensureUserAgentShadowRoot();
}

void HTMLPlugInElement::setDisplayState(DisplayState state)
{
    if (state == m_displayState)
        return;

    m_displayState = state;

    m_swapRendererTimer.stop();
    if (displayState() == PreparingPluginReplacement)
        m_swapRendererTimer.startOneShot(0_s);
}

void HTMLPlugInElement::didAddUserAgentShadowRoot(ShadowRoot& root)
{
    if (!m_pluginReplacement || !document().page() || displayState() != PreparingPluginReplacement)
        return;
    
    root.setResetStyleInheritance(true);

    m_pluginReplacement->installReplacement(root);

    setDisplayState(DisplayingPluginReplacement);
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
    if (m_pluginReplacement)
        return true;

    URL completedURL;
    if (!relativeURL.isEmpty())
        completedURL = document().completeURL(relativeURL);

    ReplacementPlugin* replacement = pluginReplacementForType(completedURL, mimeType);
    if (!replacement || !replacement->isEnabledBySettings(document().settings()))
        return false;

    LOG(Plugins, "%p - Found plug-in replacement for %s.", this, completedURL.string().utf8().data());

    m_pluginReplacement = replacement->create(*this, paramNames, paramValues);
    setDisplayState(PreparingPluginReplacement);
    return true;
}

bool HTMLPlugInElement::setReplacement(RenderEmbeddedObject::PluginUnavailabilityReason reason, const String& unavailabilityDescription)
{
    if (!is<RenderEmbeddedObject>(renderer()))
        return false;

    if (reason == RenderEmbeddedObject::UnsupportedPlugin)
        document().addConsoleMessage(MessageSource::JS, MessageLevel::Warning, "Tried to use an unsupported plug-in."_s);

    Ref<HTMLPlugInElement> protectedThis(*this);
    downcast<RenderEmbeddedObject>(*renderer()).setPluginUnavailabilityReasonWithDescription(reason, unavailabilityDescription);
    bool replacementIsObscured = isReplacementObscured();
    // hittest in isReplacementObscured() method could destroy the renderer. Let's refetch it.
    if (is<RenderEmbeddedObject>(renderer()))
        downcast<RenderEmbeddedObject>(*renderer()).setUnavailablePluginIndicatorIsHidden(replacementIsObscured);
    return replacementIsObscured;
}

bool HTMLPlugInElement::isReplacementObscured()
{
    Ref topDocument = document().topDocument();
    RefPtr topFrameView = topDocument->view();
    if (!topFrameView)
        return false;

    topFrameView->updateLayoutAndStyleIfNeededRecursive();

    // Updating the layout may have detached this document from the top document.
    auto* renderView = topDocument->renderView();
    if (!renderView || !document().view() || &document().topDocument() != topDocument.ptr())
        return false;

    if (!renderer() || !is<RenderEmbeddedObject>(*renderer()))
        return false;
    auto& pluginRenderer = downcast<RenderEmbeddedObject>(*renderer());
    // Check the opacity of each layer containing the element or its ancestors.
    float opacity = 1.0;
    for (auto* layer = pluginRenderer.enclosingLayer(); layer; layer = layer->parent()) {
        opacity *= layer->renderer().style().opacity();
        if (opacity < 0.1)
            return true;
    }
    // Calculate the absolute rect for the blocked plugin replacement text.
    LayoutPoint absoluteLocation(pluginRenderer.absoluteBoundingBoxRect().location());
    LayoutRect rect = pluginRenderer.unavailablePluginIndicatorBounds(absoluteLocation);
    if (rect.isEmpty())
        return true;
    auto viewRect = document().view()->convertToRootView(snappedIntRect(rect));
    auto x = viewRect.x();
    auto y = viewRect.y();
    auto width = viewRect.width();
    auto height = viewRect.height();
    // Hit test the center and near the corners of the replacement text to ensure
    // it is visible and is not masked by other elements.
    constexpr OptionSet<HitTestRequest::Type> hitType { HitTestRequest::Type::ReadOnly, HitTestRequest::Type::Active, HitTestRequest::Type::IgnoreClipping, HitTestRequest::Type::DisallowUserAgentShadowContent, HitTestRequest::Type::AllowChildFrameContent };
    HitTestResult result;
    HitTestLocation location { LayoutPoint { viewRect.center() } };
    ASSERT(!renderView->needsLayout());
    ASSERT(!renderView->document().needsStyleRecalc());
    bool hit = topDocument->hitTest(hitType, location, result);
    if (!hit || result.innerNode() != &pluginRenderer.frameOwnerElement())
        return true;

    location = LayoutPoint(x, y);
    hit = topDocument->hitTest(hitType, location, result);
    if (!hit || result.innerNode() != &pluginRenderer.frameOwnerElement())
        return true;

    location = LayoutPoint(x + width, y);
    hit = topDocument->hitTest(hitType, location, result);
    if (!hit || result.innerNode() != &pluginRenderer.frameOwnerElement())
        return true;

    location = LayoutPoint(x + width, y + height);
    hit = topDocument->hitTest(hitType, location, result);
    if (!hit || result.innerNode() != &pluginRenderer.frameOwnerElement())
        return true;

    location = LayoutPoint(x, y + height);
    hit = topDocument->hitTest(hitType, location, result);
    if (!hit || result.innerNode() != &pluginRenderer.frameOwnerElement())
        return true;
    return false;
}

bool HTMLPlugInElement::canLoadScriptURL(const URL&) const
{
    // FIXME: Probably want to at least check canAddSubframe.
    return true;
}

}
