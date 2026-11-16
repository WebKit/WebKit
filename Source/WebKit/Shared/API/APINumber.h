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

#pragma once

#include "APIObject.h"

namespace API {

template<typename NumberType>
class Number {
public:
    NumberType value() const { return m_value; }
protected:
    explicit Number(NumberType value)
        : m_value(value) { }
private:
    const NumberType m_value;
};

class Boolean : public Number<bool>, public ObjectImpl<API::Object::Type::Boolean> {
public:
    static Ref<Boolean> create(bool value) { return adoptRef(*new Boolean(value)); }
private:
    explicit Boolean(bool value)
        : Number(value) { }
};

class Double : public Number<double>, public ObjectImpl<API::Object::Type::Double> {
public:
    static Ref<Double> create(double value) { return adoptRef(*new Double(value)); }
private:
    explicit Double(double value)
        : Number(value) { }
};

class UInt64 : public Number<uint64_t>, public ObjectImpl<API::Object::Type::UInt64> {
public:
    static Ref<UInt64> create(uint64_t value) { return adoptRef(*new UInt64(value)); }
private:
    explicit UInt64(uint64_t value)
        : Number(value) { }
};

class Int64 : public Number<int64_t>, public ObjectImpl<API::Object::Type::Int64> {
public:
    static Ref<Int64> create(int64_t value) { return adoptRef(*new Int64(value)); }
private:
    explicit Int64(int64_t value)
        : Number(value) { }
};

} // namespace API

SPECIALIZE_TYPE_TRAITS_API_OBJECT(Boolean);
SPECIALIZE_TYPE_TRAITS_API_OBJECT(Double);
SPECIALIZE_TYPE_TRAITS_API_OBJECT(UInt64);
SPECIALIZE_TYPE_TRAITS_API_OBJECT(Int64);
