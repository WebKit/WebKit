/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Matt Lilek <webkit@mattlilek.com>
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef InspectorDOMStorageResource_h
#define InspectorDOMStorageResource_h

#include "InspectorFrontend.h"

#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {
class SecurityOrigin;
class StorageArea;
class Frame;
class InspectorFrontend;

class InspectorDOMStorageResource : public RefCounted<InspectorDOMStorageResource> {
public:
    static PassRefPtr<InspectorDOMStorageResource> create(StorageArea* storageArea, bool isLocalStorage, Frame* frame)
    {
        return adoptRef(new InspectorDOMStorageResource(storageArea, isLocalStorage, frame));
    }

    void bind(InspectorFrontend*);
    void unbind();

    bool isSameOriginAndType(SecurityOrigin*, bool isLocalStorage) const;
    String id() const { return m_id; }
    StorageArea* storageArea() const { return m_storageArea.get(); }
    Frame* frame() const { return m_frame.get(); }

    void reportMemoryUsage(MemoryObjectInfo*) const;

private:
    InspectorDOMStorageResource(StorageArea*, bool isLocalStorage, Frame*);

    RefPtr<StorageArea> m_storageArea;
    bool m_isLocalStorage;
    RefPtr<Frame> m_frame;
    InspectorFrontend::DOMStorage* m_frontend;
    String m_id;

    static int s_nextUnusedId;
};

} // namespace WebCore

#endif // InspectorDOMStorageResource_h
