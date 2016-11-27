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
#include <wtf/Optional.h>
#include <wtf/Variant.h>

namespace WebCore {

template<typename ReturnType> class ExceptionOr {
public:
    ExceptionOr(Exception&&);
    ExceptionOr(ReturnType&&);
    template<typename OtherType> ExceptionOr(const OtherType&, typename std::enable_if<std::is_scalar<OtherType>::value && std::is_convertible<OtherType, ReturnType>::value>::type* = nullptr);

    bool hasException() const;
    Exception&& releaseException();
    ReturnType&& releaseReturnValue();

private:
    Variant<Exception, ReturnType> m_value;
};

template<typename ReturnReferenceType> class ExceptionOr<ReturnReferenceType&> {
public:
    ExceptionOr(Exception&&);
    ExceptionOr(ReturnReferenceType&);

    bool hasException() const;
    Exception&& releaseException();
    ReturnReferenceType& releaseReturnValue();

private:
    ExceptionOr<ReturnReferenceType*> m_value;
};

template<> class ExceptionOr<void> {
public:
    ExceptionOr(Exception&&);
    ExceptionOr() = default;

    bool hasException() const;
    Exception&& releaseException();

private:
    std::optional<Exception> m_exception;
};

ExceptionOr<void> isolatedCopy(ExceptionOr<void>&&);

template<typename ReturnType> inline ExceptionOr<ReturnType>::ExceptionOr(Exception&& exception)
    : m_value(WTFMove(exception))
{
}

template<typename ReturnType> inline ExceptionOr<ReturnType>::ExceptionOr(ReturnType&& returnValue)
    : m_value(WTFMove(returnValue))
{
}

template<typename ReturnType> template<typename OtherType> inline ExceptionOr<ReturnType>::ExceptionOr(const OtherType& returnValue, typename std::enable_if<std::is_scalar<OtherType>::value && std::is_convertible<OtherType, ReturnType>::value>::type*)
    : m_value(static_cast<ReturnType>(returnValue))
{
}

template<typename ReturnType> inline bool ExceptionOr<ReturnType>::hasException() const
{
    return WTF::holds_alternative<Exception>(m_value);
}

template<typename ReturnType> inline Exception&& ExceptionOr<ReturnType>::releaseException()
{
    return WTF::get<Exception>(WTFMove(m_value));
}

template<typename ReturnType> inline ReturnType&& ExceptionOr<ReturnType>::releaseReturnValue()
{
    return WTF::get<ReturnType>(WTFMove(m_value));
}

template<typename ReturnReferenceType> inline ExceptionOr<ReturnReferenceType&>::ExceptionOr(Exception&& exception)
    : m_value(WTFMove(exception))
{
}

template<typename ReturnReferenceType> inline ExceptionOr<ReturnReferenceType&>::ExceptionOr(ReturnReferenceType& returnValue)
    : m_value(&returnValue)
{
}

template<typename ReturnReferenceType> inline bool ExceptionOr<ReturnReferenceType&>::hasException() const
{
    return m_value.hasException();
}

template<typename ReturnReferenceType> inline Exception&& ExceptionOr<ReturnReferenceType&>::releaseException()
{
    return m_value.releaseException();
}

template<typename ReturnReferenceType> inline ReturnReferenceType& ExceptionOr<ReturnReferenceType&>::releaseReturnValue()
{
    return *m_value.releaseReturnValue();
}

inline ExceptionOr<void>::ExceptionOr(Exception&& exception)
    : m_exception(WTFMove(exception))
{
}

inline bool ExceptionOr<void>::hasException() const
{
    return !!m_exception;
}

inline Exception&& ExceptionOr<void>::releaseException()
{
    return WTFMove(m_exception.value());
}

inline ExceptionOr<void> isolatedCopy(ExceptionOr<void>&& value)
{
    if (value.hasException())
        return isolatedCopy(value.releaseException());
    return { };
}

}
