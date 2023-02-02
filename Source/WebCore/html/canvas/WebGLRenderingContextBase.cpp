/*
 * Copyright (C) 2015-2023 Apple Inc. All rights reserved.
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
#include "WebGLRenderingContextBase.h"

#if ENABLE(WEBGL)

#include "ANGLEInstancedArrays.h"
#include "CachedImage.h"
#include "Chrome.h"
#include "DOMWindow.h"
#include "DiagnosticLoggingClient.h"
#include "DiagnosticLoggingKeys.h"
#include "Document.h"
#include "EXTBlendMinMax.h"
#include "EXTColorBufferFloat.h"
#include "EXTColorBufferHalfFloat.h"
#include "EXTFloatBlend.h"
#include "EXTFragDepth.h"
#include "EXTShaderTextureLOD.h"
#include "EXTTextureCompressionBPTC.h"
#include "EXTTextureCompressionRGTC.h"
#include "EXTTextureFilterAnisotropic.h"
#include "EXTTextureNorm16.h"
#include "EXTsRGB.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "GraphicsContextGLImageExtractor.h"
#include "GraphicsLayerContentsDisplayDelegate.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "HTMLVideoElement.h"
#include "HostWindow.h"
#include "ImageBitmap.h"
#include "ImageBuffer.h"
#include "ImageData.h"
#include "InspectorInstrumentation.h"
#include "IntSize.h"
#include "JSDOMPromiseDeferred.h"
#include "JSExecState.h"
#include "KHRParallelShaderCompile.h"
#include "Logging.h"
#include "NavigatorWebXR.h"
#include "NotImplemented.h"
#include "OESDrawBuffersIndexed.h"
#include "OESElementIndexUint.h"
#include "OESFBORenderMipmap.h"
#include "OESStandardDerivatives.h"
#include "OESTextureFloat.h"
#include "OESTextureFloatLinear.h"
#include "OESTextureHalfFloat.h"
#include "OESTextureHalfFloatLinear.h"
#include "OESVertexArrayObject.h"
#include "Page.h"
#include "RenderBox.h"
#include "Settings.h"
#include "WebCoreOpaqueRoot.h"
#include "WebGL2RenderingContext.h"
#include "WebGLActiveInfo.h"
#include "WebGLBuffer.h"
#include "WebGLClipCullDistance.h"
#include "WebGLColorBufferFloat.h"
#include "WebGLCompressedTextureASTC.h"
#include "WebGLCompressedTextureETC.h"
#include "WebGLCompressedTextureETC1.h"
#include "WebGLCompressedTexturePVRTC.h"
#include "WebGLCompressedTextureS3TC.h"
#include "WebGLCompressedTextureS3TCsRGB.h"
#include "WebGLContextAttributes.h"
#include "WebGLContextEvent.h"
#include "WebGLContextGroup.h"
#include "WebGLDebugRendererInfo.h"
#include "WebGLDebugShaders.h"
#include "WebGLDepthTexture.h"
#include "WebGLDrawBuffers.h"
#include "WebGLDrawInstancedBaseVertexBaseInstance.h"
#include "WebGLFramebuffer.h"
#include "WebGLLoseContext.h"
#include "WebGLMultiDraw.h"
#include "WebGLMultiDrawInstancedBaseVertexBaseInstance.h"
#include "WebGLProgram.h"
#include "WebGLProvokingVertex.h"
#include "WebGLRenderbuffer.h"
#include "WebGLRenderingContext.h"
#include "WebGLSampler.h"
#include "WebGLShader.h"
#include "WebGLShaderPrecisionFormat.h"
#include "WebGLTexture.h"
#include "WebGLTransformFeedback.h"
#include "WebGLUniformLocation.h"
#include "WebGLVertexArrayObject.h"
#include "WebGLVertexArrayObjectOES.h"
#include "WebXRSystem.h"
#include "WorkerClient.h"
#include "WorkerGlobalScope.h"
#include <JavaScriptCore/ConsoleMessage.h>
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/ScriptCallStack.h>
#include <JavaScriptCore/ScriptCallStackFactory.h>
#include <JavaScriptCore/SlotVisitor.h>
#include <JavaScriptCore/SlotVisitorInlines.h>
#include <JavaScriptCore/TypedArrayInlines.h>
#include <JavaScriptCore/Uint32Array.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/HashMap.h>
#include <wtf/HexNumber.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/Lock.h>
#include <wtf/Locker.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Scope.h>
#include <wtf/StdLibExtras.h>
#include <wtf/ThreadSpecific.h>
#include <wtf/UniqueArray.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

#if ENABLE(OFFSCREEN_CANVAS)
#include "OffscreenCanvas.h"
#endif

#if PLATFORM(MAC)
#include "PlatformScreen.h"
#endif

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(WebGLRenderingContextBase);

static constexpr Seconds secondsBetweenRestoreAttempts { 1_s };
static constexpr int maxGLErrorsAllowedToConsole = 256;
static constexpr Seconds checkContextLossHandlingDelay { 3_s };
static constexpr size_t maxActiveContexts = 16;
static constexpr size_t maxActiveWorkerContexts = 4;

namespace {
    
    GCGLint clamp(GCGLint value, GCGLint min, GCGLint max)
    {
        if (value < min)
            value = min;
        if (value > max)
            value = max;
        return value;
    }

    // Return true if a character belongs to the ASCII subset as defined in
    // GLSL ES 1.0 spec section 3.1.
    bool validateCharacter(unsigned char c)
    {
        // Printing characters are valid except " $ ` @ \ ' DEL.
        if (c >= 32 && c <= 126
            && c != '"' && c != '$' && c != '`' && c != '@' && c != '\\' && c != '\'')
            return true;
        // Horizontal tab, line feed, vertical tab, form feed, carriage return
        // are also valid.
        if (c >= 9 && c <= 13)
            return true;
        return false;
    }

    bool isPrefixReserved(const String& name)
    {
        if (name.startsWith("gl_"_s) || name.startsWith("webgl_"_s) || name.startsWith("_webgl_"_s))
            return true;
        return false;
    }

    // ES2 formats and internal formats supported by TexImageSource.
    static const GCGLenum SupportedFormatsES2[] = {
        GraphicsContextGL::RGB,
        GraphicsContextGL::RGBA,
        GraphicsContextGL::LUMINANCE_ALPHA,
        GraphicsContextGL::LUMINANCE,
        GraphicsContextGL::ALPHA,
    };

    // ES2 types supported by TexImageSource.
    static const GCGLenum SupportedTypesES2[] = {
        GraphicsContextGL::UNSIGNED_BYTE,
        GraphicsContextGL::UNSIGNED_SHORT_5_6_5,
        GraphicsContextGL::UNSIGNED_SHORT_4_4_4_4,
        GraphicsContextGL::UNSIGNED_SHORT_5_5_5_1,
    };

    // ES3 internal formats supported by TexImageSource.
    static const GCGLenum SupportedInternalFormatsTexImageSourceES3[] = {
        GraphicsContextGL::R8,
        GraphicsContextGL::R16F,
        GraphicsContextGL::R32F,
        GraphicsContextGL::R8UI,
        GraphicsContextGL::RG8,
        GraphicsContextGL::RG16F,
        GraphicsContextGL::RG32F,
        GraphicsContextGL::RG8UI,
        GraphicsContextGL::RGB8,
        GraphicsContextGL::SRGB8,
        GraphicsContextGL::RGB565,
        GraphicsContextGL::R11F_G11F_B10F,
        GraphicsContextGL::RGB9_E5,
        GraphicsContextGL::RGB16F,
        GraphicsContextGL::RGB32F,
        GraphicsContextGL::RGB8UI,
        GraphicsContextGL::RGBA8,
        GraphicsContextGL::SRGB8_ALPHA8,
        GraphicsContextGL::RGB5_A1,
        GraphicsContextGL::RGBA4,
        GraphicsContextGL::RGBA16F,
        GraphicsContextGL::RGBA32F,
        GraphicsContextGL::RGBA8UI,
        GraphicsContextGL::RGB10_A2,
    };

    // ES3 formats supported by TexImageSource.
    static const GCGLenum SupportedFormatsTexImageSourceES3[] = {
        GraphicsContextGL::RED,
        GraphicsContextGL::RED_INTEGER,
        GraphicsContextGL::RG,
        GraphicsContextGL::RG_INTEGER,
        GraphicsContextGL::RGB,
        GraphicsContextGL::RGB_INTEGER,
        GraphicsContextGL::RGBA,
        GraphicsContextGL::RGBA_INTEGER,
    };

    // ES3 types supported by TexImageSource.
    static const GCGLenum SupportedTypesTexImageSourceES3[] = {
        GraphicsContextGL::HALF_FLOAT,
        GraphicsContextGL::FLOAT,
        GraphicsContextGL::UNSIGNED_INT_10F_11F_11F_REV,
        GraphicsContextGL::UNSIGNED_INT_2_10_10_10_REV,
    };

    // Internal formats exposed by GL_EXT_sRGB.
    static const GCGLenum SupportedInternalFormatsEXTsRGB[] = {
        GraphicsContextGL::SRGB,
        GraphicsContextGL::SRGB_ALPHA,
    };

    // Formats exposed by GL_EXT_sRGB.
    static const GCGLenum SupportedFormatsEXTsRGB[] = {
        GraphicsContextGL::SRGB,
        GraphicsContextGL::SRGB_ALPHA,
    };

    // Types exposed by GL_OES_texture_float.
    static const GCGLenum SupportedTypesOESTextureFloat[] = {
        GraphicsContextGL::FLOAT,
    };

    // Types exposed by GL_OES_texture_half_float.
    static const GCGLenum SupportedTypesOESTextureHalfFloat[] = {
        GraphicsContextGL::HALF_FLOAT_OES,
    };
} // namespace anonymous

class ScopedUnpackParametersResetRestore {
public:
    explicit ScopedUnpackParametersResetRestore(WebGLRenderingContextBase* context, bool enabled = true)
        : m_context(context), m_enabled(enabled)
    {
        if (m_enabled)
            m_context->resetUnpackParameters();
    }

    ~ScopedUnpackParametersResetRestore()
    {
        if (m_enabled)
            m_context->restoreUnpackParameters();
    }

private:
    WebGLRenderingContextBase* m_context;
    bool m_enabled;
};

class ScopedDisableRasterizerDiscard {
public:
    explicit ScopedDisableRasterizerDiscard(WebGLRenderingContextBase* context, bool wasEnabled)
        : m_context(context)
        , m_wasEnabled(wasEnabled)
    {
        if (m_wasEnabled)
            m_context->disable(GraphicsContextGL::RASTERIZER_DISCARD);
    }

    ~ScopedDisableRasterizerDiscard()
    {
        if (m_wasEnabled)
            m_context->enable(GraphicsContextGL::RASTERIZER_DISCARD);
    }

private:
    WebGLRenderingContextBase* m_context;
    bool m_wasEnabled;
};

class ScopedEnableBackbuffer {
public:
    explicit ScopedEnableBackbuffer(GraphicsContextGL& context, GCGLenum backDrawBuffer, bool isWebGL2)
        : m_context(context)
        , m_backDrawBufferDisabled(backDrawBuffer == GraphicsContextGL::NONE)
        , m_isWebGL2(isWebGL2)
    {
        ASSERT(backDrawBuffer == GraphicsContextGL::NONE || backDrawBuffer == GraphicsContextGL::BACK);
        if (m_backDrawBufferDisabled) {
            GCGLenum value[1] { GraphicsContextGL::COLOR_ATTACHMENT0 };
            if (m_isWebGL2)
                m_context.drawBuffers(value);
            else
                m_context.drawBuffersEXT(value);
        }
    }

    ~ScopedEnableBackbuffer()
    {
        if (m_backDrawBufferDisabled) {
            GCGLenum value[1] { GraphicsContextGL::NONE };
            if (m_isWebGL2)
                m_context.drawBuffers(value);
            else
                m_context.drawBuffersEXT(value);
        }
    }

private:
    GraphicsContextGL& m_context;
    const bool m_backDrawBufferDisabled;
    const bool m_isWebGL2;
};

#define ADD_VALUES_TO_SET(set, arr) \
    set.add(arr, arr + std::size(arr))

InspectorScopedShaderProgramHighlight::InspectorScopedShaderProgramHighlight(WebGLRenderingContextBase& context, WebGLProgram* program)
    : m_context(context)
    , m_program(program)
{
    showHighlight();
}

InspectorScopedShaderProgramHighlight::~InspectorScopedShaderProgramHighlight()
{
    hideHighlight();
}

void InspectorScopedShaderProgramHighlight::showHighlight()
{
    if (!m_program || LIKELY(!InspectorInstrumentation::isWebGLProgramHighlighted(m_context, *m_program)))
        return;

    if (m_context.m_framebufferBinding)
        return;

    auto& gl = *m_context.graphicsContextGL();

    // When OES_draw_buffers_indexed extension is enabled,
    // these state queries return the state for draw buffer 0.
    // Constant blend color is always the same for all draw buffers.
    gl.getFloatv(GraphicsContextGL::BLEND_COLOR, m_savedBlend.color);
    m_savedBlend.equationRGB = gl.getInteger(GraphicsContextGL::BLEND_EQUATION_RGB);
    m_savedBlend.equationAlpha = gl.getInteger(GraphicsContextGL::BLEND_EQUATION_ALPHA);
    m_savedBlend.srcRGB = gl.getInteger(GraphicsContextGL::BLEND_SRC_RGB);
    m_savedBlend.dstRGB = gl.getInteger(GraphicsContextGL::BLEND_DST_RGB);
    m_savedBlend.srcAlpha = gl.getInteger(GraphicsContextGL::BLEND_SRC_ALPHA);
    m_savedBlend.dstAlpha = gl.getInteger(GraphicsContextGL::BLEND_DST_ALPHA);
    m_savedBlend.enabled = gl.isEnabled(GraphicsContextGL::BLEND);

    static const GCGLfloat red = 111.0 / 255.0;
    static const GCGLfloat green = 168.0 / 255.0;
    static const GCGLfloat blue = 220.0 / 255.0;
    static const GCGLfloat alpha = 2.0 / 3.0;
    gl.blendColor(red, green, blue, alpha);

    if (m_context.m_oesDrawBuffersIndexed) {
        gl.enableiOES(GraphicsContextGL::BLEND, 0);
        gl.blendEquationiOES(0, GraphicsContextGL::FUNC_ADD);
        gl.blendFunciOES(0, GraphicsContextGL::CONSTANT_COLOR, GraphicsContextGL::ONE_MINUS_SRC_ALPHA);
    } else {
        gl.enable(GraphicsContextGL::BLEND);
        gl.blendEquation(GraphicsContextGL::FUNC_ADD);
        gl.blendFunc(GraphicsContextGL::CONSTANT_COLOR, GraphicsContextGL::ONE_MINUS_SRC_ALPHA);
    }

    m_didApply = true;
}

void InspectorScopedShaderProgramHighlight::hideHighlight()
{
    if (!m_didApply)
        return;

    auto& gl = *m_context.graphicsContextGL();

    gl.blendColor(m_savedBlend.color[0], m_savedBlend.color[1], m_savedBlend.color[2], m_savedBlend.color[3]);

    if (m_context.m_oesDrawBuffersIndexed) {
        gl.blendEquationSeparateiOES(0, m_savedBlend.equationRGB, m_savedBlend.equationAlpha);
        gl.blendFuncSeparateiOES(0, m_savedBlend.srcRGB, m_savedBlend.dstRGB, m_savedBlend.srcAlpha, m_savedBlend.dstAlpha);
        if (!m_savedBlend.enabled)
            gl.disableiOES(GraphicsContextGL::BLEND, 0);
    } else {
        gl.blendEquationSeparate(m_savedBlend.equationRGB, m_savedBlend.equationAlpha);
        gl.blendFuncSeparate(m_savedBlend.srcRGB, m_savedBlend.dstRGB, m_savedBlend.srcAlpha, m_savedBlend.dstAlpha);
        if (!m_savedBlend.enabled)
            gl.disable(GraphicsContextGL::BLEND);
    }

    m_didApply = false;
}

static bool isHighPerformanceContext(const RefPtr<GraphicsContextGL>& context)
{
    return context->contextAttributes().powerPreference == WebGLPowerPreference::HighPerformance;
}

// Counter for determining which context has the earliest active ordinal number.
static std::atomic<uint64_t> s_lastActiveOrdinal;

using WebGLRenderingContextBaseSet = HashSet<WebGLRenderingContextBase*>;

static WebGLRenderingContextBaseSet& activeContexts()
{
    if (isMainThread()) {
        // WebKitLegacy special case: check for main thread because TLS does not work when entering sometimes from
        // WebThread and sometimes from real main thread.
        // Leave this on for non-legacy cases, as this is the base case for current operation where offscreen canvas
        // is not supported or is rarely used.
        static NeverDestroyed<WebGLRenderingContextBaseSet> s_mainThreadActiveContexts;
        return s_mainThreadActiveContexts.get();
    }
    static LazyNeverDestroyed<ThreadSpecific<WebGLRenderingContextBaseSet>> s_activeContexts;
    static std::once_flag s_onceFlag;
    std::call_once(s_onceFlag, [] {
        s_activeContexts.construct();
    });
    return *s_activeContexts.get();
}

static void addActiveContext(WebGLRenderingContextBase& newContext)
{
    auto& contexts = activeContexts();
    auto maxContextsSize = isMainThread() ? maxActiveContexts : maxActiveWorkerContexts;
    if (contexts.size() >= maxContextsSize) {
        auto* earliest = *std::min_element(contexts.begin(), contexts.end(), [] (auto& a, auto& b) {
            return a->activeOrdinal() < b->activeOrdinal();
        });
        earliest->recycleContext();
        ASSERT(earliest != &newContext); // This assert is here so we can assert isNewEntry below instead of top-level `!contexts.contains(newContext);`.
        ASSERT(contexts.size() < maxContextsSize);
    }
    auto result = contexts.add(&newContext);
    ASSERT_UNUSED(result, result.isNewEntry);
}

static void removeActiveContext(WebGLRenderingContextBase& context)
{
    bool didContain = activeContexts().remove(&context);
    ASSERT_UNUSED(didContain, didContain);
}

std::unique_ptr<WebGLRenderingContextBase> WebGLRenderingContextBase::create(CanvasBase& canvas, WebGLContextAttributes& attributes, WebGLVersion type)
{
    auto scriptExecutionContext = canvas.scriptExecutionContext();
    if (!scriptExecutionContext)
        return nullptr;

    if (type == GraphicsContextGLWebGLVersion::WebGL2 && !scriptExecutionContext->settingsValues().webGLEnabled)
        return nullptr;

    GraphicsClient* graphicsClient = canvas.graphicsClient();

    auto* canvasElement = dynamicDowncast<HTMLCanvasElement>(canvas);

    if (!scriptExecutionContext->settingsValues().webGLEnabled) {
        canvasElement->dispatchEvent(WebGLContextEvent::create(eventNames().webglcontextcreationerrorEvent,
            Event::CanBubble::No, Event::IsCancelable::Yes, "Web page was not allowed to create a WebGL context."_s));
        return nullptr;
    }

    if (scriptExecutionContext->settingsValues().forceWebGLUsesLowPower) {
        if (attributes.powerPreference == GraphicsContextGLPowerPreference::HighPerformance)
            LOG(WebGL, "Overriding powerPreference from high-performance to low-power.");
        attributes.powerPreference = GraphicsContextGLPowerPreference::LowPower;
    }

    if (canvasElement) {
        Document& document = canvasElement->document();
        RefPtr<Frame> frame = document.frame();
        if (!frame)
            return nullptr;

        Document& topDocument = document.topDocument();
        Page* page = topDocument.page();
        if (page)
            attributes.devicePixelRatio = page->deviceScaleFactor();
    }

    // FIXME: Should we try get the devicePixelRatio for workers for the page that created
    // the worker? What if it's a shared worker, and there's multiple answers?

    attributes.noExtensions = true;
    attributes.shareResources = false;
    attributes.initialPowerPreference = attributes.powerPreference;
    attributes.webGLVersion = type;
#if PLATFORM(MAC)
    // FIXME: Add MACCATALYST support for gpuIDForDisplay.
    if (graphicsClient)
        attributes.windowGPUID = gpuIDForDisplay(graphicsClient->displayID());
#endif
#if PLATFORM(COCOA)
    attributes.useMetal = scriptExecutionContext->settingsValues().webGLUsingMetal;
#endif

    RefPtr<GraphicsContextGL> context;
    if (graphicsClient)
        context = graphicsClient->createGraphicsContextGL(attributes);
    if (!context) {
        if (canvasElement) {
            canvasElement->dispatchEvent(WebGLContextEvent::create(eventNames().webglcontextcreationerrorEvent,
                Event::CanBubble::No, Event::IsCancelable::Yes, "Could not create a WebGL context."_s));
        }
        return nullptr;
    }

    std::unique_ptr<WebGLRenderingContextBase> renderingContext;
    if (type == WebGLVersion::WebGL2)
        renderingContext = WebGL2RenderingContext::create(canvas, context.releaseNonNull(), attributes);
    else
        renderingContext = WebGLRenderingContext::create(canvas, context.releaseNonNull(), attributes);
    renderingContext->suspendIfNeeded();

    return renderingContext;
}

WebGLRenderingContextBase::WebGLRenderingContextBase(CanvasBase& canvas, WebGLContextAttributes attributes)
    : GPUBasedCanvasRenderingContext(canvas)
    , m_restoreTimer(canvas.scriptExecutionContext(), *this, &WebGLRenderingContextBase::maybeRestoreContext)
    , m_attributes(attributes)
    , m_numGLErrorsToConsoleAllowed(maxGLErrorsAllowedToConsole)
    , m_checkForContextLossHandlingTimer(*this, &WebGLRenderingContextBase::checkForContextLossHandling)
#if ENABLE(WEBXR)
    , m_isXRCompatible(attributes.xrCompatible)
#endif
{
    m_restoreTimer.suspendIfNeeded();

    registerWithWebGLStateTracker();
    if (canvas.isHTMLCanvasElement())
        m_checkForContextLossHandlingTimer.startOneShot(checkContextLossHandlingDelay);
}

WebGLRenderingContextBase::WebGLRenderingContextBase(CanvasBase& canvas, Ref<GraphicsContextGL>&& context, WebGLContextAttributes attributes)
    : GPUBasedCanvasRenderingContext(canvas)
    , m_restoreTimer(canvas.scriptExecutionContext(), *this, &WebGLRenderingContextBase::maybeRestoreContext)
    , m_generatedImageCache(4)
    , m_attributes(attributes)
    , m_numGLErrorsToConsoleAllowed(maxGLErrorsAllowedToConsole)
    , m_checkForContextLossHandlingTimer(*this, &WebGLRenderingContextBase::checkForContextLossHandling)
#if ENABLE(WEBXR)
    , m_isXRCompatible(attributes.xrCompatible)
#endif
{
    setGraphicsContextGL(WTFMove(context));

    m_restoreTimer.suspendIfNeeded();

    m_contextGroup = WebGLContextGroup::create();
    m_contextGroup->addContext(*this);

    m_context->getIntegerv(GraphicsContextGL::MAX_VIEWPORT_DIMS, m_maxViewportDims);

    setupFlags();

    registerWithWebGLStateTracker();
    if (canvas.isHTMLCanvasElement())
        m_checkForContextLossHandlingTimer.startOneShot(checkContextLossHandlingDelay);

    addActivityStateChangeObserverIfNecessary();
}

WebGLCanvas WebGLRenderingContextBase::canvas()
{
    auto& base = canvasBase();
#if ENABLE(OFFSCREEN_CANVAS)
    if (is<OffscreenCanvas>(base))
        return &downcast<OffscreenCanvas>(base);
#endif
    return &downcast<HTMLCanvasElement>(base);
}

#if ENABLE(OFFSCREEN_CANVAS)
OffscreenCanvas* WebGLRenderingContextBase::offscreenCanvas()
{
    auto& base = canvasBase();
    if (!is<OffscreenCanvas>(base))
        return nullptr;
    return &downcast<OffscreenCanvas>(base);
}
#endif

// We check for context loss handling after a few seconds to give the JS a chance to register the event listeners
// and to discard temporary GL contexts (e.g. feature detection).
void WebGLRenderingContextBase::checkForContextLossHandling()
{
    auto canvas = htmlCanvas();
    if (!canvas)
        return;

    if (!canvas->renderer())
        return;

    auto* page = canvas->document().page();
    if (!page)
        return;

    bool handlesContextLoss = canvas->hasEventListeners(eventNames().webglcontextlostEvent) && canvas->hasEventListeners(eventNames().webglcontextrestoredEvent);
    page->diagnosticLoggingClient().logDiagnosticMessage(DiagnosticLoggingKeys::pageHandlesWebGLContextLossKey(), handlesContextLoss ? DiagnosticLoggingKeys::yesKey() : DiagnosticLoggingKeys::noKey(), ShouldSample::No);
}

void WebGLRenderingContextBase::registerWithWebGLStateTracker()
{
    auto canvas = htmlCanvas();
    if (!canvas)
        return;

    auto* page = canvas->document().page();
    if (!page)
        return;

    auto* tracker = page->webGLStateTracker();
    if (!tracker)
        return;

    m_trackerToken = tracker->token(m_attributes.initialPowerPreference);
}

void WebGLRenderingContextBase::initializeNewContext()
{
    ASSERT(!isContextLost());
    m_needsUpdate = true;
    m_markedCanvasDirty = false;
    m_activeTextureUnit = 0;
    m_packAlignment = 4;
    m_unpackAlignment = 4;
    m_unpackFlipY = false;
    m_unpackPremultiplyAlpha = false;
    m_unpackColorspaceConversion = GraphicsContextGL::BROWSER_DEFAULT_WEBGL;
    m_boundArrayBuffer = nullptr;
    m_currentProgram = nullptr;
    m_framebufferBinding = nullptr;
    m_renderbufferBinding = nullptr;
    m_depthMask = true;
    m_stencilEnabled = false;
    m_stencilMask = 0xFFFFFFFF;
    m_stencilMaskBack = 0xFFFFFFFF;
    m_stencilFuncRef = 0;
    m_stencilFuncRefBack = 0;
    m_stencilFuncMask = 0xFFFFFFFF;
    m_stencilFuncMaskBack = 0xFFFFFFFF;
    m_layerCleared = false;
    m_numGLErrorsToConsoleAllowed = maxGLErrorsAllowedToConsole;

    m_rasterizerDiscardEnabled = false;

    m_clearColor[0] = m_clearColor[1] = m_clearColor[2] = m_clearColor[3] = 0;
    m_scissorEnabled = false;
    m_clearDepth = 1;
    m_clearStencil = 0;
    m_colorMask[0] = m_colorMask[1] = m_colorMask[2] = m_colorMask[3] = true;

    GCGLint numCombinedTextureImageUnits = m_context->getInteger(GraphicsContextGL::MAX_COMBINED_TEXTURE_IMAGE_UNITS);
    if (numCombinedTextureImageUnits < 8) {
        // OpenGL ES 2.0 sets the minimum for
        // GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS to 8. Receiving a value less than
        // 8 means an error with the context. Signal that the context is lost.
        forceContextLost();
        return;
    }

    m_textureUnits.clear();
    m_textureUnits.resize(numCombinedTextureImageUnits);

    GCGLint numVertexAttribs = m_context->getInteger(GraphicsContextGL::MAX_VERTEX_ATTRIBS);
    if (numVertexAttribs < 8) {
        // OpenGL ES 2.0 sets the minimum for GL_MAX_VERTEX_ATTRIBS to
        // 8. Receiving a value less than 8 means an error with the
        // context. Signal that the context is lost.
        forceContextLost();
        return;
    }
    m_maxVertexAttribs = numVertexAttribs;
    m_vertexAttribValue.clear();
    m_vertexAttribValue.resize(m_maxVertexAttribs);

    m_maxTextureSize = m_context->getInteger(GraphicsContextGL::MAX_TEXTURE_SIZE);
    m_maxTextureLevel = WebGLTexture::computeLevelCount(m_maxTextureSize, m_maxTextureSize);
    m_maxCubeMapTextureSize = m_context->getInteger(GraphicsContextGL::MAX_CUBE_MAP_TEXTURE_SIZE);
    m_maxCubeMapTextureLevel = WebGLTexture::computeLevelCount(m_maxCubeMapTextureSize, m_maxCubeMapTextureSize);
    m_maxRenderbufferSize = m_context->getInteger(GraphicsContextGL::MAX_RENDERBUFFER_SIZE);

    // These two values from EXT_draw_buffers are lazily queried.
    m_maxDrawBuffers = 0;
    m_maxColorAttachments = 0;

    m_backDrawBuffer = GraphicsContextGL::BACK;
    m_drawBuffersWebGLRequirementsChecked = false;
    m_drawBuffersSupported = false;

    m_context->setDrawingBufferColorSpace(toDestinationColorSpace(m_drawingBufferColorSpace));

    IntSize canvasSize = clampedCanvasSize();
    m_context->reshape(canvasSize.width(), canvasSize.height());
    m_context->viewport(0, 0, canvasSize.width(), canvasSize.height());
    m_context->scissor(0, 0, canvasSize.width(), canvasSize.height());

    m_supportedTexImageSourceInternalFormats.clear();
    m_supportedTexImageSourceFormats.clear();
    m_supportedTexImageSourceTypes.clear();
    m_areWebGL2TexImageSourceFormatsAndTypesAdded = false;
    m_areOESTextureFloatFormatsAndTypesAdded = false;
    m_areOESTextureHalfFloatFormatsAndTypesAdded = false;
    m_areEXTsRGBFormatsAndTypesAdded = false;
    ADD_VALUES_TO_SET(m_supportedTexImageSourceInternalFormats, SupportedFormatsES2);
    ADD_VALUES_TO_SET(m_supportedTexImageSourceFormats, SupportedFormatsES2);
    ADD_VALUES_TO_SET(m_supportedTexImageSourceTypes, SupportedTypesES2);

    initializeVertexArrayObjects();
}

void WebGLRenderingContextBase::setupFlags()
{
    ASSERT(m_context);

    auto canvas = htmlCanvas();
    if (canvas) {
        if (Page* page = canvas->document().page())
            m_synthesizedErrorsToConsole = page->settings().webGLErrorsToConsoleEnabled();
    }

    // FIXME: With ANGLE as a backend this probably isn't needed any more. Unfortunately
    // turning it off causes problems.
    m_isGLES2Compliant = m_context->isGLES2Compliant();
    if (m_isGLES2Compliant) {
        m_isGLES2NPOTStrict = !m_context->isExtensionEnabled("GL_OES_texture_npot"_s);
        m_isDepthStencilSupported = m_context->isExtensionEnabled("GL_OES_packed_depth_stencil"_s) || m_context->isExtensionEnabled("GL_ANGLE_depth_texture"_s);
    } else {
        m_isGLES2NPOTStrict = true;
        m_isDepthStencilSupported = m_context->isExtensionEnabled("GL_EXT_packed_depth_stencil"_s) || m_context->isExtensionEnabled("GL_ANGLE_depth_texture"_s);
    }
    m_isRobustnessEXTSupported = m_context->isExtensionEnabled("GL_EXT_robustness"_s);
}

void WebGLRenderingContextBase::addCompressedTextureFormat(GCGLenum format)
{
    if (!m_compressedTextureFormats.contains(format))
        m_compressedTextureFormats.append(format);
}

void WebGLRenderingContextBase::resetUnpackParameters()
{
    if (m_unpackAlignment != 1)
        m_context->pixelStorei(GraphicsContextGL::UNPACK_ALIGNMENT, 1);
}

void WebGLRenderingContextBase::restoreUnpackParameters()
{
    if (m_unpackAlignment != 1)
        m_context->pixelStorei(GraphicsContextGL::UNPACK_ALIGNMENT, m_unpackAlignment);
}

void WebGLRenderingContextBase::addActivityStateChangeObserverIfNecessary()
{
    // We are only interested in visibility changes for contexts
    // that are using the high-performance GPU.
    if (!isHighPerformanceContext(m_context))
        return;

    auto* canvas = htmlCanvas();
    if (!canvas)
        return;

    auto* page = canvas->document().page();
    if (!page)
        return;

    page->addActivityStateChangeObserver(*this);

    // We won't get a state change right away, so
    // make sure the context knows if it visible or not.
    if (m_context)
        m_context->setContextVisibility(page->isVisible());
}

void WebGLRenderingContextBase::removeActivityStateChangeObserver()
{
    auto* canvas = htmlCanvas();
    if (canvas) {
        if (auto* page = canvas->document().page())
            page->removeActivityStateChangeObserver(*this);
    }
}

WebGLRenderingContextBase::~WebGLRenderingContextBase()
{
    // Remove all references to WebGLObjects so if they are the last reference
    // they will be freed before the last context is removed from the context group.
    m_boundArrayBuffer = nullptr;
    m_defaultVertexArrayObject = nullptr;
    m_boundVertexArrayObject = nullptr;
    m_currentProgram = nullptr;
    m_framebufferBinding = nullptr;
    m_renderbufferBinding = nullptr;

    for (auto& textureUnit : m_textureUnits) {
        textureUnit.texture2DBinding = nullptr;
        textureUnit.textureCubeMapBinding = nullptr;
    }

    m_blackTexture2D = nullptr;
    m_blackTextureCubeMap = nullptr;

    detachAndRemoveAllObjects();
    loseExtensions(LostContextMode::RealLostContext);
    destroyGraphicsContextGL();
    m_contextGroup->removeContext(*this);

    {
        Locker locker { WebGLProgram::instancesLock() };
        for (auto& entry : WebGLProgram::instances()) {
            if (entry.value == this) {
                // Don't remove any WebGLProgram from the instances list, as they may still exist.
                // Only remove the association with a WebGL context.
                entry.value = nullptr;
            }
        }
    }
}

void WebGLRenderingContextBase::setGraphicsContextGL(Ref<GraphicsContextGL>&& context)
{
    bool wasActive = m_context;
    if (m_context) {
        m_context->setClient(nullptr);
        m_context = nullptr;
    }
    m_context = WTFMove(context);
    m_context->setClient(this);
    updateActiveOrdinal();
    if (!wasActive)
        addActiveContext(*this);
}

void WebGLRenderingContextBase::destroyGraphicsContextGL()
{
    removeActivityStateChangeObserver();

    if (m_context) {
        m_context->setClient(nullptr);
        m_context = nullptr;
        removeActiveContext(*this);
    }
}

void WebGLRenderingContextBase::markContextChanged()
{
    if (m_framebufferBinding)
        return;

    m_context->markContextChanged();

    m_layerCleared = false;
    m_compositingResultsNeedUpdating = true;

    if (auto* canvas = htmlCanvas()) {
        RenderBox* renderBox = canvas->renderBox();
        if (isAccelerated() && renderBox && renderBox->hasAcceleratedCompositing()) {
            m_markedCanvasDirty = true;
            canvas->clearCopiedImage();
            renderBox->contentChanged(CanvasPixelsChanged);
            return;
        }
    }

    if (!m_markedCanvasDirty) {
        m_markedCanvasDirty = true;
        canvasBase().didDraw(FloatRect(FloatPoint(0, 0), clampedCanvasSize()));
    }
}

void WebGLRenderingContextBase::markContextChangedAndNotifyCanvasObserver(WebGLRenderingContextBase::CallerType caller)
{
    // Draw and clear ops with rasterizer discard enabled do not change the canvas.
    if (caller == CallerTypeDrawOrClear && m_rasterizerDiscardEnabled)
        return;

    // If we're not touching the default framebuffer, nothing visible has changed.
    if (m_framebufferBinding)
        return;

    markContextChanged();
    if (!isAccelerated())
        return;

    auto* canvas = htmlCanvas();
    if (!canvas)
        return;

    canvas->notifyObserversCanvasChanged(FloatRect(FloatPoint(0, 0), clampedCanvasSize()));
}

bool WebGLRenderingContextBase::clearIfComposited(WebGLRenderingContextBase::CallerType caller, GCGLbitfield mask)
{
    if (isContextLost())
        return false;

    // `clearIfComposited()` is a function that prepares for updates. Mark the context as active.
    updateActiveOrdinal();

    if (!m_context->layerComposited() || m_layerCleared || m_preventBufferClearForInspector)
        return false;

    GCGLbitfield buffersNeedingClearing = m_context->getBuffersToAutoClear();

    if (!buffersNeedingClearing || (mask && m_framebufferBinding) || (m_rasterizerDiscardEnabled && caller == CallerTypeDrawOrClear))
        return false;

    // Use the underlying GraphicsContext3D's attributes to take into
    // account (for example) packed depth/stencil buffers.
    auto contextAttributes = m_context->contextAttributes();

    // Determine if it's possible to combine the clear the user asked for and this clear.
    bool combinedClear = mask && !m_scissorEnabled;

    m_context->disable(GraphicsContextGL::SCISSOR_TEST);
    if (combinedClear && (mask & GraphicsContextGL::COLOR_BUFFER_BIT) && (m_backDrawBuffer != GraphicsContextGL::NONE)) {
        m_context->clearColor(m_colorMask[0] ? m_clearColor[0] : 0,
                              m_colorMask[1] ? m_clearColor[1] : 0,
                              m_colorMask[2] ? m_clearColor[2] : 0,
                              m_colorMask[3] ? m_clearColor[3] : 0);
    } else
        m_context->clearColor(0, 0, 0, 0);
    if (m_oesDrawBuffersIndexed)
        m_context->colorMaskiOES(0, true, true, true, true);
    else
        m_context->colorMask(true, true, true, true);
    GCGLbitfield clearMask = GraphicsContextGL::COLOR_BUFFER_BIT;
    if (contextAttributes.depth) {
        if (!combinedClear || !m_depthMask || !(mask & GraphicsContextGL::DEPTH_BUFFER_BIT))
            m_context->clearDepth(1.0f);
        clearMask |= GraphicsContextGL::DEPTH_BUFFER_BIT;
        m_context->depthMask(true);
    }
    if (contextAttributes.stencil) {
        if (combinedClear && (mask & GraphicsContextGL::STENCIL_BUFFER_BIT))
            m_context->clearStencil(m_clearStencil & m_stencilMask);
        else
            m_context->clearStencil(0);
        clearMask |= GraphicsContextGL::STENCIL_BUFFER_BIT;
        m_context->stencilMaskSeparate(GraphicsContextGL::FRONT, 0xFFFFFFFF);
    }

    GCGLenum bindingPoint = isWebGL2() ? GraphicsContextGL::DRAW_FRAMEBUFFER : GraphicsContextGL::FRAMEBUFFER;
    if (m_framebufferBinding)
        m_context->bindFramebuffer(bindingPoint, 0);
    {
        ScopedDisableRasterizerDiscard disable(this, m_rasterizerDiscardEnabled);
        ScopedEnableBackbuffer enable(*m_context, m_backDrawBuffer, isWebGL2());
        // If the WebGL 2.0 clearBuffer APIs already have been used to
        // selectively clear some of the buffers, don't destroy those
        // results.
        m_context->clear(clearMask & buffersNeedingClearing);
    }
    m_context->setBuffersToAutoClear(0);

    restoreStateAfterClear();
    if (m_framebufferBinding)
        m_context->bindFramebuffer(bindingPoint, objectOrZero(m_framebufferBinding.get()));
    m_layerCleared = true;

    return combinedClear;
}

void WebGLRenderingContextBase::restoreStateAfterClear()
{
    // Restore the state that the context set.
    if (m_scissorEnabled)
        m_context->enable(GraphicsContextGL::SCISSOR_TEST);
    m_context->clearColor(m_clearColor[0], m_clearColor[1],
                          m_clearColor[2], m_clearColor[3]);
    if (m_oesDrawBuffersIndexed)
        m_context->colorMaskiOES(0, m_colorMask[0], m_colorMask[1], m_colorMask[2], m_colorMask[3]);
    else
        m_context->colorMask(m_colorMask[0], m_colorMask[1], m_colorMask[2], m_colorMask[3]);
    m_context->clearDepth(m_clearDepth);
    m_context->clearStencil(m_clearStencil);
    m_context->stencilMaskSeparate(GraphicsContextGL::FRONT, m_stencilMask);
    m_context->depthMask(m_depthMask);
}

void WebGLRenderingContextBase::prepareForDisplayWithPaint()
{
    m_isDisplayingWithPaint = true;
}

void WebGLRenderingContextBase::paintRenderingResultsToCanvas()
{
    if (isContextLost())
        return;

    if (m_isDisplayingWithPaint) {
        bool canvasContainsDisplayBuffer = !m_markedCanvasDirty;
        prepareForDisplay();
        m_isDisplayingWithPaint = false;
        if (!canvasContainsDisplayBuffer) {
            auto& base = canvasBase();
            base.clearCopiedImage();
            m_markedCanvasDirty = false;
            if (auto buffer = base.buffer()) {
                // FIXME: Remote ImageBuffers do not flush the buffers that are drawn to a buffer.
                // Avoid leaking the WebGL content in the cases where a WebGL canvas element is drawn to a Context2D
                // canvas element repeatedly.
                buffer->flushDrawingContext();
                m_context->paintCompositedResultsToCanvas(*buffer);
            }
        }
        return;
    }

    clearIfComposited(CallerTypeOther);

    if (!m_markedCanvasDirty && !m_layerCleared)
        return;

    auto& base = canvasBase();
    base.clearCopiedImage();

    m_markedCanvasDirty = false;
    if (auto buffer = base.buffer()) {
        // FIXME: Remote ImageBuffers do not flush the buffers that are drawn to a buffer.
        // Avoid leaking the WebGL content in the cases where a WebGL canvas element is drawn to a Context2D
        // canvas element repeatedly.
        buffer->flushDrawingContext();
        m_context->paintRenderingResultsToCanvas(*buffer);
    }
}

RefPtr<PixelBuffer> WebGLRenderingContextBase::paintRenderingResultsToPixelBuffer()
{
    if (isContextLost())
        return nullptr;
    clearIfComposited(CallerTypeOther);
    return m_context->paintRenderingResultsToPixelBuffer();
}

#if ENABLE(MEDIA_STREAM)
RefPtr<VideoFrame> WebGLRenderingContextBase::paintCompositedResultsToVideoFrame()
{
    if (isContextLost())
        return nullptr;
    return m_context->paintCompositedResultsToVideoFrame();
}
#endif

WebGLTexture::TextureExtensionFlag WebGLRenderingContextBase::textureExtensionFlags() const
{
    return static_cast<WebGLTexture::TextureExtensionFlag>((m_oesTextureFloatLinear ? WebGLTexture::TextureExtensionFloatLinearEnabled : 0) | (m_oesTextureHalfFloatLinear ? WebGLTexture::TextureExtensionHalfFloatLinearEnabled : 0));
}

void WebGLRenderingContextBase::reshape(int width, int height)
{
    if (isContextLost())
        return;

    // This is an approximation because at WebGLRenderingContext level we don't
    // know if the underlying FBO uses textures or renderbuffers.
    GCGLint maxSize = std::min(m_maxTextureSize, m_maxRenderbufferSize);
    GCGLint maxWidth = std::min(maxSize, m_maxViewportDims[0]);
    GCGLint maxHeight = std::min(maxSize, m_maxViewportDims[1]);
    width = clamp(width, 1, maxWidth);
    height = clamp(height, 1, maxHeight);

    if (m_needsUpdate) {
        notifyCanvasContentChanged();
        m_needsUpdate = false;
    }

    // We don't have to mark the canvas as dirty, since the newly created image buffer will also start off
    // clear (and this matches what reshape will do).
    m_context->reshape(width, height);

    auto& textureUnit = m_textureUnits[m_activeTextureUnit];
    m_context->bindTexture(GraphicsContextGL::TEXTURE_2D, objectOrZero(textureUnit.texture2DBinding.get()));
    m_context->bindRenderbuffer(GraphicsContextGL::RENDERBUFFER, objectOrZero(m_renderbufferBinding.get()));
    if (m_framebufferBinding)
        m_context->bindFramebuffer(GraphicsContextGL::FRAMEBUFFER, objectOrZero(m_framebufferBinding.get()));
}

int WebGLRenderingContextBase::drawingBufferWidth() const
{
    if (isContextLost())
        return 0;

    return m_context->getInternalFramebufferSize().width();
}

int WebGLRenderingContextBase::drawingBufferHeight() const
{
    if (isContextLost())
        return 0;

    return m_context->getInternalFramebufferSize().height();
}

void WebGLRenderingContextBase::setDrawingBufferColorSpace(PredefinedColorSpace colorSpace)
{
    if (m_drawingBufferColorSpace == colorSpace)
        return;

    m_drawingBufferColorSpace = colorSpace;

    if (isContextLost())
        return;

    m_context->setDrawingBufferColorSpace(toDestinationColorSpace(colorSpace));
}

unsigned WebGLRenderingContextBase::sizeInBytes(GCGLenum type)
{
    switch (type) {
    case GraphicsContextGL::BYTE:
        return sizeof(GCGLbyte);
    case GraphicsContextGL::UNSIGNED_BYTE:
        return sizeof(GCGLubyte);
    case GraphicsContextGL::SHORT:
        return sizeof(GCGLshort);
    case GraphicsContextGL::UNSIGNED_SHORT:
        return sizeof(GCGLushort);
    case GraphicsContextGL::INT:
        return sizeof(GCGLint);
    case GraphicsContextGL::UNSIGNED_INT:
        return sizeof(GCGLuint);
    case GraphicsContextGL::FLOAT:
        return sizeof(GCGLfloat);
    case GraphicsContextGL::HALF_FLOAT:
        return 2;
    case GraphicsContextGL::INT_2_10_10_10_REV:
    case GraphicsContextGL::UNSIGNED_INT_2_10_10_10_REV:
        return 4;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

void WebGLRenderingContextBase::activeTexture(GCGLenum texture)
{
    if (isContextLost())
        return;
    if (texture - GraphicsContextGL::TEXTURE0 >= m_textureUnits.size()) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "activeTexture", "texture unit out of range");
        return;
    }
    m_activeTextureUnit = texture - GraphicsContextGL::TEXTURE0;
    m_context->activeTexture(texture);
}

void WebGLRenderingContextBase::attachShader(WebGLProgram& program, WebGLShader& shader)
{
    Locker locker { objectGraphLock() };

    if (!validateWebGLProgramOrShader("attachShader", &program) || !validateWebGLProgramOrShader("attachShader", &shader))
        return;
    if (!program.attachShader(locker, &shader)) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "attachShader", "shader attachment already has shader");
        return;
    }
    m_context->attachShader(program.object(), shader.object());
    shader.onAttached();
}

void WebGLRenderingContextBase::bindAttribLocation(WebGLProgram& program, GCGLuint index, const String& name)
{
    if (!validateWebGLProgramOrShader("bindAttribLocation", &program))
        return;
    if (!validateLocationLength("bindAttribLocation", name))
        return;
    if (!validateString("bindAttribLocation", name))
        return;
    if (isPrefixReserved(name)) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "bindAttribLocation", "reserved prefix");
        return;
    }
    if (index >= m_maxVertexAttribs) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "bindAttribLocation", "index out of range");
        return;
    }
    m_context->bindAttribLocation(program.object(), index, name);
}

bool WebGLRenderingContextBase::validateNullableWebGLObject(const char* functionName, WebGLObject* object)
{
    if (isContextLost())
        return false;
    if (!object) {
        // This differs in behavior to ValidateWebGLObject; null objects are allowed
        // in these entry points.
        return true;
    }
    return validateWebGLObject(functionName, object);
}

bool WebGLRenderingContextBase::validateBufferTarget(const char* functionName, GCGLenum target)
{
    switch (target) {
    case GraphicsContextGL::ARRAY_BUFFER:
    case GraphicsContextGL::ELEMENT_ARRAY_BUFFER:
        return true;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid target");
        return false;
    }
}

WebGLBuffer* WebGLRenderingContextBase::validateBufferDataTarget(const char* functionName, GCGLenum target)
{
    WebGLBuffer* buffer = nullptr;
    switch (target) {
    case GraphicsContextGL::ELEMENT_ARRAY_BUFFER:
        buffer = m_boundVertexArrayObject->getElementArrayBuffer();
        break;
    case GraphicsContextGL::ARRAY_BUFFER:
        buffer = m_boundArrayBuffer.get();
        break;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid target");
        return nullptr;
    }
    if (!buffer) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "no buffer");
        return nullptr;
    }
    return buffer;
}

bool WebGLRenderingContextBase::validateAndCacheBufferBinding(const AbstractLocker& locker, const char* functionName, GCGLenum target, WebGLBuffer* buffer)
{
    if (!validateBufferTarget(functionName, target))
        return false;

    if (buffer && buffer->getTarget() && buffer->getTarget() != target) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "buffers can not be used with multiple targets");
        return false;
    }

    if (target == GraphicsContextGL::ARRAY_BUFFER)
        m_boundArrayBuffer = buffer;
    else {
        ASSERT(target == GraphicsContextGL::ELEMENT_ARRAY_BUFFER);
        m_boundVertexArrayObject->setElementArrayBuffer(locker, buffer);
    }

    if (buffer && !buffer->getTarget())
        buffer->setTarget(target);
    return true;
}

void WebGLRenderingContextBase::bindBuffer(GCGLenum target, WebGLBuffer* buffer)
{
    Locker locker { objectGraphLock() };

    if (!validateNullableWebGLObject("bindBuffer", buffer))
        return;

    if (!validateAndCacheBufferBinding(locker, "bindBuffer", target, buffer))
        return;

    m_context->bindBuffer(target, objectOrZero(buffer));
}

void WebGLRenderingContextBase::bindFramebuffer(GCGLenum target, WebGLFramebuffer* buffer)
{
    Locker locker { objectGraphLock() };

    if (!validateNullableWebGLObject("bindFramebuffer", buffer))
        return;

    if (target != GraphicsContextGL::FRAMEBUFFER) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "bindFramebuffer", "invalid target");
        return;
    }

    setFramebuffer(locker, target, buffer);
}

void WebGLRenderingContextBase::bindRenderbuffer(GCGLenum target, WebGLRenderbuffer* renderBuffer)
{
    Locker locker { objectGraphLock() };

    if (!validateNullableWebGLObject("bindRenderbuffer", renderBuffer))
        return;
    if (target != GraphicsContextGL::RENDERBUFFER) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "bindRenderbuffer", "invalid target");
        return;
    }
    m_renderbufferBinding = renderBuffer;
    m_context->bindRenderbuffer(target, objectOrZero(renderBuffer));
    if (renderBuffer)
        renderBuffer->setHasEverBeenBound();
}

void WebGLRenderingContextBase::bindTexture(GCGLenum target, WebGLTexture* texture)
{
    Locker locker { objectGraphLock() };

    if (!validateNullableWebGLObject("bindTexture", texture))
        return;
    if (texture && texture->getTarget() && texture->getTarget() != target) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "bindTexture", "textures can not be used with multiple targets");
        return;
    }
    auto& textureUnit = m_textureUnits[m_activeTextureUnit];
    if (target == GraphicsContextGL::TEXTURE_2D) {
        textureUnit.texture2DBinding = texture;
    } else if (target == GraphicsContextGL::TEXTURE_CUBE_MAP) {
        textureUnit.textureCubeMapBinding = texture;
    } else if (isWebGL2() && target == GraphicsContextGL::TEXTURE_2D_ARRAY) {
        textureUnit.texture2DArrayBinding = texture;
    } else if (isWebGL2() && target == GraphicsContextGL::TEXTURE_3D) {
        textureUnit.texture3DBinding = texture;
    } else {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "bindTexture", "invalid target");
        return;
    }
    m_context->bindTexture(target, objectOrZero(texture));
    if (texture)
        texture->setTarget(target);

    // Note: previously we used to automatically set the TEXTURE_WRAP_R
    // repeat mode to CLAMP_TO_EDGE for cube map textures, because OpenGL
    // ES 2.0 doesn't expose this flag (a bug in the specification) and
    // otherwise the application has no control over the seams in this
    // dimension. However, it appears that supporting this properly on all
    // platforms is fairly involved (will require a HashMap from texture ID
    // in all ports), and we have not had any complaints, so the logic has
    // been removed.
}

void WebGLRenderingContextBase::blendColor(GCGLfloat red, GCGLfloat green, GCGLfloat blue, GCGLfloat alpha)
{
    if (isContextLost())
        return;
    m_context->blendColor(red, green, blue, alpha);
}

void WebGLRenderingContextBase::blendEquation(GCGLenum mode)
{
    if (isContextLost() || !validateBlendEquation("blendEquation", mode))
        return;
    m_context->blendEquation(mode);
}

void WebGLRenderingContextBase::blendEquationSeparate(GCGLenum modeRGB, GCGLenum modeAlpha)
{
    if (isContextLost() || !validateBlendEquation("blendEquation", modeRGB) || !validateBlendEquation("blendEquation", modeAlpha))
        return;
    m_context->blendEquationSeparate(modeRGB, modeAlpha);
}

void WebGLRenderingContextBase::blendFunc(GCGLenum sfactor, GCGLenum dfactor)
{
    if (isContextLost() || !validateBlendFuncFactors("blendFunc", sfactor, dfactor))
        return;
    m_context->blendFunc(sfactor, dfactor);
}

void WebGLRenderingContextBase::blendFuncSeparate(GCGLenum srcRGB, GCGLenum dstRGB, GCGLenum srcAlpha, GCGLenum dstAlpha)
{
    // Note: Alpha does not have the same restrictions as RGB.
    if (isContextLost() || !validateBlendFuncFactors("blendFunc", srcRGB, dstRGB))
        return;
    m_context->blendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void WebGLRenderingContextBase::bufferData(GCGLenum target, long long size, GCGLenum usage)
{
    if (isContextLost())
        return;
    RefPtr<WebGLBuffer> buffer = validateBufferDataParameters("bufferData", target, usage);
    if (!buffer)
        return;
    if (size < 0) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "bufferData", "size < 0");
        return;
    }
    if (size > static_cast<long long>(std::numeric_limits<unsigned>::max())) {
        // Trying to allocate too large buffers cause unexpected context loss. Better to disallow
        // it in validation.
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "bufferData", "size more than 32-bits");
        return;
    }
    m_context->bufferData(target, static_cast<GCGLsizeiptr>(size), usage);
}

void WebGLRenderingContextBase::bufferData(GCGLenum target, std::optional<BufferDataSource>&& data, GCGLenum usage)
{
    if (isContextLost())
        return;
    if (!data) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "bufferData", "null data");
        return;
    }
    RefPtr<WebGLBuffer> buffer = validateBufferDataParameters("bufferData", target, usage);
    if (!buffer)
        return;

    std::visit([&](auto& data) {
        m_context->bufferData(target, GCGLSpan<const GCGLvoid>(data->data(), data->byteLength()), usage);
    }, data.value());
}

void WebGLRenderingContextBase::bufferSubData(GCGLenum target, long long offset, BufferDataSource&& data)
{
    if (isContextLost())
        return;
    RefPtr<WebGLBuffer> buffer = validateBufferDataParameters("bufferSubData", target, GraphicsContextGL::STATIC_DRAW);
    if (!buffer)
        return;
    if (offset < 0) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "bufferSubData", "offset < 0");
        return;
    }

    std::visit([&](auto& data) {
        m_context->bufferSubData(target, static_cast<GCGLintptr>(offset), GCGLSpan<const GCGLvoid>(data->data(), data->byteLength()));
    }, data);
}

GCGLenum WebGLRenderingContextBase::checkFramebufferStatus(GCGLenum target)
{
    if (isContextLost())
        return GraphicsContextGL::FRAMEBUFFER_UNSUPPORTED;
    if (!validateFramebufferTarget(target)) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "checkFramebufferStatus", "invalid target");
        return 0;
    }
    return m_context->checkFramebufferStatus(target);
}

void WebGLRenderingContextBase::clear(GCGLbitfield mask)
{
    if (isContextLost())
        return;
    if (!clearIfComposited(CallerTypeDrawOrClear, mask))
        m_context->clear(mask);
    markContextChangedAndNotifyCanvasObserver();
}

void WebGLRenderingContextBase::clearColor(GCGLfloat r, GCGLfloat g, GCGLfloat b, GCGLfloat a)
{
    if (isContextLost())
        return;
    if (std::isnan(r))
        r = 0;
    if (std::isnan(g))
        g = 0;
    if (std::isnan(b))
        b = 0;
    if (std::isnan(a))
        a = 1;
    m_clearColor[0] = r;
    m_clearColor[1] = g;
    m_clearColor[2] = b;
    m_clearColor[3] = a;
    m_context->clearColor(r, g, b, a);
}

void WebGLRenderingContextBase::clearDepth(GCGLfloat depth)
{
    if (isContextLost())
        return;
    m_clearDepth = depth;
    m_context->clearDepth(depth);
}

void WebGLRenderingContextBase::clearStencil(GCGLint s)
{
    if (isContextLost())
        return;
    m_clearStencil = s;
    m_context->clearStencil(s);
}

void WebGLRenderingContextBase::colorMask(GCGLboolean red, GCGLboolean green, GCGLboolean blue, GCGLboolean alpha)
{
    if (isContextLost())
        return;
    m_colorMask[0] = red;
    m_colorMask[1] = green;
    m_colorMask[2] = blue;
    m_colorMask[3] = alpha;
    m_context->colorMask(red, green, blue, alpha);
}

void WebGLRenderingContextBase::compileShader(WebGLShader& shader)
{
    if (!validateWebGLProgramOrShader("compileShader", &shader))
        return;
    m_context->compileShader(shader.object());
}

void WebGLRenderingContextBase::compressedTexImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, ArrayBufferView& data)
{
    if (isContextLost())
        return;
    if (!validateTexture2DBinding("compressedTexImage2D", target))
        return;
    if (!validateCompressedTexFormat("compressedTexImage2D", internalformat))
        return;
    m_context->compressedTexImage2D(target, level, internalformat, width, height,
        border, data.byteLength(), makeGCGLSpan(data.baseAddress(), data.byteLength()));
}

void WebGLRenderingContextBase::compressedTexSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, ArrayBufferView& data)
{
    if (isContextLost())
        return;
    if (!validateTexture2DBinding("compressedTexSubImage2D", target))
        return;
    if (!validateCompressedTexFormat("compressedTexSubImage2D", format))
        return;
    m_context->compressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format, data.byteLength(), GCGLSpan<const GCGLvoid>(data.baseAddress(), data.byteLength()));
}

bool WebGLRenderingContextBase::validateSettableTexInternalFormat(const char* functionName, GCGLenum internalFormat)
{
    if (isWebGL2())
        return true;

    switch (internalFormat) {
    case GraphicsContextGL::DEPTH_COMPONENT:
    case GraphicsContextGL::DEPTH_STENCIL:
    case GraphicsContextGL::DEPTH_COMPONENT16:
    case GraphicsContextGL::DEPTH_COMPONENT24:
    case GraphicsContextGL::DEPTH_COMPONENT32F:
    case GraphicsContextGL::DEPTH24_STENCIL8:
    case GraphicsContextGL::DEPTH32F_STENCIL8:
    case GraphicsContextGL::STENCIL_INDEX8:
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "format can not be set, only rendered to");
        return false;
    default:
        return true;
    }
}

void WebGLRenderingContextBase::copyTexSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height)
{
    if (isContextLost())
        return;
    if (!validateTexture2DBinding("copyTexSubImage2D", target))
        return;
    clearIfComposited(CallerTypeOther);
    m_context->copyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
}

RefPtr<WebGLBuffer> WebGLRenderingContextBase::createBuffer()
{
    if (isContextLost())
        return nullptr;
    auto buffer = WebGLBuffer::create(*this);
    addSharedObject(buffer.get());
    return buffer;
}

RefPtr<WebGLFramebuffer> WebGLRenderingContextBase::createFramebuffer()
{
    if (isContextLost())
        return nullptr;
    auto buffer = WebGLFramebuffer::create(*this);
    addContextObject(buffer.get());
    return buffer;
}

RefPtr<WebGLTexture> WebGLRenderingContextBase::createTexture()
{
    if (isContextLost())
        return nullptr;
    auto texture = WebGLTexture::create(*this);
    addSharedObject(texture.get());
    return texture;
}

RefPtr<WebGLProgram> WebGLRenderingContextBase::createProgram()
{
    if (isContextLost())
        return nullptr;
    auto program = WebGLProgram::create(*this);
    addSharedObject(program.get());

    InspectorInstrumentation::didCreateWebGLProgram(*this, program.get());

    return program;
}

RefPtr<WebGLRenderbuffer> WebGLRenderingContextBase::createRenderbuffer()
{
    if (isContextLost())
        return nullptr;
    auto buffer = WebGLRenderbuffer::create(*this);
    addSharedObject(buffer.get());
    return buffer;
}

RefPtr<WebGLShader> WebGLRenderingContextBase::createShader(GCGLenum type)
{
    if (isContextLost())
        return nullptr;
    if (type != GraphicsContextGL::VERTEX_SHADER && type != GraphicsContextGL::FRAGMENT_SHADER) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "createShader", "invalid shader type");
        return nullptr;
    }

    auto shader = WebGLShader::create(*this, type);
    addSharedObject(shader.get());
    return shader;
}

void WebGLRenderingContextBase::cullFace(GCGLenum mode)
{
    if (isContextLost())
        return;
    m_context->cullFace(mode);
}

bool WebGLRenderingContextBase::deleteObject(const AbstractLocker& locker, WebGLObject* object)
{
    if (isContextLost() || !object)
        return false;
    if (!object->validate(contextGroup(), *this)) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "delete", "object does not belong to this context");
        return false;
    }
    if (object->isDeleted())
        return false;
    if (object->object())
        // We need to pass in context here because we want
        // things in this context unbound.
        object->deleteObject(locker, graphicsContextGL());
    return true;
}

#define REMOVE_BUFFER_FROM_BINDING(binding) \
    if (binding == buffer) \
        binding = nullptr;

void WebGLRenderingContextBase::uncacheDeletedBuffer(const AbstractLocker& locker, WebGLBuffer* buffer)
{
    REMOVE_BUFFER_FROM_BINDING(m_boundArrayBuffer);

    m_boundVertexArrayObject->unbindBuffer(locker, *buffer);
}

void WebGLRenderingContextBase::setBoundVertexArrayObject(const AbstractLocker&, WebGLVertexArrayObjectBase* arrayObject)
{
    m_boundVertexArrayObject = arrayObject ? arrayObject : m_defaultVertexArrayObject;
}

#undef REMOVE_BUFFER_FROM_BINDING

void WebGLRenderingContextBase::deleteBuffer(WebGLBuffer* buffer)
{
    Locker locker { objectGraphLock() };

    if (!deleteObject(locker, buffer))
        return;

    uncacheDeletedBuffer(locker, buffer);
}

void WebGLRenderingContextBase::deleteFramebuffer(WebGLFramebuffer* framebuffer)
{
    Locker locker { objectGraphLock() };

#if ENABLE(WEBXR)
    if (framebuffer && framebuffer->isOpaque()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "deleteFramebuffer", "An opaque framebuffer's attachments cannot be inspected or changed");
        return;
    }
#endif

    if (!deleteObject(locker, framebuffer))
        return;

    if (framebuffer == m_framebufferBinding) {
        m_framebufferBinding = nullptr;
        m_context->bindFramebuffer(GraphicsContextGL::FRAMEBUFFER, 0);
    }
}

void WebGLRenderingContextBase::deleteProgram(WebGLProgram* program)
{
    if (program)
        InspectorInstrumentation::willDestroyWebGLProgram(*program);

    Locker locker { objectGraphLock() };

    deleteObject(locker, program);
    // We don't reset m_currentProgram to 0 here because the deletion of the
    // current program is delayed.
}

void WebGLRenderingContextBase::deleteRenderbuffer(WebGLRenderbuffer* renderbuffer)
{
    Locker locker { objectGraphLock() };

    if (!deleteObject(locker, renderbuffer))
        return;
    if (renderbuffer == m_renderbufferBinding)
        m_renderbufferBinding = nullptr;
    if (m_framebufferBinding)
        m_framebufferBinding->removeAttachmentFromBoundFramebuffer(locker, GraphicsContextGL::FRAMEBUFFER, renderbuffer);
    auto readFramebufferBinding = getFramebufferBinding(GraphicsContextGL::READ_FRAMEBUFFER);
    if (readFramebufferBinding)
        readFramebufferBinding->removeAttachmentFromBoundFramebuffer(locker, GraphicsContextGL::READ_FRAMEBUFFER, renderbuffer);
}

void WebGLRenderingContextBase::deleteShader(WebGLShader* shader)
{
    Locker locker { objectGraphLock() };
    deleteObject(locker, shader);
}

void WebGLRenderingContextBase::deleteTexture(WebGLTexture* texture)
{
    Locker locker { objectGraphLock() };

    if (!deleteObject(locker, texture))
        return;

    for (auto& textureUnit : m_textureUnits) {
        if (texture == textureUnit.texture2DBinding)
            textureUnit.texture2DBinding = nullptr;
        if (texture == textureUnit.textureCubeMapBinding)
            textureUnit.textureCubeMapBinding = nullptr;
        if (isWebGL2()) {
            if (texture == textureUnit.texture3DBinding)
                textureUnit.texture3DBinding = nullptr;
            if (texture == textureUnit.texture2DArrayBinding)
                textureUnit.texture2DArrayBinding = nullptr;
        }
    }
    if (m_framebufferBinding)
        m_framebufferBinding->removeAttachmentFromBoundFramebuffer(locker, GraphicsContextGL::FRAMEBUFFER, texture);
    auto readFramebufferBinding = getFramebufferBinding(GraphicsContextGL::READ_FRAMEBUFFER);
    if (readFramebufferBinding)
        readFramebufferBinding->removeAttachmentFromBoundFramebuffer(locker, GraphicsContextGL::READ_FRAMEBUFFER, texture);
}

void WebGLRenderingContextBase::depthFunc(GCGLenum func)
{
    if (isContextLost())
        return;
    m_context->depthFunc(func);
}

void WebGLRenderingContextBase::depthMask(GCGLboolean flag)
{
    if (isContextLost())
        return;
    m_depthMask = flag;
    m_context->depthMask(flag);
}

void WebGLRenderingContextBase::depthRange(GCGLfloat zNear, GCGLfloat zFar)
{
    if (isContextLost())
        return;
    if (zNear > zFar) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "depthRange", "zNear > zFar");
        return;
    }
    m_context->depthRange(zNear, zFar);
}

void WebGLRenderingContextBase::detachShader(WebGLProgram& program, WebGLShader& shader)
{
    Locker locker { objectGraphLock() };

    if (!validateWebGLProgramOrShader("detachShader", &program) || !validateWebGLProgramOrShader("detachShader", &shader))
        return;
    if (!program.detachShader(locker, &shader)) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "detachShader", "shader not attached");
        return;
    }
    m_context->detachShader(program.object(), shader.object());
    shader.onDetached(locker, graphicsContextGL());
}

void WebGLRenderingContextBase::disable(GCGLenum cap)
{
    if (isContextLost() || !validateCapability("disable", cap))
        return;
    if (cap == GraphicsContextGL::STENCIL_TEST)
        m_stencilEnabled = false;
    if (cap == GraphicsContextGL::SCISSOR_TEST)
        m_scissorEnabled = false;
    if (cap == GraphicsContextGL::RASTERIZER_DISCARD)
        m_rasterizerDiscardEnabled = false;
    m_context->disable(cap);
}

void WebGLRenderingContextBase::disableVertexAttribArray(GCGLuint index)
{
    if (isContextLost())
        return;
    if (index >= m_maxVertexAttribs) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "disableVertexAttribArray", "index out of range");
        return;
    }
    m_boundVertexArrayObject->setVertexAttribEnabled(index, false);
    m_context->disableVertexAttribArray(index);
}

bool WebGLRenderingContextBase::validateVertexArrayObject(const char* functionName)
{
    if (!m_boundVertexArrayObject->areAllEnabledAttribBuffersBound()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "no buffer is bound to enabled attribute");
        return false;
    }
    return true;
}

bool WebGLRenderingContextBase::validateWebGLObject(const char* functionName, WebGLObject* object)
{
    if (isContextLost())
        return false;
    if (!object) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "object is null");
        return false;
    }
    if (object->isDeleted()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "attempt to use a deleted object");
        return false;
    }
    if (!object->validate(contextGroup(), *this)) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "object does not belong to this context");
        return false;
    }
    return true;
}

bool WebGLRenderingContextBase::validateWebGLProgramOrShader(const char* functionName, WebGLObject* object)
{
    if (isContextLost())
        return false;
    if (!object) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "program or shader is null");
        return false;
    }
    // OpenGL ES 3.0.5 p. 45:
    // "Commands that accept shader or program object names will generate the
    // error INVALID_VALUE if the provided name is not the name of either a shader
    // or program object and INVALID_OPERATION if the provided name identifies an
    // object that is not the expected type."
    //
    // Programs and shaders also have slightly different lifetime rules than other
    // objects in the API; they continue to be usable after being marked for
    // deletion.
    if (!object->object()) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "attempt to use a deleted program or shader");
        return false;
    }
    if (!object->validate(contextGroup(), *this)) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "object does not belong to this context");
        return false;
    }
    return true;
}

void WebGLRenderingContextBase::drawArrays(GCGLenum mode, GCGLint first, GCGLsizei count)
{
    if (isContextLost())
        return;
    if (!validateVertexArrayObject("drawArrays"))
        return;

    if (m_currentProgram && InspectorInstrumentation::isWebGLProgramDisabled(*this, *m_currentProgram))
        return;

    clearIfComposited(CallerTypeDrawOrClear);

    {
        InspectorScopedShaderProgramHighlight scopedHighlight(*this, m_currentProgram.get());

        m_context->drawArrays(mode, first, count);
    }

    markContextChangedAndNotifyCanvasObserver();
}

void WebGLRenderingContextBase::drawElements(GCGLenum mode, GCGLsizei count, GCGLenum type, long long offset)
{
    if (isContextLost())
        return;
    if (!validateVertexArrayObject("drawElements"))
        return;

    if (m_currentProgram && InspectorInstrumentation::isWebGLProgramDisabled(*this, *m_currentProgram))
        return;

    clearIfComposited(CallerTypeDrawOrClear);

    {
        InspectorScopedShaderProgramHighlight scopedHighlight(*this, m_currentProgram.get());

        m_context->drawElements(mode, count, type, static_cast<GCGLintptr>(offset));
    }
    markContextChangedAndNotifyCanvasObserver();
}

void WebGLRenderingContextBase::enable(GCGLenum cap)
{
    if (isContextLost() || !validateCapability("enable", cap))
        return;
    if (cap == GraphicsContextGL::STENCIL_TEST)
        m_stencilEnabled = true;
    if (cap == GraphicsContextGL::SCISSOR_TEST)
        m_scissorEnabled = true;
    if (cap == GraphicsContextGL::RASTERIZER_DISCARD)
        m_rasterizerDiscardEnabled = true;
    m_context->enable(cap);
}

void WebGLRenderingContextBase::enableVertexAttribArray(GCGLuint index)
{
    if (isContextLost())
        return;
    if (index >= m_maxVertexAttribs) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "enableVertexAttribArray", "index out of range");
        return;
    }
    m_boundVertexArrayObject->setVertexAttribEnabled(index, true);
    m_context->enableVertexAttribArray(index);
}

void WebGLRenderingContextBase::finish()
{
    if (isContextLost())
        return;
    m_context->finish();
}

void WebGLRenderingContextBase::flush()
{
    if (isContextLost())
        return;
    m_context->flush();
}

void WebGLRenderingContextBase::framebufferRenderbuffer(GCGLenum target, GCGLenum attachment, GCGLenum renderbuffertarget, WebGLRenderbuffer* buffer)
{
    if (isContextLost() || !validateFramebufferFuncParameters("framebufferRenderbuffer", target, attachment))
        return;
    if (renderbuffertarget != GraphicsContextGL::RENDERBUFFER) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "framebufferRenderbuffer", "invalid target");
        return;
    }
    if (!validateNullableWebGLObject("framebufferRenderbuffer", buffer))
        return;
    if (buffer && !buffer->hasEverBeenBound()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "framebufferRenderbuffer", "buffer has never been bound");
        return;
    }

    // Don't allow the default framebuffer to be mutated; all current
    // implementations use an FBO internally in place of the default
    // FBO.
    WebGLFramebuffer* framebufferBinding = getFramebufferBinding(target);
    if (!framebufferBinding || !framebufferBinding->object()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "framebufferRenderbuffer", "no framebuffer bound");
        return;
    }

#if ENABLE(WEBXR)
    if (framebufferBinding->isOpaque()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "framebufferRenderbuffer", "An opaque framebuffer's attachments cannot be inspected or changed");
        return;
    }
#endif

    framebufferBinding->setAttachmentForBoundFramebuffer(target, attachment, buffer);
}

void WebGLRenderingContextBase::framebufferTexture2D(GCGLenum target, GCGLenum attachment, GCGLenum textarget, WebGLTexture* texture, GCGLint level)
{
    if (isContextLost() || !validateFramebufferFuncParameters("framebufferTexture2D", target, attachment))
        return;
    if (level && isWebGL1() && !m_oesFBORenderMipmap) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "framebufferTexture2D", "level not 0 and OES_fbo_render_mipmap not enabled");
        return;
    }
    if (!validateNullableWebGLObject("framebufferTexture2D", texture))
        return;

    // Don't allow the default framebuffer to be mutated; all current
    // implementations use an FBO internally in place of the default
    // FBO.
    WebGLFramebuffer* framebufferBinding = getFramebufferBinding(target);
    if (!framebufferBinding || !framebufferBinding->object()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "framebufferTexture2D", "no framebuffer bound");
        return;
    }
#if ENABLE(WEBXR)
    if (framebufferBinding->isOpaque()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "framebufferTexture2D", "An opaque framebuffer's attachments cannot be inspected or changed");
        return;
    }
#endif

    framebufferBinding->setAttachmentForBoundFramebuffer(target, attachment, textarget, texture, level, 0);
}

void WebGLRenderingContextBase::frontFace(GCGLenum mode)
{
    if (isContextLost())
        return;
    m_context->frontFace(mode);
}

void WebGLRenderingContextBase::generateMipmap(GCGLenum target)
{
    if (isContextLost())
        return;
    if (!validateTextureBinding("generateMipmap", target))
        return;
    m_context->generateMipmap(target);
}

RefPtr<WebGLActiveInfo> WebGLRenderingContextBase::getActiveAttrib(WebGLProgram& program, GCGLuint index)
{
    if (!validateWebGLProgramOrShader("getActiveAttrib", &program))
        return nullptr;
    GraphicsContextGLActiveInfo info;
    if (!m_context->getActiveAttrib(program.object(), index, info))
        return nullptr;

    LOG(WebGL, "Returning active attribute %d: %s", index, info.name.utf8().data());

    return WebGLActiveInfo::create(info.name, info.type, info.size);
}

RefPtr<WebGLActiveInfo> WebGLRenderingContextBase::getActiveUniform(WebGLProgram& program, GCGLuint index)
{
    if (!validateWebGLProgramOrShader("getActiveUniform", &program))
        return nullptr;
    GraphicsContextGLActiveInfo info;
    if (!m_context->getActiveUniform(program.object(), index, info))
        return nullptr;
    // FIXME: Do we still need this for the ANGLE backend?
    if (!isGLES2Compliant())
        if (info.size > 1 && !info.name.endsWith("[0]"_s))
            info.name = makeString(info.name, "[0]"_s);

    LOG(WebGL, "Returning active uniform %d: %s", index, info.name.utf8().data());

    return WebGLActiveInfo::create(info.name, info.type, info.size);
}

std::optional<Vector<RefPtr<WebGLShader>>> WebGLRenderingContextBase::getAttachedShaders(WebGLProgram& program)
{
    if (!validateWebGLProgramOrShader("getAttachedShaders", &program))
        return std::nullopt;

    const GCGLenum shaderTypes[] = {
        GraphicsContextGL::VERTEX_SHADER,
        GraphicsContextGL::FRAGMENT_SHADER
    };
    Vector<RefPtr<WebGLShader>> shaderObjects;
    for (auto shaderType : shaderTypes) {
        if (RefPtr shader = program.getAttachedShader(shaderType))
            shaderObjects.append(WTFMove(shader));
    }
    return shaderObjects;
}

GCGLint WebGLRenderingContextBase::getAttribLocation(WebGLProgram& program, const String& name)
{
    if (!validateWebGLProgramOrShader("getAttribLocation", &program))
        return -1;
    if (!validateLocationLength("getAttribLocation", name))
        return -1;
    if (!validateString("getAttribLocation", name))
        return -1;
    if (isPrefixReserved(name))
        return -1;
    if (!program.getLinkStatus()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "getAttribLocation", "program not linked");
        return -1;
    }
    return m_context->getAttribLocation(program.object(), name);
}

WebGLAny WebGLRenderingContextBase::getBufferParameter(GCGLenum target, GCGLenum pname)
{
    if (isContextLost())
        return nullptr;

    bool valid = false;
    if (target == GraphicsContextGL::ARRAY_BUFFER || target == GraphicsContextGL::ELEMENT_ARRAY_BUFFER)
        valid = true;

    if (isWebGL2()) {
        switch (target) {
        case GraphicsContextGL::COPY_READ_BUFFER:
        case GraphicsContextGL::COPY_WRITE_BUFFER:
        case GraphicsContextGL::PIXEL_PACK_BUFFER:
        case GraphicsContextGL::PIXEL_UNPACK_BUFFER:
        case GraphicsContextGL::TRANSFORM_FEEDBACK_BUFFER:
        case GraphicsContextGL::UNIFORM_BUFFER:
            valid = true;
        }
    }

    if (!valid) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getBufferParameter", "invalid target");
        return nullptr;
    }

    if (pname != GraphicsContextGL::BUFFER_SIZE && pname != GraphicsContextGL::BUFFER_USAGE) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getBufferParameter", "invalid parameter name");
        return nullptr;
    }

    GCGLint value = m_context->getBufferParameteri(target, pname);
    if (pname == GraphicsContextGL::BUFFER_SIZE)
        return value;
    return static_cast<unsigned>(value);
}

std::optional<WebGLContextAttributes> WebGLRenderingContextBase::getContextAttributes()
{
    if (isContextLost())
        return std::nullopt;

    // Also, we need to enforce requested values of "false" for depth
    // and stencil, regardless of the properties of the underlying
    // GraphicsContextGLOpenGL.

    auto attributes = m_context->contextAttributes();
    if (!m_attributes.depth)
        attributes.depth = false;
    if (!m_attributes.stencil)
        attributes.stencil = false;
#if ENABLE(WEBXR)
    attributes.xrCompatible = m_isXRCompatible;
#endif
    return attributes;
}

GCGLenum WebGLRenderingContextBase::getError()
{
    if (isContextLost()) {
        auto& errors = m_contextLostState->errors;
        if (!errors.isEmpty())
            return errors.takeFirst();
        return GraphicsContextGL::NO_ERROR;
    }
    return m_context->getError();
}

WebGLAny WebGLRenderingContextBase::getParameter(GCGLenum pname)
{
    if (isContextLost())
        return nullptr;

    switch (pname) {
    case GraphicsContextGL::ACTIVE_TEXTURE:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::ALIASED_LINE_WIDTH_RANGE:
        return getWebGLFloatArrayParameter(pname);
    case GraphicsContextGL::ALIASED_POINT_SIZE_RANGE:
        return getWebGLFloatArrayParameter(pname);
    case GraphicsContextGL::ALPHA_BITS:
        if (!m_framebufferBinding && !m_attributes.alpha)
            return 0;
        return getIntParameter(pname);
    case GraphicsContextGL::ARRAY_BUFFER_BINDING:
        return m_boundArrayBuffer;
    case GraphicsContextGL::BLEND:
        return getBooleanParameter(pname);
    case GraphicsContextGL::BLEND_COLOR:
        return getWebGLFloatArrayParameter(pname);
    case GraphicsContextGL::BLEND_DST_ALPHA:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::BLEND_DST_RGB:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::BLEND_EQUATION_ALPHA:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::BLEND_EQUATION_RGB:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::BLEND_SRC_ALPHA:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::BLEND_SRC_RGB:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::BLUE_BITS:
        return getIntParameter(pname);
    case GraphicsContextGL::COLOR_CLEAR_VALUE:
        return getWebGLFloatArrayParameter(pname);
    case GraphicsContextGL::COLOR_WRITEMASK:
        return getBooleanArrayParameter(pname);
    case GraphicsContextGL::COMPRESSED_TEXTURE_FORMATS:
        return Uint32Array::tryCreate(m_compressedTextureFormats.data(), m_compressedTextureFormats.size());
    case GraphicsContextGL::CULL_FACE:
        return getBooleanParameter(pname);
    case GraphicsContextGL::CULL_FACE_MODE:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::CURRENT_PROGRAM:
        return m_currentProgram;
    case GraphicsContextGL::DEPTH_BITS:
        if (!m_framebufferBinding && !m_attributes.depth)
            return 0;
        return getIntParameter(pname);
    case GraphicsContextGL::DEPTH_CLEAR_VALUE:
        return getFloatParameter(pname);
    case GraphicsContextGL::DEPTH_FUNC:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::DEPTH_RANGE:
        return getWebGLFloatArrayParameter(pname);
    case GraphicsContextGL::DEPTH_TEST:
        return getBooleanParameter(pname);
    case GraphicsContextGL::DEPTH_WRITEMASK:
        return getBooleanParameter(pname);
    case GraphicsContextGL::DITHER:
        return getBooleanParameter(pname);
    case GraphicsContextGL::ELEMENT_ARRAY_BUFFER_BINDING:
        return RefPtr { m_boundVertexArrayObject->getElementArrayBuffer() };
    case GraphicsContextGL::FRAMEBUFFER_BINDING:
        return m_framebufferBinding;
    case GraphicsContextGL::FRONT_FACE:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::GENERATE_MIPMAP_HINT:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::GREEN_BITS:
        return getIntParameter(pname);
    case GraphicsContextGL::IMPLEMENTATION_COLOR_READ_FORMAT:
        FALLTHROUGH;
    case GraphicsContextGL::IMPLEMENTATION_COLOR_READ_TYPE: {
        int value = getIntParameter(pname);
        if (!value) {
            // This indicates the read framebuffer is incomplete and an
            // INVALID_OPERATION has been generated.
            return nullptr;
        }
        return value;
    }
    case GraphicsContextGL::LINE_WIDTH:
        return getFloatParameter(pname);
    case GraphicsContextGL::MAX_COMBINED_TEXTURE_IMAGE_UNITS:
        return getIntParameter(pname);
    case GraphicsContextGL::MAX_CUBE_MAP_TEXTURE_SIZE:
        return getIntParameter(pname);
    case GraphicsContextGL::MAX_FRAGMENT_UNIFORM_VECTORS:
        return getIntParameter(pname);
    case GraphicsContextGL::MAX_RENDERBUFFER_SIZE:
        return getIntParameter(pname);
    case GraphicsContextGL::MAX_TEXTURE_IMAGE_UNITS:
        return getIntParameter(pname);
    case GraphicsContextGL::MAX_TEXTURE_SIZE:
        return getIntParameter(pname);
    case GraphicsContextGL::MAX_VARYING_VECTORS:
        return getIntParameter(pname);
    case GraphicsContextGL::MAX_VERTEX_ATTRIBS:
        return getIntParameter(pname);
    case GraphicsContextGL::MAX_VERTEX_TEXTURE_IMAGE_UNITS:
        return getIntParameter(pname);
    case GraphicsContextGL::MAX_VERTEX_UNIFORM_VECTORS:
        return getIntParameter(pname);
    case GraphicsContextGL::MAX_VIEWPORT_DIMS:
        return getWebGLIntArrayParameter(pname);
    case GraphicsContextGL::NUM_SHADER_BINARY_FORMATS:
        return getIntParameter(pname);
    case GraphicsContextGL::PACK_ALIGNMENT:
        return getIntParameter(pname);
    case GraphicsContextGL::POLYGON_OFFSET_FACTOR:
        return getFloatParameter(pname);
    case GraphicsContextGL::POLYGON_OFFSET_FILL:
        return getBooleanParameter(pname);
    case GraphicsContextGL::POLYGON_OFFSET_UNITS:
        return getFloatParameter(pname);
    case GraphicsContextGL::RED_BITS:
        return getIntParameter(pname);
    case GraphicsContextGL::RENDERBUFFER_BINDING:
        return m_renderbufferBinding;
    case GraphicsContextGL::RENDERER:
        return "WebKit WebGL"_str;
    case GraphicsContextGL::SAMPLE_ALPHA_TO_COVERAGE:
        return getBooleanParameter(pname);
    case GraphicsContextGL::SAMPLE_BUFFERS:
        return getIntParameter(pname);
    case GraphicsContextGL::SAMPLE_COVERAGE:
        return getBooleanParameter(pname);
    case GraphicsContextGL::SAMPLE_COVERAGE_INVERT:
        return getBooleanParameter(pname);
    case GraphicsContextGL::SAMPLE_COVERAGE_VALUE:
        return getFloatParameter(pname);
    case GraphicsContextGL::SAMPLES:
        return getIntParameter(pname);
    case GraphicsContextGL::SCISSOR_BOX:
        return getWebGLIntArrayParameter(pname);
    case GraphicsContextGL::SCISSOR_TEST:
        return getBooleanParameter(pname);
    case GraphicsContextGL::SHADING_LANGUAGE_VERSION:
        if (!scriptExecutionContext()->settingsValues().maskWebGLStringsEnabled)
            return makeString("WebGL GLSL ES 1.0 (", m_context->getString(GraphicsContextGL::SHADING_LANGUAGE_VERSION), ')');
        return "WebGL GLSL ES 1.0 (1.0)"_str;
    case GraphicsContextGL::STENCIL_BACK_FAIL:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::STENCIL_BACK_FUNC:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::STENCIL_BACK_PASS_DEPTH_FAIL:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::STENCIL_BACK_PASS_DEPTH_PASS:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::STENCIL_BACK_REF:
        return getIntParameter(pname);
    case GraphicsContextGL::STENCIL_BACK_VALUE_MASK:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::STENCIL_BACK_WRITEMASK:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::STENCIL_BITS:
        if (!m_framebufferBinding && !m_attributes.stencil)
            return 0;
        return getIntParameter(pname);
    case GraphicsContextGL::STENCIL_CLEAR_VALUE:
        return getIntParameter(pname);
    case GraphicsContextGL::STENCIL_FAIL:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::STENCIL_FUNC:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::STENCIL_PASS_DEPTH_FAIL:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::STENCIL_PASS_DEPTH_PASS:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::STENCIL_REF:
        return getIntParameter(pname);
    case GraphicsContextGL::STENCIL_TEST:
        return m_stencilEnabled;
    case GraphicsContextGL::STENCIL_VALUE_MASK:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::STENCIL_WRITEMASK:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::SUBPIXEL_BITS:
        return getIntParameter(pname);
    case GraphicsContextGL::TEXTURE_BINDING_2D:
        return m_textureUnits[m_activeTextureUnit].texture2DBinding;
    case GraphicsContextGL::TEXTURE_BINDING_CUBE_MAP:
        return m_textureUnits[m_activeTextureUnit].textureCubeMapBinding;
    case GraphicsContextGL::UNPACK_ALIGNMENT:
        return getIntParameter(pname);
    case GraphicsContextGL::UNPACK_FLIP_Y_WEBGL:
        return m_unpackFlipY;
    case GraphicsContextGL::UNPACK_PREMULTIPLY_ALPHA_WEBGL:
        return m_unpackPremultiplyAlpha;
    case GraphicsContextGL::UNPACK_COLORSPACE_CONVERSION_WEBGL:
        return m_unpackColorspaceConversion;
    case GraphicsContextGL::VENDOR:
        return "WebKit"_str;
    case GraphicsContextGL::VERSION:
        return "WebGL 1.0"_str;
    case GraphicsContextGL::VIEWPORT:
        return getWebGLIntArrayParameter(pname);
    case GraphicsContextGL::FRAGMENT_SHADER_DERIVATIVE_HINT_OES: // OES_standard_derivatives
        if (m_oesStandardDerivatives)
            return getUnsignedIntParameter(GraphicsContextGL::FRAGMENT_SHADER_DERIVATIVE_HINT_OES);
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getParameter", "invalid parameter name, OES_standard_derivatives not enabled");
        return nullptr;
    case WebGLDebugRendererInfo::UNMASKED_RENDERER_WEBGL:
        if (m_webglDebugRendererInfo) {
#if !PLATFORM(IOS_FAMILY)
            if (!scriptExecutionContext()->settingsValues().maskWebGLStringsEnabled)
                return m_context->getString(GraphicsContextGL::RENDERER);
#endif
            return "Apple GPU"_str;
        }
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getParameter", "invalid parameter name, WEBGL_debug_renderer_info not enabled");
        return nullptr;
    case WebGLDebugRendererInfo::UNMASKED_VENDOR_WEBGL:
        if (m_webglDebugRendererInfo) {
#if !PLATFORM(IOS_FAMILY)
            if (!scriptExecutionContext()->settingsValues().maskWebGLStringsEnabled)
                return m_context->getString(GraphicsContextGL::VENDOR);
#endif
            return "Apple Inc."_str;
        }
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getParameter", "invalid parameter name, WEBGL_debug_renderer_info not enabled");
        return nullptr;
    case GraphicsContextGL::VERTEX_ARRAY_BINDING_OES: // OES_vertex_array_object
        if (m_oesVertexArrayObject) {
            if (m_boundVertexArrayObject->isDefaultObject())
                return nullptr;
            return static_pointer_cast<WebGLVertexArrayObjectOES>(m_boundVertexArrayObject);
        }
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getParameter", "invalid parameter name, OES_vertex_array_object not enabled");
        return nullptr;
    case GraphicsContextGL::MAX_TEXTURE_MAX_ANISOTROPY_EXT: // EXT_texture_filter_anisotropic
        if (m_extTextureFilterAnisotropic)
            return getUnsignedIntParameter(GraphicsContextGL::MAX_TEXTURE_MAX_ANISOTROPY_EXT);
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getParameter", "invalid parameter name, EXT_texture_filter_anisotropic not enabled");
        return nullptr;
    case GraphicsContextGL::MAX_COLOR_ATTACHMENTS_EXT: // EXT_draw_buffers BEGIN
        if (m_webglDrawBuffers || isWebGL2())
            return getMaxColorAttachments();
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getParameter", "invalid parameter name, WEBGL_draw_buffers not enabled");
        return nullptr;
    case GraphicsContextGL::MAX_DRAW_BUFFERS_EXT:
        if (m_webglDrawBuffers || isWebGL2())
            return getMaxDrawBuffers();
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getParameter", "invalid parameter name, WEBGL_draw_buffers not enabled");
        return nullptr;
    default:
        if ((m_webglDrawBuffers || isWebGL2())
            && pname >= GraphicsContextGL::DRAW_BUFFER0_EXT
            && pname < static_cast<GCGLenum>(GraphicsContextGL::DRAW_BUFFER0_EXT + getMaxDrawBuffers())) {
            GCGLint value = GraphicsContextGL::NONE;
            if (m_framebufferBinding)
                value = m_framebufferBinding->getDrawBuffer(pname);
            else // emulated backbuffer
                value = m_backDrawBuffer;
            return value;
        }
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getParameter", "invalid parameter name");
        return nullptr;
    }
}

WebGLAny WebGLRenderingContextBase::getProgramParameter(WebGLProgram& program, GCGLenum pname)
{
    // COMPLETION_STATUS_KHR should always return true if the context is lost, so applications
    // don't get stuck in an infinite polling loop.
    if (isContextLost()) {
        if (pname == GraphicsContextGL::COMPLETION_STATUS_KHR)
            return true;
        return nullptr;
    }
    if (!validateWebGLProgramOrShader("getProgramParameter", &program))
        return nullptr;

    switch (pname) {
    case GraphicsContextGL::DELETE_STATUS:
        return program.isDeleted();
    case GraphicsContextGL::VALIDATE_STATUS:
        return static_cast<bool>(m_context->getProgrami(program.object(), pname));
    case GraphicsContextGL::LINK_STATUS:
        return program.getLinkStatus();
    case GraphicsContextGL::ATTACHED_SHADERS:
        return m_context->getProgrami(program.object(), pname);
    case GraphicsContextGL::ACTIVE_ATTRIBUTES:
    case GraphicsContextGL::ACTIVE_UNIFORMS:
        return m_context->getProgrami(program.object(), pname);
    case GraphicsContextGL::COMPLETION_STATUS_KHR:
        if (m_khrParallelShaderCompile)
            return static_cast<bool>(m_context->getProgrami(program.object(), pname));
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getProgramParameter", "KHR_parallel_shader_compile not enabled");
        return nullptr;
    default:
        if (isWebGL2()) {
            switch (pname) {
            case GraphicsContextGL::TRANSFORM_FEEDBACK_BUFFER_MODE:
            case GraphicsContextGL::TRANSFORM_FEEDBACK_VARYINGS:
            case GraphicsContextGL::ACTIVE_UNIFORM_BLOCKS:
                return m_context->getProgrami(program.object(), pname);
            default:
                break;
            }
        }
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getProgramParameter", "invalid parameter name");
        return nullptr;
    }
}

String WebGLRenderingContextBase::getProgramInfoLog(WebGLProgram& program)
{
    if (!validateWebGLProgramOrShader("getProgramInfoLog", &program))
        return String();
    return ensureNotNull(m_context->getProgramInfoLog(program.object()));
}

WebGLAny WebGLRenderingContextBase::getRenderbufferParameter(GCGLenum target, GCGLenum pname)
{
    if (isContextLost())
        return nullptr;
    if (target != GraphicsContextGL::RENDERBUFFER) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getRenderbufferParameter", "invalid target");
        return nullptr;
    }
    if (!m_renderbufferBinding || !m_renderbufferBinding->object()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "getRenderbufferParameter", "no renderbuffer bound");
        return nullptr;
    }

    if (m_renderbufferBinding->getInternalFormat() == GraphicsContextGL::DEPTH_STENCIL
        && !m_renderbufferBinding->isValid()) {
        ASSERT(!isDepthStencilSupported());
        int value = 0;
        switch (pname) {
        case GraphicsContextGL::RENDERBUFFER_WIDTH:
            value = m_renderbufferBinding->getWidth();
            break;
        case GraphicsContextGL::RENDERBUFFER_HEIGHT:
            value = m_renderbufferBinding->getHeight();
            break;
        case GraphicsContextGL::RENDERBUFFER_RED_SIZE:
        case GraphicsContextGL::RENDERBUFFER_GREEN_SIZE:
        case GraphicsContextGL::RENDERBUFFER_BLUE_SIZE:
        case GraphicsContextGL::RENDERBUFFER_ALPHA_SIZE:
            value = 0;
            break;
        case GraphicsContextGL::RENDERBUFFER_DEPTH_SIZE:
            value = 24;
            break;
        case GraphicsContextGL::RENDERBUFFER_STENCIL_SIZE:
            value = 8;
            break;
        case GraphicsContextGL::RENDERBUFFER_INTERNAL_FORMAT:
            return m_renderbufferBinding->getInternalFormat();
        default:
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getRenderbufferParameter", "invalid parameter name");
            return nullptr;
        }
        return value;
    }

    switch (pname) {
    case GraphicsContextGL::RENDERBUFFER_SAMPLES:
        if (!isWebGL2()) {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getRenderbufferParameter", "invalid parameter name");
            return nullptr;
        }
        FALLTHROUGH;
    case GraphicsContextGL::RENDERBUFFER_WIDTH:
    case GraphicsContextGL::RENDERBUFFER_HEIGHT:
    case GraphicsContextGL::RENDERBUFFER_RED_SIZE:
    case GraphicsContextGL::RENDERBUFFER_GREEN_SIZE:
    case GraphicsContextGL::RENDERBUFFER_BLUE_SIZE:
    case GraphicsContextGL::RENDERBUFFER_ALPHA_SIZE:
    case GraphicsContextGL::RENDERBUFFER_DEPTH_SIZE:
    case GraphicsContextGL::RENDERBUFFER_STENCIL_SIZE:
        return m_context->getRenderbufferParameteri(target, pname);
    case GraphicsContextGL::RENDERBUFFER_INTERNAL_FORMAT:
        return m_renderbufferBinding->getInternalFormat();
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getRenderbufferParameter", "invalid parameter name");
        return nullptr;
    }
}

WebGLAny WebGLRenderingContextBase::getShaderParameter(WebGLShader& shader, GCGLenum pname)
{
    // COMPLETION_STATUS_KHR should always return true if the context is lost, so applications
    // don't get stuck in an infinite polling loop.
    if (isContextLost()) {
        if (pname == GraphicsContextGL::COMPLETION_STATUS_KHR)
            return true;
        return nullptr;
    }
    if (!validateWebGLProgramOrShader("getShaderParameter", &shader))
        return nullptr;

    switch (pname) {
    case GraphicsContextGL::DELETE_STATUS:
        return shader.isDeleted();
    case GraphicsContextGL::COMPILE_STATUS:
        return static_cast<bool>(m_context->getShaderi(shader.object(), pname));
    case GraphicsContextGL::SHADER_TYPE:
        return static_cast<unsigned>(m_context->getShaderi(shader.object(), pname));
    case GraphicsContextGL::COMPLETION_STATUS_KHR:
        if (m_khrParallelShaderCompile)
            return static_cast<bool>(m_context->getShaderi(shader.object(), pname));
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getShaderParameter", "KHR_parallel_shader_compile not enabled");
        return nullptr;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getShaderParameter", "invalid parameter name");
        return nullptr;
    }
}

String WebGLRenderingContextBase::getShaderInfoLog(WebGLShader& shader)
{
    if (!validateWebGLProgramOrShader("getShaderInfoLog", &shader))
        return String();
    return ensureNotNull(m_context->getShaderInfoLog(shader.object()));
}

RefPtr<WebGLShaderPrecisionFormat> WebGLRenderingContextBase::getShaderPrecisionFormat(GCGLenum shaderType, GCGLenum precisionType)
{
    if (isContextLost())
        return nullptr;
    switch (shaderType) {
    case GraphicsContextGL::VERTEX_SHADER:
    case GraphicsContextGL::FRAGMENT_SHADER:
        break;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getShaderPrecisionFormat", "invalid shader type");
        return nullptr;
    }
    switch (precisionType) {
    case GraphicsContextGL::LOW_FLOAT:
    case GraphicsContextGL::MEDIUM_FLOAT:
    case GraphicsContextGL::HIGH_FLOAT:
    case GraphicsContextGL::LOW_INT:
    case GraphicsContextGL::MEDIUM_INT:
    case GraphicsContextGL::HIGH_INT:
        break;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getShaderPrecisionFormat", "invalid precision type");
        return nullptr;
    }

    GCGLint range[2] { };
    GCGLint precision = 0;
    m_context->getShaderPrecisionFormat(shaderType, precisionType, range, &precision);
    return WebGLShaderPrecisionFormat::create(range[0], range[1], precision);
}

String WebGLRenderingContextBase::getShaderSource(WebGLShader& shader)
{
    if (!validateWebGLProgramOrShader("getShaderSource", &shader))
        return String();
    return ensureNotNull(shader.getSource());
}

WebGLAny WebGLRenderingContextBase::getTexParameter(GCGLenum target, GCGLenum pname)
{
    if (isContextLost())
        return nullptr;
    auto tex = validateTextureBinding("getTexParameter", target);
    if (!tex)
        return nullptr;
    switch (pname) {
    case GraphicsContextGL::TEXTURE_MAG_FILTER:
    case GraphicsContextGL::TEXTURE_MIN_FILTER:
    case GraphicsContextGL::TEXTURE_WRAP_S:
    case GraphicsContextGL::TEXTURE_WRAP_T:
        return static_cast<unsigned>(m_context->getTexParameteri(target, pname));
    case GraphicsContextGL::TEXTURE_MAX_ANISOTROPY_EXT: // EXT_texture_filter_anisotropic
        if (m_extTextureFilterAnisotropic)
            return m_context->getTexParameterf(target, pname);
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getTexParameter", "invalid parameter name, EXT_texture_filter_anisotropic not enabled");
        return nullptr;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getTexParameter", "invalid parameter name");
        return nullptr;
    }
}

WebGLAny WebGLRenderingContextBase::getUniform(WebGLProgram& program, const WebGLUniformLocation& uniformLocation)
{
    if (!validateWebGLProgramOrShader("getUniform", &program))
        return nullptr;
    if (uniformLocation.program() != &program) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "getUniform", "no uniformlocation or not valid for this program");
        return nullptr;
    }
    GCGLint location = uniformLocation.location();

    GCGLenum baseType;
    unsigned length;
    switch (uniformLocation.type()) {
    case GraphicsContextGL::BOOL:
        baseType = GraphicsContextGL::BOOL;
        length = 1;
        break;
    case GraphicsContextGL::BOOL_VEC2:
        baseType = GraphicsContextGL::BOOL;
        length = 2;
        break;
    case GraphicsContextGL::BOOL_VEC3:
        baseType = GraphicsContextGL::BOOL;
        length = 3;
        break;
    case GraphicsContextGL::BOOL_VEC4:
        baseType = GraphicsContextGL::BOOL;
        length = 4;
        break;
    case GraphicsContextGL::INT:
        baseType = GraphicsContextGL::INT;
        length = 1;
        break;
    case GraphicsContextGL::INT_VEC2:
        baseType = GraphicsContextGL::INT;
        length = 2;
        break;
    case GraphicsContextGL::INT_VEC3:
        baseType = GraphicsContextGL::INT;
        length = 3;
        break;
    case GraphicsContextGL::INT_VEC4:
        baseType = GraphicsContextGL::INT;
        length = 4;
        break;
    case GraphicsContextGL::FLOAT:
        baseType = GraphicsContextGL::FLOAT;
        length = 1;
        break;
    case GraphicsContextGL::FLOAT_VEC2:
        baseType = GraphicsContextGL::FLOAT;
        length = 2;
        break;
    case GraphicsContextGL::FLOAT_VEC3:
        baseType = GraphicsContextGL::FLOAT;
        length = 3;
        break;
    case GraphicsContextGL::FLOAT_VEC4:
        baseType = GraphicsContextGL::FLOAT;
        length = 4;
        break;
    case GraphicsContextGL::FLOAT_MAT2:
        baseType = GraphicsContextGL::FLOAT;
        length = 4;
        break;
    case GraphicsContextGL::FLOAT_MAT3:
        baseType = GraphicsContextGL::FLOAT;
        length = 9;
        break;
    case GraphicsContextGL::FLOAT_MAT4:
        baseType = GraphicsContextGL::FLOAT;
        length = 16;
        break;
    case GraphicsContextGL::SAMPLER_2D:
    case GraphicsContextGL::SAMPLER_CUBE:
        baseType = GraphicsContextGL::INT;
        length = 1;
        break;
    default:
        if (!isWebGL2()) {
            // Can't handle this type.
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "getUniform", "unhandled type");
            return nullptr;
        }
        switch (uniformLocation.type()) {
        case GraphicsContextGL::UNSIGNED_INT:
            baseType = GraphicsContextGL::UNSIGNED_INT;
            length = 1;
            break;
        case GraphicsContextGL::UNSIGNED_INT_VEC2:
            baseType = GraphicsContextGL::UNSIGNED_INT;
            length = 2;
            break;
        case GraphicsContextGL::UNSIGNED_INT_VEC3:
            baseType = GraphicsContextGL::UNSIGNED_INT;
            length = 3;
            break;
        case GraphicsContextGL::UNSIGNED_INT_VEC4:
            baseType = GraphicsContextGL::UNSIGNED_INT;
            length = 4;
            break;
        case GraphicsContextGL::FLOAT_MAT2x3:
            baseType = GraphicsContextGL::FLOAT;
            length = 6;
            break;
        case GraphicsContextGL::FLOAT_MAT2x4:
            baseType = GraphicsContextGL::FLOAT;
            length = 8;
            break;
        case GraphicsContextGL::FLOAT_MAT3x2:
            baseType = GraphicsContextGL::FLOAT;
            length = 6;
            break;
        case GraphicsContextGL::FLOAT_MAT3x4:
            baseType = GraphicsContextGL::FLOAT;
            length = 12;
            break;
        case GraphicsContextGL::FLOAT_MAT4x2:
            baseType = GraphicsContextGL::FLOAT;
            length = 8;
            break;
        case GraphicsContextGL::FLOAT_MAT4x3:
            baseType = GraphicsContextGL::FLOAT;
            length = 12;
            break;
        case GraphicsContextGL::SAMPLER_3D:
        case GraphicsContextGL::SAMPLER_2D_ARRAY:
        case GraphicsContextGL::SAMPLER_2D_SHADOW:
        case GraphicsContextGL::SAMPLER_CUBE_SHADOW:
        case GraphicsContextGL::SAMPLER_2D_ARRAY_SHADOW:
        case GraphicsContextGL::INT_SAMPLER_2D:
        case GraphicsContextGL::INT_SAMPLER_CUBE:
        case GraphicsContextGL::INT_SAMPLER_3D:
        case GraphicsContextGL::INT_SAMPLER_2D_ARRAY:
        case GraphicsContextGL::UNSIGNED_INT_SAMPLER_2D:
        case GraphicsContextGL::UNSIGNED_INT_SAMPLER_CUBE:
        case GraphicsContextGL::UNSIGNED_INT_SAMPLER_3D:
        case GraphicsContextGL::UNSIGNED_INT_SAMPLER_2D_ARRAY:
            baseType = GraphicsContextGL::INT;
            length = 1;
            break;
        default:
            // Can't handle this type.
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "getUniform", "unhandled type");
            return nullptr;
        }
    }
    switch (baseType) {
    case GraphicsContextGL::FLOAT: {
        GCGLfloat value[16] = {0};
        m_context->getUniformfv(program.object(), location, makeGCGLSpan(value, length));
        if (length == 1)
            return value[0];
        return Float32Array::tryCreate(value, length);
    }
    case GraphicsContextGL::INT: {
        GCGLint value[4] = {0};
        m_context->getUniformiv(program.object(), location, makeGCGLSpan(value, length));
        if (length == 1)
            return value[0];
        return Int32Array::tryCreate(value, length);
    }
    case GraphicsContextGL::UNSIGNED_INT: {
        GCGLuint value[4] = {0};
        m_context->getUniformuiv(program.object(), location, makeGCGLSpan(value, length));
        if (length == 1)
            return value[0];
        return Uint32Array::tryCreate(value, length);
    }
    case GraphicsContextGL::BOOL: {
        GCGLint value[4] = {0};
        m_context->getUniformiv(program.object(), location, makeGCGLSpan(value, length));
        if (length > 1) {
            Vector<bool> vector(length);
            for (unsigned j = 0; j < length; j++)
                vector[j] = value[j];
            return vector;
        }
        return static_cast<bool>(value[0]);
    }
    default:
        notImplemented();
    }

    // If we get here, something went wrong in our unfortunately complex logic above
    synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "getUniform", "unknown error");
    return nullptr;
}

RefPtr<WebGLUniformLocation> WebGLRenderingContextBase::getUniformLocation(WebGLProgram& program, const String& name)
{
    if (!validateWebGLProgramOrShader("getUniformLocation", &program))
        return nullptr;
    if (!validateLocationLength("getUniformLocation", name))
        return nullptr;
    if (!validateString("getUniformLocation", name))
        return nullptr;
    if (isPrefixReserved(name))
        return nullptr;
    if (!program.getLinkStatus()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "getUniformLocation", "program not linked");
        return nullptr;
    }
    GCGLint uniformLocation = m_context->getUniformLocation(program.object(), name);
    if (uniformLocation == -1)
        return nullptr;

    GCGLint activeUniforms = m_context->getProgrami(program.object(), GraphicsContextGL::ACTIVE_UNIFORMS);
    for (GCGLint i = 0; i < activeUniforms; i++) {
        GraphicsContextGLActiveInfo info;
        if (!m_context->getActiveUniform(program.object(), i, info))
            return nullptr;
        // Strip "[0]" from the name if it's an array.
        if (info.name.endsWith("[0]"_s))
            info.name = info.name.left(info.name.length() - 3);
        // If it's an array, we need to iterate through each element, appending "[index]" to the name.
        for (GCGLint index = 0; index < info.size; ++index) {
            auto uniformName = makeString(info.name, '[', index, ']');

            if (name == uniformName || name == info.name)
                return WebGLUniformLocation::create(&program, uniformLocation, info.type);
        }
    }
    return nullptr;
}

WebGLAny WebGLRenderingContextBase::getVertexAttrib(GCGLuint index, GCGLenum pname)
{
    if (isContextLost())
        return nullptr;

    if (index >= m_maxVertexAttribs) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "getVertexAttrib", "index out of range");
        return nullptr;
    }

    const WebGLVertexArrayObjectBase::VertexAttribState& state = m_boundVertexArrayObject->getVertexAttribState(index);

    if ((isWebGL2() || m_angleInstancedArrays) && pname == GraphicsContextGL::VERTEX_ATTRIB_ARRAY_DIVISOR_ANGLE)
        return state.divisor;

    if (isWebGL2() && pname == GraphicsContextGL::VERTEX_ATTRIB_ARRAY_INTEGER)
        return state.isInteger;

    switch (pname) {
    case GraphicsContextGL::VERTEX_ATTRIB_ARRAY_BUFFER_BINDING:
        return state.bufferBinding;
    case GraphicsContextGL::VERTEX_ATTRIB_ARRAY_ENABLED:
        return state.enabled;
    case GraphicsContextGL::VERTEX_ATTRIB_ARRAY_NORMALIZED:
        return state.normalized;
    case GraphicsContextGL::VERTEX_ATTRIB_ARRAY_SIZE:
        return state.size;
    case GraphicsContextGL::VERTEX_ATTRIB_ARRAY_STRIDE:
        return state.originalStride;
    case GraphicsContextGL::VERTEX_ATTRIB_ARRAY_TYPE:
        return state.type;
    case GraphicsContextGL::CURRENT_VERTEX_ATTRIB: {
        switch (m_vertexAttribValue[index].type) {
        case GraphicsContextGL::FLOAT:
            return Float32Array::tryCreate(m_vertexAttribValue[index].fValue, 4);
        case GraphicsContextGL::INT:
            return Int32Array::tryCreate(m_vertexAttribValue[index].iValue, 4);
        case GraphicsContextGL::UNSIGNED_INT:
            return Uint32Array::tryCreate(m_vertexAttribValue[index].uiValue, 4);
        default:
            ASSERT_NOT_REACHED();
            break;
        }
        return nullptr;
    }
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getVertexAttrib", "invalid parameter name");
        return nullptr;
    }
}

long long WebGLRenderingContextBase::getVertexAttribOffset(GCGLuint index, GCGLenum pname)
{
    if (isContextLost())
        return 0;
    return m_context->getVertexAttribOffset(index, pname);
}

// This function is used by InspectorCanvasAgent to list currently enabled extensions
bool WebGLRenderingContextBase::extensionIsEnabled(const String& name)
{
#define CHECK_EXTENSION(variable, nameLiteral) \
    if (equalIgnoringASCIICase(name, nameLiteral ## _s)) \
        return variable != nullptr;

    CHECK_EXTENSION(m_angleInstancedArrays, "ANGLE_instanced_arrays");
    CHECK_EXTENSION(m_extBlendMinMax, "EXT_blend_minmax");
    CHECK_EXTENSION(m_extColorBufferFloat, "EXT_color_buffer_float");
    CHECK_EXTENSION(m_extColorBufferHalfFloat, "EXT_color_buffer_half_float");
    CHECK_EXTENSION(m_extFloatBlend, "EXT_float_blend");
    CHECK_EXTENSION(m_extFragDepth, "EXT_frag_depth");
    CHECK_EXTENSION(m_extShaderTextureLOD, "EXT_shader_texture_lod");
    CHECK_EXTENSION(m_extTextureCompressionBPTC, "EXT_texture_compression_bptc");
    CHECK_EXTENSION(m_extTextureCompressionRGTC, "EXT_texture_compression_rgtc");
    CHECK_EXTENSION(m_extTextureFilterAnisotropic, "EXT_texture_filter_anisotropic");
    CHECK_EXTENSION(m_extTextureNorm16, "EXT_texture_norm16");
    CHECK_EXTENSION(m_extsRGB, "EXT_sRGB");
    CHECK_EXTENSION(m_khrParallelShaderCompile, "KHR_parallel_shader_compile");
    CHECK_EXTENSION(m_oesDrawBuffersIndexed, "OES_draw_buffers_indexed");
    CHECK_EXTENSION(m_oesElementIndexUint, "OES_element_index_uint");
    CHECK_EXTENSION(m_oesFBORenderMipmap, "OES_fbo_render_mipmap");
    CHECK_EXTENSION(m_oesStandardDerivatives, "OES_standard_derivatives");
    CHECK_EXTENSION(m_oesTextureFloat, "OES_texture_float");
    CHECK_EXTENSION(m_oesTextureFloatLinear, "OES_texture_float_linear");
    CHECK_EXTENSION(m_oesTextureHalfFloat, "OES_texture_half_float");
    CHECK_EXTENSION(m_oesTextureHalfFloatLinear, "OES_texture_half_float_linear");
    CHECK_EXTENSION(m_oesVertexArrayObject, "OES_vertex_array_object");
    CHECK_EXTENSION(m_webglClipCullDistance, "WEBGL_clip_cull_distance");
    CHECK_EXTENSION(m_webglColorBufferFloat, "WEBGL_color_buffer_float");
    CHECK_EXTENSION(m_webglCompressedTextureASTC, "WEBGL_compressed_texture_astc");
    CHECK_EXTENSION(m_webglCompressedTextureETC, "WEBGL_compressed_texture_etc");
    CHECK_EXTENSION(m_webglCompressedTextureETC1, "WEBGL_compressed_texture_etc1");
    CHECK_EXTENSION(m_webglCompressedTexturePVRTC, "WEBGL_compressed_texture_pvrtc");
    CHECK_EXTENSION(m_webglCompressedTexturePVRTC, "WEBKIT_WEBGL_compressed_texture_pvrtc");
    CHECK_EXTENSION(m_webglCompressedTextureS3TC, "WEBGL_compressed_texture_s3tc");
    CHECK_EXTENSION(m_webglCompressedTextureS3TCsRGB, "WEBGL_compressed_texture_s3tc_srgb");
    CHECK_EXTENSION(m_webglDebugRendererInfo, "WEBGL_debug_renderer_info");
    CHECK_EXTENSION(m_webglDebugShaders, "WEBGL_debug_shaders");
    CHECK_EXTENSION(m_webglDepthTexture, "WEBGL_depth_texture");
    CHECK_EXTENSION(m_webglDrawBuffers, "WEBGL_draw_buffers");
    CHECK_EXTENSION(m_webglDrawInstancedBaseVertexBaseInstance, "WEBGL_draw_instanced_base_vertex_base_instance");
    CHECK_EXTENSION(m_webglLoseContext, "WEBGL_lose_context");
    CHECK_EXTENSION(m_webglMultiDraw, "WEBGL_multi_draw");
    CHECK_EXTENSION(m_webglMultiDrawInstancedBaseVertexBaseInstance, "WEBGL_multi_draw_instanced_base_vertex_base_instance");
    CHECK_EXTENSION(m_webglProvokingVertex, "WEBGL_provoking_vertex");
    return false;
}

void WebGLRenderingContextBase::hint(GCGLenum target, GCGLenum mode)
{
    if (isContextLost())
        return;
    bool isValid = false;
    switch (target) {
    case GraphicsContextGL::GENERATE_MIPMAP_HINT:
        isValid = true;
        break;
    case GraphicsContextGL::FRAGMENT_SHADER_DERIVATIVE_HINT_OES: // OES_standard_derivatives, or core in WebGL 2.0
        if (m_oesStandardDerivatives || isWebGL2())
            isValid = true;
        break;
    }
    if (!isValid) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "hint", "invalid target");
        return;
    }
    m_context->hint(target, mode);
}

GCGLboolean WebGLRenderingContextBase::isBuffer(WebGLBuffer* buffer)
{
    if (!buffer || isContextLost() || !buffer->validate(contextGroup(), *this))
        return 0;

    if (!buffer->hasEverBeenBound())
        return 0;
    if (buffer->isDeleted())
        return 0;

    return m_context->isBuffer(buffer->object());
}

bool WebGLRenderingContextBase::isContextLost() const
{
    return m_contextLostState.has_value();
}

GCGLboolean WebGLRenderingContextBase::isEnabled(GCGLenum cap)
{
    if (isContextLost() || !validateCapability("isEnabled", cap))
        return 0;
    if (cap == GraphicsContextGL::STENCIL_TEST)
        return m_stencilEnabled;
    return m_context->isEnabled(cap);
}

GCGLboolean WebGLRenderingContextBase::isFramebuffer(WebGLFramebuffer* framebuffer)
{
    if (!framebuffer || isContextLost() || !framebuffer->validate(contextGroup(), *this))
        return 0;

    if (!framebuffer->hasEverBeenBound())
        return 0;
    if (framebuffer->isDeleted())
        return 0;

    return m_context->isFramebuffer(framebuffer->object());
}

GCGLboolean WebGLRenderingContextBase::isProgram(WebGLProgram* program)
{
    if (!program || isContextLost() || !program->validate(contextGroup(), *this))
        return 0;

    // OpenGL ES special-cases the behavior of program objects; if they're deleted
    // while attached to the current context state, glIsProgram is supposed to
    // still return true. For this reason, isDeleted is not checked here.

    return m_context->isProgram(program->object());
}

GCGLboolean WebGLRenderingContextBase::isRenderbuffer(WebGLRenderbuffer* renderbuffer)
{
    if (!renderbuffer || isContextLost() || !renderbuffer->validate(contextGroup(), *this))
        return 0;

    if (!renderbuffer->hasEverBeenBound())
        return 0;
    if (renderbuffer->isDeleted())
        return 0;

    return m_context->isRenderbuffer(renderbuffer->object());
}

GCGLboolean WebGLRenderingContextBase::isShader(WebGLShader* shader)
{
    if (!shader || isContextLost() || !shader->validate(contextGroup(), *this))
        return 0;

    // OpenGL ES special-cases the behavior of shader objects; if they're deleted
    // while attached to a program, glIsShader is supposed to still return true.
    // For this reason, isDeleted is not checked here.

    return m_context->isShader(shader->object());
}

GCGLboolean WebGLRenderingContextBase::isTexture(WebGLTexture* texture)
{
    if (!texture || isContextLost() || !texture->validate(contextGroup(), *this))
        return 0;

    if (!texture->hasEverBeenBound())
        return 0;
    if (texture->isDeleted())
        return 0;

    return m_context->isTexture(texture->object());
}

void WebGLRenderingContextBase::lineWidth(GCGLfloat width)
{
    if (isContextLost())
        return;
    m_context->lineWidth(width);
}

void WebGLRenderingContextBase::linkProgram(WebGLProgram& program)
{
    if (!linkProgramWithoutInvalidatingAttribLocations(&program))
        return;

    program.increaseLinkCount();
}

bool WebGLRenderingContextBase::linkProgramWithoutInvalidatingAttribLocations(WebGLProgram* program)
{
    if (!validateWebGLProgramOrShader("linkProgram", program))
        return false;
    m_context->linkProgram(objectOrZero(program));
    return true;
}

#if ENABLE(WEBXR)
// https://immersive-web.github.io/webxr/#dom-webglrenderingcontextbase-makexrcompatible
void WebGLRenderingContextBase::makeXRCompatible(MakeXRCompatiblePromise&& promise)
{
    // Returning an exception in these two checks is not part of the spec.
    auto canvas = htmlCanvas();
    if (!canvas) {
        m_isXRCompatible = false;
        promise.reject(Exception { InvalidStateError });
        return;
    }

    auto* window = canvas->document().domWindow();
    if (!window) {
        m_isXRCompatible = false;
        promise.reject(Exception { InvalidStateError });
        return;
    }

    // 1. Let promise be a new Promise.
    // 2. Let context be the target WebGLRenderingContextBase object.
    // 3. Ensure an immersive XR device is selected.
    auto& xrSystem = NavigatorWebXR::xr(window->navigator());
    xrSystem.ensureImmersiveXRDeviceIsSelected([this, protectedThis = Ref { *this }, promise = WTFMove(promise), protectedXrSystem = Ref { xrSystem }]() mutable {
        auto rejectPromiseWithInvalidStateError = makeScopeExit([&]() {
            m_isXRCompatible = false;
            promise.reject(Exception { InvalidStateError });
        });

        // 4. Set context’s XR compatible boolean as follows:
        //    If context’s WebGL context lost flag is set
        //      Set context’s XR compatible boolean to false and reject promise with an InvalidStateError.
        if (isContextLost())
            return;

        // If the immersive XR device is null
        //    Set context’s XR compatible boolean to false and reject promise with an InvalidStateError.
        if (!protectedXrSystem->hasActiveImmersiveXRDevice())
            return;

        // If context’s XR compatible boolean is true. Resolve promise.
        // If context was created on a compatible graphics adapter for the immersive XR device
        //  Set context’s XR compatible boolean to true and resolve promise.
        // Otherwise: Queue a task on the WebGL task source to perform the following steps:
        // FIXME: add a way to verify that we're using a compatible graphics adapter.
        m_isXRCompatible = true;

#if PLATFORM(COCOA) && !PLATFORM(IOS_FAMILY_SIMULATOR)
        enableSupportedExtension("GL_OES_EGL_image"_s);
#endif

        promise.resolve();
        rejectPromiseWithInvalidStateError.release();
    });
}
#endif

void WebGLRenderingContextBase::pixelStorei(GCGLenum pname, GCGLint param)
{
    if (isContextLost())
        return;
    switch (pname) {
    case GraphicsContextGL::UNPACK_FLIP_Y_WEBGL:
        m_unpackFlipY = param;
        break;
    case GraphicsContextGL::UNPACK_PREMULTIPLY_ALPHA_WEBGL:
        m_unpackPremultiplyAlpha = param;
        break;
    case GraphicsContextGL::UNPACK_COLORSPACE_CONVERSION_WEBGL:
        if (param == GraphicsContextGL::BROWSER_DEFAULT_WEBGL || param == GraphicsContextGL::NONE)
            m_unpackColorspaceConversion = static_cast<GCGLenum>(param);
        else {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "pixelStorei", "invalid parameter for UNPACK_COLORSPACE_CONVERSION_WEBGL");
            return;
        }
        break;
    case GraphicsContextGL::PACK_ALIGNMENT:
    case GraphicsContextGL::UNPACK_ALIGNMENT:
        if (param == 1 || param == 2 || param == 4 || param == 8) {
            if (pname == GraphicsContextGL::PACK_ALIGNMENT)
                m_packAlignment = param;
            else // GraphicsContextGL::UNPACK_ALIGNMENT:
                m_unpackAlignment = param;
            m_context->pixelStorei(pname, param);
        } else {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "pixelStorei", "invalid parameter for alignment");
            return;
        }
        break;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "pixelStorei", "invalid parameter name");
        return;
    }
}

void WebGLRenderingContextBase::polygonOffset(GCGLfloat factor, GCGLfloat units)
{
    if (isContextLost())
        return;
    m_context->polygonOffset(factor, units);
}

enum class InternalFormatTheme {
    None,
    NormalizedFixedPoint,
    Packed,
    SignedNormalizedFixedPoint,
    FloatingPoint,
    SignedInteger,
    UnsignedInteger
};

void WebGLRenderingContextBase::readPixels(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, RefPtr<ArrayBufferView>&& maybePixels)
{
    if (isContextLost())
        return;
    // Due to WebGL's same-origin restrictions, it is not possible to
    // taint the origin using the WebGL API.
    ASSERT(canvasBase().originClean());
    if (!maybePixels) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "readPixels", "no pixels");
        return;
    }
    ArrayBufferView& pixels = *maybePixels;

    // ANGLE will validate the readback from the framebuffer according
    // to WebGL's restrictions. At this level, just validate the type
    // of the readback against the typed array's type.
    if (!validateTypeAndArrayBufferType("readPixels", ArrayBufferViewFunctionType::ReadPixels, type, &pixels))
        return;

    clearIfComposited(CallerTypeOther);
    void* data = pixels.baseAddress();

    m_context->readnPixels(x, y, width, height, format, type, makeGCGLSpan(data, pixels.byteLength()));
}

void WebGLRenderingContextBase::renderbufferStorage(GCGLenum target, GCGLenum internalformat, GCGLsizei width, GCGLsizei height)
{
    const char* functionName = "renderbufferStorage";
    if (isContextLost())
        return;
    if (target != GraphicsContextGL::RENDERBUFFER) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid target");
        return;
    }
    if (!m_renderbufferBinding || !m_renderbufferBinding->object()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "no bound renderbuffer");
        return;
    }
    if (!validateSize(functionName, width, height))
        return;
    renderbufferStorageImpl(target, 0, internalformat, width, height, functionName);
}

void WebGLRenderingContextBase::renderbufferStorageImpl(GCGLenum target, GCGLsizei samples, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, const char* functionName)
{
#if ASSERT_ENABLED
    // |samples| > 0 is only valid in WebGL2's renderbufferStorageMultisample().
    ASSERT(!samples);
#else
    UNUSED_PARAM(samples);
#endif
    // Make sure this is overridden in WebGL 2.
    ASSERT(!isWebGL2());
    switch (internalformat) {
    case GraphicsContextGL::DEPTH_COMPONENT16:
    case GraphicsContextGL::RGBA4:
    case GraphicsContextGL::RGB5_A1:
    case GraphicsContextGL::RGB565:
    case GraphicsContextGL::STENCIL_INDEX8:
    case GraphicsContextGL::SRGB8_ALPHA8_EXT:
    case GraphicsContextGL::RGB16F:
    case GraphicsContextGL::RGBA16F:
    case GraphicsContextGL::RGBA32F:
        if (internalformat == GraphicsContextGL::SRGB8_ALPHA8_EXT && !m_extsRGB) {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "EXT_sRGB not enabled");
            return;
        }
        if ((internalformat == GraphicsContextGL::RGB16F || internalformat == GraphicsContextGL::RGBA16F) && !m_extColorBufferHalfFloat) {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "EXT_color_buffer_half_float not enabled");
            return;
        }
        if (internalformat == GraphicsContextGL::RGBA32F && !m_webglColorBufferFloat) {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "WEBGL_color_buffer_float not enabled");
            return;
        }
        m_context->renderbufferStorage(target, internalformat, width, height);
        m_renderbufferBinding->setInternalFormat(internalformat);
        m_renderbufferBinding->setIsValid(true);
        m_renderbufferBinding->setSize(width, height);
        break;
    case GraphicsContextGL::DEPTH_STENCIL:
        if (isDepthStencilSupported())
            m_context->renderbufferStorage(target, GraphicsContextGL::DEPTH24_STENCIL8, width, height);
        m_renderbufferBinding->setSize(width, height);
        m_renderbufferBinding->setIsValid(isDepthStencilSupported());
        m_renderbufferBinding->setInternalFormat(internalformat);
        break;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid internalformat");
        return;
    }
}

void WebGLRenderingContextBase::sampleCoverage(GCGLfloat value, GCGLboolean invert)
{
    if (isContextLost())
        return;
    m_context->sampleCoverage(value, invert);
}

void WebGLRenderingContextBase::scissor(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height)
{
    if (isContextLost())
        return;
    if (!validateSize("scissor", width, height))
        return;
    m_context->scissor(x, y, width, height);
}

void WebGLRenderingContextBase::shaderSource(WebGLShader& shader, const String& string)
{
    if (!validateWebGLProgramOrShader("shaderSource", &shader))
        return;
    m_context->shaderSource(shader.object(), string);
    shader.setSource(string);
}

void WebGLRenderingContextBase::stencilFunc(GCGLenum func, GCGLint ref, GCGLuint mask)
{
    if (isContextLost())
        return;
    if (!validateStencilFunc("stencilFunc", func))
        return;
    m_stencilFuncRef = ref;
    m_stencilFuncRefBack = ref;
    m_stencilFuncMask = mask;
    m_stencilFuncMaskBack = mask;
    m_context->stencilFunc(func, ref, mask);
}

void WebGLRenderingContextBase::stencilFuncSeparate(GCGLenum face, GCGLenum func, GCGLint ref, GCGLuint mask)
{
    if (isContextLost())
        return;
    if (!validateStencilFunc("stencilFuncSeparate", func))
        return;
    switch (face) {
    case GraphicsContextGL::FRONT_AND_BACK:
        m_stencilFuncRef = ref;
        m_stencilFuncRefBack = ref;
        m_stencilFuncMask = mask;
        m_stencilFuncMaskBack = mask;
        break;
    case GraphicsContextGL::FRONT:
        m_stencilFuncRef = ref;
        m_stencilFuncMask = mask;
        break;
    case GraphicsContextGL::BACK:
        m_stencilFuncRefBack = ref;
        m_stencilFuncMaskBack = mask;
        break;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "stencilFuncSeparate", "invalid face");
        return;
    }
    m_context->stencilFuncSeparate(face, func, ref, mask);
}

void WebGLRenderingContextBase::stencilMask(GCGLuint mask)
{
    if (isContextLost())
        return;
    m_stencilMask = mask;
    m_stencilMaskBack = mask;
    m_context->stencilMask(mask);
}

void WebGLRenderingContextBase::stencilMaskSeparate(GCGLenum face, GCGLuint mask)
{
    if (isContextLost())
        return;
    switch (face) {
    case GraphicsContextGL::FRONT_AND_BACK:
        m_stencilMask = mask;
        m_stencilMaskBack = mask;
        break;
    case GraphicsContextGL::FRONT:
        m_stencilMask = mask;
        break;
    case GraphicsContextGL::BACK:
        m_stencilMaskBack = mask;
        break;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "stencilMaskSeparate", "invalid face");
        return;
    }
    m_context->stencilMaskSeparate(face, mask);
}

void WebGLRenderingContextBase::stencilOp(GCGLenum fail, GCGLenum zfail, GCGLenum zpass)
{
    if (isContextLost())
        return;
    m_context->stencilOp(fail, zfail, zpass);
}

void WebGLRenderingContextBase::stencilOpSeparate(GCGLenum face, GCGLenum fail, GCGLenum zfail, GCGLenum zpass)
{
    if (isContextLost())
        return;
    m_context->stencilOpSeparate(face, fail, zfail, zpass);
}

IntRect WebGLRenderingContextBase::sentinelEmptyRect()
{
    // Return a rectangle with -1 width and height so we can recognize
    // it later and recalculate it based on the Image whose data we'll
    // upload. It's important that there be no possible differences in
    // the logic which computes the image's size.
    return IntRect(0, 0, -1, -1);
}

IntRect WebGLRenderingContextBase::safeGetImageSize(Image* image)
{
    if (!image)
        return IntRect();

    return getTextureSourceSize(image);
}

IntRect WebGLRenderingContextBase::getImageDataSize(ImageData* pixels)
{
    ASSERT(pixels);
    return getTextureSourceSize(pixels);
}

IntRect WebGLRenderingContextBase::getTexImageSourceSize(TexImageSource& source)
{
    auto visitor = WTF::makeVisitor([&](const RefPtr<ImageBitmap>& bitmap) -> ExceptionOr<IntRect> {
        return getTextureSourceSize(bitmap.get());
    }, [&](const RefPtr<ImageData>& pixels) -> ExceptionOr<IntRect> {
        return getTextureSourceSize(pixels.get());
    }, [&](const RefPtr<HTMLImageElement>& image) -> ExceptionOr<IntRect> {
        return getTextureSourceSize(image.get());
    }, [&](const RefPtr<HTMLCanvasElement>& canvas) -> ExceptionOr<IntRect> {
        return getTextureSourceSize(canvas.get());
#if ENABLE(VIDEO)
    }, [&](const RefPtr<HTMLVideoElement>& video) -> ExceptionOr<IntRect> {
        return IntRect(0, 0, video->videoWidth(), video->videoHeight());
#endif // ENABLE(VIDEO)
#if ENABLE(WEB_CODECS)
    }, [&](const RefPtr<WebCodecsVideoFrame>& frame) -> ExceptionOr<IntRect> {
        return IntRect(0, 0, frame->displayWidth(), frame->displayHeight());
#endif // ENABLE(WEB_CODECS)
    });

    ExceptionOr<IntRect> result = std::visit(visitor, source);
    if (result.hasException())
        return sentinelEmptyRect();
    return result.returnValue();
}

#if ENABLE(WEB_CODECS)
static bool isVideoFrameFormatEligibleToCopy(WebCodecsVideoFrame& frame)
{
#if PLATFORM(IOS_FAMILY)
    // FIXME: We should be able to remove the YUV restriction, see https://bugs.webkit.org/show_bug.cgi?id=251234.
    auto format = frame.format();
    return format && (*format == VideoPixelFormat::I420 || *format == VideoPixelFormat::NV12);
#else
    UNUSED_PARAM(frame);
    return true;
#endif
}
#endif // ENABLE(WEB_CODECS)

ExceptionOr<void> WebGLRenderingContextBase::texImageSourceHelper(TexImageFunctionID functionID, GCGLenum target, GCGLint level, GCGLint internalformat, GCGLint border, GCGLenum format, GCGLenum type, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, const IntRect& inputSourceImageRect, GCGLsizei depth, GCGLint unpackImageHeight, TexImageSource&& source)
{
    if (isContextLost())
        return { };

    const char* functionName = getTexImageFunctionName(functionID);
    TexImageFunctionType functionType;
    if (functionID == TexImageFunctionID::TexImage2D || functionID == TexImageFunctionID::TexImage3D)
        functionType = TexImageFunctionType::TexImage;
    else
        functionType = TexImageFunctionType::TexSubImage;

    auto visitor = WTF::makeVisitor([&](const RefPtr<ImageBitmap>& bitmap) -> ExceptionOr<void> {
        auto validationResult = validateImageBitmap(functionName, bitmap.get());
        if (validationResult.hasException())
            return validationResult.releaseException();
        auto texture = validateTexImageBinding(functionName, functionID, target);
        if (!texture)
            return { };
        IntRect sourceImageRect = inputSourceImageRect;
        if (sourceImageRect == sentinelEmptyRect()) {
            // Simply measure the input for WebGL 1.0, which doesn't support sub-rectangle selection.
            sourceImageRect = getTextureSourceSize(bitmap.get());
        }
        bool selectingSubRectangle = false;
        if (!validateTexImageSubRectangle(functionName, functionID, bitmap.get(), sourceImageRect, depth, unpackImageHeight, &selectingSubRectangle))
            return { };
        int width = sourceImageRect.width();
        int height = sourceImageRect.height();
        if (!validateTexFunc(functionName, functionType, SourceImageBitmap, target, level, internalformat, width, height, depth, border, format, type, xoffset, yoffset, zoffset))
            return { };

        ImageBuffer* buffer = bitmap->buffer();
        if (!buffer)
            return { };

        // Fallback pure SW path.
        RefPtr<Image> image = buffer->copyImage(DontCopyBackingStore);
        // The premultiplyAlpha and flipY pixel unpack parameters are ignored for ImageBitmaps.
        if (image)
            texImageImpl(functionID, target, level, internalformat, xoffset, yoffset, zoffset, format, type, image.get(), GraphicsContextGL::DOMSource::Image, false, bitmap->premultiplyAlpha(), bitmap->forciblyPremultiplyAlpha(), sourceImageRect, depth, unpackImageHeight);
        return { };
    }, [&](const RefPtr<ImageData>& pixels) -> ExceptionOr<void> {
        if (pixels->data().isDetached()) {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "The source data has been detached.");
            return { };
        }

        if (!validateTexImageBinding(functionName, functionID, target))
            return { };
        if (!validateTexFunc(functionName, functionType, SourceImageData, target, level, internalformat, pixels->width(), pixels->height(), depth, border, format, type, xoffset, yoffset, zoffset))
            return { };
        IntRect sourceImageRect = inputSourceImageRect;
        if (sourceImageRect == sentinelEmptyRect()) {
            // Simply measure the input for WebGL 1.0, which doesn't support sub-rectangle selection.
            sourceImageRect = getTextureSourceSize(pixels.get());
        }
        bool selectingSubRectangle = false;
        if (!validateTexImageSubRectangle(functionName, functionID, pixels.get(), sourceImageRect, depth, unpackImageHeight, &selectingSubRectangle))
            return { };
        // Adjust the source image rectangle if doing a y-flip.
        IntRect adjustedSourceImageRect = sourceImageRect;
        if (m_unpackFlipY)
            adjustedSourceImageRect.setY(pixels->height() - adjustedSourceImageRect.maxY());

        GCGLSpan<const GCGLvoid> imageData { pixels->data() };
        Vector<uint8_t> data;

        // The data from ImageData is always of format RGBA8.
        // No conversion is needed if destination format is RGBA and type is USIGNED_BYTE and no Flip or Premultiply operation is required.
        if (m_unpackFlipY || m_unpackPremultiplyAlpha || format != GraphicsContextGL::RGBA || type != GraphicsContextGL::UNSIGNED_BYTE || selectingSubRectangle || depth != 1) {
            if (type == GraphicsContextGL::UNSIGNED_INT_10F_11F_11F_REV) {
                // The UNSIGNED_INT_10F_11F_11F_REV type pack/unpack isn't implemented.
                type = GraphicsContextGL::FLOAT;
            }
            if (!m_context->extractPixelBuffer(pixels->pixelBuffer(), GraphicsContextGL::DataFormat::RGBA8, adjustedSourceImageRect, depth, unpackImageHeight, format, type, m_unpackFlipY, m_unpackPremultiplyAlpha, data)) {
                synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "texImage2D", "bad image data");
                return { };
            }
            imageData = GCGLSpan { data };
        }
        ScopedUnpackParametersResetRestore temporaryResetUnpack(this);
        if (functionID == TexImageFunctionID::TexImage2D) {
            texImage2DBase(target, level, internalformat,
                adjustedSourceImageRect.width(), adjustedSourceImageRect.height(), 0,
                format, type, imageData);
        } else if (functionID == TexImageFunctionID::TexSubImage2D) {
            texSubImage2DBase(target, level, xoffset, yoffset,
                adjustedSourceImageRect.width(), adjustedSourceImageRect.height(),
                format, format, type, imageData);
        } else {
            // 3D functions.
            if (functionID == TexImageFunctionID::TexImage3D) {
                m_context->texImage3D(target, level, internalformat,
                    adjustedSourceImageRect.width(), adjustedSourceImageRect.height(), depth, 0,
                    format, type, imageData);
            } else {
                ASSERT(functionID == TexImageFunctionID::TexSubImage3D);
                m_context->texSubImage3D(target, level, xoffset, yoffset, zoffset,
                    adjustedSourceImageRect.width(), adjustedSourceImageRect.height(), depth,
                    format, type, imageData);
            }
        }

        return { };
    }, [&](const RefPtr<HTMLImageElement>& image) -> ExceptionOr<void> {
        auto validationResult = validateHTMLImageElement(functionName, image.get());
        if (validationResult.hasException())
            return validationResult.releaseException();
        if (!validationResult.returnValue())
            return { };

        RefPtr<Image> imageForRender = image->cachedImage()->imageForRenderer(image->renderer());
        if (!imageForRender)
            return { };

        if (imageForRender->drawsSVGImage() || imageForRender->orientation() != ImageOrientation::Orientation::None || imageForRender->hasDensityCorrectedSize())
            imageForRender = drawImageIntoBuffer(*imageForRender, image->width(), image->height(), 1, functionName);

        if (!imageForRender || !validateTexFunc(functionName, functionType, SourceHTMLImageElement, target, level, internalformat, imageForRender->width(), imageForRender->height(), depth, border, format, type, xoffset, yoffset, zoffset))
            return { };

        // Pass along inputSourceImageRect unchanged. HTMLImageElements are unique in that their
        // size may differ from that of the Image obtained from them (because of devicePixelRatio),
        // so for WebGL 1.0 uploads, defer measuring their rectangle as long as possible.
        texImageImpl(functionID, target, level, internalformat, xoffset, yoffset, zoffset, format, type, imageForRender.get(), GraphicsContextGL::DOMSource::Image, m_unpackFlipY, m_unpackPremultiplyAlpha, false, inputSourceImageRect, depth, unpackImageHeight);
        return { };
    }, [&](const RefPtr<HTMLCanvasElement>& canvas) -> ExceptionOr<void> {
        auto validationResult = validateHTMLCanvasElement(functionName, canvas.get());
        if (validationResult.hasException())
            return validationResult.releaseException();
        if (!validationResult.returnValue())
            return { };
        auto texture = validateTexImageBinding(functionName, functionID, target);
        if (!texture)
            return { };
        IntRect sourceImageRect = inputSourceImageRect;
        if (sourceImageRect == sentinelEmptyRect()) {
            // Simply measure the input for WebGL 1.0, which doesn't support sub-rectangle selection.
            sourceImageRect = getTextureSourceSize(canvas.get());
        }
        if (!validateTexFunc(functionName, functionType, SourceHTMLCanvasElement, target, level, internalformat, sourceImageRect.width(), sourceImageRect.height(), depth, border, format, type, xoffset, yoffset, zoffset))
            return { };

        RefPtr<ImageData> imageData = canvas->getImageData();
        if (imageData)
            texImageSourceHelper(functionID, target, level, internalformat, border, format, type, xoffset, yoffset, zoffset, sourceImageRect, depth, unpackImageHeight, TexImageSource(imageData.get()));
        else
            texImageImpl(functionID, target, level, internalformat, xoffset, yoffset, zoffset, format, type, canvas->copiedImage(), GraphicsContextGL::DOMSource::Canvas, m_unpackFlipY, m_unpackPremultiplyAlpha, false, sourceImageRect, depth, unpackImageHeight);
        return { };
    }
#if ENABLE(VIDEO)
    , [&](const RefPtr<HTMLVideoElement>& video) -> ExceptionOr<void> {
        auto validationResult = validateHTMLVideoElement("texImage2D", video.get());
        if (validationResult.hasException())
            return validationResult.releaseException();
        if (!validationResult.returnValue())
            return { };
        auto texture = validateTexImageBinding(functionName, functionID, target);
        if (!texture)
            return { };
        if (!validateTexFunc(functionName, functionType, SourceHTMLVideoElement, target, level, internalformat, video->videoWidth(), video->videoHeight(), depth, border, format, type, xoffset, yoffset, zoffset))
            return { };
        if (!inputSourceImageRect.isValid()) {
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "source sub-rectangle specified via pixel unpack parameters is invalid");
            return { };
        }
        // Pass along inputSourceImageRect unchanged, including empty rectangles. Measure video
        // elements' size for WebGL 1.0 as late as possible.
        bool sourceImageRectIsDefault = inputSourceImageRect == sentinelEmptyRect() || inputSourceImageRect == IntRect(0, 0, video->videoWidth(), video->videoHeight());

#if PLATFORM(COCOA) && !HAVE(AVSAMPLEBUFFERDISPLAYLAYER_COPYDISPLAYEDPIXELBUFFER)
        if (auto player = video->player())
            player->willBeAskedToPaintGL();
#endif
        // Go through the fast path doing a GPU-GPU textures copy without a readback to system memory if possible.
        // Otherwise, it will fall back to the normal SW path.
        // FIXME: The current restrictions require that format shoud be RGB or RGBA,
        // type should be UNSIGNED_BYTE and level should be 0. It may be lifted in the future.
        if (functionID == TexImageFunctionID::TexImage2D && sourceImageRectIsDefault && texture
            && (format == GraphicsContextGL::RGB || format == GraphicsContextGL::RGBA)
            && type == GraphicsContextGL::UNSIGNED_BYTE
            && !level) {
            if (auto player = video->player()) {
                if (m_context->copyTextureFromMedia(*player, texture->object(), target, level, internalformat, format, type, m_unpackPremultiplyAlpha, m_unpackFlipY))
                    return { };
            }
        }

        // Fallback pure SW path.
        RefPtr<Image> image = videoFrameToImage(video.get(), DontCopyBackingStore, functionName);
        if (!image)
            return { };
        texImageImpl(functionID, target, level, internalformat, xoffset, yoffset, zoffset, format, type, image.get(), GraphicsContextGL::DOMSource::Video, m_unpackFlipY, m_unpackPremultiplyAlpha, false, inputSourceImageRect, depth, unpackImageHeight);
        return { };
    }
#endif // ENABLE(VIDEO)
#if ENABLE(WEB_CODECS)
    , [&](const RefPtr<WebCodecsVideoFrame>& frame) -> ExceptionOr<void> {
        if (frame->isDetached()) {
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "The video frame has been detached.");
            return { };
        }

        auto texture = validateTexImageBinding(functionName, functionID, target);
        if (!texture)
            return { };

        auto internalFrame = frame->internalFrame();

        // Go through the fast path doing a GPU-GPU textures copy without a readback to system memory if possible.
        // Otherwise, it will fall back to the normal SW path.
        // FIXME: The current restrictions require that format shoud be RGB or RGBA,
        // type should be UNSIGNED_BYTE and level should be 0. It may be lifted in the future.
        bool sourceImageRectIsDefault = inputSourceImageRect == sentinelEmptyRect() || inputSourceImageRect == IntRect(0, 0, static_cast<int>(internalFrame->presentationSize().width()), static_cast<int>(internalFrame->presentationSize().height()));
        if (isVideoFrameFormatEligibleToCopy(*frame) && functionID == TexImageFunctionID::TexImage2D && texture && (format == GraphicsContextGL::RGB || format == GraphicsContextGL::RGBA) && sourceImageRectIsDefault && type == GraphicsContextGL::UNSIGNED_BYTE && !level) {
            if (m_context->copyTextureFromVideoFrame(*internalFrame, texture->object(), target, level, internalformat, format, type, m_unpackPremultiplyAlpha, m_unpackFlipY))
                return { };
        }

        // Fallback pure SW path.
        auto image = m_context->videoFrameToImage(*internalFrame);
        if (!image)
            return { };

        texImageImpl(functionID, target, level, internalformat, xoffset, yoffset, zoffset, format, type, image.get(), GraphicsContextGL::DOMSource::Video, m_unpackFlipY, m_unpackPremultiplyAlpha, false, inputSourceImageRect, depth, unpackImageHeight);
        return { };
    }
#endif // ENABLE(WEB_CODECS)
    );

    return std::visit(visitor, source);
}

void WebGLRenderingContextBase::texImageArrayBufferViewHelper(TexImageFunctionID functionID, GCGLenum target, GCGLint level, GCGLint internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLint border, GCGLenum format, GCGLenum type, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, RefPtr<ArrayBufferView>&& pixels, NullDisposition nullDisposition, GCGLuint srcOffset)
{
    if (isContextLost())
        return;

    const char* functionName = getTexImageFunctionName(functionID);
    auto texture = validateTexImageBinding(functionName, functionID, target);
    if (!texture)
        return;

    auto functionType = (functionID == TexImageFunctionID::TexImage2D || functionID == TexImageFunctionID::TexImage3D) ? TexImageFunctionType::TexImage : TexImageFunctionType::TexSubImage;
    if (!validateTexFunc(functionName, functionType, SourceArrayBufferView, target, level, internalformat, width, height, depth, border, format, type, xoffset, yoffset, zoffset))
        return;

    auto sourceType = (functionID == TexImageFunctionID::TexImage2D || functionID == TexImageFunctionID::TexSubImage2D) ? TexImageDimension::Tex2D : TexImageDimension::Tex3D;
    auto data = validateTexFuncData(functionName, sourceType, width, height, depth, format, type, pixels.get(), nullDisposition, srcOffset);
    if (!data)
        return;

    Vector<uint8_t> tempData;
    bool changeUnpackParams = false;
    if (data->data() && width && height
        && (m_unpackFlipY || m_unpackPremultiplyAlpha)) {
        ASSERT(sourceType == TexImageDimension::Tex2D);
        // Only enter here if width or height is non-zero. Otherwise, call to the
        // underlying driver to generate appropriate GL errors if needed.
        PixelStoreParams unpackParams = getUnpackPixelStoreParams(TexImageDimension::Tex2D);
        GCGLint dataStoreWidth = unpackParams.rowLength ? unpackParams.rowLength : width;
        if (unpackParams.skipPixels + width > dataStoreWidth) {
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "Invalid unpack params combination.");
            return;
        }
        if (!m_context->extractTextureData(width, height, format, type, unpackParams, m_unpackFlipY, m_unpackPremultiplyAlpha, data.value(), tempData)) {
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "Invalid format/type combination.");
            return;
        }
        data.emplace(tempData);
        changeUnpackParams = true;
    }
    if (functionID == TexImageFunctionID::TexImage3D) {
        m_context->texImage3D(target, level, internalformat, width, height, depth, border, format, type, data.value());
        return;
    }
    if (functionID == TexImageFunctionID::TexSubImage3D) {
        m_context->texSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, data.value());
        return;
    }
    ScopedUnpackParametersResetRestore temporaryResetUnpack(this, changeUnpackParams);
    if (functionID == TexImageFunctionID::TexImage2D)
        texImage2DBase(target, level, internalformat, width, height, border, format, type, data.value());
    else {
        ASSERT(functionID == TexImageFunctionID::TexSubImage2D);
        texSubImage2DBase(target, level, xoffset, yoffset, width, height, format, format, type, data.value());
    }
}

void WebGLRenderingContextBase::texImageImpl(TexImageFunctionID functionID, GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, GCGLenum format, GCGLenum type, Image* image, GraphicsContextGL::DOMSource domSource, bool flipY, bool premultiplyAlpha, bool ignoreNativeImageAlphaPremultiplication, const IntRect& sourceImageRect, GCGLsizei depth, GCGLint unpackImageHeight)
{
    const char* functionName = getTexImageFunctionName(functionID);
    // All calling functions check isContextLost, so a duplicate check is not
    // needed here.
    if (type == GraphicsContextGL::UNSIGNED_INT_10F_11F_11F_REV) {
        // The UNSIGNED_INT_10F_11F_11F_REV type pack/unpack isn't implemented.
        type = GraphicsContextGL::FLOAT;
    }
    Vector<uint8_t> data;

    IntRect subRect = sourceImageRect;
    if (subRect.isValid() && subRect == sentinelEmptyRect()) {
        // Recalculate based on the size of the Image.
        subRect = safeGetImageSize(image);
    }

    bool selectingSubRectangle = false;
    if (!validateTexImageSubRectangle(functionName, functionID, image, subRect, depth, unpackImageHeight, &selectingSubRectangle))
        return;

    // Adjust the source image rectangle if doing a y-flip.
    IntRect adjustedSourceImageRect = subRect;
    if (m_unpackFlipY)
        adjustedSourceImageRect.setY(image->height() - adjustedSourceImageRect.maxY());

    GraphicsContextGLImageExtractor imageExtractor(image, domSource, premultiplyAlpha, m_unpackColorspaceConversion == GraphicsContextGL::NONE, ignoreNativeImageAlphaPremultiplication);
    if (!imageExtractor.extractSucceeded()) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "bad image data");
        return;
    }

    GraphicsContextGL::DataFormat sourceDataFormat = imageExtractor.imageSourceFormat();
    GraphicsContextGL::AlphaOp alphaOp = imageExtractor.imageAlphaOp();
    const void* imagePixelData = imageExtractor.imagePixelData();
    CheckedSize imagePixelByteLength(imageExtractor.imageWidth());
    imagePixelByteLength *= imageExtractor.imageHeight();
    imagePixelByteLength *= 4u;
    if (imagePixelByteLength.hasOverflowed()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "image too large");
        return;
    }

    GCGLSpan<const GCGLvoid> pixels { imagePixelData, imagePixelByteLength };
    if (type != GraphicsContextGL::UNSIGNED_BYTE || sourceDataFormat != GraphicsContextGL::DataFormat::RGBA8 || format != GraphicsContextGL::RGBA || alphaOp != GraphicsContextGL::AlphaOp::DoNothing || flipY || selectingSubRectangle || depth != 1) {
        if (!m_context->packImageData(image, imagePixelData, format, type, flipY, alphaOp, sourceDataFormat, imageExtractor.imageWidth(), imageExtractor.imageHeight(), adjustedSourceImageRect, depth, imageExtractor.imageSourceUnpackAlignment(), unpackImageHeight, data)) {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "packImage error");
            return;
        }
        pixels = GCGLSpan { data };
    }

    ScopedUnpackParametersResetRestore temporaryResetUnpack(this, true);
    if (functionID == TexImageFunctionID::TexImage2D) {
        texImage2DBase(target, level, internalformat,
            adjustedSourceImageRect.width(), adjustedSourceImageRect.height(), 0,
            format, type, pixels);
    } else if (functionID == TexImageFunctionID::TexSubImage2D) {
        texSubImage2DBase(target, level, xoffset, yoffset,
            adjustedSourceImageRect.width(), adjustedSourceImageRect.height(),
            format, format, type, pixels);
    } else {
        // 3D functions.
        if (functionID == TexImageFunctionID::TexImage3D) {
            m_context->texImage3D(target, level, internalformat,
                adjustedSourceImageRect.width(), adjustedSourceImageRect.height(), depth, 0,
                format, type, pixels);
        } else {
            ASSERT(functionID == TexImageFunctionID::TexSubImage3D);
            m_context->texSubImage3D(target, level, xoffset, yoffset, zoffset,
                adjustedSourceImageRect.width(), adjustedSourceImageRect.height(), depth,
                format, type, pixels);
        }
    }
}

void WebGLRenderingContextBase::texImage2DBase(GCGLenum target, GCGLint level, GCGLenum internalFormat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLenum format, GCGLenum type, GCGLSpan<const GCGLvoid> pixels)
{
    m_context->texImage2D(target, level, internalFormat, width, height, border, format, type, pixels);
}

void WebGLRenderingContextBase::texSubImage2DBase(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum internalFormat, GCGLenum format, GCGLenum type, GCGLSpan<const GCGLvoid> pixels)
{
    ASSERT(!isContextLost());
    UNUSED_PARAM(internalFormat);
    m_context->texSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
}

const char* WebGLRenderingContextBase::getTexImageFunctionName(TexImageFunctionID funcName)
{
    switch (funcName) {
    case TexImageFunctionID::TexImage2D:
        return "texImage2D";
    case TexImageFunctionID::TexSubImage2D:
        return "texSubImage2D";
    case TexImageFunctionID::TexSubImage3D:
        return "texSubImage3D";
    case TexImageFunctionID::TexImage3D:
        return "texImage3D";
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

bool WebGLRenderingContextBase::validateTexFunc(const char* functionName, TexImageFunctionType functionType, TexFuncValidationSourceType sourceType, GCGLenum target, GCGLint level, GCGLenum internalFormat, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLint border, GCGLenum format, GCGLenum type, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset)
{
    if (!validateTexFuncLevel(functionName, target, level))
        return false;

    if (!validateTexFuncParameters(functionName, functionType, sourceType, target, level, internalFormat, width, height, depth, border, format, type))
        return false;

    if (functionType == TexImageFunctionType::TexSubImage) {
        // Format suffices to validate this.
        if (!validateSettableTexInternalFormat(functionName, format))
            return false;
        if (!validateSize(functionName, xoffset, yoffset, zoffset))
            return false;
    } else {
        // For SourceArrayBufferView, function validateTexFuncData()
        // will handle whether to validate the SettableTexFormat by
        // checking if the ArrayBufferView is null or not.
        if (sourceType != SourceArrayBufferView) {
            if (!validateSettableTexInternalFormat(functionName, format))
                return false;
        }
    }
    return true;
}

void WebGLRenderingContextBase::texImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLenum format, GCGLenum type, RefPtr<ArrayBufferView>&& pixels)
{
    texImageArrayBufferViewHelper(TexImageFunctionID::TexImage2D, target, level, internalformat, width, height, 1, border, format, type, 0, 0, 0, WTFMove(pixels), NullAllowed, 0);
}

void WebGLRenderingContextBase::texSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, RefPtr<ArrayBufferView>&& pixels)
{
    texImageArrayBufferViewHelper(TexImageFunctionID::TexSubImage2D, target, level, 0, width, height, 1, 0, format, type, xoffset, yoffset, 0, WTFMove(pixels), NullNotAllowed, 0);
}

ExceptionOr<void> WebGLRenderingContextBase::texSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLenum format, GCGLenum type, std::optional<TexImageSource>&& source)
{
    if (isContextLost())
        return { };

    if (!source) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "texSubImage2D", "source is null");
        return { };
    }

    return texImageSourceHelper(TexImageFunctionID::TexSubImage2D, target, level, 0, 0, format, type, xoffset, yoffset, 0, sentinelEmptyRect(), 1, 0, WTFMove(*source));
}

bool WebGLRenderingContextBase::validateTypeAndArrayBufferType(const char* functionName, ArrayBufferViewFunctionType functionType, GCGLenum type, ArrayBufferView* pixels)
{
    JSC::TypedArrayType expectedArrayType = JSC::NotTypedArray;
    const char* error = "pixels is not null";
    switch (type) {
    case GraphicsContextGL::UNSIGNED_BYTE: {
        if (!pixels)
            return true;
        auto type = pixels->getType();
        if (type == JSC::TypeUint8 || type == JSC::TypeUint8Clamped)
            return true;
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "pixels is not TypeUint8 or TypeUint8Clamped");
        return false;
    }
    case GraphicsContextGL::BYTE:
        expectedArrayType = JSC::TypeInt8;
        error = "pixels is not TypeInt8";
        break;
    case GraphicsContextGL::UNSIGNED_SHORT:
    case GraphicsContextGL::UNSIGNED_SHORT_5_6_5:
    case GraphicsContextGL::UNSIGNED_SHORT_4_4_4_4:
    case GraphicsContextGL::UNSIGNED_SHORT_5_5_5_1:
        expectedArrayType = JSC::TypeUint16;
        error = "pixels is not TypeUint16";
        break;
    case GraphicsContextGL::SHORT:
        expectedArrayType = JSC::TypeInt16;
        error = "pixels is not TypeInt16";
        break;
    case GraphicsContextGL::UNSIGNED_INT_2_10_10_10_REV:
    case GraphicsContextGL::UNSIGNED_INT_10F_11F_11F_REV:
    case GraphicsContextGL::UNSIGNED_INT_5_9_9_9_REV:
    case GraphicsContextGL::UNSIGNED_INT_24_8:
    case GraphicsContextGL::UNSIGNED_INT:
        expectedArrayType = JSC::TypeUint32;
        error = "pixels is not TypeUint32";
        break;
    case GraphicsContextGL::INT:
        expectedArrayType = JSC::TypeInt32;
        error = "pixels is not TypeInt32";
        break;
    case GraphicsContextGL::FLOAT: // OES_texture_float
        expectedArrayType = JSC::TypeFloat32;
        error = "pixels is not TypeFloat32";
        break;
    case GraphicsContextGL::HALF_FLOAT_OES: // OES_texture_half_float
    case GraphicsContextGL::HALF_FLOAT:
        expectedArrayType = JSC::TypeUint16;
        error = "pixels is not TypeUint16";
        break;
    case GraphicsContextGL::FLOAT_32_UNSIGNED_INT_24_8_REV:
        if (functionType == ArrayBufferViewFunctionType::TexImage) {
            expectedArrayType = JSC::NotTypedArray;
            error = "type is FLOAT_32_UNSIGNED_INT_24_8_REV but pixels is not null";
            break;
        }
        ASSERT(functionType == ArrayBufferViewFunctionType::ReadPixels);
        FALLTHROUGH;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid type");
        return false;
    }

    if (!pixels)
        return true;

    if (expectedArrayType == JSC::NotTypedArray) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, error);
        return false;
    }
    if (pixels->getType() == expectedArrayType)
        return true;
    synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, error);
    return false;
}

std::optional<GCGLSpan<const GCGLvoid>> WebGLRenderingContextBase::validateTexFuncData(const char* functionName, TexImageDimension texDimension, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLenum format, GCGLenum type, ArrayBufferView* pixels, NullDisposition disposition, GCGLuint srcOffset)
{
    // All calling functions check isContextLost, so a duplicate check is not
    // needed here.
    if (!pixels && disposition != NullAllowed) {
        ASSERT(disposition != NullNotReachable);
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "no pixels");
        return std::nullopt;
    }

    // validateTexFuncFormatAndType handles validating the combination of internalformat, format and type.
    // validateSettableTexInternalFormat rejects initialize of combinations with pixel data that can't
    // accept anything other than null.
    if (pixels && !validateSettableTexInternalFormat(functionName, format))
        return std::nullopt;

    if (!validateTypeAndArrayBufferType(functionName, ArrayBufferViewFunctionType::TexImage, type, pixels))
        return std::nullopt;

    unsigned totalBytesRequired, skipBytes;
    GCGLenum error = m_context->computeImageSizeInBytes(format, type, width, height, depth, getUnpackPixelStoreParams(texDimension), &totalBytesRequired, nullptr, &skipBytes);
    if (error != GraphicsContextGL::NO_ERROR) {
        synthesizeGLError(error, functionName, "invalid texture dimensions");
        return std::nullopt;
    }

    auto dataLength = CheckedSize { totalBytesRequired } + skipBytes;
    auto offset = CheckedSize { pixels ? JSC::elementSize(pixels->getType()) : 0 } * srcOffset;
    auto total = offset + dataLength;
    if (total.hasOverflowed() || !isInBounds<GCGLsizei>(dataLength)) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "image too large");
        return std::nullopt;
    }

    if (!pixels)
        return std::make_optional<GCGLSpan<const GCGLvoid>>(nullptr, 0);

    if (pixels->byteLength() < total) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "ArrayBufferView not big enough for request");
        return std::nullopt;
    }
    ASSERT(!offset.hasOverflowed()); // Checked already as part of `total.hasOverflowed()` check.
    auto data = static_cast<uint8_t*>(pixels->baseAddress()) + offset.value();
    return std::make_optional<GCGLSpan<const GCGLvoid>>(data, static_cast<GCGLsizei>(dataLength));
}

bool WebGLRenderingContextBase::validateTexFuncParameters(const char* functionName,
    TexImageFunctionType functionType,
    TexFuncValidationSourceType sourceType,
    GCGLenum target, GCGLint level,
    GCGLenum internalformat,
    GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLint border,
    GCGLenum format, GCGLenum type)
{
    // We absolutely have to validate the format and type combination.
    // The texImage2D entry points taking HTMLImage, etc. will produce
    // temporary data based on this combination, so it must be legal.
    if (sourceType == SourceHTMLImageElement
        || sourceType == SourceHTMLCanvasElement
#if ENABLE(VIDEO)
        || sourceType == SourceHTMLVideoElement
#endif
        || sourceType == SourceImageData
        || sourceType == SourceImageBitmap) {
        if (!validateTexImageSourceFormatAndType(functionName, functionType, internalformat, format, type))
            return false;
    } else {
        if (!validateTexFuncFormatAndType(functionName, internalformat, format, type, level))
            return false;
    }

    if (width < 0 || height < 0 || depth < 0) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "width or height < 0");
        return false;
    }
    UNUSED_PARAM(target);
    UNUSED_PARAM(level);
    if (border) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "border != 0");
        return false;
    }
    return true;
}

void WebGLRenderingContextBase::addExtensionSupportedFormatsAndTypes()
{
    if (!m_areOESTextureFloatFormatsAndTypesAdded && m_oesTextureFloat) {
        ADD_VALUES_TO_SET(m_supportedTexImageSourceTypes, SupportedTypesOESTextureFloat);
        m_areOESTextureFloatFormatsAndTypesAdded = true;
    }

    if (!m_areOESTextureHalfFloatFormatsAndTypesAdded && m_oesTextureHalfFloat) {
        ADD_VALUES_TO_SET(m_supportedTexImageSourceTypes, SupportedTypesOESTextureHalfFloat);
        m_areOESTextureHalfFloatFormatsAndTypesAdded = true;
    }

    if (!m_areEXTsRGBFormatsAndTypesAdded && m_extsRGB) {
        ADD_VALUES_TO_SET(m_supportedTexImageSourceInternalFormats, SupportedInternalFormatsEXTsRGB);
        ADD_VALUES_TO_SET(m_supportedTexImageSourceFormats, SupportedFormatsEXTsRGB);
        m_areEXTsRGBFormatsAndTypesAdded = true;
    }
}

void WebGLRenderingContextBase::addExtensionSupportedFormatsAndTypesWebGL2()
{
    // FIXME: add EXT_texture_norm16_dom_source support.
}

bool WebGLRenderingContextBase::validateTexImageSourceFormatAndType(const char* functionName, TexImageFunctionType functionType, GCGLenum internalformat, GCGLenum format, GCGLenum type)
{
    if (!m_areWebGL2TexImageSourceFormatsAndTypesAdded && isWebGL2()) {
        ADD_VALUES_TO_SET(m_supportedTexImageSourceInternalFormats, SupportedInternalFormatsTexImageSourceES3);
        ADD_VALUES_TO_SET(m_supportedTexImageSourceFormats, SupportedFormatsTexImageSourceES3);
        ADD_VALUES_TO_SET(m_supportedTexImageSourceTypes, SupportedTypesTexImageSourceES3);
        m_areWebGL2TexImageSourceFormatsAndTypesAdded = true;
    }

    if (!isWebGL2())
        addExtensionSupportedFormatsAndTypes();
    else
        addExtensionSupportedFormatsAndTypesWebGL2();

    if (internalformat && !m_supportedTexImageSourceInternalFormats.contains(internalformat)) {
        if (functionType == TexImageFunctionType::TexImage)
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "invalid internalformat");
        else
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid internalformat");
        return false;
    }
    if (!m_supportedTexImageSourceFormats.contains(format)) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid format");
        return false;
    }
    if (!m_supportedTexImageSourceTypes.contains(type)) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid type");
        return false;
    }

    return true;
}

bool WebGLRenderingContextBase::validateTexFuncFormatAndType(const char* functionName, GCGLenum internalFormat, GCGLenum format, GCGLenum type, GCGLint level)
{
    switch (format) {
    case GraphicsContextGL::ALPHA:
    case GraphicsContextGL::LUMINANCE:
    case GraphicsContextGL::LUMINANCE_ALPHA:
    case GraphicsContextGL::RGB:
    case GraphicsContextGL::RGBA:
        break;
    case GraphicsContextGL::DEPTH_STENCIL:
    case GraphicsContextGL::DEPTH_COMPONENT:
        if (!m_webglDepthTexture && isWebGL1()) {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "depth texture formats not enabled");
            return false;
        }
        if (level > 0 && isWebGL1()) {
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "level must be 0 for depth formats");
            return false;
        }
        break;
    case GraphicsContextGL::SRGB_EXT:
    case GraphicsContextGL::SRGB_ALPHA_EXT:
        if (!m_extsRGB) {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "sRGB texture formats not enabled");
            return false;
        }
        break;
    default:
        if (!isWebGL1()) {
            switch (format) {
            case GraphicsContextGL::RED:
            case GraphicsContextGL::RED_INTEGER:
            case GraphicsContextGL::RG:
            case GraphicsContextGL::RG_INTEGER:
            case GraphicsContextGL::RGB_INTEGER:
            case GraphicsContextGL::RGBA_INTEGER:
                break;
            default:
                synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid texture format");
                return false;
            }
        } else {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid texture format");
            return false;
        }
    }

    switch (type) {
    case GraphicsContextGL::UNSIGNED_BYTE:
    case GraphicsContextGL::UNSIGNED_SHORT_5_6_5:
    case GraphicsContextGL::UNSIGNED_SHORT_4_4_4_4:
    case GraphicsContextGL::UNSIGNED_SHORT_5_5_5_1:
        break;
    case GraphicsContextGL::FLOAT:
        if (!m_oesTextureFloat && isWebGL1()) {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid texture type");
            return false;
        }
        break;
    case GraphicsContextGL::HALF_FLOAT:
    case GraphicsContextGL::HALF_FLOAT_OES:
        if (!m_oesTextureHalfFloat && isWebGL1()) {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid texture type");
            return false;
        }
        break;
    case GraphicsContextGL::UNSIGNED_INT:
    case GraphicsContextGL::UNSIGNED_INT_24_8:
    case GraphicsContextGL::UNSIGNED_SHORT:
        if (!m_webglDepthTexture && isWebGL1()) {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid texture type");
            return false;
        }
        break;
    default:
        if (!isWebGL1()) {
            switch (type) {
            case GraphicsContextGL::BYTE:
            case GraphicsContextGL::SHORT:
            case GraphicsContextGL::INT:
            case GraphicsContextGL::UNSIGNED_INT_2_10_10_10_REV:
            case GraphicsContextGL::UNSIGNED_INT_10F_11F_11F_REV:
            case GraphicsContextGL::UNSIGNED_INT_5_9_9_9_REV:
            case GraphicsContextGL::FLOAT_32_UNSIGNED_INT_24_8_REV:
                break;
            default:
                synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid texture type");
                return false;
            }
        } else {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid texture type");
            return false;
        }
    }

    if (!validateForbiddenInternalFormats(functionName, internalFormat))
        return false;
    return true;
}

bool WebGLRenderingContextBase::validateForbiddenInternalFormats(const char* functionName, GCGLenum internalformat)
{
    // These formats are never exposed to WebGL apps but may be accepted by ANGLE.
    switch (internalformat) {
    case GraphicsContextGL::BGRA4_ANGLEX:
    case GraphicsContextGL::BGR5_A1_ANGLEX:
    case GraphicsContextGL::BGRA8_SRGB_ANGLEX:
    case GraphicsContextGL::BGRA_EXT:
    case GraphicsContextGL::DEPTH_COMPONENT32_OES:
    case GraphicsContextGL::BGRA8_EXT:
    case GraphicsContextGL::RGBX8_ANGLE:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid internalformat");
        return false;
    }
    return true;
}

void WebGLRenderingContextBase::copyTexImage2D(GCGLenum target, GCGLint level, GCGLenum internalFormat, GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height, GCGLint border)
{
    if (isContextLost())
        return;
    if (!validateForbiddenInternalFormats("copyTexImage2D", internalFormat))
        return;
    if (!validateSettableTexInternalFormat("copyTexImage2D", internalFormat))
        return;
    auto tex = validateTexture2DBinding("copyTexImage2D", target);
    if (!tex)
        return;
    clearIfComposited(CallerTypeOther);
    m_context->copyTexImage2D(target, level, internalFormat, x, y, width, height, border);
}

ExceptionOr<void> WebGLRenderingContextBase::texImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLenum format, GCGLenum type, std::optional<TexImageSource> source)
{
    if (isContextLost())
        return { };

    if (!source) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "texImage2D", "source is null");
        return { };
    }

    return texImageSourceHelper(TexImageFunctionID::TexImage2D, target, level, internalformat, 0, format, type, 0, 0, 0, sentinelEmptyRect(), 1, 0, WTFMove(*source));
}

RefPtr<Image> WebGLRenderingContextBase::drawImageIntoBuffer(Image& image, int width, int height, int deviceScaleFactor, const char* functionName)
{
    IntSize size(width, height);
    size.scale(deviceScaleFactor);
    ImageBuffer* buf = m_generatedImageCache.imageBuffer(size, DestinationColorSpace::SRGB());
    if (!buf) {
        synthesizeGLError(GraphicsContextGL::OUT_OF_MEMORY, functionName, "out of memory");
        return nullptr;
    }

    FloatRect srcRect(FloatPoint(), image.size());
    FloatRect destRect(FloatPoint(), size);
    buf->context().drawImage(image, destRect, srcRect);
    return buf->copyImage(DontCopyBackingStore);
}

#if ENABLE(VIDEO)

RefPtr<Image> WebGLRenderingContextBase::videoFrameToImage(HTMLVideoElement* video, BackingStoreCopy backingStoreCopy, const char* functionName)
{
    ImageBuffer* imageBuffer = nullptr;
    // FIXME: When texImage2D is passed an HTMLVideoElement, implementations
    // interoperably use the native RGB color values of the video frame (e.g.
    // Rec.601 color space values) for the texture. But nativeImageForCurrentTime
    // and paintCurrentFrameInContext return and use an image with its color space
    // correctly matching the video.
    //
    // https://github.com/KhronosGroup/WebGL/issues/2165 is open on converting
    // the video element image source to sRGB instead of leaving it in its
    // native RGB color space. For now, we make sure to paint into an
    // ImageBuffer with a matching color space, to avoid the conversion.
#if USE(AVFOUNDATION)
    auto nativeImage = video->nativeImageForCurrentTime();
    // Currently we might be missing an image due to MSE not being able to provide the first requested frame.
    // https://bugs.webkit.org/show_bug.cgi?id=228997
    if (nativeImage) {
        IntSize imageSize = nativeImage->size();
        if (imageSize.isEmpty()) {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "video visible size is empty");
            return nullptr;
        }
        FloatRect imageRect { { }, imageSize };
        imageBuffer = m_generatedImageCache.imageBuffer(imageSize, nativeImage->colorSpace(), CompositeOperator::Copy);
        if (!imageBuffer) {
            synthesizeGLError(GraphicsContextGL::OUT_OF_MEMORY, functionName, "out of memory");
            return nullptr;
        }
        imageBuffer->context().drawNativeImage(*nativeImage, imageRect.size(), imageRect, imageRect, CompositeOperator::Copy);
    }
#endif
    if (!imageBuffer) {
        // This is a legacy code path that produces incompatible texture size when the
        // video visible size is different to the natural size. This should be removed
        // once all platforms implement nativeImageForCurrentTime().
        IntSize videoSize { static_cast<int>(video->videoWidth()), static_cast<int>(video->videoHeight()) };
        auto colorSpace = video->colorSpace();
        if (!colorSpace)
            colorSpace = DestinationColorSpace::SRGB();
        imageBuffer = m_generatedImageCache.imageBuffer(videoSize, *colorSpace);
        if (!imageBuffer) {
            synthesizeGLError(GraphicsContextGL::OUT_OF_MEMORY, functionName, "out of memory");
            return nullptr;
        }
        video->paintCurrentFrameInContext(imageBuffer->context(), { { }, videoSize });
    }
    RefPtr<Image> image = imageBuffer->copyImage(backingStoreCopy);
    if (!image) {
        synthesizeGLError(GraphicsContextGL::OUT_OF_MEMORY, functionName, "out of memory");
        return nullptr;
    }
    return image;
}

#endif

void WebGLRenderingContextBase::texParameter(GCGLenum target, GCGLenum pname, GCGLfloat paramf, GCGLint parami, bool isFloat)
{
    if (isContextLost())
        return;
    auto tex = validateTextureBinding("texParameter", target);
    if (!tex)
        return;
    switch (pname) {
    case GraphicsContextGL::TEXTURE_MIN_FILTER:
    case GraphicsContextGL::TEXTURE_MAG_FILTER:
        break;
    case GraphicsContextGL::TEXTURE_WRAP_R:
        if (!isWebGL2()) {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "texParameter", "invalid parameter name");
            return;
        }
        FALLTHROUGH;
    case GraphicsContextGL::TEXTURE_WRAP_S:
    case GraphicsContextGL::TEXTURE_WRAP_T:
        if ((isFloat && paramf != GraphicsContextGL::CLAMP_TO_EDGE && paramf != GraphicsContextGL::MIRRORED_REPEAT && paramf != GraphicsContextGL::REPEAT)
            || (!isFloat && parami != GraphicsContextGL::CLAMP_TO_EDGE && parami != GraphicsContextGL::MIRRORED_REPEAT && parami != GraphicsContextGL::REPEAT)) {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "texParameter", "invalid parameter");
            return;
        }
        break;
    case GraphicsContextGL::TEXTURE_MAX_ANISOTROPY_EXT: // EXT_texture_filter_anisotropic
        if (!m_extTextureFilterAnisotropic) {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "texParameter", "invalid parameter, EXT_texture_filter_anisotropic not enabled");
            return;
        }
        break;
    case GraphicsContextGL::TEXTURE_COMPARE_FUNC:
    case GraphicsContextGL::TEXTURE_COMPARE_MODE:
    case GraphicsContextGL::TEXTURE_BASE_LEVEL:
    case GraphicsContextGL::TEXTURE_MAX_LEVEL:
    case GraphicsContextGL::TEXTURE_MAX_LOD:
    case GraphicsContextGL::TEXTURE_MIN_LOD:
        if (!isWebGL2()) {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "texParameter", "invalid parameter name");
            return;
        }
        break;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "texParameter", "invalid parameter name");
        return;
    }
    if (isFloat)
        m_context->texParameterf(target, pname, paramf);
    else
        m_context->texParameteri(target, pname, parami);
}

void WebGLRenderingContextBase::texParameterf(GCGLenum target, GCGLenum pname, GCGLfloat param)
{
    texParameter(target, pname, param, 0, true);
}

void WebGLRenderingContextBase::texParameteri(GCGLenum target, GCGLenum pname, GCGLint param)
{
    texParameter(target, pname, 0, param, false);
}

bool WebGLRenderingContextBase::validateUniformLocation(const char* functionName, const WebGLUniformLocation* location)
{
    if (!location)
        return false;
    if (location->program() != m_currentProgram) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "location not for current program");
        return false;
    }
    return true;
}

void WebGLRenderingContextBase::uniform1f(const WebGLUniformLocation* location, GCGLfloat x)
{
    if (isContextLost() || !validateUniformLocation("uniform1f", location))
        return;

    m_context->uniform1f(location->location(), x);
}

void WebGLRenderingContextBase::uniform2f(const WebGLUniformLocation* location, GCGLfloat x, GCGLfloat y)
{
    if (isContextLost() || !validateUniformLocation("uniform2f", location))
        return;

    m_context->uniform2f(location->location(), x, y);
}

void WebGLRenderingContextBase::uniform3f(const WebGLUniformLocation* location, GCGLfloat x, GCGLfloat y, GCGLfloat z)
{
    if (isContextLost() || !validateUniformLocation("uniform3f", location))
        return;

    m_context->uniform3f(location->location(), x, y, z);
}

void WebGLRenderingContextBase::uniform4f(const WebGLUniformLocation* location, GCGLfloat x, GCGLfloat y, GCGLfloat z, GCGLfloat w)
{
    if (isContextLost() || !validateUniformLocation("uniform4f", location))
        return;

    m_context->uniform4f(location->location(), x, y, z, w);
}

void WebGLRenderingContextBase::uniform1i(const WebGLUniformLocation* location, GCGLint x)
{
    if (isContextLost() || !validateUniformLocation("uniform1i", location))
        return;
    m_context->uniform1i(location->location(), x);
}

void WebGLRenderingContextBase::uniform2i(const WebGLUniformLocation* location, GCGLint x, GCGLint y)
{
    if (isContextLost() || !validateUniformLocation("uniform2i", location))
        return;

    m_context->uniform2i(location->location(), x, y);
}

void WebGLRenderingContextBase::uniform3i(const WebGLUniformLocation* location, GCGLint x, GCGLint y, GCGLint z)
{
    if (isContextLost() || !validateUniformLocation("uniform3i", location))
        return;

    m_context->uniform3i(location->location(), x, y, z);
}

void WebGLRenderingContextBase::uniform4i(const WebGLUniformLocation* location, GCGLint x, GCGLint y, GCGLint z, GCGLint w)
{
    if (isContextLost() || !validateUniformLocation("uniform4i", location))
        return;

    m_context->uniform4i(location->location(), x, y, z, w);
}

void WebGLRenderingContextBase::uniform1fv(const WebGLUniformLocation* location, Float32List&& v)
{
    if (isContextLost())
        return;
    auto data = validateUniformParameters("uniform1fv", location, v, 1);
    if (!data)
        return;
    m_context->uniform1fv(location->location(), data.value());
}

void WebGLRenderingContextBase::uniform2fv(const WebGLUniformLocation* location, Float32List&& v)
{
    if (isContextLost())
        return;
    auto data = validateUniformParameters("uniform2fv", location, v, 2);
    if (!data)
        return;
    m_context->uniform2fv(location->location(), data.value());
}

void WebGLRenderingContextBase::uniform3fv(const WebGLUniformLocation* location, Float32List&& v)
{
    if (isContextLost())
        return;
    auto data = validateUniformParameters("uniform3fv", location, v, 3);
    if (!data)
        return;
    m_context->uniform3fv(location->location(), data.value());
}

void WebGLRenderingContextBase::uniform4fv(const WebGLUniformLocation* location, Float32List&& v)
{
    if (isContextLost())
        return;
    auto data = validateUniformParameters("uniform4fv", location, v, 4);
    if (!data)
        return;
    m_context->uniform4fv(location->location(), data.value());
}

void WebGLRenderingContextBase::uniform1iv(const WebGLUniformLocation* location, Int32List&& v)
{
    if (isContextLost())
        return;
    auto result = validateUniformParameters("uniform1iv", location, v, 1);
    if (!result)
        return;

    auto data = result.value();

    m_context->uniform1iv(location->location(), data);
}

void WebGLRenderingContextBase::uniform2iv(const WebGLUniformLocation* location, Int32List&& v)
{
    if (isContextLost())
        return;
    auto data = validateUniformParameters("uniform2iv", location, v, 2);
    if (!data)
        return;
    m_context->uniform2iv(location->location(), data.value());
}

void WebGLRenderingContextBase::uniform3iv(const WebGLUniformLocation* location, Int32List&& v)
{
    if (isContextLost())
        return;
    auto data = validateUniformParameters("uniform3iv", location, v, 3);
    if (!data)
        return;
    m_context->uniform3iv(location->location(), data.value());
}

void WebGLRenderingContextBase::uniform4iv(const WebGLUniformLocation* location, Int32List&& v)
{
    if (isContextLost())
        return;
    auto data = validateUniformParameters("uniform4iv", location, v, 4);
    if (!data)
        return;
    m_context->uniform4iv(location->location(), data.value());
}

void WebGLRenderingContextBase::uniformMatrix2fv(const WebGLUniformLocation* location, GCGLboolean transpose, Float32List&& v)
{
    if (isContextLost())
        return;
    auto data = validateUniformMatrixParameters("uniformMatrix2fv", location, transpose, v, 4);
    if (!data)
        return;
    m_context->uniformMatrix2fv(location->location(), transpose, data.value());
}

void WebGLRenderingContextBase::uniformMatrix3fv(const WebGLUniformLocation* location, GCGLboolean transpose, Float32List&& v)
{
    if (isContextLost())
        return;
    auto data = validateUniformMatrixParameters("uniformMatrix3fv", location, transpose, v, 9);
    if (!data)
        return;
    m_context->uniformMatrix3fv(location->location(), transpose, data.value());
}

void WebGLRenderingContextBase::uniformMatrix4fv(const WebGLUniformLocation* location, GCGLboolean transpose, Float32List&& v)
{
    if (isContextLost())
        return;
    auto data = validateUniformMatrixParameters("uniformMatrix4fv", location, transpose, v, 16);
    if (!data)
        return;
    m_context->uniformMatrix4fv(location->location(), transpose, data.value());
}

void WebGLRenderingContextBase::useProgram(WebGLProgram* program)
{
    Locker locker { objectGraphLock() };

    if (!validateNullableWebGLObject("useProgram", program))
        return;
    if (program && !program->getLinkStatus()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "useProgram", "program not valid");
        return;
    }

    // Extend the base useProgram method instead of overriding it in
    // WebGL2RenderingContext to keep the preceding validations in the same order.
    if (isWebGL2()) {
        ASSERT(is<WebGL2RenderingContext>(*this));
        if (downcast<WebGL2RenderingContext>(*this).isTransformFeedbackActiveAndNotPaused()) {
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "useProgram", "transform feedback is active and not paused");
            return;
        }
    }

    if (m_currentProgram != program) {
        if (m_currentProgram)
            m_currentProgram->onDetached(locker, graphicsContextGL());
        m_currentProgram = program;
        m_context->useProgram(objectOrZero(program));
        if (program)
            program->onAttached();
    }
}

void WebGLRenderingContextBase::validateProgram(WebGLProgram& program)
{
    if (!validateWebGLProgramOrShader("validateProgram", &program))
        return;
    m_context->validateProgram(program.object());
}

void WebGLRenderingContextBase::vertexAttrib1f(GCGLuint index, GCGLfloat v0)
{
    vertexAttribfImpl("vertexAttrib1f", index, 1, v0, 0.0f, 0.0f, 1.0f);
}

void WebGLRenderingContextBase::vertexAttrib2f(GCGLuint index, GCGLfloat v0, GCGLfloat v1)
{
    vertexAttribfImpl("vertexAttrib2f", index, 2, v0, v1, 0.0f, 1.0f);
}

void WebGLRenderingContextBase::vertexAttrib3f(GCGLuint index, GCGLfloat v0, GCGLfloat v1, GCGLfloat v2)
{
    vertexAttribfImpl("vertexAttrib3f", index, 3, v0, v1, v2, 1.0f);
}

void WebGLRenderingContextBase::vertexAttrib4f(GCGLuint index, GCGLfloat v0, GCGLfloat v1, GCGLfloat v2, GCGLfloat v3)
{
    vertexAttribfImpl("vertexAttrib4f", index, 4, v0, v1, v2, v3);
}

void WebGLRenderingContextBase::vertexAttrib1fv(GCGLuint index, Float32List&& v)
{
    vertexAttribfvImpl("vertexAttrib1fv", index, WTFMove(v), 1);
}

void WebGLRenderingContextBase::vertexAttrib2fv(GCGLuint index, Float32List&& v)
{
    vertexAttribfvImpl("vertexAttrib2fv", index, WTFMove(v), 2);
}

void WebGLRenderingContextBase::vertexAttrib3fv(GCGLuint index, Float32List&& v)
{
    vertexAttribfvImpl("vertexAttrib3fv", index, WTFMove(v), 3);
}

void WebGLRenderingContextBase::vertexAttrib4fv(GCGLuint index, Float32List&& v)
{
    vertexAttribfvImpl("vertexAttrib4fv", index, WTFMove(v), 4);
}

void WebGLRenderingContextBase::vertexAttribPointer(GCGLuint index, GCGLint size, GCGLenum type, GCGLboolean normalized, GCGLsizei stride, long long offset)
{
    Locker locker { objectGraphLock() };

    if (isContextLost())
        return;
    switch (type) {
    case GraphicsContextGL::BYTE:
    case GraphicsContextGL::UNSIGNED_BYTE:
    case GraphicsContextGL::SHORT:
    case GraphicsContextGL::UNSIGNED_SHORT:
    case GraphicsContextGL::FLOAT:
        break;
    default:
        if (!isWebGL2()) {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "vertexAttribPointer", "invalid type");
            return;
        }
        switch (type) {
        case GraphicsContextGL::INT:
        case GraphicsContextGL::UNSIGNED_INT:
        case GraphicsContextGL::HALF_FLOAT:
            break;
        case GraphicsContextGL::INT_2_10_10_10_REV:
        case GraphicsContextGL::UNSIGNED_INT_2_10_10_10_REV:
            if (size != 4) {
                synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "vertexAttribPointer", "[UNSIGNED_]INT_2_10_10_10_REV requires size 4");
                return;
            }
            break;
        default:
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "vertexAttribPointer", "invalid type");
            return;
        }
    }
    if (index >= m_maxVertexAttribs) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "vertexAttribPointer", "index out of range");
        return;
    }
    if (size < 1 || size > 4) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "vertexAttribPointer", "bad size");
        return;
    }
    if (stride < 0 || stride > 255) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "vertexAttribPointer", "bad stride");
        return;
    }
    if (offset < 0 || offset > std::numeric_limits<int32_t>::max()) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "vertexAttribPointer", "bad offset");
        return;
    }
    if (!m_boundArrayBuffer && offset) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "vertexAttribPointer", "no bound ARRAY_BUFFER");
        return;
    }
    // Determine the number of elements the bound buffer can hold, given the offset, size, type and stride
    auto typeSize = sizeInBytes(type);
    if (!typeSize) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "vertexAttribPointer", "invalid type");
        return;
    }
    if ((stride % typeSize) || (static_cast<GCGLintptr>(offset) % typeSize)) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "vertexAttribPointer", "stride or offset not valid for type");
        return;
    }
    GCGLsizei bytesPerElement = size * typeSize;
    m_boundVertexArrayObject->setVertexAttribState(locker, index, bytesPerElement, size, type, normalized, stride, static_cast<GCGLintptr>(offset), false, m_boundArrayBuffer.get());
    m_context->vertexAttribPointer(index, size, type, normalized, stride, static_cast<GCGLintptr>(offset));
}

void WebGLRenderingContextBase::viewport(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height)
{
    if (isContextLost())
        return;
    if (!validateSize("viewport", width, height))
        return;
    m_context->viewport(x, y, width, height);
}

void WebGLRenderingContextBase::forceLostContext(WebGLRenderingContextBase::LostContextMode mode)
{
    if (isContextLost()) {
        synthesizeLostContextGLError(GraphicsContextGL::INVALID_OPERATION, "loseContext", "context already lost");
        return;
    }

    m_contextGroup->loseContextGroup(mode);
}

void WebGLRenderingContextBase::loseContextImpl(WebGLRenderingContextBase::LostContextMode mode)
{
    if (isContextLost())
        return;
    if (mode == RealLostContext)
        printToConsole(MessageLevel::Error, "WebGL: context lost."_s);

    m_contextLostState = ContextLostState { mode };
    m_contextLostState->errors.add(GraphicsContextGL::CONTEXT_LOST_WEBGL);

    detachAndRemoveAllObjects();
    loseExtensions(mode);

    // There is no direct way to clear errors from a GL implementation and
    // looping until getError() becomes NO_ERROR might cause an infinite loop if
    // the driver or context implementation had a bug. So, loop a reasonably
    // large number of times to clear any existing errors.
    for (int i = 0; i < 100; ++i) {
        if (m_context->getError() == GraphicsContextGL::NO_ERROR)
            break;
    }

    // Always defer the dispatch of the context lost event, to implement
    // the spec behavior of queueing a task.
    scheduleTaskToDispatchContextLostEvent();
}

void WebGLRenderingContextBase::forceRestoreContext()
{
    if (!isContextLost()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "restoreContext", "context not lost");
        return;
    }
    if (!m_contextLostState->restoreRequested) {
        if (m_contextLostState->mode == SyntheticLostContext)
            synthesizeLostContextGLError(GraphicsContextGL::INVALID_OPERATION, "restoreContext", "context restoration not allowed");
        return;
    }

    if (!m_restoreTimer.isActive())
        m_restoreTimer.startOneShot(0_s);
}

bool WebGLRenderingContextBase::isContextUnrecoverablyLost() const
{
    return isContextLost() && !m_contextLostState->restoreRequested;
}

RefPtr<GraphicsLayerContentsDisplayDelegate> WebGLRenderingContextBase::layerContentsDisplayDelegate()
{
    if (isContextLost())
        return nullptr;
    return m_context->layerContentsDisplayDelegate();
}

void WebGLRenderingContextBase::removeSharedObject(WebGLSharedObject& object)
{
    m_contextGroup->removeObject(object);
}

void WebGLRenderingContextBase::addSharedObject(WebGLSharedObject& object)
{
    ASSERT(!isContextLost());
    m_contextGroup->addObject(object);
}

void WebGLRenderingContextBase::removeContextObject(WebGLContextObject& object)
{
    m_contextObjects.remove(&object);
}

void WebGLRenderingContextBase::addContextObject(WebGLContextObject& object)
{
    m_contextObjects.add(&object);
}

void WebGLRenderingContextBase::detachAndRemoveAllObjects()
{
    Locker locker { objectGraphLock() };

    while (m_contextObjects.size() > 0) {
        HashSet<WebGLContextObject*>::iterator it = m_contextObjects.begin();
        (*it)->detachContext(locker);
    }
}

void WebGLRenderingContextBase::stop()
{
    if (!isContextLost()) {
        forceLostContext(SyntheticLostContext);
        destroyGraphicsContextGL();
    }
}

const char* WebGLRenderingContextBase::activeDOMObjectName() const
{
    return "WebGLRenderingContext";
}

void WebGLRenderingContextBase::suspend(ReasonForSuspension)
{
    m_isSuspended = true;
}

void WebGLRenderingContextBase::resume()
{
    m_isSuspended = false;
}

bool WebGLRenderingContextBase::getBooleanParameter(GCGLenum pname)
{
    return m_context->getBoolean(pname);
}

Vector<bool> WebGLRenderingContextBase::getBooleanArrayParameter(GCGLenum pname)
{
    if (pname != GraphicsContextGL::COLOR_WRITEMASK) {
        notImplemented();
        return { };
    }
    GCGLboolean value[4] = { 0 };
    m_context->getBooleanv(pname, value);
    Vector<bool> vector(4);
    for (unsigned i = 0; i < 4; ++i)
        vector[i] = value[i];
    return vector;
}

float WebGLRenderingContextBase::getFloatParameter(GCGLenum pname)
{
    return m_context->getFloat(pname);
}

int WebGLRenderingContextBase::getIntParameter(GCGLenum pname)
{
    return m_context->getInteger(pname);
}

unsigned WebGLRenderingContextBase::getUnsignedIntParameter(GCGLenum pname)
{
    return m_context->getInteger(pname);
}

RefPtr<Float32Array> WebGLRenderingContextBase::getWebGLFloatArrayParameter(GCGLenum pname)
{
    GCGLfloat value[4] = {0};
    m_context->getFloatv(pname, value);
    unsigned length = 0;
    switch (pname) {
    case GraphicsContextGL::ALIASED_POINT_SIZE_RANGE:
    case GraphicsContextGL::ALIASED_LINE_WIDTH_RANGE:
    case GraphicsContextGL::DEPTH_RANGE:
        length = 2;
        break;
    case GraphicsContextGL::BLEND_COLOR:
    case GraphicsContextGL::COLOR_CLEAR_VALUE:
        length = 4;
        break;
    default:
        notImplemented();
    }
    return Float32Array::tryCreate(value, length);
}

RefPtr<Int32Array> WebGLRenderingContextBase::getWebGLIntArrayParameter(GCGLenum pname)
{
    GCGLint value[4] = {0};
    m_context->getIntegerv(pname, value);
    unsigned length = 0;
    switch (pname) {
    case GraphicsContextGL::MAX_VIEWPORT_DIMS:
        length = 2;
        break;
    case GraphicsContextGL::SCISSOR_BOX:
    case GraphicsContextGL::VIEWPORT:
        length = 4;
        break;
    default:
        notImplemented();
    }
    return Int32Array::tryCreate(value, length);
}

WebGLRenderingContextBase::PixelStoreParams WebGLRenderingContextBase::getPackPixelStoreParams() const
{
    PixelStoreParams params;
    params.alignment = m_packAlignment;
    return params;
}

WebGLRenderingContextBase::PixelStoreParams WebGLRenderingContextBase::getUnpackPixelStoreParams(TexImageDimension dimension) const
{
    UNUSED_PARAM(dimension);
    PixelStoreParams params;
    params.alignment = m_unpackAlignment;
    return params;
}

RefPtr<WebGLTexture> WebGLRenderingContextBase::validateTextureBinding(const char* functionName, GCGLenum target)
{
    RefPtr<WebGLTexture> texture;
    switch (target) {
    case GraphicsContextGL::TEXTURE_2D:
        texture = m_textureUnits[m_activeTextureUnit].texture2DBinding;
        break;
    case GraphicsContextGL::TEXTURE_CUBE_MAP:
        texture = m_textureUnits[m_activeTextureUnit].textureCubeMapBinding;
        break;
    case GraphicsContextGL::TEXTURE_3D:
        if (!isWebGL2()) {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid texture target");
            return nullptr;
        }
        texture = m_textureUnits[m_activeTextureUnit].texture3DBinding;
        break;
    case GraphicsContextGL::TEXTURE_2D_ARRAY:
        if (!isWebGL2()) {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid texture target");
            return nullptr;
        }
        texture = m_textureUnits[m_activeTextureUnit].texture2DArrayBinding;
        break;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid texture target");
        return nullptr;
    }
    if (!texture)
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "no texture");
    return texture;
}

RefPtr<WebGLTexture> WebGLRenderingContextBase::validateTexImageBinding(const char* functionName, TexImageFunctionID functionID, GCGLenum target)
{
    UNUSED_PARAM(functionID);
    return validateTexture2DBinding(functionName, target);
}

RefPtr<WebGLTexture> WebGLRenderingContextBase::validateTexture2DBinding(const char* functionName, GCGLenum target)
{
    RefPtr<WebGLTexture> texture;
    switch (target) {
    case GraphicsContextGL::TEXTURE_2D:
        texture = m_textureUnits[m_activeTextureUnit].texture2DBinding;
        break;
    case GraphicsContextGL::TEXTURE_CUBE_MAP_POSITIVE_X:
    case GraphicsContextGL::TEXTURE_CUBE_MAP_NEGATIVE_X:
    case GraphicsContextGL::TEXTURE_CUBE_MAP_POSITIVE_Y:
    case GraphicsContextGL::TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case GraphicsContextGL::TEXTURE_CUBE_MAP_POSITIVE_Z:
    case GraphicsContextGL::TEXTURE_CUBE_MAP_NEGATIVE_Z:
        texture = m_textureUnits[m_activeTextureUnit].textureCubeMapBinding;
        break;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid texture target");
        return nullptr;
    }
    if (!texture)
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "no texture");
    return texture;
}

bool WebGLRenderingContextBase::validateLocationLength(const char* functionName, const String& string)
{
    unsigned maxWebGLLocationLength = isWebGL2() ? 1024 : 256;
    if (string.length() > maxWebGLLocationLength) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "location length is too large");
        return false;
    }
    return true;
}

bool WebGLRenderingContextBase::validateSize(const char* functionName, GCGLint x, GCGLint y, GCGLint z)
{
    if (x < 0 || y < 0 || z < 0) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "size < 0");
        return false;
    }
    return true;
}

bool WebGLRenderingContextBase::validateString(const char* functionName, const String& string)
{
    for (size_t i = 0; i < string.length(); ++i) {
        if (!validateCharacter(string[i])) {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "string not ASCII");
            return false;
        }
    }
    return true;
}

bool WebGLRenderingContextBase::validateTexFuncLevel(const char* functionName, GCGLenum target, GCGLint level)
{
    if (level < 0) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "level < 0");
        return false;
    }
    GCGLint maxLevel = maxTextureLevelForTarget(target);
    if (maxLevel && level >= maxLevel) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "level out of range");
        return false;
    }
    // This function only checks if level is legal, so we return true and don't
    // generate INVALID_ENUM if target is illegal.
    return true;
}

GCGLint WebGLRenderingContextBase::maxTextureLevelForTarget(GCGLenum target)
{
    switch (target) {
    case GraphicsContextGL::TEXTURE_2D:
        return m_maxTextureLevel;
    case GraphicsContextGL::TEXTURE_CUBE_MAP_POSITIVE_X:
    case GraphicsContextGL::TEXTURE_CUBE_MAP_NEGATIVE_X:
    case GraphicsContextGL::TEXTURE_CUBE_MAP_POSITIVE_Y:
    case GraphicsContextGL::TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case GraphicsContextGL::TEXTURE_CUBE_MAP_POSITIVE_Z:
    case GraphicsContextGL::TEXTURE_CUBE_MAP_NEGATIVE_Z:
        return m_maxCubeMapTextureLevel;
    }
    return 0;
}

bool WebGLRenderingContextBase::validateCompressedTexFormat(const char* functionName, GCGLenum format)
{
    if (!m_compressedTextureFormats.contains(format)) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid format");
        return false;
    }
    return true;
}

bool WebGLRenderingContextBase::validateDrawMode(const char* functionName, GCGLenum mode)
{
    switch (mode) {
    case GraphicsContextGL::POINTS:
    case GraphicsContextGL::LINE_STRIP:
    case GraphicsContextGL::LINE_LOOP:
    case GraphicsContextGL::LINES:
    case GraphicsContextGL::TRIANGLE_STRIP:
    case GraphicsContextGL::TRIANGLE_FAN:
    case GraphicsContextGL::TRIANGLES:
        return true;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid draw mode");
        return false;
    }
}

bool WebGLRenderingContextBase::validateStencilSettings(const char* functionName)
{
    if (m_stencilMask != m_stencilMaskBack || m_stencilFuncRef != m_stencilFuncRefBack || m_stencilFuncMask != m_stencilFuncMaskBack) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "front and back stencils settings do not match");
        return false;
    }
    return true;
}

bool WebGLRenderingContextBase::validateStencilFunc(const char* functionName, GCGLenum func)
{
    switch (func) {
    case GraphicsContextGL::NEVER:
    case GraphicsContextGL::LESS:
    case GraphicsContextGL::LEQUAL:
    case GraphicsContextGL::GREATER:
    case GraphicsContextGL::GEQUAL:
    case GraphicsContextGL::EQUAL:
    case GraphicsContextGL::NOTEQUAL:
    case GraphicsContextGL::ALWAYS:
        return true;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid function");
        return false;
    }
}

void WebGLRenderingContextBase::printToConsole(MessageLevel level, const String& message)
{
    if (!m_synthesizedErrorsToConsole || !m_numGLErrorsToConsoleAllowed)
        return;

    std::unique_ptr<Inspector::ConsoleMessage> consoleMessage;

    // Error messages can occur during function calls, so show stack traces for them.
    if (level == MessageLevel::Error) {
        Ref<Inspector::ScriptCallStack> stackTrace = Inspector::createScriptCallStack(JSExecState::currentState());
        consoleMessage = makeUnique<Inspector::ConsoleMessage>(MessageSource::Rendering, MessageType::Log, level, message, WTFMove(stackTrace));
    } else
        consoleMessage = makeUnique<Inspector::ConsoleMessage>(MessageSource::Rendering, MessageType::Log, level, message);

    auto* canvas = htmlCanvas();
    if (canvas)
        canvas->document().addConsoleMessage(WTFMove(consoleMessage));

    --m_numGLErrorsToConsoleAllowed;
    if (!m_numGLErrorsToConsoleAllowed)
        printToConsole(MessageLevel::Warning, "WebGL: too many errors, no more errors will be reported to the console for this context."_s);
}

bool WebGLRenderingContextBase::validateFramebufferTarget(GCGLenum target)
{
    if (target == GraphicsContextGL::FRAMEBUFFER)
        return true;
    return false;
}

WebGLFramebuffer* WebGLRenderingContextBase::getFramebufferBinding(GCGLenum target)
{
    if (target == GraphicsContextGL::FRAMEBUFFER)
        return m_framebufferBinding.get();
    return nullptr;
}

WebGLFramebuffer* WebGLRenderingContextBase::getReadFramebufferBinding()
{
    return m_framebufferBinding.get();
}

bool WebGLRenderingContextBase::validateFramebufferFuncParameters(const char* functionName, GCGLenum target, GCGLenum attachment)
{
    if (!validateFramebufferTarget(target)) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid target");
        return false;
    }
    // This rejects attempts to set COLOR_ATTACHMENT > 0 if the functionality for multiple color
    // attachments is not enabled, either through the WEBGL_draw_buffers extension or availability
    // of WebGL 2.0.
    switch (attachment) {
    case GraphicsContextGL::COLOR_ATTACHMENT0:
    case GraphicsContextGL::DEPTH_ATTACHMENT:
    case GraphicsContextGL::STENCIL_ATTACHMENT:
    case GraphicsContextGL::DEPTH_STENCIL_ATTACHMENT:
        return true;
    default:
        if ((m_webglDrawBuffers || isWebGL2())
            && attachment > GraphicsContextGL::COLOR_ATTACHMENT0
            && attachment < static_cast<GCGLenum>(GraphicsContextGL::COLOR_ATTACHMENT0 + getMaxColorAttachments()))
            return true;
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid attachment");
        return false;
    }
}

bool WebGLRenderingContextBase::validateBlendFuncFactors(const char* functionName, GCGLenum src, GCGLenum dst)
{
    if (((src == GraphicsContextGL::CONSTANT_COLOR || src == GraphicsContextGL::ONE_MINUS_CONSTANT_COLOR)
        && (dst == GraphicsContextGL::CONSTANT_ALPHA || dst == GraphicsContextGL::ONE_MINUS_CONSTANT_ALPHA))
        || ((dst == GraphicsContextGL::CONSTANT_COLOR || dst == GraphicsContextGL::ONE_MINUS_CONSTANT_COLOR)
            && (src == GraphicsContextGL::CONSTANT_ALPHA || src == GraphicsContextGL::ONE_MINUS_CONSTANT_ALPHA))) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "incompatible src and dst");
        return false;
    }
    return true;
}

bool WebGLRenderingContextBase::validateCapability(const char* functionName, GCGLenum cap)
{
    switch (cap) {
    case GraphicsContextGL::BLEND:
    case GraphicsContextGL::CULL_FACE:
    case GraphicsContextGL::DEPTH_TEST:
    case GraphicsContextGL::DITHER:
    case GraphicsContextGL::POLYGON_OFFSET_FILL:
    case GraphicsContextGL::SAMPLE_ALPHA_TO_COVERAGE:
    case GraphicsContextGL::SAMPLE_COVERAGE:
    case GraphicsContextGL::SCISSOR_TEST:
    case GraphicsContextGL::STENCIL_TEST:
        return true;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid capability");
        return false;
    }
}

template<typename T, typename TypedListType>
std::optional<GCGLSpan<const T>> WebGLRenderingContextBase::validateUniformMatrixParameters(const char* functionName, const WebGLUniformLocation* location, GCGLboolean transpose, const TypedList<TypedListType, T>& values, GCGLsizei requiredMinSize, GCGLuint srcOffset, GCGLuint srcLength)
{
    if (!validateUniformLocation(functionName, location))
        return { };
    if (!values.data()) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "no array");
        return { };
    }
    if (transpose && !isWebGL2()) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "transpose not FALSE");
        return { };
    }
    if (srcOffset >= static_cast<GCGLuint>(values.length())) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "invalid srcOffset");
        return { };
    }
    GCGLsizei actualSize = values.length() - srcOffset;
    if (srcLength > 0) {
        if (srcLength > static_cast<GCGLuint>(actualSize)) {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "invalid srcOffset + srcLength");
            return { };
        }
        actualSize = srcLength;
    }
    if (actualSize < requiredMinSize || (actualSize % requiredMinSize)) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "invalid size");
        return { };
    }
    return makeGCGLSpan(values.data() + srcOffset, actualSize);
}

template
std::optional<GCGLSpan<const GCGLuint>> WebGLRenderingContextBase::validateUniformMatrixParameters<GCGLuint, Uint32Array>(const char* functionName, const WebGLUniformLocation*, GCGLboolean transpose, const TypedList<Uint32Array, uint32_t>& values, GCGLsizei requiredMinSize, GCGLuint srcOffset, GCGLuint srcLength);

template
std::optional<GCGLSpan<const GCGLint>> WebGLRenderingContextBase::validateUniformMatrixParameters<GCGLint, Int32Array>(const char* functionName, const WebGLUniformLocation*, GCGLboolean transpose, const TypedList<Int32Array, int32_t>& values, GCGLsizei requiredMinSize, GCGLuint srcOffset, GCGLuint srcLength);

template
std::optional<GCGLSpan<const GCGLfloat>> WebGLRenderingContextBase::validateUniformMatrixParameters<GCGLfloat, Float32Array>(const char* functionName, const WebGLUniformLocation*, GCGLboolean transpose, const TypedList<Float32Array, float>& values, GCGLsizei requiredMinSize, GCGLuint srcOffset, GCGLuint srcLength);

WebGLBuffer* WebGLRenderingContextBase::validateBufferDataParameters(const char* functionName, GCGLenum target, GCGLenum usage)
{
    WebGLBuffer* buffer = validateBufferDataTarget(functionName, target);
    if (!buffer)
        return nullptr;
    switch (usage) {
    case GraphicsContextGL::STREAM_DRAW:
    case GraphicsContextGL::STATIC_DRAW:
    case GraphicsContextGL::DYNAMIC_DRAW:
        return buffer;
    case GraphicsContextGL::STREAM_COPY:
    case GraphicsContextGL::STATIC_COPY:
    case GraphicsContextGL::DYNAMIC_COPY:
    case GraphicsContextGL::STREAM_READ:
    case GraphicsContextGL::STATIC_READ:
    case GraphicsContextGL::DYNAMIC_READ:
        if (isWebGL2())
            return buffer;
    }
    synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid usage");
    return nullptr;
}

ExceptionOr<bool> WebGLRenderingContextBase::validateHTMLImageElement(const char* functionName, HTMLImageElement* image)
{
    if (!image || !image->cachedImage()) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "no image");
        return false;
    }
    const URL& url = image->cachedImage()->response().url();
    if (url.isNull() || url.isEmpty() || !url.isValid()) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "invalid image");
        return false;
    }
    if (taintsOrigin(image))
        return Exception { SecurityError };
    return true;
}

ExceptionOr<bool> WebGLRenderingContextBase::validateHTMLCanvasElement(const char* functionName, HTMLCanvasElement* canvas)
{
    if (!canvas || !canvas->buffer()) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "no canvas");
        return false;
    }
    if (taintsOrigin(canvas))
        return Exception { SecurityError };
    return true;
}

#if ENABLE(VIDEO)

ExceptionOr<bool> WebGLRenderingContextBase::validateHTMLVideoElement(const char* functionName, HTMLVideoElement* video)
{
    if (!video || !video->videoWidth() || !video->videoHeight()) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "no video");
        return false;
    }
    if (taintsOrigin(video))
        return Exception { SecurityError };
    return true;
}

#endif

ExceptionOr<bool> WebGLRenderingContextBase::validateImageBitmap(const char* functionName, ImageBitmap* bitmap)
{
    if (!bitmap) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "no ImageBitmap");
        return false;
    }
    if (bitmap->isDetached()) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "The ImageBitmap has been detached.");
        return false;
    }
    if (!bitmap->originClean())
        return Exception { SecurityError };
    return true;
}

void WebGLRenderingContextBase::vertexAttribfImpl(const char* functionName, GCGLuint index, GCGLsizei expectedSize, GCGLfloat v0, GCGLfloat v1, GCGLfloat v2, GCGLfloat v3)
{
    if (isContextLost())
        return;
    if (index >= m_maxVertexAttribs) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "index out of range");
        return;
    }
    switch (expectedSize) {
    case 1:
        m_context->vertexAttrib1f(index, v0);
        break;
    case 2:
        m_context->vertexAttrib2f(index, v0, v1);
        break;
    case 3:
        m_context->vertexAttrib3f(index, v0, v1, v2);
        break;
    case 4:
        m_context->vertexAttrib4f(index, v0, v1, v2, v3);
        break;
    }
    VertexAttribValue& attribValue = m_vertexAttribValue[index];
    attribValue.type = GraphicsContextGL::FLOAT;
    attribValue.fValue[0] = v0;
    attribValue.fValue[1] = v1;
    attribValue.fValue[2] = v2;
    attribValue.fValue[3] = v3;
}

void WebGLRenderingContextBase::vertexAttribfvImpl(const char* functionName, GCGLuint index, Float32List&& list, GCGLsizei expectedSize)
{
    if (isContextLost())
        return;
    
    auto data = list.data();
    if (!data) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "no array");
        return;
    }

    int size = list.length();
    if (size < expectedSize) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "invalid size");
        return;
    }
    if (index >= m_maxVertexAttribs) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "index out of range");
        return;
    }
    switch (expectedSize) {
    case 1:
        m_context->vertexAttrib1fv(index, makeGCGLSpan<1>(data));
        break;
    case 2:
        m_context->vertexAttrib2fv(index, makeGCGLSpan<2>(data));
        break;
    case 3:
        m_context->vertexAttrib3fv(index, makeGCGLSpan<3>(data));
        break;
    case 4:
        m_context->vertexAttrib4fv(index, makeGCGLSpan<4>(data));
        break;
    }
    VertexAttribValue& attribValue = m_vertexAttribValue[index];
    attribValue.initValue();
    for (int ii = 0; ii < expectedSize; ++ii)
        attribValue.fValue[ii] = data[ii];
}

void WebGLRenderingContextBase::scheduleTaskToDispatchContextLostEvent()
{
    auto* canvas = htmlCanvas();
    if (!canvas)
        return;

    // It is safe to capture |this| because we keep the canvas element alive and it owns |this|.
    queueTaskKeepingObjectAlive(*canvas, TaskSource::WebGL, [this, canvas] {
        if (isContextStopped())
            return;
        if (!isContextLost())
            return;
        auto event = WebGLContextEvent::create(eventNames().webglcontextlostEvent, Event::CanBubble::No, Event::IsCancelable::Yes, emptyString());
        canvas->dispatchEvent(event);
        m_contextLostState->restoreRequested = event->defaultPrevented();
        if (m_contextLostState->mode == RealLostContext && m_contextLostState->restoreRequested)
            m_restoreTimer.startOneShot(0_s);
    });
}

void WebGLRenderingContextBase::maybeRestoreContext()
{
    RELEASE_ASSERT(!m_isSuspended);
    if (!isContextLost() || !m_contextLostState->restoreRequested) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto scriptExecutionContext = canvasBase().scriptExecutionContext();
    if (!scriptExecutionContext)
        return;

    if (!scriptExecutionContext->settingsValues().webGLEnabled)
        return;

    GraphicsClient* graphicsClient = canvasBase().graphicsClient();
    if (!graphicsClient)
        return;

    RefPtr<GraphicsContextGL> context = graphicsClient->createGraphicsContextGL(m_attributes);
    if (!context) {
        if (m_contextLostState->mode == RealLostContext)
            m_restoreTimer.startOneShot(secondsBetweenRestoreAttempts);
        else
            printToConsole(MessageLevel::Error, "WebGL: error restoring lost context."_s);
        return;
    }

    setGraphicsContextGL(context.releaseNonNull());
    addActivityStateChangeObserverIfNecessary();
    m_contextLostState = std::nullopt;
    setupFlags();
    initializeNewContext();

    auto* canvas = htmlCanvas();
    if (!canvas)
        return;

    if (!isContextLost())
        canvas->dispatchEvent(WebGLContextEvent::create(eventNames().webglcontextrestoredEvent, Event::CanBubble::No, Event::IsCancelable::Yes, emptyString()));
}

void WebGLRenderingContextBase::simulateEventForTesting(SimulatedEventForTesting event)
{
    if (m_context)
        m_context->simulateEventForTesting(event);
}

String WebGLRenderingContextBase::ensureNotNull(const String& text) const
{
    if (text.isNull())
        return emptyString();
    return text;
}

WebGLRenderingContextBase::LRUImageBufferCache::LRUImageBufferCache(int capacity)
    : m_buffers(capacity)
{
}

ImageBuffer* WebGLRenderingContextBase::LRUImageBufferCache::imageBuffer(const IntSize& size, DestinationColorSpace colorSpace, CompositeOperator fillOperator)
{
    size_t i;
    for (i = 0; i < m_buffers.size(); ++i) {
        if (!m_buffers[i])
            break;
        ImageBuffer& buf = m_buffers[i]->second.get();
        if (m_buffers[i]->first != colorSpace || buf.truncatedLogicalSize() != size)
            continue;
        bubbleToFront(i);
        if (fillOperator != CompositeOperator::Copy && fillOperator != CompositeOperator::Clear)
            buf.context().clearRect({ { }, size });
        return &buf;
    }

    // FIXME (149423): Should this ImageBuffer be unconditionally unaccelerated?
    auto temp = ImageBuffer::create(size, RenderingPurpose::Unspecified, 1, colorSpace, PixelFormat::BGRA8);
    if (!temp)
        return nullptr;
    ASSERT(m_buffers.size() > 0);
    i = std::min(m_buffers.size() - 1, i);
    m_buffers[i] = { colorSpace, temp.releaseNonNull() };

    ImageBuffer& buf = m_buffers[i]->second.get();
    bubbleToFront(i);
    return &buf;
}

void WebGLRenderingContextBase::LRUImageBufferCache::bubbleToFront(size_t idx)
{
    for (size_t i = idx; i > 0; --i)
        m_buffers[i].swap(m_buffers[i-1]);
}

namespace {

    String GetErrorString(GCGLenum error)
    {
        switch (error) {
        case GraphicsContextGL::INVALID_ENUM:
            return "INVALID_ENUM"_s;
        case GraphicsContextGL::INVALID_VALUE:
            return "INVALID_VALUE"_s;
        case GraphicsContextGL::INVALID_OPERATION:
            return "INVALID_OPERATION"_s;
        case GraphicsContextGL::OUT_OF_MEMORY:
            return "OUT_OF_MEMORY"_s;
        case GraphicsContextGL::INVALID_FRAMEBUFFER_OPERATION:
            return "INVALID_FRAMEBUFFER_OPERATION"_s;
        case GraphicsContextGL::CONTEXT_LOST_WEBGL:
            return "CONTEXT_LOST_WEBGL"_s;
        default:
            return makeString("WebGL ERROR(", hex(error, 4, Lowercase), ')');
        }
    }

} // namespace anonymous

void WebGLRenderingContextBase::synthesizeGLError(GCGLenum error, const char* functionName, const char* description)
{
    printToConsole(MessageLevel::Error, makeString("WebGL: ", GetErrorString(error), ": ", functionName, ": ", description));
    if (m_context)
        m_context->synthesizeGLError(error);
}

void WebGLRenderingContextBase::synthesizeLostContextGLError(GCGLenum error, const char* functionName, const char* description)
{
    printToConsole(MessageLevel::Error, makeString("WebGL: ", GetErrorString(error), ": ", functionName, ": ", description));
    m_contextLostState->errors.add(error);
}

void WebGLRenderingContextBase::enableOrDisable(GCGLenum capability, bool enable)
{
    if (enable)
        m_context->enable(capability);
    else
        m_context->disable(capability);
}

IntSize WebGLRenderingContextBase::clampedCanvasSize()
{
    return IntSize(clamp(canvasBase().width(), 1, m_maxViewportDims[0]),
        clamp(canvasBase().height(), 1, m_maxViewportDims[1]));
}

GCGLint WebGLRenderingContextBase::getMaxDrawBuffers()
{
    if (!supportsDrawBuffers())
        return 0;
    if (!m_maxDrawBuffers)
        m_maxDrawBuffers = m_context->getInteger(GraphicsContextGL::MAX_DRAW_BUFFERS_EXT);
    if (!m_maxColorAttachments)
        m_maxColorAttachments = m_context->getInteger(GraphicsContextGL::MAX_COLOR_ATTACHMENTS_EXT);
    // WEBGL_draw_buffers requires MAX_COLOR_ATTACHMENTS >= MAX_DRAW_BUFFERS.
    return std::min(m_maxDrawBuffers, m_maxColorAttachments);
}

GCGLint WebGLRenderingContextBase::getMaxColorAttachments()
{
    if (!supportsDrawBuffers())
        return 0;
    if (!m_maxColorAttachments)
        m_maxColorAttachments = m_context->getInteger(GraphicsContextGL::MAX_COLOR_ATTACHMENTS_EXT);
    return m_maxColorAttachments;
}

void WebGLRenderingContextBase::setBackDrawBuffer(GCGLenum buf)
{
    ASSERT(buf == GraphicsContextGL::NONE || buf == GraphicsContextGL::BACK);
    m_backDrawBuffer = buf;
}

void WebGLRenderingContextBase::setFramebuffer(const AbstractLocker&, GCGLenum target, WebGLFramebuffer* buffer)
{
    if (buffer)
        buffer->setHasEverBeenBound();

    if (target == GraphicsContextGL::FRAMEBUFFER || target == GraphicsContextGL::DRAW_FRAMEBUFFER)
        m_framebufferBinding = buffer;
    m_context->bindFramebuffer(target, objectOrZero(buffer));
}

void WebGLRenderingContextBase::restoreCurrentFramebuffer()
{
    bindFramebuffer(GraphicsContextGL::FRAMEBUFFER, m_framebufferBinding.get());
}

void WebGLRenderingContextBase::restoreCurrentTexture2D()
{
    auto texture = m_textureUnits[m_activeTextureUnit].texture2DBinding.get();
    bindTexture(GraphicsContextGL::TEXTURE_2D, texture);
}

bool WebGLRenderingContextBase::supportsDrawBuffers()
{
    if (!m_drawBuffersWebGLRequirementsChecked) {
        m_drawBuffersWebGLRequirementsChecked = true;
        m_drawBuffersSupported = WebGLDrawBuffers::supported(*this);
    }
    return m_drawBuffersSupported;
}

void WebGLRenderingContextBase::drawArraysInstanced(GCGLenum mode, GCGLint first, GCGLsizei count, GCGLsizei primcount)
{
    if (isContextLost())
        return;
    if (!validateVertexArrayObject("drawArraysInstanced"))
        return;

    if (m_currentProgram && InspectorInstrumentation::isWebGLProgramDisabled(*this, *m_currentProgram))
        return;

    clearIfComposited(CallerTypeDrawOrClear);

    {
        InspectorScopedShaderProgramHighlight scopedHighlight(*this, m_currentProgram.get());

        m_context->drawArraysInstanced(mode, first, count, primcount);
    }

    markContextChangedAndNotifyCanvasObserver();
}

void WebGLRenderingContextBase::drawElementsInstanced(GCGLenum mode, GCGLsizei count, GCGLenum type, long long offset, GCGLsizei primcount)
{
    if (isContextLost())
        return;
    if (!validateVertexArrayObject("drawElementsInstanced"))
        return;

    if (m_currentProgram && InspectorInstrumentation::isWebGLProgramDisabled(*this, *m_currentProgram))
        return;

    clearIfComposited(CallerTypeDrawOrClear);

    {
        InspectorScopedShaderProgramHighlight scopedHighlight(*this, m_currentProgram.get());

        m_context->drawElementsInstanced(mode, count, type, static_cast<GCGLintptr>(offset), primcount);
    }

    markContextChangedAndNotifyCanvasObserver();
}

void WebGLRenderingContextBase::vertexAttribDivisor(GCGLuint index, GCGLuint divisor)
{
    if (isContextLost())
        return;

    if (index >= m_maxVertexAttribs) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "vertexAttribDivisor", "index out of range");
        return;
    }

    m_boundVertexArrayObject->setVertexAttribDivisor(index, divisor);
    m_context->vertexAttribDivisor(index, divisor);
}

bool WebGLRenderingContextBase::enableSupportedExtension(ASCIILiteral extensionNameLiteral)
{
    ASSERT(m_context);
    String extensionName { extensionNameLiteral };
    if (!m_context->supportsExtension(extensionName))
        return false;
    m_context->ensureExtensionEnabled(extensionName);
    return true;
}

void WebGLRenderingContextBase::loseExtensions(LostContextMode mode)
{
#define LOSE_EXTENSION(variable) \
    if (variable) { \
        variable->loseParentContext(mode); \
        if (variable->isLostContext()) \
            (void) variable.releaseNonNull(); \
    }

    LOSE_EXTENSION(m_angleInstancedArrays);
    LOSE_EXTENSION(m_extBlendMinMax);
    LOSE_EXTENSION(m_extColorBufferFloat);
    LOSE_EXTENSION(m_extColorBufferHalfFloat);
    LOSE_EXTENSION(m_extFloatBlend);
    LOSE_EXTENSION(m_extFragDepth);
    LOSE_EXTENSION(m_extShaderTextureLOD);
    LOSE_EXTENSION(m_extTextureCompressionBPTC);
    LOSE_EXTENSION(m_extTextureCompressionRGTC);
    LOSE_EXTENSION(m_extTextureFilterAnisotropic);
    LOSE_EXTENSION(m_extTextureNorm16);
    LOSE_EXTENSION(m_extsRGB);
    LOSE_EXTENSION(m_khrParallelShaderCompile);
    LOSE_EXTENSION(m_oesDrawBuffersIndexed);
    LOSE_EXTENSION(m_oesElementIndexUint);
    LOSE_EXTENSION(m_oesFBORenderMipmap);
    LOSE_EXTENSION(m_oesStandardDerivatives);
    LOSE_EXTENSION(m_oesTextureFloat);
    LOSE_EXTENSION(m_oesTextureFloatLinear);
    LOSE_EXTENSION(m_oesTextureHalfFloat);
    LOSE_EXTENSION(m_oesTextureHalfFloatLinear);
    LOSE_EXTENSION(m_oesVertexArrayObject);
    LOSE_EXTENSION(m_webglClipCullDistance);
    LOSE_EXTENSION(m_webglColorBufferFloat);
    LOSE_EXTENSION(m_webglCompressedTextureASTC);
    LOSE_EXTENSION(m_webglCompressedTextureETC);
    LOSE_EXTENSION(m_webglCompressedTextureETC1);
    LOSE_EXTENSION(m_webglCompressedTexturePVRTC);
    LOSE_EXTENSION(m_webglCompressedTextureS3TC);
    LOSE_EXTENSION(m_webglCompressedTextureS3TCsRGB);
    LOSE_EXTENSION(m_webglDebugRendererInfo);
    LOSE_EXTENSION(m_webglDebugShaders);
    LOSE_EXTENSION(m_webglDepthTexture);
    LOSE_EXTENSION(m_webglDrawBuffers);
    LOSE_EXTENSION(m_webglDrawInstancedBaseVertexBaseInstance);
    LOSE_EXTENSION(m_webglLoseContext);
    LOSE_EXTENSION(m_webglMultiDraw);
    LOSE_EXTENSION(m_webglMultiDrawInstancedBaseVertexBaseInstance);
    LOSE_EXTENSION(m_webglProvokingVertex);
}

void WebGLRenderingContextBase::activityStateDidChange(OptionSet<ActivityState::Flag> oldActivityState, OptionSet<ActivityState::Flag> newActivityState)
{
    if (!m_context)
        return;

    auto changed = oldActivityState ^ newActivityState;
    if (changed & ActivityState::IsVisible)
        m_context->setContextVisibility(newActivityState.contains(ActivityState::IsVisible));
}

void WebGLRenderingContextBase::didComposite()
{
    m_compositingResultsNeedUpdating = false;

    if (UNLIKELY(hasActiveInspectorCanvasCallTracer()))
        InspectorInstrumentation::didFinishRecordingCanvasFrame(*this);
}

void WebGLRenderingContextBase::forceContextLost()
{
    forceLostContext(WebGLRenderingContextBase::RealLostContext);
}

void WebGLRenderingContextBase::recycleContext()
{
    printToConsole(MessageLevel::Error, "There are too many active WebGL contexts on this page, the oldest context will be lost."_s);
    // Using SyntheticLostContext means the developer won't be able to force the restoration
    // of the context by calling preventDefault() in a "webglcontextlost" event handler.
    forceLostContext(SyntheticLostContext);
    destroyGraphicsContextGL();
}

void WebGLRenderingContextBase::dispatchContextChangedNotification()
{
    auto* canvas = htmlCanvas();
    if (!canvas)
        return;

    queueTaskToDispatchEvent(*canvas, TaskSource::WebGL, WebGLContextEvent::create(eventNames().webglcontextchangedEvent, Event::CanBubble::No, Event::IsCancelable::Yes, emptyString()));
}

void WebGLRenderingContextBase::addMembersToOpaqueRoots(JSC::AbstractSlotVisitor& visitor)
{
    Locker locker { objectGraphLock() };

    addWebCoreOpaqueRoot(visitor, m_boundArrayBuffer.get());

    addWebCoreOpaqueRoot(visitor, m_boundVertexArrayObject.get());
    if (m_boundVertexArrayObject)
        m_boundVertexArrayObject->addMembersToOpaqueRoots(locker, visitor);

    addWebCoreOpaqueRoot(visitor, m_currentProgram.get());
    if (m_currentProgram)
        m_currentProgram->addMembersToOpaqueRoots(locker, visitor);

    addWebCoreOpaqueRoot(visitor, m_framebufferBinding.get());
    if (m_framebufferBinding)
        m_framebufferBinding->addMembersToOpaqueRoots(locker, visitor);

    addWebCoreOpaqueRoot(visitor, m_renderbufferBinding.get());

    for (auto& unit : m_textureUnits) {
        addWebCoreOpaqueRoot(visitor, unit.texture2DBinding.get());
        addWebCoreOpaqueRoot(visitor, unit.textureCubeMapBinding.get());
        addWebCoreOpaqueRoot(visitor, unit.texture3DBinding.get());
        addWebCoreOpaqueRoot(visitor, unit.texture2DArrayBinding.get());
    }

    // Extensions' IDL files use GenerateIsReachable=ImplWebGLRenderingContext,
    // which checks to see whether the context is in the opaque root set (it is;
    // it's added in JSWebGLRenderingContext / JSWebGL2RenderingContext's custom
    // bindings code). For this reason it's unnecessary to explicitly add opaque
    // roots for extensions.
}

Lock& WebGLRenderingContextBase::objectGraphLock()
{
    return m_objectGraphLock;
}

void WebGLRenderingContextBase::prepareForDisplay()
{
    if (!m_context)
        return;

    m_context->prepareForDisplay();
}

void WebGLRenderingContextBase::updateActiveOrdinal()
{
    m_activeOrdinal = s_lastActiveOrdinal++;
}

WebCoreOpaqueRoot root(WebGLRenderingContextBase* context)
{
    return WebCoreOpaqueRoot { context };
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
