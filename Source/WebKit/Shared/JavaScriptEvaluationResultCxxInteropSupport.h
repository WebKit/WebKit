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

#if defined(HAVE_NEW_CODABLE) && HAVE_NEW_CODABLE

// FIXME: Remove this once rdar://185497624 is fixed.
class JavaScriptEvaluationOwnedResult {
public:
    explicit JavaScriptEvaluationOwnedResult(JavaScriptEvaluationResult&& result)
        : m_result(WTF::move(result)) { }

    JavaScriptEvaluationOwnedResult(const JavaScriptEvaluationOwnedResult&) = delete;
    JavaScriptEvaluationOwnedResult& operator=(const JavaScriptEvaluationOwnedResult&) = delete;
    JavaScriptEvaluationOwnedResult(JavaScriptEvaluationOwnedResult&&) = default;
    JavaScriptEvaluationOwnedResult& operator=(JavaScriptEvaluationOwnedResult&&) = default;

    using const_iterator = JavaScriptEvaluationResult::Map::const_iterator;
    const_iterator begin() const { return m_result.map().begin(); }
    const_iterator end() const { return m_result.map().end(); }

private:
    JavaScriptEvaluationResult m_result;
};

#endif

}

namespace WebKit::CxxInteropSupport {

// FIXME: Generalize this once rdar://186426517 is fixed.
template<typename Alternative>
inline Alternative alternativeForVariant(const JavaScriptEvaluationResult::Value& value)
{
    return std::get<Alternative>(value);
}

// FIXME: Remove this once rdar://186426517 is fixed.
template<typename Alternative>
inline size_t alternativeIndexForJavaScriptEvaluationResultValue()
{
    return WTF::alternativeIndexV<Alternative, JavaScriptEvaluationResult::Value>;
}

inline uint64_t jsObjectIDRawValue(const JSObjectID& id)
{
    return id.toUInt64();
}

inline RunJavaScriptResult::value_type takeValue(RunJavaScriptResult&& expected)
{
    static_assert(!std::is_trivially_destructible_v<RunJavaScriptResult::value_type>);
    static_assert(!std::copy_constructible<RunJavaScriptResult::value_type>);
    return WTF::move(*expected);
}

}
