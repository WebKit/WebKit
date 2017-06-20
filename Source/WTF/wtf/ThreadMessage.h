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

#pragma once

#include <wtf/ScopedLambda.h>
#include <wtf/Threading.h>

namespace WTF {

using ThreadMessage = ScopedLambda<void(PlatformRegisters&)>;

enum class MessageStatus {
    MessageRan,
    ThreadExited,
};

// This method allows us to send a message which will be run in a signal handler on the desired thread.
// There are several caveates to this method however, This function uses signals so your message should
// be sync signal safe.
WTF_EXPORT_PRIVATE MessageStatus sendMessageScoped(Thread&, const ThreadMessage&);

template<typename Functor>
MessageStatus sendMessage(Thread& targetThread, const Functor& func)
{
    auto lambda = scopedLambdaRef<void(PlatformRegisters&)>(func);
    return sendMessageScoped(targetThread, lambda);
}

} // namespace WTF

using WTF::sendMessage;
