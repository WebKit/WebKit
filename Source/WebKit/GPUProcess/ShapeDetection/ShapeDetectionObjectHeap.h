/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(GPU_PROCESS)

#include "ScopedActiveMessageReceiveQueue.h"
#include "ShapeDetectionIdentifier.h"
#include <functional>
#include <variant>
#include <wtf/HashMap.h>
#include <wtf/Ref.h>
#include <wtf/WeakPtr.h>

namespace WebCore::ShapeDetection {
class BarcodeDetector;
class FaceDetector;
class TextDetector;
}

namespace WebKit {
class RemoteBarcodeDetector;
class RemoteFaceDetector;
class RemoteTextDetector;
}

namespace WebKit::ShapeDetection {

class ObjectHeap final : public RefCounted<ObjectHeap>, public CanMakeWeakPtr<ObjectHeap> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<ObjectHeap> create()
    {
        return adoptRef(*new ObjectHeap);
    }

    ~ObjectHeap();

    void addObject(ShapeDetectionIdentifier, RemoteBarcodeDetector&);
    void addObject(ShapeDetectionIdentifier, RemoteFaceDetector&);
    void addObject(ShapeDetectionIdentifier, RemoteTextDetector&);

    void removeObject(ShapeDetectionIdentifier);

    void clear();

private:
    ObjectHeap();

    HashMap<ShapeDetectionIdentifier, Ref<RemoteBarcodeDetector>> m_barcodeDetectors;
    HashMap<ShapeDetectionIdentifier, Ref<RemoteFaceDetector>> m_faceDetectors;
    HashMap<ShapeDetectionIdentifier, Ref<RemoteTextDetector>> m_textDetectors;
};

} // namespace WebKit::ShapeDetection

#endif // ENABLE(GPU_PROCESS)
