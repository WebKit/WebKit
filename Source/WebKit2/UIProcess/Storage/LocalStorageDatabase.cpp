/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "LocalStorageDatabase.h"

#include "WorkQueue.h"
#include <wtf/PassRefPtr.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

PassRefPtr<LocalStorageDatabase> LocalStorageDatabase::create(const String& databaseFilename, PassRefPtr<WorkQueue> queue)
{
    return adoptRef(new LocalStorageDatabase(databaseFilename, queue));
}

LocalStorageDatabase::LocalStorageDatabase(const String& databaseFilename, PassRefPtr<WorkQueue> queue)
    : m_queue(queue)
{
    // FIXME: If it can't import, then the default WebKit behavior should be that of private browsing,
    // not silently ignoring it. https://bugs.webkit.org/show_bug.cgi?id=25894
    m_queue->dispatch(bind(&LocalStorageDatabase::performImport, this));
}

LocalStorageDatabase::~LocalStorageDatabase()
{
}

void LocalStorageDatabase::performImport()
{
    // FIXME: Actually import.
}

} // namespace WebKit
