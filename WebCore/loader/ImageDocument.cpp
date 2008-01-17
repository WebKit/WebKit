/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "ImageDocument.h"

#include "CachedImage.h"
#include "DocumentLoader.h"
#include "Element.h"
#include "EventListener.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "HTMLImageElement.h"
#include "HTMLNames.h"
#include "MouseEvent.h"
#include "Page.h"
#include "SegmentedString.h"
#include "Settings.h"
#include "Text.h"
#include "XMLTokenizer.h"

#if PLATFORM(MAC)
#include "ImageDocumentMac.h"
#endif 

using std::min;

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;

class ImageEventListener : public EventListener {
public:
    ImageEventListener(ImageDocument* doc) : m_doc(doc) { }
    virtual void handleEvent(Event*, bool isWindowEvent);

private:
    ImageDocument* m_doc;
};
    
class ImageTokenizer : public Tokenizer {
public:
    ImageTokenizer(ImageDocument* doc) : m_doc(doc) {}

    virtual bool write(const SegmentedString&, bool appendData);
    virtual void finish();
    virtual bool isWaitingForScripts() const;
    
    virtual bool wantsRawData() const { return true; }
    virtual bool writeRawData(const char* data, int len);

private:
    ImageDocument* m_doc;
};

class ImageDocumentElement : public HTMLImageElement {
public:
    ImageDocumentElement(ImageDocument* doc) : HTMLImageElement(doc), m_imageDocument(doc) { }
    virtual ~ImageDocumentElement();
    virtual void willMoveToNewOwnerDocument();

private:
    ImageDocument* m_imageDocument;
};

// --------

bool ImageTokenizer::write(const SegmentedString& s, bool appendData)
{
    ASSERT_NOT_REACHED();
    return false;
}

bool ImageTokenizer::writeRawData(const char* data, int len)
{
    CachedImage* cachedImage = m_doc->cachedImage();
    cachedImage->data(m_doc->frame()->loader()->documentLoader()->mainResourceData(), false);

    m_doc->imageChanged();
    
    return false;
}

void ImageTokenizer::finish()
{
    if (!m_parserStopped && m_doc->imageElement()) {
        CachedImage* cachedImage = m_doc->cachedImage();
        RefPtr<SharedBuffer> data = m_doc->frame()->loader()->documentLoader()->mainResourceData();

        // If this is a multipart image, make a copy of the current part, since the resource data
        // will be overwritten by the next part.
        if (m_doc->frame()->loader()->documentLoader()->isLoadingMultipartContent())
            data = new SharedBuffer(data->data(), data->size());

        cachedImage->data(data.release(), true);
        cachedImage->finish();

        cachedImage->setResponse(m_doc->frame()->loader()->documentLoader()->response());
        
        // FIXME: Need code to set the title for platforms other than Mac OS X.
#if PLATFORM(MAC)
        finishImageLoad(m_doc, cachedImage);
#endif
        m_doc->imageChanged();
    }

    m_doc->finishedParsing();
}
    
bool ImageTokenizer::isWaitingForScripts() const
{
    // An image document is never waiting for scripts
    return false;
}
    
// --------

ImageDocument::ImageDocument(DOMImplementation* implementation, Frame* frame)
    : HTMLDocument(implementation, frame)
    , m_imageElement(0)
    , m_imageSizeIsKnown(false)
    , m_didShrinkImage(false)
    , m_shouldShrinkImage(shouldShrinkToFit())
{
    setParseMode(Compat);
}
    
Tokenizer* ImageDocument::createTokenizer()
{
    return new ImageTokenizer(this);
}

void ImageDocument::createDocumentStructure()
{
    ExceptionCode ec;
    
    RefPtr<Element> rootElement = createElementNS(xhtmlNamespaceURI, "html", ec);
    appendChild(rootElement, ec);
    
    RefPtr<Element> body = createElementNS(xhtmlNamespaceURI, "body", ec);
    body->setAttribute(styleAttr, "margin: 0px;");
    
    rootElement->appendChild(body, ec);
    
    RefPtr<ImageDocumentElement> imageElement = new ImageDocumentElement(this);
    
    imageElement->setAttribute(styleAttr, "-webkit-user-select: none");        
    imageElement->setLoadManually(true);
    imageElement->setSrc(url());
    
    body->appendChild(imageElement, ec);
    
    if (shouldShrinkToFit()) {
        // Add event listeners
        RefPtr<EventListener> listener = new ImageEventListener(this);
        addWindowEventListener("resize", listener, false);
        imageElement->addEventListener("click", listener.release(), false);
    }

    m_imageElement = imageElement.get();
}

float ImageDocument::scale() const
{
    if (!m_imageElement)
        return 1.0f;

    IntSize imageSize = m_imageElement->cachedImage()->imageSize();
    IntSize windowSize = IntSize(frame()->view()->width(), frame()->view()->height());
    
    float widthScale = (float)windowSize.width() / imageSize.width();
    float heightScale = (float)windowSize.height() / imageSize.height();

    return min(widthScale, heightScale);
}

