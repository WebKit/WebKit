/*
 * Copyright (c) 2021-2022 Apple Inc. All rights reserved.
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

#import "config.h"
#import "QuerySet.h"

#import "Device.h"
#import "WebGPUExt.h"

namespace WebGPU {

RefPtr<QuerySet> Device::createQuerySet(const WGPUQuerySetDescriptor* descriptor)
{
    UNUSED_PARAM(descriptor);
    return QuerySet::create();
}

QuerySet::QuerySet() = default;

QuerySet::~QuerySet() = default;

void QuerySet::destroy()
{
}

void QuerySet::setLabel(const char* label)
{
    UNUSED_PARAM(label);
}

} // namespace WebGPU

void wgpuQuerySetRelease(WGPUQuerySet querySet)
{
    delete querySet;
}

void wgpuQuerySetDestroy(WGPUQuerySet querySet)
{
    querySet->querySet->destroy();
}

void wgpuQuerySetSetLabel(WGPUQuerySet querySet, const char* label)
{
    querySet->querySet->setLabel(label);
}
