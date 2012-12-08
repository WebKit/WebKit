/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef ScaledImageFragment_h
#define ScaledImageFragment_h

#include "SkBitmap.h"
#include "SkRect.h"
#include "SkSize.h"

#include <wtf/PassOwnPtr.h>

namespace WebCore {

// ScaledImageFragment is a scaled version of an image.
class ScaledImageFragment {
public:
    static PassOwnPtr<ScaledImageFragment> create(const SkISize& scaledSize, const SkBitmap& bitmap, bool isComplete)
    {
        return adoptPtr(new ScaledImageFragment(scaledSize, bitmap, isComplete));
    }

    ScaledImageFragment(const SkISize&, const SkBitmap&, bool isComplete);
    ~ScaledImageFragment();

    const SkISize& scaledSize() const { return m_scaledSize; }
    const SkBitmap& bitmap() const { return m_bitmap; }
    SkBitmap& bitmap() { return m_bitmap; }
    bool isComplete() const { return m_isComplete; }
    void setIsComplete() { m_isComplete = true; }

private:
    SkISize m_scaledSize;
    SkBitmap m_bitmap;
    bool m_isComplete;
};

} // namespace WebCore

#endif
