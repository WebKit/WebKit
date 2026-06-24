/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 *
 */

#pragma once

#include <JavaScriptCore/ArrayBuffer.h>
#include <WebCore/FileSystemStorageConnection.h>
#include <WebCore/NonSerializedDataToken.h>
#include <WebCore/URLKeepingBlobAlive.h>
#include <wtf/FastMalloc.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>

#if ENABLE(MEDIA_STREAM)
#include <WebCore/MediaStreamTrackDataHolder.h>
#include <WebCore/MediaStreamTrackHandle.h>
#endif
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
#include <WebCore/MediaSourceHandle.h>
#endif

#if ENABLE(WEB_CODECS)
#include <WebCore/WebCodecsAudioInternalData.h>
#include <WebCore/WebCodecsEncodedAudioChunk.h>
#include <WebCore/WebCodecsEncodedVideoChunk.h>
#include <WebCore/WebCodecsVideoFrame.h>
#endif

#if ENABLE(WEB_RTC)
#include <WebCore/DetachedRTCDataChannel.h>
#include <WebCore/RTCRtpTransformableFrame.h>
#endif

#if ENABLE(WEBASSEMBLY)
namespace JSC { namespace Wasm { class Module; } }
#endif

namespace WebCore {

class DetachedImageBitmap;
class MessagePort;
class OffscreenCanvas;

#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
class DetachedOffscreenCanvas;
#endif

#if ENABLE(WEBASSEMBLY)
using WasmModuleArray = Vector<Ref<::JSC::Wasm::Module>>;
using WasmMemoryHandleArray = Vector<RefPtr<::JSC::SharedArrayBufferContents>>;
#endif

using ArrayBufferContentsArray = Vector<::JSC::ArrayBufferContents>;

struct SerializedScriptValueInternals {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED_EXPORT(SerializedScriptValueInternals, WEBCORE_EXPORT);
    Vector<unsigned char> data { };
    std::unique_ptr<ArrayBufferContentsArray> arrayBufferContentsArray { nullptr };
#if ENABLE(WEB_RTC)
    Vector<std::unique_ptr<DetachedRTCDataChannel>> detachedRTCDataChannels { };
#endif
#if ENABLE(WEB_CODECS)
    Vector<Ref<WebCodecsEncodedVideoChunkStorage>> serializedVideoChunks { };
    Vector<Ref<WebCodecsEncodedAudioChunkStorage>> serializedAudioChunks { };
#endif
    uint64_t exposedMessagePortCount { 0 };
    std::optional<NonSerializedDataToken> nonSerializedDataToken { };
    Vector<FileSystemHandleKeepAlive> fileSystemHandleKeepAlives { };
#if ENABLE(WEB_CODECS)
    Vector<WebCodecsVideoFrameData> serializedVideoFrames { };
    Vector<WebCodecsAudioInternalData> serializedAudioData { };
#endif
#if ENABLE(WEB_RTC)
    Vector<Ref<RTCRtpTransformableFrame>> serializedRTCEncodedAudioFrames { };
    Vector<Ref<RTCRtpTransformableFrame>> serializedRTCEncodedVideoFrames { };
#endif
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
    Vector<RefPtr<DetachedMediaSourceHandle>> detachedMediaSourceHandles { };
#endif
#if ENABLE(MEDIA_STREAM)
    Vector<std::unique_ptr<MediaStreamTrackDataHolder>> detachedMediaStreamTracks { };
    Vector<std::unique_ptr<MediaStreamTrackHandleDataHolder>> detachedMediaStreamTrackHandles { };
#endif
    std::unique_ptr<ArrayBufferContentsArray> sharedBufferContentsArray { nullptr };
    Vector<std::optional<DetachedImageBitmap>> detachedImageBitmaps { };
#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS)
    Vector<std::unique_ptr<DetachedOffscreenCanvas>> detachedOffscreenCanvases { };
    Vector<Ref<OffscreenCanvas>> inMemoryOffscreenCanvases { };
#endif
    Vector<Ref<MessagePort>> inMemoryMessagePorts { };
#if ENABLE(WEBASSEMBLY)
    std::unique_ptr<WasmModuleArray> wasmModulesArray { };
    std::unique_ptr<WasmMemoryHandleArray> wasmMemoryHandlesArray { };
#endif
    Vector<URLKeepingBlobAlive> blobHandles { };
    uint64_t memoryCost { 0 };

    WEBCORE_EXPORT SerializedScriptValueInternals clone() const;
};

} // namespace WebCore
