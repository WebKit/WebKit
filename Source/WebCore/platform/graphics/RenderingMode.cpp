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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RenderingMode.h"

#include <wtf/text/TextStream.h>

namespace WebCore {

TextStream& operator<<(TextStream& ts, RenderingPurpose purpose)
{
    switch (purpose) {
    case RenderingPurpose::Unspecified: ts << "Unspecified"_s; break;
    case RenderingPurpose::Canvas: ts << "Canvas"_s; break;
    case RenderingPurpose::DOM: ts << "DOM"_s; break;
    case RenderingPurpose::LayerBacking: ts << "LayerBacking"_s; break;
    case RenderingPurpose::Snapshot: ts << "Snapshot"_s; break;
    case RenderingPurpose::ShareableSnapshot: ts << "ShareableSnapshot"_s; break;
    case RenderingPurpose::ShareableLocalSnapshot: ts << "ShareableLocalSnapshot"_s; break;
    case RenderingPurpose::MediaPainting: ts << "MediaPainting"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, RenderingMode mode)
{
    switch (mode) {
    case RenderingMode::Unaccelerated: ts << "Unaccelerated"_s; break;
    case RenderingMode::Accelerated: ts << "Accelerated"_s; break;
    case RenderingMode::PDFDocument: ts << "PDFDocument"_s; break;
    case RenderingMode::DisplayList: ts << "DisplayList"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, RenderingMethod method)
{
    switch (method) {
    case RenderingMethod::Local: ts << "Local"_s; break;
    }

    return ts;
}

} // namespace WebCore
