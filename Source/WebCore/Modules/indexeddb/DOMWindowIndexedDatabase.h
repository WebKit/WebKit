/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#pragma once

#if ENABLE(INDEXED_DATABASE)

#include "DOMWindowProperty.h"
#include "Supplementable.h"

namespace WebCore {

class IDBFactory;
class DOMWindow;

class DOMWindowIndexedDatabase : public DOMWindowProperty, public Supplement<DOMWindow> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit DOMWindowIndexedDatabase(DOMWindow*);
    virtual ~DOMWindowIndexedDatabase();

    static DOMWindowIndexedDatabase* from(DOMWindow*);

    WEBCORE_EXPORT static IDBFactory* indexedDB(DOMWindow&);

    void suspendForPageCache() override;
    void resumeFromPageCache() override;
    void willDestroyGlobalObjectInCachedFrame() override;
    void willDestroyGlobalObjectInFrame() override;
    void willDetachGlobalObjectFromFrame() override;

private:
    IDBFactory* indexedDB();
    static const char* supplementName();

    DOMWindow* m_window;
    RefPtr<IDBFactory> m_idbFactory;
    RefPtr<IDBFactory> m_suspendedIDBFactory;
};

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
