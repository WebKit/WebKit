/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#include "Functional.h"
#include "MainThread.h"
#include "StdLibExtras.h"
#include <mutex>

// This file contains deprecated symbols that the last released version of Safari uses.
// Once Safari stops using them, we should remove them.

namespace WTF {

WTF_EXPORT_PRIVATE void callOnMainThread(const Function<void ()>&);
WTF_EXPORT_PRIVATE void lockAtomicallyInitializedStaticMutex();
WTF_EXPORT_PRIVATE void unlockAtomicallyInitializedStaticMutex();

void callOnMainThread(const Function<void ()>& function)
{
    callOnMainThread(std::function<void ()>(function));
}

static std::mutex& atomicallyInitializedStaticMutex()
{
    static std::once_flag onceFlag;
    static std::mutex* mutex;
    std::call_once(onceFlag, []{
        mutex = std::make_unique<std::mutex>().release();
    });

    return *mutex;
}

void lockAtomicallyInitializedStaticMutex()
{
    atomicallyInitializedStaticMutex().lock();
}

void unlockAtomicallyInitializedStaticMutex()
{
    atomicallyInitializedStaticMutex().unlock();
}

} // namespace WTF
