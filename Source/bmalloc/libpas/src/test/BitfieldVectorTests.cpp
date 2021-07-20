/*
 * Copyright (c) 2019 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "TestHarness.h"
#include "pas_bitfield_vector.h"
#include <vector>

using namespace std;

namespace {

void testBitfieldNumWords(unsigned numFields, unsigned numBitsPerField, unsigned expectedNumWords)
{
    CHECK_EQUAL(PAS_BITFIELD_VECTOR_NUM_WORDS(numFields, numBitsPerField), expectedNumWords);
    CHECK_EQUAL(PAS_BITFIELD_VECTOR_NUM_BYTES(numFields, numBitsPerField), expectedNumWords * sizeof(unsigned));
}

void testBitfieldNumFields(unsigned numWords, unsigned numBitsPerField, unsigned expectedNumFields)
{
    CHECK_EQUAL(PAS_BITFIELD_VECTOR_NUM_FIELDS(numWords, numBitsPerField), expectedNumFields);
}

void testBitfieldWordIndex(unsigned fieldIndex, unsigned numBitsPerField, unsigned expectedWordIndex)
{
    CHECK_EQUAL(PAS_BITFIELD_VECTOR_WORD_INDEX(fieldIndex, numBitsPerField), expectedWordIndex);
}

void testBitfieldFieldIndex(unsigned wordIndex, unsigned numBitsPerField, unsigned expectedFieldIndex)
{
    CHECK_EQUAL(PAS_BITFIELD_VECTOR_FIELD_INDEX(wordIndex, numBitsPerField), expectedFieldIndex);
}

void testBitfieldFieldShift(unsigned fieldIndex, unsigned numBitsPerField, unsigned expectedFieldShift)
{
    CHECK_EQUAL(PAS_BITFIELD_VECTOR_FIELD_SHIFT(fieldIndex, numBitsPerField), expectedFieldShift);
}

class BitfieldVector {
public:
    BitfieldVector(unsigned numFields, unsigned numBitsPerField)
        : m_bits(PAS_BITFIELD_VECTOR_NUM_WORDS(numFields, numBitsPerField), 0)
        , m_numBitsPerField(numBitsPerField)
    {
    }
    
    unsigned get(unsigned fieldIndex) const
    {
        return pas_bitfield_vector_get(m_bits.data(), m_numBitsPerField, fieldIndex);
    }
    
    void set(unsigned fieldIndex, unsigned value)
    {
        pas_bitfield_vector_set(m_bits.data(), m_numBitsPerField, fieldIndex, value);
    }
private:
    vector<unsigned> m_bits;
    unsigned m_numBitsPerField;
};

void testBitfieldVectorForward(unsigned numFields, unsigned numBitsPerField)
{
    BitfieldVector myVector(numFields, numBitsPerField);
    
    for (unsigned i = 0; i < numFields; ++i)
        myVector.set(i, (i * 7) & PAS_BITFIELD_VECTOR_FIELD_MASK(numBitsPerField));
    
    for (unsigned i = 0; i < numFields; ++i)
        CHECK_EQUAL(myVector.get(i), (i * 7) & PAS_BITFIELD_VECTOR_FIELD_MASK(numBitsPerField));
}

void testBitfieldVectorBackward(unsigned numFields, unsigned numBitsPerField)
{
    BitfieldVector myVector(numFields, numBitsPerField);
    
    for (unsigned i = numFields; i--;)
        myVector.set(i, (i * 11) & PAS_BITFIELD_VECTOR_FIELD_MASK(numBitsPerField));
    
    for (unsigned i = numFields; i--;)
        CHECK_EQUAL(myVector.get(i), (i * 11) & PAS_BITFIELD_VECTOR_FIELD_MASK(numBitsPerField));
}

} // anonymous namespace

void addBitfieldVectorTests()
{
    ADD_TEST(testBitfieldNumWords(0, 1, 0));
    ADD_TEST(testBitfieldNumWords(0, 2, 0));
    ADD_TEST(testBitfieldNumWords(0, 4, 0));
    ADD_TEST(testBitfieldNumWords(0, 8, 0));
    ADD_TEST(testBitfieldNumWords(0, 16, 0));
    ADD_TEST(testBitfieldNumWords(0, 32, 0));
    ADD_TEST(testBitfieldNumWords(1, 1, 1));
    ADD_TEST(testBitfieldNumWords(1, 2, 1));
    ADD_TEST(testBitfieldNumWords(1, 4, 1));
    ADD_TEST(testBitfieldNumWords(1, 8, 1));
    ADD_TEST(testBitfieldNumWords(1, 16, 1));
    ADD_TEST(testBitfieldNumWords(1, 32, 1));
    ADD_TEST(testBitfieldNumWords(2, 1, 1));
    ADD_TEST(testBitfieldNumWords(2, 2, 1));
    ADD_TEST(testBitfieldNumWords(2, 4, 1));
    ADD_TEST(testBitfieldNumWords(2, 8, 1));
    ADD_TEST(testBitfieldNumWords(2, 16, 1));
    ADD_TEST(testBitfieldNumWords(2, 32, 2));
    ADD_TEST(testBitfieldNumWords(3, 1, 1));
    ADD_TEST(testBitfieldNumWords(3, 2, 1));
    ADD_TEST(testBitfieldNumWords(3, 4, 1));
    ADD_TEST(testBitfieldNumWords(3, 8, 1));
    ADD_TEST(testBitfieldNumWords(3, 16, 2));
    ADD_TEST(testBitfieldNumWords(3, 32, 3));
    ADD_TEST(testBitfieldNumWords(4, 1, 1));
    ADD_TEST(testBitfieldNumWords(4, 2, 1));
    ADD_TEST(testBitfieldNumWords(4, 4, 1));
    ADD_TEST(testBitfieldNumWords(4, 8, 1));
    ADD_TEST(testBitfieldNumWords(4, 16, 2));
    ADD_TEST(testBitfieldNumWords(4, 32, 4));
    ADD_TEST(testBitfieldNumWords(8, 1, 1));
    ADD_TEST(testBitfieldNumWords(8, 2, 1));
    ADD_TEST(testBitfieldNumWords(8, 4, 1));
    ADD_TEST(testBitfieldNumWords(8, 8, 2));
    ADD_TEST(testBitfieldNumWords(8, 16, 4));
    ADD_TEST(testBitfieldNumWords(8, 32, 8));
    ADD_TEST(testBitfieldNumWords(9, 1, 1));
    ADD_TEST(testBitfieldNumWords(9, 2, 1));
    ADD_TEST(testBitfieldNumWords(9, 4, 2));
    ADD_TEST(testBitfieldNumWords(9, 8, 3));
    ADD_TEST(testBitfieldNumWords(9, 16, 5));
    ADD_TEST(testBitfieldNumWords(9, 32, 9));
    ADD_TEST(testBitfieldNumWords(19, 1, 1));
    ADD_TEST(testBitfieldNumWords(19, 2, 2));
    ADD_TEST(testBitfieldNumWords(19, 4, 3));
    ADD_TEST(testBitfieldNumWords(19, 8, 5));
    ADD_TEST(testBitfieldNumWords(19, 16, 10));
    ADD_TEST(testBitfieldNumWords(19, 32, 19));
    ADD_TEST(testBitfieldNumWords(81, 1, 3));
    ADD_TEST(testBitfieldNumWords(81, 2, 6));
    ADD_TEST(testBitfieldNumWords(81, 4, 11));
    ADD_TEST(testBitfieldNumWords(81, 8, 21));
    ADD_TEST(testBitfieldNumWords(81, 16, 41));
    ADD_TEST(testBitfieldNumWords(81, 32, 81));
    ADD_TEST(testBitfieldNumWords(95, 1, 3));
    ADD_TEST(testBitfieldNumWords(95, 2, 6));
    ADD_TEST(testBitfieldNumWords(95, 4, 12));
    ADD_TEST(testBitfieldNumWords(95, 8, 24));
    ADD_TEST(testBitfieldNumWords(95, 16, 48));
    ADD_TEST(testBitfieldNumWords(95, 32, 95));
    ADD_TEST(testBitfieldNumWords(96, 1, 3));
    ADD_TEST(testBitfieldNumWords(96, 2, 6));
    ADD_TEST(testBitfieldNumWords(96, 4, 12));
    ADD_TEST(testBitfieldNumWords(96, 8, 24));
    ADD_TEST(testBitfieldNumWords(96, 16, 48));
    ADD_TEST(testBitfieldNumWords(96, 32, 96));
    ADD_TEST(testBitfieldNumWords(97, 1, 4));
    ADD_TEST(testBitfieldNumWords(97, 2, 7));
    ADD_TEST(testBitfieldNumWords(97, 4, 13));
    ADD_TEST(testBitfieldNumWords(97, 8, 25));
    ADD_TEST(testBitfieldNumWords(97, 16, 49));
    ADD_TEST(testBitfieldNumWords(97, 32, 97));
    
    ADD_TEST(testBitfieldNumFields(0, 1, 0));
    ADD_TEST(testBitfieldNumFields(0, 2, 0));
    ADD_TEST(testBitfieldNumFields(0, 4, 0));
    ADD_TEST(testBitfieldNumFields(0, 8, 0));
    ADD_TEST(testBitfieldNumFields(0, 16, 0));
    ADD_TEST(testBitfieldNumFields(0, 32, 0));
    ADD_TEST(testBitfieldNumFields(1, 1, 32));
    ADD_TEST(testBitfieldNumFields(1, 2, 16));
    ADD_TEST(testBitfieldNumFields(1, 4, 8));
    ADD_TEST(testBitfieldNumFields(1, 8, 4));
    ADD_TEST(testBitfieldNumFields(1, 16, 2));
    ADD_TEST(testBitfieldNumFields(1, 32, 1));
    ADD_TEST(testBitfieldNumFields(3, 1, 96));
    ADD_TEST(testBitfieldNumFields(3, 2, 48));
    ADD_TEST(testBitfieldNumFields(3, 4, 24));
    ADD_TEST(testBitfieldNumFields(3, 8, 12));
    ADD_TEST(testBitfieldNumFields(3, 16, 6));
    ADD_TEST(testBitfieldNumFields(3, 32, 3));
    
    ADD_TEST(testBitfieldWordIndex(0, 1, 0));
    ADD_TEST(testBitfieldWordIndex(0, 2, 0));
    ADD_TEST(testBitfieldWordIndex(0, 4, 0));
    ADD_TEST(testBitfieldWordIndex(0, 8, 0));
    ADD_TEST(testBitfieldWordIndex(0, 16, 0));
    ADD_TEST(testBitfieldWordIndex(0, 32, 0));
    ADD_TEST(testBitfieldWordIndex(1, 1, 0));
    ADD_TEST(testBitfieldWordIndex(1, 2, 0));
    ADD_TEST(testBitfieldWordIndex(1, 4, 0));
    ADD_TEST(testBitfieldWordIndex(1, 8, 0));
    ADD_TEST(testBitfieldWordIndex(1, 16, 0));
    ADD_TEST(testBitfieldWordIndex(1, 32, 1));
    ADD_TEST(testBitfieldWordIndex(2, 1, 0));
    ADD_TEST(testBitfieldWordIndex(2, 2, 0));
    ADD_TEST(testBitfieldWordIndex(2, 4, 0));
    ADD_TEST(testBitfieldWordIndex(2, 8, 0));
    ADD_TEST(testBitfieldWordIndex(2, 16, 1));
    ADD_TEST(testBitfieldWordIndex(2, 32, 2));
    ADD_TEST(testBitfieldWordIndex(3, 1, 0));
    ADD_TEST(testBitfieldWordIndex(3, 2, 0));
    ADD_TEST(testBitfieldWordIndex(3, 4, 0));
    ADD_TEST(testBitfieldWordIndex(3, 8, 0));
    ADD_TEST(testBitfieldWordIndex(3, 16, 1));
    ADD_TEST(testBitfieldWordIndex(3, 32, 3));
    ADD_TEST(testBitfieldWordIndex(4, 1, 0));
    ADD_TEST(testBitfieldWordIndex(4, 2, 0));
    ADD_TEST(testBitfieldWordIndex(4, 4, 0));
    ADD_TEST(testBitfieldWordIndex(4, 8, 1));
    ADD_TEST(testBitfieldWordIndex(4, 16, 2));
    ADD_TEST(testBitfieldWordIndex(4, 32, 4));
    ADD_TEST(testBitfieldWordIndex(95, 1, 2));
    ADD_TEST(testBitfieldWordIndex(95, 2, 5));
    ADD_TEST(testBitfieldWordIndex(95, 4, 11));
    ADD_TEST(testBitfieldWordIndex(95, 8, 23));
    ADD_TEST(testBitfieldWordIndex(95, 16, 47));
    ADD_TEST(testBitfieldWordIndex(95, 32, 95));
    ADD_TEST(testBitfieldWordIndex(96, 1, 3));
    ADD_TEST(testBitfieldWordIndex(96, 2, 6));
    ADD_TEST(testBitfieldWordIndex(96, 4, 12));
    ADD_TEST(testBitfieldWordIndex(96, 8, 24));
    ADD_TEST(testBitfieldWordIndex(96, 16, 48));
    ADD_TEST(testBitfieldWordIndex(96, 32, 96));
    ADD_TEST(testBitfieldWordIndex(97, 1, 3));
    ADD_TEST(testBitfieldWordIndex(97, 2, 6));
    ADD_TEST(testBitfieldWordIndex(97, 4, 12));
    ADD_TEST(testBitfieldWordIndex(97, 8, 24));
    ADD_TEST(testBitfieldWordIndex(97, 16, 48));
    ADD_TEST(testBitfieldWordIndex(97, 32, 97));

    ADD_TEST(testBitfieldFieldIndex(0, 1, 0));
    ADD_TEST(testBitfieldFieldIndex(0, 2, 0));
    ADD_TEST(testBitfieldFieldIndex(0, 4, 0));
    ADD_TEST(testBitfieldFieldIndex(0, 8, 0));
    ADD_TEST(testBitfieldFieldIndex(0, 16, 0));
    ADD_TEST(testBitfieldFieldIndex(0, 32, 0));
    ADD_TEST(testBitfieldFieldIndex(1, 1, 32));
    ADD_TEST(testBitfieldFieldIndex(1, 2, 16));
    ADD_TEST(testBitfieldFieldIndex(1, 4, 8));
    ADD_TEST(testBitfieldFieldIndex(1, 8, 4));
    ADD_TEST(testBitfieldFieldIndex(1, 16, 2));
    ADD_TEST(testBitfieldFieldIndex(1, 32, 1));
    ADD_TEST(testBitfieldFieldIndex(3, 1, 96));
    ADD_TEST(testBitfieldFieldIndex(3, 2, 48));
    ADD_TEST(testBitfieldFieldIndex(3, 4, 24));
    ADD_TEST(testBitfieldFieldIndex(3, 8, 12));
    ADD_TEST(testBitfieldFieldIndex(3, 16, 6));
    ADD_TEST(testBitfieldFieldIndex(3, 32, 3));
    
    ADD_TEST(testBitfieldFieldShift(0, 1, 0));
    ADD_TEST(testBitfieldFieldShift(0, 2, 0));
    ADD_TEST(testBitfieldFieldShift(0, 4, 0));
    ADD_TEST(testBitfieldFieldShift(0, 8, 0));
    ADD_TEST(testBitfieldFieldShift(0, 16, 0));
    ADD_TEST(testBitfieldFieldShift(0, 32, 0));
    ADD_TEST(testBitfieldFieldShift(1, 1, 1));
    ADD_TEST(testBitfieldFieldShift(1, 2, 2));
    ADD_TEST(testBitfieldFieldShift(1, 4, 4));
    ADD_TEST(testBitfieldFieldShift(1, 8, 8));
    ADD_TEST(testBitfieldFieldShift(1, 16, 16));
    ADD_TEST(testBitfieldFieldShift(1, 32, 0));
    ADD_TEST(testBitfieldFieldShift(2, 1, 2));
    ADD_TEST(testBitfieldFieldShift(2, 2, 4));
    ADD_TEST(testBitfieldFieldShift(2, 4, 8));
    ADD_TEST(testBitfieldFieldShift(2, 8, 16));
    ADD_TEST(testBitfieldFieldShift(2, 16, 0));
    ADD_TEST(testBitfieldFieldShift(2, 32, 0));
    ADD_TEST(testBitfieldFieldShift(3, 1, 3));
    ADD_TEST(testBitfieldFieldShift(3, 2, 6));
    ADD_TEST(testBitfieldFieldShift(3, 4, 12));
    ADD_TEST(testBitfieldFieldShift(3, 8, 24));
    ADD_TEST(testBitfieldFieldShift(3, 16, 16));
    ADD_TEST(testBitfieldFieldShift(3, 32, 0));
    ADD_TEST(testBitfieldFieldShift(4, 1, 4));
    ADD_TEST(testBitfieldFieldShift(4, 2, 8));
    ADD_TEST(testBitfieldFieldShift(4, 4, 16));
    ADD_TEST(testBitfieldFieldShift(4, 8, 0));
    ADD_TEST(testBitfieldFieldShift(4, 16, 0));
    ADD_TEST(testBitfieldFieldShift(4, 32, 0));
    ADD_TEST(testBitfieldFieldShift(95, 1, 31));
    ADD_TEST(testBitfieldFieldShift(95, 2, 30));
    ADD_TEST(testBitfieldFieldShift(95, 4, 28));
    ADD_TEST(testBitfieldFieldShift(95, 8, 24));
    ADD_TEST(testBitfieldFieldShift(95, 16, 16));
    ADD_TEST(testBitfieldFieldShift(95, 32, 0));
    ADD_TEST(testBitfieldFieldShift(96, 1, 0));
    ADD_TEST(testBitfieldFieldShift(96, 2, 0));
    ADD_TEST(testBitfieldFieldShift(96, 4, 0));
    ADD_TEST(testBitfieldFieldShift(96, 8, 0));
    ADD_TEST(testBitfieldFieldShift(96, 16, 0));
    ADD_TEST(testBitfieldFieldShift(96, 32, 0));
    ADD_TEST(testBitfieldFieldShift(97, 1, 1));
    ADD_TEST(testBitfieldFieldShift(97, 2, 2));
    ADD_TEST(testBitfieldFieldShift(97, 4, 4));
    ADD_TEST(testBitfieldFieldShift(97, 8, 8));
    ADD_TEST(testBitfieldFieldShift(97, 16, 16));
    ADD_TEST(testBitfieldFieldShift(97, 32, 0));
    
    ADD_TEST(testBitfieldVectorForward(95, 1));
    ADD_TEST(testBitfieldVectorForward(95, 2));
    ADD_TEST(testBitfieldVectorForward(95, 4));
    ADD_TEST(testBitfieldVectorForward(95, 8));
    ADD_TEST(testBitfieldVectorForward(95, 16));
    ADD_TEST(testBitfieldVectorForward(95, 32));

    ADD_TEST(testBitfieldVectorBackward(95, 1));
    ADD_TEST(testBitfieldVectorBackward(95, 2));
    ADD_TEST(testBitfieldVectorBackward(95, 4));
    ADD_TEST(testBitfieldVectorBackward(95, 8));
    ADD_TEST(testBitfieldVectorBackward(95, 16));
    ADD_TEST(testBitfieldVectorBackward(95, 32));
}

