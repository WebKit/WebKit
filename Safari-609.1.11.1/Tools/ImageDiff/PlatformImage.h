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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <cmath>
#include <memory>

#if defined(USE_CAIRO) && USE_CAIRO
typedef struct _cairo_surface cairo_surface_t;
#else
typedef struct CGImage *CGImageRef;
#endif

namespace ImageDiff {

class PlatformImage {
public:
    static std::unique_ptr<PlatformImage> createFromStdin(size_t);
    static std::unique_ptr<PlatformImage> createFromDiffData(void*, size_t width, size_t height);

#if defined(USE_CAIRO) && USE_CAIRO
    PlatformImage(cairo_surface_t*);
#else
    PlatformImage(CGImageRef);
#endif
    ~PlatformImage();

    size_t width() const;
    size_t height() const;
    size_t rowBytes() const;
    bool hasAlpha() const;
    unsigned char* pixels() const;
    bool isCompatible(const PlatformImage&) const;
    std::unique_ptr<PlatformImage> difference(const PlatformImage&, float& percentageDifference);
    void writeAsPNGToStdout();

private:
#if defined(USE_CAIRO) && USE_CAIRO
    cairo_surface_t* m_image;
#else
    CGImageRef m_image;
    mutable void* m_buffer { nullptr };
#endif
};

} // namespace ImageDiff
