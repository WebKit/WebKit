/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MediaStreamTrackSourcesRequest.h"

#if ENABLE(MEDIA_STREAM)

#include "MediaStreamTrackSourcesCallback.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"
#include "SourceInfo.h"
#include <wtf/Functional.h>
#include <wtf/MainThread.h>

namespace WebCore {

PassRefPtr<MediaStreamTrackSourcesRequest> MediaStreamTrackSourcesRequest::create(ScriptExecutionContext* context, PassRefPtr<MediaStreamTrackSourcesCallback> callback)
{
    return adoptRef(new MediaStreamTrackSourcesRequest(context, callback));
}

MediaStreamTrackSourcesRequest::MediaStreamTrackSourcesRequest(ScriptExecutionContext* context, PassRefPtr<MediaStreamTrackSourcesCallback> callback)
    : m_callback(callback)
{
    m_origin = context->securityOrigin()->toString();
}

void MediaStreamTrackSourcesRequest::didCompleteRequest(const Vector<RefPtr<TrackSourceInfo>>& requestSourceInfos)
{
    ASSERT(m_callback);

    for (size_t i = 0; i < requestSourceInfos.size(); ++i)
        m_sourceInfos.append(SourceInfo::create(requestSourceInfos[i]));

    callOnMainThread(bind(&MediaStreamTrackSourcesRequest::callCompletionHandler, this));
}

void MediaStreamTrackSourcesRequest::callCompletionHandler()
{
    ASSERT(m_callback);

    m_callback->handleEvent(m_sourceInfos);
    m_callback = nullptr;
}

} // namespace WebCore

#endif
