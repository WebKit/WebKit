/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef IDBObjectStoreBackendImpl_h
#define IDBObjectStoreBackendImpl_h

#include "IDBObjectStoreBackendInterface.h"
#include "StringHash.h"
#include <wtf/HashMap.h>

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

template <typename ValueType> class IDBKeyTree;

class IDBObjectStoreBackendImpl : public IDBObjectStoreBackendInterface {
public:
    static PassRefPtr<IDBObjectStoreBackendInterface> create(const String& name, const String& keyPath, bool autoIncrement)
    {
        return adoptRef(new IDBObjectStoreBackendImpl(name, keyPath, autoIncrement));
    }
    ~IDBObjectStoreBackendImpl();

    String name() const { return m_name; }
    String keyPath() const { return m_keyPath; }
    PassRefPtr<DOMStringList> indexNames() const;

    void get(PassRefPtr<IDBKey> key, PassRefPtr<IDBCallbacks>);
    void put(PassRefPtr<SerializedScriptValue> value, PassRefPtr<IDBKey> key, bool addOnly, PassRefPtr<IDBCallbacks>);
    void remove(PassRefPtr<IDBKey> key, PassRefPtr<IDBCallbacks>);

    void createIndex(const String& name, const String& keyPath, bool unique, PassRefPtr<IDBCallbacks>);
    PassRefPtr<IDBIndexBackendInterface> index(const String& name);
    void removeIndex(const String& name, PassRefPtr<IDBCallbacks>);

    void openCursor(PassRefPtr<IDBKeyRange> range, unsigned short direction, PassRefPtr<IDBCallbacks>);

private:
    IDBObjectStoreBackendImpl(const String& name, const String& keyPath, bool autoIncrement);

    String m_name;
    String m_keyPath;
    bool m_autoIncrement;

    typedef HashMap<String, RefPtr<IDBIndexBackendInterface> > IndexMap;
    IndexMap m_indexes;

    typedef IDBKeyTree<SerializedScriptValue> Tree;
    RefPtr<Tree> m_tree;
};

} // namespace WebCore

#endif

#endif // IDBObjectStoreBackendImpl_h
