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
#include "PageCache.h"

#include "PageState.h"
#include "SystemTime.h"

namespace WebCore {

PageCache::PageCache()
    : m_timeStamp(0)
{
}

void PageCache::setPageState(PassRefPtr<PageState> pageState)
{
    m_pageState = pageState;
}

PageCache::~PageCache()
{
    close();
}

PageState* PageCache::pageState()
{
    return m_pageState.get();
}

void PageCache::setDocumentLoader(PassRefPtr<DocumentLoader> loader)
{
    m_documentLoader = loader;
}

DocumentLoader* PageCache::documentLoader()
{
    return m_documentLoader.get();
}

void PageCache::setTimeStamp(double timeStamp)
{
    m_timeStamp = timeStamp;
}

void PageCache::setTimeStampToNow()
{
    m_timeStamp = currentTime();
}

double PageCache::timeStamp() const
{
    return m_timeStamp;
}

} // namespace WebCore
