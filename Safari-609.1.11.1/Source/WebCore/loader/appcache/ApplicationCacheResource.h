/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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

#include "SubstituteResource.h"
#include <wtf/WeakPtr.h>

namespace WebCore {

class ApplicationCacheResource : public SubstituteResource, public CanMakeWeakPtr<ApplicationCacheResource> {
public:
    enum Type {
        Master = 1 << 0,
        Manifest = 1 << 1,
        Explicit = 1 << 2,
        Foreign = 1 << 3,
        Fallback = 1 << 4
    };

    static Ref<ApplicationCacheResource> create(const URL&, const ResourceResponse&, unsigned type, RefPtr<SharedBuffer>&& = SharedBuffer::create(), const String& path = String());

    unsigned type() const { return m_type; }
    void addType(unsigned type);
    
    void setStorageID(unsigned storageID) { m_storageID = storageID; }
    unsigned storageID() const { return m_storageID; }
    void clearStorageID() { m_storageID = 0; }
    int64_t estimatedSizeInStorage();

    const String& path() const { return m_path; }
    void setPath(const String& path) { m_path = path; }

#ifndef NDEBUG
    static void dumpType(unsigned type);
#endif
    
private:
    ApplicationCacheResource(URL&&, ResourceResponse&&, unsigned type, Ref<SharedBuffer>&&, const String& path);

    void deliver(ResourceLoader&) override;

    unsigned m_type;
    unsigned m_storageID;
    int64_t m_estimatedSizeInStorage;
    String m_path;
};
    
} // namespace WebCore
