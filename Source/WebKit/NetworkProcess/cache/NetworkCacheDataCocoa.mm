/*
 * Copyright (C) 2014-2015 Apple Inc. All rights reserved.
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

#import "config.h"
#import "NetworkCacheData.h"

#import <WebCore/SharedMemory.h>
#import <sys/mman.h>
#import <sys/stat.h>
#import <wtf/FileHandle.h>
#import <wtf/cocoa/SpanCocoa.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/darwin/DispatchExtras.h>

namespace WebKit {
namespace NetworkCache {

Data::Data(std::span<const uint8_t> data)
    : m_dispatchData(adoptOSObject(dispatch_data_create(data.data(), data.size(), nullptr, DISPATCH_DATA_DESTRUCTOR_DEFAULT)))
{
}

Data::Data(OSObjectPtr<dispatch_data_t>&& dispatchData, Backing backing)
    : m_dispatchData(WTFMove(dispatchData))
    , m_isMap(backing == Backing::Map && dispatch_data_get_size(m_dispatchData.get()))
{
}

Data::Data(Vector<uint8_t>&& data)
    : Data(makeDispatchData(WTFMove(data)).get(), Backing::Buffer)
{
}

OSObjectPtr<dispatch_data_t> Data::protectedDispatchData() const
{
    return dispatchData();
}

Data Data::empty()
{
    return { OSObjectPtr<dispatch_data_t> { dispatch_data_empty } };
}

std::span<const uint8_t> Data::span() const
{
    if (!m_data.data() && m_dispatchData) {
        const void* data = nullptr;
        size_t size = 0;
        m_dispatchData = adoptOSObject(dispatch_data_create_map(m_dispatchData.get(), &data, &size));
        m_data = unsafeMakeSpan(static_cast<const uint8_t*>(data), size);
    }
    return m_data;
}

size_t Data::size() const
{
    if (!m_data.data() && m_dispatchData)
        return dispatch_data_get_size(m_dispatchData.get());
    return m_data.size();
}

bool Data::isNull() const
{
    return !m_dispatchData;
}

bool Data::apply(NOESCAPE const Function<bool(std::span<const uint8_t>)>& applier) const
{
    if (!size())
        return false;
    return dispatch_data_apply_span(m_dispatchData.get(), applier);
}

Data Data::subrange(size_t offset, size_t size) const
{
    return { adoptOSObject(dispatch_data_create_subrange(protectedDispatchData().get(), offset, size)) };
}

Data concatenate(const Data& a, const Data& b)
{
    if (a.isNull())
        return b;
    if (b.isNull())
        return a;
    return { adoptOSObject(dispatch_data_create_concat(a.protectedDispatchData().get(), b.protectedDispatchData().get())) };
}

Data Data::adoptMap(FileSystem::MappedFileData&& mappedFile, FileSystem::FileHandle&& outputHandle)
{
    auto span = mappedFile.leakHandle();
    ASSERT(span.data());
    ASSERT(span.data() != MAP_FAILED);
    outputHandle = { };
    auto bodyMap = adoptOSObject(dispatch_data_create(span.data(), span.size(), globalDispatchQueueSingleton(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), [span] {
        munmap(span.data(), span.size());
    }));
    return { WTFMove(bodyMap), Data::Backing::Map };
}

RefPtr<WebCore::SharedMemory> Data::tryCreateSharedMemory() const
{
    if (isNull() || !isMap())
        return nullptr;

    return WebCore::SharedMemory::wrapMap(span(), WebCore::SharedMemory::Protection::ReadOnly);
}

}
}
