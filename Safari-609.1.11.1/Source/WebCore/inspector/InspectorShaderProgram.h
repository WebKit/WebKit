/*
 * Copyright (C) 2017 Apple Inc.  All rights reserved.
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

#include <JavaScriptCore/InspectorProtocolObjects.h>
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>
#include <wtf/Variant.h>

namespace WebCore {

class InspectorCanvas;

#if ENABLE(WEBGL)
class WebGLProgram;
class WebGLRenderingContextBase;
#endif

#if ENABLE(WEBGPU)
class WebGPUPipeline;
#endif

class InspectorShaderProgram final : public RefCounted<InspectorShaderProgram> {
public:
#if ENABLE(WEBGL)
    static Ref<InspectorShaderProgram> create(WebGLProgram&, InspectorCanvas&);
#endif
#if ENABLE(WEBGPU)
    static Ref<InspectorShaderProgram> create(WebGPUPipeline&, InspectorCanvas&);
#endif

    const String& identifier() const { return m_identifier; }
    InspectorCanvas& canvas() const { return m_canvas; }

#if ENABLE(WEBGL)
    WebGLProgram* program() const;
#endif
#if ENABLE(WEBGPU)
    WebGPUPipeline* pipeline() const;
#endif

    String requestShaderSource(Inspector::Protocol::Canvas::ShaderType);
    bool updateShader(Inspector::Protocol::Canvas::ShaderType, const String& source);

    bool disabled() const { return m_disabled; }
    void setDisabled(bool disabled) { m_disabled = disabled; }

    bool highlighted() const { return m_highlighted; }
    void setHighlighted(bool value) { m_highlighted = value; }

    Ref<Inspector::Protocol::Canvas::ShaderProgram> buildObjectForShaderProgram();

private:
#if ENABLE(WEBGL)
    InspectorShaderProgram(WebGLProgram&, InspectorCanvas&);
#endif
#if ENABLE(WEBGPU)
    InspectorShaderProgram(WebGPUPipeline&, InspectorCanvas&);
#endif

    String m_identifier;
    InspectorCanvas& m_canvas;

    Variant<
#if ENABLE(WEBGL)
        std::reference_wrapper<WebGLProgram>,
#endif
#if ENABLE(WEBGPU)
        std::reference_wrapper<WebGPUPipeline>,
#endif
        WTF::Monostate
    > m_program;

    bool m_disabled { false };
    bool m_highlighted { false };
};

} // namespace WebCore
