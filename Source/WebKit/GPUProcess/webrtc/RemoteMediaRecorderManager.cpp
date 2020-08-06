/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include "config.h"
#include "RemoteMediaRecorderManager.h"

#if PLATFORM(COCOA) && ENABLE(GPU_PROCESS) && ENABLE(MEDIA_STREAM) && HAVE(AVASSETWRITERDELEGATE)

#include "DataReference.h"
#include "Decoder.h"
#include "RemoteMediaRecorder.h"
#include <WebCore/ExceptionData.h>

namespace WebKit {
using namespace WebCore;

RemoteMediaRecorderManager::RemoteMediaRecorderManager(GPUConnectionToWebProcess& gpuConnectionToWebProcess)
    : m_gpuConnectionToWebProcess(gpuConnectionToWebProcess)
{
}

RemoteMediaRecorderManager::~RemoteMediaRecorderManager()
{
}

void RemoteMediaRecorderManager::didReceiveRemoteMediaRecorderMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    if (auto* recorder = m_recorders.get(makeObjectIdentifier<MediaRecorderIdentifierType>(decoder.destinationID())))
        recorder->didReceiveMessage(connection, decoder);
}

void RemoteMediaRecorderManager::createRecorder(MediaRecorderIdentifier identifier, bool recordAudio, bool recordVideo, const MediaRecorderPrivateOptions& options, CompletionHandler<void(Optional<ExceptionData>&&)>&& completionHandler)
{
    ASSERT(!m_recorders.contains(identifier));
    auto recorder = RemoteMediaRecorder::create(m_gpuConnectionToWebProcess, identifier, recordAudio, recordVideo, options);
    if (!recorder)
        return completionHandler(ExceptionData { NotSupportedError, "Unable to create a recorder with the provided stream"_s });

    m_recorders.add(identifier, WTFMove(recorder));
    completionHandler({ });
}

void RemoteMediaRecorderManager::releaseRecorder(MediaRecorderIdentifier identifier)
{
    m_recorders.remove(identifier);
}

}

#endif
