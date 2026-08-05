/*
 * Copyright (C) 2017-2023 Apple Inc. All rights reserved.
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
#include <wtf/CompletionHandler.h>
#include <wtf/Ref.h>
#include <wtf/Variant.h>
#include <wtf/WeakRef.h>

namespace WebCore {

class GPUComputePipeline;
class GPURenderPipeline;
class InspectorCanvas;

#if ENABLE(WEBGL)
class WebGLProgram;
#endif // ENABLE(WEBGL)

class InspectorShaderProgram final : public RefCounted<InspectorShaderProgram> {
public:
#if ENABLE(WEBGL)
    static Ref<InspectorShaderProgram> create(WebGLProgram&, InspectorCanvas&);
#endif // ENABLE(WEBGL)
    static Ref<InspectorShaderProgram> create(GPUComputePipeline&, InspectorCanvas&);
    static Ref<InspectorShaderProgram> create(GPURenderPipeline&, InspectorCanvas&);

    const String& identifier() const LIFETIME_BOUND { return m_identifier; }
    InspectorCanvas& canvas() const { return m_canvas; }

#if ENABLE(WEBGL)
    WebGLProgram* program() const;
#endif // ENABLE(WEBGL)
    GPUComputePipeline* computePipeline() const;
    GPURenderPipeline* renderPipeline() const;

    String requestShaderSource(Inspector::Protocol::Canvas::ShaderType);
    void updateShader(Inspector::Protocol::Canvas::ShaderType, const String& source, CompletionHandler<void(bool)>&&);

    bool disabled() const { return m_disabled; }
    bool setDisabled(bool);

    bool highlighted() const { return m_highlighted; }
    void setHighlighted(bool value) { m_highlighted = value; }

    Ref<Inspector::Protocol::Canvas::ShaderProgram> buildObjectForShaderProgram();

private:

#if ENABLE(WEBGL)
    InspectorShaderProgram(WebGLProgram&, InspectorCanvas&);
#endif // ENABLE(WEBGL)
    InspectorShaderProgram(GPUComputePipeline&, InspectorCanvas&);
    InspectorShaderProgram(GPURenderPipeline&, InspectorCanvas&);

    String m_identifier;
    WeakRef<InspectorCanvas> m_canvas;
    Variant<
#if ENABLE(WEBGL)
        WeakRef<WebGLProgram>,
#endif // ENABLE(WEBGL)
        WeakRef<GPUComputePipeline>,
        WeakRef<GPURenderPipeline>
    > m_program;
    bool m_disabled { false };
    bool m_highlighted { false };
};

} // namespace WebCore
