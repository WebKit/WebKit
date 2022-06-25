/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "GCRequest.h"

namespace JSC {

bool GCRequest::subsumedBy(const GCRequest& other) const
{
    // If we have callbacks, then there is no chance that we're subsumed by an existing request.
    if (didFinishEndPhase)
        return false;
    
    if (other.scope == CollectionScope::Full)
        return true;
    
    if (scope) {
        // If we're eden, then we're subsumed by the other scope because the other scope is either eden
        // or disengaged (so either eden or full). If we're full, then we're not subsumed, for the same
        // reason.
        return scope == CollectionScope::Eden;
    }
    
    // At this point we know that other.scope is either not engaged or Eden, and this.scope is not
    // engaged. So, we're expecting to do either an eden or full collection, and the other scope is
    // either the same or is requesting specifically a full collection. We are subsumed if the other
    // scope is disengaged (so same as us).
    return !other.scope;
}

void GCRequest::dump(PrintStream& out) const
{
    out.print("{scope = ", scope, ", didFinishEndPhase = ", didFinishEndPhase ? "engaged" : "null", "}");
}

} // namespace JSC

