/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "SwiftCxxInteropTestbed.h"

#include <optional>
#include <wtf/Assertions.h>

namespace SwiftCxxInteropTestbed {

// Handed to a stored handler that resetStoredIntCompletionHandler() has to call itself.
static constexpr int teardownValue = -2;

int callIntBoolFunction(bool argument, IntBoolFunction&& function)
{
    return function(argument);
}

void callIntCompletionHandler(int argument, IntCompletionHandler&& completionHandler)
{
    completionHandler(argument);
}

void callVoidCompletionHandler(VoidCompletionHandler&& completionHandler)
{
    completionHandler();
}

static std::optional<IntCompletionHandler>& storedIntCompletionHandler()
{
    static std::optional<IntCompletionHandler> handler;
    return handler;
}

void storeIntCompletionHandler(IntCompletionHandler&& completionHandler)
{
    // Overwriting the slot would destroy an uncalled handler: that trips
    // ~CompletionHandler's assertion and strands whatever the handler owned, so a test
    // awaiting a continuation it held would hang instead of failing.
    RELEASE_ASSERT(!storedIntCompletionHandler());
    storedIntCompletionHandler().emplace(WTF::move(completionHandler));
}

void invokeStoredIntCompletionHandler(int argument)
{
    auto handler = WTF::move(storedIntCompletionHandler());
    storedIntCompletionHandler().reset();

    if (handler)
        (*handler)(argument);
}

void resetStoredIntCompletionHandler()
{
    auto handler = WTF::move(storedIntCompletionHandler());
    storedIntCompletionHandler().reset();

    // Call rather than just drop it, both because ~CompletionHandler requires it and so
    // that a test which returned early fails on the sentinel instead of hanging. Teardown
    // must never assert itself, so check the handler has not already run.
    if (handler && *handler)
        (*handler)(teardownValue);
}

};
