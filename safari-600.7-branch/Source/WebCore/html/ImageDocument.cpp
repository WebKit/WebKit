/*
 * Copyright (C) 2006, 2007, 2008, 2010, 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
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
#include "Chrome.h"
#include "DocumentLoader.h"
#include "EventListener.h"
#include "EventNames.h"
#include "ExceptionCodePlaceholder.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "FrameView.h"
#include "HTMLBodyElement.h"
#include "HTMLHtmlElement.h"
#include "HTMLImageElement.h"
#include "HTMLNames.h"
#include "LocalizedStrings.h"
#include "MIMETypeRegistry.h"
#include "MainFrame.h"
#include "MouseEvent.h"
#include "Page.h"
#include "RawDataDocumentParser.h"
#include "RenderElement.h"
#include "ResourceBuffer.h"
#include "Settings.h"

namespace WebCore {

using namespace HTMLNames;

#if !PLATFORM(IOS)
class ImageEventListener final : public EventListener {
public:
    static PassRefPtr<ImageEventListener> create(ImageDocument& document) { return adoptRef(new ImageEventListener(document)); }

private:
    ImageEventListener(ImageDocument& document)
        : EventListener(ImageEventListenerType)
        , m_document(document)
    {
    }

    virtual bool operator==(const EventListener&) override;
    virtual void handleEvent(ScriptExecutionContext*, Event*) override;

    ImageDocument& m_document;
};
#endif

class ImageDocumentParser final : public RawDataDocumentParser {
public:
    static PassRefPtr<ImageDocumentParser> create(ImageDocument& document)
    {
        return adoptRef(new ImageDocumentParser(document));
    }

private:
    ImageDocumentParser(ImageDocument& document)
        : RawDataDocumentParser(document)
    {
    }

    ImageDocument& document() const;

    virtual void appendBytes(DocumentWriter&, const char*, size_t) override;
    virtual void finish() override;
};

class ImageDocumentElement final : public HTMLImageElement {
public:
    static PassRefPtr<ImageDocumentElement> create(ImageDocument&);

private:
    ImageDocumentElement(ImageDocument& document)
        : HTMLImageElement(imgTag, document)
        , m_imageDocument(&document)
    {
    }

    virtual ~ImageDocumentElement();
    virtual void didMoveToNewDocument(Document* oldDocument) override;

    ImageDocument* m_imageDocument;
};

inline PassRefPtr<ImageDocumentElement> ImageDocumentElement::create(ImageDocument& document)
{
    return adoptRef(new ImageDocumentElement(document));
}

// --------

HTMLImageElement* ImageDocument::imageElement() const
{
    return m_imageElement;
}

LayoutSize ImageDocument::imageSize()
{
    ASSERT(m_imageElement);
    updateStyleIfNeeded();
    return m_imageElement->cachedImage()->imageSizeForRenderer(m_imageElement->renderer(), frame() ? frame()->pageZoomFactor() : 1);
}

void ImageDocument::updateDuringParsing()
{
    if (!frame()->settings().areImagesEnabled())
        return;

    if (!m_imageElement)
        createDocumentStructure();

    m_imageElement->cachedImage()->addDataBuffer(loader()->mainResourceData().get());

    imageUpdated();
}

void ImageDocument::finishedParsing()
{
    if (!parser()->isStopped() && m_imageElement) {
        CachedImage& cachedImage = *m_imageElement->cachedImage();
        RefPtr<ResourceBuffer> data = loader()->mainResourceData();

        // If this is a multipart image, make a copy of the current part, since the resource data
        // will be overwritten by the next part.
        if (loader()->isLoadingMultipartContent())
            data = data->copy();

        cachedImage.finishLoading(data.get());
        cachedImage.finish();

        // Report the natural image size in the page title, regardless of zoom level.
        // At a zoom level of 1 the image is guaranteed to have an integer size.
        updateStyleIfNeeded();
        IntSize size = flooredIntSize(cachedImage.imageSizeForRenderer(m_imageElement->renderer(), 1));
        if (size.width()) {
            // Compute the title. We use the decoded filename of the resource, falling
            // back on the hostname if there is no path.
            String name = decodeURLEscapeSequences(url().lastPathComponent());
            if (name.isEmpty())
                name = url().host();
            setTitle(imageTitle(name, size));
        }

        imageUpdated();
    }

    HTMLDocument::finishedParsing();
}
    
inline ImageDocument& ImageDocumentParser::document() const
{
    // Only used during parsing, so document is guaranteed to be non-null.
    ASSERT(RawDataDocumentParser::document());
    return toImageDocument(*RawDataDocumentParser::document());
}

void ImageDocumentParser::appendBytes(DocumentWriter&, const char*, size_t)
{
    document().updateDuringParsing();
}

void ImageDocumentParser::finish()
{
    document().finishedParsing();
}

ImageDocument::ImageDocument(Frame& frame, const URL& url)
    : HTMLDocument(&frame, url, ImageDocumentClass)
    , m_imageElement(nullptr)
    , m_imageSizeIsKnown(false)
#if !PLATFORM(IOS)
    , m_didShrinkImage(false)
#endif
    , m_shouldShrinkImage(frame.settings().shrinksStandaloneImagesToFit() && frame.isMainFrame())
{
    setCompatibilityMode(DocumentCompatibilityMode::QuirksMode);
    lockCompatibilityMode();
}
    
PassRefPtr<DocumentParser> ImageDocument::createParser()
{
    return ImageDocumentParser::create(*this);
}

void ImageDocument::createDocumentStructure()
{
    RefPtr<Element> rootElement = Document::createElement(htmlTag, false);
    appendChild(rootElement);
    toHTMLHtmlElement(rootElement.get())->insertedByParser();

    frame()->injectUserScripts(InjectAtDocumentStart);

    RefPtr<Element> body = Document::createElement(bodyTag, false);
    body->setAttribute(styleAttr, "margin: 0px");
    if (MIMETypeRegistry::isPDFMIMEType(document().loader()->responseMIMEType()))
        toHTMLBodyElement(body.get())->setInlineStyleProperty(CSSPropertyBackgroundColor, "white", CSSPrimitiveValue::CSS_IDENT);
    rootElement->appendChild(body);
    
    RefPtr<ImageDocumentElement> imageElement = ImageDocumentElement::create(*this);
    if (m_shouldShrinkImage)
        imageElement->setAttribute(styleAttr, "-webkit-user-select:none; display:block; margin:auto;");
    else
        imageElement->setAttribute(styleAttr, "-webkit-user-select:none;");
    imageElement->setLoadManually(true);
    imageElement->setSrc(url().string());
    imageElement->cachedImage()->setResponse(loader()->response());
    body->appendChild(imageElement);
    
    if (m_shouldShrinkImage) {
#if PLATFORM(IOS)
        // Set the viewport to be in device pixels (rather than the default of 980).
        processViewport(ASCIILiteral("width=device-width"), ViewportArguments::ImageDocument);
#else
        RefPtr<EventListener> listener = ImageEventListener::create(*this);
        if (DOMWindow* window = this->domWindow())
            window->addEventListener("resize", listener, false);
        imageElement->addEventListener("click", listener.release(), false);
#endif
    }

    m_imageElement = imageElement.get();
}

void ImageDocument::imageUpdated()
{
    ASSERT(m_imageElement);

    if (m_imageSizeIsKnown)
        return;

    LayoutSize imageSize = this->imageSize();
    if (imageSize.isEmpty())
        return;

    m_imageSizeIsKnown = true;

    if (m_shouldShrinkImage) {
#if PLATFORM(IOS)
        FloatSize screenSize = page()->chrome().screenSize();
        if (imageSize.width() > screenSize.width())
            processViewport(String::format("width=%u", static_cast<unsigned>(imageSize.width().toInt())), ViewportArguments::ImageDocument);
#else
        // Call windowSizeChanged for its side effect of sizing the image.
        windowSizeChanged();
#endif
    }
}

#if !PLATFORM(IOS)
float ImageDocument::scale()
{
    if (!m_imageElement)
        return 1;

    FrameView* view = this->view();
    if (!view)
        return 1;

    LayoutSize imageSize = this->imageSize();

    IntSize viewportSize = view->visibleSize();
    float widthScale = viewportSize.width() / imageSize.width().toFloat();
    float heightScale = viewportSize.height() / imageSize.height().toFloat();

    return std::min(widthScale, heightScale);
}

void ImageDocument::resizeImageToFit()
{
    if (!m_imageElement)
        return;

    LayoutSize imageSize = this->imageSize();

    float scale = this->scale();
    m_imageElement->setWidth(static_cast<int>(imageSize.width() * scale));
    m_imageElement->setHeight(static_cast<int>(imageSize.height() * scale));

    m_imageElement->setInlineStyleProperty(CSSPropertyCursor, CSSValueWebkitZoomIn);
}

void ImageDocument::restoreImageSize()
{
    if (!m_imageElement || !m_imageSizeIsKnown)
        return;

    LayoutSize imageSize = this->imageSize();
    m_imageElement->setWidth(imageSize.width());
    m_imageElement->setHeight(imageSize.height());

    if (imageFitsInWindow())
        m_imageElement->removeInlineStyleProperty(CSSPropertyCursor);
    else
        m_imageElement->setInlineStyleProperty(CSSPropertyCursor, CSSValueWebkitZoomOut);

    m_didShrinkImage = false;
}

bool ImageDocument::imageFitsInWindow()
{
    if (!m_imageElement)
        return true;

    FrameView* view = this->view();
    if (!view)
        return true;

    LayoutSize imageSize = this->imageSize();
    IntSize viewportSize = view->visibleSize();
    return imageSize.width() <= viewportSize.width() && imageSize.height() <= viewportSize.height();
}


void ImageDocument::windowSizeChanged()
{
    if (!m_imageElement || !m_imageSizeIsKnown)
        return;

    bool fitsInWindow = imageFitsInWindow();

    // If the image has been explicitly zoomed in, restore the cursor if the image fits
    // and set it to a zoom out cursor if the image doesn't fit
    if (!m_shouldShrinkImage) {
        if (fitsInWindow)
            m_imageElement->removeInlineStyleProperty(CSSPropertyCursor);
        else
            m_imageElement->setInlineStyleProperty(CSSPropertyCursor, CSSValueWebkitZoomOut);
        return;
    }

    if (m_didShrinkImage) {
        // If the window has been resized so that the image fits, restore the image size,
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

void ImageDocument::imageClicked(int x, int y)
{
    if (!m_imageSizeIsKnown || imageFitsInWindow())
        return;

    m_shouldShrinkImage = !m_shouldShrinkImage;

    if (m_shouldShrinkImage) {
        // Call windowSizeChanged for its side effect of sizing the image.
        windowSizeChanged();
    } else {
        restoreImageSize();

        updateLayout();

        float scale = this->scale();

        IntSize viewportSize = view()->visibleSize();
        int scrollX = static_cast<int>(x / scale - viewportSize.width() / 2.0f);
        int scrollY = static_cast<int>(y / scale - viewportSize.height() / 2.0f);

        view()->setScrollPosition(IntPoint(scrollX, scrollY));
    }
}

void ImageEventListener::handleEvent(ScriptExecutionContext*, Event* event)
{
    if (event->type() == eventNames().resizeEvent)
        m_document.windowSizeChanged();
    else if (event->type() == eventNames().clickEvent && event->isMouseEvent()) {
        MouseEvent& mouseEvent = toMouseEvent(*event);
        m_document.imageClicked(mouseEvent.x(), mouseEvent.y());
    }
}

bool ImageEventListener::operator==(const EventListener& other)
{
    // All ImageEventListener objects compare as equal; OK since there is only one per document.
    return other.type() == ImageEventListenerType;
}
#endif

// --------

ImageDocumentElement::~ImageDocumentElement()
{
    if (m_imageDocument)
        m_imageDocument->disconnectImageElement();
}

void ImageDocumentElement::didMoveToNewDocument(Document* oldDocument)
{
    if (m_imageDocument) {
        m_imageDocument->disconnectImageElement();
        m_imageDocument = nullptr;
    }
    HTMLImageElement::didMoveToNewDocument(oldDocument);
}

}
