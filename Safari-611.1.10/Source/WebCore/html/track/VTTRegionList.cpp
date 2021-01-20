/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2014-2017 Apple Inc. All rights reserved.
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
#include "VTTRegionList.h"

#if ENABLE(VIDEO)

namespace WebCore {

VTTRegion* VTTRegionList::item(unsigned index) const
{
    if (index >= m_vector.size())
        return nullptr;
    return const_cast<VTTRegion*>(m_vector[index].ptr());
}

VTTRegion* VTTRegionList::getRegionById(const String& id) const
{
    // FIXME: Why is this special case needed?
    if (id.isEmpty())
        return nullptr;
    for (auto& region : m_vector) {
        if (region->id() == id)
            return const_cast<VTTRegion*>(region.ptr());
    }
    return nullptr;
}

void VTTRegionList::add(Ref<VTTRegion>&& region)
{
    m_vector.append(WTFMove(region));
}

void VTTRegionList::remove(VTTRegion& region)
{
    for (unsigned i = 0, size = m_vector.size(); i < size; ++i) {
        if (m_vector[i].ptr() == &region) {
            m_vector.remove(i);
            return;
        }
    }
    ASSERT_NOT_REACHED();
}

} // namespace WebCore

#endif
