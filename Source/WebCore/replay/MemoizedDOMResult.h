/*
 * Copyright (C) 2012 University of Washington. All rights reserved.
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MemoizedDOMResult_h
#define MemoizedDOMResult_h

#if ENABLE(WEB_REPLAY)

#include <replay/EncodedValue.h>
#include <replay/NondeterministicInput.h>

namespace WebCore {

class SerializedScriptValue;

typedef int ExceptionCode;

// Add new memoized ctypes here. The first argument is the enum value,
// which cannot conflict with built-in primitive types. The second is
// the actual C type that is used to specialize CTypeTraits. New enum
// values should also be added to the definition in WebInputs.json.
#define FOR_EACH_MEMOIZED_CTYPE(macro) \
    macro(Boolean, bool) \
    macro(Int, int) \
    macro(String, String) \
    macro(Unsigned, unsigned) \
    \
// end of FOR_EACH_MEMOIZED_CTYPE

// We encode this enum so that we can recover MemoizedType when decoding the input
// and then call the correct specialized MemoizedDOMResult<T> constructor.
enum class EncodedCType {
#define CREATE_ENUM_VALUE(name, type) name,

FOR_EACH_MEMOIZED_CTYPE(CREATE_ENUM_VALUE)
#undef CREATE_ENUM_VALUE
};

class MemoizedDOMResultBase : public NondeterministicInputBase {
public:
    MemoizedDOMResultBase(const String& attribute, EncodedCType ctype, ExceptionCode exceptionCode = 0)
        : m_attribute(attribute)
        , m_ctype(ctype)
        , m_exceptionCode(exceptionCode) { }

    virtual ~MemoizedDOMResultBase() { }

    static std::unique_ptr<MemoizedDOMResultBase> createFromEncodedResult(const String& attribute, EncodedCType, EncodedValue, ExceptionCode);

    template<typename T>
    bool convertTo(T& decodedValue);

    virtual EncodedValue encodedResult() const = 0;
    virtual InputQueue queue() const final override { return InputQueue::ScriptMemoizedData; }
    virtual const AtomicString& type() const final override;

    const String& attribute() const { return m_attribute; }
    EncodedCType ctype() const { return m_ctype; }
    ExceptionCode exceptionCode() const { return m_exceptionCode; }
private:
    String m_attribute;
    EncodedCType m_ctype;
    ExceptionCode m_exceptionCode;
};

template<typename T>
struct CTypeTraits {
    static bool decode(EncodedValue& encodedValue, T& decodedValue)
    {
        return EncodingTraits<T>::decodeValue(encodedValue, decodedValue);
    }
};

#define CREATE_CTYPE_TRAITS(_name, _type) \
template<> \
struct CTypeTraits<_type> { \
    typedef _type CType; \
    static const EncodedCType encodedType = EncodedCType::_name; \
}; \

FOR_EACH_MEMOIZED_CTYPE(CREATE_CTYPE_TRAITS)
#undef CREATE_CTYPE_TRAITS

template<typename MemoizedType>
class MemoizedDOMResult final : public MemoizedDOMResultBase {
public:
    MemoizedDOMResult(const String& attribute, typename CTypeTraits<MemoizedType>::CType result, ExceptionCode exceptionCode)
        : MemoizedDOMResultBase(attribute, CTypeTraits<MemoizedType>::encodedType, exceptionCode)
        , m_result(result) { }
    virtual ~MemoizedDOMResult() { }

    virtual EncodedValue encodedResult() const override
    {
        return EncodingTraits<MemoizedType>::encodeValue(m_result);
    }

    typename CTypeTraits<MemoizedType>::CType result() const { return m_result; }
private:
    typename CTypeTraits<MemoizedType>::CType m_result;
};

// This is used by clients of the memoized DOM result to get out the memoized
// value without performing a cast to MemoizedDOMResult<T> and calling result().
template<typename T>
bool MemoizedDOMResultBase::convertTo(T& convertedValue)
{
    // Type tag doesn't match; fail to decode the value.
    if (m_ctype != CTypeTraits<T>::encodedType)
        return false;

    MemoizedDOMResult<T>& castedResult = static_cast<MemoizedDOMResult<T>&>(*this);
    convertedValue = castedResult.result();
    return true;
}

} // namespace WebCore

using WebCore::MemoizedDOMResultBase;

namespace JSC {

template<>
struct InputTraits<MemoizedDOMResultBase> {
    static InputQueue queue() { return InputQueue::ScriptMemoizedData; }
    static const AtomicString& type();

    static void encode(EncodedValue&, const MemoizedDOMResultBase& input);
    static bool decode(EncodedValue&, std::unique_ptr<MemoizedDOMResultBase>& input);
};

} // namespace JSC

#endif // ENABLE(WEB_REPLAY)

#endif // MemoizedDOMResult_h
