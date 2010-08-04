/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebGLES2Context_h
#define WebGLES2Context_h

#include "WebCommon.h"
#include "WebNonCopyable.h"

namespace WebKit {

struct WebSize;
class WebView;

// This interface abstracts the creation and management of an
// OpenGL ES 2.0 context.

class WebGLES2Context : public WebNonCopyable {
public:
    virtual ~WebGLES2Context() {}

    virtual bool initialize(WebView*, WebGLES2Context* parent) = 0;
    virtual bool makeCurrent() = 0;
    virtual bool destroy() = 0;
    virtual bool swapBuffers() = 0;

    // The follow two functions are for managing a context that renders offscreen.

    // Resizes the backing store used for offscreen rendering.
    virtual void resizeOffscreenContent(const WebSize&) = 0;

    // Returns the ID of the texture used for offscreen rendering in the context of the parent.
    virtual unsigned getOffscreenContentParentTextureId() = 0;

    // The following function is used only on Mac OS X and is needed
    // in order to report window size changes.
#if defined(__APPLE__)
    virtual void resizeOnscreenContent(const WebSize&) = 0;
#endif
};

} // namespace WebKit

#endif
