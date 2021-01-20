/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "File.h"
#include "TextEncoding.h"
#include <wtf/RefCounted.h>
#include <wtf/Variant.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class HTMLFormElement;

class DOMFormData : public RefCounted<DOMFormData> {
public:
    using FormDataEntryValue = Variant<RefPtr<File>, String>;

    struct Item {
        String name;
        FormDataEntryValue data;
    };

    static Ref<DOMFormData> create(HTMLFormElement* form) { return adoptRef(*new DOMFormData(form)); }
    static Ref<DOMFormData> create(const TextEncoding& encoding) { return adoptRef(*new DOMFormData(encoding)); }

    const Vector<Item>& items() const { return m_items; }
    const TextEncoding& encoding() const { return m_encoding; }

    void append(const String& name, const String& value);
    void append(const String& name, Blob&, const String& filename = { });
    void remove(const String& name);
    Optional<FormDataEntryValue> get(const String& name);
    Vector<FormDataEntryValue> getAll(const String& name);
    bool has(const String& name);
    void set(const String& name, const String& value);
    void set(const String& name, Blob&, const String& filename = { });

    class Iterator {
    public:
        explicit Iterator(DOMFormData&);
        Optional<KeyValuePair<String, FormDataEntryValue>> next();

    private:
        Ref<DOMFormData> m_target;
        size_t m_index { 0 };
    };
    Iterator createIterator() { return Iterator { *this }; }

private:
    explicit DOMFormData(const TextEncoding&);
    explicit DOMFormData(HTMLFormElement*);

    Item createFileEntry(const String& name, Blob&, const String& filename);
    void set(const String& name, Item&&);

    TextEncoding m_encoding;
    Vector<Item> m_items;
};

} // namespace WebCore
