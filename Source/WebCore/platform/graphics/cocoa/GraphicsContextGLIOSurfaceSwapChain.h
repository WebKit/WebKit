/*
 * Copyright (C) 2009, 2014-2019 Apple Inc. All rights reserved.
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

#if ENABLE(WEBGL) && PLATFORM(COCOA)

#include "IOSurface.h"
#include <memory>

namespace WebCore {

// An interface for implementing front buffer management of a 3-buffering swap
// chain of IOSurfaces.
// The implementor will own the IOSurfaces it uses for display.
// The client may attach metadata to each IOSurface and will receive the metadata
// back once the IOSurface has been displayed. However, the client may not neccessarily
// be able to obtain the IOSurface itself for reuse.
// Example use of the metadata is to use EGLSurface binding as the metadata. This way
// when the display client is done with the IOSurface display, the client can continue using
// the existing binding obtained through the buffer recycle logic.
class WEBCORE_EXPORT GraphicsContextGLIOSurfaceSwapChain {
public:
    virtual ~GraphicsContextGLIOSurfaceSwapChain();
    struct Buffer {
        // The actual contents. Client transfers the ownership of the IOSurface.
        std::unique_ptr<WebCore::IOSurface> surface;
        // Producer specific metadata handle (such as EGLSurface). Client does not transfer the ownership.
        void* handle { nullptr };
    };
    // Returns the metadata handle of last unused contents buffer.
    // Client may recieve back also the ownership of the contents surface, in case it is available at the
    // time of the call.
    // Returns either:
    // - Empty buffer if no buffer has been submitted.
    // - Buffer with empty surface and non-empty metadata handle if the recycled buffer was available
    //   but the surface is still in use.
    // - Surface and handle.
    virtual Buffer recycleBuffer();

    // Prepares the target for display with a contents buffer.
    virtual void present(Buffer&&);

    // Returns the most recent presented display buffer. The reference is valid until
    // next present, recycleBuffer or detachClient call.
    virtual const Buffer& displayBuffer() const;

    // Detaches the client and returns the current contents buffer metadata handle.
    // The if multiple buffers have been submitted, recycleBuffer must have been called before calling
    // this.
    virtual void* detachClient();

protected:
    Buffer m_displayBuffer;
    Buffer m_spareBuffer;
};

}

#endif
