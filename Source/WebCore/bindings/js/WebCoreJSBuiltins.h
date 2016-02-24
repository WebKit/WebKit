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

#ifndef WebCoreJSBuiltins_h
#define WebCoreJSBuiltins_h

#include "ByteLengthQueuingStrategyBuiltins.h"
#include "CountQueuingStrategyBuiltins.h"
#include "FetchHeadersBuiltins.h"
#include "FetchResponseBuiltins.h"
#include "MediaDevicesBuiltins.h"
#include "NavigatorUserMediaBuiltins.h"
#include "RTCPeerConnectionBuiltins.h"
#include "RTCPeerConnectionInternalsBuiltins.h"
#include "ReadableStreamBuiltins.h"
#include "ReadableStreamControllerBuiltins.h"
#include "ReadableStreamInternalsBuiltins.h"
#include "ReadableStreamReaderBuiltins.h"
#include "StreamInternalsBuiltins.h"
#include "WritableStreamBuiltins.h"
#include "WritableStreamInternalsBuiltins.h"
#include <runtime/VM.h>

namespace WebCore {

class JSBuiltinFunctions {
public:
    explicit JSBuiltinFunctions(JSC::VM& v)
        : vm(v)
#if ENABLE(FETCH_API)
        , m_fetchHeadersBuiltins(&vm)
        , m_fetchResponseBuiltins(&vm)
#endif
#if ENABLE(STREAMS_API)
        , m_byteLengthQueuingStrategyBuiltins(&vm)
        , m_countQueuingStrategyBuiltins(&vm)
        , m_readableStreamBuiltins(&vm)
        , m_readableStreamControllerBuiltins(&vm)
        , m_readableStreamInternalsBuiltins(&vm)
        , m_readableStreamReaderBuiltins(&vm)
        , m_streamInternalsBuiltins(&vm)
        , m_writableStreamBuiltins(&vm)
        , m_writableStreamInternalsBuiltins(&vm)
#endif
#if ENABLE(MEDIA_STREAM)
        , m_mediaDevicesBuiltins(&vm)
        , m_navigatorUserMediaBuiltins(&vm)
        , m_rtcPeerConnectionBuiltins(&vm)
        , m_rtcPeerConnectionInternalsBuiltins(&vm)
#endif
    {
#if ENABLE(STREAMS_API)
        m_readableStreamInternalsBuiltins.exportNames();
        m_streamInternalsBuiltins.exportNames();
        m_writableStreamInternalsBuiltins.exportNames();
#endif
#if ENABLE(MEDIA_STREAM)
        m_rtcPeerConnectionInternalsBuiltins.exportNames();
#endif
    }
#if ENABLE(FETCH_API)
    FetchHeadersBuiltinsWrapper& fetchHeadersBuiltins() { return m_fetchHeadersBuiltins; }
    FetchResponseBuiltinsWrapper& fetchResponseBuiltins() { return m_fetchResponseBuiltins; }
#endif
#if ENABLE(STREAMS_API)
    ByteLengthQueuingStrategyBuiltinsWrapper& byteLengthQueuingStrategyBuiltins() { return m_byteLengthQueuingStrategyBuiltins; }
    CountQueuingStrategyBuiltinsWrapper& countQueuingStrategyBuiltins() { return m_countQueuingStrategyBuiltins; }
    ReadableStreamBuiltinsWrapper& readableStreamBuiltins() { return m_readableStreamBuiltins; }
    ReadableStreamControllerBuiltinsWrapper& readableStreamControllerBuiltins() { return m_readableStreamControllerBuiltins; }
    ReadableStreamInternalsBuiltinsWrapper& readableStreamInternalsBuiltins() { return m_readableStreamInternalsBuiltins; }
    ReadableStreamReaderBuiltinsWrapper& readableStreamReaderBuiltins() { return m_readableStreamReaderBuiltins; }
    StreamInternalsBuiltinsWrapper& streamInternalsBuiltins() { return m_streamInternalsBuiltins; }
    WritableStreamBuiltinsWrapper& writableStreamBuiltins() { return m_writableStreamBuiltins; }
    WritableStreamInternalsBuiltinsWrapper& writableStreamInternalsBuiltins() { return m_writableStreamInternalsBuiltins;}
#endif
#if ENABLE(MEDIA_STREAM)
    MediaDevicesBuiltinsWrapper& mediaDevicesBuiltins() { return m_mediaDevicesBuiltins; }
    NavigatorUserMediaBuiltinsWrapper& navigatorUserMediaBuiltins() { return m_navigatorUserMediaBuiltins;}
    RTCPeerConnectionBuiltinsWrapper& rtcPeerConnectionBuiltins() { return m_rtcPeerConnectionBuiltins; }
    RTCPeerConnectionInternalsBuiltinsWrapper& rtcPeerConnectionInternalsBuiltins() { return m_rtcPeerConnectionInternalsBuiltins; }
#endif

private:
    JSC::VM& vm;
#if ENABLE(FETCH_API)
    FetchHeadersBuiltinsWrapper m_fetchHeadersBuiltins;
    FetchResponseBuiltinsWrapper m_fetchResponseBuiltins;
#endif
#if ENABLE(STREAMS_API)
    ByteLengthQueuingStrategyBuiltinsWrapper m_byteLengthQueuingStrategyBuiltins;
    CountQueuingStrategyBuiltinsWrapper m_countQueuingStrategyBuiltins;
    ReadableStreamBuiltinsWrapper m_readableStreamBuiltins;
    ReadableStreamControllerBuiltinsWrapper m_readableStreamControllerBuiltins;
    ReadableStreamInternalsBuiltinsWrapper m_readableStreamInternalsBuiltins;
    ReadableStreamReaderBuiltinsWrapper m_readableStreamReaderBuiltins;
    StreamInternalsBuiltinsWrapper m_streamInternalsBuiltins;
    WritableStreamBuiltinsWrapper m_writableStreamBuiltins;
    WritableStreamInternalsBuiltinsWrapper m_writableStreamInternalsBuiltins;
#endif
#if ENABLE(MEDIA_STREAM)
    MediaDevicesBuiltinsWrapper m_mediaDevicesBuiltins;
    NavigatorUserMediaBuiltinsWrapper m_navigatorUserMediaBuiltins;
    RTCPeerConnectionBuiltinsWrapper m_rtcPeerConnectionBuiltins;
    RTCPeerConnectionInternalsBuiltinsWrapper m_rtcPeerConnectionInternalsBuiltins;
#endif

};

}

#endif
