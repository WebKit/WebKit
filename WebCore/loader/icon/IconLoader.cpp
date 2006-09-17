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
 
#include "config.h"
#include "IconLoader.h"

#include "dom/Document.h"
#include "loader/icon/IconDatabase.h"
#include "Logging.h"
#include "page/Frame.h"

#if PLATFORM(MAC)
#include "FrameMac.h"
#endif

using namespace WebCore;

IconLoader::IconLoader(Frame* frame)
    : m_resourceLoader(0)
    , m_frame(frame)
    , m_httpStatusCode(0)
{
}

IconLoader* IconLoader::createForFrame(Frame* frame)
{
    if (frame)
        return new IconLoader(frame);
    return 0;
}

IconLoader::~IconLoader()
{
    delete m_resourceLoader;
}

void IconLoader::startLoading()
{    
    if (m_resourceLoader)
        return;
    
    m_httpStatusCode = 0;
    m_resourceLoader = new ResourceLoader(this, "GET", m_frame->iconURL());
    
    // A frame may be documentless - one example is viewing a PDF directly
    if (!m_frame->document()) {
        // FIXME - http://bugzilla.opendarwin.org/show_bug.cgi?id=10902
        // Once the loader infrastructure will cleanly let us load an icon without a DocLoader, we can implement this
        LOG(IconDatabase, "Documentless-frame - icon won't be loaded");
    } else if (!m_resourceLoader->start(m_frame->document()->docLoader())) {
        LOG_ERROR("Failed to start load for icon at url %s", m_frame->iconURL().url().ascii());
        m_resourceLoader = 0;
    }
}

void IconLoader::stopLoading()
{
    delete m_resourceLoader;
    m_resourceLoader = 0;
    m_data.clear();
}

void IconLoader::receivedData(ResourceLoader* resourceLoader, const char* data, int size)
{
    ASSERT(resourceLoader = m_resourceLoader);
    ASSERT(data);
    ASSERT(size > -1);
    
    for (int i=0; i<size; ++i)
        m_data.append(data[i]);
}

void IconLoader::receivedAllData(ResourceLoader* resourceLoader)
{
    ASSERT(resourceLoader == m_resourceLoader);

    const char* data;
    int size;
    
    // If we logged an HTTP response, only set the icon data if it was a valid response
    if (m_httpStatusCode && (m_httpStatusCode < 200 || m_httpStatusCode > 299)) {
        data = 0;
        size = 0;
    } else {
        data = m_data.data();
        size = m_data.size();
    }
        
    IconDatabase * iconDatabase = IconDatabase::sharedIconDatabase();
    ASSERT(iconDatabase);
    
    KURL iconURL(resourceLoader->url());
    
    if (data)
        iconDatabase->setIconDataForIconURL(data, size, iconURL.url());
    else
        iconDatabase->setHaveNoIconForIconURL(iconURL.url());
        
    // We set both the original request URL and the final URL as the PageURLs as different parts
    // of the app tend to want to retain both
    iconDatabase->setIconURLForPageURL(iconURL.url(), m_frame->url().url());

    // FIXME - Need to be able to do the following platform independently
#if PLATFORM(MAC)
    FrameMac* frameMac = Mac(m_frame);
    iconDatabase->setIconURLForPageURL(iconURL.url(), frameMac->originalRequestURL().url());
#endif

    notifyIconChanged(iconURL);

    // ResourceLoaders delete themselves after they deliver their last data, so we can just forget about it
    m_resourceLoader = 0;
    m_data.clear();
}

