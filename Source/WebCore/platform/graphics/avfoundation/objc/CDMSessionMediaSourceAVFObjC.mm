/*
 * Copyright (C) 2014-2015 Apple Inc. All rights reserved.
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

#import "config.h"
#import "CDMSessionMediaSourceAVFObjC.h"

#if ENABLE(LEGACY_ENCRYPTED_MEDIA) && ENABLE(MEDIA_SOURCE)

#import "CDMPrivateMediaSourceAVFObjC.h"
#import "WebCoreNSErrorExtras.h"
#import <AVFoundation/AVError.h>
#import <wtf/FileSystem.h>

namespace WebCore {

CDMSessionMediaSourceAVFObjC::CDMSessionMediaSourceAVFObjC(CDMPrivateMediaSourceAVFObjC& cdm, LegacyCDMSessionClient* client)
    : m_cdm(&cdm)
    , m_client(client)
{
}

CDMSessionMediaSourceAVFObjC::~CDMSessionMediaSourceAVFObjC()
{
    if (m_cdm)
        m_cdm->invalidateSession(this);

    for (auto& sourceBuffer : m_sourceBuffers)
        sourceBuffer->unregisterForErrorNotifications(this);
    m_sourceBuffers.clear();
}

void CDMSessionMediaSourceAVFObjC::layerDidReceiveError(AVSampleBufferDisplayLayer *, NSError *error, bool& shouldIgnore)
{
    if (!m_client)
        return;

    unsigned long code = mediaKeyErrorSystemCode(error);

    // FIXME(142246): Remove the following once <rdar://problem/20027434> is resolved.
    shouldIgnore = m_stopped && code == 12785;
    if (!shouldIgnore)
        m_client->sendError(LegacyCDMSessionClient::MediaKeyErrorDomain, code);
}

void CDMSessionMediaSourceAVFObjC::rendererDidReceiveError(AVSampleBufferAudioRenderer *, NSError *error, bool& shouldIgnore)
{
    if (!m_client)
        return;

    unsigned long code = mediaKeyErrorSystemCode(error);

    // FIXME(142246): Remove the following once <rdar://problem/20027434> is resolved.
    shouldIgnore = m_stopped && code == 12785;
    if (!shouldIgnore)
        m_client->sendError(LegacyCDMSessionClient::MediaKeyErrorDomain, code);
}


void CDMSessionMediaSourceAVFObjC::addSourceBuffer(SourceBufferPrivateAVFObjC* sourceBuffer)
{
    ASSERT(!m_sourceBuffers.contains(sourceBuffer));
    ASSERT(sourceBuffer);

    addParser(sourceBuffer->parser());

    m_sourceBuffers.append(sourceBuffer);
    sourceBuffer->registerForErrorNotifications(this);
}

void CDMSessionMediaSourceAVFObjC::removeSourceBuffer(SourceBufferPrivateAVFObjC* sourceBuffer)
{
    ASSERT(m_sourceBuffers.contains(sourceBuffer));
    ASSERT(sourceBuffer);

    removeParser(sourceBuffer->parser());

    sourceBuffer->unregisterForErrorNotifications(this);
    m_sourceBuffers.remove(m_sourceBuffers.find(sourceBuffer));
}

String CDMSessionMediaSourceAVFObjC::storagePath() const
{
    if (!m_client)
        return emptyString();

    String storageDirectory = m_client->mediaKeysStorageDirectory();
    if (storageDirectory.isEmpty())
        return emptyString();

    return FileSystem::pathByAppendingComponent(storageDirectory, "SecureStop.plist");
}

}

#endif // ENABLE(LEGACY_ENCRYPTED_MEDIA) && ENABLE(MEDIA_SOURCE)
