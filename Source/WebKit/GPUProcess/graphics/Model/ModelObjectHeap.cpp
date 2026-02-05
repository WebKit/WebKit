/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "ModelObjectHeap.h"

#if ENABLE(GPU_PROCESS)

#include "RemoteMesh.h"
#include <wtf/TZoneMallocInlines.h>

#include <WebCore/Mesh.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ModelObjectHeap);

ModelObjectHeap::ModelObjectHeap()
{
    weakPtrFactory().prepareForUseOnlyOnNonMainThread();
}

ModelObjectHeap::~ModelObjectHeap() = default;

void ModelObjectHeap::addObject(WebModelIdentifier identifier, RemoteMesh& mesh)
{
#if ENABLE(GPU_PROCESS_MODEL)
    auto result = m_objects.add(identifier, Object { IPC::ScopedActiveMessageReceiveQueue<RemoteMesh> { Ref { mesh } } });
    ASSERT_UNUSED(result, result.isNewEntry);
#else
    UNUSED_PARAM(identifier);
    UNUSED_PARAM(mesh);
#endif
}

void ModelObjectHeap::removeObject(WebModelIdentifier identifier)
{
    auto result = m_objects.remove(identifier);
    ASSERT_UNUSED(result, result);
}

void ModelObjectHeap::clear()
{
    m_objects.clear();
}

WeakPtr<WebCore::Mesh> ModelObjectHeap::convertMeshFromBacking(WebModelIdentifier identifier)
{
#if ENABLE(GPU_PROCESS_MODEL)
    auto iterator = m_objects.find(identifier);
    if (iterator == m_objects.end() || !std::holds_alternative<IPC::ScopedActiveMessageReceiveQueue<RemoteMesh>>(iterator->value))
        return nullptr;
    return &std::get<IPC::ScopedActiveMessageReceiveQueue<RemoteMesh>>(iterator->value)->backing();
#else
    UNUSED_PARAM(identifier);
    return nullptr;
#endif
}

ModelObjectHeap::ExistsAndValid ModelObjectHeap::objectExistsAndValid(const WebCore::WebGPU::GPU& gpu, WebModelIdentifier identifier) const
{
    ExistsAndValid result;
    auto it = m_objects.find(identifier);
    if (it == m_objects.end())
        return result;

    result.exists = true;
    result.valid = WTF::switchOn(it->value, [&](std::monostate) -> bool {
        return false;
    },
    [&](auto&) -> bool {
        return true;
    });

    return result;
}

}

#endif // HAVE(GPU_PROCESS)
