/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef APINumber_h
#define APINumber_h

#include "APIObject.h"
#include "ArgumentDecoder.h"
#include "ArgumentEncoder.h"
#include <wtf/PassRefPtr.h>

namespace API {

template<typename NumberType, API::Object::Type APIObjectType>
class Number : public ObjectImpl<APIObjectType> {
public:
    static PassRefPtr<Number> create(NumberType value)
    {
        return adoptRef(new Number(value));
    }

    NumberType value() const { return m_value; }

    void encode(IPC::ArgumentEncoder& encoder) const
    {
        encoder << m_value;
    }

    static bool decode(IPC::ArgumentDecoder& decoder, RefPtr<Object>& result)
    {
        NumberType value;
        if (!decoder.decode(value))
            return false;

        result = Number::create(value);
        return true;
    }

private:
    explicit Number(NumberType value)
        : m_value(value)
    {
    }

    const NumberType m_value;
};

typedef Number<bool, API::Object::Type::Boolean> Boolean;
typedef Number<double, API::Object::Type::Double> Double;
typedef Number<uint64_t, API::Object::Type::UInt64> UInt64;

} // namespace API

#endif // APINumber_h
