/*

Copyright (C) 2016 Apple Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1.  Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
2.  Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#pragma once

#include "Exception.h"
#include <wtf/Variant.h>

namespace WebCore {

template<typename ReturnType> class ExceptionOr {
public:
    ExceptionOr(Exception&&);
    ExceptionOr(ReturnType&&);

    bool hasException() const;
    ExceptionCode exceptionCode() const;
    ReturnType&& takeReturnValue();

private:
    std::experimental::variant<Exception, ReturnType> m_value;
};

template<typename ReturnType> inline ExceptionOr<ReturnType>::ExceptionOr(Exception&& exception)
    : m_value(WTFMove(exception))
{
}

template<typename ReturnType> inline ExceptionOr<ReturnType>::ExceptionOr(ReturnType&& returnValue)
    : m_value(WTFMove(returnValue))
{
}

template<typename ReturnType> inline bool ExceptionOr<ReturnType>::hasException() const
{
    return std::experimental::holds_alternative<Exception>(m_value);
}

template<typename ReturnType> inline ExceptionCode ExceptionOr<ReturnType>::exceptionCode() const
{
    return std::experimental::get<Exception>(m_value).code();
}

template<typename ReturnType> inline ReturnType&& ExceptionOr<ReturnType>::takeReturnValue()
{
    return std::experimental::get<ReturnType>(WTFMove(m_value));
}

}
