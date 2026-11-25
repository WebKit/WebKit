/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#include <WebCore/LocalDOMWindowProperty.h>
#include <WebCore/ScriptWrappable.h>

namespace WebCore {

class StorageArea;
template<typename> class ExceptionOr;

class Storage final : public ScriptWrappable, public RefCounted<Storage>, public LocalDOMWindowProperty {
    WTF_MAKE_TZONE_ALLOCATED(Storage);
public:
    static Ref<Storage> create(LocalDOMWindow&, Ref<StorageArea>&&);
    ~Storage();

    unsigned length() const;
    RefPtr<StringImpl> key(unsigned index) const;
    RefPtr<StringImpl> getItem(StringImpl* key) const;
    ExceptionOr<void> setItem(StringImpl* key, StringImpl* value);
    ExceptionOr<void> removeItem(StringImpl* key);
    ExceptionOr<void> clear();
    bool contains(StringImpl* key) const;

    // Bindings support functions.
    bool isSupportedPropertyName(StringImpl*) const;
    Vector<RefPtr<AtomStringImpl>> supportedPropertyNames() const;

    StorageArea& area() const { return m_storageArea.get(); }

private:
    Storage(LocalDOMWindow&, Ref<StorageArea>&&);

    bool requiresScriptTrackingPrivacyProtection() const;

    const Ref<StorageArea> m_storageArea;
};

} // namespace WebCore
