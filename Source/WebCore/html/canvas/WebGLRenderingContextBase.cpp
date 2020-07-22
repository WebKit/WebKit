/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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
#include "DOMWindow.h"
#include "DiagnosticLoggingClient.h"
#include "DiagnosticLoggingKeys.h"
#include "Document.h"
#include "EXTBlendMinMax.h"
#include "EXTColorBufferFloat.h"
#include "EXTColorBufferHalfFloat.h"
#include "EXTFragDepth.h"
#include "EXTShaderTextureLOD.h"
#include "EXTTextureFilterAnisotropic.h"
#include "EXTsRGB.h"
#include "EventNames.h"
#include "ExtensionsGL.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "HTMLVideoElement.h"
#include "ImageBitmap.h"
#include "ImageBuffer.h"
#include "ImageData.h"
#include "InspectorInstrumentation.h"
#include "IntSize.h"
#include "JSExecState.h"
#include "Logging.h"
#include "NavigatorWebXR.h"
#include "NotImplemented.h"
#include "OESElementIndexUint.h"
#include "OESStandardDerivatives.h"
#include "OESTextureFloat.h"
#include "OESTextureFloatLinear.h"
#include "OESTextureHalfFloat.h"
#include "OESTextureHalfFloatLinear.h"
#include "OESVertexArrayObject.h"
#include "Page.h"
#include "RenderBox.h"
#include "RuntimeEnabledFeatures.h"
#include "Settings.h"
#include "WebGL2RenderingContext.h"
#include "WebGLActiveInfo.h"
#include "WebGLBuffer.h"
#include "WebGLColorBufferFloat.h"
#include "WebGLCompressedTextureASTC.h"
#include "WebGLCompressedTextureATC.h"
#include "WebGLCompressedTextureETC.h"
#include "WebGLCompressedTextureETC1.h"
#include "WebGLCompressedTexturePVRTC.h"
#include "WebGLCompressedTextureS3TC.h"
#include "WebGLContextAttributes.h"
#include "WebGLContextEvent.h"
#include "WebGLContextGroup.h"
#include "WebGLDebugRendererInfo.h"
#include "WebGLDebugShaders.h"
#include "WebGLDepthTexture.h"
#include "WebGLDrawBuffers.h"
#include "WebGLFramebuffer.h"
#include "WebGLLoseContext.h"
#include "WebGLProgram.h"
#include "WebGLRenderbuffer.h"
#include "WebGLRenderingContext.h"
#include "WebGLShader.h"
#include "WebGLShaderPrecisionFormat.h"
#include "WebGLTexture.h"
#include "WebGLUniformLocation.h"
#include "WebXRSystem.h"
#include <JavaScriptCore/ConsoleMessage.h>
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/ScriptCallStack.h>
#include <JavaScriptCore/ScriptCallStackFactory.h>
#include <JavaScriptCore/TypedArrayInlines.h>
#include <JavaScriptCore/Uint32Array.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/HashMap.h>
#include <wtf/HexNumber.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/Lock.h>
#include <wtf/Scope.h>
#include <wtf/StdLibExtras.h>
#include <wtf/UniqueArray.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

#if ENABLE(OFFSCREEN_CANVAS)
#include "OffscreenCanvas.h"
#endif

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(WebGLRenderingContextBase);

static const Seconds secondsBetweenRestoreAttempts { 1_s };
const int maxGLErrorsAllowedToConsole = 256;
static const Seconds checkContextLossHandlingDelay { 3_s };

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
        if (name.startsWith("gl_") || name.startsWith("webgl_") || name.startsWith("_webgl_"))
            return true;
        return false;
    }

    // Strips comments from shader text. This allows non-ASCII characters
    // to be used in comments without potentially breaking OpenGL
    // implementations not expecting characters outside the GLSL ES set.
    class StripComments {
    public:
        StripComments(const String& str)
            : m_parseState(BeginningOfLine)
            , m_sourceString(str)
            , m_length(str.length())
            , m_position(0)
        {
            parse();
        }

        String result()
        {
            return m_builder.toString();
        }

    private:
        bool hasMoreCharacters() const
        {
            return (m_position < m_length);
        }

        void parse()
        {
            while (hasMoreCharacters()) {
                process(current());
                // process() might advance the position.
                if (hasMoreCharacters())
                    advance();
            }
        }

        void process(UChar);

        bool peek(UChar& character) const
        {
            if (m_position + 1 >= m_length)
                return false;
            character = m_sourceString[m_position + 1];
            return true;
        }

        UChar current() const
        {
            ASSERT_WITH_SECURITY_IMPLICATION(m_position < m_length);
            return m_sourceString[m_position];
        }

        void advance()
        {
            ++m_position;
        }

        bool isNewline(UChar character) const
        {
            // Don't attempt to canonicalize newline related characters.
            return (character == '\n' || character == '\r');
        }

        void emit(UChar character)
        {
            m_builder.append(character);
        }

        enum ParseState {
            // Have not seen an ASCII non-whitespace character yet on
            // this line. Possible that we might see a preprocessor
            // directive.
            BeginningOfLine,

            // Have seen at least one ASCII non-whitespace character
            // on this line.
            MiddleOfLine,

            // Handling a preprocessor directive. Passes through all
            // characters up to the end of the line. Disables comment
            // processing.
            InPreprocessorDirective,

            // Handling a single-line comment. The comment text is
            // replaced with a single space.
            InSingleLineComment,

            // Handling a multi-line comment. Newlines are passed
            // through to preserve line numbers.
            InMultiLineComment
        };

        ParseState m_parseState;
        String m_sourceString;
        unsigned m_length;
        unsigned m_position;
        StringBuilder m_builder;
    };

    void StripComments::process(UChar c)
    {
        if (isNewline(c)) {
            // No matter what state we are in, pass through newlines
            // so we preserve line numbers.
            emit(c);

            if (m_parseState != InMultiLineComment)
                m_parseState = BeginningOfLine;

            return;
        }

        UChar temp = 0;
        switch (m_parseState) {
        case BeginningOfLine:
            if (WTF::isASCIISpace(c)) {
                emit(c);
                break;
            }

            if (c == '#') {
                m_parseState = InPreprocessorDirective;
                emit(c);
                break;
            }

            // Transition to normal state and re-handle character.
            m_parseState = MiddleOfLine;
            process(c);
            break;

        case MiddleOfLine:
            if (c == '/' && peek(temp)) {
                if (temp == '/') {
                    m_parseState = InSingleLineComment;
                    emit(' ');
                    advance();
                    break;
                }

                if (temp == '*') {
                    m_parseState = InMultiLineComment;
                    // Emit the comment start in case the user has
                    // an unclosed comment and we want to later
                    // signal an error.
                    emit('/');
                    emit('*');
                    advance();
                    break;
                }
            }

            emit(c);
            break;

        case InPreprocessorDirective:
            // No matter what the character is, just pass it
            // through. Do not parse comments in this state. This
            // might not be the right thing to do long term, but it
            // should handle the #error preprocessor directive.
            emit(c);
            break;

        case InSingleLineComment:
            // The newline code at the top of this function takes care
            // of resetting our state when we get out of the
            // single-line comment. Swallow all other characters.
            break;

        case InMultiLineComment:
            if (c == '*' && peek(temp) && temp == '/') {
                emit('*');
                emit('/');
                m_parseState = MiddleOfLine;
                advance();
                break;
            }

            // Swallow all other characters. Unclear whether we may
            // want or need to just emit a space per character to try
            // to preserve column numbers for debugging purposes.
            break;
        }
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

#define ADD_VALUES_TO_SET(set, arr) \
    set.add(arr, arr + WTF_ARRAY_LENGTH(arr))

#if !USE(ANGLE)
// Returns false if no clipping is necessary, i.e., x, y, width, height stay the same.
static bool clip2D(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height,
    GCGLsizei sourceWidth, GCGLsizei sourceHeight,
    GCGLint* clippedX, GCGLint* clippedY, GCGLsizei* clippedWidth, GCGLsizei*clippedHeight)
{
    ASSERT(clippedX && clippedY && clippedWidth && clippedHeight);

    GCGLint left = std::max(x, 0);
    GCGLint top = std::max(y, 0);
    GCGLint right = 0;
    GCGLint bottom = 0;

    Checked<GCGLint, RecordOverflow> checkedInputRight = Checked<GCGLint>(x) + Checked<GCGLsizei>(width);
    Checked<GCGLint, RecordOverflow> checkedInputBottom = Checked<GCGLint>(y) + Checked<GCGLsizei>(height);
    if (!checkedInputRight.hasOverflowed() && !checkedInputBottom.hasOverflowed()) {
        right = std::min(checkedInputRight.unsafeGet(), sourceWidth);
        bottom = std::min(checkedInputBottom.unsafeGet(), sourceHeight);
    }

    if (left >= right || top >= bottom) {
        *clippedX = 0;
        *clippedY = 0;
        *clippedWidth = 0;
        *clippedHeight = 0;
        return true;
    }

    *clippedX = left;
    *clippedY = top;
    *clippedWidth = right - left;
    *clippedHeight = bottom - top;

    return (*clippedX != x || *clippedY != y || *clippedWidth != width || *clippedHeight != height);
}
#endif

class InspectorScopedShaderProgramHighlight {
public:
    InspectorScopedShaderProgramHighlight(WebGLRenderingContextBase& context, WebGLProgram* program)
        : m_context(context)
        , m_program(program)
    {
        showHightlight();
    }

    ~InspectorScopedShaderProgramHighlight()
    {
        hideHighlight();
    }

private:
    void showHightlight()
    {
        if (!m_program || LIKELY(!InspectorInstrumentation::isWebGLProgramHighlighted(m_context, *m_program)))
            return;

        if (hasBufferBinding(GraphicsContextGL::FRAMEBUFFER_BINDING)) {
            if (!hasBufferBinding(GraphicsContextGL::RENDERBUFFER_BINDING))
                return;
            if (hasFramebufferParameterAttachment(GraphicsContextGL::DEPTH_ATTACHMENT))
                return;
            if (hasFramebufferParameterAttachment(GraphicsContextGL::STENCIL_ATTACHMENT))
                return;
#if ENABLE(WEBGL2)
            if (hasFramebufferParameterAttachment(GraphicsContextGL::DEPTH_STENCIL_ATTACHMENT))
                return;
#endif
        }

        saveBlendValue(GraphicsContextGL::BLEND_COLOR, m_savedBlend.color);
        saveBlendValue(GraphicsContextGL::BLEND_EQUATION_RGB, m_savedBlend.equationRGB);
        saveBlendValue(GraphicsContextGL::BLEND_EQUATION_ALPHA, m_savedBlend.equationAlpha);
        saveBlendValue(GraphicsContextGL::BLEND_SRC_RGB, m_savedBlend.srcRGB);
        saveBlendValue(GraphicsContextGL::BLEND_SRC_ALPHA, m_savedBlend.srcAlpha);
        saveBlendValue(GraphicsContextGL::BLEND_DST_RGB, m_savedBlend.dstRGB);
        saveBlendValue(GraphicsContextGL::BLEND_DST_ALPHA, m_savedBlend.dstAlpha);
        saveBlendValue(GraphicsContextGL::BLEND, m_savedBlend.enabled);

        static const GCGLfloat red = 111.0 / 255.0;
        static const GCGLfloat green = 168.0 / 255.0;
        static const GCGLfloat blue = 220.0 / 255.0;
        static const GCGLfloat alpha = 2.0 / 3.0;

        m_context.enable(GraphicsContextGL::BLEND);
        m_context.blendColor(red, green, blue, alpha);
        m_context.blendEquation(GraphicsContextGL::FUNC_ADD);
        m_context.blendFunc(GraphicsContextGL::CONSTANT_COLOR, GraphicsContextGL::ONE_MINUS_SRC_ALPHA);

        m_didApply = true;
    }

    void hideHighlight()
    {
        if (!m_didApply)
            return;

        if (!m_savedBlend.enabled)
            m_context.disable(GraphicsContextGL::BLEND);

        const RefPtr<Float32Array>& color = m_savedBlend.color;
        m_context.blendColor(color->item(0), color->item(1), color->item(2), color->item(3));
        m_context.blendEquationSeparate(m_savedBlend.equationRGB, m_savedBlend.equationAlpha);
        m_context.blendFuncSeparate(m_savedBlend.srcRGB, m_savedBlend.dstRGB, m_savedBlend.srcAlpha, m_savedBlend.dstAlpha);

        m_savedBlend.color = nullptr;

        m_didApply = false;
    }

    template <typename T>
    void saveBlendValue(GCGLenum attachment, T& destination)
    {
        WebGLAny param = m_context.getParameter(attachment);
        if (WTF::holds_alternative<T>(param))
            destination = WTF::get<T>(param);
    }

    bool hasBufferBinding(GCGLenum pname)
    {
        WebGLAny binding = m_context.getParameter(pname);
        if (pname == GraphicsContextGL::FRAMEBUFFER_BINDING)
            return WTF::holds_alternative<RefPtr<WebGLFramebuffer>>(binding) && WTF::get<RefPtr<WebGLFramebuffer>>(binding);
        if (pname == GraphicsContextGL::RENDERBUFFER_BINDING)
            return WTF::holds_alternative<RefPtr<WebGLRenderbuffer>>(binding) && WTF::get<RefPtr<WebGLRenderbuffer>>(binding);
        return false;
    }

    bool hasFramebufferParameterAttachment(GCGLenum attachment)
    {
        WebGLAny attachmentParameter = m_context.getFramebufferAttachmentParameter(GraphicsContextGL::FRAMEBUFFER, attachment, GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE);
        if (!WTF::holds_alternative<unsigned>(attachmentParameter))
            return false;
        if (WTF::get<unsigned>(attachmentParameter) != static_cast<unsigned>(GraphicsContextGL::RENDERBUFFER))
            return false;
        return true;
    }

    struct {
        RefPtr<Float32Array> color;
        unsigned equationRGB { 0 };
        unsigned equationAlpha { 0 };
        unsigned srcRGB { 0 };
        unsigned srcAlpha { 0 };
        unsigned dstRGB { 0 };
        unsigned dstAlpha { 0 };
        bool enabled { false };
    } m_savedBlend;

    WebGLRenderingContextBase& m_context;
    WebGLProgram* m_program { nullptr };
    bool m_didApply { false };
};

static bool isHighPerformanceContext(const RefPtr<GraphicsContextGLOpenGL>& context)
{
    return context->powerPreferenceUsedForCreation() == WebGLPowerPreference::HighPerformance;
}

std::unique_ptr<WebGLRenderingContextBase> WebGLRenderingContextBase::create(CanvasBase& canvas, WebGLContextAttributes& attributes, const String& type)
{
#if ENABLE(WEBGL2)
    // WebGL 2.0 is only supported with the ANGLE backend.
#if USE(ANGLE)
    if (type == "webgl2" && !RuntimeEnabledFeatures::sharedFeatures().webGL2Enabled())
        return nullptr;
#else
    if (type == "webgl2")
        return nullptr;
#endif
#else
    UNUSED_PARAM(type);
#endif

    bool isPendingPolicyResolution = false;
    HostWindow* hostWindow = nullptr;

    auto* canvasElement = is<HTMLCanvasElement>(canvas) ? &downcast<HTMLCanvasElement>(canvas) : nullptr;

    if (canvasElement) {
        Document& document = canvasElement->document();
        RefPtr<Frame> frame = document.frame();
        if (!frame)
            return nullptr;

        // The FrameLoaderClient might block creation of a new WebGL context despite the page settings; in
        // particular, if WebGL contexts were lost one or more times via the GL_ARB_robustness extension.
        if (!frame->loader().client().allowWebGL(frame->settings().webGLEnabled())) {
            canvasElement->dispatchEvent(WebGLContextEvent::create(eventNames().webglcontextcreationerrorEvent,
                Event::CanBubble::No, Event::IsCancelable::Yes, "Web page was not allowed to create a WebGL context."));
            return nullptr;
        }

        Document& topDocument = document.topDocument();
        Page* page = topDocument.page();
        bool forcingPendingPolicy = frame->settings().isForcePendingWebGLPolicy();

        if (forcingPendingPolicy || (page && !topDocument.url().isLocalFile())) {
            WebGLLoadPolicy policy = forcingPendingPolicy ? WebGLLoadPolicy::WebGLPendingCreation : page->mainFrame().loader().client().webGLPolicyForURL(topDocument.url());

            if (policy == WebGLLoadPolicy::WebGLBlockCreation) {
                LOG(WebGL, "The policy for this URL (%s) is to block WebGL.", topDocument.url().host().utf8().data());
                return nullptr;
            }

            if (policy == WebGLLoadPolicy::WebGLPendingCreation) {
                LOG(WebGL, "WebGL policy is pending. May need to be resolved later.");
                isPendingPolicyResolution = true;
            }
        }

        if (frame->settings().forceWebGLUsesLowPower()) {
            if (attributes.powerPreference == GraphicsContextGLPowerPreference::HighPerformance)
                LOG(WebGL, "Overriding powerPreference from high-performance to low-power.");
            attributes.powerPreference = GraphicsContextGLPowerPreference::LowPower;
        }

        if (page)
            attributes.devicePixelRatio = page->deviceScaleFactor();

        hostWindow = document.view()->root()->hostWindow();
    }

    attributes.noExtensions = true;
    attributes.shareResources = false;

    attributes.initialPowerPreference = attributes.powerPreference;


#if ENABLE(WEBGL2)
    if (type == "webgl2")
        attributes.isWebGL2 = true;
#endif

    if (isPendingPolicyResolution) {
        LOG(WebGL, "Create a WebGL context that looks real, but will require a policy resolution if used.");
        std::unique_ptr<WebGLRenderingContextBase> renderingContext = nullptr;
#if ENABLE(WEBGL2)
        if (type == "webgl2")
            renderingContext = WebGL2RenderingContext::create(canvas, attributes);
        else
#endif
            renderingContext = WebGLRenderingContext::create(canvas, attributes);
        renderingContext->suspendIfNeeded();
        return renderingContext;
    }

    auto context = GraphicsContextGLOpenGL::create(attributes, hostWindow);
    if (!context || !context->makeContextCurrent()) {
        if (canvasElement) {
            canvasElement->dispatchEvent(WebGLContextEvent::create(eventNames().webglcontextcreationerrorEvent,
                Event::CanBubble::No, Event::IsCancelable::Yes, "Could not create a WebGL context."));
        }
        return nullptr;
    }

    auto& extensions = context->getExtensions();
    if (extensions.supports("GL_EXT_debug_marker"_s))
        extensions.pushGroupMarkerEXT("WebGLRenderingContext"_s);

#if ENABLE(WEBGL2) && PLATFORM(MAC) && !USE(ANGLE)
    // glTexStorage() was only added to Core in OpenGL 4.2.
    // However, according to https://developer.apple.com/opengl/capabilities/ all Apple GPUs support this extension.
    if (attributes.isWebGL2 && !extensions.supports("GL_ARB_texture_storage"))
        return nullptr;
#endif

    std::unique_ptr<WebGLRenderingContextBase> renderingContext;
#if ENABLE(WEBGL2)
    if (type == "webgl2")
        renderingContext = WebGL2RenderingContext::create(canvas, context.releaseNonNull(), attributes);
    else
#endif
        renderingContext = WebGLRenderingContext::create(canvas, context.releaseNonNull(), attributes);
    renderingContext->suspendIfNeeded();

    return renderingContext;
}

WebGLRenderingContextBase::WebGLRenderingContextBase(CanvasBase& canvas, WebGLContextAttributes attributes)
    : GPUBasedCanvasRenderingContext(canvas)
    , m_restoreTimer(canvas.scriptExecutionContext(), *this, &WebGLRenderingContextBase::maybeRestoreContext)
    , m_attributes(attributes)
    , m_numGLErrorsToConsoleAllowed(maxGLErrorsAllowedToConsole)
    , m_isPendingPolicyResolution(true)
    , m_checkForContextLossHandlingTimer(*this, &WebGLRenderingContextBase::checkForContextLossHandling)
#if ENABLE(WEBXR)
    , m_isXRCompatible(attributes.xrCompatible)
#endif
{
    m_restoreTimer.suspendIfNeeded();

    registerWithWebGLStateTracker();
    m_checkForContextLossHandlingTimer.startOneShot(checkContextLossHandlingDelay);
}

WebGLRenderingContextBase::WebGLRenderingContextBase(CanvasBase& canvas, Ref<GraphicsContextGLOpenGL>&& context, WebGLContextAttributes attributes)
    : GPUBasedCanvasRenderingContext(canvas)
    , m_context(WTFMove(context))
    , m_restoreTimer(canvas.scriptExecutionContext(), *this, &WebGLRenderingContextBase::maybeRestoreContext)
    , m_generatedImageCache(4)
    , m_attributes(attributes)
    , m_numGLErrorsToConsoleAllowed(maxGLErrorsAllowedToConsole)
    , m_checkForContextLossHandlingTimer(*this, &WebGLRenderingContextBase::checkForContextLossHandling)
#if ENABLE(WEBXR)
    , m_isXRCompatible(attributes.xrCompatible)
#endif
{
    m_restoreTimer.suspendIfNeeded();

    m_contextGroup = WebGLContextGroup::create();
    m_contextGroup->addContext(*this);

    m_context->addClient(*this);

    m_context->getIntegerv(GraphicsContextGL::MAX_VIEWPORT_DIMS, m_maxViewportDims);

    setupFlags();

    registerWithWebGLStateTracker();
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
    ASSERT(!m_contextLost);
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
    
    m_clearColor[0] = m_clearColor[1] = m_clearColor[2] = m_clearColor[3] = 0;
    m_scissorEnabled = false;
    m_clearDepth = 1;
    m_clearStencil = 0;
    m_colorMask[0] = m_colorMask[1] = m_colorMask[2] = m_colorMask[3] = true;

    GCGLint numCombinedTextureImageUnits = 0;
    m_context->getIntegerv(GraphicsContextGL::MAX_COMBINED_TEXTURE_IMAGE_UNITS, &numCombinedTextureImageUnits);
    m_textureUnits.clear();
    m_textureUnits.resize(numCombinedTextureImageUnits);
#if !USE(ANGLE)
    for (GCGLint i = 0; i < numCombinedTextureImageUnits; ++i)
        m_unrenderableTextureUnits.add(i);
#endif
    
    GCGLint numVertexAttribs = 0;
    m_context->getIntegerv(GraphicsContextGL::MAX_VERTEX_ATTRIBS, &numVertexAttribs);
    m_maxVertexAttribs = numVertexAttribs;
    
    m_maxTextureSize = 0;
    m_context->getIntegerv(GraphicsContextGL::MAX_TEXTURE_SIZE, &m_maxTextureSize);
    m_maxTextureLevel = WebGLTexture::computeLevelCount(m_maxTextureSize, m_maxTextureSize);
    m_maxCubeMapTextureSize = 0;
    m_context->getIntegerv(GraphicsContextGL::MAX_CUBE_MAP_TEXTURE_SIZE, &m_maxCubeMapTextureSize);
    m_maxCubeMapTextureLevel = WebGLTexture::computeLevelCount(m_maxCubeMapTextureSize, m_maxCubeMapTextureSize);
    m_maxRenderbufferSize = 0;
    m_context->getIntegerv(GraphicsContextGL::MAX_RENDERBUFFER_SIZE, &m_maxRenderbufferSize);

    // These two values from EXT_draw_buffers are lazily queried.
    m_maxDrawBuffers = 0;
    m_maxColorAttachments = 0;

    m_backDrawBuffer = GraphicsContextGL::BACK;
    m_drawBuffersWebGLRequirementsChecked = false;
    m_drawBuffersSupported = false;
    
    m_vertexAttribValue.resize(m_maxVertexAttribs);

#if !USE(ANGLE)
    if (!isGLES2NPOTStrict())
        createFallbackBlackTextures1x1();
#endif

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
        m_isGLES2NPOTStrict = !m_context->getExtensions().isEnabled("GL_OES_texture_npot");
        m_isDepthStencilSupported = m_context->getExtensions().isEnabled("GL_OES_packed_depth_stencil") || m_context->getExtensions().isEnabled("GL_ANGLE_depth_texture");
    } else {
#if USE(ANGLE)
        m_isGLES2NPOTStrict = true;
#else
        m_isGLES2NPOTStrict = !m_context->getExtensions().isEnabled("GL_ARB_texture_non_power_of_two");
#endif
        m_isDepthStencilSupported = m_context->getExtensions().isEnabled("GL_EXT_packed_depth_stencil") || m_context->getExtensions().isEnabled("GL_ANGLE_depth_texture");
    }
    m_isRobustnessEXTSupported = m_context->getExtensions().isEnabled("GL_EXT_robustness");
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
#if !USE(ANGLE)
    m_vertexAttrib0Buffer = nullptr;
#endif
    m_currentProgram = nullptr;
    m_framebufferBinding = nullptr;
    m_renderbufferBinding = nullptr;

    for (auto& textureUnit : m_textureUnits) {
        textureUnit.texture2DBinding = nullptr;
        textureUnit.textureCubeMapBinding = nullptr;
    }

    m_blackTexture2D = nullptr;
    m_blackTextureCubeMap = nullptr;

    if (!m_isPendingPolicyResolution) {
        detachAndRemoveAllObjects();
        destroyGraphicsContextGL();
        m_contextGroup->removeContext(*this);
    }

    {
        LockHolder lock(WebGLProgram::instancesMutex());
        for (auto& entry : WebGLProgram::instances(lock)) {
            if (entry.value == this) {
                // Don't remove any WebGLProgram from the instances list, as they may still exist.
                // Only remove the association with a WebGL context.
                entry.value = nullptr;
            }
        }
    }
}

void WebGLRenderingContextBase::destroyGraphicsContextGL()
{
    if (m_isPendingPolicyResolution)
        return;

    removeActivityStateChangeObserver();

    if (m_context) {
        m_context->removeClient(*this);
        m_context = nullptr;
    }
}

void WebGLRenderingContextBase::markContextChanged()
{
    if (m_framebufferBinding)
        return;

    m_context->markContextChanged();

    m_layerCleared = false;

    auto* canvas = htmlCanvas();
    if (!canvas)
        return;

    RenderBox* renderBox = canvas->renderBox();
    if (isAccelerated() && renderBox && renderBox->hasAcceleratedCompositing()) {
        m_markedCanvasDirty = true;
        htmlCanvas()->clearCopiedImage();
        renderBox->contentChanged(CanvasPixelsChanged);
    } else {
        if (!m_markedCanvasDirty) {
            m_markedCanvasDirty = true;
            canvas->didDraw(FloatRect(FloatPoint(0, 0), clampedCanvasSize()));
        }
    }
}

void WebGLRenderingContextBase::markContextChangedAndNotifyCanvasObserver()
{
    markContextChanged();
    if (!isAccelerated())
        return;

    auto* canvas = htmlCanvas();
    if (!canvas)
        return;

    RenderBox* renderBox = canvas->renderBox();
    if (renderBox && renderBox->hasAcceleratedCompositing())
        canvas->notifyObserversCanvasChanged(FloatRect(FloatPoint(0, 0), clampedCanvasSize()));
}

bool WebGLRenderingContextBase::clearIfComposited(GCGLbitfield mask)
{
    if (isContextLostOrPending())
        return false;

    if (!m_context->layerComposited() || m_layerCleared || m_preventBufferClearForInspector)
        return false;

    GCGLbitfield buffersNeedingClearing = m_context->getBuffersToAutoClear();

    if (!buffersNeedingClearing || (mask && m_framebufferBinding))
        return false;

    // Use the underlying GraphicsContext3D's attributes to take into
    // account (for example) packed depth/stencil buffers.
    auto contextAttributes = m_context->contextAttributes();

    // Determine if it's possible to combine the clear the user asked for and this clear.
    bool combinedClear = mask && !m_scissorEnabled;

    m_context->disable(GraphicsContextGL::SCISSOR_TEST);
    if (combinedClear && (mask & GraphicsContextGL::COLOR_BUFFER_BIT)) {
        m_context->clearColor(m_colorMask[0] ? m_clearColor[0] : 0,
                              m_colorMask[1] ? m_clearColor[1] : 0,
                              m_colorMask[2] ? m_clearColor[2] : 0,
                              m_colorMask[3] ? m_clearColor[3] : 0);
    } else
        m_context->clearColor(0, 0, 0, 0);
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
    // If the WebGL 2.0 clearBuffer APIs already have been used to
    // selectively clear some of the buffers, don't destroy those
    // results.
    m_context->clear(clearMask & buffersNeedingClearing);
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
    m_context->colorMask(m_colorMask[0], m_colorMask[1],
                         m_colorMask[2], m_colorMask[3]);
    m_context->clearDepth(m_clearDepth);
    m_context->clearStencil(m_clearStencil);
    m_context->stencilMaskSeparate(GraphicsContextGL::FRONT, m_stencilMask);
    m_context->depthMask(m_depthMask);
}

