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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "CachedFont.h"

#include "Cache.h"
#include "CachedResourceClient.h"
#include "CachedResourceClientWalker.h"
#include "FontPlatformData.h"
#if PLATFORM(CG)
#include "FontCustomPlatformData.h"
#endif
#include "TextResourceDecoder.h"
#include "loader.h"
#include <wtf/Vector.h>

namespace WebCore {

CachedFont::CachedFont(DocLoader* dl, const String &url)
    : CachedResource(url, FontResource), m_fontData(0)
{
    // Don't load the file yet.  Wait for an access before triggering the load.
    m_loading = true;
    m_loadInitiated = false;
}

CachedFont::~CachedFont()
{
    delete m_fontData;
}

void CachedFont::ref(CachedResourceClient *c)
{
    CachedResource::ref(c);
    
    if (!m_loading)
        c->fontLoaded(this);
}

void CachedFont::data(PassRefPtr<SharedBuffer> data, bool allDataReceived)
{
    if (!allDataReceived)
        return;

    m_data = data;     
    setEncodedSize(m_data.get() ? m_data->size() : 0);
    m_loading = false;
    checkNotify();
}

void CachedFont::beginLoadIfNeeded(DocLoader* dl)
{
    if (!m_loadInitiated) {
        m_loadInitiated = true;
        cache()->loader()->load(dl, this, false);
    }
}

bool CachedFont::ensureCustomFontData()
{
#if PLATFORM(CG)
    if (!m_fontData && !m_errorOccurred && !m_loading) {
        m_fontData = createFontCustomPlatformData(m_data.get());
        if (!m_fontData)
            m_errorOccurred = true;
    }
#endif
    return m_fontData;
}

FontPlatformData CachedFont::platformDataFromCustomData(int size, bool bold, bool italic)
{
#if PLATFORM(CG)
    ASSERT(m_fontData);
    return m_fontData->fontPlatformData(size, bold, italic);
#else
    return FontPlatformData();
#endif
}

void CachedFont::allReferencesRemoved()
{
    if (m_fontData) {
        delete m_fontData;
        m_fontData = 0;
    }
}

void CachedFont::checkNotify()
{
    if (m_loading)
        return;
    
    CachedResourceClientWalker w(m_clients);
    while (CachedResourceClient *c = w.next())
         c->fontLoaded(this);
}


void CachedFont::error()
{
    m_loading = false;
    m_errorOccurred = true;
    checkNotify();
}

}
