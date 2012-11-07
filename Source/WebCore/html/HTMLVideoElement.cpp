/*
 * Copyright (C) 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#if ENABLE(VIDEO)
#include "HTMLVideoElement.h"

#include "Attribute.h"
#include "CSSPropertyNames.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "Document.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "HTMLImageLoader.h"
#include "HTMLNames.h"
#include "Page.h"
#include "RenderImage.h"
#include "RenderVideo.h"
#include "ScriptController.h"

namespace WebCore {

using namespace HTMLNames;

inline HTMLVideoElement::HTMLVideoElement(const QualifiedName& tagName, Document* document, bool createdByParser)
    : HTMLMediaElement(tagName, document, createdByParser)
{
    ASSERT(hasTagName(videoTag));
}

PassRefPtr<HTMLVideoElement> HTMLVideoElement::create(const QualifiedName& tagName, Document* document, bool createdByParser)
{
    RefPtr<HTMLVideoElement> videoElement(adoptRef(new HTMLVideoElement(tagName, document, createdByParser)));
    videoElement->suspendIfNeeded();
    return videoElement.release();
}

bool HTMLVideoElement::rendererIsNeeded(const NodeRenderingContext& context) 
{
    return HTMLElement::rendererIsNeeded(context); 
}

#if !ENABLE(PLUGIN_PROXY_FOR_VIDEO)
RenderObject* HTMLVideoElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    return new (arena) RenderVideo(this);
}
#endif

void HTMLVideoElement::attach()
{
    HTMLMediaElement::attach();

#if !ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    updateDisplayState();
    if (shouldDisplayPosterImage()) {
        if (!m_imageLoader)
            m_imageLoader = adoptPtr(new HTMLImageLoader(this));
        m_imageLoader->updateFromElement();
        if (renderer())
            toRenderImage(renderer())->imageResource()->setCachedImage(m_imageLoader->image()); 
    }
#endif
}

void HTMLVideoElement::collectStyleForAttribute(Attribute* attr, StylePropertySet* style)
{
    if (attr->name() == widthAttr)
        addHTMLLengthToStyle(style, CSSPropertyWidth, attr->value());
    else if (attr->name() == heightAttr)
        addHTMLLengthToStyle(style, CSSPropertyHeight, attr->value());
    else
        HTMLMediaElement::collectStyleForAttribute(attr, style);
}

bool HTMLVideoElement::isPresentationAttribute(const QualifiedName& name) const
{
    if (name == widthAttr || name == heightAttr)
        return true;
    return HTMLMediaElement::isPresentationAttribute(name);
}

void HTMLVideoElement::parseAttribute(Attribute* attr)
{
    const QualifiedName& attrName = attr->name();

    if (attrName == posterAttr) {
        // Force a poster recalc by setting m_displayMode to Unknown directly before calling updateDisplayState.
        HTMLMediaElement::setDisplayMode(Unknown);
        updateDisplayState();
#if !ENABLE(PLUGIN_PROXY_FOR_VIDEO)
        if (shouldDisplayPosterImage()) {
            if (!m_imageLoader)
                m_imageLoader = adoptPtr(new HTMLImageLoader(this));
            m_imageLoader->updateFromElementIgnoringPreviousError();
        } else {
            if (renderer())
                toRenderImage(renderer())->imageResource()->setCachedImage(0); 
        }
#endif
    } else
        HTMLMediaElement::parseAttribute(attr);
}

bool HTMLVideoElement::supportsFullscreen() const
{
    Page* page = document() ? document()->page() : 0;
    if (!page) 
        return false;

    if (!player() || !player()->supportsFullscreen())
        return false;

#if ENABLE(FULLSCREEN_API)
    // If the full screen API is enabled and is supported for the current element
    // do not require that the player has a video track to enter full screen.
    if (page->chrome()->client()->supportsFullScreenForElement(this, false))
        return true;
#endif

    if (!player()->hasVideo())
        return false;

    return page->chrome()->client()->supportsFullscreenForNode(this);
}

unsigned HTMLVideoElement::videoWidth() const
{
    if (!player())
        return 0;
    return player()->naturalSize().width();
}

unsigned HTMLVideoElement::videoHeight() const
{
    if (!player())
        return 0;
    return player()->naturalSize().height();
}

unsigned HTMLVideoElement::width() const
{
    bool ok;
    unsigned w = getAttribute(widthAttr).string().toUInt(&ok);
    return ok ? w : 0;
}
    
unsigned HTMLVideoElement::height() const
{
    bool ok;
    unsigned h = getAttribute(heightAttr).string().toUInt(&ok);
    return ok ? h : 0;
}
    
bool HTMLVideoElement::isURLAttribute(Attribute* attribute) const
{
    return HTMLMediaElement::isURLAttribute(attribute)
        || attribute->name() == posterAttr;
}

const QualifiedName& HTMLVideoElement::imageSourceAttributeName() const
{
    return posterAttr;
}

void HTMLVideoElement::setDisplayMode(DisplayMode mode)
{
    DisplayMode oldMode = displayMode();
    KURL poster = getNonEmptyURLAttribute(posterAttr);

    if (!poster.isEmpty()) {
        // We have a poster path, but only show it until the user triggers display by playing or seeking and the
        // media engine has something to display.
        if (mode == Video) {
            if (oldMode != Video && player())
                player()->prepareForRendering();
            if (!hasAvailableVideoFrame())
                mode = PosterWaitingForVideo;
        }
    } else if (oldMode != Video && player())
        player()->prepareForRendering();

    HTMLMediaElement::setDisplayMode(mode);

    if (player() && player()->canLoadPoster()) {
        bool canLoad = true;
        if (!poster.isEmpty()) {
            Frame* frame = document()->frame();
            FrameLoader* loader = frame ? frame->loader() : 0;
            canLoad = loader && loader->willLoadMediaElementURL(poster);
        }
        if (canLoad)
            player()->setPoster(poster);
    }

#if !ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    if (renderer() && displayMode() != oldMode)
        renderer()->updateFromElement();
#endif
}

void HTMLVideoElement::updateDisplayState()
{
    if (getNonEmptyURLAttribute(posterAttr).isEmpty())
        setDisplayMode(Video);
    else if (displayMode() < Poster)
        setDisplayMode(Poster);
}

void HTMLVideoElement::paintCurrentFrameInContext(GraphicsContext* context, const IntRect& destRect)
{
    MediaPlayer* player = HTMLMediaElement::player();
    if (!player)
        return;
    
    player->setVisible(true); // Make player visible or it won't draw.
    player->paintCurrentFrameInContext(context, destRect);
}

bool HTMLVideoElement::hasAvailableVideoFrame() const
{
    if (!player())
        return false;
    
    return player()->hasVideo() && player()->hasAvailableVideoFrame();
}

void HTMLVideoElement::webkitEnterFullscreen(ExceptionCode& ec)
{
    if (isFullscreen())
        return;

    // Generate an exception if this isn't called in response to a user gesture, or if the 
    // element does not support fullscreen.
    if ((userGestureRequiredForFullscreen() && !ScriptController::processingUserGesture()) || !supportsFullscreen()) {
        ec = INVALID_STATE_ERR;
        return;
    }

    enterFullscreen();
}

void HTMLVideoElement::webkitExitFullscreen()
{
    if (isFullscreen())
        exitFullscreen();
}

bool HTMLVideoElement::webkitSupportsFullscreen()
{
    return supportsFullscreen();
}

bool HTMLVideoElement::webkitDisplayingFullscreen()
{
    return isFullscreen();
}

void HTMLVideoElement::didMoveToNewDocument(Document* oldDocument)
{
    if (m_imageLoader)
        m_imageLoader->elementDidMoveToNewDocument();
    HTMLMediaElement::didMoveToNewDocument(oldDocument);
}

#if ENABLE(MEDIA_STATISTICS)
unsigned HTMLVideoElement::webkitDecodedFrameCount() const
{
    if (!player())
        return 0;

    return player()->decodedFrameCount();
}

unsigned HTMLVideoElement::webkitDroppedFrameCount() const
{
    if (!player())
        return 0;

    return player()->droppedFrameCount();
}
#endif

}

#endif