void WebGLRenderingContextBase::markLayerComposited()
{
    if (isContextLostOrPending())
        return;
    m_context->markLayerComposited();
}

void WebGLRenderingContextBase::paintRenderingResultsToCanvas()
{
    if (isContextLostOrPending())
        return;

    auto* canvas = htmlCanvas();
    if (!canvas)
        return;

    if (canvas->document().printing())
        canvas->clearPresentationCopy();

    // Until the canvas is written to by the application, the clear that
    // happened after it was composited should be ignored by the compositor.
    if (m_context->layerComposited() && !m_attributes.preserveDrawingBuffer) {
        m_context->paintCompositedResultsToCanvas(canvas->buffer());

        canvas->makePresentationCopy();
    } else
        canvas->clearPresentationCopy();
    clearIfComposited();

    if (!m_markedCanvasDirty && !m_layerCleared)
        return;

    canvas->clearCopiedImage();
    m_markedCanvasDirty = false;

    m_context->paintRenderingResultsToCanvas(canvas->buffer());
}

RefPtr<ImageData> WebGLRenderingContextBase::paintRenderingResultsToImageData()
{
    if (isContextLostOrPending())
        return nullptr;
    clearIfComposited();
    return m_context->paintRenderingResultsToImageData();
}

WebGLTexture::TextureExtensionFlag WebGLRenderingContextBase::textureExtensionFlags() const
{
    return static_cast<WebGLTexture::TextureExtensionFlag>((m_oesTextureFloatLinear ? WebGLTexture::TextureExtensionFloatLinearEnabled : 0) | (m_oesTextureHalfFloatLinear ? WebGLTexture::TextureExtensionHalfFloatLinearEnabled : 0));
}

void WebGLRenderingContextBase::reshape(int width, int height)
{
    if (isContextLostOrPending())
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
#if !USE(ANGLE)
    if (textureUnit.texture2DBinding && textureUnit.texture2DBinding->needToUseBlackTexture(textureExtensionFlags()))
        m_unrenderableTextureUnits.add(m_activeTextureUnit);
#endif
    m_context->bindRenderbuffer(GraphicsContextGL::RENDERBUFFER, objectOrZero(m_renderbufferBinding.get()));
    if (m_framebufferBinding)
        m_context->bindFramebuffer(GraphicsContextGL::FRAMEBUFFER, objectOrZero(m_framebufferBinding.get()));
}

int WebGLRenderingContextBase::drawingBufferWidth() const
{
    if (isContextLost())
        return 0;

    if (m_isPendingPolicyResolution && !m_hasRequestedPolicyResolution)
        return 0;

    return m_context->getInternalFramebufferSize().width();
}

int WebGLRenderingContextBase::drawingBufferHeight() const
{
    if (isContextLost())
        return 0;

    if (m_isPendingPolicyResolution && !m_hasRequestedPolicyResolution)
        return 0;

    return m_context->getInternalFramebufferSize().height();
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
    }
    ASSERT_NOT_REACHED();
    return 0;
}

void WebGLRenderingContextBase::activeTexture(GCGLenum texture)
{
    if (isContextLostOrPending())
        return;
    if (texture - GraphicsContextGL::TEXTURE0 >= m_textureUnits.size()) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "activeTexture", "texture unit out of range");
        return;
    }
    m_activeTextureUnit = texture - GraphicsContextGL::TEXTURE0;
    m_context->activeTexture(texture);
}

void WebGLRenderingContextBase::attachShader(WebGLProgram* program, WebGLShader* shader)
{
    if (isContextLostOrPending() || !validateWebGLProgramOrShader("attachShader", program) || !validateWebGLProgramOrShader("attachShader", shader))
        return;
    if (!program->attachShader(shader)) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "attachShader", "shader attachment already has shader");
        return;
    }
    m_context->attachShader(objectOrZero(program), objectOrZero(shader));
    shader->onAttached();
}

void WebGLRenderingContextBase::bindAttribLocation(WebGLProgram* program, GCGLuint index, const String& name)
{
    if (isContextLostOrPending() || !validateWebGLProgramOrShader("bindAttribLocation", program))
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
    m_context->bindAttribLocation(objectOrZero(program), index, name);
}

bool WebGLRenderingContextBase::checkObjectToBeBound(const char* functionName, WebGLObject* object)
{
    if (isContextLostOrPending())
        return false;
    if (object) {
        if (!object->validate(contextGroup(), *this)) {
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "object not from this context");
            return false;
        }
        if (!object->object()) {
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "deleted objects cannot be bound");
            return false;
        }
    }
    return true;
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

bool WebGLRenderingContextBase::validateAndCacheBufferBinding(const char* functionName, GCGLenum target, WebGLBuffer* buffer)
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
        m_boundVertexArrayObject->setElementArrayBuffer(buffer);
    }

    if (buffer && !buffer->getTarget())
        buffer->setTarget(target);
    return true;
}

void WebGLRenderingContextBase::bindBuffer(GCGLenum target, WebGLBuffer* buffer)
{
    if (!checkObjectToBeBound("bindBuffer", buffer))
        return;

    if (!validateAndCacheBufferBinding("bindBuffer", target, buffer))
        return;

    m_context->bindBuffer(target, objectOrZero(buffer));
}

void WebGLRenderingContextBase::bindFramebuffer(GCGLenum target, WebGLFramebuffer* buffer)
{
    if (!checkObjectToBeBound("bindFramebuffer", buffer))
        return;

    if (target != GraphicsContextGL::FRAMEBUFFER) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "bindFramebuffer", "invalid target");
        return;
    }

    setFramebuffer(target, buffer);
}

void WebGLRenderingContextBase::bindRenderbuffer(GCGLenum target, WebGLRenderbuffer* renderBuffer)
{
    if (!checkObjectToBeBound("bindRenderbuffer", renderBuffer))
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
    if (!checkObjectToBeBound("bindTexture", texture))
        return;
    if (texture && texture->getTarget() && texture->getTarget() != target) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "bindTexture", "textures can not be used with multiple targets");
        return;
    }
    GCGLint maxLevel = 0;
    auto& textureUnit = m_textureUnits[m_activeTextureUnit];
    if (target == GraphicsContextGL::TEXTURE_2D) {
        textureUnit.texture2DBinding = texture;
        maxLevel = m_maxTextureLevel;
#if !USE(ANGLE)
        if (texture && texture->needToUseBlackTexture(textureExtensionFlags()))
            m_unrenderableTextureUnits.add(m_activeTextureUnit);
        else
            m_unrenderableTextureUnits.remove(m_activeTextureUnit);
#endif
    } else if (target == GraphicsContextGL::TEXTURE_CUBE_MAP) {
        textureUnit.textureCubeMapBinding = texture;
        maxLevel = m_maxCubeMapTextureLevel;
#if !USE(ANGLE)
        if (texture && texture->needToUseBlackTexture(textureExtensionFlags()))
            m_unrenderableTextureUnits.add(m_activeTextureUnit);
        else
            m_unrenderableTextureUnits.remove(m_activeTextureUnit);
#endif
    } else if (isWebGL2() && target == GraphicsContextGL::TEXTURE_2D_ARRAY) {
        textureUnit.texture2DArrayBinding = texture;
        // maxLevel is unused in the ANGLE backend.
    } else if (isWebGL2() && target == GraphicsContextGL::TEXTURE_3D) {
        textureUnit.texture3DBinding = texture;
        // maxLevel is unused in the ANGLE backend.
    } else {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "bindTexture", "invalid target");
        return;
    }
    m_context->bindTexture(target, objectOrZero(texture));
    if (texture)
        texture->setTarget(target, maxLevel);

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
    if (isContextLostOrPending())
        return;
    m_context->blendColor(red, green, blue, alpha);
}

void WebGLRenderingContextBase::blendEquation(GCGLenum mode)
{
    if (isContextLostOrPending() || !validateBlendEquation("blendEquation", mode))
        return;
    m_context->blendEquation(mode);
}

void WebGLRenderingContextBase::blendEquationSeparate(GCGLenum modeRGB, GCGLenum modeAlpha)
{
    if (isContextLostOrPending() || !validateBlendEquation("blendEquation", modeRGB) || !validateBlendEquation("blendEquation", modeAlpha))
        return;
    m_context->blendEquationSeparate(modeRGB, modeAlpha);
}


void WebGLRenderingContextBase::blendFunc(GCGLenum sfactor, GCGLenum dfactor)
{
    if (isContextLostOrPending() || !validateBlendFuncFactors("blendFunc", sfactor, dfactor))
        return;
    m_context->blendFunc(sfactor, dfactor);
}

void WebGLRenderingContextBase::blendFuncSeparate(GCGLenum srcRGB, GCGLenum dstRGB, GCGLenum srcAlpha, GCGLenum dstAlpha)
{
    // Note: Alpha does not have the same restrictions as RGB.
    if (isContextLostOrPending() || !validateBlendFuncFactors("blendFunc", srcRGB, dstRGB))
        return;
    m_context->blendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void WebGLRenderingContextBase::bufferData(GCGLenum target, long long size, GCGLenum usage)
{
    if (isContextLostOrPending())
        return;
    RefPtr<WebGLBuffer> buffer = validateBufferDataParameters("bufferData", target, usage);
    if (!buffer)
        return;
    if (size < 0) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "bufferData", "size < 0");
        return;
    }
    if (!size) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "bufferData", "size == 0");
        return;
    }
    if (!buffer->associateBufferData(static_cast<GCGLsizeiptr>(size))) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "bufferData", "invalid buffer");
        return;
    }

    m_context->moveErrorsToSyntheticErrorList();
    m_context->bufferData(target, static_cast<GCGLsizeiptr>(size), usage);
    if (m_context->moveErrorsToSyntheticErrorList()) {
        // The bufferData function failed. Tell the buffer it doesn't have the data it thinks it does.
        buffer->disassociateBufferData();
    }
}

void WebGLRenderingContextBase::bufferData(GCGLenum target, Optional<BufferDataSource>&& data, GCGLenum usage)
{
    if (isContextLostOrPending())
        return;
    if (!data) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "bufferData", "null data");
        return;
    }
    RefPtr<WebGLBuffer> buffer = validateBufferDataParameters("bufferData", target, usage);
    if (!buffer)
        return;

    WTF::visit([&](auto& data) {
        if (!buffer->associateBufferData(data.get())) {
            this->synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "bufferData", "invalid buffer");
            return;
        }

        m_context->moveErrorsToSyntheticErrorList();
        m_context->bufferData(target, data->byteLength(), data->data(), usage);
        if (m_context->moveErrorsToSyntheticErrorList()) {
            // The bufferData function failed. Tell the buffer it doesn't have the data it thinks it does.
            buffer->disassociateBufferData();
        }
    }, data.value());
}

void WebGLRenderingContextBase::bufferSubData(GCGLenum target, long long offset, Optional<BufferDataSource>&& data)
{
    if (isContextLostOrPending())
        return;
    RefPtr<WebGLBuffer> buffer = validateBufferDataParameters("bufferSubData", target, GraphicsContextGL::STATIC_DRAW);
    if (!buffer)
        return;
    if (offset < 0) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "bufferSubData", "offset < 0");
        return;
    }
    if (!data)
        return;

    WTF::visit([&](auto& data) {
        if (!buffer->associateBufferSubData(static_cast<GCGLintptr>(offset), data.get())) {
            this->synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "bufferSubData", "offset out of range");
            return;
        }

        m_context->moveErrorsToSyntheticErrorList();
        m_context->bufferSubData(target, static_cast<GCGLintptr>(offset), data->byteLength(), data->data());
        if (m_context->moveErrorsToSyntheticErrorList()) {
            // The bufferSubData function failed. Tell the buffer it doesn't have the data it thinks it does.
            buffer->disassociateBufferData();
        }
    }, data.value());
}

GCGLenum WebGLRenderingContextBase::checkFramebufferStatus(GCGLenum target)
{
    if (isContextLostOrPending())
        return GraphicsContextGL::FRAMEBUFFER_UNSUPPORTED;
    if (!validateFramebufferTarget(target)) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "checkFramebufferStatus", "invalid target");
        return 0;
    }
#if USE(ANGLE)
    return m_context->checkFramebufferStatus(target);
#else
    auto targetFramebuffer = getFramebufferBinding(target);

    if (!targetFramebuffer || !targetFramebuffer->object())
        return GraphicsContextGL::FRAMEBUFFER_COMPLETE;
    const char* reason = "framebuffer incomplete";
    GCGLenum result = targetFramebuffer->checkStatus(&reason);
    if (result != GraphicsContextGL::FRAMEBUFFER_COMPLETE) {
        String str = "WebGL: checkFramebufferStatus:" + String(reason);
        printToConsole(MessageLevel::Warning, str);
        return result;
    }
    result = m_context->checkFramebufferStatus(target);
    return result;
#endif
}

void WebGLRenderingContextBase::clearColor(GCGLfloat r, GCGLfloat g, GCGLfloat b, GCGLfloat a)
{
    if (isContextLostOrPending())
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
    if (isContextLostOrPending())
        return;
    m_clearDepth = depth;
    m_context->clearDepth(depth);
}

void WebGLRenderingContextBase::clearStencil(GCGLint s)
{
    if (isContextLostOrPending())
        return;
    m_clearStencil = s;
    m_context->clearStencil(s);
}

void WebGLRenderingContextBase::colorMask(GCGLboolean red, GCGLboolean green, GCGLboolean blue, GCGLboolean alpha)
{
    if (isContextLostOrPending())
        return;
    m_colorMask[0] = red;
    m_colorMask[1] = green;
    m_colorMask[2] = blue;
    m_colorMask[3] = alpha;
    m_context->colorMask(red, green, blue, alpha);
}

void WebGLRenderingContextBase::compileShader(WebGLShader* shader)
{
    if (isContextLostOrPending() || !validateWebGLProgramOrShader("compileShader", shader))
        return;
    m_context->compileShader(objectOrZero(shader));
    GCGLint value;
    m_context->getShaderiv(objectOrZero(shader), GraphicsContextGL::COMPILE_STATUS, &value);
    shader->setValid(value);

    auto* canvas = htmlCanvas();

    if (canvas && m_synthesizedErrorsToConsole && !value) {
        Ref<Inspector::ScriptCallStack> stackTrace = Inspector::createScriptCallStack(JSExecState::currentState());

        for (auto& error : getShaderInfoLog(shader).split('\n'))
            canvas->document().addConsoleMessage(makeUnique<Inspector::ConsoleMessage>(MessageSource::Rendering, MessageType::Log, MessageLevel::Error, "WebGL: " + error, stackTrace.copyRef()));
    }
}

void WebGLRenderingContextBase::compressedTexImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, ArrayBufferView& data)
{
    if (isContextLostOrPending())
        return;
#if USE(ANGLE)
    if (!validateTexture2DBinding("compressedTexImage2D", target))
        return;
    m_context->getExtensions().compressedTexImage2DRobustANGLE(target, level, internalformat, width, height,
        border, data.byteLength(), data.byteLength(), data.baseAddress());
#else
    if (!validateTexFuncLevel("compressedTexImage2D", target, level))
        return;

    if (!validateCompressedTexFormat(internalformat)) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "compressedTexImage2D", "invalid internalformat");
        return;
    }
    if (border) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "compressedTexImage2D", "border not 0");
        return;
    }
    if (!validateCompressedTexDimensions("compressedTexImage2D", target, level, width, height, internalformat))
        return;
    if (!validateCompressedTexFuncData("compressedTexImage2D", width, height, internalformat, data))
        return;

    auto tex = validateTexture2DBinding("compressedTexImage2D", target);
    if (!tex)
        return;
    if (!validateNPOTTextureLevel(width, height, level, "compressedTexImage2D"))
        return;
    m_context->moveErrorsToSyntheticErrorList();
    m_context->compressedTexImage2D(target, level, internalformat, width, height,
        border, data.byteLength(), data.baseAddress());
    if (m_context->moveErrorsToSyntheticErrorList()) {
        // The compressedTexImage2D function failed. Tell the WebGLTexture it doesn't have the data for this level.
        tex->markInvalid(target, level);
        return;
    }

    tex->setLevelInfo(target, level, internalformat, width, height, GraphicsContextGL::UNSIGNED_BYTE);
    tex->setCompressed();
#endif
}

void WebGLRenderingContextBase::compressedTexSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, ArrayBufferView& data)
{
    if (isContextLostOrPending())
        return;
#if USE(ANGLE)
    if (!validateTexture2DBinding("compressedTexSubImage2D", target))
        return;
    graphicsContextGL()->getExtensions().compressedTexSubImage2DRobustANGLE(target, level, xoffset, yoffset, width, height, format, data.byteLength(), data.byteLength(), data.baseAddress());
#else
    if (!validateTexFuncLevel("compressedTexSubImage2D", target, level))
        return;
    if (!validateCompressedTexFormat(format)) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "compressedTexSubImage2D", "invalid format");
        return;
    }
    if (!validateCompressedTexFuncData("compressedTexSubImage2D", width, height, format, data))
        return;

    auto tex = validateTexture2DBinding("compressedTexSubImage2D", target);
    if (!tex)
        return;

    if (format != tex->getInternalFormat(target, level)) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "compressedTexSubImage2D", "format does not match texture format");
        return;
    }

    if (!validateCompressedTexSubDimensions("compressedTexSubImage2D", target, level, xoffset, yoffset, width, height, format, tex.get()))
        return;

    graphicsContextGL()->compressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format, data.byteLength(), data.baseAddress());
    tex->setCompressed();
#endif
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
    if (isContextLostOrPending())
        return;
#if USE(ANGLE)
    if (!validateTexture2DBinding("copyTexSubImage2D", target))
        return;
    clearIfComposited();
    m_context->copyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
#else
    if (!validateTexFuncLevel("copyTexSubImage2D", target, level))
        return;
    auto tex = validateTexture2DBinding("copyTexSubImage2D", target);
    if (!tex)
        return;
    if (!validateSize("copyTexSubImage2D", xoffset, yoffset) || !validateSize("copyTexSubImage2D", width, height))
        return;
    // Before checking if it is in the range, check if overflow happens first.
    if (xoffset + width < 0 || yoffset + height < 0) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "copyTexSubImage2D", "bad dimensions");
        return;
    }
    if (xoffset + width > tex->getWidth(target, level) || yoffset + height > tex->getHeight(target, level)) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "copyTexSubImage2D", "rectangle out of range");
        return;
    }
    GCGLenum internalFormat = tex->getInternalFormat(target, level);
    if (!validateSettableTexInternalFormat("copyTexSubImage2D", internalFormat))
        return;
    if (!isTexInternalFormatColorBufferCombinationValid(internalFormat, getBoundReadFramebufferColorFormat())) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "copyTexSubImage2D", "framebuffer is incompatible format");
        return;
    }
    const char* reason = "framebuffer incomplete";
    if (m_framebufferBinding && !m_framebufferBinding->onAccess(graphicsContextGL(), &reason)) {
        synthesizeGLError(GraphicsContextGL::INVALID_FRAMEBUFFER_OPERATION, "copyTexSubImage2D", reason);
        return;
    }
    clearIfComposited();

    GCGLint clippedX, clippedY;
    GCGLsizei clippedWidth, clippedHeight;
    if (clip2D(x, y, width, height, getBoundReadFramebufferWidth(), getBoundReadFramebufferHeight(), &clippedX, &clippedY, &clippedWidth, &clippedHeight)) {
        GCGLenum format;
        GCGLenum type;
        if (!GraphicsContextGLOpenGL::possibleFormatAndTypeForInternalFormat(tex->getInternalFormat(target, level), format, type)) {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "copyTexSubImage2D", "Texture has unknown internal format");
            return;
        }
        UniqueArray<unsigned char> zero;
        if (width && height) {
            unsigned size;
            GCGLenum error = m_context->computeImageSizeInBytes(format, type, width, height, 1, getUnpackPixelStoreParams(TexImageDimension::Tex2D), &size, nullptr, nullptr);
            if (error != GraphicsContextGL::NO_ERROR) {
                synthesizeGLError(error, "copyTexSubImage2D", "bad dimensions");
                return;
            }
            zero = makeUniqueArray<unsigned char>(size);
            if (!zero) {
                synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "copyTexSubImage2D", "out of memory");
                return;
            }
            memset(zero.get(), 0, size);
        }
        m_context->texSubImage2D(target, level, xoffset, yoffset, width, height, format, type, zero.get());
        if (clippedWidth > 0 && clippedHeight > 0)
            m_context->copyTexSubImage2D(target, level, xoffset + clippedX - x, yoffset + clippedY - y, clippedX, clippedY, clippedWidth, clippedHeight);
    } else
        m_context->copyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
#endif
}

RefPtr<WebGLBuffer> WebGLRenderingContextBase::createBuffer()
{
    if (isContextLostOrPending())
        return nullptr;
    auto buffer = WebGLBuffer::create(*this);
    addSharedObject(buffer.get());
    return buffer;
}

RefPtr<WebGLFramebuffer> WebGLRenderingContextBase::createFramebuffer()
{
    if (isContextLostOrPending())
        return nullptr;
    auto buffer = WebGLFramebuffer::create(*this);
    addContextObject(buffer.get());
    return buffer;
}

RefPtr<WebGLTexture> WebGLRenderingContextBase::createTexture()
{
    if (isContextLostOrPending())
        return nullptr;
    auto texture = WebGLTexture::create(*this);
    addSharedObject(texture.get());
    return texture;
}

RefPtr<WebGLProgram> WebGLRenderingContextBase::createProgram()
{
    if (isContextLostOrPending())
        return nullptr;
    auto program = WebGLProgram::create(*this);
    addSharedObject(program.get());

    InspectorInstrumentation::didCreateWebGLProgram(*this, program.get());

    return program;
}

RefPtr<WebGLRenderbuffer> WebGLRenderingContextBase::createRenderbuffer()
{
    if (isContextLostOrPending())
        return nullptr;
    auto buffer = WebGLRenderbuffer::create(*this);
    addSharedObject(buffer.get());
    return buffer;
}

RefPtr<WebGLShader> WebGLRenderingContextBase::createShader(GCGLenum type)
{
    if (isContextLostOrPending())
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
    if (isContextLostOrPending())
        return;
    m_context->cullFace(mode);
}

bool WebGLRenderingContextBase::deleteObject(WebGLObject* object)
{
    if (isContextLostOrPending() || !object)
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
        object->deleteObject(graphicsContextGL());
    return true;
}

#define REMOVE_BUFFER_FROM_BINDING(binding) \
    if (binding == buffer) \
        binding = nullptr;

void WebGLRenderingContextBase::uncacheDeletedBuffer(WebGLBuffer* buffer)
{
    REMOVE_BUFFER_FROM_BINDING(m_boundArrayBuffer);

    m_boundVertexArrayObject->unbindBuffer(*buffer);
}

#undef REMOVE_BUFFER_FROM_BINDING

void WebGLRenderingContextBase::deleteBuffer(WebGLBuffer* buffer)
{
    if (!deleteObject(buffer))
        return;

    uncacheDeletedBuffer(buffer);
}

void WebGLRenderingContextBase::deleteFramebuffer(WebGLFramebuffer* framebuffer)
{
    if (!deleteObject(framebuffer))
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

    deleteObject(program);
    // We don't reset m_currentProgram to 0 here because the deletion of the
    // current program is delayed.
}

void WebGLRenderingContextBase::deleteRenderbuffer(WebGLRenderbuffer* renderbuffer)
{
    if (!deleteObject(renderbuffer))
        return;
    if (renderbuffer == m_renderbufferBinding)
        m_renderbufferBinding = nullptr;
    if (m_framebufferBinding)
        m_framebufferBinding->removeAttachmentFromBoundFramebuffer(GraphicsContextGL::FRAMEBUFFER, renderbuffer);
    auto readFramebufferBinding = getFramebufferBinding(GraphicsContextGL::READ_FRAMEBUFFER);
    if (readFramebufferBinding)
        readFramebufferBinding->removeAttachmentFromBoundFramebuffer(GraphicsContextGL::READ_FRAMEBUFFER, renderbuffer);
}

void WebGLRenderingContextBase::deleteShader(WebGLShader* shader)
{
    deleteObject(shader);
}

void WebGLRenderingContextBase::deleteTexture(WebGLTexture* texture)
{
    if (!deleteObject(texture))
        return;

    unsigned current = 0;
    for (auto& textureUnit : m_textureUnits) {
        if (texture == textureUnit.texture2DBinding) {
            textureUnit.texture2DBinding = nullptr;
#if !USE(ANGLE)
            m_unrenderableTextureUnits.remove(current);
#endif
        }
        if (texture == textureUnit.textureCubeMapBinding) {
            textureUnit.textureCubeMapBinding = nullptr;
#if !USE(ANGLE)
            m_unrenderableTextureUnits.remove(current);
#endif
        }
        if (isWebGL2()) {
            if (texture == textureUnit.texture3DBinding)
                textureUnit.texture3DBinding = nullptr;
            if (texture == textureUnit.texture2DArrayBinding)
                textureUnit.texture2DArrayBinding = nullptr;
        }
        ++current;
    }
    if (m_framebufferBinding)
        m_framebufferBinding->removeAttachmentFromBoundFramebuffer(GraphicsContextGL::FRAMEBUFFER, texture);
    auto readFramebufferBinding = getFramebufferBinding(GraphicsContextGL::READ_FRAMEBUFFER);
    if (readFramebufferBinding)
        readFramebufferBinding->removeAttachmentFromBoundFramebuffer(GraphicsContextGL::READ_FRAMEBUFFER, texture);
}

void WebGLRenderingContextBase::depthFunc(GCGLenum func)
{
    if (isContextLostOrPending())
        return;
    m_context->depthFunc(func);
}

void WebGLRenderingContextBase::depthMask(GCGLboolean flag)
{
    if (isContextLostOrPending())
        return;
    m_depthMask = flag;
    m_context->depthMask(flag);
}

void WebGLRenderingContextBase::depthRange(GCGLfloat zNear, GCGLfloat zFar)
{
    if (isContextLostOrPending())
        return;
    if (zNear > zFar) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "depthRange", "zNear > zFar");
        return;
    }
    m_context->depthRange(zNear, zFar);
}

void WebGLRenderingContextBase::detachShader(WebGLProgram* program, WebGLShader* shader)
{
    if (isContextLostOrPending() || !validateWebGLProgramOrShader("detachShader", program) || !validateWebGLProgramOrShader("detachShader", shader))
        return;
    if (!program->detachShader(shader)) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "detachShader", "shader not attached");
        return;
    }
    m_context->detachShader(objectOrZero(program), objectOrZero(shader));
    shader->onDetached(graphicsContextGL());
}

void WebGLRenderingContextBase::disable(GCGLenum cap)
{
    if (isContextLostOrPending() || !validateCapability("disable", cap))
        return;
    if (cap == GraphicsContextGL::STENCIL_TEST) {
        m_stencilEnabled = false;
        applyStencilTest();
        return;
    }
    if (cap == GraphicsContextGL::SCISSOR_TEST)
        m_scissorEnabled = false;
    m_context->disable(cap);
}

void WebGLRenderingContextBase::disableVertexAttribArray(GCGLuint index)
{
    if (isContextLostOrPending())
        return;
    if (index >= m_maxVertexAttribs) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "disableVertexAttribArray", "index out of range");
        return;
    }

    WebGLVertexArrayObjectBase::VertexAttribState& state = m_boundVertexArrayObject->getVertexAttribState(index);
    state.enabled = false;

#if !USE(ANGLE)
    if (index > 0 || isGLES2Compliant())
#endif
        m_context->disableVertexAttribArray(index);
}

#if !USE(ANGLE)
bool WebGLRenderingContextBase::validateNPOTTextureLevel(GCGLsizei width, GCGLsizei height, GCGLint level, const char* functionName)
{
    if (!isGLES2NPOTStrict() && level && WebGLTexture::isNPOT(width, height)) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "level > 0 not power of 2");
        return false;
    }

    return true;
}
#endif

bool WebGLRenderingContextBase::validateElementArraySize(GCGLsizei count, GCGLenum type, GCGLintptr offset)
{
    RefPtr<WebGLBuffer> elementArrayBuffer = m_boundVertexArrayObject->getElementArrayBuffer();
    
    if (!elementArrayBuffer)
        return false;

    if (offset < 0)
        return false;

    if (type == GraphicsContextGL::UNSIGNED_INT) {
        // For an unsigned int array, offset must be divisible by 4 for alignment reasons.
        if (offset % 4)
            return false;

        // Make uoffset an element offset.
        offset /= 4;

        GCGLsizeiptr n = elementArrayBuffer->byteLength() / 4;
        if (offset > n || count > n - offset)
            return false;
    } else if (type == GraphicsContextGL::UNSIGNED_SHORT) {
        // For an unsigned short array, offset must be divisible by 2 for alignment reasons.
        if (offset % 2)
            return false;

        // Make uoffset an element offset.
        offset /= 2;

        GCGLsizeiptr n = elementArrayBuffer->byteLength() / 2;
        if (offset > n || count > n - offset)
            return false;
    } else if (type == GraphicsContextGL::UNSIGNED_BYTE) {
        GCGLsizeiptr n = elementArrayBuffer->byteLength();
        if (offset > n || count > n - offset)
            return false;
    }
    return true;
}

