/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef WebSharedGraphicsContext3D_h
#define WebSharedGraphicsContext3D_h

#include "WebCommon.h"

class GrContext;

namespace WebKit {

class WebGraphicsContext3D;

class WebSharedGraphicsContext3D {
public:
    // Returns a context usable from the main thread. If the context is lost or has never been created, this will attempt to create a new context.
    // The returned pointer is only valid until the next call to mainThreadContext().
    WEBKIT_EXPORT static WebGraphicsContext3D* mainThreadContext();

    // Returns a ganesh context associated with the main thread shared context. The ganesh context has the same lifetime as the main thread shared
    // WebGraphicsContext3D.
    WEBKIT_EXPORT static GrContext* mainThreadGrContext();

    // Attempts to create a context usable from the compositor thread. Must be called from the main (WebKit) thread. Returns false
    // if a context could not be created.
    WEBKIT_EXPORT static bool createCompositorThreadContext();

    // Can be called from any thread.
    WEBKIT_EXPORT static bool haveCompositorThreadContext();

    // Returns a context usable from the compositor thread, if there is one. The context must be used only from the compositor thread.
    // The returned context is valid until the next call to createCompositorThreadContext().
    // If this context becomes lost or unusable, the caller should call createCompositorThreadContext() on the main thread and
    // then use the newly created context from the compositor thread.
    WEBKIT_EXPORT static WebGraphicsContext3D* compositorThreadContext();

    // Returns a ganesh context associated with the compositor thread shared context. The ganesh context has the same lifetime as the compositor thread
    // shared WebGraphicsContext3D.
    WEBKIT_EXPORT static GrContext* compositorThreadGrContext();
};

}

#endif // WebSharedGraphicsContext3D_h
