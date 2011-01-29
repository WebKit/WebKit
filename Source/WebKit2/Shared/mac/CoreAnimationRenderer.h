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

#ifndef CoreAnimationRenderer_h
#define CoreAnimationRenderer_h

#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RetainPtr.h>

OBJC_CLASS CALayer;
OBJC_CLASS CARenderer;

typedef struct _CGLContextObject* CGLContextObj;

namespace WebKit {

class CoreAnimationRenderer : public RefCounted<CoreAnimationRenderer> {
public:
    class Client {
    public:
        virtual ~Client() { }
        virtual void rendererDidChange(CoreAnimationRenderer*) = 0;
    };

    static PassRefPtr<CoreAnimationRenderer> create(Client*, CGLContextObj, CALayer *);
    ~CoreAnimationRenderer();

    void invalidate();

    void setBounds(CGRect);
    void render(CFTimeInterval frameTime, CVTimeStamp*, CFTimeInterval& nextFrameTime);

private:
    CoreAnimationRenderer(Client*, CGLContextObj, CALayer *);

    static void rendererDidChange(void*);
    void rendererDidChange();

    Client* m_client;
    CGLContextObj m_cglContext;
    RetainPtr<CARenderer> m_renderer;
};    
    
} // namespace WebKit

#endif // CoreAnimationRenderer_h