bool WebGLRenderingContextBase::validateIndexArrayPrecise(GCGLsizei count, GCGLenum type, GCGLintptr offset, unsigned& numElementsRequired)
{
    ASSERT(count >= 0 && offset >= 0);
    unsigned maxIndex = 0;
    
    RefPtr<WebGLBuffer> elementArrayBuffer = m_boundVertexArrayObject->getElementArrayBuffer();

    if (!elementArrayBuffer)
        return false;

    if (!count) {
        numElementsRequired = 0;
        return true;
    }

    auto buffer = elementArrayBuffer->elementArrayBuffer();
    if (!buffer)
        return false;

    switch (type) {
    case GraphicsContextGL::UNSIGNED_INT:
        maxIndex = getMaxIndex<GCGLuint>(buffer, offset, count);
        break;
    case GraphicsContextGL::UNSIGNED_SHORT:
        maxIndex = getMaxIndex<GCGLushort>(buffer, offset, count);
        break;
    case GraphicsContextGL::UNSIGNED_BYTE:
        maxIndex = getMaxIndex<GCGLubyte>(buffer, offset, count);
        break;
    }

    // Then set the maxiumum index in the index array and make sure it is valid.
    auto checkedNumElementsRequired = checkedAddAndMultiply<unsigned>(maxIndex, 1, 1);
    if (!checkedNumElementsRequired)
        return false;
    numElementsRequired = checkedNumElementsRequired.value();
    return true;
}

bool WebGLRenderingContextBase::validateVertexAttributes(unsigned elementCount, unsigned primitiveCount)
{
#if USE(ANGLE)
    UNUSED_PARAM(elementCount);
    UNUSED_PARAM(primitiveCount);
#else
    if (!m_currentProgram)
        return false;

    // Look in each enabled vertex attrib and check if they've been bound to a buffer.
    for (unsigned i = 0; i < m_maxVertexAttribs; ++i) {
        if (!m_boundVertexArrayObject->getVertexAttribState(i).validateBinding())
            return false;
    }

    if (!elementCount)
        return true;

    // Look in each consumed vertex attrib (by the current program).
    bool sawNonInstancedAttrib = false;
    bool sawEnabledAttrib = false;
    int numActiveAttribLocations = m_currentProgram->numActiveAttribLocations();
    for (int i = 0; i < numActiveAttribLocations; ++i) {
        int loc = m_currentProgram->getActiveAttribLocation(i);
        if (loc >= 0 && loc < static_cast<int>(m_maxVertexAttribs)) {
            const WebGLVertexArrayObjectBase::VertexAttribState& state = m_boundVertexArrayObject->getVertexAttribState(loc);
            if (state.enabled) {
                sawEnabledAttrib = true;
                // Avoid off-by-one errors in numElements computation.
                // For the last element, we will only touch the data for the
                // element and nothing beyond it.
                int bytesRemaining = static_cast<int>(state.bufferBinding->byteLength() - state.offset);
                if (bytesRemaining <= 0)
                    return false;
                unsigned numElements = 0;
                ASSERT(state.stride > 0);
                if (bytesRemaining >= state.bytesPerElement)
                    numElements = 1 + (bytesRemaining - state.bytesPerElement) / state.stride;
                unsigned instancesRequired = 0;
                if (state.divisor) {
                    instancesRequired = ceil(static_cast<float>(primitiveCount) / state.divisor);
                    if (instancesRequired > numElements)
                        return false;
                } else {
                    sawNonInstancedAttrib = true;
                    if (elementCount > numElements)
                        return false;
                }
            }
        }
    }

    if (!sawNonInstancedAttrib && sawEnabledAttrib)
        return false;

    bool usingSimulatedArrayBuffer = m_currentProgram->isUsingVertexAttrib0();

    // Guard against access into non-existent buffers.
    if (elementCount && !sawEnabledAttrib && !usingSimulatedArrayBuffer)
        return false;

    if (elementCount && sawEnabledAttrib) {
        if (!m_boundArrayBuffer && !m_boundVertexArrayObject->getElementArrayBuffer()) {
            if (usingSimulatedArrayBuffer) {
                auto& state = m_boundVertexArrayObject->getVertexAttribState(0);
                if (state.enabled && state.isBound()) {
                    if (state.bufferBinding->getTarget() == GraphicsContextGL::ARRAY_BUFFER || state.bufferBinding->getTarget() == GraphicsContextGL::ELEMENT_ARRAY_BUFFER)
                        return !!state.bufferBinding->byteLength();
                }
            }
            return false;
        }
    }
#endif

    return true;
}

bool WebGLRenderingContextBase::validateWebGLObject(const char* functionName, WebGLObject* object)
{
    if (!object) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "object is null");
        return false;
    }
    if (!object->object()) {
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
    if (!object) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "program or shader is null");
        return false;
    }
    // Using a deleted program or shader is INVALID_VALUE instead of INVALID_OPERATION as for
    // other WebGL objects.
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

#if !USE(ANGLE)
bool WebGLRenderingContextBase::validateDrawArrays(const char* functionName, GCGLenum mode, GCGLint first, GCGLsizei count, GCGLsizei primitiveCount)
{
    if (isContextLostOrPending() || !validateDrawMode(functionName, mode))
        return false;

    if (!validateStencilSettings(functionName))
        return false;

    if (first < 0 || count < 0) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "first or count < 0");
        return false;
    }

    if (!count) {
        markContextChanged();
        return false;
    }

    if (primitiveCount < 0) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "primcount < 0");
        return false;
    }

    // Ensure we have a valid rendering state.
    Checked<GCGLint, RecordOverflow> checkedSum = Checked<GCGLint, RecordOverflow>(first) + Checked<GCGLint, RecordOverflow>(count);
    Checked<GCGLint, RecordOverflow> checkedPrimitiveCount(primitiveCount);
    if (checkedSum.hasOverflowed() || checkedPrimitiveCount.hasOverflowed() || !validateVertexAttributes(checkedSum.unsafeGet(), checkedPrimitiveCount.unsafeGet())) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "attempt to access out of bounds arrays");
        return false;
    }
#if !USE(ANGLE)
    if (!validateSimulatedVertexAttrib0(checkedSum.unsafeGet() - 1)) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "attempt to access outside the bounds of the simulated vertexAttrib0 array");
        return false;
    }
#endif

    const char* reason = "framebuffer incomplete";
    if (m_framebufferBinding && !m_framebufferBinding->onAccess(graphicsContextGL(), &reason)) {
        synthesizeGLError(GraphicsContextGL::INVALID_FRAMEBUFFER_OPERATION, functionName, reason);
        return false;
    }

    return true;
}

bool WebGLRenderingContextBase::validateDrawElements(const char* functionName, GCGLenum mode, GCGLsizei count, GCGLenum type, long long offset, unsigned& numElements, GCGLsizei primitiveCount)
{
    if (isContextLostOrPending() || !validateDrawMode(functionName, mode))
        return false;
    
    if (!validateStencilSettings(functionName))
        return false;
    
    switch (type) {
    case GraphicsContextGL::UNSIGNED_BYTE:
    case GraphicsContextGL::UNSIGNED_SHORT:
        break;
    case GraphicsContextGL::UNSIGNED_INT:
        if (m_oesElementIndexUint || isWebGL2())
            break;
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid type");
        return false;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid type");
        return false;
    }
    
    if (count < 0 || offset < 0) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "count or offset < 0");
        return false;
    }
    
    if (!count) {
        markContextChanged();
        return false;
    }
    
    if (primitiveCount < 0) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "primcount < 0");
        return false;
    }
    
    if (!m_boundVertexArrayObject->getElementArrayBuffer()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "no ELEMENT_ARRAY_BUFFER bound");
        return false;
    }

    // Ensure we have a valid rendering state.
    if (!validateElementArraySize(count, type, static_cast<GCGLintptr>(offset))) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "request out of bounds for current ELEMENT_ARRAY_BUFFER");
        return false;
    }
    if (!count)
        return false;
    
    Checked<GCGLint, RecordOverflow> checkedCount(count);
    Checked<GCGLint, RecordOverflow> checkedPrimitiveCount(primitiveCount);
    if (checkedCount.hasOverflowed() || checkedPrimitiveCount.hasOverflowed()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "attempt to access out of bounds arrays");
        return false;
    }
    
    if (!validateIndexArrayConservative(type, numElements) || !validateVertexAttributes(numElements, checkedPrimitiveCount.unsafeGet())) {
        if (!validateIndexArrayPrecise(checkedCount.unsafeGet(), type, static_cast<GCGLintptr>(offset), numElements) || !validateVertexAttributes(numElements, checkedPrimitiveCount.unsafeGet())) {
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "attempt to access out of bounds arrays");
            return false;
        }
    }

#if !USE(ANGLE)
    if (!validateSimulatedVertexAttrib0(numElements)) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "attempt to access outside the bounds of the simulated vertexAttrib0 array");
        return false;
    }
#endif
    
    const char* reason = "framebuffer incomplete";
    if (m_framebufferBinding && !m_framebufferBinding->onAccess(graphicsContextGL(), &reason)) {
        synthesizeGLError(GraphicsContextGL::INVALID_FRAMEBUFFER_OPERATION, functionName, reason);
        return false;
    }
    
    return true;
}
#endif

void WebGLRenderingContextBase::drawArrays(GCGLenum mode, GCGLint first, GCGLsizei count)
{
#if USE(ANGLE)
    if (isContextLostOrPending())
        return;
#else
    if (!validateDrawArrays("drawArrays", mode, first, count, 0))
        return;
#endif

    if (m_currentProgram && InspectorInstrumentation::isWebGLProgramDisabled(*this, *m_currentProgram))
        return;

    clearIfComposited();

#if !USE(ANGLE)
    bool vertexAttrib0Simulated = false;
    if (!isGLES2Compliant()) {
        auto simulateVertexAttrib0Status = simulateVertexAttrib0(first + count - 1);
        if (!simulateVertexAttrib0Status) {
            // We were unable to simulate the attribute buffer.
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "drawArrays", "unable to simulate vertexAttrib0 array");
            return;
        }
        vertexAttrib0Simulated = simulateVertexAttrib0Status.value();
    }
    bool usesFallbackTexture = false;
    if (!isGLES2NPOTStrict())
        usesFallbackTexture = checkTextureCompleteness("drawArrays", true);
#endif

    {
        InspectorScopedShaderProgramHighlight scopedHighlight(*this, m_currentProgram.get());

        m_context->drawArrays(mode, first, count);
    }

#if !USE(ANGLE)
    if (!isGLES2Compliant() && vertexAttrib0Simulated)
        restoreStatesAfterVertexAttrib0Simulation();
    if (usesFallbackTexture)
        checkTextureCompleteness("drawArrays", false);
#endif
    markContextChangedAndNotifyCanvasObserver();
}

#if USE(OPENGL) && ENABLE(WEBGL2)
static GCGLuint getRestartIndex(GCGLenum type)
{
    switch (type) {
    case GraphicsContextGL::UNSIGNED_BYTE:
        return std::numeric_limits<GCGLubyte>::max();
    case GraphicsContextGL::UNSIGNED_SHORT:
        return std::numeric_limits<GCGLushort>::max();
    case GraphicsContextGL::UNSIGNED_INT:
        return std::numeric_limits<GCGLuint>::max();
    }

    return 0;
}
#endif

void WebGLRenderingContextBase::drawElements(GCGLenum mode, GCGLsizei count, GCGLenum type, long long offset)
{
#if USE(ANGLE)
    if (isContextLostOrPending())
        return;
#else
    unsigned numElements = 0;
    if (!validateDrawElements("drawElements", mode, count, type, offset, numElements, 0))
        return;
#endif

    if (m_currentProgram && InspectorInstrumentation::isWebGLProgramDisabled(*this, *m_currentProgram))
        return;

    clearIfComposited();

#if !USE(ANGLE)
    bool vertexAttrib0Simulated = false;
    if (!isGLES2Compliant()) {
        if (!numElements)
            validateIndexArrayPrecise(count, type, static_cast<GCGLintptr>(offset), numElements);
        auto simulateVertexAttrib0Status = simulateVertexAttrib0(numElements);
        if (!simulateVertexAttrib0Status) {
            // We were unable to simulate the attribute buffer.
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "drawElements", "unable to simulate vertexAttrib0 array");
            return;
        }
        vertexAttrib0Simulated = simulateVertexAttrib0Status.value();
    }
    bool usesFallbackTexture = false;
    if (!isGLES2NPOTStrict())
        usesFallbackTexture = checkTextureCompleteness("drawElements", true);
#endif

#if USE(OPENGL) && ENABLE(WEBGL2)
    if (isWebGL2())
        m_context->primitiveRestartIndex(getRestartIndex(type));
#endif

    {
        InspectorScopedShaderProgramHighlight scopedHighlight(*this, m_currentProgram.get());

        m_context->drawElements(mode, count, type, static_cast<GCGLintptr>(offset));
    }

#if !USE(ANGLE)
    if (!isGLES2Compliant() && vertexAttrib0Simulated)
        restoreStatesAfterVertexAttrib0Simulation();
    if (usesFallbackTexture)
        checkTextureCompleteness("drawElements", false);
#endif
    markContextChangedAndNotifyCanvasObserver();
}

void WebGLRenderingContextBase::enable(GCGLenum cap)
{
    if (isContextLostOrPending() || !validateCapability("enable", cap))
        return;
    if (cap == GraphicsContextGL::STENCIL_TEST) {
        m_stencilEnabled = true;
        applyStencilTest();
        return;
    }
    if (cap == GraphicsContextGL::SCISSOR_TEST)
        m_scissorEnabled = true;
    m_context->enable(cap);
}

void WebGLRenderingContextBase::enableVertexAttribArray(GCGLuint index)
{
    if (isContextLostOrPending())
        return;
    if (index >= m_maxVertexAttribs) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "enableVertexAttribArray", "index out of range");
        return;
    }

    WebGLVertexArrayObjectBase::VertexAttribState& state = m_boundVertexArrayObject->getVertexAttribState(index);
    state.enabled = true;

    m_context->enableVertexAttribArray(index);
}

void WebGLRenderingContextBase::finish()
{
    if (isContextLostOrPending())
        return;
    m_context->finish();
}

void WebGLRenderingContextBase::flush()
{
    if (isContextLostOrPending())
        return;
    m_context->flush();
}

void WebGLRenderingContextBase::framebufferRenderbuffer(GCGLenum target, GCGLenum attachment, GCGLenum renderbuffertarget, WebGLRenderbuffer* buffer)
{
    if (isContextLostOrPending() || !validateFramebufferFuncParameters("framebufferRenderbuffer", target, attachment))
        return;
    if (renderbuffertarget != GraphicsContextGL::RENDERBUFFER) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "framebufferRenderbuffer", "invalid target");
        return;
    }
    if (buffer && !buffer->validate(contextGroup(), *this)) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "framebufferRenderbuffer", "no buffer or buffer not from this context");
        return;
    }
    if (buffer && !buffer->hasEverBeenBound()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "framebufferRenderbuffer", "buffer has never been bound");
        return;
    }

    auto targetFramebuffer = (target == GraphicsContextGL::READ_FRAMEBUFFER) ? getReadFramebufferBinding() : m_framebufferBinding;

    // Don't allow the default framebuffer to be mutated; all current
    // implementations use an FBO internally in place of the default
    // FBO.
    if (!targetFramebuffer || !targetFramebuffer->object()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "framebufferRenderbuffer", "no framebuffer bound");
        return;
    }
    PlatformGLObject bufferObject = objectOrZero(buffer);
#if !USE(ANGLE)
    if (attachment == GraphicsContextGL::DEPTH_STENCIL_ATTACHMENT) {
        m_context->framebufferRenderbuffer(target, GraphicsContextGL::DEPTH_ATTACHMENT, renderbuffertarget, bufferObject);
        m_context->framebufferRenderbuffer(target, GraphicsContextGL::STENCIL_ATTACHMENT, renderbuffertarget, bufferObject);
    } else
#endif
        m_context->framebufferRenderbuffer(target, attachment, renderbuffertarget, bufferObject);
    targetFramebuffer->setAttachmentForBoundFramebuffer(target, attachment, buffer);
    applyStencilTest();
}

void WebGLRenderingContextBase::framebufferTexture2D(GCGLenum target, GCGLenum attachment, GCGLenum textarget, WebGLTexture* texture, GCGLint level)
{
    if (isContextLostOrPending() || !validateFramebufferFuncParameters("framebufferTexture2D", target, attachment))
        return;
    if (level && isWebGL1()) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "framebufferTexture2D", "level not 0");
        return;
    }
    if (texture && !texture->validate(contextGroup(), *this)) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "framebufferTexture2D", "no texture or texture not from this context");
        return;
    }

    auto targetFramebuffer = (target == GraphicsContextGL::READ_FRAMEBUFFER) ? getReadFramebufferBinding() : m_framebufferBinding;

    // Don't allow the default framebuffer to be mutated; all current
    // implementations use an FBO internally in place of the default
    // FBO.
    if (!targetFramebuffer || !targetFramebuffer->object()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "framebufferTexture2D", "no framebuffer bound");
        return;
    }
    PlatformGLObject textureObject = objectOrZero(texture);

#if !USE_ANGLE
    if (attachment == GraphicsContextGL::DEPTH_STENCIL_ATTACHMENT) {
        m_context->framebufferTexture2D(target, GraphicsContextGL::DEPTH_ATTACHMENT, textarget, textureObject, level);
        m_context->framebufferTexture2D(target, GraphicsContextGL::STENCIL_ATTACHMENT, textarget, textureObject, level);
    } else
#endif
        m_context->framebufferTexture2D(target, attachment, textarget, textureObject, level);

    targetFramebuffer->setAttachmentForBoundFramebuffer(target, attachment, textarget, texture, level);
    applyStencilTest();
}

void WebGLRenderingContextBase::frontFace(GCGLenum mode)
{
    if (isContextLostOrPending())
        return;
    m_context->frontFace(mode);
}

void WebGLRenderingContextBase::generateMipmap(GCGLenum target)
{
    if (isContextLostOrPending())
        return;
#if USE(ANGLE)
    if (!validateTextureBinding("generateMipmap", target))
        return;
    m_context->generateMipmap(target);
#else
    auto tex = validateTextureBinding("generateMipmap", target);
    if (!tex)
        return;
    if (!tex->canGenerateMipmaps()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "generateMipmap", "level 0 not power of 2 or not all the same size");
        return;
    }
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=123916. Compressed textures should be allowed in WebGL 2:
    if (tex->isCompressed()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "generateMipmap", "trying to generate mipmaps from compressed texture");
        return;
    }
    if (!validateSettableTexInternalFormat("generateMipmap", tex->getInternalFormat(target, 0)))
        return;

    // generateMipmap won't work properly if minFilter is not NEAREST_MIPMAP_LINEAR
    // on Mac.  Remove the hack once this driver bug is fixed.
#if OS(DARWIN)
    bool needToResetMinFilter = false;
    if (tex->getMinFilter() != GraphicsContextGL::NEAREST_MIPMAP_LINEAR) {
        m_context->texParameteri(target, GraphicsContextGL::TEXTURE_MIN_FILTER, GraphicsContextGL::NEAREST_MIPMAP_LINEAR);
        needToResetMinFilter = true;
    }
#endif
    m_context->generateMipmap(target);
#if OS(DARWIN)
    if (needToResetMinFilter)
        m_context->texParameteri(target, GraphicsContextGL::TEXTURE_MIN_FILTER, tex->getMinFilter());
#endif
    tex->generateMipmapLevelInfo();
#endif
}

RefPtr<WebGLActiveInfo> WebGLRenderingContextBase::getActiveAttrib(WebGLProgram* program, GCGLuint index)
{
    if (isContextLostOrPending() || !validateWebGLProgramOrShader("getActiveAttrib", program))
        return nullptr;
    GraphicsContextGL::ActiveInfo info;
    if (!m_context->getActiveAttrib(objectOrZero(program), index, info))
        return nullptr;

    LOG(WebGL, "Returning active attribute %d: %s", index, info.name.utf8().data());

    return WebGLActiveInfo::create(info.name, info.type, info.size);
}

RefPtr<WebGLActiveInfo> WebGLRenderingContextBase::getActiveUniform(WebGLProgram* program, GCGLuint index)
{
    if (isContextLostOrPending() || !validateWebGLProgramOrShader("getActiveUniform", program))
        return nullptr;
    GraphicsContextGL::ActiveInfo info;
    if (!m_context->getActiveUniform(objectOrZero(program), index, info))
        return nullptr;
    // FIXME: Do we still need this for the ANGLE backend?
    if (!isGLES2Compliant())
        if (info.size > 1 && !info.name.endsWith("[0]"))
            info.name.append("[0]");

    LOG(WebGL, "Returning active uniform %d: %s", index, info.name.utf8().data());

    return WebGLActiveInfo::create(info.name, info.type, info.size);
}

Optional<Vector<RefPtr<WebGLShader>>> WebGLRenderingContextBase::getAttachedShaders(WebGLProgram* program)
{
    if (isContextLostOrPending() || !validateWebGLProgramOrShader("getAttachedShaders", program))
        return WTF::nullopt;

    const GCGLenum shaderTypes[] = {
        GraphicsContextGL::VERTEX_SHADER,
        GraphicsContextGL::FRAGMENT_SHADER
    };
    Vector<RefPtr<WebGLShader>> shaderObjects;
    for (auto shaderType : shaderTypes) {
        RefPtr<WebGLShader> shader = program->getAttachedShader(shaderType);
        if (shader)
            shaderObjects.append(shader);
    }
    return shaderObjects;
}

GCGLint WebGLRenderingContextBase::getAttribLocation(WebGLProgram* program, const String& name)
{
    if (isContextLostOrPending() || !validateWebGLProgramOrShader("getAttribLocation", program))
        return -1;
    if (!validateLocationLength("getAttribLocation", name))
        return -1;
    if (!validateString("getAttribLocation", name))
        return -1;
    if (isPrefixReserved(name))
        return -1;
    if (!program->getLinkStatus()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "getAttribLocation", "program not linked");
        return -1;
    }
    return m_context->getAttribLocation(objectOrZero(program), name);
}

WebGLAny WebGLRenderingContextBase::getBufferParameter(GCGLenum target, GCGLenum pname)
{
    if (isContextLostOrPending())
        return nullptr;

    bool valid = false;
    if (target == GraphicsContextGL::ARRAY_BUFFER || target == GraphicsContextGL::ELEMENT_ARRAY_BUFFER)
        valid = true;
#if ENABLE(WEBGL2)
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
#endif
    if (!valid) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getBufferParameter", "invalid target");
        return nullptr;
    }

    if (pname != GraphicsContextGL::BUFFER_SIZE && pname != GraphicsContextGL::BUFFER_USAGE) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getBufferParameter", "invalid parameter name");
        return nullptr;
    }

    GCGLint value = 0;
    m_context->getBufferParameteriv(target, pname, &value);
    if (pname == GraphicsContextGL::BUFFER_SIZE)
        return value;
    return static_cast<unsigned>(value);
}

Optional<WebGLContextAttributes> WebGLRenderingContextBase::getContextAttributes()
{
    if (isContextLostOrPending())
        return WTF::nullopt;

    // Also, we need to enforce requested values of "false" for depth
    // and stencil, regardless of the properties of the underlying
    // GraphicsContextGLOpenGL.

    auto attributes = m_context->contextAttributes();
    if (!m_attributes.depth)
        attributes.depth = false;
    if (!m_attributes.stencil)
        attributes.stencil = false;
    return attributes;
}

GCGLenum WebGLRenderingContextBase::getError()
{
    if (m_isPendingPolicyResolution)
        return GraphicsContextGL::NO_ERROR;
    return m_context->getError();
}

