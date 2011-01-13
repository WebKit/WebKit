/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2010 Stephan Aßmus, <superstippi@gmx.de>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "ImageDecoder.h"

#include <Bitmap.h>

namespace WebCore {

NativeImagePtr ImageFrame::asNewNativeImage() const
{
    int bytesPerRow = width() * sizeof(PixelData);
    OwnPtr<BBitmap> bitmap(new BBitmap(BRect(0, 0, width() - 1, height() - 1), 0, B_RGBA32, bytesPerRow));

    const uint8* source = reinterpret_cast<const uint8*>(m_bytes);
    uint8* destination = reinterpret_cast<uint8*>(bitmap->Bits());
    int h = height();
    int w = width();
    for (int y = 0; y < h; y++) {
#if 0
// FIXME: Enable this conversion once Haiku has B_RGBA32P[remultiplied]...
        memcpy(dst, source, bytesPerRow);
#else
        const uint8* sourceHandle = source;
        uint8* destinationHandle = destination;
        for (int x = 0; x < w; x++) {
            if (sourceHandle[3] == 255 || !sourceHandle[3]) {
                destinationHandle[0] = sourceHandle[0];
                destinationHandle[1] = sourceHandle[1];
                destinationHandle[2] = sourceHandle[2];
                destinationHandle[3] = sourceHandle[3];
            } else {
                destinationHandle[0] = static_cast<uint16>(sourceHandle[0]) * 255 / sourceHandle[3];
                destinationHandle[1] = static_cast<uint16>(sourceHandle[1]) * 255 / sourceHandle[3];
                destinationHandle[2] = static_cast<uint16>(sourceHandle[2]) * 255 / sourceHandle[3];
                destinationHandle[3] = sourceHandle[3];
            }
            destinationHandle += 4;
            sourceHandle += 4;
        }
#endif
        destination += bytesPerRow;
        source += bytesPerRow;
    }

    return bitmap.release();
}

} // namespace WebCore

