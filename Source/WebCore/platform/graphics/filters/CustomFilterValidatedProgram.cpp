/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER “AS IS” AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(CSS_SHADERS)

#include "CustomFilterValidatedProgram.h"

#include "ANGLEWebKitBridge.h"
#include "CustomFilterGlobalContext.h"
#include "CustomFilterProgramInfo.h"
#include "NotImplemented.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

#define SHADER(Src) (#Src) 

String CustomFilterValidatedProgram::defaultVertexShaderString()
{
    DEFINE_STATIC_LOCAL(String, vertexShaderString, (ASCIILiteral(SHADER(
        attribute mediump vec4 a_position;
        uniform mediump mat4 u_projectionMatrix;

        void main()
        {
            gl_Position = u_projectionMatrix * a_position;
        }
    ))));
    return vertexShaderString;
}

String CustomFilterValidatedProgram::defaultFragmentShaderString()
{
    DEFINE_STATIC_LOCAL(String, fragmentShaderString, (ASCIILiteral(SHADER(
        void main()
        {
        }
    ))));
    return fragmentShaderString;
}

CustomFilterValidatedProgram::CustomFilterValidatedProgram(CustomFilterGlobalContext* globalContext, const CustomFilterProgramInfo& programInfo)
    : m_globalContext(globalContext)
    , m_programInfo(programInfo)
    , m_isInitialized(false)
{
    platformInit();

    String originalVertexShader = programInfo.vertexShaderString();
    if (originalVertexShader.isNull())
        originalVertexShader = defaultVertexShaderString();

    String originalFragmentShader = programInfo.fragmentShaderString();
    if (originalFragmentShader.isNull())
        originalFragmentShader = defaultFragmentShaderString();

    // Shaders referenced from the CSS mix function use a different validator than regular WebGL shaders. See CustomFilterGlobalContext.h for more details.
    ANGLEWebKitBridge* validator = programInfo.mixSettings().enabled ? m_globalContext->mixShaderValidator() : m_globalContext->webglShaderValidator();
    String vertexShaderLog, fragmentShaderLog;
    Vector<ANGLEShaderSymbol> symbols;
    bool vertexShaderValid = validator->compileShaderSource(originalVertexShader.utf8().data(), SHADER_TYPE_VERTEX, m_validatedVertexShader, vertexShaderLog, symbols);
    bool fragmentShaderValid = validator->compileShaderSource(originalFragmentShader.utf8().data(), SHADER_TYPE_FRAGMENT, m_validatedFragmentShader, fragmentShaderLog, symbols);
    if (!vertexShaderValid || !fragmentShaderValid) {
        // FIXME: Report the validation errors.
        // https://bugs.webkit.org/show_bug.cgi?id=74416
        return;
    }

    // Validate the author's samplers.
    for (Vector<ANGLEShaderSymbol>::iterator it = symbols.begin(); it != symbols.end(); ++it) {
        if (it->isSampler()) {
            // FIXME: For now, we restrict shaders with any sampler defined.
            // When we implement texture parameters, we will allow shaders whose samplers are bound to valid textures.
            // We must not allow OpenGL to give unbound samplers a default value of 0 because that references the DOM element texture,
            // which should be inaccessible to the author's shader code.
            // https://bugs.webkit.org/show_bug.cgi?id=96230
            return;
        }
    }

    // We need to add texture access, blending, and compositing code to shaders that are referenced from the CSS mix function.
    if (programInfo.mixSettings().enabled) {
        rewriteMixVertexShader();
        rewriteMixFragmentShader();
    }

    m_isInitialized = true;
}

PassRefPtr<CustomFilterCompiledProgram> CustomFilterValidatedProgram::compiledProgram()
{
    ASSERT(m_isInitialized && m_globalContext && !m_validatedVertexShader.isNull() && !m_validatedFragmentShader.isNull());
    if (!m_compiledProgram)
        m_compiledProgram = CustomFilterCompiledProgram::create(m_globalContext->context(), m_validatedVertexShader, m_validatedFragmentShader, m_programInfo.programType());
    return m_compiledProgram;
}

void CustomFilterValidatedProgram::rewriteMixVertexShader()
{
    ASSERT(m_programInfo.mixSettings().enabled);

    // During validation, ANGLE renamed the author's "main" function to "css_main".
    // We write our own "main" function and call "css_main" from it.
    // This makes rewriting easy and ensures that our code runs after all author code.
    m_validatedVertexShader.append(SHADER(
        attribute mediump vec2 css_a_texCoord;
        varying mediump vec2 css_v_texCoord;

        void main()
        {
            css_main();
            css_v_texCoord = css_a_texCoord;
        }
    ));
}