WebGLAny WebGLRenderingContextBase::getParameter(GCGLenum pname)
{
    if (isContextLostOrPending())
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
        return makeRefPtr(m_boundVertexArrayObject->getElementArrayBuffer());
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
        if (!RuntimeEnabledFeatures::sharedFeatures().maskWebGLStringsEnabled())
            return "WebGL GLSL ES 1.0 (" + m_context->getString(GraphicsContextGL::SHADING_LANGUAGE_VERSION) + ")";
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
    case ExtensionsGL::FRAGMENT_SHADER_DERIVATIVE_HINT_OES: // OES_standard_derivatives
        if (m_oesStandardDerivatives)
            return getUnsignedIntParameter(ExtensionsGL::FRAGMENT_SHADER_DERIVATIVE_HINT_OES);
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getParameter", "invalid parameter name, OES_standard_derivatives not enabled");
        return nullptr;
    case WebGLDebugRendererInfo::UNMASKED_RENDERER_WEBGL:
        if (m_webglDebugRendererInfo) {
#if !PLATFORM(IOS_FAMILY)
            if (!RuntimeEnabledFeatures::sharedFeatures().maskWebGLStringsEnabled())
                return m_context->getString(GraphicsContextGL::RENDERER);
#endif
            return "Apple GPU"_str;
        }
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getParameter", "invalid parameter name, WEBGL_debug_renderer_info not enabled");
        return nullptr;
    case WebGLDebugRendererInfo::UNMASKED_VENDOR_WEBGL:
        if (m_webglDebugRendererInfo) {
#if !PLATFORM(IOS_FAMILY)
            if (!RuntimeEnabledFeatures::sharedFeatures().maskWebGLStringsEnabled())
                return m_context->getString(GraphicsContextGL::VENDOR);
#endif
            return "Apple Inc."_str;
        }
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getParameter", "invalid parameter name, WEBGL_debug_renderer_info not enabled");
        return nullptr;
    case ExtensionsGL::VERTEX_ARRAY_BINDING_OES: // OES_vertex_array_object
        if (m_oesVertexArrayObject) {
            if (m_boundVertexArrayObject->isDefaultObject())
                return nullptr;
            return makeRefPtr(static_cast<WebGLVertexArrayObjectOES&>(*m_boundVertexArrayObject));
        }
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getParameter", "invalid parameter name, OES_vertex_array_object not enabled");
        return nullptr;
    case ExtensionsGL::MAX_TEXTURE_MAX_ANISOTROPY_EXT: // EXT_texture_filter_anisotropic
        if (m_extTextureFilterAnisotropic)
            return getUnsignedIntParameter(ExtensionsGL::MAX_TEXTURE_MAX_ANISOTROPY_EXT);
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getParameter", "invalid parameter name, EXT_texture_filter_anisotropic not enabled");
        return nullptr;
    case ExtensionsGL::MAX_COLOR_ATTACHMENTS_EXT: // EXT_draw_buffers BEGIN
        if (m_webglDrawBuffers || isWebGL2())
            return getMaxColorAttachments();
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getParameter", "invalid parameter name, WEBGL_draw_buffers not enabled");
        return nullptr;
    case ExtensionsGL::MAX_DRAW_BUFFERS_EXT:
        if (m_webglDrawBuffers || isWebGL2())
            return getMaxDrawBuffers();
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getParameter", "invalid parameter name, WEBGL_draw_buffers not enabled");
        return nullptr;
    default:
        if ((m_webglDrawBuffers || isWebGL2())
            && pname >= ExtensionsGL::DRAW_BUFFER0_EXT
            && pname < static_cast<GCGLenum>(ExtensionsGL::DRAW_BUFFER0_EXT + getMaxDrawBuffers())) {
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

WebGLAny WebGLRenderingContextBase::getProgramParameter(WebGLProgram* program, GCGLenum pname)
{
    if (isContextLostOrPending() || !validateWebGLProgramOrShader("getProgramParameter", program))
        return nullptr;

    GCGLint value = 0;
    switch (pname) {
    case GraphicsContextGL::DELETE_STATUS:
        return program->isDeleted();
    case GraphicsContextGL::VALIDATE_STATUS:
        m_context->getProgramiv(objectOrZero(program), pname, &value);
        return static_cast<bool>(value);
    case GraphicsContextGL::LINK_STATUS:
        return program->getLinkStatus();
    case GraphicsContextGL::ATTACHED_SHADERS:
        m_context->getProgramiv(objectOrZero(program), pname, &value);
        return value;
    case GraphicsContextGL::ACTIVE_ATTRIBUTES:
    case GraphicsContextGL::ACTIVE_UNIFORMS:
#if USE(ANGLE)
        m_context->getProgramiv(objectOrZero(program), pname, &value);
#else
        m_context->getNonBuiltInActiveSymbolCount(objectOrZero(program), pname, &value);
#endif // USE(ANGLE)
        return value;
    default:
#if ENABLE(WEBGL2)
        if (isWebGL2()) {
            switch (pname) {
            case GraphicsContextGL::TRANSFORM_FEEDBACK_BUFFER_MODE:
            case GraphicsContextGL::TRANSFORM_FEEDBACK_VARYINGS:
            case GraphicsContextGL::ACTIVE_UNIFORM_BLOCKS:
                m_context->getProgramiv(objectOrZero(program), pname, &value);
                return value;
            default:
                break;
            }
        }
#endif // ENABLE(WEBGL2)
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getProgramParameter", "invalid parameter name");
        return nullptr;
    }
}

String WebGLRenderingContextBase::getProgramInfoLog(WebGLProgram* program)
{
    if (isContextLostOrPending() || !validateWebGLProgramOrShader("getProgramInfoLog", program))
        return String();
    return ensureNotNull(m_context->getProgramInfoLog(objectOrZero(program)));
}

WebGLAny WebGLRenderingContextBase::getRenderbufferParameter(GCGLenum target, GCGLenum pname)
{
    if (isContextLostOrPending())
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

    GCGLint value = 0;
    switch (pname) {
    case GraphicsContextGL::RENDERBUFFER_WIDTH:
    case GraphicsContextGL::RENDERBUFFER_HEIGHT:
    case GraphicsContextGL::RENDERBUFFER_RED_SIZE:
    case GraphicsContextGL::RENDERBUFFER_GREEN_SIZE:
    case GraphicsContextGL::RENDERBUFFER_BLUE_SIZE:
    case GraphicsContextGL::RENDERBUFFER_ALPHA_SIZE:
    case GraphicsContextGL::RENDERBUFFER_DEPTH_SIZE:
    case GraphicsContextGL::RENDERBUFFER_STENCIL_SIZE:
        m_context->getRenderbufferParameteriv(target, pname, &value);
        return value;
    case GraphicsContextGL::RENDERBUFFER_INTERNAL_FORMAT:
        return m_renderbufferBinding->getInternalFormat();
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getRenderbufferParameter", "invalid parameter name");
        return nullptr;
    }
}

WebGLAny WebGLRenderingContextBase::getShaderParameter(WebGLShader* shader, GCGLenum pname)
{
    if (isContextLostOrPending() || !validateWebGLProgramOrShader("getShaderParameter", shader))
        return nullptr;
    GCGLint value = 0;
    switch (pname) {
    case GraphicsContextGL::DELETE_STATUS:
        return shader->isDeleted();
    case GraphicsContextGL::COMPILE_STATUS:
        m_context->getShaderiv(objectOrZero(shader), pname, &value);
        return static_cast<bool>(value);
    case GraphicsContextGL::SHADER_TYPE:
        m_context->getShaderiv(objectOrZero(shader), pname, &value);
        return static_cast<unsigned>(value);
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getShaderParameter", "invalid parameter name");
        return nullptr;
    }
}

String WebGLRenderingContextBase::getShaderInfoLog(WebGLShader* shader)
{
    if (isContextLostOrPending() || !validateWebGLProgramOrShader("getShaderInfoLog", shader))
        return String();
    return ensureNotNull(m_context->getShaderInfoLog(objectOrZero(shader)));
}

RefPtr<WebGLShaderPrecisionFormat> WebGLRenderingContextBase::getShaderPrecisionFormat(GCGLenum shaderType, GCGLenum precisionType)
{
    if (isContextLostOrPending())
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

    GCGLint range[2] = {0, 0};
    GCGLint precision = 0;
    m_context->getShaderPrecisionFormat(shaderType, precisionType, range, &precision);
    return WebGLShaderPrecisionFormat::create(range[0], range[1], precision);
}

String WebGLRenderingContextBase::getShaderSource(WebGLShader* shader)
{
    if (isContextLostOrPending() || !validateWebGLProgramOrShader("getShaderSource", shader))
        return String();
    return ensureNotNull(shader->getSource());
}

WebGLAny WebGLRenderingContextBase::getTexParameter(GCGLenum target, GCGLenum pname)
{
    if (isContextLostOrPending())
        return nullptr;
    auto tex = validateTextureBinding("getTexParameter", target);
    if (!tex)
        return nullptr;
    GCGLint value = 0;
    GCGLfloat fValue = 0;
    switch (pname) {
    case GraphicsContextGL::TEXTURE_MAG_FILTER:
    case GraphicsContextGL::TEXTURE_MIN_FILTER:
    case GraphicsContextGL::TEXTURE_WRAP_S:
    case GraphicsContextGL::TEXTURE_WRAP_T:
        m_context->getTexParameteriv(target, pname, &value);
        return static_cast<unsigned>(value);
    case ExtensionsGL::TEXTURE_MAX_ANISOTROPY_EXT: // EXT_texture_filter_anisotropic
        if (m_extTextureFilterAnisotropic) {
            m_context->getTexParameterfv(target, pname, &fValue);
            return static_cast<float>(fValue);
        }
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getTexParameter", "invalid parameter name, EXT_texture_filter_anisotropic not enabled");
        return nullptr;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getTexParameter", "invalid parameter name");
        return nullptr;
    }
}

WebGLAny WebGLRenderingContextBase::getUniform(WebGLProgram* program, const WebGLUniformLocation* uniformLocation)
{
    if (isContextLostOrPending() || !validateWebGLProgramOrShader("getUniform", program))
        return nullptr;
    if (!uniformLocation || uniformLocation->program() != program) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "getUniform", "no uniformlocation or not valid for this program");
        return nullptr;
    }
    GCGLint location = uniformLocation->location();

    GCGLenum baseType;
    unsigned length;
    switch (uniformLocation->type()) {
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
        switch (uniformLocation->type()) {
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
        if (m_isRobustnessEXTSupported)
            m_context->getExtensions().getnUniformfvEXT(objectOrZero(program), location, 16 * sizeof(GCGLfloat), value);
        else
            m_context->getUniformfv(objectOrZero(program), location, value);
        if (length == 1)
            return value[0];
        return Float32Array::tryCreate(value, length);
    }
    case GraphicsContextGL::INT: {
        GCGLint value[4] = {0};
        if (m_isRobustnessEXTSupported)
            m_context->getExtensions().getnUniformivEXT(objectOrZero(program), location, 4 * sizeof(GCGLint), value);
        else
            m_context->getUniformiv(objectOrZero(program), location, value);
        if (length == 1)
            return value[0];
        return Int32Array::tryCreate(value, length);
    }
    case GraphicsContextGL::UNSIGNED_INT: {
        GCGLuint value[4] = {0};
        m_context->getUniformuiv(objectOrZero(program), location, value);
        if (length == 1)
            return value[0];
        return Uint32Array::tryCreate(value, length);
    }
    case GraphicsContextGL::BOOL: {
        GCGLint value[4] = {0};
        if (m_isRobustnessEXTSupported)
            m_context->getExtensions().getnUniformivEXT(objectOrZero(program), location, 4 * sizeof(GCGLint), value);
        else
            m_context->getUniformiv(objectOrZero(program), location, value);
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

RefPtr<WebGLUniformLocation> WebGLRenderingContextBase::getUniformLocation(WebGLProgram* program, const String& name)
{
    if (isContextLostOrPending() || !validateWebGLProgramOrShader("getUniformLocation", program))
        return nullptr;
    if (!validateLocationLength("getUniformLocation", name))
        return nullptr;
    if (!validateString("getUniformLocation", name))
        return nullptr;
    if (isPrefixReserved(name))
        return nullptr;
    if (!program->getLinkStatus()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "getUniformLocation", "program not linked");
        return nullptr;
    }
    GCGLint uniformLocation = m_context->getUniformLocation(objectOrZero(program), name);
    if (uniformLocation == -1)
        return nullptr;

    GCGLint activeUniforms = 0;
#if USE(ANGLE)
    m_context->getProgramiv(objectOrZero(program), GraphicsContextGL::ACTIVE_UNIFORMS, &activeUniforms);
#else
    m_context->getNonBuiltInActiveSymbolCount(objectOrZero(program), GraphicsContextGL::ACTIVE_UNIFORMS, &activeUniforms);
#endif
    for (GCGLint i = 0; i < activeUniforms; i++) {
        GraphicsContextGL::ActiveInfo info;
        if (!m_context->getActiveUniform(objectOrZero(program), i, info))
            return nullptr;
        // Strip "[0]" from the name if it's an array.
        if (info.name.endsWith("[0]"))
            info.name = info.name.left(info.name.length() - 3);
        // If it's an array, we need to iterate through each element, appending "[index]" to the name.
        for (GCGLint index = 0; index < info.size; ++index) {
            String uniformName = makeString(info.name, '[', index, ']');

            if (name == uniformName || name == info.name)
                return WebGLUniformLocation::create(program, uniformLocation, info.type);
        }
    }
    return nullptr;
}

WebGLAny WebGLRenderingContextBase::getVertexAttrib(GCGLuint index, GCGLenum pname)
{
    if (isContextLostOrPending())
        return nullptr;

    if (index >= m_maxVertexAttribs) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "getVertexAttrib", "index out of range");
        return nullptr;
    }

    const WebGLVertexArrayObjectBase::VertexAttribState& state = m_boundVertexArrayObject->getVertexAttribState(index);

    if ((isWebGL2() || m_angleInstancedArrays) && pname == GraphicsContextGL::VERTEX_ATTRIB_ARRAY_DIVISOR_ANGLE)
        return state.divisor;

    switch (pname) {
    case GraphicsContextGL::VERTEX_ATTRIB_ARRAY_BUFFER_BINDING:
#if !USE(ANGLE)
        if ((!isGLES2Compliant() && !index && m_boundVertexArrayObject->getVertexAttribState(0).bufferBinding == m_vertexAttrib0Buffer)
            || !state.bufferBinding
            || !state.bufferBinding->object())
            return nullptr;
#endif
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
    if (isContextLostOrPending())
        return 0;
    return m_context->getVertexAttribOffset(index, pname);
}

bool WebGLRenderingContextBase::extensionIsEnabled(const String& name)
{
#define CHECK_EXTENSION(variable, nameLiteral) \
    if (equalIgnoringASCIICase(name, nameLiteral)) \
        return variable != nullptr;

    CHECK_EXTENSION(m_extFragDepth, "EXT_frag_depth");
    CHECK_EXTENSION(m_extBlendMinMax, "EXT_blend_minmax");
    CHECK_EXTENSION(m_extsRGB, "EXT_sRGB");
    CHECK_EXTENSION(m_extTextureFilterAnisotropic, "EXT_texture_filter_anisotropic");
    CHECK_EXTENSION(m_extTextureFilterAnisotropic, "WEBKIT_EXT_texture_filter_anisotropic");
    CHECK_EXTENSION(m_extShaderTextureLOD, "EXT_shader_texture_lod");
    CHECK_EXTENSION(m_oesTextureFloat, "OES_texture_float");
    CHECK_EXTENSION(m_oesTextureFloatLinear, "OES_texture_float_linear");
    CHECK_EXTENSION(m_oesTextureHalfFloat, "OES_texture_half_float");
    CHECK_EXTENSION(m_oesTextureHalfFloatLinear, "OES_texture_half_float_linear");
    CHECK_EXTENSION(m_oesStandardDerivatives, "OES_standard_derivatives");
    CHECK_EXTENSION(m_oesVertexArrayObject, "OES_vertex_array_object");
    CHECK_EXTENSION(m_oesElementIndexUint, "OES_element_index_uint");
    CHECK_EXTENSION(m_webglLoseContext, "WEBGL_lose_context");
    CHECK_EXTENSION(m_webglDebugRendererInfo, "WEBGL_debug_renderer_info");
    CHECK_EXTENSION(m_webglDebugShaders, "WEBGL_debug_shaders");
    CHECK_EXTENSION(m_webglCompressedTextureASTC, "WEBGL_compressed_texture_astc");
    CHECK_EXTENSION(m_webglCompressedTextureATC, "WEBKIT_WEBGL_compressed_texture_atc");
    CHECK_EXTENSION(m_webglCompressedTextureETC, "WEBGL_compressed_texture_etc");
    CHECK_EXTENSION(m_webglCompressedTextureETC1, "WEBGL_compressed_texture_etc1");
    CHECK_EXTENSION(m_webglCompressedTexturePVRTC, "WEBKIT_WEBGL_compressed_texture_pvrtc");
    CHECK_EXTENSION(m_webglCompressedTextureS3TC, "WEBGL_compressed_texture_s3tc");
    CHECK_EXTENSION(m_webglDepthTexture, "WEBGL_depth_texture");
    CHECK_EXTENSION(m_webglDrawBuffers, "WEBGL_draw_buffers");
    CHECK_EXTENSION(m_angleInstancedArrays, "ANGLE_instanced_arrays");
    CHECK_EXTENSION(m_extColorBufferHalfFloat, "EXT_color_buffer_half_float");
    CHECK_EXTENSION(m_webglColorBufferFloat, "WEBGL_color_buffer_float");
    CHECK_EXTENSION(m_extColorBufferFloat, "EXT_color_buffer_float");
    return false;
}

void WebGLRenderingContextBase::enablePreserveDrawingBuffer()
{
    ASSERT(!m_attributes.preserveDrawingBuffer);
    m_attributes.preserveDrawingBuffer = true;
    // Must send this notification down to the GraphicsContextGL as well.
    m_context->enablePreserveDrawingBuffer();
}

GCGLboolean WebGLRenderingContextBase::isBuffer(WebGLBuffer* buffer)
{
    if (!buffer || isContextLostOrPending())
        return 0;

    if (!buffer->hasEverBeenBound())
        return 0;

    return m_context->isBuffer(buffer->object());
}

bool WebGLRenderingContextBase::isContextLost() const
{
    return m_contextLost;
}

bool WebGLRenderingContextBase::isContextLostOrPending()
{
    if (m_isPendingPolicyResolution && !m_hasRequestedPolicyResolution) {
        LOG(WebGL, "Context is being used. Attempt to resolve the policy.");
        auto* canvas = htmlCanvas();
        if (canvas) {
            Document& document = canvas->document().topDocument();
            Page* page = document.page();
            if (page && !document.url().isLocalFile())
                page->mainFrame().loader().client().resolveWebGLPolicyForURL(document.url());
            // FIXME: We don't currently do anything with the result from resolution. A more
            // complete implementation might try to construct a real context, etc and proceed
            // with normal operation.
            // https://bugs.webkit.org/show_bug.cgi?id=129122
        }
        m_hasRequestedPolicyResolution = true;
    }

    return m_contextLost || m_isPendingPolicyResolution;
}

GCGLboolean WebGLRenderingContextBase::isEnabled(GCGLenum cap)
{
    if (isContextLostOrPending() || !validateCapability("isEnabled", cap))
        return 0;
    if (cap == GraphicsContextGL::STENCIL_TEST)
        return m_stencilEnabled;
    return m_context->isEnabled(cap);
}

GCGLboolean WebGLRenderingContextBase::isFramebuffer(WebGLFramebuffer* framebuffer)
{
    if (!framebuffer || isContextLostOrPending())
        return 0;

    if (!framebuffer->hasEverBeenBound())
        return 0;

    return m_context->isFramebuffer(framebuffer->object());
}

GCGLboolean WebGLRenderingContextBase::isProgram(WebGLProgram* program)
{
    if (!program || isContextLostOrPending())
        return 0;

    return m_context->isProgram(program->object());
}

GCGLboolean WebGLRenderingContextBase::isRenderbuffer(WebGLRenderbuffer* renderbuffer)
{
    if (!renderbuffer || isContextLostOrPending())
        return 0;

    if (!renderbuffer->hasEverBeenBound())
        return 0;

    return m_context->isRenderbuffer(renderbuffer->object());
}

GCGLboolean WebGLRenderingContextBase::isShader(WebGLShader* shader)
{
    if (!shader || isContextLostOrPending())
        return 0;

    return m_context->isShader(shader->object());
}

GCGLboolean WebGLRenderingContextBase::isTexture(WebGLTexture* texture)
{
    if (!texture || isContextLostOrPending())
        return 0;

    if (!texture->hasEverBeenBound())
        return 0;

    return m_context->isTexture(texture->object());
}

void WebGLRenderingContextBase::lineWidth(GCGLfloat width)
{
    if (isContextLostOrPending())
        return;
    m_context->lineWidth(width);
}

void WebGLRenderingContextBase::linkProgram(WebGLProgram* program)
{
    if (!linkProgramWithoutInvalidatingAttribLocations(program))
        return;

    program->increaseLinkCount();
}

bool WebGLRenderingContextBase::linkProgramWithoutInvalidatingAttribLocations(WebGLProgram* program)
{
    if (isContextLostOrPending() || !validateWebGLProgramOrShader("linkProgram", program))
        return false;

    RefPtr<WebGLShader> vertexShader = program->getAttachedShader(GraphicsContextGL::VERTEX_SHADER);
    RefPtr<WebGLShader> fragmentShader = program->getAttachedShader(GraphicsContextGL::FRAGMENT_SHADER);
    if (!vertexShader || !vertexShader->isValid() || !fragmentShader || !fragmentShader->isValid()) {
        program->setLinkStatus(false);
        return false;
    }

#if !USE(ANGLE)
    if (!m_context->precisionsMatch(objectOrZero(vertexShader.get()), objectOrZero(fragmentShader.get()))
        || !m_context->checkVaryingsPacking(objectOrZero(vertexShader.get()), objectOrZero(fragmentShader.get()))) {
        program->setLinkStatus(false);
        return false;
    }
#endif

    m_context->linkProgram(objectOrZero(program));
    return true;
}

#if ENABLE(WEBXR)
// https://immersive-web.github.io/webxr/#dom-webglrenderingcontextbase-makexrcompatible
void WebGLRenderingContextBase::makeXRCompatible(MakeXRCompatiblePromise&& promise)
{
    auto rejectPromiseWithInvalidStateError = makeScopeExit([&] () {
        m_isXRCompatible = false;
        promise.reject(Exception { InvalidStateError });
    });

    // Returning an exception in these two checks is not part of the spec.
    auto canvas = htmlCanvas();
    if (!canvas)
        return;

    auto* window = canvas->document().domWindow();
    if (!window)
        return;

    // 1. Let promise be a new Promise.
    // 2. Let context be the target WebGLRenderingContextBase object.
    // 3. Ensure an immersive XR device is selected.
    auto& xrSystem = NavigatorWebXR::xr(window->navigator());
    xrSystem.ensureImmersiveXRDeviceIsSelected();

    // 4. Set context’s XR compatible boolean as follows:
    //    If context’s WebGL context lost flag is set
    //      Set context’s XR compatible boolean to false and reject promise with an InvalidStateError.
    if (isContextLostOrPending())
        return;

    // If the immersive XR device is null
    //    Set context’s XR compatible boolean to false and reject promise with an InvalidStateError.
    if (!xrSystem.hasActiveImmersiveXRDevice())
        return;

    // If context’s XR compatible boolean is true. Resolve promise.
    // If context was created on a compatible graphics adapter for the immersive XR device
    //  Set context’s XR compatible boolean to true and resolve promise.
    // Otherwise: Queue a task on the WebGL task source to perform the following steps:
    // TODO: add a way to verify that we're using a compatible graphics adapter.
    m_isXRCompatible = true;
    promise.resolve();
    rejectPromiseWithInvalidStateError.release();
}
#endif

void WebGLRenderingContextBase::pixelStorei(GCGLenum pname, GCGLint param)
{
    if (isContextLostOrPending())
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
    if (isContextLostOrPending())
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

#if !USE(ANGLE)
static InternalFormatTheme internalFormatTheme(GCGLenum internalFormat)
{
    switch (internalFormat) {
    case GraphicsContextGL::RGB:
    case GraphicsContextGL::RGBA:
    case GraphicsContextGL::LUMINANCE_ALPHA:
    case GraphicsContextGL::LUMINANCE:
    case GraphicsContextGL::ALPHA:
    case GraphicsContextGL::R8:
    case GraphicsContextGL::RG8:
    case GraphicsContextGL::RGB8:
    case GraphicsContextGL::SRGB8:
    case GraphicsContextGL::RGBA8:
    case GraphicsContextGL::SRGB8_ALPHA8:
    case GraphicsContextGL::SRGB_ALPHA:
        return InternalFormatTheme::NormalizedFixedPoint;
    case GraphicsContextGL::RGB565:
    case GraphicsContextGL::RGB5_A1:
    case GraphicsContextGL::RGBA4:
    case GraphicsContextGL::RGB9_E5:
    case GraphicsContextGL::RGB10_A2:
    case GraphicsContextGL::R11F_G11F_B10F:
    case GraphicsContextGL::RGB10_A2UI:
        return InternalFormatTheme::Packed;
    case GraphicsContextGL::R8_SNORM:
    case GraphicsContextGL::RG8_SNORM:
    case GraphicsContextGL::RGB8_SNORM:
    case GraphicsContextGL::RGBA8_SNORM:
        return InternalFormatTheme::SignedNormalizedFixedPoint;
    case GraphicsContextGL::R16F:
    case GraphicsContextGL::R32F:
    case GraphicsContextGL::RG16F:
    case GraphicsContextGL::RG32F:
    case GraphicsContextGL::RGB16F:
    case GraphicsContextGL::RGB32F:
    case GraphicsContextGL::RGBA16F:
    case GraphicsContextGL::RGBA32F:
        return InternalFormatTheme::FloatingPoint;
    case GraphicsContextGL::R8I:
    case GraphicsContextGL::R16I:
    case GraphicsContextGL::R32I:
    case GraphicsContextGL::RG8I:
    case GraphicsContextGL::RG16I:
    case GraphicsContextGL::RG32I:
    case GraphicsContextGL::RGB8I:
    case GraphicsContextGL::RGB16I:
    case GraphicsContextGL::RGB32I:
    case GraphicsContextGL::RGBA8I:
    case GraphicsContextGL::RGBA16I:
    case GraphicsContextGL::RGBA32I:
        return InternalFormatTheme::SignedInteger;
    case GraphicsContextGL::R8UI:
    case GraphicsContextGL::R16UI:
    case GraphicsContextGL::R32UI:
    case GraphicsContextGL::RG8UI:
    case GraphicsContextGL::RG16UI:
    case GraphicsContextGL::RG32UI:
    case GraphicsContextGL::RGB8UI:
    case GraphicsContextGL::RGB16UI:
    case GraphicsContextGL::RGB32UI:
    case GraphicsContextGL::RGBA8UI:
    case GraphicsContextGL::RGBA16UI:
    case GraphicsContextGL::RGBA32UI:
        return InternalFormatTheme::UnsignedInteger;
    default:
        return InternalFormatTheme::None;
    }
}

static int numberOfComponentsForFormat(GCGLenum format)
{
    switch (format) {
    case GraphicsContextGL::RED:
    case GraphicsContextGL::RED_INTEGER:
        return 1;
    case GraphicsContextGL::RG:
    case GraphicsContextGL::RG_INTEGER:
        return 2;
    case GraphicsContextGL::RGB:
    case GraphicsContextGL::RGB_INTEGER:
        return 3;
    case GraphicsContextGL::RGBA:
    case GraphicsContextGL::RGBA_INTEGER:
        return 4;
    default:
        return 0;
    }
}

static int numberOfComponentsForInternalFormat(GCGLenum internalFormat)
{
    switch (internalFormat) {
    case GraphicsContextGL::LUMINANCE:
    case GraphicsContextGL::ALPHA:
    case GraphicsContextGL::R8:
    case GraphicsContextGL::R8_SNORM:
    case GraphicsContextGL::R16F:
    case GraphicsContextGL::R32F:
    case GraphicsContextGL::R8UI:
    case GraphicsContextGL::R8I:
    case GraphicsContextGL::R16UI:
    case GraphicsContextGL::R16I:
    case GraphicsContextGL::R32UI:
    case GraphicsContextGL::R32I:
    case GraphicsContextGL::DEPTH_COMPONENT16:
    case GraphicsContextGL::DEPTH_COMPONENT24:
    case GraphicsContextGL::DEPTH_COMPONENT32F:
        return 1;
    case GraphicsContextGL::RG8:
    case GraphicsContextGL::LUMINANCE_ALPHA:
    case GraphicsContextGL::RG8_SNORM:
    case GraphicsContextGL::RG16F:
    case GraphicsContextGL::RG32F:
    case GraphicsContextGL::RG8UI:
    case GraphicsContextGL::RG8I:
    case GraphicsContextGL::RG16UI:
    case GraphicsContextGL::RG16I:
    case GraphicsContextGL::RG32UI:
    case GraphicsContextGL::RG32I:
    case GraphicsContextGL::DEPTH24_STENCIL8:
    case GraphicsContextGL::DEPTH32F_STENCIL8:
        return 2;
    case GraphicsContextGL::RGB:
    case GraphicsContextGL::RGB8:
    case GraphicsContextGL::SRGB8:
    case GraphicsContextGL::RGB565:
    case GraphicsContextGL::RGB8_SNORM:
    case GraphicsContextGL::R11F_G11F_B10F:
    case GraphicsContextGL::RGB9_E5:
    case GraphicsContextGL::RGB16F:
    case GraphicsContextGL::RGB32F:
    case GraphicsContextGL::RGB8UI:
    case GraphicsContextGL::RGB8I:
    case GraphicsContextGL::RGB16UI:
    case GraphicsContextGL::RGB16I:
    case GraphicsContextGL::RGB32UI:
    case GraphicsContextGL::RGB32I:
        return 3;
    case GraphicsContextGL::RGBA:
    case GraphicsContextGL::RGBA8:
    case GraphicsContextGL::SRGB_ALPHA:
    case GraphicsContextGL::SRGB8_ALPHA8:
    case GraphicsContextGL::RGBA8_SNORM:
    case GraphicsContextGL::RGB5_A1:
    case GraphicsContextGL::RGBA4:
    case GraphicsContextGL::RGB10_A2:
    case GraphicsContextGL::RGBA16F:
    case GraphicsContextGL::RGBA32F:
    case GraphicsContextGL::RGBA8UI:
    case GraphicsContextGL::RGBA8I:
    case GraphicsContextGL::RGB10_A2UI:
    case GraphicsContextGL::RGBA16UI:
    case GraphicsContextGL::RGBA16I:
    case GraphicsContextGL::RGBA32UI:
    case GraphicsContextGL::RGBA32I:
        return 4;
    default:
        return 0;
    }
}

#endif // !USE(ANGLE)

void WebGLRenderingContextBase::readPixels(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, ArrayBufferView& pixels)
{
    if (isContextLostOrPending())
        return;
    // Due to WebGL's same-origin restrictions, it is not possible to
    // taint the origin using the WebGL API.
    ASSERT(canvasBase().originClean());

#if USE(ANGLE)
    // ANGLE will validate the readback from the framebuffer according
    // to WebGL's restrictions. At this level, just validate the type
    // of the readback against the typed array's type.
    if (!validateArrayBufferType("readPixels", type, Optional<JSC::TypedArrayType>(pixels.getType())))
        return;
#else
    GCGLenum internalFormat = 0;
    if (m_framebufferBinding) {
        const char* reason = "framebuffer incomplete";
        if (!m_framebufferBinding->onAccess(graphicsContextGL(), &reason)) {
            synthesizeGLError(GraphicsContextGL::INVALID_FRAMEBUFFER_OPERATION, "readPixels", reason);
            return;
        }
        // FIXME: readBuffer() should affect this
        internalFormat = getReadFramebufferBinding()->getColorBufferFormat();
    } else {
        if (m_attributes.alpha)
            internalFormat = GraphicsContextGL::RGBA8;
        else
            internalFormat = GraphicsContextGL::RGB8;
    }

    if (!internalFormat) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "readPixels", "Missing attachment");
        return;
    }

    if (isWebGL1()) {
        switch (format) {
        case GraphicsContextGL::ALPHA:
        case GraphicsContextGL::RGB:
        case GraphicsContextGL::RGBA:
            break;
        default:
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "readPixels", "invalid format");
            return;
        }
        switch (type) {
        case GraphicsContextGL::UNSIGNED_BYTE:
        case GraphicsContextGL::UNSIGNED_SHORT_5_6_5:
        case GraphicsContextGL::UNSIGNED_SHORT_4_4_4_4:
        case GraphicsContextGL::UNSIGNED_SHORT_5_5_5_1:
            break;
        case GraphicsContextGL::FLOAT:
            if (!m_oesTextureFloat && !m_oesTextureHalfFloat) {
                synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "readPixels", "invalid type");
                return;
            }
            break;
        default:
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "readPixels", "invalid type");
            return;
        }
        if (format != GraphicsContextGL::RGBA || (type != GraphicsContextGL::UNSIGNED_BYTE && type != GraphicsContextGL::FLOAT)) {
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "readPixels", "format not RGBA or type not UNSIGNED_BYTE|FLOAT");
            return;
        }
    }

    InternalFormatTheme internalFormatTheme = WebCore::internalFormatTheme(internalFormat);
    int internalFormatComponentCount = numberOfComponentsForInternalFormat(internalFormat);
    if (internalFormatTheme == InternalFormatTheme::None || !internalFormatComponentCount) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "readPixels", "Incorrect internal format");
        return;
    }

