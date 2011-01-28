/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#ifndef ShareableSurface_h
#define ShareableSurface_h

#include <WebCore/IntSize.h>
#include <wtf/Noncopyable.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RetainPtr.h>

typedef struct _CGLContextObject* CGLContextObj;
typedef struct __IOSurface* IOSurfaceRef;

namespace CoreIPC {
    class ArgumentDecoder;
    class ArgumentEncoder;
}

namespace WebKit {

class ShareableSurface : public RefCounted<ShareableSurface> {
public:
    class Handle {
        WTF_MAKE_NONCOPYABLE(Handle);
    
    public:
        Handle();
        ~Handle();

        void encode(CoreIPC::ArgumentEncoder*) const;
        static bool decode(CoreIPC::ArgumentDecoder*, Handle&);

    private:
        friend class ShareableSurface;

        mutable mach_port_t m_port;
    };

    // Create a shareable surface with the given size. Returns 0 on failure.
    static PassRefPtr<ShareableSurface> create(CGLContextObj, const WebCore::IntSize&);

    // Create a shareable surface from a handle. Returns 0 on failure.
    static PassRefPtr<ShareableSurface> create(CGLContextObj, const Handle&);

    ~ShareableSurface();

    bool createHandle(Handle&);
    
    unsigned textureID();

    void attach();
    void detach();

private:
    ShareableSurface(CGLContextObj, const WebCore::IntSize&, IOSurfaceRef);

    // The OpenGL context.
    CGLContextObj m_cglContextObj;

    // The size of the surface.
    WebCore::IntSize m_size;
    
    // The ID of the texture.
    unsigned m_textureID;

    // The frame buffer object ID of the texture; used when the surface is used as a render destination.
    unsigned m_frameBufferObjectID;

    RetainPtr<IOSurfaceRef> m_ioSurface;
};

} // namespace WebKit

#endif // ShareableSurface_h
