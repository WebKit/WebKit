/*
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
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

#pragma once

#include "CallTracerTypes.h"
#include "CanvasRenderingContext.h"
#include <JavaScriptCore/InspectorProtocolObjects.h>
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/ScriptCallFrame.h>
#include <JavaScriptCore/ScriptCallStack.h>
#include <initializer_list>
#include <wtf/HashSet.h>
#include <wtf/Variant.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

#if ENABLE(WEBGPU)
#include "WebGPUDevice.h"
#endif

namespace WebCore {

class CanvasGradient;
class CanvasPattern;
class Element;
class HTMLCanvasElement;
class HTMLImageElement;
class HTMLVideoElement;
class ImageBitmap;
class ImageData;
#if ENABLE(OFFSCREEN_CANVAS)
class OffscreenCanvas;
#endif
#if ENABLE(CSS_TYPED_OM)
class TypedOMCSSImageValue;
#endif

typedef String ErrorString;

class InspectorCanvas final : public RefCounted<InspectorCanvas> {
public:
    static Ref<InspectorCanvas> create(CanvasRenderingContext&);
#if ENABLE(WEBGPU)
    static Ref<InspectorCanvas> create(WebGPUDevice&);
#endif

    const String& identifier() const { return m_identifier; }

    CanvasRenderingContext* canvasContext() const;
    HTMLCanvasElement* canvasElement() const;

#if ENABLE(WEBGPU)
    WebGPUDevice* deviceContext() const;
    bool isDeviceForCanvasContext(CanvasRenderingContext&) const;
#endif

    ScriptExecutionContext* scriptExecutionContext() const;

    JSC::JSValue resolveContext(JSC::JSGlobalObject*) const;

    HashSet<Element*> clientNodes() const;

    void canvasChanged();

    void resetRecordingData();
    bool hasRecordingData() const;
    bool currentFrameHasData() const;
    void recordAction(const String&, std::initializer_list<RecordCanvasActionVariant>&& = { });

    Ref<JSON::ArrayOf<Inspector::Protocol::Recording::Frame>> releaseFrames() { return m_frames.releaseNonNull(); }

    void finalizeFrame();
    void markCurrentFrameIncomplete();

    void setRecordingName(const String& name) { m_recordingName = name; }

    void setBufferLimit(long);
    bool hasBufferSpace() const;
    long bufferUsed() const { return m_bufferUsed; }

    void setFrameCount(long);
    bool overFrameCount() const;

    Ref<Inspector::Protocol::Canvas::Canvas> buildObjectForCanvas(bool captureBacktrace);
    Ref<Inspector::Protocol::Recording::Recording> releaseObjectForRecording();

    String getCanvasContentAsDataURL(ErrorString&);

private:
    InspectorCanvas(CanvasRenderingContext&);
#if ENABLE(WEBGPU)
    InspectorCanvas(WebGPUDevice&);
#endif

    void appendActionSnapshotIfNeeded();

    using DuplicateDataVariant = Variant<
        RefPtr<CanvasGradient>,
        RefPtr<CanvasPattern>,
        RefPtr<HTMLCanvasElement>,
        RefPtr<HTMLImageElement>,
#if ENABLE(VIDEO)
        RefPtr<HTMLVideoElement>,
#endif
        RefPtr<ImageData>,
        RefPtr<ImageBitmap>,
        RefPtr<Inspector::ScriptCallStack>,
#if ENABLE(CSS_TYPED_OM)
        RefPtr<TypedOMCSSImageValue>,
#endif
        Inspector::ScriptCallFrame,
#if ENABLE(OFFSCREEN_CANVAS)
        RefPtr<OffscreenCanvas>,
#endif
        String
    >;

    int indexForData(DuplicateDataVariant);
    String stringIndexForKey(const String&);
    Ref<Inspector::Protocol::Recording::InitialState> buildInitialState();
    Ref<JSON::ArrayOf<JSON::Value>> buildAction(const String&, std::initializer_list<RecordCanvasActionVariant>&& = { });
    Ref<JSON::ArrayOf<JSON::Value>> buildArrayForCanvasGradient(const CanvasGradient&);
    Ref<JSON::ArrayOf<JSON::Value>> buildArrayForCanvasPattern(const CanvasPattern&);
    Ref<JSON::ArrayOf<JSON::Value>> buildArrayForImageData(const ImageData&);

    String m_identifier;

    Variant<
        std::reference_wrapper<CanvasRenderingContext>,
#if ENABLE(WEBGPU)
        std::reference_wrapper<WebGPUDevice>,
#endif
        Monostate
    > m_context;

    RefPtr<Inspector::Protocol::Recording::InitialState> m_initialState;
    RefPtr<JSON::ArrayOf<Inspector::Protocol::Recording::Frame>> m_frames;
    RefPtr<JSON::ArrayOf<JSON::Value>> m_currentActions;
    RefPtr<JSON::ArrayOf<JSON::Value>> m_lastRecordedAction;
    RefPtr<JSON::ArrayOf<JSON::Value>> m_serializedDuplicateData;
    Vector<DuplicateDataVariant> m_indexedDuplicateData;

    String m_recordingName;
    MonotonicTime m_currentFrameStartTime { MonotonicTime::nan() };
    size_t m_bufferLimit { 100 * 1024 * 1024 };
    size_t m_bufferUsed { 0 };
    Optional<size_t> m_frameCount;
    size_t m_framesCaptured { 0 };
    bool m_contentChanged { false };
};

} // namespace WebCore
