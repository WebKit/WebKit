/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
#include "Element.h"
#include "HTMLNames.h"
#include "SegmentedString.h"
#include "Text.h"
#include "HTMLImageElement.h"
#include "xml_tokenizer.h"

#ifdef __APPLE__
#include <Cocoa/Cocoa.h>
#include "FrameMac.h"
#include "WebCoreFrameBridge.h"
#endif 

namespace WebCore {
    
using namespace HTMLNames;
    
class ImageTokenizer : public Tokenizer {
public:
    ImageTokenizer(Document* doc) : m_doc(doc), m_imageElement(0) {}
        
    virtual bool write(const SegmentedString&, bool appendData);
    virtual void stopParsing();
    virtual void finish();
    virtual bool isWaitingForScripts() const;
    
    virtual bool wantsRawData() const { return true; }
    virtual bool writeRawData(const char *data, int len);

    void createDocumentStructure();
private:
    Document* m_doc;
    HTMLImageElement* m_imageElement;
    DeprecatedByteArray m_buffer;
};
    
bool ImageTokenizer::write(const SegmentedString& s, bool appendData)
{
    ASSERT_NOT_REACHED();
    return false;
}

void ImageTokenizer::createDocumentStructure()
{
    ExceptionCode ec;
    
    RefPtr<Element> rootElement = m_doc->createElementNS(xhtmlNamespaceURI, "html", ec);
    m_doc->appendChild(rootElement, ec);
    
    RefPtr<Element> body = m_doc->createElementNS(xhtmlNamespaceURI, "body", ec);
    body->setAttribute(styleAttr, "margin: 0px;");
    
    rootElement->appendChild(body, ec);
    
    RefPtr<Element> imageElement = m_doc->createElementNS(xhtmlNamespaceURI, "img", ec);
    
    m_imageElement = static_cast<HTMLImageElement *>(imageElement.get());
    m_imageElement->setAttribute(styleAttr, "-webkit-user-select: none");        
    m_imageElement->setLoadManually(true);
    m_imageElement->setSrc(m_doc->URL());
    
    body->appendChild(imageElement, ec);    
}

bool ImageTokenizer::writeRawData(const char *data, int len)
{
    if (!m_imageElement)
        createDocumentStructure();

    unsigned oldSize = m_buffer.size();
    m_buffer.resize(oldSize + len);
    memcpy(m_buffer.data() + oldSize, data, len);

    m_imageElement->cachedImage()->data(m_buffer, false);

    return false;
}

void ImageTokenizer::stopParsing()
{
    Tokenizer::stopParsing();
   
    m_imageElement->cachedImage()->error();
}

void ImageTokenizer::finish()
{
    if (!m_parserStopped) {
        m_imageElement->cachedImage()->data(m_buffer, true);

        // FIXME: For platforms other than Mac OS X, the title needs to be set in this function
        
#ifdef __APPLE__
        WebCoreFrameBridge *bridge = Mac(m_doc->frame())->bridge();
        NSURLResponse *response = [bridge mainResourceURLResponse];
        NSData *data = [NSData dataWithBytes:m_buffer.data() length:m_buffer.size()];
        m_imageElement->cachedImage()->setAllData([data retain]);
        m_imageElement->cachedImage()->setResponse(response);

        if (m_imageElement->cachedImage()->imageSize().width() > 0) {
            NSSize size = NSMakeSize(m_imageElement->cachedImage()->imageSize().width(), 
                                     m_imageElement->cachedImage()->imageSize().height());
            m_doc->setTitle([bridge imageTitleForFilename:[response suggestedFilename]
                                                     size:size]);
        }
#endif
    }

    m_doc->finishedParsing();    
    
}
    
bool ImageTokenizer::isWaitingForScripts() const
{
    // An image document is never waiting for scripts
    return false;
}
    
ImageDocument::ImageDocument(DOMImplementation *_implementation, FrameView *v)
    : HTMLDocument(_implementation, v)
{
}
    
Tokenizer* ImageDocument::createTokenizer()
{
    return new ImageTokenizer(this);
}
    
}