#define CHECK_COMPONENT_COUNT \
    if (numberOfComponentsForFormat(format) < internalFormatComponentCount) { \
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "readPixels", "Not enough components in format"); \
        return; \
    }

#define INTERNAL_FORMAT_CHECK(typeMacro, pixelTypeMacro) \
    if (type != GraphicsContextGLOpenGL::typeMacro || pixels.getType() != JSC::pixelTypeMacro) { \
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "readPixels", "type does not match internal format"); \
        return; \
    } \
    if (format != GraphicsContextGL::RED && format != GraphicsContextGL::RG && format != GraphicsContextGL::RGB && format != GraphicsContextGL::RGBA) { \
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "readPixels", "Unknown format"); \
        return; \
    } \
    CHECK_COMPONENT_COUNT

#define INTERNAL_FORMAT_INTEGER_CHECK(typeMacro, pixelTypeMacro) \
    if (type != GraphicsContextGLOpenGL::typeMacro || pixels.getType() != JSC::pixelTypeMacro) { \
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "readPixels", "type does not match internal format"); \
        return; \
    } \
    if (format != GraphicsContextGL::RED_INTEGER && format != GraphicsContextGL::RG_INTEGER && format != GraphicsContextGL::RGB_INTEGER && format != GraphicsContextGL::RGBA_INTEGER) { \
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "readPixels", "Unknown format"); \
        return; \
    } \
    CHECK_COMPONENT_COUNT

#define CASE_PACKED_INTERNAL_FORMAT_CHECK(internalFormatMacro, formatMacro, type0Macro, pixelType0Macro, type1Macro, pixelType1Macro) case GraphicsContextGLOpenGL::internalFormatMacro: \
    if (!(type == GraphicsContextGLOpenGL::type0Macro && pixels.getType() == JSC::pixelType0Macro) \
        && !(type == GraphicsContextGLOpenGL::type1Macro && pixels.getType() == JSC::pixelType1Macro)) { \
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "readPixels", "type does not match internal format"); \
        return; \
    } \
    if (format != GraphicsContextGLOpenGL::formatMacro) { \
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "readPixels", "Invalid format"); \
        return; \
    } \
    break;

    switch (internalFormatTheme) {
    case InternalFormatTheme::NormalizedFixedPoint:
        if (type == GraphicsContextGL::FLOAT) {
            INTERNAL_FORMAT_CHECK(FLOAT, TypeFloat32);
        } else {
            INTERNAL_FORMAT_CHECK(UNSIGNED_BYTE, TypeUint8);
        }
        break;
    case InternalFormatTheme::SignedNormalizedFixedPoint:
        INTERNAL_FORMAT_CHECK(BYTE, TypeInt8);
        break;
    case InternalFormatTheme::FloatingPoint:
        INTERNAL_FORMAT_CHECK(FLOAT, TypeFloat32);
        break;
    case InternalFormatTheme::SignedInteger:
        INTERNAL_FORMAT_INTEGER_CHECK(INT, TypeInt32);
        break;
    case InternalFormatTheme::UnsignedInteger:
        INTERNAL_FORMAT_INTEGER_CHECK(UNSIGNED_INT, TypeUint32);
        break;
    case InternalFormatTheme::Packed:
        switch (internalFormat) {
            CASE_PACKED_INTERNAL_FORMAT_CHECK(RGB565        , RGB         , UNSIGNED_SHORT_5_6_5        , TypeUint16, UNSIGNED_BYTE              , TypeUint8  );
            CASE_PACKED_INTERNAL_FORMAT_CHECK(RGB5_A1       , RGBA        , UNSIGNED_SHORT_5_5_5_1      , TypeUint16, UNSIGNED_BYTE              , TypeUint8  );
            CASE_PACKED_INTERNAL_FORMAT_CHECK(RGBA4         , RGBA        , UNSIGNED_SHORT_4_4_4_4      , TypeUint16, UNSIGNED_BYTE              , TypeUint8  );
            CASE_PACKED_INTERNAL_FORMAT_CHECK(RGB9_E5       , RGB         , UNSIGNED_INT_5_9_9_9_REV    , TypeUint32, UNSIGNED_INT_5_9_9_9_REV   , TypeUint32 );
            CASE_PACKED_INTERNAL_FORMAT_CHECK(RGB10_A2      , RGBA        , UNSIGNED_INT_2_10_10_10_REV , TypeUint32, UNSIGNED_INT_2_10_10_10_REV, TypeUint32 );
            CASE_PACKED_INTERNAL_FORMAT_CHECK(R11F_G11F_B10F, RGB         , UNSIGNED_INT_10F_11F_11F_REV, TypeUint32, FLOAT                      , TypeFloat32);
            CASE_PACKED_INTERNAL_FORMAT_CHECK(RGB10_A2UI    , RGBA_INTEGER, UNSIGNED_INT_2_10_10_10_REV , TypeUint32, UNSIGNED_INT_2_10_10_10_REV, TypeUint32 );
        }
        break;
    case InternalFormatTheme::None:
        ASSERT_NOT_REACHED();
    }
#undef CHECK_COMPONENT_COUNT
#undef INTERNAL_FORMAT_CHECK
#undef INTERNAL_FORMAT_INTEGER_CHECK
#undef CASE_PACKED_INTERNAL_FORMAT_CHECK

    // Calculate array size, taking into consideration of PACK_ALIGNMENT.
    unsigned totalBytesRequired = 0;
    unsigned padding = 0;
    if (!m_isRobustnessEXTSupported) {
        GCGLenum error = m_context->computeImageSizeInBytes(format, type, width, height, 1, getPackPixelStoreParams(), &totalBytesRequired, &padding, nullptr);
        if (error != GraphicsContextGL::NO_ERROR) {
            synthesizeGLError(error, "readPixels", "invalid dimensions");
            return;
        }
        if (pixels.byteLength() < totalBytesRequired) {
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "readPixels", "ArrayBufferView not large enough for dimensions");
            return;
        }
    }
#endif // USE(ANGLE)

    clearIfComposited();
    void* data = pixels.baseAddress();

#if USE(ANGLE)
        GLsizei length, columns, rows;
        m_context->makeContextCurrent();
        m_context->getExtensions().readnPixelsRobustANGLE(x, y, width, height, format, type, pixels.byteLength(), &length, &columns, &rows, data, false);
#else
    if (m_isRobustnessEXTSupported)
        m_context->getExtensions().readnPixelsEXT(x, y, width, height, format, type, pixels.byteLength(), data);
    else
        m_context->readPixels(x, y, width, height, format, type, data);
#endif
}

void WebGLRenderingContextBase::releaseShaderCompiler()
{
    if (isContextLostOrPending())
        return;
    m_context->releaseShaderCompiler();
}

void WebGLRenderingContextBase::sampleCoverage(GCGLfloat value, GCGLboolean invert)
{
    if (isContextLostOrPending())
        return;
    m_context->sampleCoverage(value, invert);
}

void WebGLRenderingContextBase::scissor(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height)
{
    if (isContextLostOrPending())
        return;
    if (!validateSize("scissor", width, height))
        return;
    m_context->scissor(x, y, width, height);
}

void WebGLRenderingContextBase::shaderSource(WebGLShader* shader, const String& string)
{
    if (isContextLostOrPending() || !validateWebGLProgramOrShader("shaderSource", shader))
        return;
#if USE(ANGLE)
    m_context->shaderSource(objectOrZero(shader), string);
#else
    String stringWithoutComments = StripComments(string).result();
    if (!validateString("shaderSource", stringWithoutComments))
        return;
    m_context->shaderSource(objectOrZero(shader), stringWithoutComments);
#endif
    shader->setSource(string);
}

void WebGLRenderingContextBase::stencilFunc(GCGLenum func, GCGLint ref, GCGLuint mask)
{
    if (isContextLostOrPending())
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
    if (isContextLostOrPending())
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
    if (isContextLostOrPending())
        return;
    m_stencilMask = mask;
    m_stencilMaskBack = mask;
    m_context->stencilMask(mask);
}

void WebGLRenderingContextBase::stencilMaskSeparate(GCGLenum face, GCGLuint mask)
{
    if (isContextLostOrPending())
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
    if (isContextLostOrPending())
        return;
    m_context->stencilOp(fail, zfail, zpass);
}

void WebGLRenderingContextBase::stencilOpSeparate(GCGLenum face, GCGLenum fail, GCGLenum zfail, GCGLenum zpass)
{
    if (isContextLostOrPending())
        return;
    m_context->stencilOpSeparate(face, fail, zfail, zpass);
}

#if !USE(ANGLE)
static bool isRGBFormat(GCGLenum internalFormat)
{
    return internalFormat == GraphicsContextGL::RGB
        || internalFormat == GraphicsContextGL::RGBA
        || internalFormat == GraphicsContextGL::RGB8
        || internalFormat == GraphicsContextGL::RGBA8;
}
#endif // !USE(ANGLE)

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
    }
    );

    ExceptionOr<IntRect> result = WTF::visit(visitor, source);
    if (result.hasException())
        return sentinelEmptyRect();
    return result.returnValue();
}

ExceptionOr<void> WebGLRenderingContextBase::texImageSourceHelper(TexImageFunctionID functionID, GCGLenum target, GCGLint level, GCGLint internalformat, GCGLint border, GCGLenum format, GCGLenum type, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, const IntRect& inputSourceImageRect, GCGLsizei depth, GCGLint unpackImageHeight, TexImageSource&& source)
{
    if (isContextLostOrPending())
        return { };

    const char* functionName = getTexImageFunctionName(functionID);
    TexImageFunctionType functionType;
    if (functionID == TexImageFunctionID::TexImage2D || functionID == TexImageFunctionID::TexImage3D)
        functionType = TexImageFunctionType::TexImage;
    else
        functionType = TexImageFunctionType::TexSubImage;

#if !USE(ANGLE)
    if (functionType == TexImageFunctionType::TexSubImage) {
        auto texture = validateTexImageBinding(functionName, functionID, target);
        if (texture)
            internalformat = texture->getInternalFormat(target, level);
    }
#endif

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

#if !USE(ANGLE)
        // FIXME: Enable this path with ANGLE. Only used with Cairo, not on macOS or iOS.
        // If possible, copy from the bitmap directly to the texture
        // via the GPU, without a read-back to system memory.
        //
        // FIXME: restriction of (RGB || RGBA)/UNSIGNED_BYTE should be lifted when
        // ImageBuffer::copyToPlatformTexture implementations are fully functional.
        if (texture
            && (format == GraphicsContextGL::RGB || format == GraphicsContextGL::RGBA)
            && type == GraphicsContextGL::UNSIGNED_BYTE) {
            auto textureInternalFormat = texture->getInternalFormat(target, level);
            if (isRGBFormat(textureInternalFormat) || !texture->isValid(target, level)) {
                if (buffer->copyToPlatformTexture(*m_context.get(), target, texture->object(), internalformat, m_unpackPremultiplyAlpha, m_unpackFlipY)) {
                    texture->setLevelInfo(target, level, internalformat, width, height, type);
                    return { };
                }
            }
        }
#endif // !USE(ANGLE)

        // Fallback pure SW path.
        RefPtr<Image> image = buffer->copyImage(DontCopyBackingStore);
        if (image)
            texImageImpl(functionID, target, level, internalformat, xoffset, yoffset, zoffset, format, type, image.get(), GraphicsContextGL::DOMSource::Image, m_unpackFlipY, m_unpackPremultiplyAlpha, sourceImageRect, depth, unpackImageHeight);
        return { };
    }, [&](const RefPtr<ImageData>& pixels) -> ExceptionOr<void> {
        if (pixels->data()->isNeutered()) {
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

        Vector<uint8_t> data;
        bool needConversion = true;
        GCGLsizei byteLength = 0;
        // The data from ImageData is always of format RGBA8.
        // No conversion is needed if destination format is RGBA and type is USIGNED_BYTE and no Flip or Premultiply operation is required.
        if (!m_unpackFlipY && !m_unpackPremultiplyAlpha && format == GraphicsContextGL::RGBA && type == GraphicsContextGL::UNSIGNED_BYTE && !selectingSubRectangle && depth == 1) {
            needConversion = false;
            byteLength = pixels->data()->byteLength();
        } else {
            if (type == GraphicsContextGL::UNSIGNED_INT_10F_11F_11F_REV) {
                // The UNSIGNED_INT_10F_11F_11F_REV type pack/unpack isn't implemented.
                type = GraphicsContextGL::FLOAT;
            }
            if (!m_context->extractImageData(pixels.get(), GraphicsContextGLOpenGL::DataFormat::RGBA8, adjustedSourceImageRect, depth, unpackImageHeight, format, type, m_unpackFlipY, m_unpackPremultiplyAlpha, data)) {
                synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "texImage2D", "bad image data");
                return { };
            }
            byteLength = data.size();
        }
        ScopedUnpackParametersResetRestore temporaryResetUnpack(this);
        if (functionID == TexImageFunctionID::TexImage2D) {
            texImage2DBase(target, level, internalformat,
                adjustedSourceImageRect.width(), adjustedSourceImageRect.height(), 0,
                format, type, byteLength, needConversion ? data.data() : pixels->data()->data());
        } else if (functionID == TexImageFunctionID::TexSubImage2D) {
            texSubImage2DBase(target, level, xoffset, yoffset,
                adjustedSourceImageRect.width(), adjustedSourceImageRect.height(),
                format, format, type, byteLength, needConversion ? data.data() : pixels->data()->data());
        } else {
#if USE(ANGLE)
            // 3D functions.
            if (functionID == TexImageFunctionID::TexImage3D) {
                m_context->getExtensions().texImage3DRobustANGLE(target, level, internalformat,
                    adjustedSourceImageRect.width(), adjustedSourceImageRect.height(), depth, 0,
                    format, type, byteLength, needConversion ? data.data() : pixels->data()->data());
            } else {
                ASSERT(functionID == TexImageFunctionID::TexSubImage3D);
                m_context->getExtensions().texSubImage3DRobustANGLE(target, level, xoffset, yoffset, zoffset,
                    adjustedSourceImageRect.width(), adjustedSourceImageRect.height(), depth,
                    format, type, byteLength, needConversion ? data.data() : pixels->data()->data());
            }
#else
            // WebGL 2.0 is only supported with the ANGLE backend.
            ASSERT_NOT_REACHED();
#endif
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
        if (imageForRender->isSVGImage() || imageForRender->orientation() != ImageOrientation::None)
            imageForRender = drawImageIntoBuffer(*imageForRender, image->width(), image->height(), 1, functionName);

        if (!imageForRender || !validateTexFunc(functionName, functionType, SourceHTMLImageElement, target, level, internalformat, imageForRender->width(), imageForRender->height(), depth, border, format, type, xoffset, yoffset, zoffset))
            return { };

        // Pass along inputSourceImageRect unchanged. HTMLImageElements are unique in that their
        // size may differ from that of the Image obtained from them (because of devicePixelRatio),
        // so for WebGL 1.0 uploads, defer measuring their rectangle as long as possible.
        texImageImpl(functionID, target, level, internalformat, xoffset, yoffset, zoffset, format, type, imageForRender.get(), GraphicsContextGL::DOMSource::Image, m_unpackFlipY, m_unpackPremultiplyAlpha, inputSourceImageRect, depth, unpackImageHeight);
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

#if !USE(ANGLE)
        // FIXME: Enable this path with ANGLE. Only used with Cairo, not on macOS or iOS.

        // If possible, copy from the canvas element directly to the texture
        // via the GPU, without a read-back to system memory.
        //
        // FIXME: restriction of (RGB || RGBA)/UNSIGNED_BYTE should be lifted when
        // ImageBuffer::copyToPlatformTexture implementations are fully functional.
        if (texture
            && (format == GraphicsContextGL::RGB || format == GraphicsContextGL::RGBA)
            && type == GraphicsContextGL::UNSIGNED_BYTE) {
            auto textureInternalFormat = texture->getInternalFormat(target, level);
            if (isRGBFormat(textureInternalFormat) || !texture->isValid(target, level)) {
                ImageBuffer* buffer = canvas->buffer();
                if (buffer && buffer->copyToPlatformTexture(*m_context.get(), target, texture->object(), internalformat, m_unpackPremultiplyAlpha, m_unpackFlipY)) {
                    texture->setLevelInfo(target, level, internalformat, sourceImageRect.width(), sourceImageRect.height(), type);
                    return { };
                }
            }
        }
#endif // !USE(ANGLE)
        RefPtr<ImageData> imageData = canvas->getImageData();
        if (imageData)
            texImageSourceHelper(functionID, target, level, internalformat, border, format, type, xoffset, yoffset, zoffset, sourceImageRect, depth, unpackImageHeight, TexImageSource(imageData.get()));
        else
            texImageImpl(functionID, target, level, internalformat, xoffset, yoffset, zoffset, format, type, canvas->copiedImage(), GraphicsContextGL::DOMSource::Canvas, m_unpackFlipY, m_unpackPremultiplyAlpha, sourceImageRect, depth, unpackImageHeight);
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

        // Go through the fast path doing a GPU-GPU textures copy without a readback to system memory if possible.
        // Otherwise, it will fall back to the normal SW path.
        // FIXME: The current restrictions require that format shoud be RGB or RGBA,
        // type should be UNSIGNED_BYTE and level should be 0. It may be lifted in the future.
        if (functionID == TexImageFunctionID::TexImage2D && sourceImageRectIsDefault && texture
            && (format == GraphicsContextGL::RGB || format == GraphicsContextGL::RGBA)
            && type == GraphicsContextGL::UNSIGNED_BYTE
            && !level) {
            if (video->copyVideoTextureToPlatformTexture(m_context.get(), texture->object(), target, level, internalformat, format, type, m_unpackPremultiplyAlpha, m_unpackFlipY)) {
#if !USE(ANGLE)
                texture->setLevelInfo(target, level, internalformat, video->videoWidth(), video->videoHeight(), type);
#endif // !USE(ANGLE)
                return { };
            }
        }

        // Fallback pure SW path.
        RefPtr<Image> image = videoFrameToImage(video.get(), DontCopyBackingStore);
        if (!image)
            return { };
        texImageImpl(functionID, target, level, internalformat, xoffset, yoffset, zoffset, format, type, image.get(), GraphicsContextGL::DOMSource::Video, m_unpackFlipY, m_unpackPremultiplyAlpha, inputSourceImageRect, depth, unpackImageHeight);
        return { };
    }
#endif // ENABLE(VIDEO)
    );

    return WTF::visit(visitor, source);
}

void WebGLRenderingContextBase::texImageArrayBufferViewHelper(TexImageFunctionID functionID, GCGLenum target, GCGLint level, GCGLint internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLint border, GCGLenum format, GCGLenum type, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, RefPtr<ArrayBufferView>&& pixels, NullDisposition nullDisposition, GCGLuint srcOffset)
{
    if (isContextLostOrPending())
        return;
    const char* functionName = getTexImageFunctionName(functionID);
    auto texture = validateTexImageBinding(functionName, functionID, target);
    if (!texture)
        return;
    TexImageFunctionType functionType;
    if (functionID == TexImageFunctionID::TexImage2D || functionID == TexImageFunctionID::TexImage3D)
        functionType = TexImageFunctionType::TexImage;
    else
        functionType = TexImageFunctionType::TexSubImage;
#if !USE(ANGLE)
    if (functionType == TexImageFunctionType::TexSubImage)
        internalformat = texture->getInternalFormat(target, level);
#endif

    if (!validateTexFunc(functionName, functionType, SourceArrayBufferView, target, level, internalformat, width, height, depth, border, format, type, xoffset, yoffset, zoffset))
        return;
    TexImageDimension sourceType;
    if (functionID == TexImageFunctionID::TexImage2D || functionID == TexImageFunctionID::TexSubImage2D)
        sourceType = TexImageDimension::Tex2D;
    else
        sourceType = TexImageDimension::Tex3D;
    if (!validateTexFuncData(functionName, sourceType, width, height, depth, format, type, pixels.get(), nullDisposition, srcOffset))
        return;
    uint8_t* data = reinterpret_cast<uint8_t*>(pixels ? pixels->baseAddress() : nullptr);
    if (srcOffset) {
        ASSERT(pixels);
        // No need to check overflow because validateTexFuncData() already did.
        data += srcOffset * JSC::elementSize(pixels->getType());
    }
    GCGLsizei byteLength = pixels ? pixels->byteLength() : 0;
    Vector<uint8_t> tempData;
    bool changeUnpackParams = false;
    if (data && width && height
        && (m_unpackFlipY || m_unpackPremultiplyAlpha)) {
        ASSERT(sourceType == TexImageDimension::Tex2D);
        // Only enter here if width or height is non-zero. Otherwise, call to the
        // underlying driver to generate appropriate GL errors if needed.
        GraphicsContextGLOpenGL::PixelStoreParams unpackParams = getUnpackPixelStoreParams(TexImageDimension::Tex2D);
        GCGLint dataStoreWidth = unpackParams.rowLength ? unpackParams.rowLength : width;
        if (unpackParams.skipPixels + width > dataStoreWidth) {
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "Invalid unpack params combination.");
            return;
        }
        if (!m_context->extractTextureData(width, height, format, type, unpackParams, m_unpackFlipY, m_unpackPremultiplyAlpha, data, tempData))
            return;
        data = tempData.data();
        byteLength = tempData.size();
        changeUnpackParams = true;
    }
#if USE(ANGLE)
    if (functionID == TexImageFunctionID::TexImage3D) {
        m_context->getExtensions().texImage3DRobustANGLE(target, level, internalformat, width, height, depth, border, format, type, byteLength, data);
        return;
    }
    if (functionID == TexImageFunctionID::TexSubImage3D) {
        m_context->getExtensions().texSubImage3DRobustANGLE(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, byteLength, data);
        return;
    }
#endif // USE(ANGLE)
    ScopedUnpackParametersResetRestore temporaryResetUnpack(this, changeUnpackParams);
    if (functionID == TexImageFunctionID::TexImage2D)
        texImage2DBase(target, level, internalformat, width, height, border, format, type, byteLength, data);
    else {
        ASSERT(functionID == TexImageFunctionID::TexSubImage2D);
        texSubImage2DBase(target, level, xoffset, yoffset, width, height, format, format, type, byteLength, data);
    }
}

void WebGLRenderingContextBase::texImageImpl(TexImageFunctionID functionID, GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, GCGLenum format, GCGLenum type, Image* image, GraphicsContextGL::DOMSource domSource, bool flipY, bool premultiplyAlpha, const IntRect& sourceImageRect, GCGLsizei depth, GCGLint unpackImageHeight)
{
    const char* functionName = getTexImageFunctionName(functionID);
    // All calling functions check isContextLostOrPending, so a duplicate check is not
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

    GraphicsContextGLOpenGL::ImageExtractor imageExtractor(image, domSource, premultiplyAlpha, m_unpackColorspaceConversion == GraphicsContextGL::NONE);
    if (!imageExtractor.extractSucceeded()) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "bad image data");
        return;
    }

    GraphicsContextGL::DataFormat sourceDataFormat = imageExtractor.imageSourceFormat();
    GraphicsContextGL::AlphaOp alphaOp = imageExtractor.imageAlphaOp();
    const void* imagePixelData = imageExtractor.imagePixelData();

    bool needConversion = true;
    GCGLsizei byteLength = 0;
    if (type == GraphicsContextGL::UNSIGNED_BYTE && sourceDataFormat == GraphicsContextGL::DataFormat::RGBA8 && format == GraphicsContextGL::RGBA && alphaOp == GraphicsContextGL::AlphaOp::DoNothing && !flipY && !selectingSubRectangle && depth == 1) {
        needConversion = false;
        byteLength = imageExtractor.imageWidth() * imageExtractor.imageHeight() * 4;
    } else {
        if (!m_context->packImageData(image, imagePixelData, format, type, flipY, alphaOp, sourceDataFormat, imageExtractor.imageWidth(), imageExtractor.imageHeight(), adjustedSourceImageRect, depth, imageExtractor.imageSourceUnpackAlignment(), unpackImageHeight, data)) {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "packImage error");
            return;
        }
        byteLength = data.size();
    }

    ScopedUnpackParametersResetRestore temporaryResetUnpack(this, true);
    if (functionID == TexImageFunctionID::TexImage2D) {
        texImage2DBase(target, level, internalformat,
            adjustedSourceImageRect.width(), adjustedSourceImageRect.height(), 0,
            format, type, byteLength, needConversion ? data.data() : imagePixelData);
    } else if (functionID == TexImageFunctionID::TexSubImage2D) {
        texSubImage2DBase(target, level, xoffset, yoffset,
            adjustedSourceImageRect.width(), adjustedSourceImageRect.height(),
            format, format, type, byteLength, needConversion ? data.data() : imagePixelData);
    } else {
#if USE(ANGLE)
        // 3D functions.
        if (functionID == TexImageFunctionID::TexImage3D) {
            m_context->getExtensions().texImage3DRobustANGLE(target, level, internalformat,
                adjustedSourceImageRect.width(), adjustedSourceImageRect.height(), depth, 0,
                format, type, byteLength, needConversion ? data.data() : imagePixelData);
        } else {
            ASSERT(functionID == TexImageFunctionID::TexSubImage3D);
            m_context->getExtensions().texSubImage3DRobustANGLE(target, level, xoffset, yoffset, zoffset,
                adjustedSourceImageRect.width(), adjustedSourceImageRect.height(), depth,
                format, type, byteLength, needConversion ? data.data() : imagePixelData);
        }
#else
        UNUSED_PARAM(zoffset);
        // WebGL 2.0 is only supported with the ANGLE backend.
        ASSERT_NOT_REACHED();
#endif
    }
}

void WebGLRenderingContextBase::texImage2DBase(GCGLenum target, GCGLint level, GCGLenum internalFormat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLenum format, GCGLenum type, GCGLsizei byteLength, const void* pixels)
{
#if USE(ANGLE)
    m_context->getExtensions().texImage2DRobustANGLE(target, level, internalFormat, width, height, border, format, type, byteLength, pixels);
#else
    UNUSED_PARAM(byteLength);
    // FIXME: For now we ignore any errors returned.
    auto tex = validateTexture2DBinding("texImage2D", target);
    ASSERT(validateTexFuncParameters("texImage2D", TexImageFunctionType::TexImage, SourceArrayBufferView, target, level, internalFormat, width, height, 1, border, format, type));
    ASSERT(tex);
    ASSERT(validateNPOTTextureLevel(width, height, level, "texImage2D"));
    if (!pixels) {
        if (!m_context->texImage2DResourceSafe(target, level, internalFormat, width, height, border, format, type, m_unpackAlignment))
            return;
    } else {
        ASSERT(validateSettableTexInternalFormat("texImage2D", internalFormat));
        m_context->moveErrorsToSyntheticErrorList();
        m_context->texImage2D(target, level, internalFormat, width, height, border, format, type, pixels);
        if (m_context->moveErrorsToSyntheticErrorList()) {
            // The texImage2D function failed. Tell the WebGLTexture it doesn't have the data for this level.
            tex->markInvalid(target, level);
            return;
        }
    }
    tex->setLevelInfo(target, level, internalFormat, width, height, type);
#endif // USE(ANGLE)
}

