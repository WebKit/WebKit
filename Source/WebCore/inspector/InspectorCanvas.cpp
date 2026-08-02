/*
 * Copyright (C) 2017-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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

#include "config.h"
#include "InspectorCanvas.h"

#include "AffineTransform.h"
#include "CSSStyleImageValue.h"
#include "CachedImage.h"
#include "CanvasGradient.h"
#include "CanvasPattern.h"
#include "CanvasRenderingContext2D.h"
#include "ColorSerialization.h"
#include "DOMMatrix2DInit.h"
#include "DOMPointInit.h"
#include "Document.h"
#include "Element.h"
#include "FloatPoint.h"
#include "GPUCanvasContext.h"
#include "GPUDevice.h"
#include "Gradient.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "HTMLVideoElement.h"
#include "Image.h"
#include "ImageBitmap.h"
#include "ImageBitmapRenderingContext.h"
#include "ImageBuffer.h"
#include "ImageData.h"
#include "ImageUtilities.h"
#include "InspectorCanvasAgent.h"
#include "InspectorDOMAgent.h"
#include "InspectorInstrumentation.h"
#include "JSCanvasDirection.h"
#include "JSCanvasFillRule.h"
#include "JSCanvasLineCap.h"
#include "JSCanvasLineJoin.h"
#include "JSCanvasRenderingContext2D.h"
#include "JSCanvasTextAlign.h"
#include "JSCanvasTextBaseline.h"
#include "JSDOMWrapperCache.h"
#include "JSExecState.h"
#include "JSGPUDevice.h"
#include "JSImageBitmapRenderingContext.h"
#include "JSImageSmoothingQuality.h"
#include "JSPredefinedColorSpace.h"
#include "JSWebGL2RenderingContext.h"
#include "JSWebGLRenderingContext.h"
#include "Path2D.h"
#include "SVGPathUtilities.h"
#include "StringAdaptors.h"
#include <JavaScriptCore/IdentifiersFactory.h>
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/ScriptCallStackFactory.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/Function.h>
#include <wtf/RefPtr.h>
#include <wtf/Scope.h>
#include <wtf/Vector.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/WTFString.h>

#if ENABLE(OFFSCREEN_CANVAS)
#include "JSOffscreenCanvasRenderingContext2D.h"
#include "OffscreenCanvas.h"
#include "OffscreenCanvasRenderingContext2D.h"
#endif

namespace WebCore {

using namespace Inspector;

Ref<InspectorCanvas> InspectorCanvas::create(CanvasRenderingContext& context)
{
    return adoptRef(*new InspectorCanvas(context));
}

Ref<InspectorCanvas> InspectorCanvas::create(GPUDevice& device)
{
    return adoptRef(*new InspectorCanvas(device));
}

InspectorCanvas::InspectorCanvas(CanvasRenderingContext& context)
    : m_identifier(makeString("canvas:"_s, IdentifiersFactory::createIdentifier()))
    , m_context(context)
{
}

CanvasRenderingContext* InspectorCanvas::canvasContext() const
{
    auto* context = std::get_if<WeakRef<CanvasRenderingContext>>(&m_context);
    return context ? context->ptr() : nullptr;
}

InspectorCanvas::InspectorCanvas(GPUDevice& device)
    : m_identifier(makeString("canvas:"_s, IdentifiersFactory::createIdentifier()))
    , m_context(device)
{
}

GPUDevice* InspectorCanvas::deviceContext() const
{
    auto* device = std::get_if<WeakRef<GPUDevice, WeakPtrImplWithEventTargetData>>(&m_context);
    return device ? device->ptr() : nullptr;
}

bool InspectorCanvas::hasActiveInspectorCanvasCallTracer() const
{
    return WTF::switchOn(m_context,
        [](const WeakRef<CanvasRenderingContext>& context) {
            return context->hasActiveInspectorCanvasCallTracer();
        },
        [](const WeakRef<GPUDevice, WeakPtrImplWithEventTargetData>& device) {
            return device->hasActiveInspectorCanvasCallTracer();
        }
    );
}

void InspectorCanvas::setHasActiveInspectorCanvasCallTracer(bool active)
{
    WTF::switchOn(m_context,
        [active](const WeakRef<CanvasRenderingContext>& context) {
            context->setHasActiveInspectorCanvasCallTracer(active);
        },
        [active](const WeakRef<GPUDevice, WeakPtrImplWithEventTargetData>& device) {
            device->setHasActiveInspectorCanvasCallTracer(active);
        }
    );
}

static bool canvasContextMatchesDevice(const CanvasRenderingContext& context, const GPUDevice& device)
{
    auto* gpuCanvasContext = dynamicDowncast<GPUCanvasContext>(context);
    return gpuCanvasContext && gpuCanvasContext->device() == &device;
}

HTMLCanvasElement* InspectorCanvas::canvasElement() const
{
    return WTF::switchOn(m_context,
        [](const WeakRef<CanvasRenderingContext>& weakContext) {
            Ref context = weakContext;
            return dynamicDowncast<HTMLCanvasElement>(context->canvasBase());
        },
        [](const WeakRef<GPUDevice, WeakPtrImplWithEventTargetData>&) -> HTMLCanvasElement* {
            return nullptr;
        }
    );
}

ScriptExecutionContext* InspectorCanvas::scriptExecutionContext() const
{
    return WTF::switchOn(m_context,
        [](const WeakRef<CanvasRenderingContext>& weakContext) {
            Ref context = weakContext;
            return context->canvasBase().scriptExecutionContext();
        },
        [](const WeakRef<GPUDevice, WeakPtrImplWithEventTargetData>& weakDevice) {
            Ref device = weakDevice;
            return device->scriptExecutionContext();
        }
    );
}

JSC::JSValue InspectorCanvas::resolveContext(JSC::JSGlobalObject* exec)
{
    JSC::JSLockHolder lock(exec);
    auto* globalObject = deprecatedGlobalObjectForPrototype(exec);

    return WTF::switchOn(m_context,
        [&](const WeakRef<CanvasRenderingContext>& weakContext) {
            Ref context = weakContext;
            if (is<CanvasRenderingContext2D>(context))
                return toJS(exec, globalObject, downcast<CanvasRenderingContext2D>(context));
#if ENABLE(OFFSCREEN_CANVAS)
            if (is<OffscreenCanvasRenderingContext2D>(context))
                return toJS(exec, globalObject, downcast<OffscreenCanvasRenderingContext2D>(context));
#endif
            if (is<ImageBitmapRenderingContext>(context))
                return toJS(exec, globalObject, downcast<ImageBitmapRenderingContext>(context));
#if ENABLE(WEBGL)
            if (is<WebGLRenderingContext>(context))
                return toJS(exec, globalObject, downcast<WebGLRenderingContext>(context));
            if (is<WebGL2RenderingContext>(context))
                return toJS(exec, globalObject, downcast<WebGL2RenderingContext>(context));
#endif
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const WeakRef<GPUDevice, WeakPtrImplWithEventTargetData>& weakDevice) {
            Ref device = weakDevice;
            return toJS(exec, globalObject, device);
        }
    );
}

HashSet<Element*> InspectorCanvas::clientNodes() const
{
    return WTF::switchOn(m_context,
        [](const WeakRef<CanvasRenderingContext>& weakContext) {
            Ref context = weakContext;
            return context->canvasBase().cssCanvasClients();
        },
        [](const WeakRef<GPUDevice, WeakPtrImplWithEventTargetData>& weakDevice) {
            Ref device = weakDevice;
            HashSet<Element*> clientNodes;
            Locker locker { CanvasRenderingContext::instancesLock() };
            for (SUPPRESS_UNCOUNTED_ARG auto* context : CanvasRenderingContext::instances()) {
                if (!context->isContextThread() || !canvasContextMatchesDevice(*context, device))
                    continue;
                if (RefPtr canvasElement = dynamicDowncast<HTMLCanvasElement>(context->canvasBase()))
                    clientNodes.add(canvasElement);
            }
            return clientNodes;
        }
    );
}

size_t InspectorCanvas::memoryCost() const
{
    return WTF::switchOn(m_context,
        [](const WeakRef<CanvasRenderingContext>& weakContext) {
            Ref context = weakContext;

            return context->memoryCost();
        },
        [](const WeakRef<GPUDevice, WeakPtrImplWithEventTargetData>& weakDevice) -> size_t {
            Ref device = weakDevice;

            CheckedSize memoryCost;
            Locker locker { CanvasRenderingContext::instancesLock() };
            for (SUPPRESS_UNCOUNTED_ARG auto* context : CanvasRenderingContext::instances()) {
                if (!context->isContextThread() || !canvasContextMatchesDevice(*context, device))
                    continue;
                memoryCost += context->memoryCost();
            }
            return memoryCost;
        }
    );
}

void InspectorCanvas::canvasChanged()
{
    Ref context = std::get<WeakRef<CanvasRenderingContext>>(m_context);
    if (!context->hasActiveInspectorCanvasCallTracer())
        return;

    // Since 2D contexts are able to be fully reproduced in the frontend, we don't need snapshots.
    if (is<CanvasRenderingContext2D>(context))
        return;
#if ENABLE(OFFSCREEN_CANVAS)
    if (is<OffscreenCanvasRenderingContext2D>(context))
        return;
#endif

    m_contentChanged = true;
}

void InspectorCanvas::resetRecordingData()
{
    m_initialState = nullptr;
    m_frames = nullptr;
    m_currentActions = nullptr;
    m_serializedDuplicateData = nullptr;
    m_indexedDuplicateData.clear();
    m_recordingObjectIdentifiers.clear();
    m_nextRecordingObjectIdentifier = 0;
    m_recordingName = { };
    m_bufferLimit = 100 * 1024 * 1024;
    m_bufferUsed = 0;
    m_frameCount = std::nullopt;
    m_framesCaptured = 0;
    m_contentChanged = false;

    setHasActiveInspectorCanvasCallTracer(false);
}

bool InspectorCanvas::hasRecordingData() const
{
    return m_bufferUsed > 0;
}

bool InspectorCanvas::currentFrameHasData() const
{
    return !!m_frames;
}

static bool shouldSnapshotBitmapRendererAction(const String& name)
{
    return name == "transferFromImageBitmap"_s;
}

#if ENABLE(WEBGL)
static bool shouldSnapshotWebGLAction(const String& name)
{
    return name == "clear"_s
        || name == "drawArrays"_s
        || name == "drawElements"_s;
}

static bool shouldSnapshotWebGL2Action(const String& name)
{
    return name == "clear"_s
        || name == "drawArrays"_s
        || name == "drawArraysInstanced"_s
        || name == "drawElements"_s
        || name == "drawElementsInstanced"_s;
}
#endif

static bool shouldSnapshotWebGPUAction(RecordingSwizzleType receiverSwizzleType, const String& name)
{
    if (receiverSwizzleType == RecordingSwizzleType::GPUQueue) {
        return name == "submit"_s
            || name == "writeTexture"_s
            || name == "copyExternalImageToTexture"_s;
    }
    return false;
}

void InspectorCanvas::recordAction(String&& name, InspectorCanvasProcessedArguments&& arguments)
{
    recordAction(WTF::move(name), WTF::move(arguments), nullptr);
}

void InspectorCanvas::recordAction(String&& name, InspectorCanvasProcessedArguments&& arguments, RefPtr<JSON::ArrayOf<int>> receiver)
{
    if (!m_initialState) {
        // We should only construct the initial state for the first action of the recording.
        ASSERT(!m_frames && !m_currentActions);

        m_initialState = buildInitialState();
        m_bufferUsed += protect(m_initialState)->memoryCost();
    }

    if (!m_frames)
        m_frames = JSON::ArrayOf<Inspector::Protocol::Recording::Frame>::create();

    if (!m_currentActions) {
        m_currentActions = JSON::ArrayOf<JSON::Value>::create();

        auto frame = Inspector::Protocol::Recording::Frame::create()
            .setActions(*m_currentActions)
            .release();

        protect(m_frames)->addItem(WTF::move(frame));
        ++m_framesCaptured;

        m_currentFrameStartTime = MonotonicTime::now();
    }

    appendActionSnapshotIfNeeded();

    if (RefPtr context = canvasContext()) {
        if (is<ImageBitmapRenderingContext>(context) && shouldSnapshotBitmapRendererAction(name))
            m_contentChanged = true;
#if ENABLE(WEBGL)
        else if (is<WebGLRenderingContext>(context) && shouldSnapshotWebGLAction(name))
            m_contentChanged = true;
        else if (is<WebGL2RenderingContext>(context) && shouldSnapshotWebGL2Action(name))
            m_contentChanged = true;
#endif
    }

    m_lastRecordedAction = buildAction(WTF::move(name), WTF::move(arguments));
    if (receiver)
        protect(m_lastRecordedAction)->addItem(receiver.releaseNonNull());
    m_bufferUsed += protect(m_lastRecordedAction)->memoryCost();
    protect(m_currentActions)->addItem(*m_lastRecordedAction);
}

static Ref<JSON::ArrayOf<int>> buildActionReceiver(size_t identifier, RecordingSwizzleType swizzleType)
{
    RELEASE_ASSERT(identifier <= static_cast<size_t>(std::numeric_limits<int>::max()));

    auto receiver = JSON::ArrayOf<int>::create();
    receiver->addItem(static_cast<int>(identifier));
    receiver->addItem(static_cast<int>(swizzleType));
    return receiver;
}

void InspectorCanvas::recordAction(String&& name, RecordingSwizzleType receiverSwizzleType, InspectorCanvasProcessedArguments&& arguments)
{
    ASSERT(receiverSwizzleType == RecordingSwizzleType::Canvas);

    recordAction(WTF::move(name), WTF::move(arguments), buildActionReceiver(0, receiverSwizzleType));
}

void InspectorCanvas::recordAction(String&& name, uintptr_t receiver, RecordingSwizzleType receiverSwizzleType, InspectorCanvasProcessedArguments&& arguments)
{
    ASSERT(deviceContext());

    bool shouldSnapshot = shouldSnapshotWebGPUAction(receiverSwizzleType, name);

    recordAction(WTF::move(name), WTF::move(arguments), buildActionReceiver(identifierForRecordingObject(receiver), receiverSwizzleType));

    if (shouldSnapshot)
        m_contentChanged = true;
}

void InspectorCanvas::finalizeFrame()
{
    appendActionSnapshotIfNeeded();

    if (m_frames && m_frames->length() && !m_currentFrameStartTime.isNaN()) {
        auto currentFrame = unsafeRefDowncast<Inspector::Protocol::Recording::Frame>(m_frames->get(m_frames->length() - 1));
        currentFrame->setDuration((MonotonicTime::now() - m_currentFrameStartTime).milliseconds());

        m_currentFrameStartTime = MonotonicTime::nan();
    }

    m_currentActions = nullptr;
}

void InspectorCanvas::markCurrentFrameIncomplete()
{
    if (!m_currentActions || !m_frames || !m_frames->length())
        return;

    auto currentFrame = unsafeRefDowncast<Inspector::Protocol::Recording::Frame>(m_frames->get(m_frames->length() - 1));
    currentFrame->setIncomplete(true);
}

void InspectorCanvas::setBufferLimit(long memoryLimit)
{
    m_bufferLimit = std::min<long>(memoryLimit, std::numeric_limits<int>::max());
}

bool InspectorCanvas::hasBufferSpace() const
{
    return m_bufferUsed < m_bufferLimit;
}

void InspectorCanvas::setFrameCount(long frameCount)
{
    if (frameCount > 0)
        m_frameCount = std::min<long>(frameCount, std::numeric_limits<int>::max());
    else
        m_frameCount = std::nullopt;
}

bool InspectorCanvas::overFrameCount() const
{
    return m_frameCount && m_framesCaptured >= m_frameCount.value();
}

static RefPtr<Inspector::Protocol::Canvas::ContextAttributes> buildObjectForCanvasContextAttributes(CanvasRenderingContext& context)
{
    if (is<CanvasRenderingContext2DBase>(context)) {
        auto attributes = downcast<CanvasRenderingContext2DBase>(context).getContextAttributes();
        auto contextAttributesPayload = Inspector::Protocol::Canvas::ContextAttributes::create()
            .release();
        switch (attributes.colorSpace) {
        case PredefinedColorSpace::SRGB:
            contextAttributesPayload->setColorSpace(Inspector::Protocol::Canvas::ColorSpace::SRGB);
            break;
        case PredefinedColorSpace::SRGBLinear:
            contextAttributesPayload->setColorSpace(Inspector::Protocol::Canvas::ColorSpace::SRGBLinear);
            break;
#if ENABLE(PREDEFINED_COLOR_SPACE_DISPLAY_P3)
        case PredefinedColorSpace::DisplayP3:
            contextAttributesPayload->setColorSpace(Inspector::Protocol::Canvas::ColorSpace::DisplayP3);
            break;
        case PredefinedColorSpace::DisplayP3Linear:
            contextAttributesPayload->setColorSpace(Inspector::Protocol::Canvas::ColorSpace::DisplayP3Linear);
            break;
#endif
        }
        contextAttributesPayload->setDesynchronized(attributes.desynchronized);
        contextAttributesPayload->setWillReadFrequently(attributes.willReadFrequently);
        return contextAttributesPayload;
    }

    if (is<ImageBitmapRenderingContext>(context)) {
        auto contextAttributesPayload = Inspector::Protocol::Canvas::ContextAttributes::create()
            .release();
        contextAttributesPayload->setAlpha(downcast<ImageBitmapRenderingContext>(context).hasAlpha());
        return contextAttributesPayload;
    }

#if ENABLE(WEBGL)
    if (is<WebGLRenderingContextBase>(context)) {
        const auto& attributes = downcast<WebGLRenderingContextBase>(context).getContextAttributes();
        if (!attributes)
            return nullptr;

        auto contextAttributesPayload = Inspector::Protocol::Canvas::ContextAttributes::create()
            .release();
        contextAttributesPayload->setAlpha(attributes->alpha);
        contextAttributesPayload->setDepth(attributes->depth);
        contextAttributesPayload->setStencil(attributes->stencil);
        contextAttributesPayload->setAntialias(attributes->antialias);
        contextAttributesPayload->setPremultipliedAlpha(attributes->premultipliedAlpha);
        contextAttributesPayload->setPreserveDrawingBuffer(attributes->preserveDrawingBuffer);
        switch (attributes->powerPreference) {
        case WebGLPowerPreference::Default:
            contextAttributesPayload->setPowerPreference("default"_s);
            break;
        case WebGLPowerPreference::LowPower:
            contextAttributesPayload->setPowerPreference("low-power"_s);
            break;
        case WebGLPowerPreference::HighPerformance:
            contextAttributesPayload->setPowerPreference("high-performance"_s);
            break;
        }
        contextAttributesPayload->setFailIfMajorPerformanceCaveat(attributes->failIfMajorPerformanceCaveat);
        return contextAttributesPayload;
    }
#endif // ENABLE(WEBGL)

    return nullptr;
}

Ref<Inspector::Protocol::Canvas::Canvas> InspectorCanvas::buildObjectForCanvas(bool captureBacktrace)
{
    Ref canvas = WTF::switchOn(m_context,
        [&](const WeakRef<CanvasRenderingContext>& weakContext) {
            Ref context = weakContext;

            auto contextType = [&] {
                bool isOffscreen = false;
#if ENABLE(OFFSCREEN_CANVAS)
                if (is<OffscreenCanvas>(context->canvasBase()))
                    isOffscreen = true;
#endif

                if (is<CanvasRenderingContext2D>(context)) {
                    ASSERT(!isOffscreen);
                    return Inspector::Protocol::Canvas::ContextType::Canvas2D;
                }
#if ENABLE(OFFSCREEN_CANVAS)
                if (is<OffscreenCanvasRenderingContext2D>(context)) {
                    ASSERT(isOffscreen);
                    return Inspector::Protocol::Canvas::ContextType::OffscreenCanvas2D;
                }
#endif
                if (is<ImageBitmapRenderingContext>(context)) {
                    if (isOffscreen)
                        return Inspector::Protocol::Canvas::ContextType::OffscreenBitmapRenderer;
                    return Inspector::Protocol::Canvas::ContextType::BitmapRenderer;
                }
#if ENABLE(WEBGL)
                if (is<WebGLRenderingContext>(context)) {
                    if (isOffscreen)
                        return Inspector::Protocol::Canvas::ContextType::OffscreenWebGL;
                    return Inspector::Protocol::Canvas::ContextType::WebGL;
                }
                if (is<WebGL2RenderingContext>(context)) {
                    if (isOffscreen)
                        return Inspector::Protocol::Canvas::ContextType::OffscreenWebGL2;
                    return Inspector::Protocol::Canvas::ContextType::WebGL2;
                }
#endif

                RELEASE_ASSERT_NOT_REACHED();
            }();

            auto result = Inspector::Protocol::Canvas::Canvas::create()
                .setCanvasId(m_identifier)
                .setContextType(contextType)
                .release();

            const auto& size = context->canvasBase().size();
            result->setWidth(size.width());
            result->setHeight(size.height());

            if (RefPtr node = dynamicDowncast<HTMLCanvasElement>(context->canvasBase())) {
                String cssCanvasName = node->document().nameForCSSCanvasElement(*node);
                if (!cssCanvasName.isEmpty())
                    result->setCssCanvasName(cssCanvasName);

                // FIXME: <https://webkit.org/b/178282> Web Inspector: send a DOM node with each Canvas payload and eliminate Canvas.requestNode
            }

            if (auto attributes = buildObjectForCanvasContextAttributes(context))
                result->setContextAttributes(attributes.releaseNonNull());

            return result;
        },
        [&](const WeakRef<GPUDevice, WeakPtrImplWithEventTargetData>& weakDevice) {
            Ref device = weakDevice;

            auto result = Inspector::Protocol::Canvas::Canvas::create()
                .setCanvasId(m_identifier)
                .setContextType(Inspector::Protocol::Canvas::ContextType::WebGPU)
                .release();

            auto features = JSON::ArrayOf<String>::create();
            for (const String& feature : device->backing().features().features())
                features->addItem(feature);
            result->setFeatures(WTF::move(features));

            if (auto label = device->label(); !label.isEmpty())
                result->setName(label);

            return result;
        }
    );

    if (size_t memoryCost = this->memoryCost())
        canvas->setMemoryCost(memoryCost);

    if (captureBacktrace) {
        auto stackTrace = Inspector::createScriptCallStack(JSExecState::currentState());
        canvas->setStackTrace(stackTrace->buildInspectorObject());
    }

    return canvas;
}

Ref<Inspector::Protocol::Recording::Recording> InspectorCanvas::releaseObjectForRecording()
{
    ASSERT(!m_currentActions);
    ASSERT(!m_lastRecordedAction);
    ASSERT(!m_frames);

    bool isOffscreen = false;
    RefPtr context = canvasContext();
#if ENABLE(OFFSCREEN_CANVAS)
    if (context && is<OffscreenCanvas>(context->canvasBase()))
        isOffscreen = true;
#endif

    Inspector::Protocol::Recording::Type type;
    if (!context) {
        ASSERT(deviceContext());
        type = Inspector::Protocol::Recording::Type::CanvasWebGPU;
    } else if (is<CanvasRenderingContext2D>(context)) {
        ASSERT(!isOffscreen);
        type = Inspector::Protocol::Recording::Type::Canvas2D;
#if ENABLE(OFFSCREEN_CANVAS)
    } else if (is<OffscreenCanvasRenderingContext2D>(context)) {
        ASSERT(isOffscreen);
        type = Inspector::Protocol::Recording::Type::OffscreenCanvas2D;
#endif
    } else if (is<ImageBitmapRenderingContext>(context)) {
        type = isOffscreen ? Inspector::Protocol::Recording::Type::OffscreenCanvasBitmapRenderer : Inspector::Protocol::Recording::Type::CanvasBitmapRenderer;
#if ENABLE(WEBGL)
    } else if (is<WebGLRenderingContext>(context)) {
        type = isOffscreen ? Inspector::Protocol::Recording::Type::OffscreenCanvasWebGL : Inspector::Protocol::Recording::Type::CanvasWebGL;
    } else if (is<WebGL2RenderingContext>(context)) {
        type = isOffscreen ? Inspector::Protocol::Recording::Type::OffscreenCanvasWebGL2 : Inspector::Protocol::Recording::Type::CanvasWebGL2;
#endif
    } else {
        ASSERT_NOT_REACHED();
        type = Inspector::Protocol::Recording::Type::Canvas2D;
    }

    auto recording = Inspector::Protocol::Recording::Recording::create()
        .setVersion(Inspector::Protocol::Recording::VERSION)
        .setType(type)
        .setInitialState(m_initialState.releaseNonNull())
        .setData(m_serializedDuplicateData.releaseNonNull())
        .release();

    if (!m_recordingName.isEmpty())
        recording->setName(m_recordingName);

    resetRecordingData();

    return recording;
}

Inspector::Protocol::ErrorStringOr<String> InspectorCanvas::getContentAsDataURL(CanvasRenderingContext& context)
{
    auto surfaceBuffer = context.compositingResultsNeedUpdating() ? CanvasRenderingContext::SurfaceBuffer::DrawingBuffer : CanvasRenderingContext::SurfaceBuffer::DisplayBufferForInspector;
    return encodeDataURL(context.surfaceBufferToImageBuffer(surfaceBuffer), "image/png"_s);
}

Inspector::Protocol::ErrorStringOr<String> InspectorCanvas::getContentAsDataURL()
{
    return WTF::switchOn(m_context,
        [](const WeakRef<CanvasRenderingContext>& weakContext) {
            Ref context = weakContext;
            return getContentAsDataURL(context);
        },
        [](const WeakRef<GPUDevice, WeakPtrImplWithEventTargetData>& weakDevice) -> Inspector::Protocol::ErrorStringOr<String> {
            Ref device = weakDevice;
            RefPtr<CanvasRenderingContext> context;
            {
                Locker locker { CanvasRenderingContext::instancesLock() };
                for (SUPPRESS_UNCOUNTED_ARG auto* candidate : CanvasRenderingContext::instances()) {
                    if (!candidate->isContextThread() || !canvasContextMatchesDevice(*candidate, device))
                        continue;
                    if (context) {
                        context = nullptr;
                        break;
                    }
                    context = candidate;
                }
            }
            if (!context)
                return makeUnexpected("GPUDevice must be configured for one <canvas>."_s);
            return getContentAsDataURL(*context);
        }
    );
}

void InspectorCanvas::appendActionSnapshotIfNeeded()
{
    if (!m_lastRecordedAction)
        return;

    if (m_contentChanged) {
        if (auto content = getContentAsDataURL()) {
            Ref lastRecordedAction = *m_lastRecordedAction;
            m_bufferUsed -= lastRecordedAction->memoryCost();
            if (lastRecordedAction->length() == 4)
                lastRecordedAction->addItem(-1); // Add the receiver if needed.
            lastRecordedAction->addItem(indexForData(*content));
            m_bufferUsed += lastRecordedAction->memoryCost();
        }
    }

    m_lastRecordedAction = nullptr;
    m_contentChanged = false;
}

int InspectorCanvas::indexForData(DuplicateDataVariant data)
{
    size_t index = m_indexedDuplicateData.findIf([&] (auto item) {
        if (data == item)
            return true;

        auto stackTraceA = std::get_if<Ref<ScriptCallStack>>(&data);
        auto stackTraceB = std::get_if<Ref<ScriptCallStack>>(&item);
        if (stackTraceA && stackTraceB)
            return (*stackTraceA)->isEqual(stackTraceB->ptr());

        auto parentStackTraceA = std::get_if<Ref<AsyncStackTrace>>(&data);
        auto parentStackTraceB = std::get_if<Ref<AsyncStackTrace>>(&item);
        if (parentStackTraceA && parentStackTraceB)
            return parentStackTraceA->ptr() == parentStackTraceB->ptr();

        return false;
    });
    if (index != notFound) {
        ASSERT(index < static_cast<size_t>(std::numeric_limits<int>::max()));
        return static_cast<int>(index);
    }

    if (!m_serializedDuplicateData)
        m_serializedDuplicateData = JSON::ArrayOf<JSON::Value>::create();

    RefPtr<JSON::Value> item;
    WTF::switchOn(data,
        [&](const Ref<HTMLImageElement>& imageElement) {
            String dataURL = "data:,"_s;

            if (RefPtr cachedImage = imageElement->cachedImage()) {
                RefPtr<Image> image = cachedImage->image();
                if (image && image != &Image::nullImage()) {
                    dataURL = encodeDataURL(image->currentNativeImage(), "image/png"_s);
                }
            }

            index = indexForData(dataURL);
        },
#if ENABLE(VIDEO)
        [&](Ref<HTMLVideoElement>& videoElement) {
            unsigned videoWidth = videoElement->videoWidth();
            unsigned videoHeight = videoElement->videoHeight();
            RefPtr imageBuffer = ImageBuffer::create(FloatSize(videoWidth, videoHeight), RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, 1, DestinationColorSpace::SRGB(), PixelFormat::BGRA8);
            if (imageBuffer)
                videoElement->paintCurrentFrameInContext(imageBuffer->context(), FloatRect(0, 0, videoWidth, videoHeight));
            index = indexForData(encodeDataURL(WTF::move(imageBuffer), "image/png"_s, std::nullopt));
        },
#endif
        [&](Ref<HTMLCanvasElement>& canvasElement) {
            String dataURL = "data:,"_s;

            ExceptionOr<UncachedString> result = canvasElement->toDataURL("image/png"_s);
            if (!result.hasException())
                dataURL = result.releaseReturnValue().string;

            index = indexForData(dataURL);
        },
        [&](Ref<CanvasGradient>& canvasGradient) { item = buildArrayForCanvasGradient(canvasGradient); },
        [&](Ref<CanvasPattern>& canvasPattern) { item = buildArrayForCanvasPattern(canvasPattern); },
        [&](Ref<ImageData>& imageData) { item = buildArrayForImageData(imageData); },
        [&](Ref<ImageBitmap>& imageBitmap) {
            index = indexForData(encodeDataURL(imageBitmap->buffer(), "image/png"_s));
        },
        [&](Ref<ScriptCallStack>& scriptCallStack) {
            auto stackTrace = JSON::ArrayOf<JSON::Value>::create();

            auto callFrames = JSON::ArrayOf<double>::create();
            for (size_t i = 0; i < scriptCallStack->size(); ++i)
                callFrames->addItem(indexForData(scriptCallStack->at(i)));
            stackTrace->addItem(WTF::move(callFrames));

            stackTrace->addItem(/* topCallFrameIsBoundary */ false);

            stackTrace->addItem(scriptCallStack->truncated());

            if (RefPtr parentStackTrace = scriptCallStack->parentStackTrace())
                stackTrace->addItem(indexForData(parentStackTrace.releaseNonNull()));

            item = WTF::move(stackTrace);
        },
        [&](const Ref<AsyncStackTrace>& parentStackTrace) {
            auto stackTrace = JSON::ArrayOf<JSON::Value>::create();

            auto callFrames = JSON::ArrayOf<double>::create();
            for (size_t i = 0; i < parentStackTrace->size(); ++i)
                callFrames->addItem(indexForData(parentStackTrace->at(i)));
            stackTrace->addItem(WTF::move(callFrames));

            stackTrace->addItem(parentStackTrace->topCallFrameIsBoundary());

            stackTrace->addItem(parentStackTrace->truncated());

            if (RefPtr grandparentStackTrace = parentStackTrace->parentStackTrace())
                stackTrace->addItem(indexForData(grandparentStackTrace.releaseNonNull()));

            item = WTF::move(stackTrace);
        },
        [&](const Ref<CSSStyleImageValue>& cssImageValue) {
            String dataURL = "data:,"_s;

            if (RefPtr cachedImage = cssImageValue->image()) {
                RefPtr image = cachedImage->image();
                if (image && image != &Image::nullImage())
                    dataURL = encodeDataURL(image->currentNativeImage(), "image/png"_s);
            }

            index = indexForData(dataURL);
        },
        [&](const ScriptCallFrame& scriptCallFrame) {
            auto array = JSON::ArrayOf<double>::create();
            array->addItem(indexForData(scriptCallFrame.functionName()));
            array->addItem(indexForData(scriptCallFrame.sourceURL()));
            array->addItem(static_cast<int>(scriptCallFrame.lineNumber()));
            array->addItem(static_cast<int>(scriptCallFrame.columnNumber()));
            item = WTF::move(array);
        },
