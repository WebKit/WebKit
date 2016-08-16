/*
 * Copyright (C) 2015 Ericsson AB. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Ericsson nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MediaDevices.h"

#if ENABLE(MEDIA_STREAM)

#include "Dictionary.h"
#include "Document.h"
#include "MediaDevicesRequest.h"
#include "MediaStream.h"
#include "MediaTrackSupportedConstraints.h"
#include "RealtimeMediaSourceCenter.h"
#include "UserMediaController.h"
#include "UserMediaRequest.h"

namespace WebCore {

Ref<MediaDevices> MediaDevices::create(ScriptExecutionContext* context)
{
    return adoptRef(*new MediaDevices(context));
}

MediaDevices::MediaDevices(ScriptExecutionContext* context)
    : ContextDestructionObserver(context)
{
}

MediaDevices::~MediaDevices()
{
}

Document* MediaDevices::document() const
{
    return downcast<Document>(scriptExecutionContext());
}

void MediaDevices::getUserMedia(Ref<MediaConstraintsImpl>&& audioConstraints, Ref<MediaConstraintsImpl>&& videoConstraints, Promise&& promise, ExceptionCode& ec) const
{
    UserMediaRequest::start(document(), WTFMove(audioConstraints), WTFMove(videoConstraints), WTFMove(promise), ec);
}

void MediaDevices::enumerateDevices(EnumerateDevicesPromise&& promise, ExceptionCode& ec) const
{
    RefPtr<MediaDevicesRequest> request = MediaDevicesRequest::create(document(), WTFMove(promise), ec);
    if (request)
        request->start();
}

RefPtr<MediaTrackSupportedConstraints> MediaDevices::getSupportedConstraints()
{
    return MediaTrackSupportedConstraints::create(RealtimeMediaSourceCenter::singleton().supportedConstraints());
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