void CustomFilterValidatedProgram::rewriteMixFragmentShader()
{
    ASSERT(m_programInfo.mixSettings().enabled);

    StringBuilder builder;
    // ANGLE considered these symbols as built-ins during validation under the SH_CSS_SHADERS_SPEC flag.
    // Now, we have to define these symbols in order to make this shader valid GLSL.
    // We define these symbols before the author's shader code, which makes them accessible to author code.
    builder.append(SHADER(
        mediump vec4 css_MixColor = vec4(0.0);
        mediump mat4 css_ColorMatrix = mat4(1.0);
    ));
    builder.append(m_validatedFragmentShader);
    builder.append(blendFunctionString(m_programInfo.mixSettings().blendMode));
    builder.append(compositeFunctionString(m_programInfo.mixSettings().compositeOperator));
    // We define symbols like "css_u_texture" after the author's shader code, which makes them inaccessible to author code.
    // In particular, "css_u_texture" represents the DOM element texture, so it's important to keep it inaccessible to
    // author code for security reasons.
    builder.append(SHADER(
        uniform sampler2D css_u_texture;
        varying mediump vec2 css_v_texCoord;

        void main()
        {
            css_main();
            mediump vec4 originalColor = texture2D(css_u_texture, css_v_texCoord);
            mediump vec4 multipliedColor = css_ColorMatrix * originalColor;
            mediump vec3 blendedColor = css_Blend(multipliedColor.rgb, css_MixColor.rgb);
            gl_FragColor = css_Composite(multipliedColor.rgb, multipliedColor.a, blendedColor.rgb, css_MixColor.a);
        }
    ));
    m_validatedFragmentShader = builder.toString();
}

String CustomFilterValidatedProgram::blendFunctionString(BlendMode blendMode)
{
    // Implemented using the same symbol names as the Compositing and Blending spec:
    // https://dvcs.w3.org/hg/FXTF/rawfile/tip/compositing/index.html#blendingnormal
    // Cs: is the source color
    // Cb: is the backdrop color
    const char* expression = 0;
    switch (blendMode) {
    case BlendModeNormal:
        expression = "Cs";
        break;
    case BlendModeMultiply:
        expression = "Cs * Cb";
        break;
    case BlendModeScreen:
        expression = "Cb + Cs - (Cb * Cs)";
        break;
    case BlendModeDarken:
        expression = "min(Cb, Cs)";
        break;
    case BlendModeLighten:
        expression = "max(Cb, Cs)";
        break;
    case BlendModeDifference:
        expression = "abs(Cb - Cs)";
        break;
    case BlendModeExclusion:
        expression = "Cb + Cs - 2.0 * Cb * Cs";
        break;
    case BlendModeOverlay:
    case BlendModeColorDodge:
    case BlendModeColorBurn:
    case BlendModeHardLight:
    case BlendModeSoftLight:
    case BlendModeHue:
    case BlendModeSaturation:
    case BlendModeColor:
    case BlendModeLuminosity:
        notImplemented();
        return String();
    }

    ASSERT(expression);
    return String::format(SHADER(
        mediump vec3 css_Blend(mediump vec3 Cb, mediump vec3 Cs)
        {
            return %s;
        }
    ), expression);
}

String CustomFilterValidatedProgram::compositeFunctionString(CompositeOperator compositeOperator)
{
    // Use the same symbol names as the Compositing and Blending spec:
    // https://dvcs.w3.org/hg/FXTF/rawfile/tip/compositing/index.html#blendingnormal
    // Cs: is the source color
    // Cb: is the backdrop color
    // as: is the source alpha
    // ab: is the backdrop alpha
    // Fa: is defined by the Porter Duff operator in use
    // Fb: is defined by the Porter Duff operator in use
    const char* Fa = 0;
    const char* Fb = 0;
    switch (compositeOperator) {
    case CompositeSourceAtop:
        Fa = "ab";
        Fb = "1.0 - as";
        break;
    case CompositeClear:
        Fa = "0.0";
        Fb = "0.0";
        break;
    case CompositeCopy:
        Fa = "1.0";
        Fb = "0.0";
        break;
    case CompositeSourceOver:
        Fa = "1.0";
        Fb = "1.0 - as";
        break;
    case CompositeSourceIn:
        Fa = "ab";
        Fb = "0.0";
        break;
    case CompositeSourceOut:
        Fa = "1.0 - ab";
        Fb = "0.0";
        break;
    case CompositeDestinationOver:
        Fa = "1.0 - ab";
        Fb = "1.0";
        break;
    case CompositeDestinationIn:
        Fa = "0.0";
        Fb = "as";
        break;
    case CompositeDestinationOut:
        Fa = "0.0";
        Fb = "1.0 - as";
        break;
    case CompositeDestinationAtop:
        Fa = "1.0 - ab";
        Fb = "as";
        break;
    case CompositeXOR:
        Fa = "1.0 - ab";
        Fb = "1.0 - as";
        break;
    case CompositePlusLighter:
        notImplemented();
        return String();
    default:
        // The CSS parser should not have accepted any other composite operators.
        ASSERT_NOT_REACHED();
        return String();
    }

    ASSERT(Fa && Fb);
    // Use the general formula for compositing, lifted from the spec:
    // https://dvcs.w3.org/hg/FXTF/rawfile/tip/compositing/index.html#generalformula
    return String::format(SHADER(
        mediump vec4 css_Composite(mediump vec3 Cb, mediump float ab, mediump vec3 Cs, mediump float as)
        {
            mediump float Fa = %s;
            mediump float Fb = %s;
            return vec4(as * Fa * Cs + ab * Fb * Cb, as * Fa + ab * Fb); 
        }
    ), Fa, Fb);
}
    
CustomFilterValidatedProgram::~CustomFilterValidatedProgram()
{
    platformDestroy();

    if (m_globalContext)
        m_globalContext->removeValidatedProgram(this);
}

#if !PLATFORM(BLACKBERRY)
void CustomFilterValidatedProgram::platformInit()
{
}

void CustomFilterValidatedProgram::platformDestroy()
{
}
#endif

} // namespace WebCore

#endif // ENABLE(CSS_SHADERS)