#if ENABLE(OFFSCREEN_CANVAS)
        [&](const Ref<OffscreenCanvas> offscreenCanvas) {
            String dataURL = "data:,"_s;
            if (offscreenCanvas->originClean())
                dataURL = encodeDataURL(offscreenCanvas->makeRenderingResultsAvailable(), "image/png"_s);
            index = indexForData(dataURL);
        },
#endif
        [&](const String& value) { item = JSON::Value::create(value); }
    );

    if (item) {
        m_bufferUsed += item->memoryCost();
        protect(m_serializedDuplicateData)->addItem(item.releaseNonNull());

        m_indexedDuplicateData.append(data);
        index = m_indexedDuplicateData.size() - 1;
    }

    ASSERT(index < static_cast<size_t>(std::numeric_limits<int>::max()));
    return static_cast<int>(index);
}

size_t InspectorCanvas::identifierForRecordingObject(uintptr_t object)
{
    return m_recordingObjectIdentifiers.ensure(object, [&] {
        return ++m_nextRecordingObjectIdentifier;
    }).iterator->value;
}

Ref<JSON::Value> InspectorCanvas::valueIndexForData(DuplicateDataVariant data)
{
    return JSON::Value::create(indexForData(data));
}

String InspectorCanvas::stringIndexForKey(const String& key)
{
    return String::number(indexForData(key));
}

