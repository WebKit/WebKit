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

#import "config.h"
#import "AVFoundationMIMETypeCache.h"

#if ENABLE(VIDEO) && USE(AVFOUNDATION)

#import <AVFoundation/AVAsset.h>
#import <wtf/HashSet.h>
#import <wtf/Locker.h>
#import <wtf/NeverDestroyed.h>

#import <pal/cf/CoreMediaSoftLink.h>

SOFT_LINK_FRAMEWORK_OPTIONAL(AVFoundation)
SOFT_LINK_CLASS(AVFoundation, AVURLAsset)

namespace WebCore {

using namespace PAL;

AVFoundationMIMETypeCache::AVFoundationMIMETypeCache()
{
    m_loaderQueue = dispatch_queue_create("WebCoreAudioVisualMIMETypes loader queue", DISPATCH_QUEUE_SERIAL);
}

void AVFoundationMIMETypeCache::loadTypes()
{
    {
        Locker<Lock> lock(m_lock);

        if (m_status != NotLoaded)
            return;

        if (!AVFoundationLibrary()) {
            m_status = Loaded;
            return;
        }

        m_status = Loading;
    }

    dispatch_async(m_loaderQueue, [this] {
        for (NSString *type in [getAVURLAssetClass() audiovisualMIMETypes])
            m_cache.add(type);
        m_lock.lock();
        m_status = Loaded;
        m_lock.unlock();
    });
}

const HashSet<String, ASCIICaseInsensitiveHash>& AVFoundationMIMETypeCache::types()
{
    m_lock.lock();
    MIMETypeLoadStatus status = m_status;
    m_lock.unlock();

    if (status != Loaded) {
        loadTypes();
        dispatch_sync(m_loaderQueue, ^{ });
    }

    return m_cache;
}

AVFoundationMIMETypeCache& AVFoundationMIMETypeCache::singleton()
{
    static NeverDestroyed<AVFoundationMIMETypeCache> cache;
    return cache.get();
}

}

#endif
