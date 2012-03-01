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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
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

#include "config.h"
#include "DOMWindowIndexedDatabase.h"

#if ENABLE(INDEXED_DATABASE)

#include "DOMWindow.h"
#include "Document.h"
#include "IDBFactory.h"
#include "Page.h"
#include "PageGroup.h"
#include "SecurityOrigin.h"

namespace WebCore {

DOMWindowIndexedDatabase::DOMWindowIndexedDatabase()
{
}

DOMWindowIndexedDatabase::~DOMWindowIndexedDatabase()
{
}

IDBFactory* DOMWindowIndexedDatabase::webkitIndexedDB(DOMWindow* window)
{
    Document* document = window->document();
    if (!document)
        return 0;

    Page* page = document->page();
    if (!page)
        return 0;

    if (!document->securityOrigin()->canAccessDatabase())
        return 0;

    if (!window->idbFactory() && window->isCurrentlyDisplayedInFrame())
        window->setIDBFactory(IDBFactory::create(page->group().idbFactory()));
    return window->idbFactory();
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
