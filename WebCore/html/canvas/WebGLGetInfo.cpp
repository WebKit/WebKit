/*
 * Copyright (C) 2009 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009 Google Inc. All Rights Reserved.
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

#if ENABLE(3D_CANVAS)

#include "WebGLGetInfo.h"
#include "WebGLBuffer.h"
#include "WebGLFloatArray.h"
#include "WebGLFramebuffer.h"
#include "WebGLIntArray.h"
#include "WebGLProgram.h"
#include "WebGLRenderbuffer.h"
#include "WebGLTexture.h"
#include "WebGLUnsignedByteArray.h"

namespace WebCore {

WebGLGetInfo::WebGLGetInfo(bool value)
    : m_type(kTypeBool)
    , m_bool(value)
{
}

WebGLGetInfo::WebGLGetInfo(float value)
    : m_type(kTypeFloat)
    , m_float(value)
{
}

WebGLGetInfo::WebGLGetInfo(long value)
    : m_type(kTypeLong)
    , m_long(value)
{
}

WebGLGetInfo::WebGLGetInfo()
    : m_type(kTypeNull)
{
}

WebGLGetInfo::WebGLGetInfo(const String& value)
    : m_type(kTypeString)
    , m_string(value)
{
}

WebGLGetInfo::WebGLGetInfo(unsigned long value)
    : m_type(kTypeUnsignedLong)
    , m_unsignedLong(value)
{
}

WebGLGetInfo::WebGLGetInfo(PassRefPtr<WebGLBuffer> value)
    : m_type(kTypeWebGLBuffer)
    , m_webglBuffer(value)
{
}

WebGLGetInfo::WebGLGetInfo(PassRefPtr<WebGLFloatArray> value)
    : m_type(kTypeWebGLFloatArray)
    , m_webglFloatArray(value)
{
}

WebGLGetInfo::WebGLGetInfo(PassRefPtr<WebGLFramebuffer> value)
    : m_type(kTypeWebGLFramebuffer)
    , m_webglFramebuffer(value)
{
}

WebGLGetInfo::WebGLGetInfo(PassRefPtr<WebGLIntArray> value)
    : m_type(kTypeWebGLIntArray)
    , m_webglIntArray(value)
{
}

WebGLGetInfo::WebGLGetInfo(PassRefPtr<WebGLProgram> value)
    : m_type(kTypeWebGLProgram)
    , m_webglProgram(value)
{
}

WebGLGetInfo::WebGLGetInfo(PassRefPtr<WebGLRenderbuffer> value)
    : m_type(kTypeWebGLRenderbuffer)
    , m_webglRenderbuffer(value)
{
}

WebGLGetInfo::WebGLGetInfo(PassRefPtr<WebGLTexture> value)
    : m_type(kTypeWebGLTexture)
    , m_webglTexture(value)
{
}

WebGLGetInfo::WebGLGetInfo(PassRefPtr<WebGLUnsignedByteArray> value)
    : m_type(kTypeWebGLUnsignedByteArray)
    , m_webglUnsignedByteArray(value)
{
}

WebGLGetInfo::~WebGLGetInfo()
{
}

WebGLGetInfo::Type WebGLGetInfo::getType() const
{
    return m_type;
}

bool WebGLGetInfo::getBool() const
{
    ASSERT(getType() == kTypeBool);
    return m_bool;
}

float WebGLGetInfo::getFloat() const
{
    ASSERT(getType() == kTypeFloat);
    return m_float;
}

long WebGLGetInfo::getLong() const
{
    ASSERT(getType() == kTypeLong);
    return m_long;
}

const String& WebGLGetInfo::getString() const
{
    ASSERT(getType() == kTypeString);
    return m_string;
}

unsigned long WebGLGetInfo::getUnsignedLong() const
{
    ASSERT(getType() == kTypeUnsignedLong);
    return m_unsignedLong;
}

PassRefPtr<WebGLBuffer> WebGLGetInfo::getWebGLBuffer() const
{
    ASSERT(getType() == kTypeWebGLBuffer);
    return m_webglBuffer;
}

PassRefPtr<WebGLFloatArray> WebGLGetInfo::getWebGLFloatArray() const
{
    ASSERT(getType() == kTypeWebGLFloatArray);
    return m_webglFloatArray;
}

PassRefPtr<WebGLFramebuffer> WebGLGetInfo::getWebGLFramebuffer() const
{
    ASSERT(getType() == kTypeWebGLFramebuffer);
    return m_webglFramebuffer;
}

PassRefPtr<WebGLIntArray> WebGLGetInfo::getWebGLIntArray() const
{
    ASSERT(getType() == kTypeWebGLIntArray);
    return m_webglIntArray;
}

PassRefPtr<WebGLProgram> WebGLGetInfo::getWebGLProgram() const
{
    ASSERT(getType() == kTypeWebGLProgram);
    return m_webglProgram;
}

PassRefPtr<WebGLRenderbuffer> WebGLGetInfo::getWebGLRenderbuffer() const
{
    ASSERT(getType() == kTypeWebGLRenderbuffer);
    return m_webglRenderbuffer;
}

PassRefPtr<WebGLTexture> WebGLGetInfo::getWebGLTexture() const
{
    ASSERT(getType() == kTypeWebGLTexture);
    return m_webglTexture;
}

PassRefPtr<WebGLUnsignedByteArray> WebGLGetInfo::getWebGLUnsignedByteArray() const
{
    ASSERT(getType() == kTypeWebGLUnsignedByteArray);
    return m_webglUnsignedByteArray;
}

} // namespace WebCore

#endif // ENABLE(3D_CANVAS)
