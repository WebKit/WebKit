/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(WEBMETAL)

#include "GPULegacyDepthStencilDescriptor.h"
#include "WebMetalEnums.h"
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class WebMetalDepthStencilDescriptor : public RefCounted<WebMetalDepthStencilDescriptor> {
public:
    static Ref<WebMetalDepthStencilDescriptor> create();

    bool depthWriteEnabled() const;
    void setDepthWriteEnabled(bool);

    using CompareFunction = WebMetalCompareFunction;
    CompareFunction depthCompareFunction() const;
    void setDepthCompareFunction(CompareFunction);

    GPULegacyDepthStencilDescriptor& descriptor() { return m_descriptor; }

private:
    WebMetalDepthStencilDescriptor() = default;

    // FIXME: The default value of "Always" is defined both here and in the
    // GPUDepthStencilDescriptor class's implementation. Might be better to not
    // store the compare function separately here, translate it instead, and then
    // there would be no need for a default value here.

    WebMetalCompareFunction m_depthCompareFunction { WebMetalCompareFunction::Always };
    GPULegacyDepthStencilDescriptor m_descriptor;
};

} // namespace WebCore

#endif
