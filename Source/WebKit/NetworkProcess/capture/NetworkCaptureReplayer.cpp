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
#include "NetworkCaptureReplayer.h"

#if ENABLE(NETWORK_CAPTURE)

#include "NetworkCaptureLogging.h"
#include "NetworkCaptureManager.h"
#include "NetworkDataTaskReplay.h"
#include "NetworkLoadParameters.h"

#define DEBUG_CLASS Replayer

namespace WebKit {
namespace NetworkCapture {

Ref<NetworkDataTask> Replayer::replayResource(NetworkSession& session, NetworkDataTaskClient& client, const NetworkLoadParameters& parameters)
{
    auto foundResource = Manager::singleton().findMatch(parameters.request);
    Manager::singleton().logPlayedBackResource(parameters.request, !foundResource);
    return NetworkDataTaskReplay::create(session, client, parameters, foundResource);
}

} // namespace NetworkCapture
} // namespace WebKit

#endif // ENABLE(NETWORK_CAPTURE)
