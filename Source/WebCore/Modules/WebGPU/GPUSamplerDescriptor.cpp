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
#include "GPUSamplerDescriptor.h"

#include "JSGPUAddressMode.h"
#include "JSGPUCompareFunction.h"
#include "JSGPUFilterMode.h"
#include "JSGPUMipmapFilterMode.h"
#include <wtf/JSONValues.h>

namespace WebCore {

Ref<JSON::Object> GPUSamplerDescriptor::toJSON() const
{
    Ref json = GPUObjectDescriptorBase::toJSON();
    json->setString("addressModeU"_s, convertEnumerationToString(addressModeU));
    json->setString("addressModeV"_s, convertEnumerationToString(addressModeV));
    json->setString("addressModeW"_s, convertEnumerationToString(addressModeW));
    json->setString("magFilter"_s, convertEnumerationToString(magFilter));
    json->setString("minFilter"_s, convertEnumerationToString(minFilter));
    json->setString("mipmapFilter"_s, convertEnumerationToString(mipmapFilter));
    json->setDouble("lodMinClamp"_s, lodMinClamp);
    json->setDouble("lodMaxClamp"_s, lodMaxClamp);
    if (compare)
        json->setString("compare"_s, convertEnumerationToString(*compare));
    json->setDouble("maxAnisotropy"_s, maxAnisotropy);
    return json;
}

} // namespace WebCore
