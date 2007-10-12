/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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

#ifndef CachedFont_h
#define CachedFont_h

#include "CachedResource.h"
#include <wtf/Vector.h>

namespace WebCore {

class DocLoader;
class Cache;
class FontCustomPlatformData;
class FontPlatformData;

class CachedFont : public CachedResource {
public:
    CachedFont(DocLoader*, const String& url);
    virtual ~CachedFont();

    virtual void ref(CachedResourceClient*);
    virtual void data(PassRefPtr<SharedBuffer> data, bool allDataReceived);
    virtual void error();

    virtual void allReferencesRemoved();

    virtual bool schedule() const { return true; }

    void checkNotify();

    void beginLoadIfNeeded(DocLoader* dl);
    bool ensureCustomFontData();

    FontPlatformData platformDataFromCustomData(int size, bool bold, bool italic);

private:
    FontCustomPlatformData* m_fontData;
    bool m_loadInitiated;

    friend class Cache;
};

}

#endif
