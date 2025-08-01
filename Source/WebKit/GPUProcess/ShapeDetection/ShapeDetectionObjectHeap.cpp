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

#include "config.h"
#include "ShapeDetectionObjectHeap.h"

#if ENABLE(GPU_PROCESS)

#include "RemoteBarcodeDetector.h"
#include "RemoteFaceDetector.h"
#include "RemoteRenderingBackend.h"
#include "RemoteTextDetector.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebKit::ShapeDetection {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ObjectHeap);

ObjectHeap::ObjectHeap()
{
    weakPtrFactory().prepareForUseOnlyOnNonMainThread();
}

ObjectHeap::~ObjectHeap() = default;

void ObjectHeap::addObject(ShapeDetectionIdentifier identifier, RemoteBarcodeDetector& barcodeDetector)
{
    m_barcodeDetectors.add(identifier, barcodeDetector);
}

void ObjectHeap::addObject(ShapeDetectionIdentifier identifier, RemoteFaceDetector& faceDetector)
{
    m_faceDetectors.add(identifier, faceDetector);
}

void ObjectHeap::addObject(ShapeDetectionIdentifier identifier, RemoteTextDetector& textDetector)
{
    m_textDetectors.add(identifier, textDetector);
}

void ObjectHeap::removeObject(ShapeDetectionIdentifier identifier)
{
    m_barcodeDetectors.remove(identifier);
    m_faceDetectors.remove(identifier);
    m_textDetectors.remove(identifier);
}

void ObjectHeap::clear()
{
    m_barcodeDetectors.clear();
    m_faceDetectors.clear();
    m_textDetectors.clear();
}

} // namespace WebKit::ShapeDetection

#endif // ENABLE(GPU_PROCESS)