static Ref<JSON::ArrayOf<double>> buildArrayForAffineTransform(const AffineTransform& affineTransform)
{
    auto array = JSON::ArrayOf<double>::create();
    array->addItem(affineTransform.a());
    array->addItem(affineTransform.b());
    array->addItem(affineTransform.c());
    array->addItem(affineTransform.d());
    array->addItem(affineTransform.e());
    array->addItem(affineTransform.f());
    return array;
}

Ref<Inspector::Protocol::Recording::InitialState> InspectorCanvas::buildInitialState()
{
    auto initialStatePayload = Inspector::Protocol::Recording::InitialState::create().release();

    if (RefPtr context = canvasContext()) {
        auto attributesPayload = JSON::Object::create();
        attributesPayload->setInteger("width"_s, context->canvasBase().width());
        attributesPayload->setInteger("height"_s, context->canvasBase().height());

        auto statesPayload = JSON::ArrayOf<JSON::Object>::create();

        auto parametersPayload = JSON::ArrayOf<JSON::Value>::create();

        if (RefPtr context2d = dynamicDowncast<CanvasRenderingContext2DBase>(context)) {
            for (auto& state : context2d->stateStack()) {
                auto statePayload = JSON::Object::create();

                statePayload->setArray(stringIndexForKey("setTransform"_s), buildArrayForAffineTransform(state.transform));
                statePayload->setDouble(stringIndexForKey("globalAlpha"_s), state.globalAlpha);
                statePayload->setInteger(stringIndexForKey("globalCompositeOperation"_s), indexForData(state.globalCompositeOperationString()));
                statePayload->setDouble(stringIndexForKey("lineWidth"_s), state.lineWidth);
                statePayload->setInteger(stringIndexForKey("lineCap"_s), indexForData(convertEnumerationToString(state.canvasLineCap())));
                statePayload->setInteger(stringIndexForKey("lineJoin"_s), indexForData(convertEnumerationToString(state.canvasLineJoin())));
                statePayload->setDouble(stringIndexForKey("miterLimit"_s), state.miterLimit);
                statePayload->setDouble(stringIndexForKey("shadowOffsetX"_s), state.shadowOffset.width());
                statePayload->setDouble(stringIndexForKey("shadowOffsetY"_s), state.shadowOffset.height());
                statePayload->setDouble(stringIndexForKey("shadowBlur"_s), state.shadowBlur);
                statePayload->setInteger(stringIndexForKey("shadowColor"_s), indexForData(serializationForHTML(state.shadowColor)));

                // The parameter to `setLineDash` is itself an array, so we need to wrap the parameters
                // list in an array to allow spreading.
                auto setLineDash = JSON::ArrayOf<JSON::Value>::create();
                setLineDash->addItem(Inspector::Protocol::buildArray(state.lineDash));
                statePayload->setArray(stringIndexForKey("setLineDash"_s), WTF::move(setLineDash));

                statePayload->setDouble(stringIndexForKey("lineDashOffset"_s), state.lineDashOffset);
                statePayload->setInteger(stringIndexForKey("font"_s), indexForData(state.fontString()));
                statePayload->setInteger(stringIndexForKey("textAlign"_s), indexForData(convertEnumerationToString(state.canvasTextAlign())));
                statePayload->setInteger(stringIndexForKey("textBaseline"_s), indexForData(convertEnumerationToString(state.canvasTextBaseline())));
                statePayload->setInteger(stringIndexForKey("direction"_s), indexForData(convertEnumerationToString(state.direction)));

                int strokeStyleIndex;
                if (RefPtr canvasGradient = state.strokeStyle.canvasGradient())
                    strokeStyleIndex = indexForData(canvasGradient.releaseNonNull());
                else if (RefPtr canvasPattern = state.strokeStyle.canvasPattern())
                    strokeStyleIndex = indexForData(canvasPattern.releaseNonNull());
                else
                    strokeStyleIndex = indexForData(state.strokeStyle.colorString());
                statePayload->setInteger(stringIndexForKey("strokeStyle"_s), strokeStyleIndex);

                int fillStyleIndex;
                if (RefPtr canvasGradient = state.fillStyle.canvasGradient())
                    fillStyleIndex = indexForData(canvasGradient.releaseNonNull());
                else if (RefPtr canvasPattern = state.fillStyle.canvasPattern())
                    fillStyleIndex = indexForData(canvasPattern.releaseNonNull());
                else
                    fillStyleIndex = indexForData(state.fillStyle.colorString());
                statePayload->setInteger(stringIndexForKey("fillStyle"_s), fillStyleIndex);

                statePayload->setBoolean(stringIndexForKey("imageSmoothingEnabled"_s), state.imageSmoothingEnabled);
                statePayload->setInteger(stringIndexForKey("imageSmoothingQuality"_s), indexForData(convertEnumerationToString(state.imageSmoothingQuality)));

                // FIXME: This is wrong: it will repeat the context's current path for every level in the stack, ignoring saved paths.
                auto setPath = JSON::ArrayOf<JSON::Value>::create();
                setPath->addItem(indexForData(buildStringFromPath(context2d->getPath()->path())));
                statePayload->setArray(stringIndexForKey("setPath"_s), WTF::move(setPath));

                statesPayload->addItem(WTF::move(statePayload));
            }
        }

        if (auto contextAttributes = buildObjectForCanvasContextAttributes(*context))
            parametersPayload->addItem(contextAttributes.releaseNonNull());

        initialStatePayload->setAttributes(WTF::move(attributesPayload));

        if (statesPayload->length())
            initialStatePayload->setStates(WTF::move(statesPayload));

        if (parametersPayload->length())
            initialStatePayload->setParameters(WTF::move(parametersPayload));
    }

    if (auto content = getContentAsDataURL())
        initialStatePayload->setContent(*content);

    return initialStatePayload;
}