void ImageDocument::resizeImageToFit()
{
    if (!m_imageElement)
        return;

    IntSize imageSize = m_imageElement->cachedImage()->imageSize();

    float scale = this->scale();
    m_imageElement->setWidth(static_cast<int>(imageSize.width() * scale));
    m_imageElement->setHeight(static_cast<int>(imageSize.height() * scale));
    
    ExceptionCode ec;
    m_imageElement->style()->setProperty("cursor", "-webkit-zoom-in", ec);
}

void ImageDocument::imageClicked(int x, int y)
{
    if (!m_imageSizeIsKnown || imageFitsInWindow())
        return;

    m_shouldShrinkImage = !m_shouldShrinkImage;
    
    if (m_shouldShrinkImage)
        windowSizeChanged();
    else {
        restoreImageSize();
        
        updateLayout();
        
        float scale = this->scale();
        
        int scrollX = static_cast<int>(x / scale - (float)frame()->view()->width() / 2);
        int scrollY = static_cast<int>(y / scale - (float)frame()->view()->height() / 2);
        
        frame()->view()->setContentsPos(scrollX, scrollY);
    }
}

void ImageDocument::imageChanged()
{
    ASSERT(m_imageElement);
    
    if (m_imageSizeIsKnown)
        return;

    if (m_imageElement->cachedImage()->imageSize().isEmpty())
        return;
    
    m_imageSizeIsKnown = true;
    
    if (shouldShrinkToFit()) {
        // Force resizing of the image
        windowSizeChanged();
    }
}

void ImageDocument::restoreImageSize()
{
    if (!m_imageElement || !m_imageSizeIsKnown)
        return;
    
    m_imageElement->setWidth(m_imageElement->cachedImage()->imageSize().width());
    m_imageElement->setHeight(m_imageElement->cachedImage()->imageSize().height());
    
    ExceptionCode ec;
    if (imageFitsInWindow())
        m_imageElement->style()->removeProperty("cursor", ec);
    else
        m_imageElement->style()->setProperty("cursor", "-webkit-zoom-out", ec);
        
    m_didShrinkImage = false;
}

bool ImageDocument::imageFitsInWindow() const
{
    if (!m_imageElement)
        return true;

    IntSize imageSize = m_imageElement->cachedImage()->imageSize();
    IntSize windowSize = IntSize(frame()->view()->width(), frame()->view()->height());
    
    return imageSize.width() <= windowSize.width() && imageSize.height() <= windowSize.height();    
}

void ImageDocument::windowSizeChanged()
{
    if (!m_imageElement || !m_imageSizeIsKnown)
        return;

    bool fitsInWindow = imageFitsInWindow();
    
    // If the image has been explicitly zoomed in, restore the cursor if the image fits
    // and set it to a zoom out cursor if the image doesn't fit
    if (!m_shouldShrinkImage) {
        ExceptionCode ec;
        
        if (fitsInWindow)
            m_imageElement->style()->removeProperty("cursor", ec);
        else
            m_imageElement->style()->setProperty("cursor", "-webkit-zoom-out", ec);
        return;
    }
    
    if (m_didShrinkImage) {
        // If the window has been resized so that the image fits, restore the image size
        // otherwise update the restored image size.
        if (fitsInWindow)
            restoreImageSize();
        else
            resizeImageToFit();
    } else {
        // If the image isn't resized but needs to be, then resize it.
        if (!fitsInWindow) {
            resizeImageToFit();
            m_didShrinkImage = true;
        }
    }
}

CachedImage* ImageDocument::cachedImage()
{ 
    if (!m_imageElement)
        createDocumentStructure();
    
    return m_imageElement->cachedImage();
}

bool ImageDocument::shouldShrinkToFit() const
{
    return frame()->page()->settings()->shrinksStandaloneImagesToFit() &&
        frame()->page()->mainFrame() == frame();
}

// --------

void ImageEventListener::handleEvent(Event* event, bool isWindowEvent)
{
    if (event->type() == resizeEvent)
        m_doc->windowSizeChanged();
    else if (event->type() == clickEvent) {
        MouseEvent* mouseEvent = static_cast<MouseEvent*>(event);
        m_doc->imageClicked(mouseEvent->x(), mouseEvent->y());
    }
}

// --------

ImageDocumentElement::~ImageDocumentElement()
{
    if (m_imageDocument)
        m_imageDocument->disconnectImageElement();
}

void ImageDocumentElement::willMoveToNewOwnerDocument()
{
    if (m_imageDocument) {
        m_imageDocument->disconnectImageElement();
        m_imageDocument = 0;
    }
    HTMLImageElement::willMoveToNewOwnerDocument();
}

}
