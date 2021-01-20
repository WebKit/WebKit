/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "Node.h"
#include <wtf/FastMalloc.h>
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/Variant.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class TypeConversions : public RefCounted<TypeConversions> {
public:
    static Ref<TypeConversions> create() { return adoptRef(*new TypeConversions()); }

    struct OtherDictionary {
        int longValue;
        String stringValue;
    };

    struct Dictionary {
        int longValue;
        String stringValue;
        String treatNullAsEmptyStringValue;
        Vector<String> sequenceValue;
        Variant<RefPtr<Node>, Vector<String>, OtherDictionary> unionValue;
        int clampLongValue;
        int enforceRangeLongValue;
    };

    int8_t testByte() { return m_byte; }
    void setTestByte(int8_t value) { m_byte = value; }
    int8_t testEnforceRangeByte() { return m_byte; }
    void setTestEnforceRangeByte(int8_t value) { m_byte = value; }
    int8_t testClampByte() { return m_byte; }
    void setTestClampByte(int8_t value) { m_byte = value; }
    uint8_t testOctet() { return m_octet; }
    void setTestOctet(uint8_t value) { m_octet = value; }
    uint8_t testEnforceRangeOctet() { return m_octet; }
    void setTestEnforceRangeOctet(uint8_t value) { m_octet = value; }
    uint8_t testClampOctet() { return m_octet; }
    void setTestClampOctet(uint8_t value) { m_octet = value; }

    int16_t testShort() { return m_short; }
    void setTestShort(int16_t value) { m_short = value; }
    int16_t testEnforceRangeShort() { return m_short; }
    void setTestEnforceRangeShort(int16_t value) { m_short = value; }
    int16_t testClampShort() { return m_short; }
    void setTestClampShort(int16_t value) { m_short = value; }
    uint16_t testUnsignedShort() { return m_unsignedShort; }
    void setTestUnsignedShort(uint16_t value) { m_unsignedShort = value; }
    uint16_t testEnforceRangeUnsignedShort() { return m_unsignedShort; }
    void setTestEnforceRangeUnsignedShort(uint16_t value) { m_unsignedShort = value; }
    uint16_t testClampUnsignedShort() { return m_unsignedShort; }
    void setTestClampUnsignedShort(uint16_t value) { m_unsignedShort = value; }

    int testLong() { return m_long; }
    void setTestLong(int value) { m_long = value; }
    int testEnforceRangeLong() { return m_long; }
    void setTestEnforceRangeLong(int value) { m_long = value; }
    int testClampLong() { return m_long; }
    void setTestClampLong(int value) { m_long = value; }
    unsigned testUnsignedLong() { return m_unsignedLong; }
    void setTestUnsignedLong(unsigned value) { m_unsignedLong = value; }
    unsigned testEnforceRangeUnsignedLong() { return m_unsignedLong; }
    void setTestEnforceRangeUnsignedLong(unsigned value) { m_unsignedLong = value; }
    unsigned testClampUnsignedLong() { return m_unsignedLong; }
    void setTestClampUnsignedLong(unsigned value) { m_unsignedLong = value; }

    long long testLongLong() { return m_longLong; }
    void setTestLongLong(long long value) { m_longLong = value; }
    long long testEnforceRangeLongLong() { return m_longLong; }
    void setTestEnforceRangeLongLong(long long value) { m_longLong = value; }
    long long testClampLongLong() { return m_longLong; }
    void setTestClampLongLong(long long value) { m_longLong = value; }
    unsigned long long testUnsignedLongLong() { return m_unsignedLongLong; }
    void setTestUnsignedLongLong(unsigned long long value) { m_unsignedLongLong = value; }
    unsigned long long testEnforceRangeUnsignedLongLong() { return m_unsignedLongLong; }
    void setTestEnforceRangeUnsignedLongLong(unsigned long long value) { m_unsignedLongLong = value; }
    unsigned long long testClampUnsignedLongLong() { return m_unsignedLongLong; }
    void setTestClampUnsignedLongLong(unsigned long long value) { m_unsignedLongLong = value; }

    const String& testString() const { return m_string; }
    void setTestString(const String& string) { m_string = string; }
    const String& testUSVString() const { return m_usvstring; }
    void setTestUSVString(const String& usvstring) { m_usvstring = usvstring; }
    const String& testByteString() const { return m_byteString; }
    void setTestByteString(const String& byteString) { m_byteString = byteString; }
    const String& testTreatNullAsEmptyString() const { return m_treatNullAsEmptyString; }
    void setTestTreatNullAsEmptyString(const String& string) { m_treatNullAsEmptyString = string; }

    const Vector<WTF::KeyValuePair<String, int>>& testLongRecord() const { return m_longRecord; }
    void setTestLongRecord(const Vector<WTF::KeyValuePair<String, int>>& value) { m_longRecord = value; }
    const Vector<WTF::KeyValuePair<String, RefPtr<Node>>>& testNodeRecord() const { return m_nodeRecord; }
    void setTestNodeRecord(const Vector<WTF::KeyValuePair<String, RefPtr<Node>>>& value) { m_nodeRecord = value; }
    const Vector<WTF::KeyValuePair<String, Vector<String>>>& testSequenceRecord() const { return m_sequenceRecord; }
    void setTestSequenceRecord(const Vector<WTF::KeyValuePair<String, Vector<String>>>& value) { m_sequenceRecord = value; }

    using TestUnion = Variant<String, int, bool, RefPtr<Node>, Vector<int>>;
    const TestUnion& testUnion() const { return m_union; }
    void setTestUnion(TestUnion&& value) { m_union = value; }

    const Dictionary& testDictionary() const { return m_testDictionary; }
    void setTestDictionary(Dictionary&& dictionary) { m_testDictionary = dictionary; }
    

    using TestClampUnion = Variant<String, int, Vector<int>>;
    const TestClampUnion& testClampUnion() const { return m_clampUnion; }
    void setTestClampUnion(const TestClampUnion& value) { m_clampUnion = value; }

    using TestEnforceRangeUnion = Variant<String, int, Vector<int>>;
    const TestEnforceRangeUnion& testEnforceRangeUnion() const { return m_enforceRangeUnion; }
    void setTestEnforceRangeUnion(const TestEnforceRangeUnion& value) { m_enforceRangeUnion = value; }

    using TestTreatNullAsEmptyStringUnion = Variant<String, int, Vector<String>>;
    const TestTreatNullAsEmptyStringUnion& testTreatNullAsEmptyStringUnion() const { return m_treatNullAsEmptyStringUnion; }
    void setTestTreatNullAsEmptyStringUnion(const TestTreatNullAsEmptyStringUnion& value) { m_treatNullAsEmptyStringUnion = value; }

    double testImpureNaNUnrestrictedDouble() const { return bitwise_cast<double>(0xffff000000000000ll); }
    double testImpureNaN2UnrestrictedDouble() const { return bitwise_cast<double>(0x7ff8000000000001ll); }
    double testQuietNaNUnrestrictedDouble() const { return std::numeric_limits<double>::quiet_NaN(); }
    double testPureNaNUnrestrictedDouble() const { return JSC::pureNaN(); }

private:
    TypeConversions() = default;

    int8_t m_byte { 0 };
    uint8_t m_octet { 0 };
    int16_t m_short { 0 };
    uint16_t m_unsignedShort { 0 };
    int m_long { 0 };
    unsigned m_unsignedLong { 0 };
    long long m_longLong { 0 };
    unsigned long long m_unsignedLongLong { 0 };
    String m_string;
    String m_usvstring;
    String m_byteString;
    String m_treatNullAsEmptyString;
    Vector<WTF::KeyValuePair<String, int>> m_longRecord;
    Vector<WTF::KeyValuePair<String, RefPtr<Node>>> m_nodeRecord;
    Vector<WTF::KeyValuePair<String, Vector<String>>> m_sequenceRecord;

    Dictionary m_testDictionary;

    TestUnion m_union;
    TestClampUnion m_clampUnion;
    TestEnforceRangeUnion m_enforceRangeUnion;
    TestTreatNullAsEmptyStringUnion m_treatNullAsEmptyStringUnion;
};

} // namespace WebCore