Ref<JSON::ArrayOf<JSON::Value>> InspectorCanvas::buildAction(String&& name, InspectorCanvasProcessedArguments&& arguments)
{
    auto action = JSON::ArrayOf<JSON::Value>::create();
    action->addItem(indexForData(WTF::move(name)));

    auto parametersData = JSON::ArrayOf<JSON::Value>::create();
    auto swizzleTypes = JSON::ArrayOf<int>::create();
    for (auto&& argument : WTF::move(arguments)) {
        if (!argument)
            continue;

        parametersData->addItem(argument->value.copyRef());
        swizzleTypes->addItem(static_cast<int>(argument->swizzleType));
    }
    action->addItem(WTF::move(parametersData));
    action->addItem(WTF::move(swizzleTypes));

    auto stackTrace = Inspector::createScriptCallStack(JSExecState::currentState());
    action->addItem(indexForData(WTF::move(stackTrace)));

    return action;
}

Ref<JSON::ArrayOf<JSON::Value>> InspectorCanvas::buildArrayForCanvasGradient(const CanvasGradient& canvasGradient)
{
    ASCIILiteral type = "linear-gradient"_s;
    auto parameters = JSON::ArrayOf<double>::create();
    WTF::switchOn(canvasGradient.gradient().data(),
        [&] (const Gradient::LinearData& data) {
            parameters->addItem(data.point0.x());
            parameters->addItem(data.point0.y());
            parameters->addItem(data.point1.x());
            parameters->addItem(data.point1.y());
        },
        [&] (const Gradient::RadialData& data) {
            type = "radial-gradient"_s;
            parameters->addItem(data.point0.x());
            parameters->addItem(data.point0.y());
            parameters->addItem(data.startRadius);
            parameters->addItem(data.point1.x());
            parameters->addItem(data.point1.y());
            parameters->addItem(data.endRadius);
        },
        [&] (const Gradient::ConicData& data) {
            type = "conic-gradient"_s;
            parameters->addItem(data.point0.x());
            parameters->addItem(data.point0.y());
            parameters->addItem(data.angleRadians);
        }
    );

    auto stops = JSON::ArrayOf<JSON::Value>::create();
    for (auto& colorStop : canvasGradient.gradient().stops()) {
        auto stop = JSON::ArrayOf<JSON::Value>::create();
        stop->addItem(colorStop.offset);
        stop->addItem(indexForData(serializationForCSS(colorStop.color)));
        stops->addItem(WTF::move(stop));
    }

    auto array = JSON::ArrayOf<JSON::Value>::create();
    array->addItem(indexForData(type));
    array->addItem(WTF::move(parameters));
    array->addItem(WTF::move(stops));
    return array;
}

Ref<JSON::ArrayOf<JSON::Value>> InspectorCanvas::buildArrayForCanvasPattern(const CanvasPattern& canvasPattern)
{
    String repeat;
    bool repeatX = canvasPattern.pattern().repeatX();
    bool repeatY = canvasPattern.pattern().repeatY();
    if (repeatX && repeatY)
        repeat = "repeat"_s;
    else if (repeatX && !repeatY)
        repeat = "repeat-x"_s;
    else if (!repeatX && repeatY)
        repeat = "repeat-y"_s;
    else
        repeat = "no-repeat"_s;

    auto array = JSON::ArrayOf<JSON::Value>::create();
    array->addItem(indexForData(encodeDataURL(canvasPattern.pattern().tileImageBuffer(), "image/png"_s)));
    array->addItem(indexForData(repeat));
    return array;
}

Ref<JSON::ArrayOf<JSON::Value>> InspectorCanvas::buildArrayForImageData(const ImageData& imageData)
{
    auto array = JSON::ArrayOf<JSON::Value>::create();
    array->addItem(imageData.data().copyToJSONArray());
    array->addItem(imageData.width());
    array->addItem(imageData.height());
    return array;
}

} // namespace WebCore

