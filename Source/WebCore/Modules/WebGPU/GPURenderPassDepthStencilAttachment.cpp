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
#include "GPURenderPassDepthStencilAttachment.h"

#include "JSGPULoadOp.h"
#include "JSGPUStoreOp.h"
#include <wtf/JSONValues.h>

namespace WebCore {

Ref<JSON::Object> GPURenderPassDepthStencilAttachment::toJSON() const
{
    Ref json = JSON::Object::create();
    // `GPURenderPassDepthAttachmentView` is not JSON serializable.
    if (depthClearValue)
        json->setDouble("depthClearValue"_s, *depthClearValue);
    if (depthLoadOp)
        json->setString("depthLoadOp"_s, convertEnumerationToString(*depthLoadOp));
    if (depthStoreOp)
        json->setString("depthStoreOp"_s, convertEnumerationToString(*depthStoreOp));
    json->setBoolean("depthReadOnly"_s, depthReadOnly);
    json->setDouble("stencilClearValue"_s, stencilClearValue);
    if (stencilLoadOp)
        json->setString("stencilLoadOp"_s, convertEnumerationToString(*stencilLoadOp));
    if (stencilStoreOp)
        json->setString("stencilStoreOp"_s, convertEnumerationToString(*stencilStoreOp));
    json->setBoolean("stencilReadOnly"_s, stencilReadOnly);
    return json;
}

} // namespace WebCore
