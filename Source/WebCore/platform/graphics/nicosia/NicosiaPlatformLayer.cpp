/*
 * Copyright (C) 2018 Metrological Group B.V.
 * Copyright (C) 2018 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "NicosiaPlatformLayer.h"

namespace Nicosia {

PlatformLayer::PlatformLayer(uint64_t id)
    : m_id(id)
{
}

PlatformLayer::~PlatformLayer() = default;


CompositionLayer::CompositionLayer(uint64_t id, const Impl::Factory& factory)
    : PlatformLayer(id)
    , m_impl(factory(id, *this))
{
}

CompositionLayer::~CompositionLayer() = default;
CompositionLayer::Impl::~Impl() = default;


ContentLayer::ContentLayer(const Impl::Factory& factory)
    : PlatformLayer(0)
    , m_impl(factory(*this))
{
}

ContentLayer::~ContentLayer() = default;
ContentLayer::Impl::~Impl() = default;


BackingStore::BackingStore(const Impl::Factory& factory)
    : m_impl(factory(*this))
{
}

BackingStore::~BackingStore() = default;
BackingStore::Impl::~Impl() = default;


ImageBacking::ImageBacking(const Impl::Factory& factory)
    : m_impl(factory(*this))
{
}

ImageBacking::~ImageBacking() = default;
ImageBacking::Impl::~Impl() = default;

} // namespace Nicosia
