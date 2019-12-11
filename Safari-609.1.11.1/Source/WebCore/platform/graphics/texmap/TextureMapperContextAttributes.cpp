/*
 * Copyright (C) 2017 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "TextureMapperContextAttributes.h"

#if USE(TEXTURE_MAPPER_GL)

#include "TextureMapperGLHeaders.h"
#include <mutex>
#include <wtf/ThreadSpecific.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

static WTF::ThreadSpecific<TextureMapperContextAttributes>& threadSpecificAttributes()
{
    static WTF::ThreadSpecific<TextureMapperContextAttributes>* s_textureMapperContextAttributes;
    static std::once_flag s_onceFlag;
    std::call_once(s_onceFlag,
        [] {
            s_textureMapperContextAttributes = new WTF::ThreadSpecific<TextureMapperContextAttributes>;
        });
    return *s_textureMapperContextAttributes;
}

const TextureMapperContextAttributes& TextureMapperContextAttributes::get()
{
    auto& attributes = *threadSpecificAttributes();
    if (!attributes.initialized) {
        attributes.initialized = true;
#if USE(OPENGL_ES)
        attributes.isGLES2Compliant = true;

        String extensionsString(reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS)));
        attributes.supportsNPOTTextures = extensionsString.contains("GL_OES_texture_npot"_s);
        attributes.supportsUnpackSubimage = extensionsString.contains("GL_EXT_unpack_subimage"_s);
#else
        attributes.isGLES2Compliant = false;
        attributes.supportsNPOTTextures = true;
        attributes.supportsUnpackSubimage = true;
#endif
    }
    return attributes;
}

} // namespace WebCore

#endif // USE(TEXTURE_MAPPER_GL)
