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

#ifndef WebCompositorSoftwareOutputDevice_h
#define WebCompositorSoftwareOutputDevice_h

namespace WebKit {

#ifndef USE_CC_SOFTWARE_OUTPUT_DEVICE
class WebImage;
struct WebSize;

// This is a "tear-off" class providing software drawing support to
// WebCompositorOutputSurface, such as to a platform-provided window
// framebuffer.
class WebCompositorSoftwareOutputDevice {
public:
    virtual ~WebCompositorSoftwareOutputDevice() { }

    // Lock the framebuffer and return a pointer to a WebImage referring to its
    // pixels. Set forWrite if you intend to change the pixels. Readback
    // is supported whether or not forWrite is set.
    virtual WebImage* lock(bool forWrite) = 0;
    virtual void unlock() = 0;

    virtual void didChangeViewportSize(WebSize) = 0;
};
#endif // USE_CC_SOFTWARE_OUTPUT_DEVICE

}

#endif
