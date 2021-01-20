/*
 * Copyright (C) 2006, 2007, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
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
#include "DOMWindowWebDatabase.h"

#include "DOMWindow.h"
#include "Database.h"
#include "DatabaseManager.h"
#include "Document.h"
#include "SecurityOrigin.h"

namespace WebCore {

ExceptionOr<RefPtr<Database>> DOMWindowWebDatabase::openDatabase(DOMWindow& window, const String& name, const String& version, const String& displayName, unsigned estimatedSize, RefPtr<DatabaseCallback>&& creationCallback)
{
    if (!window.isCurrentlyDisplayedInFrame())
        return RefPtr<Database> { nullptr };
    auto& manager = DatabaseManager::singleton();
    if (!manager.isAvailable())
        return Exception { SecurityError };
    auto* document = window.document();
    if (!document)
        return Exception { SecurityError };
    document->addConsoleMessage(MessageSource::Storage, MessageLevel::Warning, "Web SQL is deprecated. Please use IndexedDB instead.");

    auto& securityOrigin = document->securityOrigin();
    if (!securityOrigin.canAccessDatabase(&document->topOrigin()))
        return Exception { SecurityError };
    auto result = manager.openDatabase(*window.document(), name, version, displayName, estimatedSize, WTFMove(creationCallback));
    if (result.hasException()) {
        // FIXME: To preserve our past behavior, this discards the error string in the exception.
        // At a later time we may decide that we want to use the error strings, and if so we can just return the exception as is.
        return Exception { result.releaseException().code() };
    }
    return RefPtr<Database> { result.releaseReturnValue() };
}

} // namespace WebCore