void WebGLRenderingContextBase::texSubImage2DBase(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum internalFormat, GCGLenum format, GCGLenum type, GCGLsizei byteLength, const void* pixels)
{
    ASSERT(!isContextLost());
#if USE(ANGLE)
    UNUSED_PARAM(internalFormat);
    m_context->getExtensions().texSubImage2DRobustANGLE(target, level, xoffset, yoffset, width, height, format, type, byteLength, pixels);
#else
    UNUSED_PARAM(byteLength);
    ASSERT(validateTexFuncParameters("texSubImage2D", TexImageFunctionType::TexSubImage, SourceArrayBufferView, target, level, internalFormat, width, height, 1, 0, format, type));
    ASSERT(validateSize("texSubImage2D", xoffset, yoffset));
    ASSERT(validateSettableTexInternalFormat("texSubImage2D", internalFormat));
    auto tex = validateTexture2DBinding("texSubImage2D", target);
    if (!tex) {
        ASSERT_NOT_REACHED();
        return;
    }
    ASSERT((xoffset + width) >= 0);
    ASSERT((yoffset + height) >= 0);
    ASSERT(tex->getWidth(target, level) >= (xoffset + width));
    ASSERT(tex->getHeight(target, level) >= (yoffset + height));
    if (!isWebGL2())
        ASSERT_UNUSED(internalFormat, tex->getInternalFormat(target, level) == internalFormat);
    m_context->texSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
#endif
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

#if USE(ANGLE)
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
#else
    UNUSED_PARAM(zoffset);
    auto texture = validateTexture2DBinding(functionName, target);
    if (!texture)
        return false;

    if (functionType != TexImageFunctionType::TexSubImage) {
        if (functionType == TexImageFunctionType::TexImage && texture->immutable()) {
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "texStorage() called on this texture previously");
            return false;
        }
        if (!validateNPOTTextureLevel(width, height, level, functionName))
            return false;

        // For SourceArrayBufferView, function validateTexFuncData() would handle whether to validate the SettableTexFormat
        // by checking if the ArrayBufferView is null or not.
        if (sourceType != SourceArrayBufferView) {
            if (!validateSettableTexInternalFormat(functionName, internalFormat))
                return false;
        }
    } else {
        if (!validateSettableTexInternalFormat(functionName, internalFormat))
            return false;
        if (!validateSize(functionName, xoffset, yoffset))
            return false;

        // Before checking if it is in the range, check if overflow happens first.
        if (xoffset + width < 0 || yoffset + height < 0) {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "bad dimensions");
            return false;
        }
        if (xoffset + width > texture->getWidth(target, level) || yoffset + height > texture->getHeight(target, level)) {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "dimensions out of range");
            return false;
        }
        if (texture->getInternalFormat(target, level) != internalFormat || (isWebGL1() && texture->getType(target, level) != type)) {
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "type and format do not match texture");
            return false;
        }
    }
#endif // USE(ANGLE)

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

ExceptionOr<void> WebGLRenderingContextBase::texSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLenum format, GCGLenum type, Optional<TexImageSource>&& source)
{
    if (isContextLostOrPending())
        return { };

    if (!source) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "texSubImage2D", "source is null");
        return { };
    }

    return texImageSourceHelper(TexImageFunctionID::TexSubImage2D, target, level, 0, 0, format, type, xoffset, yoffset, 0, sentinelEmptyRect(), 1, 0, WTFMove(*source));
}

bool WebGLRenderingContextBase::validateArrayBufferType(const char* functionName, GCGLenum type, Optional<JSC::TypedArrayType> arrayType)
{
#define TYPE_VALIDATION_CASE(arrayTypeMacro) if (arrayType && arrayType.value() != JSC::arrayTypeMacro) { \
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "ArrayBufferView not " #arrayTypeMacro); \
            return false; \
        } \
        break;

#define TYPE_VALIDATION_CASE_2(arrayTypeMacro, arrayTypeMacro2) if (arrayType && arrayType.value() != JSC::arrayTypeMacro && arrayType.value() != JSC::arrayTypeMacro2) { \
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "ArrayBufferView not " #arrayTypeMacro " or " #arrayTypeMacro2); \
            return false; \
        } \
        break;

    switch (type) {
    case GraphicsContextGL::UNSIGNED_BYTE:
        TYPE_VALIDATION_CASE_2(TypeUint8, TypeUint8Clamped);
    case GraphicsContextGL::BYTE:
        TYPE_VALIDATION_CASE(TypeInt8);
    case GraphicsContextGL::UNSIGNED_SHORT:
    case GraphicsContextGL::UNSIGNED_SHORT_5_6_5:
    case GraphicsContextGL::UNSIGNED_SHORT_4_4_4_4:
    case GraphicsContextGL::UNSIGNED_SHORT_5_5_5_1:
        TYPE_VALIDATION_CASE(TypeUint16);
    case GraphicsContextGL::SHORT:
        TYPE_VALIDATION_CASE(TypeInt16);
    case GraphicsContextGL::UNSIGNED_INT_2_10_10_10_REV:
    case GraphicsContextGL::UNSIGNED_INT_10F_11F_11F_REV:
    case GraphicsContextGL::UNSIGNED_INT_5_9_9_9_REV:
    case GraphicsContextGL::UNSIGNED_INT_24_8:
    case GraphicsContextGL::UNSIGNED_INT:
        TYPE_VALIDATION_CASE(TypeUint32);
    case GraphicsContextGL::INT:
        TYPE_VALIDATION_CASE(TypeInt32);
    case GraphicsContextGL::FLOAT: // OES_texture_float
        TYPE_VALIDATION_CASE(TypeFloat32);
    case GraphicsContextGL::HALF_FLOAT_OES: // OES_texture_half_float
    case GraphicsContextGL::HALF_FLOAT:
        TYPE_VALIDATION_CASE(TypeUint16);
    case GraphicsContextGL::FLOAT_32_UNSIGNED_INT_24_8_REV:
        if (arrayType)
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "type FLOAT_32_UNSIGNED_INT_24_8_REV but ArrayBufferView is not null");
        break;
    default:
        // This can now be reached in readPixels' ANGLE code path.
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid type");
        return false;
    }
#undef TYPE_VALIDATION_CASE
    return true;
}

bool WebGLRenderingContextBase::validateTexFuncData(const char* functionName, TexImageDimension texDimension, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLenum format, GCGLenum type, ArrayBufferView* pixels, NullDisposition disposition, GCGLuint srcOffset)
{
    // All calling functions check isContextLostOrPending, so a duplicate check is not
    // needed here.
    if (!pixels) {
        ASSERT(disposition != NullNotReachable);
        if (disposition == NullAllowed)
            return true;
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "no pixels");
        return false;
    }

    if (!validateSettableTexInternalFormat(functionName, format))
        return false;
    if (!validateArrayBufferType(functionName, type, pixels ? Optional<JSC::TypedArrayType>(pixels->getType()) : WTF::nullopt))
        return false;

    unsigned totalBytesRequired, skipBytes;
    GCGLenum error = m_context->computeImageSizeInBytes(format, type, width, height, depth, getUnpackPixelStoreParams(texDimension), &totalBytesRequired, nullptr, &skipBytes);
    if (error != GraphicsContextGL::NO_ERROR) {
        synthesizeGLError(error, functionName, "invalid texture dimensions");
        return false;
    }
    Checked<uint32_t, RecordOverflow> total = srcOffset;
    total *= JSC::elementSize(pixels->getType());
    total += totalBytesRequired;
    total += skipBytes;
    if (total.hasOverflowed() || pixels->byteLength() < total.unsafeGet()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "ArrayBufferView not big enough for request");
        return false;
    }
    return true;
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

    // ANGLE subsumes the validation of the texture level and the texture dimensions for the target.
#if USE(ANGLE)
    UNUSED_PARAM(target);
    UNUSED_PARAM(level);
#else
    GCGLint maxTextureSizeForLevel = pow(2.0, m_maxTextureLevel - 1 - level);
    switch (target) {
    case GraphicsContextGL::TEXTURE_2D:
        if (width > maxTextureSizeForLevel || height > maxTextureSizeForLevel) {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "width or height out of range");
            return false;
        }
        break;
    case GraphicsContextGL::TEXTURE_CUBE_MAP_POSITIVE_X:
    case GraphicsContextGL::TEXTURE_CUBE_MAP_NEGATIVE_X:
    case GraphicsContextGL::TEXTURE_CUBE_MAP_POSITIVE_Y:
    case GraphicsContextGL::TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case GraphicsContextGL::TEXTURE_CUBE_MAP_POSITIVE_Z:
    case GraphicsContextGL::TEXTURE_CUBE_MAP_NEGATIVE_Z:
        if (functionType != TexImageFunctionType::TexSubImage && width != height) {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "width != height for cube map");
            return false;
        }
        // No need to check height here. For texImage width == height.
        // For texSubImage that will be checked when checking yoffset + height is in range.
        if (width > maxTextureSizeForLevel) {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "width or height out of range for cube map");
            return false;
        }
        break;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid target");
        return false;
    }
#endif // USE(ANGLE)

    if (border) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "border != 0");
        return false;
    }

    return true;
}

void WebGLRenderingContextBase::addExtensionSupportedFormatsAndTypes()
{
    if (!m_areOESTextureFloatFormatsAndTypesAdded && extensionIsEnabled("OES_texture_float")) {
        ADD_VALUES_TO_SET(m_supportedTexImageSourceTypes, SupportedTypesOESTextureFloat);
        m_areOESTextureFloatFormatsAndTypesAdded = true;
    }

    if (!m_areOESTextureHalfFloatFormatsAndTypesAdded && extensionIsEnabled("OES_texture_half_float")) {
        ADD_VALUES_TO_SET(m_supportedTexImageSourceTypes, SupportedTypesOESTextureHalfFloat);
        m_areOESTextureHalfFloatFormatsAndTypesAdded = true;
    }

    if (!m_areEXTsRGBFormatsAndTypesAdded && extensionIsEnabled("EXT_sRGB")) {
        ADD_VALUES_TO_SET(m_supportedTexImageSourceInternalFormats, SupportedInternalFormatsEXTsRGB);
        ADD_VALUES_TO_SET(m_supportedTexImageSourceFormats, SupportedFormatsEXTsRGB);
        m_areEXTsRGBFormatsAndTypesAdded = true;
    }
}

void WebGLRenderingContextBase::addExtensionSupportedFormatsAndTypesWebGL2()
{
    // FIXME: add EXT_texture_norm16 support.
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
        if (level > 0) {
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "level must be 0 for depth formats");
            return false;
        }
        break;
    case ExtensionsGL::SRGB_EXT:
    case ExtensionsGL::SRGB_ALPHA_EXT:
        if (!m_extsRGB) {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "sRGB texture formats not enabled");
            return false;
        }
        break;
    default:
#if ENABLE(WEBGL2)
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
        } else
#endif
        {
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
#if ENABLE(WEBGL2)
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
        } else
#endif
        {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid texture type");
            return false;
        }
    }

#if USE(ANGLE)
    UNUSED_PARAM(internalFormat);
#else
    // Verify that the combination of internalformat, format, and type is supported.
