/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebGLCompressedTextures_h
#define WebGLCompressedTextures_h

#include "ExceptionCode.h"
#include "WebGLExtension.h"
#include <wtf/ArrayBufferView.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {

class WebGLTexture;

class WebGLCompressedTextures : public WebGLExtension {
public:
    static PassOwnPtr<WebGLCompressedTextures> create(WebGLRenderingContext*);

    static bool supported(WebGLRenderingContext*);

    virtual ~WebGLCompressedTextures();
    virtual ExtensionName getName() const;

    void compressedTexImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dsizei width,
                              GC3Dsizei height, GC3Dint border, ArrayBufferView* data);
    void compressedTexSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset,
                                 GC3Dsizei width, GC3Dsizei height, GC3Denum format, ArrayBufferView* data);

    WebGLGetInfo getCompressedTextureFormats();

private:
    WebGLCompressedTextures(WebGLRenderingContext*);

    bool validateCompressedTexFuncData(GC3Dsizei width, GC3Dsizei height,
                                       GC3Denum format, ArrayBufferView* pixels);

    bool validateCompressedTexFormat(GC3Denum format);

    bool validateCompressedTexSubDimensions(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset,
                                            GC3Dsizei width, GC3Dsizei height, GC3Denum format, WebGLTexture*);

    bool m_supportsDxt1;
    bool m_supportsDxt5;
    bool m_supportsEtc1;
    bool m_supportsPvrtc;

    Vector<int> m_formats;
};

} // namespace WebCore

#endif // WebGLCompressedTextures_h
