/*
 * Copyright (C) 2010, 2011, 2012 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

namespace WebCore {

class BlobRegistry;
class LoaderStrategy;
class PasteboardStrategy;

class PlatformStrategies {
public:
    LoaderStrategy* loaderStrategy()
    {
        if (!m_loaderStrategy)
            m_loaderStrategy = createLoaderStrategy();
        return m_loaderStrategy;
    }
    
    PasteboardStrategy* pasteboardStrategy()
    {
        if (!m_pasteboardStrategy)
            m_pasteboardStrategy = createPasteboardStrategy();
        return m_pasteboardStrategy;
    }

    BlobRegistry* blobRegistry()
    {
        if (!m_blobRegistry)
            m_blobRegistry = createBlobRegistry();
        return m_blobRegistry;
    }

protected:
    PlatformStrategies() = default;

    virtual ~PlatformStrategies()
    {
    }

private:
    virtual LoaderStrategy* createLoaderStrategy() = 0;
    virtual PasteboardStrategy* createPasteboardStrategy() = 0;
    virtual BlobRegistry* createBlobRegistry() = 0;

    LoaderStrategy* m_loaderStrategy { };
    PasteboardStrategy* m_pasteboardStrategy { };
    BlobRegistry* m_blobRegistry { };
};

WEBCORE_EXPORT PlatformStrategies* platformStrategies();
WEBCORE_EXPORT void setPlatformStrategies(PlatformStrategies*);
    
} // namespace WebCore
