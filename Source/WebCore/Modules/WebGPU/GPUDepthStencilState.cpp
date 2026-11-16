/*
 * Copyright (C) 2026 Devin Rousso <webkit@devinrousso.com>. All rights reserved.
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
#include "GPUDepthStencilState.h"

#include "JSGPUCompareFunction.h"
#include "JSGPUTextureFormat.h"
#include <wtf/JSONValues.h>

namespace WebCore {

Ref<JSON::Object> GPUDepthStencilState::toJSON() const
{
    Ref json = JSON::Object::create();
    json->setString("format"_s, convertEnumerationToString(format));
    if (depthWriteEnabled)
        json->setBoolean("depthWriteEnabled"_s, *depthWriteEnabled);
    if (depthCompare)
        json->setString("depthCompare"_s, convertEnumerationToString(*depthCompare));
    if (stencilFront)
        json->setObject("stencilFront"_s, stencilFront->toJSON());
    if (stencilBack)
        json->setObject("stencilBack"_s, stencilBack->toJSON());
    json->setDouble("stencilReadMask"_s, stencilReadMask);
    json->setDouble("stencilWriteMask"_s, stencilWriteMask);
    json->setDouble("depthBias"_s, depthBias);
    json->setDouble("depthBiasSlopeScale"_s, depthBiasSlopeScale);
    json->setDouble("depthBiasClamp"_s, depthBiasClamp);
    return json;
}

} // namespace WebCore
