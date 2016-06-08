/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#ifndef AVFoundationMIMETypeCache_h
#define AVFoundationMIMETypeCache_h

#if ENABLE(VIDEO) && USE(AVFOUNDATION)

#include <wtf/Forward.h>
#include <wtf/HashSet.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

class AVFoundationMIMETypeCache {
public:
    static AVFoundationMIMETypeCache& singleton();

    AVFoundationMIMETypeCache();

    void loadTypes();
    const HashSet<String, ASCIICaseInsensitiveHash>& types();

private:
    enum MIMETypeLoadStatus { NotLoaded, Loading, Loaded };

    MIMETypeLoadStatus m_status { NotLoaded };
    dispatch_queue_t m_loaderQueue;
    Lock m_lock;
    HashSet<String, ASCIICaseInsensitiveHash> m_cache;
};

}
#endif
#endif
