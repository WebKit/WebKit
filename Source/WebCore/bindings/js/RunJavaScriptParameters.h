/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#pragma once

#include <wtf/HashMap.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

enum class RunAsAsyncFunction : bool { No, Yes };
enum class ForceUserGesture : bool { No, Yes };

using ArgumentWireBytesMap = HashMap<String, Vector<uint8_t>>;

struct RunJavaScriptParameters {
    RunJavaScriptParameters(String&& source, RunAsAsyncFunction runAsAsyncFunction, Optional<ArgumentWireBytesMap>&& arguments, ForceUserGesture forceUserGesture)
        : source(WTFMove(source))
        , runAsAsyncFunction(runAsAsyncFunction)
        , arguments(WTFMove(arguments))
        , forceUserGesture(forceUserGesture)
    {
    }

    RunJavaScriptParameters(const String& source, bool runAsAsyncFunction, Optional<ArgumentWireBytesMap>&& arguments, bool forceUserGesture)
        : source(source)
        , runAsAsyncFunction(runAsAsyncFunction ? RunAsAsyncFunction::Yes : RunAsAsyncFunction::No)
        , arguments(WTFMove(arguments))
        , forceUserGesture(forceUserGesture ? ForceUserGesture::Yes : ForceUserGesture::No)
    {
    }

    RunJavaScriptParameters(String&& source, bool runAsAsyncFunction, Optional<ArgumentWireBytesMap>&& arguments, bool forceUserGesture)
        : source(WTFMove(source))
        , runAsAsyncFunction(runAsAsyncFunction ? RunAsAsyncFunction::Yes : RunAsAsyncFunction::No)
        , arguments(WTFMove(arguments))
        , forceUserGesture(forceUserGesture ? ForceUserGesture::Yes : ForceUserGesture::No)
    {
    }

    String source;
    RunAsAsyncFunction runAsAsyncFunction;
    Optional<ArgumentWireBytesMap> arguments;
    ForceUserGesture forceUserGesture;

    template<typename Encoder> void encode(Encoder& encoder) const
    {
        encoder << source << runAsAsyncFunction << arguments << forceUserGesture;
    }

    template<typename Decoder> static Optional<RunJavaScriptParameters> decode(Decoder& decoder)
    {
        String source;
        if (!decoder.decode(source))
            return WTF::nullopt;

        RunAsAsyncFunction runAsAsyncFunction;
        if (!decoder.decode(runAsAsyncFunction))
            return WTF::nullopt;

        Optional<ArgumentWireBytesMap> arguments;
        if (!decoder.decode(arguments))
            return WTF::nullopt;

        ForceUserGesture forceUserGesture;
        if (!decoder.decode(forceUserGesture))
            return WTF::nullopt;

        return { RunJavaScriptParameters { WTFMove(source), runAsAsyncFunction, WTFMove(arguments), forceUserGesture } };
    }
};

} // namespace WebCore
