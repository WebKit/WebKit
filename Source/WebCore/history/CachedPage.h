/*
 * Copyright (C) 2006, 2007, 2008, 2013 Apple Inc. All rights reserved.
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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#include "CachedFrame.h"
#include <wtf/MonotonicTime.h>

namespace WebCore {

class Document;
class DocumentLoader;
class Page;

class CachedPage {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit CachedPage(Page&);
    WEBCORE_EXPORT ~CachedPage();

    WEBCORE_EXPORT void restore(Page&);
    void clear();

    Page& page() const { return m_page; }
    Document* document() const { return m_cachedMainFrame->document(); }
    DocumentLoader* documentLoader() const { return m_cachedMainFrame->documentLoader(); }

    bool hasExpired() const;
    
    CachedFrame* cachedMainFrame() { return m_cachedMainFrame.get(); }

#if ENABLE(VIDEO)
    void markForCaptionPreferencesChanged() { m_needsCaptionPreferencesChanged = true; }
#endif

    void markForDeviceOrPageScaleChanged() { m_needsDeviceOrPageScaleChanged = true; }

    void markForContentsSizeChanged() { m_needsUpdateContentsSize = true; }

private:
    Page& m_page;
    MonotonicTime m_expirationTime;
    std::unique_ptr<CachedFrame> m_cachedMainFrame;
#if ENABLE(VIDEO)
    bool m_needsCaptionPreferencesChanged { false };
#endif
    bool m_needsDeviceOrPageScaleChanged { false };
    bool m_needsUpdateContentsSize { false };
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    Vector<RegistrableDomain> m_loadedSubresourceDomains;
#endif
};

} // namespace WebCore
