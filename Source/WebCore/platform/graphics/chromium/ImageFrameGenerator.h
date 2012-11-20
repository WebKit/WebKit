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

#ifndef ImageFrameGenerator_h
#define ImageFrameGenerator_h

#include "SkTypes.h"
#include "SkBitmap.h"
#include "SkSize.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/ThreadingPrimitives.h>

namespace WebCore {

class SharedBuffer;

class ImageFrameGenerator : public ThreadSafeRefCounted<ImageFrameGenerator> {
public:
    static PassRefPtr<ImageFrameGenerator> create(PassRefPtr<SharedBuffer> data, bool allDataReceived)
    {
        return adoptRef(new ImageFrameGenerator(data, allDataReceived));
    }

    ImageFrameGenerator(PassRefPtr<SharedBuffer>, bool allDataReceived);
    ~ImageFrameGenerator();

    SkBitmap decodeAndScale(const SkISize& scaledSize, const SkIRect& scaledSubset);

    void setData(PassRefPtr<SharedBuffer>, bool allDataReceived);

private:
    RefPtr<SharedBuffer> m_data;
    bool m_allDataReceived;
    Mutex m_mutex;
};

} // namespace WebCore

#endif
