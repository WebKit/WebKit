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

#pragma once

#include "TransferString.h"
#include <wtf/HashMap.h>
#include <wtf/SwiftBridging.h>
#include <wtf/URL.h>
#include <wtf/Vector.h>

namespace WebCore {
enum class RunAsAsyncFunction : bool;
enum class ForceUserGesture : bool;
enum class RemoveTransientActivation : bool;
}

namespace WebKit {

struct RunJavaScriptParameters {
    IPC::TransferString source;
    JSC::SourceTaintedOrigin taintedness;
    URL sourceURL;
    WebCore::RunAsAsyncFunction runAsAsyncFunction;
    std::optional<Vector<std::pair<String, JavaScriptEvaluationResult>>> arguments;
    WebCore::ForceUserGesture forceUserGesture;
    WebCore::RemoveTransientActivation removeTransientActivation;
};

}

namespace WebKit::CxxInteropSupport {

using RunJavaScriptParametersArguments = Vector<std::pair<String, JavaScriptEvaluationResult>>;
using OptionalRunJavaScriptParametersArguments = std::optional<RunJavaScriptParametersArguments>;

// FIXME: Remove this once rdar://186106924 is fixed.
inline OptionalRunJavaScriptParametersArguments makeOptionalArguments(RunJavaScriptParametersArguments&& arguments)
{
    return { WTF::move(arguments) };
}

}