#define INTERNAL_FORMAT_CASE(internalFormatMacro, formatMacro, type0, type1, type2, type3, type4) case GraphicsContextGLOpenGL::internalFormatMacro: \
    if (format != GraphicsContextGLOpenGL::formatMacro) { \
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "invalid format for internalformat"); \
        return false; \
    } \
    if (type != type0 && type != type1 && type != type2 && type != type3 && type != type4) { \
        if (type != GraphicsContextGL::HALF_FLOAT_OES || (type0 != GraphicsContextGL::HALF_FLOAT && type1 != GraphicsContextGL::HALF_FLOAT && type2 != GraphicsContextGL::HALF_FLOAT && type3 != GraphicsContextGL::HALF_FLOAT)) { \
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "invalid type for internalformat"); \
            return false; \
        } \
    } \
    break;
    switch (internalFormat) {
    INTERNAL_FORMAT_CASE(RGB               , RGB            , GraphicsContextGL::UNSIGNED_BYTE                 , GraphicsContextGL::UNSIGNED_SHORT_5_6_5  , GraphicsContextGL::HALF_FLOAT                  , GraphicsContextGL::FLOAT     , 0                       );
    INTERNAL_FORMAT_CASE(RGBA              , RGBA           , GraphicsContextGL::UNSIGNED_BYTE                 , GraphicsContextGL::UNSIGNED_SHORT_4_4_4_4, GraphicsContextGL::UNSIGNED_SHORT_5_5_5_1      , GraphicsContextGL::HALF_FLOAT, GraphicsContextGL::FLOAT);
    INTERNAL_FORMAT_CASE(LUMINANCE_ALPHA   , LUMINANCE_ALPHA, GraphicsContextGL::UNSIGNED_BYTE                 , GraphicsContextGL::HALF_FLOAT            , GraphicsContextGL::FLOAT                       , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(LUMINANCE         , LUMINANCE      , GraphicsContextGL::UNSIGNED_BYTE                 , GraphicsContextGL::HALF_FLOAT            , GraphicsContextGL::FLOAT                       , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(ALPHA             , ALPHA          , GraphicsContextGL::UNSIGNED_BYTE                 , GraphicsContextGL::HALF_FLOAT            , GraphicsContextGL::FLOAT                       , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(R8                , RED            , GraphicsContextGL::UNSIGNED_BYTE                 , 0                                        , 0                                              , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(R8_SNORM          , RED            , GraphicsContextGL::BYTE                          , 0                                        , 0                                              , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(R16F              , RED            , GraphicsContextGL::HALF_FLOAT                    , GraphicsContextGL::FLOAT                 , 0                                              , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(R32F              , RED            , GraphicsContextGL::FLOAT                         , 0                                        , 0                                              , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(R8UI              , RED_INTEGER    , GraphicsContextGL::UNSIGNED_BYTE                 , 0                                        , 0                                              , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(R8I               , RED_INTEGER    , GraphicsContextGL::BYTE                          , 0                                        , 0                                              , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(R16UI             , RED_INTEGER    , GraphicsContextGL::UNSIGNED_SHORT                , 0                                        , 0                                              , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(R16I              , RED_INTEGER    , GraphicsContextGL::SHORT                         , 0                                        , 0                                              , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(R32UI             , RED_INTEGER    , GraphicsContextGL::UNSIGNED_INT                  , 0                                        , 0                                              , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(R32I              , RED_INTEGER    , GraphicsContextGL::INT                           , 0                                        , 0                                              , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(RG8               , RG             , GraphicsContextGL::UNSIGNED_BYTE                 , 0                                        , 0                                              , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(RG8_SNORM         , RG             , GraphicsContextGL::BYTE                          , 0                                        , 0                                              , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(RG16F             , RG             , GraphicsContextGL::HALF_FLOAT                    , GraphicsContextGL::FLOAT                 , 0                                              , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(RG32F             , RG             , GraphicsContextGL::FLOAT                         , 0                                        , 0                                              , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(RG8UI             , RG_INTEGER     , GraphicsContextGL::UNSIGNED_BYTE                 , 0                                        , 0                                              , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(RG8I              , RG_INTEGER     , GraphicsContextGL::BYTE                          , 0                                        , 0                                              , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(RG16UI            , RG_INTEGER     , GraphicsContextGL::UNSIGNED_SHORT                , 0                                        , 0                                              , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(RG16I             , RG_INTEGER     , GraphicsContextGL::SHORT                         , 0                                        , 0                                              , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(RG32UI            , RG_INTEGER     , GraphicsContextGL::UNSIGNED_INT                  , 0                                        , 0                                              , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(RG32I             , RG_INTEGER     , GraphicsContextGL::INT                           , 0                                        , 0                                              , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(RGB8              , RGB            , GraphicsContextGL::UNSIGNED_BYTE                 , 0                                        , 0                                              , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(SRGB8             , RGB            , GraphicsContextGL::UNSIGNED_BYTE                 , 0                                        , 0                                              , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(RGB565            , RGB            , GraphicsContextGL::UNSIGNED_BYTE                 , GraphicsContextGL::UNSIGNED_SHORT_5_6_5  , 0                                              , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(RGB8_SNORM        , RGB            , GraphicsContextGL::BYTE                          , 0                                        , 0                                              , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(R11F_G11F_B10F    , RGB            , GraphicsContextGL::UNSIGNED_INT_10F_11F_11F_REV  , GraphicsContextGL::HALF_FLOAT            , GraphicsContextGL::FLOAT                       , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(RGB9_E5           , RGB            , GraphicsContextGL::UNSIGNED_INT_5_9_9_9_REV      , GraphicsContextGL::HALF_FLOAT            , GraphicsContextGL::FLOAT                       , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(RGB16F            , RGB            , GraphicsContextGL::HALF_FLOAT                    , GraphicsContextGL::FLOAT                 , 0                                              , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(RGB32F            , RGB            , GraphicsContextGL::FLOAT                         , 0                                        , 0                                              , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(RGB8UI            , RGB_INTEGER    , GraphicsContextGL::UNSIGNED_BYTE                 , 0                                        , 0                                              , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(RGB8I             , RGB_INTEGER    , GraphicsContextGL::BYTE                          , 0                                        , 0                                              , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(RGB16UI           , RGB_INTEGER    , GraphicsContextGL::UNSIGNED_SHORT                , 0                                        , 0                                              , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(RGB16I            , RGB_INTEGER    , GraphicsContextGL::SHORT                         , 0                                        , 0                                              , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(RGB32UI           , RGB_INTEGER    , GraphicsContextGL::UNSIGNED_INT                  , 0                                        , 0                                              , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(RGB32I            , RGB_INTEGER    , GraphicsContextGL::INT                           , 0                                        , 0                                              , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(RGBA8             , RGBA           , GraphicsContextGL::UNSIGNED_BYTE                 , 0                                        , 0                                              , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(SRGB8_ALPHA8      , RGBA           , GraphicsContextGL::UNSIGNED_BYTE                 , 0                                        , 0                                              , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(RGBA8_SNORM       , RGBA           , GraphicsContextGL::BYTE                          , 0                                        , 0                                              , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(RGB5_A1           , RGBA           , GraphicsContextGL::UNSIGNED_BYTE                 , GraphicsContextGL::UNSIGNED_SHORT_5_5_5_1, GraphicsContextGL::UNSIGNED_INT_2_10_10_10_REV , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(RGBA4             , RGBA           , GraphicsContextGL::UNSIGNED_BYTE                 , GraphicsContextGL::UNSIGNED_SHORT_4_4_4_4, 0                                              , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(RGB10_A2          , RGBA           , GraphicsContextGL::UNSIGNED_INT_2_10_10_10_REV   , 0                                        , 0                                              , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(RGBA16F           , RGBA           , GraphicsContextGL::HALF_FLOAT                    , GraphicsContextGL::FLOAT                 , 0                                              , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(RGBA32F           , RGBA           , GraphicsContextGL::FLOAT                         , 0                                        , 0                                              , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(RGBA8UI           , RGBA_INTEGER   , GraphicsContextGL::UNSIGNED_BYTE                 , 0                                        , 0                                              , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(RGBA8I            , RGBA_INTEGER   , GraphicsContextGL::BYTE                          , 0                                        , 0                                              , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(RGB10_A2UI        , RGBA_INTEGER   , GraphicsContextGL::UNSIGNED_INT_2_10_10_10_REV   , 0                                        , 0                                              , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(RGBA16UI          , RGBA_INTEGER   , GraphicsContextGL::UNSIGNED_SHORT                , 0                                        , 0                                              , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(RGBA16I           , RGBA_INTEGER   , GraphicsContextGL::SHORT                         , 0                                        , 0                                              , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(RGBA32I           , RGBA_INTEGER   , GraphicsContextGL::INT                           , 0                                        , 0                                              , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(RGBA32UI          , RGBA_INTEGER   , GraphicsContextGL::UNSIGNED_INT                  , 0                                        , 0                                              , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(DEPTH_COMPONENT   , DEPTH_COMPONENT, GraphicsContextGL::UNSIGNED_SHORT                , GraphicsContextGL::UNSIGNED_INT          , 0                                              , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(DEPTH_COMPONENT16 , DEPTH_COMPONENT, GraphicsContextGL::UNSIGNED_SHORT                , GraphicsContextGL::UNSIGNED_INT          , 0                                              , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(DEPTH_COMPONENT24 , DEPTH_COMPONENT, GraphicsContextGL::UNSIGNED_INT                  , 0                                        , 0                                              , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(DEPTH_COMPONENT32F, DEPTH_COMPONENT, GraphicsContextGL::FLOAT                         , 0                                        , 0                                              , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(DEPTH_STENCIL     , DEPTH_STENCIL  , GraphicsContextGL::UNSIGNED_INT_24_8             , 0                                        , 0                                              , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(DEPTH24_STENCIL8  , DEPTH_STENCIL  , GraphicsContextGL::UNSIGNED_INT_24_8             , 0                                        , 0                                              , 0                            , 0                       );
    INTERNAL_FORMAT_CASE(DEPTH32F_STENCIL8 , DEPTH_STENCIL  , GraphicsContextGL::FLOAT_32_UNSIGNED_INT_24_8_REV, 0                                        , 0                                              , 0                            , 0                       );
    case ExtensionsGL::SRGB_EXT:
        if (format != internalFormat) {
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "format and internalformat must match");
            return false;
        }
        if (type != GraphicsContextGL::UNSIGNED_BYTE && type != GraphicsContextGL::UNSIGNED_SHORT_5_6_5 && type != GraphicsContextGL::FLOAT && type != GraphicsContextGL::HALF_FLOAT_OES && type != GraphicsContextGL::HALF_FLOAT) {
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "invalid type for internal format");
            return false;
        }
        break;
    case ExtensionsGL::SRGB_ALPHA_EXT:
        if (format != internalFormat) {
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "format and internalformat must match");
            return false;
        }
        if (type != GraphicsContextGL::UNSIGNED_BYTE && type != GraphicsContextGL::UNSIGNED_SHORT_4_4_4_4 && type != GraphicsContextGL::UNSIGNED_SHORT_5_5_5_1 && type != GraphicsContextGL::FLOAT && type != GraphicsContextGL::HALF_FLOAT_OES && type != GraphicsContextGL::HALF_FLOAT) {
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "invalid type for internal format");
            return false;
        }
        break;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "Unknown internal format");
        return false;
    }
#undef INTERNAL_FORMAT_CASE

#endif // USE(ANGLE)
    return true;
}

void WebGLRenderingContextBase::copyTexImage2D(GCGLenum target, GCGLint level, GCGLenum internalFormat, GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height, GCGLint border)
{
    if (isContextLostOrPending())
        return;
#if !USE(ANGLE)
    // Fake the source so that simple validation of the format/type is done. This is subsumed by ANGLE.
    if (!validateTexFuncParameters("copyTexImage2D", TexImageFunctionType::CopyTexImage, TexFuncValidationSourceType::SourceArrayBufferView, target, level, internalFormat, width, height, 1, border, internalFormat, GraphicsContextGL::UNSIGNED_BYTE))
        return;
#endif
    if (!validateSettableTexInternalFormat("copyTexImage2D", internalFormat))
        return;
    auto tex = validateTexture2DBinding("copyTexImage2D", target);
    if (!tex)
        return;
#if USE(ANGLE)
    clearIfComposited();
    m_context->copyTexImage2D(target, level, internalFormat, x, y, width, height, border);
#else
    if (!isTexInternalFormatColorBufferCombinationValid(internalFormat, getBoundReadFramebufferColorFormat())) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "copyTexImage2D", "framebuffer is incompatible format");
        return;
    }
    if (!isGLES2NPOTStrict() && level && WebGLTexture::isNPOT(width, height)) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "copyTexImage2D", "level > 0 not power of 2");
        return;
    }
    const char* reason = "framebuffer incomplete";
    if (getReadFramebufferBinding() && !getReadFramebufferBinding()->onAccess(graphicsContextGL(), &reason)) {
        synthesizeGLError(GraphicsContextGL::INVALID_FRAMEBUFFER_OPERATION, "copyTexImage2D", reason);
        return;
    }
    clearIfComposited();

    GCGLint clippedX, clippedY;
    GCGLsizei clippedWidth, clippedHeight;
    if (clip2D(x, y, width, height, getBoundReadFramebufferWidth(), getBoundReadFramebufferHeight(), &clippedX, &clippedY, &clippedWidth, &clippedHeight)) {
        m_context->texImage2DResourceSafe(target, level, internalFormat, width, height, border,
            internalFormat, GraphicsContextGL::UNSIGNED_BYTE, m_unpackAlignment);
        if (clippedWidth > 0 && clippedHeight > 0) {
            m_context->copyTexSubImage2D(target, level, clippedX - x, clippedY - y,
                clippedX, clippedY, clippedWidth, clippedHeight);
        }
    } else
        m_context->copyTexImage2D(target, level, internalFormat, x, y, width, height, border);

    // FIXME: if the framebuffer is not complete, none of the below should be executed.
    tex->setLevelInfo(target, level, internalFormat, width, height, GraphicsContextGL::UNSIGNED_BYTE);
#endif
}

ExceptionOr<void> WebGLRenderingContextBase::texImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLenum format, GCGLenum type, Optional<TexImageSource> source)
{
    if (isContextLostOrPending())
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
    ImageBuffer* buf = m_generatedImageCache.imageBuffer(size);
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

RefPtr<Image> WebGLRenderingContextBase::videoFrameToImage(HTMLVideoElement* video, BackingStoreCopy backingStoreCopy)
{
    IntSize size(video->videoWidth(), video->videoHeight());
    ImageBuffer* buf = m_generatedImageCache.imageBuffer(size);
    if (!buf) {
        synthesizeGLError(GraphicsContextGL::OUT_OF_MEMORY, "texImage2D", "out of memory");
        return nullptr;
    }
    FloatRect destRect(0, 0, size.width(), size.height());
    // FIXME: Turn this into a GPU-GPU texture copy instead of CPU readback.
    video->paintCurrentFrameInContext(buf->context(), destRect);
    return buf->copyImage(backingStoreCopy);
}

#endif

void WebGLRenderingContextBase::texParameter(GCGLenum target, GCGLenum pname, GCGLfloat paramf, GCGLint parami, bool isFloat)
{
    if (isContextLostOrPending())
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
    case ExtensionsGL::TEXTURE_MAX_ANISOTROPY_EXT: // EXT_texture_filter_anisotropic
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
    if (isFloat) {
#if !USE(ANGLE)
        tex->setParameterf(pname, paramf);
#endif
        m_context->texParameterf(target, pname, paramf);
    } else {
#if !USE(ANGLE)
        tex->setParameteri(pname, parami);
#endif
        m_context->texParameteri(target, pname, parami);
    }
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
    if (isContextLostOrPending() || !validateUniformLocation("uniform1f", location))
        return;

    m_context->uniform1f(location->location(), x);
}

void WebGLRenderingContextBase::uniform2f(const WebGLUniformLocation* location, GCGLfloat x, GCGLfloat y)
{
    if (isContextLostOrPending() || !validateUniformLocation("uniform2f", location))
        return;

    m_context->uniform2f(location->location(), x, y);
}

void WebGLRenderingContextBase::uniform3f(const WebGLUniformLocation* location, GCGLfloat x, GCGLfloat y, GCGLfloat z)
{
    if (isContextLostOrPending() || !validateUniformLocation("uniform3f", location))
        return;

    m_context->uniform3f(location->location(), x, y, z);
}

void WebGLRenderingContextBase::uniform4f(const WebGLUniformLocation* location, GCGLfloat x, GCGLfloat y, GCGLfloat z, GCGLfloat w)
{
    if (isContextLostOrPending() || !validateUniformLocation("uniform4f", location))
        return;

    m_context->uniform4f(location->location(), x, y, z, w);
}

void WebGLRenderingContextBase::uniform1i(const WebGLUniformLocation* location, GCGLint x)
{
    if (isContextLostOrPending() || !validateUniformLocation("uniform1i", location))
        return;

    if ((location->type() == GraphicsContextGL::SAMPLER_2D || location->type() == GraphicsContextGL::SAMPLER_CUBE) && x >= (int)m_textureUnits.size()) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "uniform1i", "invalid texture unit");
        return;
    }

    m_context->uniform1i(location->location(), x);
}

void WebGLRenderingContextBase::uniform2i(const WebGLUniformLocation* location, GCGLint x, GCGLint y)
{
    if (isContextLostOrPending() || !validateUniformLocation("uniform2i", location))
        return;

    m_context->uniform2i(location->location(), x, y);
}

void WebGLRenderingContextBase::uniform3i(const WebGLUniformLocation* location, GCGLint x, GCGLint y, GCGLint z)
{
    if (isContextLostOrPending() || !validateUniformLocation("uniform3i", location))
        return;

    m_context->uniform3i(location->location(), x, y, z);
}

void WebGLRenderingContextBase::uniform4i(const WebGLUniformLocation* location, GCGLint x, GCGLint y, GCGLint z, GCGLint w)
{
    if (isContextLostOrPending() || !validateUniformLocation("uniform4i", location))
        return;

    m_context->uniform4i(location->location(), x, y, z, w);
}

void WebGLRenderingContextBase::uniform1fv(const WebGLUniformLocation* location, Float32List&& v)
{
    if (isContextLostOrPending() || !validateUniformParameters("uniform1fv", location, v, 1, 0, v.length()))
        return;

    m_context->uniform1fv(location->location(), v.length(), v.data());
}

void WebGLRenderingContextBase::uniform2fv(const WebGLUniformLocation* location, Float32List&& v)
{
    if (isContextLostOrPending() || !validateUniformParameters("uniform2fv", location, v, 2, 0, v.length()))
        return;

    m_context->uniform2fv(location->location(), v.length() / 2, v.data());
}

void WebGLRenderingContextBase::uniform3fv(const WebGLUniformLocation* location, Float32List&& v)
{
    if (isContextLostOrPending() || !validateUniformParameters("uniform3fv", location, v, 3, 0, v.length()))
        return;

    m_context->uniform3fv(location->location(), v.length() / 3, v.data());
}

void WebGLRenderingContextBase::uniform4fv(const WebGLUniformLocation* location, Float32List&& v)
{
    if (isContextLostOrPending() || !validateUniformParameters("uniform4fv", location, v, 4, 0, v.length()))
        return;

    m_context->uniform4fv(location->location(), v.length() / 4, v.data());
}

void WebGLRenderingContextBase::uniform1iv(const WebGLUniformLocation* location, Int32List&& v)
{
    if (isContextLostOrPending() || !validateUniformParameters("uniform1iv", location, v, 1, 0, v.length()))
        return;

    auto data = v.data();
    auto length = v.length();

    if (location->type() == GraphicsContextGL::SAMPLER_2D || location->type() == GraphicsContextGL::SAMPLER_CUBE) {
        for (auto i = 0; i < length; ++i) {
            if (data[i] >= static_cast<int>(m_textureUnits.size())) {
                LOG(WebGL, "Texture unit size=%zu, v[%d]=%d. Location type = %04X.", m_textureUnits.size(), i, data[i], location->type());
                synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "uniform1iv", "invalid texture unit");
                return;
            }
        }
    }

    m_context->uniform1iv(location->location(), length, data);
}

void WebGLRenderingContextBase::uniform2iv(const WebGLUniformLocation* location, Int32List&& v)
{
    if (isContextLostOrPending() || !validateUniformParameters("uniform2iv", location, v, 2, 0, v.length()))
        return;

    m_context->uniform2iv(location->location(), v.length() / 2, v.data());
}

void WebGLRenderingContextBase::uniform3iv(const WebGLUniformLocation* location, Int32List&& v)
{
    if (isContextLostOrPending() || !validateUniformParameters("uniform3iv", location, v, 3, 0, v.length()))
        return;

    m_context->uniform3iv(location->location(), v.length() / 3, v.data());
}

void WebGLRenderingContextBase::uniform4iv(const WebGLUniformLocation* location, Int32List&& v)
{
    if (isContextLostOrPending() || !validateUniformParameters("uniform4iv", location, v, 4, 0, v.length()))
        return;

    m_context->uniform4iv(location->location(), v.length() / 4, v.data());
}

void WebGLRenderingContextBase::uniformMatrix2fv(const WebGLUniformLocation* location, GCGLboolean transpose, Float32List&& v)
{
    if (isContextLostOrPending() || !validateUniformMatrixParameters("uniformMatrix2fv", location, transpose, v, 4, 0, v.length()))
        return;
    m_context->uniformMatrix2fv(location->location(), v.length() / 4, transpose, v.data());
}

void WebGLRenderingContextBase::uniformMatrix3fv(const WebGLUniformLocation* location, GCGLboolean transpose, Float32List&& v)
{
    if (isContextLostOrPending() || !validateUniformMatrixParameters("uniformMatrix3fv", location, transpose, v, 9, 0, v.length()))
        return;
    m_context->uniformMatrix3fv(location->location(), v.length() / 9, transpose, v.data());
}

void WebGLRenderingContextBase::uniformMatrix4fv(const WebGLUniformLocation* location, GCGLboolean transpose, Float32List&& v)
{
    if (isContextLostOrPending() || !validateUniformMatrixParameters("uniformMatrix4fv", location, transpose, v, 16, 0, v.length()))
        return;
    m_context->uniformMatrix4fv(location->location(), v.length() / 16, transpose, v.data());
}

void WebGLRenderingContextBase::useProgram(WebGLProgram* program)
{
    if (!checkObjectToBeBound("useProgram", program))
        return;
    if (program && !program->getLinkStatus()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "useProgram", "program not valid");
        return;
    }
    if (m_currentProgram != program) {
        if (m_currentProgram)
            m_currentProgram->onDetached(graphicsContextGL());
        m_currentProgram = program;
        m_context->useProgram(objectOrZero(program));
        if (program)
            program->onAttached();
    }
}

void WebGLRenderingContextBase::validateProgram(WebGLProgram* program)
{
    if (isContextLostOrPending() || !validateWebGLProgramOrShader("validateProgram", program))
        return;
    m_context->validateProgram(objectOrZero(program));
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
    if (isContextLostOrPending())
        return;
    switch (type) {
    case GraphicsContextGL::BYTE:
    case GraphicsContextGL::UNSIGNED_BYTE:
    case GraphicsContextGL::SHORT:
    case GraphicsContextGL::UNSIGNED_SHORT:
    case GraphicsContextGL::FLOAT:
        break;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "vertexAttribPointer", "invalid type");
        return;
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

    m_boundVertexArrayObject->setVertexAttribState(index, bytesPerElement, size, type, normalized, stride, static_cast<GCGLintptr>(offset), m_boundArrayBuffer.get());
    m_context->vertexAttribPointer(index, size, type, normalized, stride, static_cast<GCGLintptr>(offset));
}

void WebGLRenderingContextBase::viewport(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height)
{
    if (isContextLostOrPending())
        return;
    if (!validateSize("viewport", width, height))
        return;
    m_context->viewport(x, y, width, height);
}

void WebGLRenderingContextBase::forceLostContext(WebGLRenderingContextBase::LostContextMode mode)
{
    if (isContextLostOrPending()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "loseContext", "context already lost");
        return;
    }

    m_contextGroup->loseContextGroup(mode);
}

void WebGLRenderingContextBase::loseContextImpl(WebGLRenderingContextBase::LostContextMode mode)
{
    if (isContextLost())
        return;

    m_contextLost = true;
    m_contextLostMode = mode;

    if (mode == RealLostContext) {
        // Inform the embedder that a lost context was received. In response, the embedder might
        // decide to take action such as asking the user for permission to use WebGL again.
        auto* canvas = htmlCanvas();
        if (canvas) {
            if (RefPtr<Frame> frame = canvas->document().frame())
                frame->loader().client().didLoseWebGLContext(m_context->getExtensions().getGraphicsResetStatusARB());
        }
    }

    detachAndRemoveAllObjects();

    // There is no direct way to clear errors from a GL implementation and
    // looping until getError() becomes NO_ERROR might cause an infinite loop if
    // the driver or context implementation had a bug. So, loop a reasonably
    // large number of times to clear any existing errors.
    for (int i = 0; i < 100; ++i) {
        if (m_context->getError() == GraphicsContextGL::NO_ERROR)
            break;
    }
    ConsoleDisplayPreference display = (mode == RealLostContext) ? DisplayInConsole: DontDisplayInConsole;
    synthesizeGLError(GraphicsContextGL::CONTEXT_LOST_WEBGL, "loseContext", "context lost", display);

    // Don't allow restoration unless the context lost event has both been
    // dispatched and its default behavior prevented.
    m_restoreAllowed = false;

    // Always defer the dispatch of the context lost event, to implement
    // the spec behavior of queueing a task.
    scheduleTaskToDispatchContextLostEvent();
}

void WebGLRenderingContextBase::forceRestoreContext()
{
    if (!isContextLostOrPending()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "restoreContext", "context not lost");
        return;
    }

    if (!m_restoreAllowed) {
        if (m_contextLostMode == SyntheticLostContext)
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "restoreContext", "context restoration not allowed");
        return;
    }

    if (!m_restoreTimer.isActive())
        m_restoreTimer.startOneShot(0_s);
}

bool WebGLRenderingContextBase::isContextUnrecoverablyLost() const
{
    return m_contextLost && !m_restoreAllowed;
}

PlatformLayer* WebGLRenderingContextBase::platformLayer() const
{
    return (!isContextLost() && !m_isPendingPolicyResolution) ? m_context->platformLayer() : 0;
}

void WebGLRenderingContextBase::removeSharedObject(WebGLSharedObject& object)
{
    if (m_isPendingPolicyResolution)
        return;

    m_contextGroup->removeObject(object);
}

void WebGLRenderingContextBase::addSharedObject(WebGLSharedObject& object)
{
    if (m_isPendingPolicyResolution)
        return;

    ASSERT(!isContextLost());
    m_contextGroup->addObject(object);
}

void WebGLRenderingContextBase::removeContextObject(WebGLContextObject& object)
{
    if (m_isPendingPolicyResolution)
        return;

    m_contextObjects.remove(&object);
}

void WebGLRenderingContextBase::addContextObject(WebGLContextObject& object)
{
    if (m_isPendingPolicyResolution)
        return;

    ASSERT(!isContextLost());
    m_contextObjects.add(&object);
}

void WebGLRenderingContextBase::detachAndRemoveAllObjects()
{
    if (m_isPendingPolicyResolution)
        return;

    while (m_contextObjects.size() > 0) {
        HashSet<WebGLContextObject*>::iterator it = m_contextObjects.begin();
        (*it)->detachContext();
    }
}

void WebGLRenderingContextBase::stop()
{
    if (!isContextLost() && !m_isPendingPolicyResolution) {
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
    GCGLboolean value = 0;
    m_context->getBooleanv(pname, &value);
    return value;
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
    GCGLfloat value = 0;
    m_context->getFloatv(pname, &value);
    return value;
}

int WebGLRenderingContextBase::getIntParameter(GCGLenum pname)
{
    GCGLint value = 0;
    m_context->getIntegerv(pname, &value);
    return value;
}

unsigned WebGLRenderingContextBase::getUnsignedIntParameter(GCGLenum pname)
{
    GCGLint value = 0;
    m_context->getIntegerv(pname, &value);
    return value;
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

GraphicsContextGLOpenGL::PixelStoreParams WebGLRenderingContextBase::getPackPixelStoreParams() const
{
    GraphicsContextGLOpenGL::PixelStoreParams params;
    params.alignment = m_packAlignment;
    return params;
}

GraphicsContextGLOpenGL::PixelStoreParams WebGLRenderingContextBase::getUnpackPixelStoreParams(TexImageDimension dimension) const
{
    UNUSED_PARAM(dimension);
    GraphicsContextGLOpenGL::PixelStoreParams params;
    params.alignment = m_unpackAlignment;
    return params;
}

#if !USE(ANGLE)
bool WebGLRenderingContextBase::checkTextureCompleteness(const char* functionName, bool prepareToDraw)
{
    bool resetActiveUnit = false;
    bool usesAtLeastOneBlackTexture = false;
    WebGLTexture::TextureExtensionFlag extensions = textureExtensionFlags();

    Vector<unsigned> noLongerUnrenderable;
    for (unsigned badTexture : m_unrenderableTextureUnits) {
        ASSERT(badTexture < m_textureUnits.size());
        auto& textureUnit = m_textureUnits[badTexture];
        bool needsToUseBlack2DTexture = textureUnit.texture2DBinding && textureUnit.texture2DBinding->needToUseBlackTexture(extensions);
        bool needsToUseBlack3DTexture = textureUnit.textureCubeMapBinding && textureUnit.textureCubeMapBinding->needToUseBlackTexture(extensions);

        if (!needsToUseBlack2DTexture && !needsToUseBlack3DTexture) {
            noLongerUnrenderable.append(badTexture);
            continue;
        }

        usesAtLeastOneBlackTexture = true;

        if (badTexture != m_activeTextureUnit) {
            m_context->activeTexture(badTexture + GraphicsContextGL::TEXTURE0);
            resetActiveUnit = true;
        } else if (resetActiveUnit) {
            m_context->activeTexture(badTexture + GraphicsContextGL::TEXTURE0);
            resetActiveUnit = false;
        }
        RefPtr<WebGLTexture> tex2D;
        RefPtr<WebGLTexture> texCubeMap;
        if (prepareToDraw) {
            printToConsole(MessageLevel::Error, makeString("WebGL: ", functionName, ": texture bound to texture unit ", badTexture,
                " is not renderable. It maybe non-power-of-2 and have incompatible texture filtering or is not 'texture complete',"
                " or it is a float/half-float type with linear filtering and without the relevant float/half-float linear extension enabled."));
            tex2D = m_blackTexture2D.get();
            texCubeMap = m_blackTextureCubeMap.get();
        } else {
            tex2D = textureUnit.texture2DBinding.get();
            texCubeMap = textureUnit.textureCubeMapBinding.get();
        }
        if (needsToUseBlack2DTexture)
            m_context->bindTexture(GraphicsContextGL::TEXTURE_2D, objectOrZero(tex2D.get()));
        if (needsToUseBlack3DTexture)
            m_context->bindTexture(GraphicsContextGL::TEXTURE_CUBE_MAP, objectOrZero(texCubeMap.get()));
    }
    if (resetActiveUnit)
        m_context->activeTexture(m_activeTextureUnit + GraphicsContextGL::TEXTURE0);

    for (unsigned renderable : noLongerUnrenderable)
        m_unrenderableTextureUnits.remove(renderable);

    return usesAtLeastOneBlackTexture;
}

void WebGLRenderingContextBase::createFallbackBlackTextures1x1()
{
    unsigned char black[] = {0, 0, 0, 255};
    m_blackTexture2D = createTexture();
    m_context->bindTexture(GraphicsContextGL::TEXTURE_2D, m_blackTexture2D->object());
    m_context->texImage2D(GraphicsContextGL::TEXTURE_2D, 0, GraphicsContextGL::RGBA, 1, 1,
        0, GraphicsContextGL::RGBA, GraphicsContextGL::UNSIGNED_BYTE, black);
    m_context->bindTexture(GraphicsContextGL::TEXTURE_2D, 0);
    m_blackTextureCubeMap = createTexture();
    m_context->bindTexture(GraphicsContextGL::TEXTURE_CUBE_MAP, m_blackTextureCubeMap->object());
    m_context->texImage2D(GraphicsContextGL::TEXTURE_CUBE_MAP_POSITIVE_X, 0, GraphicsContextGL::RGBA, 1, 1,
        0, GraphicsContextGL::RGBA, GraphicsContextGL::UNSIGNED_BYTE, black);
    m_context->texImage2D(GraphicsContextGL::TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GraphicsContextGL::RGBA, 1, 1,
        0, GraphicsContextGL::RGBA, GraphicsContextGL::UNSIGNED_BYTE, black);
    m_context->texImage2D(GraphicsContextGL::TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GraphicsContextGL::RGBA, 1, 1,
        0, GraphicsContextGL::RGBA, GraphicsContextGL::UNSIGNED_BYTE, black);
    m_context->texImage2D(GraphicsContextGL::TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GraphicsContextGL::RGBA, 1, 1,
        0, GraphicsContextGL::RGBA, GraphicsContextGL::UNSIGNED_BYTE, black);
    m_context->texImage2D(GraphicsContextGL::TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GraphicsContextGL::RGBA, 1, 1,
        0, GraphicsContextGL::RGBA, GraphicsContextGL::UNSIGNED_BYTE, black);
    m_context->texImage2D(GraphicsContextGL::TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GraphicsContextGL::RGBA, 1, 1,
        0, GraphicsContextGL::RGBA, GraphicsContextGL::UNSIGNED_BYTE, black);
    m_context->bindTexture(GraphicsContextGL::TEXTURE_CUBE_MAP, 0);
}

bool WebGLRenderingContextBase::isTexInternalFormatColorBufferCombinationValid(GCGLenum texInternalFormat, GCGLenum colorBufferFormat)
{
    auto need = GraphicsContextGLOpenGL::getChannelBitsByFormat(texInternalFormat);
    auto have = GraphicsContextGLOpenGL::getChannelBitsByFormat(colorBufferFormat);
    return (need & have) == need;
}

GCGLenum WebGLRenderingContextBase::getBoundReadFramebufferColorFormat()
{
    WebGLFramebuffer* framebuffer = getReadFramebufferBinding();
    if (framebuffer && framebuffer->object())
        return framebuffer->getColorBufferFormat();
    if (m_attributes.alpha)
        return GraphicsContextGL::RGBA;
    return GraphicsContextGL::RGB;
}

int WebGLRenderingContextBase::getBoundReadFramebufferWidth()
{
    WebGLFramebuffer* framebuffer = getReadFramebufferBinding();
    if (framebuffer && framebuffer->object())
        return framebuffer->getColorBufferWidth();
    return m_context->getInternalFramebufferSize().width();
}

int WebGLRenderingContextBase::getBoundReadFramebufferHeight()
{
    WebGLFramebuffer* framebuffer = getReadFramebufferBinding();
    if (framebuffer && framebuffer->object())
        return framebuffer->getColorBufferHeight();
    return m_context->getInternalFramebufferSize().height();
}
#endif // USE(ANGLE)

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

#if !USE(ANGLE)
    if (texture && texture->needToUseBlackTexture(textureExtensionFlags()))
        m_unrenderableTextureUnits.add(m_activeTextureUnit);
#endif // !USE(ANGLE)

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

#if !USE(ANGLE)
    if (texture && texture->needToUseBlackTexture(textureExtensionFlags()))
        m_unrenderableTextureUnits.add(m_activeTextureUnit);
#endif // !USE(ANGLE)

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
#if USE(ANGLE)
    UNUSED_PARAM(functionName);
    UNUSED_PARAM(target);
    UNUSED_PARAM(level);
    // ANGLE subsumes this validation.
    return true;
#else
    if (level < 0) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "level < 0");
        return false;
    }
    switch (target) {
    case GraphicsContextGL::TEXTURE_2D:
        if (level >= m_maxTextureLevel) {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "level out of range");
            return false;
        }
        break;
    case GraphicsContextGL::TEXTURE_CUBE_MAP_POSITIVE_X:
    case GraphicsContextGL::TEXTURE_CUBE_MAP_NEGATIVE_X:
    case GraphicsContextGL::TEXTURE_CUBE_MAP_POSITIVE_Y:
    case GraphicsContextGL::TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case GraphicsContextGL::TEXTURE_CUBE_MAP_POSITIVE_Z:
    case GraphicsContextGL::TEXTURE_CUBE_MAP_NEGATIVE_Z:
        if (level >= m_maxCubeMapTextureLevel) {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "level out of range");
            return false;
        }
        break;
    }
    // This function only checks if level is legal, so we return true and don't
    // generate INVALID_ENUM if target is illegal.
    return true;
#endif // USE(ANGLE)
}

#if !USE(ANGLE)
bool WebGLRenderingContextBase::validateCompressedTexFormat(GCGLenum format)
{
    return m_compressedTextureFormats.contains(format);
}

struct BlockParameters {
    const int width;
    const int height;
    const int size;
};

static inline unsigned calculateBytesForASTC(GCGLsizei width, GCGLsizei height, const BlockParameters& parameters)
{
    return ((width + parameters.width - 1) / parameters.width) * ((height + parameters.height - 1) / parameters.height) * parameters.size;
}

bool WebGLRenderingContextBase::validateCompressedTexFuncData(const char* functionName, GCGLsizei width, GCGLsizei height, GCGLenum format, ArrayBufferView& pixels)
{
    if (width < 0 || height < 0) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "width or height < 0");
        return false;
    }

    unsigned bytesRequired = 0;

    // Block parameters for ASTC formats
    const int kASTCBlockSize = 16;
    static const BlockParameters ASTCParameters[] {
        BlockParameters { 4, 4, kASTCBlockSize },
        BlockParameters { 5, 4, kASTCBlockSize },
        BlockParameters { 5, 5, kASTCBlockSize },
        BlockParameters { 6, 5, kASTCBlockSize },
        BlockParameters { 6, 6, kASTCBlockSize },
        BlockParameters { 8, 5, kASTCBlockSize },
        BlockParameters { 8, 6, kASTCBlockSize },
        BlockParameters { 8, 8, kASTCBlockSize },
        BlockParameters { 10, 5, kASTCBlockSize },
        BlockParameters { 10, 6, kASTCBlockSize },
        BlockParameters { 10, 8, kASTCBlockSize },
        BlockParameters { 10, 10, kASTCBlockSize },
        BlockParameters { 12, 10, kASTCBlockSize },
        BlockParameters { 12, 12, kASTCBlockSize }
    };
    const GCGLenum ASTCEnumStartRGBA = ExtensionsGL::COMPRESSED_RGBA_ASTC_4x4_KHR;
    const GCGLenum ASTCEnumStartSRGB8 = ExtensionsGL::COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR;

    const int kEACAndETC2BlockSize = 4;

    switch (format) {
    case ExtensionsGL::COMPRESSED_RGB_S3TC_DXT1_EXT:
    case ExtensionsGL::COMPRESSED_RGBA_S3TC_DXT1_EXT:
    case ExtensionsGL::COMPRESSED_ATC_RGB_AMD:
    case ExtensionsGL::ETC1_RGB8_OES: {
        const int kBlockSize = 8;
        const int kBlockWidth = 4;
        const int kBlockHeight = 4;
        int numBlocksAcross = (width + kBlockWidth - 1) / kBlockWidth;
        int numBlocksDown = (height + kBlockHeight - 1) / kBlockHeight;
        bytesRequired = numBlocksAcross * numBlocksDown * kBlockSize;
        break;
    }
    case ExtensionsGL::COMPRESSED_RGBA_S3TC_DXT3_EXT:
    case ExtensionsGL::COMPRESSED_RGBA_S3TC_DXT5_EXT:
    case ExtensionsGL::COMPRESSED_ATC_RGBA_EXPLICIT_ALPHA_AMD:
    case ExtensionsGL::COMPRESSED_ATC_RGBA_INTERPOLATED_ALPHA_AMD:
        {
            const int kBlockSize = 16;
            const int kBlockWidth = 4;
            const int kBlockHeight = 4;
            int numBlocksAcross = (width + kBlockWidth - 1) / kBlockWidth;
            int numBlocksDown = (height + kBlockHeight - 1) / kBlockHeight;
            bytesRequired = numBlocksAcross * numBlocksDown * kBlockSize;
        }
        break;
    case ExtensionsGL::COMPRESSED_RGB_PVRTC_4BPPV1_IMG:
    case ExtensionsGL::COMPRESSED_RGBA_PVRTC_4BPPV1_IMG:
        {
            const int kBlockSize = 8;
            const int kBlockWidth = 8;
            const int kBlockHeight = 8;
            bytesRequired = (std::max(width, kBlockWidth) * std::max(height, kBlockHeight) * 4 + 7) / kBlockSize;
        }
        break;
    case ExtensionsGL::COMPRESSED_RGB_PVRTC_2BPPV1_IMG:
    case ExtensionsGL::COMPRESSED_RGBA_PVRTC_2BPPV1_IMG:
        {
            const int kBlockSize = 8;
            const int kBlockWidth = 16;
            const int kBlockHeight = 8;
            bytesRequired = (std::max(width, kBlockWidth) * std::max(height, kBlockHeight) * 2 + 7) / kBlockSize;
        }
        break;
    case ExtensionsGL::COMPRESSED_RGBA_ASTC_4x4_KHR:
    case ExtensionsGL::COMPRESSED_RGBA_ASTC_5x4_KHR:
    case ExtensionsGL::COMPRESSED_RGBA_ASTC_5x5_KHR:
    case ExtensionsGL::COMPRESSED_RGBA_ASTC_6x5_KHR:
    case ExtensionsGL::COMPRESSED_RGBA_ASTC_6x6_KHR:
    case ExtensionsGL::COMPRESSED_RGBA_ASTC_8x5_KHR:
    case ExtensionsGL::COMPRESSED_RGBA_ASTC_8x6_KHR:
    case ExtensionsGL::COMPRESSED_RGBA_ASTC_8x8_KHR:
    case ExtensionsGL::COMPRESSED_RGBA_ASTC_10x5_KHR:
    case ExtensionsGL::COMPRESSED_RGBA_ASTC_10x6_KHR:
    case ExtensionsGL::COMPRESSED_RGBA_ASTC_10x8_KHR:
    case ExtensionsGL::COMPRESSED_RGBA_ASTC_10x10_KHR:
    case ExtensionsGL::COMPRESSED_RGBA_ASTC_12x10_KHR:
    case ExtensionsGL::COMPRESSED_RGBA_ASTC_12x12_KHR:
        bytesRequired = calculateBytesForASTC(width, height, ASTCParameters[format - ASTCEnumStartRGBA]);
        break;
    case ExtensionsGL::COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR:
    case ExtensionsGL::COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR:
    case ExtensionsGL::COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR:
    case ExtensionsGL::COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR:
    case ExtensionsGL::COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR:
    case ExtensionsGL::COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR:
    case ExtensionsGL::COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR:
    case ExtensionsGL::COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR:
    case ExtensionsGL::COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR:
    case ExtensionsGL::COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR:
    case ExtensionsGL::COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR:
    case ExtensionsGL::COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR:
    case ExtensionsGL::COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR:
    case ExtensionsGL::COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR:
        bytesRequired = calculateBytesForASTC(width, height, ASTCParameters[format - ASTCEnumStartSRGB8]);
        break;
    case ExtensionsGL::COMPRESSED_R11_EAC:
    case ExtensionsGL::COMPRESSED_SIGNED_R11_EAC:
    case ExtensionsGL::COMPRESSED_RGB8_ETC2:
    case ExtensionsGL::COMPRESSED_SRGB8_ETC2:
    case ExtensionsGL::COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2:
    case ExtensionsGL::COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2: {
        Checked<unsigned, RecordOverflow> checkedBytesRequired = (width + kEACAndETC2BlockSize - 1) / kEACAndETC2BlockSize;
        checkedBytesRequired *= (height + kEACAndETC2BlockSize - 1) / kEACAndETC2BlockSize;
        checkedBytesRequired *= 8;
        if (checkedBytesRequired.hasOverflowed()) {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "too large dimensions");
            return false;
        }
        bytesRequired = checkedBytesRequired.unsafeGet();
        break;
    }
    case ExtensionsGL::COMPRESSED_RG11_EAC:
    case ExtensionsGL::COMPRESSED_SIGNED_RG11_EAC:
    case ExtensionsGL::COMPRESSED_RGBA8_ETC2_EAC:
    case ExtensionsGL::COMPRESSED_SRGB8_ALPHA8_ETC2_EAC: {
        Checked<unsigned, RecordOverflow> checkedBytesRequired = (width + kEACAndETC2BlockSize - 1) / kEACAndETC2BlockSize;
        checkedBytesRequired *= (height + kEACAndETC2BlockSize - 1) / kEACAndETC2BlockSize;
        checkedBytesRequired *= 16;
        if (checkedBytesRequired.hasOverflowed()) {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "too large dimensions");
            return false;
        }
        bytesRequired = checkedBytesRequired.unsafeGet();
        break;
    }
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid format");
        return false;
    }

    if (pixels.byteLength() != bytesRequired) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "length of ArrayBufferView is not correct for dimensions");
        return false;
    }

    return true;
}

bool WebGLRenderingContextBase::validateCompressedTexDimensions(const char* functionName, GCGLenum target, GCGLint level, GCGLsizei width, GCGLsizei height, GCGLenum format)
{
    switch (format) {
    case ExtensionsGL::COMPRESSED_RGB_S3TC_DXT1_EXT:
    case ExtensionsGL::COMPRESSED_RGBA_S3TC_DXT1_EXT:
    case ExtensionsGL::COMPRESSED_RGBA_S3TC_DXT3_EXT:
    case ExtensionsGL::COMPRESSED_RGBA_S3TC_DXT5_EXT: {
        const GCGLsizei kBlockWidth = 4;
        const GCGLsizei kBlockHeight = 4;
        const GCGLint maxTextureSize = target ? m_maxTextureSize : m_maxCubeMapTextureSize;
        const GCGLsizei maxCompressedDimension = maxTextureSize >> level;
        bool widthValid = (level && width == 1) || (level && width == 2) || (!(width % kBlockWidth) && width <= maxCompressedDimension);
        bool heightValid = (level && height == 1) || (level && height == 2) || (!(height % kBlockHeight) && height <= maxCompressedDimension);
        if (!widthValid || !heightValid) {
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "width or height invalid for level");
            return false;
        }
        return true;
    }
    case ExtensionsGL::COMPRESSED_RGB_PVRTC_4BPPV1_IMG:
    case ExtensionsGL::COMPRESSED_RGB_PVRTC_2BPPV1_IMG:
    case ExtensionsGL::COMPRESSED_RGBA_PVRTC_4BPPV1_IMG:
    case ExtensionsGL::COMPRESSED_RGBA_PVRTC_2BPPV1_IMG:
        // Height and width must be powers of 2.
        if ((width & (width - 1)) || (height & (height - 1))) {
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "width or height invalid for level");
            return false;
        }
        return true;
    case ExtensionsGL::COMPRESSED_RGBA_ASTC_4x4_KHR:
    case ExtensionsGL::COMPRESSED_RGBA_ASTC_5x4_KHR:
    case ExtensionsGL::COMPRESSED_RGBA_ASTC_5x5_KHR:
    case ExtensionsGL::COMPRESSED_RGBA_ASTC_6x5_KHR:
    case ExtensionsGL::COMPRESSED_RGBA_ASTC_6x6_KHR:
    case ExtensionsGL::COMPRESSED_RGBA_ASTC_8x5_KHR:
    case ExtensionsGL::COMPRESSED_RGBA_ASTC_8x6_KHR:
    case ExtensionsGL::COMPRESSED_RGBA_ASTC_8x8_KHR:
    case ExtensionsGL::COMPRESSED_RGBA_ASTC_10x5_KHR:
    case ExtensionsGL::COMPRESSED_RGBA_ASTC_10x6_KHR:
    case ExtensionsGL::COMPRESSED_RGBA_ASTC_10x8_KHR:
    case ExtensionsGL::COMPRESSED_RGBA_ASTC_10x10_KHR:
    case ExtensionsGL::COMPRESSED_RGBA_ASTC_12x10_KHR:
    case ExtensionsGL::COMPRESSED_RGBA_ASTC_12x12_KHR:
    case ExtensionsGL::COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR:
    case ExtensionsGL::COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR:
    case ExtensionsGL::COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR:
    case ExtensionsGL::COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR:
    case ExtensionsGL::COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR:
    case ExtensionsGL::COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR:
    case ExtensionsGL::COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR:
    case ExtensionsGL::COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR:
    case ExtensionsGL::COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR:
    case ExtensionsGL::COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR:
    case ExtensionsGL::COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR:
    case ExtensionsGL::COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR:
    case ExtensionsGL::COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR:
    case ExtensionsGL::COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR:
    case ExtensionsGL::ETC1_RGB8_OES:
    case ExtensionsGL::COMPRESSED_R11_EAC:
    case ExtensionsGL::COMPRESSED_SIGNED_R11_EAC:
    case ExtensionsGL::COMPRESSED_RG11_EAC:
    case ExtensionsGL::COMPRESSED_SIGNED_RG11_EAC:
    case ExtensionsGL::COMPRESSED_RGB8_ETC2:
    case ExtensionsGL::COMPRESSED_SRGB8_ETC2:
    case ExtensionsGL::COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2:
    case ExtensionsGL::COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2:
    case ExtensionsGL::COMPRESSED_RGBA8_ETC2_EAC:
    case ExtensionsGL::COMPRESSED_SRGB8_ALPHA8_ETC2_EAC:
        // No height and width restrictions on ASTC, ETC1 or ETC2.
        return true;
    default:
        return false;
    }
}

bool WebGLRenderingContextBase::validateCompressedTexSubDimensions(const char* functionName, GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, WebGLTexture* tex)
{
    if (xoffset < 0 || yoffset < 0) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "xoffset or yoffset < 0");
        return false;
    }

    switch (format) {
    case ExtensionsGL::COMPRESSED_RGB_S3TC_DXT1_EXT:
    case ExtensionsGL::COMPRESSED_RGBA_S3TC_DXT1_EXT:
    case ExtensionsGL::COMPRESSED_RGBA_S3TC_DXT3_EXT:
    case ExtensionsGL::COMPRESSED_RGBA_S3TC_DXT5_EXT: {
        const int kBlockWidth = 4;
        const int kBlockHeight = 4;
        if ((xoffset % kBlockWidth) || (yoffset % kBlockHeight)) {
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "xoffset or yoffset not multiple of 4");
            return false;
        }
        if (width - xoffset > tex->getWidth(target, level)
            || height - yoffset > tex->getHeight(target, level)) {
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "dimensions out of range");
            return false;
        }
        return validateCompressedTexDimensions(functionName, target, level, width, height, format);
    }
    case ExtensionsGL::COMPRESSED_RGB_PVRTC_4BPPV1_IMG:
    case ExtensionsGL::COMPRESSED_RGB_PVRTC_2BPPV1_IMG:
    case ExtensionsGL::COMPRESSED_RGBA_PVRTC_4BPPV1_IMG:
    case ExtensionsGL::COMPRESSED_RGBA_PVRTC_2BPPV1_IMG: {
        if (xoffset || yoffset) {
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "xoffset and yoffset must be zero");
            return false;
        }
        if (width != tex->getWidth(target, level)
            || height != tex->getHeight(target, level)) {
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "dimensions must match existing level");
            return false;
        }
        return validateCompressedTexDimensions(functionName, target, level, width, height, format);
    }
    case ExtensionsGL::ETC1_RGB8_OES:
        // Not supported for ETC1_RGB8_OES textures.
        return false;
    case ExtensionsGL::COMPRESSED_R11_EAC:
    case ExtensionsGL::COMPRESSED_SIGNED_R11_EAC:
    case ExtensionsGL::COMPRESSED_RG11_EAC:
    case ExtensionsGL::COMPRESSED_SIGNED_RG11_EAC:
    case ExtensionsGL::COMPRESSED_RGB8_ETC2:
    case ExtensionsGL::COMPRESSED_SRGB8_ETC2:
    case ExtensionsGL::COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2:
    case ExtensionsGL::COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2:
    case ExtensionsGL::COMPRESSED_RGBA8_ETC2_EAC:
    case ExtensionsGL::COMPRESSED_SRGB8_ALPHA8_ETC2_EAC: {
        if (target == GraphicsContextGL::TEXTURE_3D) {
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "not supported on TEXTURE_3D textures");
            return false;
        }
        const int kBlockSize = 4;
        int texWidth = tex->getWidth(target, level);
        int texHeight = tex->getHeight(target, level);
        if ((xoffset % kBlockSize) || (yoffset % kBlockSize)
            || ((width % kBlockSize) && xoffset + width != texWidth)
            || ((height % kBlockSize) && yoffset + height != texHeight)) {
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "dimensions must match existing texture level dimensions");
            return false;
        }
        return true;
    }
    default:
        return false;
    }
}
#endif // !USE(ANGLE)

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
        printToConsole(MessageLevel::Warning, "WebGL: too many errors, no more errors will be reported to the console for this context.");
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
            && attachment >= GraphicsContextGL::COLOR_ATTACHMENT0
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

bool WebGLRenderingContextBase::validateUniformParameters(const char* functionName, const WebGLUniformLocation* location, const Float32List& v, GCGLsizei requiredMinSize, GCGLuint srcOffset, GCGLuint srcLength)
{
    return validateUniformMatrixParameters(functionName, location, false, v.data(), v.length(), requiredMinSize, srcOffset, srcLength);
}

bool WebGLRenderingContextBase::validateUniformParameters(const char* functionName, const WebGLUniformLocation* location, const Int32List& v, GCGLsizei requiredMinSize, GCGLuint srcOffset, GCGLuint srcLength)
{
    return validateUniformMatrixParameters(functionName, location, false, v.data(), v.length(), requiredMinSize, srcOffset, srcLength);
}

bool WebGLRenderingContextBase::validateUniformParameters(const char* functionName, const WebGLUniformLocation* location, const Uint32List& v, GCGLsizei requiredMinSize, GCGLuint srcOffset, GCGLuint srcLength)
{
    return validateUniformMatrixParameters(functionName, location, false, v.data(), v.length(), requiredMinSize, srcOffset, srcLength);
}

bool WebGLRenderingContextBase::validateUniformParameters(const char* functionName, const WebGLUniformLocation* location, void* v, GCGLsizei size, GCGLsizei requiredMinSize, GCGLuint srcOffset, GCGLuint srcLength)
{
    return validateUniformMatrixParameters(functionName, location, false, v, size, requiredMinSize, srcOffset, srcLength);
}

bool WebGLRenderingContextBase::validateUniformMatrixParameters(const char* functionName, const WebGLUniformLocation* location, GCGLboolean transpose, const Float32List& v, GCGLsizei requiredMinSize, GCGLuint srcOffset, GCGLuint srcLength)
{
    return validateUniformMatrixParameters(functionName, location, transpose, v.data(), v.length(), requiredMinSize, srcOffset, srcLength);
}

bool WebGLRenderingContextBase::validateUniformMatrixParameters(const char* functionName, const WebGLUniformLocation* location, GCGLboolean transpose, const void* v, GCGLsizei size, GCGLsizei requiredMinSize, GCGLuint srcOffset, GCGLuint srcLength)
{
    if (!validateUniformLocation(functionName, location))
        return false;
    if (!v) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "no array");
        return false;
    }
    if (transpose && !isWebGL2()) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "transpose not FALSE");
        return false;
    }
    if (srcOffset >= static_cast<GCGLuint>(size)) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "invalid srcOffset");
        return false;
    }
    GCGLsizei actualSize = size - srcOffset;
    if (srcLength > 0) {
        if (srcLength > static_cast<GCGLuint>(actualSize)) {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "invalid srcOffset + srcLength");
            return false;
        }
        actualSize = srcLength;
    }
    if (actualSize < requiredMinSize || (actualSize % requiredMinSize)) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "invalid size");
        return false;
    }
    return true;
}

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
    if (wouldTaintOrigin(image))
        return Exception { SecurityError };
    return true;
}

ExceptionOr<bool> WebGLRenderingContextBase::validateHTMLCanvasElement(const char* functionName, HTMLCanvasElement* canvas)
{
    if (!canvas || !canvas->buffer()) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "no canvas");
        return false;
    }
    if (wouldTaintOrigin(canvas))
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
    if (wouldTaintOrigin(video))
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
    if (isContextLostOrPending())
        return;
    if (index >= m_maxVertexAttribs) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "index out of range");
        return;
    }
#if !USE(ANGLE)
    // In GL, we skip setting vertexAttrib0 values.
    if (index || isGLES2Compliant())
#endif
    {
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
    if (isContextLostOrPending())
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
#if !USE(ANGLE)
    // In GL, we skip setting vertexAttrib0 values.
    if (index || isGLES2Compliant())
#endif
    {
        switch (expectedSize) {
        case 1:
            m_context->vertexAttrib1fv(index, data);
            break;
        case 2:
            m_context->vertexAttrib2fv(index, data);
            break;
        case 3:
            m_context->vertexAttrib3fv(index, data);
            break;
        case 4:
            m_context->vertexAttrib4fv(index, data);
            break;
        }
    }
    VertexAttribValue& attribValue = m_vertexAttribValue[index];
    attribValue.initValue();
    for (int ii = 0; ii < expectedSize; ++ii)
        attribValue.fValue[ii] = data[ii];
}

#if !USE(ANGLE)
void WebGLRenderingContextBase::initVertexAttrib0()
{
    WebGLVertexArrayObjectBase::VertexAttribState& state = m_boundVertexArrayObject->getVertexAttribState(0);
    
    m_vertexAttrib0Buffer = createBuffer();
    m_context->bindBuffer(GraphicsContextGL::ARRAY_BUFFER, m_vertexAttrib0Buffer->object());
    m_context->bufferData(GraphicsContextGL::ARRAY_BUFFER, 0, GraphicsContextGL::DYNAMIC_DRAW);
    m_context->vertexAttribPointer(0, 4, GraphicsContextGL::FLOAT, false, 0, 0);
    state.bufferBinding = m_vertexAttrib0Buffer;
    m_context->bindBuffer(GraphicsContextGL::ARRAY_BUFFER, 0);
    m_context->enableVertexAttribArray(0);
    m_vertexAttrib0BufferSize = 0;
    m_vertexAttrib0BufferValue[0] = 0.0f;
    m_vertexAttrib0BufferValue[1] = 0.0f;
    m_vertexAttrib0BufferValue[2] = 0.0f;
    m_vertexAttrib0BufferValue[3] = 1.0f;
    m_forceAttrib0BufferRefill = false;
    m_vertexAttrib0UsedBefore = false;
}

bool WebGLRenderingContextBase::validateSimulatedVertexAttrib0(GCGLuint numVertex)
{
    if (!m_currentProgram)
        return true;

    bool usingVertexAttrib0 = m_currentProgram->isUsingVertexAttrib0();
    if (!usingVertexAttrib0)
        return true;

    auto& state = m_boundVertexArrayObject->getVertexAttribState(0);
    if (state.enabled)
        return true;

    auto bufferSize = checkedAddAndMultiply<GCGLuint>(numVertex, 1, 4);
    if (!bufferSize)
        return false;

    Checked<GCGLsizeiptr, RecordOverflow> bufferDataSize(bufferSize.value());
    bufferDataSize *= Checked<GCGLsizeiptr>(sizeof(GCGLfloat));
    return !bufferDataSize.hasOverflowed() && bufferDataSize.unsafeGet() > 0;
}

Optional<bool> WebGLRenderingContextBase::simulateVertexAttrib0(GCGLuint numVertex)
{
    if (!m_currentProgram)
        return false;
    bool usingVertexAttrib0 = m_currentProgram->isUsingVertexAttrib0();
    if (usingVertexAttrib0)
        m_vertexAttrib0UsedBefore = true;

    auto& state = m_boundVertexArrayObject->getVertexAttribState(0);
    if (state.enabled && usingVertexAttrib0)
        return false;
    if (!usingVertexAttrib0 && !m_vertexAttrib0UsedBefore)
        return false;
    m_vertexAttrib0UsedBefore = true;
    m_context->bindBuffer(GraphicsContextGL::ARRAY_BUFFER, m_vertexAttrib0Buffer->object());

    // We know bufferSize and bufferDataSize won't overflow or go negative, thanks to validateSimulatedVertexAttrib0
    GCGLuint bufferSize = (numVertex + 1) * 4;
    GCGLsizeiptr bufferDataSize = bufferSize * sizeof(GCGLfloat);

    if (bufferDataSize > m_vertexAttrib0BufferSize) {
        m_context->moveErrorsToSyntheticErrorList();
        m_context->bufferData(GraphicsContextGL::ARRAY_BUFFER, bufferDataSize, 0, GraphicsContextGL::DYNAMIC_DRAW);
        if (m_context->getError() != GraphicsContextGL::NO_ERROR) {
            // We were unable to create a buffer.
            m_vertexAttrib0UsedBefore = false;
            m_vertexAttrib0BufferSize = 0;
            m_forceAttrib0BufferRefill = true;
            return WTF::nullopt;
        }
        m_vertexAttrib0BufferSize = bufferDataSize;
        m_forceAttrib0BufferRefill = true;
    }

    auto& attribValue = m_vertexAttribValue[0];
    // This code shouldn't be called with WebGL2 where the type can be non-float.
    ASSERT(attribValue.type == GraphicsContextGL::FLOAT);

    if (usingVertexAttrib0
        && (m_forceAttrib0BufferRefill
            || attribValue.fValue[0] != m_vertexAttrib0BufferValue[0]
            || attribValue.fValue[1] != m_vertexAttrib0BufferValue[1]
            || attribValue.fValue[2] != m_vertexAttrib0BufferValue[2]
            || attribValue.fValue[3] != m_vertexAttrib0BufferValue[3])) {

        auto bufferData = makeUniqueArray<GCGLfloat>(bufferSize);
        for (GCGLuint ii = 0; ii < numVertex + 1; ++ii) {
            bufferData[ii * 4] = attribValue.fValue[0];
            bufferData[ii * 4 + 1] = attribValue.fValue[1];
            bufferData[ii * 4 + 2] = attribValue.fValue[2];
            bufferData[ii * 4 + 3] = attribValue.fValue[3];
        }
        m_vertexAttrib0BufferValue[0] = attribValue.fValue[0];
        m_vertexAttrib0BufferValue[1] = attribValue.fValue[1];
        m_vertexAttrib0BufferValue[2] = attribValue.fValue[2];
        m_vertexAttrib0BufferValue[3] = attribValue.fValue[3];
        m_forceAttrib0BufferRefill = false;
        m_context->bufferSubData(GraphicsContextGL::ARRAY_BUFFER, 0, bufferDataSize, bufferData.get());
    }
    m_context->vertexAttribPointer(0, 4, GraphicsContextGL::FLOAT, 0, 0, 0);
    return true;
}

void WebGLRenderingContextBase::restoreStatesAfterVertexAttrib0Simulation()
{
    const WebGLVertexArrayObjectBase::VertexAttribState& state = m_boundVertexArrayObject->getVertexAttribState(0);
    if (state.bufferBinding != m_vertexAttrib0Buffer) {
        m_context->bindBuffer(GraphicsContextGL::ARRAY_BUFFER, objectOrZero(state.bufferBinding.get()));
        m_context->vertexAttribPointer(0, state.size, state.type, state.normalized, state.originalStride, state.offset);
    }
    m_context->bindBuffer(GraphicsContextGL::ARRAY_BUFFER, objectOrZero(m_boundArrayBuffer.get()));
}
#endif

void WebGLRenderingContextBase::scheduleTaskToDispatchContextLostEvent()
{
    auto* canvas = htmlCanvas();
    if (!canvas)
        return;

    // It is safe to capture |this| because we keep the canvas element alive and it owns |this|.
    queueTaskKeepingObjectAlive(*canvas, TaskSource::WebGL, [this, canvas] {
        auto event = WebGLContextEvent::create(eventNames().webglcontextlostEvent, Event::CanBubble::No, Event::IsCancelable::Yes, emptyString());
        canvas->dispatchEvent(event);
        m_restoreAllowed = event->defaultPrevented();
        if (m_contextLostMode == RealLostContext && m_restoreAllowed)
            m_restoreTimer.startOneShot(0_s);
    });
}

void WebGLRenderingContextBase::maybeRestoreContext()
{
    RELEASE_ASSERT(!m_isSuspended);
    ASSERT(m_contextLost);
    if (!m_contextLost)
        return;

    // The rendering context is not restored unless the default behavior of the
    // webglcontextlost event was prevented earlier.
    //
    // Because of the way m_restoreTimer is set up for real vs. synthetic lost
    // context events, we don't have to worry about this test short-circuiting
    // the retry loop for real context lost events.
    if (!m_restoreAllowed)
        return;

    int contextLostReason = m_context->getExtensions().getGraphicsResetStatusARB();

    switch (contextLostReason) {
    case GraphicsContextGL::NO_ERROR:
        // The GraphicsContextGLOpenGL implementation might not fully
        // support GL_ARB_robustness semantics yet. Alternatively, the
        // WEBGL_lose_context extension might have been used to force
        // a lost context.
        break;
    case ExtensionsGL::GUILTY_CONTEXT_RESET_ARB:
        // The rendering context is not restored if this context was
        // guilty of causing the graphics reset.
        printToConsole(MessageLevel::Warning, "WARNING: WebGL content on the page caused the graphics card to reset; not restoring the context");
        return;
    case ExtensionsGL::INNOCENT_CONTEXT_RESET_ARB:
        // Always allow the context to be restored.
        break;
    case ExtensionsGL::UNKNOWN_CONTEXT_RESET_ARB:
        // Warn. Ideally, prompt the user telling them that WebGL
        // content on the page might have caused the graphics card to
        // reset and ask them whether they want to continue running
        // the content. Only if they say "yes" should we start
        // attempting to restore the context.
        printToConsole(MessageLevel::Warning, "WARNING: WebGL content on the page might have caused the graphics card to reset");
        break;
    }

    auto* canvas = htmlCanvas();
    if (!canvas)
        return;

    RefPtr<Frame> frame = canvas->document().frame();
    if (!frame)
        return;

    if (!frame->loader().client().allowWebGL(frame->settings().webGLEnabled()))
        return;

    RefPtr<FrameView> view = frame->view();
    if (!view)
        return;
    RefPtr<ScrollView> root = view->root();
    if (!root)
        return;
    HostWindow* hostWindow = root->hostWindow();
    if (!hostWindow)
        return;

    RefPtr<GraphicsContextGLOpenGL> context(GraphicsContextGLOpenGL::create(m_attributes, hostWindow));
    if (!context) {
        if (m_contextLostMode == RealLostContext)
            m_restoreTimer.startOneShot(secondsBetweenRestoreAttempts);
        else
            // This likely shouldn't happen but is the best way to report it to the WebGL app.
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "", "error restoring context");
        return;
    }

    m_context = context;
    addActivityStateChangeObserverIfNecessary();
    m_contextLost = false;
    setupFlags();
    initializeNewContext();
    canvas->dispatchEvent(WebGLContextEvent::create(eventNames().webglcontextrestoredEvent, Event::CanBubble::No, Event::IsCancelable::Yes, emptyString()));
}

void WebGLRenderingContextBase::simulateContextChanged()
{
    if (m_context)
        m_context->simulateContextChanged();
}

String WebGLRenderingContextBase::ensureNotNull(const String& text) const
{
    if (text.isNull())
        return WTF::emptyString();
    return text;
}

WebGLRenderingContextBase::LRUImageBufferCache::LRUImageBufferCache(int capacity)
    : m_buffers(capacity)
{
}

ImageBuffer* WebGLRenderingContextBase::LRUImageBufferCache::imageBuffer(const IntSize& size)
{
    size_t i;
    for (i = 0; i < m_buffers.size(); ++i) {
        ImageBuffer* buf = m_buffers[i].get();
        if (!buf)
            break;
        if (buf->logicalSize() != size)
            continue;
        bubbleToFront(i);
        buf->context().clearRect(FloatRect({ }, FloatSize(size)));
        return buf;
    }

    // FIXME (149423): Should this ImageBuffer be unconditionally unaccelerated?
    std::unique_ptr<ImageBuffer> temp = ImageBuffer::create(size, RenderingMode::Unaccelerated);
    if (!temp)
        return nullptr;
    ASSERT(m_buffers.size() > 0);
    i = std::min(m_buffers.size() - 1, i);
    m_buffers[i] = WTFMove(temp);

    ImageBuffer* buf = m_buffers[i].get();
    bubbleToFront(i);
    return buf;
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

void WebGLRenderingContextBase::synthesizeGLError(GCGLenum error, const char* functionName, const char* description, ConsoleDisplayPreference display)
{
    if (m_synthesizedErrorsToConsole && display == DisplayInConsole) {
        String str = "WebGL: " + GetErrorString(error) +  ": " + String(functionName) + ": " + String(description);
        printToConsole(MessageLevel::Error, str);
    }
    m_context->synthesizeGLError(error);
}

void WebGLRenderingContextBase::applyStencilTest()
{
    bool haveStencilBuffer = false;

    if (m_framebufferBinding)
        haveStencilBuffer = m_framebufferBinding->hasStencilBuffer();
    else {
        auto attributes = getContextAttributes();
        ASSERT(attributes);
        haveStencilBuffer = attributes->stencil;
    }
    enableOrDisable(GraphicsContextGL::STENCIL_TEST, m_stencilEnabled && haveStencilBuffer);
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
        m_context->getIntegerv(ExtensionsGL::MAX_DRAW_BUFFERS_EXT, &m_maxDrawBuffers);
    if (!m_maxColorAttachments)
        m_context->getIntegerv(ExtensionsGL::MAX_COLOR_ATTACHMENTS_EXT, &m_maxColorAttachments);
    // WEBGL_draw_buffers requires MAX_COLOR_ATTACHMENTS >= MAX_DRAW_BUFFERS.
    return std::min(m_maxDrawBuffers, m_maxColorAttachments);
}

GCGLint WebGLRenderingContextBase::getMaxColorAttachments()
{
    if (!supportsDrawBuffers())
        return 0;
    if (!m_maxColorAttachments)
        m_context->getIntegerv(ExtensionsGL::MAX_COLOR_ATTACHMENTS_EXT, &m_maxColorAttachments);
    return m_maxColorAttachments;
}

void WebGLRenderingContextBase::setBackDrawBuffer(GCGLenum buf)
{
    m_backDrawBuffer = buf;
}

void WebGLRenderingContextBase::setFramebuffer(GCGLenum target, WebGLFramebuffer* buffer)
{
    if (buffer)
        buffer->setHasEverBeenBound();

    if (target == GraphicsContextGL::FRAMEBUFFER || target == GraphicsContextGL::DRAW_FRAMEBUFFER) {
        m_framebufferBinding = buffer;
        applyStencilTest();
    }
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
#if !USE(ANGLE)
    if (texture && texture->needToUseBlackTexture(textureExtensionFlags()))
        m_unrenderableTextureUnits.add(m_activeTextureUnit);
#endif // !USE(ANGLE)
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
#if USE(ANGLE)
    if (isContextLostOrPending())
        return;
#endif // USE(ANGLE)

    if (!primcount) {
        markContextChanged();
        return;
    }

#if !USE(ANGLE)
    if (!validateDrawArrays("drawArraysInstanced", mode, first, count, primcount))
        return;
#endif // !USE(ANGLE)

    clearIfComposited();

#if !USE(ANGLE)
    bool vertexAttrib0Simulated = false;
    if (!isGLES2Compliant()) {
        auto simulateVertexAttrib0Status = simulateVertexAttrib0(first + count - 1);
        if (!simulateVertexAttrib0Status) {
            // We were unable to simulate the attribute buffer.
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "drawArraysInstanced", "unable to simulate vertexAttrib0 array");
            return;
        }
        vertexAttrib0Simulated = simulateVertexAttrib0Status.value();
    }
    if (!isGLES2NPOTStrict())
        checkTextureCompleteness("drawArraysInstanced", true);
#endif

    m_context->drawArraysInstanced(mode, first, count, primcount);

#if !USE(ANGLE)
    if (!isGLES2Compliant() && vertexAttrib0Simulated)
        restoreStatesAfterVertexAttrib0Simulation();
    if (!isGLES2NPOTStrict())
        checkTextureCompleteness("drawArraysInstanced", false);
#endif
    markContextChangedAndNotifyCanvasObserver();
}

void WebGLRenderingContextBase::drawElementsInstanced(GCGLenum mode, GCGLsizei count, GCGLenum type, long long offset, GCGLsizei primcount)
{
#if USE(ANGLE)
    if (isContextLostOrPending())
        return;
#endif

    if (!primcount) {
        markContextChanged();
        return;
    }

#if !USE(ANGLE)
    unsigned numElements = 0;
    if (!validateDrawElements("drawElementsInstanced", mode, count, type, offset, numElements, primcount))
        return;
#endif // !USE(ANGLE)

    clearIfComposited();

#if !USE(ANGLE)
    bool vertexAttrib0Simulated = false;
    if (!isGLES2Compliant()) {
        if (!numElements)
            validateIndexArrayPrecise(count, type, static_cast<GCGLintptr>(offset), numElements);
        auto simulateVertexAttrib0Status = simulateVertexAttrib0(numElements);
        if (!simulateVertexAttrib0Status) {
            // We were unable to simulate the attribute buffer.
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "drawArraysInstanced", "unable to simulate vertexAttrib0 array");
            return;
        }
        vertexAttrib0Simulated = simulateVertexAttrib0Status.value();
    }
    if (!isGLES2NPOTStrict())
        checkTextureCompleteness("drawElementsInstanced", true);
#endif

#if USE(OPENGL) && ENABLE(WEBGL2)
    if (isWebGL2())
        m_context->primitiveRestartIndex(getRestartIndex(type));
#endif

    m_context->drawElementsInstanced(mode, count, type, static_cast<GCGLintptr>(offset), primcount);

#if !USE(ANGLE)
    if (!isGLES2Compliant() && vertexAttrib0Simulated)
        restoreStatesAfterVertexAttrib0Simulation();
    if (!isGLES2NPOTStrict())
        checkTextureCompleteness("drawElementsInstanced", false);
#endif
    markContextChangedAndNotifyCanvasObserver();
}

void WebGLRenderingContextBase::vertexAttribDivisor(GCGLuint index, GCGLuint divisor)
{
    if (isContextLostOrPending())
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
    auto& extensions = m_context->getExtensions();
    String extensionName { extensionNameLiteral };
    if (!extensions.supports(extensionName))
        return false;
    extensions.ensureEnabled(extensionName);
    return true;
}

void WebGLRenderingContextBase::activityStateDidChange(OptionSet<ActivityState::Flag> oldActivityState, OptionSet<ActivityState::Flag> newActivityState)
{
    if (!m_context)
        return;

    auto changed = oldActivityState ^ newActivityState;
    if (changed & ActivityState::IsVisible)
        m_context->setContextVisibility(newActivityState.contains(ActivityState::IsVisible));
}

void WebGLRenderingContextBase::setFailNextGPUStatusCheck()
{
    if (!m_context)
        return;

    m_context->setFailNextGPUStatusCheck();
}

void WebGLRenderingContextBase::didComposite()
{
    if (UNLIKELY(callTracingActive()))
        InspectorInstrumentation::didFinishRecordingCanvasFrame(*this);
}

void WebGLRenderingContextBase::forceContextLost()
{
    forceLostContext(WebGLRenderingContextBase::RealLostContext);
}

void WebGLRenderingContextBase::recycleContext()
{
    printToConsole(MessageLevel::Error, "There are too many active WebGL contexts on this page, the oldest context will be lost.");
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

void WebGLRenderingContextBase::prepareForDisplay()
{
    if (!m_context)
        return;

    m_context->prepareForDisplay();
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
