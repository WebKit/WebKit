/*
 * Copyright (C) 2008, 2011 Apple Inc. All rights reserved.
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

#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "HTMLImageLoader.h"
#include "HTMLNames.h"
#include "Image.h"
#include "NodeRenderStyle.h"
#include "Page.h"
#include "RenderEmbeddedObject.h"
#include "RenderImage.h"
#include "SecurityOrigin.h"

namespace WebCore {

HTMLPlugInImageElement::HTMLPlugInImageElement(const QualifiedName& tagName, Document* document, bool createdByParser, PreferPlugInsForImagesOption preferPlugInsForImagesOption)
    : HTMLPlugInElement(tagName, document)
    // m_needsWidgetUpdate(!createdByParser) allows HTMLObjectElement to delay
    // widget updates until after all children are parsed.  For HTMLEmbedElement
    // this delay is unnecessary, but it is simpler to make both classes share
    // the same codepath in this class.
    , m_needsWidgetUpdate(!createdByParser)
    , m_shouldPreferPlugInsForImages(preferPlugInsForImagesOption == ShouldPreferPlugInsForImages)
    , m_needsDocumentActivationCallbacks(false)
{
    setHasCustomWillOrDidRecalcStyle();
}

HTMLPlugInImageElement::~HTMLPlugInImageElement()
{
    if (m_needsDocumentActivationCallbacks)
        document()->unregisterForPageCacheSuspensionCallbacks(this);
}

RenderEmbeddedObject* HTMLPlugInImageElement::renderEmbeddedObject() const
{
    // HTMLObjectElement and HTMLEmbedElement may return arbitrary renderers
    // when using fallback content.
    if (!renderer() || !renderer()->isEmbeddedObject())
        return 0;
    return toRenderEmbeddedObject(renderer());
}

bool HTMLPlugInImageElement::isImageType()
{
    if (m_serviceType.isEmpty() && protocolIs(m_url, "data"))
        m_serviceType = mimeTypeFromDataURL(m_url);

    if (Frame* frame = document()->frame()) {
        KURL completedURL = document()->completeURL(m_url);
        return frame->loader()->client()->objectContentType(completedURL, m_serviceType, shouldPreferPlugInsForImages()) == ObjectContentImage;
    }

    return Image::supportsType(m_serviceType);
}

// We don't use m_url, as it may not be the final URL that the object loads,
// depending on <param> values. 
bool HTMLPlugInImageElement::allowedToLoadFrameURL(const String& url)
{
    ASSERT(document());
    ASSERT(document()->frame());
    if (document()->frame()->page()->frameCount() >= Page::maxNumberOfFrames)
        return false;

    KURL completeURL = document()->completeURL(url);
    
    if (contentFrame() && protocolIsJavaScript(completeURL)
        && !document()->securityOrigin()->canAccess(contentDocument()->securityOrigin()))
        return false;
    
    // We allow one level of self-reference because some sites depend on that.
    // But we don't allow more than one.
    bool foundSelfReference = false;
    for (Frame* frame = document()->frame(); frame; frame = frame->tree()->parent()) {
        if (equalIgnoringFragmentIdentifier(frame->document()->url(), completeURL)) {
            if (foundSelfReference)
                return false;
            foundSelfReference = true;
        }
    }
    return true;
}

// We don't use m_url, or m_serviceType as they may not be the final values
// that <object> uses depending on <param> values. 
bool HTMLPlugInImageElement::wouldLoadAsNetscapePlugin(const String& url, const String& serviceType)
{
    ASSERT(document());
    ASSERT(document()->frame());
    KURL completedURL;
    if (!url.isEmpty())
        completedURL = document()->completeURL(url);

    FrameLoader* frameLoader = document()->frame()->loader();
    ASSERT(frameLoader);
    if (frameLoader->client()->objectContentType(completedURL, serviceType, shouldPreferPlugInsForImages()) == ObjectContentNetscapePlugin)
        return true;
    return false;
}

RenderObject* HTMLPlugInImageElement::createRenderer(RenderArena* arena, RenderStyle* style)
{
    // Once a PlugIn Element creates its renderer, it needs to be told when the Document goes
    // inactive or reactivates so it can clear the renderer before going into the page cache.
    if (!m_needsDocumentActivationCallbacks) {
        m_needsDocumentActivationCallbacks = true;
        document()->registerForPageCacheSuspensionCallbacks(this);
    }
    
    // Fallback content breaks the DOM->Renderer class relationship of this
    // class and all superclasses because createObject won't necessarily
    // return a RenderEmbeddedObject, RenderPart or even RenderWidget.
    if (useFallbackContent())
        return RenderObject::createObject(this, style);
    if (isImageType()) {
        RenderImage* image = new (arena) RenderImage(this);
        image->setImageResource(RenderImageResource::create());
        return image;
    }
    return new (arena) RenderEmbeddedObject(this);
}

bool HTMLPlugInImageElement::willRecalcStyle(StyleChange)
{
    // FIXME: Why is this necessary?  Manual re-attach is almost always wrong.
    if (!useFallbackContent() && needsWidgetUpdate() && renderer() && !isImageType())
        reattach();
    return true;
}

void HTMLPlugInImageElement::attach()
{
    bool isImage = isImageType();
    
    if (!isImage)
        queuePostAttachCallback(&HTMLPlugInImageElement::updateWidgetCallback, this);

    HTMLPlugInElement::attach();

    if (isImage && renderer() && !useFallbackContent()) {
        if (!m_imageLoader)
            m_imageLoader = adoptPtr(new HTMLImageLoader(this));
        m_imageLoader->updateFromElement();
    }
}
    
void HTMLPlugInImageElement::detach()
{
    // FIXME: Because of the insanity that is HTMLPlugInImageElement::recalcStyle,
    // we can end up detaching during an attach() call, before we even have a
    // renderer.  In that case, don't mark the widget for update.
    if (attached() && renderer() && !useFallbackContent())
        // Update the widget the next time we attach (detaching destroys the plugin).
        setNeedsWidgetUpdate(true);
    HTMLPlugInElement::detach();
}

void HTMLPlugInImageElement::updateWidgetIfNecessary()
{
    document()->updateStyleIfNeeded();

    if (!needsWidgetUpdate() || useFallbackContent() || isImageType())
        return;

    if (!renderEmbeddedObject() || renderEmbeddedObject()->pluginCrashedOrWasMissing())
        return;

    updateWidget(CreateOnlyNonNetscapePlugins);
}

void HTMLPlugInImageElement::finishParsingChildren()
{
    HTMLPlugInElement::finishParsingChildren();
    if (useFallbackContent())
        return;
    
    setNeedsWidgetUpdate(true);
    if (inDocument())
        setNeedsStyleRecalc();    
}

void HTMLPlugInImageElement::willMoveToNewOwnerDocument()
{
    if (m_needsDocumentActivationCallbacks)
        document()->unregisterForPageCacheSuspensionCallbacks(this);

    if (m_imageLoader)
        m_imageLoader->elementWillMoveToNewOwnerDocument();

    HTMLPlugInElement::willMoveToNewOwnerDocument();
}

void HTMLPlugInImageElement::didMoveToNewOwnerDocument()
{
    if (m_needsDocumentActivationCallbacks)
        document()->registerForPageCacheSuspensionCallbacks(this);   
    
    HTMLPlugInElement::didMoveToNewOwnerDocument();
}

void HTMLPlugInImageElement::documentWillSuspendForPageCache()
{
    if (RenderStyle* rs = renderStyle()) {
        m_customStyleForPageCache = RenderStyle::clone(rs);
        m_customStyleForPageCache->setDisplay(NONE);
    }

    setHasCustomStyleForRenderer();

    if (m_customStyleForPageCache)
        recalcStyle(Force);
        
    HTMLPlugInElement::documentWillSuspendForPageCache();
}

void HTMLPlugInImageElement::documentDidResumeFromPageCache()
{
    clearHasCustomStyleForRenderer();

    if (m_customStyleForPageCache) {
        m_customStyleForPageCache = 0;
        recalcStyle(Force);
    }
    
    HTMLPlugInElement::documentDidResumeFromPageCache();
}

PassRefPtr<RenderStyle> HTMLPlugInImageElement::customStyleForRenderer()
{
    if (!m_customStyleForPageCache)
        return renderStyle();

    return m_customStyleForPageCache;
}

void HTMLPlugInImageElement::updateWidgetCallback(Node* n, unsigned)
{
    static_cast<HTMLPlugInImageElement*>(n)->updateWidgetIfNecessary();
}

} // namespace WebCore
