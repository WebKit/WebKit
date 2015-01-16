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
#include "DatabaseBackend.h"

#include "ChangeVersionData.h"
#include "ChangeVersionWrapper.h"
#include "Database.h"
#include "DatabaseContext.h"
#include "DatabaseTask.h"
#include "DatabaseThread.h"
#include "DatabaseTracker.h"
#include "Logging.h"
#include "SQLError.h"
#include "SQLTransaction.h"
#include "SQLTransactionBackend.h"
#include "SQLTransactionCallback.h"
#include "SQLTransactionClient.h"
#include "SQLTransactionCoordinator.h"
#include "SQLTransactionErrorCallback.h"
#include "VoidCallback.h"

namespace WebCore {

DatabaseBackend::DatabaseBackend(PassRefPtr<DatabaseContext> databaseContext, const String& name, const String& expectedVersion, const String& displayName, unsigned long estimatedSize)
    : DatabaseBackendBase(databaseContext, name, expectedVersion, displayName, estimatedSize)
    , m_transactionInProgress(false)
    , m_isTransactionQueueEnabled(true)
{
}

} // namespace WebCore
