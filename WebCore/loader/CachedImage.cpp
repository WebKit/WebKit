/*
    This file is part of the KDE libraries

    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.

    This class provides all functionality needed for loading images, style sheets and html
    pages from the web. It has a memory cache for these objects.
*/

#include "config.h"
#include "CachedImage.h"

#include "BitmapImage.h"
#include "Cache.h"
#include "CachedResourceClient.h"
#include "CachedResourceClientWalker.h"
#include "DocLoader.h"
#include "Request.h"
#include <wtf/Vector.h>

#if PLATFORM(CG)
#include "PDFDocumentImage.h"
#endif

#if ENABLE(SVG_EXPERIMENTAL_FEATURES)
#if PLATFORM(MAC) || PLATFORM(QT)
#include "SVGImage.h"
#endif
#endif

using std::max;

namespace WebCore {

CachedImage::CachedImage(DocLoader* docLoader, const String& url, CachePolicy cachePolicy, time_t _expireDate)
    : CachedResource(url, ImageResource, cachePolicy, _expireDate)
{
    m_image = 0;
    m_status = Unknown;
    if (!docLoader || docLoader->autoLoadImages())  {
        m_loading = true;
        cache()->loader()->load(docLoader, this, true);
    } else
        m_loading = false;
}

CachedImage::CachedImage(Image* image)
    : CachedResource(String(), ImageResource, CachePolicyCache, 0)
{
    m_image = image;
    m_status = Cached;
    m_loading = false;
}

CachedImage::~CachedImage()
{
    delete m_image;
}

void CachedImage::ref(CachedResourceClient* c)
{
    CachedResource::ref(c);

    if (!imageRect().isEmpty())
        c->imageChanged(this);

    if (!m_loading)
        c->notifyFinished(this);
}

void CachedImage::allReferencesRemoved()
{
    if (m_image && !m_errorOccurred)
        m_image->resetAnimation();
}

static Image* brokenImage()
{
    static Image* brokenImage;
    if (!brokenImage)
        brokenImage = Image::loadPlatformResource("missingImage");
    return brokenImage;
}

static Image* nullImage()
{
    static BitmapImage nullImage;
    return &nullImage;
}

Image* CachedImage::image() const
{
    if (m_errorOccurred)
        return brokenImage();

    if (m_image)
        return m_image;

    return nullImage();
}

IntSize CachedImage::imageSize() const
{
    return (m_image ? m_image->size() : IntSize());
}

IntRect CachedImage::imageRect() const
{
    return (m_image ? m_image->rect() : IntRect());
}

void CachedImage::notifyObservers()
{
    CachedResourceClientWalker w(m_clients);
    while (CachedResourceClient *c = w.next())
        c->imageChanged(this);
}

void CachedImage::clear()
{
    destroyDecodedData();
    delete m_image;
    m_image = 0;
    setEncodedSize(0);
}

inline void CachedImage::createImage()
{
    // Create the image if it doesn't yet exist.
    if (m_image)
        return;
#if PLATFORM(CG)
    if (m_response.mimeType() == "application/pdf") {
        m_image = new PDFDocumentImage;
        return;
    }
#endif
#if ENABLE(SVG_EXPERIMENTAL_FEATURES)
#if PLATFORM(MAC) || PLATFORM(QT)
    if (m_response.mimeType() == "image/svg+xml") {
        m_image = new SVGImage(this);
        return;
    }
#endif
#endif
    m_image = new BitmapImage(this);
}

Vector<char>& CachedImage::bufferData(const char* bytes, int addedSize, Request* request)
{
    createImage();

    Vector<char>& imageBuffer = m_image->dataBuffer();

    if (addedSize > 0) {
        bool success = false;
        unsigned oldSize = imageBuffer.size();
        unsigned newSize = oldSize + addedSize;

        // Check for overflow
        if (newSize > oldSize) {
            // Use temporary Vector so we can safely detect if the allocation fails
            //
            // The code that was here before, just called resize of the imageBuffer.  Vector<>::resize
            // will crash if the resize of a non-empty Vector<> fails.
            Vector<char> tempBuffer(newSize);

            char* tempBufferBytes = tempBuffer.data();
            if (tempBufferBytes) {
                memcpy(tempBufferBytes, imageBuffer.data(), oldSize);
                memcpy(tempBufferBytes + oldSize, bytes, addedSize);
                tempBuffer.swap(imageBuffer);
                success = true;
            }
        }

        if (!success)
            error();
    }

    return imageBuffer;
}

void CachedImage::data(Vector<char>& data, bool allDataReceived)
{
    createImage();

    bool sizeAvailable = false;

    // Have the image update its data from its internal buffer.
    // It will not do anything now, but will delay decoding until 
    // queried for info (like size or specific image frames).
    sizeAvailable = m_image->setData(allDataReceived);

    // Go ahead and tell our observers to try to draw if we have either
    // received all the data or the size is known.  Each chunk from the
    // network causes observers to repaint, which will force that chunk
    // to decode.
    if (sizeAvailable || allDataReceived) {
        if (m_image->isNull()) {
            m_errorOccurred = true;
            notifyObservers();
            if (inCache())
                cache()->remove(this);
        } else
            notifyObservers();

        if (m_image) {
            Vector<char>& imageBuffer = m_image->dataBuffer();
#if PLATFORM(CG)
            // ImageIO sources copy the image data.  We will go ahead and count encoded
            // size twice until this issue is fixed.  See <rdar://problem/5050645>
            setEncodedSize(imageBuffer.size() * 2);
#else
            setEncodedSize(imageBuffer.size());
#endif
        }
    }
    
    if (allDataReceived) {
        m_loading = false;
        checkNotify();
    }
}

void CachedImage::error()
{
    clear();
    m_errorOccurred = true;
    notifyObservers();
    m_loading = false;
    checkNotify();
}

void CachedImage::checkNotify()
{
    if (m_loading)
        return;

    CachedResourceClientWalker w(m_clients);
    while (CachedResourceClient* c = w.next())
        c->notifyFinished(this);
}

void CachedImage::destroyDecodedData()
{
    if (m_image && !m_errorOccurred)
        m_image->destroyDecodedData();
}

unsigned CachedImage::decodedSize() const
{
    if (m_image && !m_errorOccurred)
        return m_image->decodedSize();
    return 0;
}

void CachedImage::decodedSizeChanged(const Image* image, int delta)
{
    if (image != m_image)
        return;
    
    if (inCache())
        cache()->adjustSize(referenced(), delta);
}

bool CachedImage::shouldPauseAnimation(const Image* image)
{
    if (image != m_image)
        return false;
    
    CachedResourceClientWalker w(m_clients);
    while (CachedResourceClient* c = w.next()) {
        if (c->willRenderImage(this))
            return false;
    }

    return true;
}

void CachedImage::animationAdvanced(const Image* image)
{
    if (image == m_image)
        notifyObservers();
}

} //namespace WebCore
