/*
 *  Copyright (c) 2015, Canon Inc. All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1.  Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *  2.  Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *  3.  Neither the name of Canon Inc. nor the names of
 *      its contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *  THIS SOFTWARE IS PROVIDED BY CANON INC. AND ITS CONTRIBUTORS "AS IS" AND ANY
 *  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL CANON INC. AND ITS CONTRIBUTORS BE LIABLE FOR
 *  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 *  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 *  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebCoreBuiltinNames_h
#define WebCoreBuiltinNames_h

#include <builtins/BuiltinUtils.h>

namespace WebCore {

#define WEBCORE_COMMON_PRIVATE_IDENTIFIERS_EACH_PROPERTY_NAME(macro)\
    macro(appendFromJS) \
    macro(closeRequested) \
    macro(closedPromiseCapability) \
    macro(controlledReadableStream) \
    macro(controller) \
    macro(disturbed) \
    macro(fillFromJS) \
    macro(getUserMediaFromJS) \
    macro(initializeWith) \
    macro(operations) \
    macro(ownerReadableStream) \
    macro(privateGetStats) \
    macro(pulling) \
    macro(pullAgain) \
    macro(queue) \
    macro(queuedAddIceCandidate) \
    macro(queuedCreateAnswer) \
    macro(queuedCreateOffer) \
    macro(queuedSetLocalDescription) \
    macro(queuedSetRemoteDescription) \
    macro(reader) \
    macro(readRequests) \
    macro(readyPromiseCapability) \
    macro(state) \
    macro(started) \
    macro(startedPromise) \
    macro(storedError) \
    macro(strategy) \
    macro(streamClosed) \
    macro(streamClosing) \
    macro(streamErrored) \
    macro(streamReadable) \
    macro(streamWaiting) \
    macro(streamWritable) \
    macro(underlyingSink) \
    macro(underlyingSource) \
    macro(writing) \
    macro(MediaStreamTrack) \
    macro(ReadableStream) \
    macro(ReadableStreamReader) \
    macro(ReadableStreamController) \
    macro(RTCIceCandidate) \
    macro(RTCSessionDescription) \

class WebCoreBuiltinNames {
public:
    explicit WebCoreBuiltinNames(JSC::VM* vm)
        : m_vm(*vm)
        WEBCORE_COMMON_PRIVATE_IDENTIFIERS_EACH_PROPERTY_NAME(INITIALIZE_BUILTIN_NAMES)
    {
#define EXPORT_NAME(name) m_vm.propertyNames->appendExternalName(name##PublicName(), name##PrivateName());
        WEBCORE_COMMON_PRIVATE_IDENTIFIERS_EACH_PROPERTY_NAME(EXPORT_NAME)
#undef EXPORT_NAME
    }

    WEBCORE_COMMON_PRIVATE_IDENTIFIERS_EACH_PROPERTY_NAME(DECLARE_BUILTIN_IDENTIFIER_ACCESSOR)

private:
    JSC::VM& m_vm;
    WEBCORE_COMMON_PRIVATE_IDENTIFIERS_EACH_PROPERTY_NAME(DECLARE_BUILTIN_NAMES)
};

} // namespace WebCore

#endif // WebCoreBuiltinNames_h
