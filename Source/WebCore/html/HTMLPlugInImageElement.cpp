/*
 * Copyright (C) 2008-2019 Apple Inc. All rights reserved.
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
#include "DocumentLoader.h"
#include "ElementInlines.h"
#include "EventLoop.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameLoaderClient.h"
#include "GCReachableRef.h"
#include "HTMLImageLoader.h"
#include "JSDOMConvertBoolean.h"
#include "JSDOMConvertInterface.h"
#include "JSDOMConvertStrings.h"
#include "JSShadowRoot.h"
#include "LegacySchemeRegistry.h"
#include "LocalizedStrings.h"
#include "Logging.h"
#include "MouseEvent.h"
#include "Page.h"
#include "PlatformMouseEvent.h"
#include "PluginViewBase.h"
#include "RenderImage.h"
#include "RenderTreeUpdater.h"
#include "ScriptController.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "StyleTreeResolver.h"
#include "SubframeLoader.h"
#include "TypedElementDescendantIterator.h"
#include "UserGestureIndicator.h"
#include <JavaScriptCore/CatchScope.h>
#include <JavaScriptCore/JSGlobalObjectInlines.h>
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLPlugInImageElement);

HTMLPlugInImageElement::HTMLPlugInImageElement(const QualifiedName& tagName, Document& document)
    : HTMLPlugInElement(tagName, document)
{
}

HTMLPlugInImageElement::~HTMLPlugInImageElement()
{
    if (m_needsDocumentActivationCallbacks)
        document().unregisterForDocumentSuspensionCallbacks(*this);
}

RenderEmbeddedObject* HTMLPlugInImageElement::renderEmbeddedObject() const
{
    // HTMLObjectElement and HTMLEmbedElement may return arbitrary renderers when using fallback content.
    return dynamicDowncast<RenderEmbeddedObject>(renderer());
}

bool HTMLPlugInImageElement::isImageType()
{
    if (m_serviceType.isEmpty() && protocolIs(m_url, "data"_s))
        m_serviceType = mimeTypeFromDataURL(m_url);

    if (RefPtr frame = document().frame())
        return frame->loader().client().objectContentType(document().completeURL(m_url), m_serviceType) == ObjectContentType::Image;

    return Image::supportsType(m_serviceType);
}

bool HTMLPlugInImageElement::canLoadURL(const String& relativeURL) const
{
    return canLoadURL(document().completeURL(relativeURL));
}

// Note that unlike HTMLFrameElementBase::canLoadURL this uses SecurityOrigin::canAccess.
bool HTMLPlugInImageElement::canLoadURL(const URL& completeURL) const
{
    if (completeURL.protocolIsJavaScript()) {
        RefPtr<Document> contentDocument = this->contentDocument();
        if (contentDocument && !document().securityOrigin().isSameOriginDomain(contentDocument->securityOrigin()))
            return false;
    }

    return !isProhibitedSelfReference(completeURL);
}

// We don't use m_url, or m_serviceType as they may not be the final values
// that <object> uses depending on <param> values.
bool HTMLPlugInImageElement::wouldLoadAsPlugIn(const String& relativeURL, const String& serviceType)
{
    ASSERT(document().frame());
    URL completedURL;
    if (!relativeURL.isEmpty())
        completedURL = document().completeURL(relativeURL);
    return document().frame()->loader().client().objectContentType(completedURL, serviceType) == ObjectContentType::PlugIn;
}

RenderPtr<RenderElement> HTMLPlugInImageElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition& insertionPosition)
{
    ASSERT(document().backForwardCacheState() == Document::NotInBackForwardCache);

    if (displayState() >= PreparingPluginReplacement)
        return HTMLPlugInElement::createElementRenderer(WTFMove(style), insertionPosition);

    // Once a plug-in element creates its renderer, it needs to be told when the document goes
    // inactive or reactivates so it can clear the renderer before going into the back/forward cache.
    if (!m_needsDocumentActivationCallbacks) {
        m_needsDocumentActivationCallbacks = true;
        document().registerForDocumentSuspensionCallbacks(*this);
    }

    if (useFallbackContent())
        return RenderElement::createFor(*this, WTFMove(style));

    if (isImageType())
        return createRenderer<RenderImage>(*this, WTFMove(style));

    return HTMLPlugInElement::createElementRenderer(WTFMove(style), insertionPosition);
}

bool HTMLPlugInImageElement::childShouldCreateRenderer(const Node& child) const
{
    return HTMLPlugInElement::childShouldCreateRenderer(child);
}

void HTMLPlugInImageElement::willRecalcStyle(Style::Change change)
{
    // Make sure style recalcs scheduled by a child shadow tree don't trigger reconstruction and cause flicker.
    if (change == Style::Change::None && styleValidity() == Style::Validity::Valid)
        return;

    // FIXME: There shoudn't be need to force render tree reconstruction here.
    // It is only done because loading and load event dispatching is tied to render tree construction.
    if (!useFallbackContent() && needsWidgetUpdate() && renderer() && !isImageType())
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
    if (RefPtr widget = pluginWidget(PluginLoadingPolicy::DoNotLoad))
        widget->willDetachRenderer();

    HTMLPlugInElement::willDetachRenderers();
}

void HTMLPlugInImageElement::scheduleUpdateForAfterStyleResolution()
{
    if (m_hasUpdateScheduledForAfterStyleResolution)
        return;

    document().incrementLoadEventDelayCount();

    m_hasUpdateScheduledForAfterStyleResolution = true;

    document().eventLoop().queueTask(TaskSource::DOMManipulation, [element = GCReachableRef { *this }] {
        element->updateAfterStyleResolution();
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
                m_imageLoader = makeUnique<HTMLImageLoader>(*this);
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
        m_imageLoader->elementDidMoveToNewDocument(oldDocument);

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

bool HTMLPlugInImageElement::shouldBypassCSPForPDFPlugin(const String& contentType) const
{
#if ENABLE(PDFKIT_PLUGIN)
    // We only consider bypassing this CSP check if plugins are disabled. In that case we know that
    // any plugin used is a browser implementation detail. It is not safe to skip this check
    // if plugins are enabled in case an external plugin is used to load PDF content.
    // FIXME: Check for alternative PDF plugins here so we can bypass this CSP check for PDFPlugin even when plugins are enabled.
    if (document().frame()->arePluginsEnabled())
        return false;

    return document().frame()->loader().client().shouldUsePDFPlugin(contentType, document().url().path());
#else
    UNUSED_PARAM(contentType);
    return false;
#endif
}

bool HTMLPlugInImageElement::canLoadPlugInContent(const String& relativeURL, const String& mimeType) const
{
    // Elements in user agent show tree should load whatever the embedding document policy is.
    if (isInUserAgentShadowTree())
        return true;

    URL completedURL;
    if (!relativeURL.isEmpty())
        completedURL = document().completeURL(relativeURL);

    ASSERT(document().contentSecurityPolicy());
    const ContentSecurityPolicy& contentSecurityPolicy = *document().contentSecurityPolicy();

    contentSecurityPolicy.upgradeInsecureRequestIfNeeded(completedURL, ContentSecurityPolicy::InsecureRequestType::Load);

    if (!shouldBypassCSPForPDFPlugin(mimeType) && !contentSecurityPolicy.allowObjectFromSource(completedURL))
        return false;

    auto& declaredMimeType = document().isPluginDocument() && document().ownerElement() ?
        document().ownerElement()->attributeWithoutSynchronization(HTMLNames::typeAttr) : attributeWithoutSynchronization(HTMLNames::typeAttr);
    return contentSecurityPolicy.allowPluginType(mimeType, declaredMimeType, completedURL);
}

bool HTMLPlugInImageElement::requestObject(const String& relativeURL, const String& mimeType, const Vector<AtomString>& paramNames, const Vector<AtomString>& paramValues)
{
    ASSERT(document().frame());

    if (relativeURL.isEmpty() && mimeType.isEmpty())
        return false;

    if (!canLoadPlugInContent(relativeURL, mimeType)) {
        renderEmbeddedObject()->setPluginUnavailabilityReason(RenderEmbeddedObject::PluginBlockedByContentSecurityPolicy);
        return false;
    }

    if (HTMLPlugInElement::requestObject(relativeURL, mimeType, paramNames, paramValues))
        return true;

    return document().frame()->loader().subframeLoader().requestObject(*this, relativeURL, getNameAttribute(), mimeType, paramNames, paramValues);
}

void HTMLPlugInImageElement::updateImageLoaderWithNewURLSoon()
{
    if (m_needsImageReload)
        return;

    m_needsImageReload = true;
    if (inRenderedDocument())
        scheduleUpdateForAfterStyleResolution();
    invalidateStyle();
}

} // namespace WebCore
