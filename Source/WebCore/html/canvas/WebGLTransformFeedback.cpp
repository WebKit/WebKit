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

#if ENABLE(WEBGL)
#include "WebGLTransformFeedback.h"

#include "WebCoreOpaqueRootInlines.h"
#include "WebGL2RenderingContext.h"
#include "WebGLBuffer.h"
#include <JavaScriptCore/AbstractSlotVisitorInlines.h>
#include <wtf/Lock.h>
#include <wtf/Locker.h>

namespace WebCore {

RefPtr<WebGLTransformFeedback> WebGLTransformFeedback::create(WebGL2RenderingContext& context)
{
    auto object = context.protectedGraphicsContextGL()->createTransformFeedback();
    if (!object)
        return nullptr;
    return adoptRef(*new WebGLTransformFeedback { context, object });
}

WebGLTransformFeedback::~WebGLTransformFeedback()
{
    if (!m_context)
        return;

    runDestructor();
}

WebGLTransformFeedback::WebGLTransformFeedback(WebGL2RenderingContext& context, PlatformGLObject object)
    : WebGLObject(context, object)
{
    m_boundIndexedTransformFeedbackBuffers.grow(context.maxTransformFeedbackSeparateAttribs());
}

void WebGLTransformFeedback::deleteObjectImpl(const AbstractLocker&, GraphicsContextGL* context3d, PlatformGLObject object)
{
    context3d->deleteTransformFeedback(object);
}

void WebGLTransformFeedback::setProgram(const AbstractLocker&, WebGLProgram& program)
{
    m_program = &program;
    m_programLinkCount = program.getLinkCount();
}

void WebGLTransformFeedback::setBoundIndexedTransformFeedbackBuffer(const AbstractLocker&, GCGLuint index, WebGLBuffer* buffer)
{
    ASSERT(index < m_boundIndexedTransformFeedbackBuffers.size());
    m_boundIndexedTransformFeedbackBuffers[index] = buffer;
}

bool WebGLTransformFeedback::getBoundIndexedTransformFeedbackBuffer(GCGLuint index, WebGLBuffer** outBuffer)
{
    if (index >= m_boundIndexedTransformFeedbackBuffers.size())
        return false;
    *outBuffer = m_boundIndexedTransformFeedbackBuffers[index].get();
    return true;
}

bool WebGLTransformFeedback::hasEnoughBuffers(GCGLuint numRequired) const
{
    if (numRequired > m_boundIndexedTransformFeedbackBuffers.size())
        return false;
    for (GCGLuint i = 0; i < numRequired; i++) {
        if (!m_boundIndexedTransformFeedbackBuffers[i].get())
            return false;
    }
    return true;
}

void WebGLTransformFeedback::addMembersToOpaqueRoots(const AbstractLocker& locker, JSC::AbstractSlotVisitor& visitor)
{
    for (auto& buffer : m_boundIndexedTransformFeedbackBuffers)
        addWebCoreOpaqueRoot(visitor, buffer.get());

    addWebCoreOpaqueRoot(visitor, m_program.get());
    if (m_program)
        m_program->addMembersToOpaqueRoots(locker, visitor);
}

void WebGLTransformFeedback::unbindBuffer(const AbstractLocker&, WebGLBuffer& buffer)
{
    for (auto& boundBuffer : m_boundIndexedTransformFeedbackBuffers) {
        if (boundBuffer == &buffer)
            boundBuffer = nullptr;
    }
}

bool WebGLTransformFeedback::validateProgramForResume(WebGLProgram* program) const
{
    return program && m_program == program && program->getLinkCount() == m_programLinkCount;
}

}

#endif // ENABLE(WEBGL)
