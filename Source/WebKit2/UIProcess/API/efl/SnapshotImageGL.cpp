/*
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
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
#include "SnapshotImageGL.h"

#if USE(ACCELERATED_COMPOSITING)
#if USE(OPENGL_ES_2)
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#else
#include "OpenGLShims.h"
#endif

PassOwnArrayPtr<unsigned char> getImageDataFromFrameBuffer(int x, int y, int width, int height)
{
    OwnArrayPtr<unsigned char> buffer = adoptArrayPtr(new unsigned char[width * height * 4]);
    glReadPixels(x, y, width, height, GL_BGRA, GL_UNSIGNED_BYTE, buffer.get());

    // Textures are flipped on the Y axis, so we need to flip the image back.
    unsigned* buf = reinterpret_cast<unsigned*>(buffer.get());

    for (int i = 0; i < height / 2; ++i) {
        for (int j = 0; j < width; ++j) {
            unsigned tmp = buf[i * width + j];
            buf[i * width + j] = buf[(height - i - 1) * width + j];
            buf[(height - i - 1) * width + j] = tmp;
        }
    }

    return buffer.release();
}

#endif
