/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "JavaScriptEvaluationResult.h"
#include "RunJavaScriptParameters.h"
#include "RunJavaScriptResult.h"
#include <WebCore/ExceptionDetails.h>
#include <wtf/StdLibExtras.h>

namespace WebKit {

// FIXME: Remove this once rdar://186141869 is fixed.
class SWIFT_NONCOPYABLE JavaScriptArgumentsBridge {
public:
    JavaScriptArgumentsBridge() = default;

    explicit JavaScriptArgumentsBridge(size_t capacity)
        : m_arguments(std::in_place)
    {
        m_arguments->reserveInitialCapacity(capacity);
    }

    JavaScriptArgumentsBridge(const JavaScriptArgumentsBridge&) = delete;
    JavaScriptArgumentsBridge& operator=(const JavaScriptArgumentsBridge&) = delete;

    JavaScriptArgumentsBridge(JavaScriptArgumentsBridge&&) = default;
    JavaScriptArgumentsBridge& operator=(JavaScriptArgumentsBridge&&) = default;

    void append(String&& key, JavaScriptEvaluationResult&& value)
    {
        m_arguments->constructAndAppend(WTF::move(key), WTF::move(value));
    }

    std::optional<Vector<std::pair<String, JavaScriptEvaluationResult>>> consume()
    {
        return std::exchange(m_arguments, std::nullopt);
    }

private:
    std::optional<Vector<std::pair<String, JavaScriptEvaluationResult>>> m_arguments;
};

}

namespace WebKit::CxxInteropSupport {

inline RunJavaScriptResult::value_type takeValue(RunJavaScriptResult&& expected)
{
    static_assert(!std::is_trivially_destructible_v<RunJavaScriptResult::value_type>);
    static_assert(!std::copy_constructible<RunJavaScriptResult::value_type>);
    return WTF::move(*expected);
}

}
