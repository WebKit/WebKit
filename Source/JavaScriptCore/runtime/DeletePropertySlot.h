/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include <JavaScriptCore/PropertyOffset.h>
#include <JavaScriptCore/PropertySlot.h>

namespace JSC {
    
class DeletePropertySlot {
public:
    enum class Type : uint8_t { Uncacheable, DeleteHit, ConfigurableDeleteMiss, Nonconfigurable };

    DeletePropertySlot()
        : m_offset(invalidOffset)
        , m_cacheability(CachingAllowed)
        , m_type(Type::Uncacheable)
    {
    }

    void setConfigurableMiss()
    {
        m_type = Type::ConfigurableDeleteMiss;
    }

    void setNonconfigurable()
    {
        m_type = Type::Nonconfigurable;
    }

    void setHit(PropertyOffset offset)
    {
        m_type = Type::DeleteHit;
        m_offset = offset;
    }

    bool isCacheableDelete() const { return isCacheable() && m_type != Type::Uncacheable; }
    bool isDeleteHit() const { return m_type == Type::DeleteHit; }
    bool isConfigurableDeleteMiss() const { return m_type == Type::ConfigurableDeleteMiss; }
    bool isNonconfigurable() const { return m_type == Type::Nonconfigurable; }

    PropertyOffset cachedOffset() const
    {
        return m_offset;
    }

    void disableCaching()
    {
        m_cacheability = CachingDisallowed;
    }

private:
    bool isCacheable() const { return m_cacheability == CachingAllowed; }

    PropertyOffset m_offset;
    CacheabilityType m_cacheability;
    Type m_type;
};

} // namespace JSC
