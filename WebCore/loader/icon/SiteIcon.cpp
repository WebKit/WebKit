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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */
#include "IconDatabase.h"

#include "Logging.h"
#include "Image.h"
#include <limits.h>

using namespace WebCore;

SiteIcon::SiteIcon(const String& url)
    : m_iconURL(url)
    , m_image(0)
    , m_dataQueried(false)
{

}

SiteIcon::~SiteIcon()
{
    // Upon destruction of a SiteIcon, its image should no longer be in use anywhere
    delete m_image;
}

Image* SiteIcon::getImage(const IntSize& size)
{
    // FIXME - For size right now, we are resizing our one-and-only shared Image
    // What we really need to do is keep a hashmap of Images based on size and make a copy when/if we create a new one
    if (m_image)
        return m_image;
    if (m_dataQueried)
        return 0;
    
    if (IconDatabase::m_sharedInstance) {
        Vector<unsigned char> imageData = IconDatabase::m_sharedInstance->imageDataForIconURL(m_iconURL);
        LOG(IconDatabase, "For URL %s, we got a buffer of size %i", m_iconURL.ascii().data(), imageData.size());
        
        m_dataQueried = true;
        if (!imageData.size())
            return 0;
        manuallySetImageData(imageData.data(), imageData.size());
        
        return m_image;
    }
    return 0;
}

void SiteIcon::manuallySetImageData(unsigned char* data, int size)
{
    if (!data)
        ASSERT(!size);
        
    NativeBytePtr nativeData = 0;
    // FIXME - Any other platform will need their own method to create NativeBytePtr from the void*
#ifdef __APPLE__
    nativeData = CFDataCreate(NULL, data, size);
#endif

    // It's okay to delete the raw image data here. Any existing clients using this icon will be
    // managing an image that was created with a copy of this raw image data.
    // FIXME: The IconDatabase interface would be more robust if it were cleaned up to never
    // expose this raw image data.
    if (m_image)
        delete m_image;
    m_image = new Image();

    if (!m_image->setNativeData(nativeData, true)) {
        LOG(IconDatabase, "Manual image data for iconURL '%s' FAILED", m_iconURL.ascii().data());
        delete m_image;
        m_image = 0;
    }
}

void SiteIcon::resetExpiration(time_t newExpiration)
{
    // FIXME - Write expiration time to SQL
}

time_t SiteIcon::getExpiration()
{
    // FIXME - Return expiration time from SQL
    return INT_MAX;
}
    

