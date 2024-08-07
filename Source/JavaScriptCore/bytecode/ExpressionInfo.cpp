/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include "config.h"
#include "ExpressionInfo.h"

#include "VM.h"
#include <wtf/DataLog.h>
#include <wtf/StringPrintStream.h>
#include <wtf/UniqueRef.h>

namespace JSC {

/*
    Since ExpressionInfo is only used to get source position info (e.g. line and column)
    for error messages / stacks and debugging, it need not be fast. However, empirical data
    shows that there is a lot of it on websites that are especially memory hungry. So, it
    behooves us to reduce this memory burden.

    ExpressionInfo is a data structure that contains:
       a. EncodedInfo entries.
          This is where the expression info data is really stored.

       b. Chapter marks in the list of EncodedInfo entries.
          This is just an optimization aid to speed up reconstruction of expression info
          from the EncodedInfo.

       c. A LineColumnMap cache.
          This is to speed up look up of LineColumn values we have looked up before.

    Encoding of EncodedInfo words
    =============================

      Extension: [   11111b | 1b |    offset:26                                ]
      AbsInstPC: [   11111b | 0b |     value:26                                ]
      MultiWide: [   11110b | 1b |     111b | numFields:5 | fields[6]:18       ] [ value:32 ] ...
        DuoWide: [   11110b | 1b | field2:3 | value2:10 | field1:3 | value1:10 ]
     SingleWide: [   11110b | 0b |  field:3 | value:23                         ]
   ExtensionEnd: [   11110b | 0b |     111b |     0:23                         ]
          Basic: [ instPC:5 |   divot:7 | start:6 |  end:6 | line:3 | column:5 ]

    Details of what these encodings are used for follows.

    EncodedInfo
    ===========
    Abstractly, each expression info defines 6 fields of unsigned type:
    1. InstPC: bytecodeIndex offset.
    2. Divot: the execution point in an expression.
    3. Start: the offset from the Divot to the start of the expression.
    4. End: the offset from the Divot to the end of the expression.
    5. Line: the line number at the Divot.
    6. Column: the column number at the Divot.

    Let's call this an expression info entry, represented by ExpressionInfo::Entry.

    However, we know that the delta values of these fields between consecutive entries tend to be
    small. So, instead of encoding an Entry with 24 bytes, we aim to encode the info in just 4 bytes
    in an EncodedInfo word. See the encoding of the Basic word above.

    UnlinkedCodeBlockGenerator::addExpressionInfo() triggers the addition of EncodedInfo.
    Each call of addExpressionInfo() may add 1 or more EncodedInfo. It can be more than 1 because
    the delta of some of the fields may exceed the capacity afforded them in the Basic word.

    To compensate for this, we introduce Wide Encodings of the EncodedInfo word.

    Wide Encodings
    ==============
    Wide Encodings specify additional delta to add to each field value. Wide Encodings must
    eventually be followed by a Basic word. The Basic word acts as the terminator for each
    expression info entry.

    The 3 types of Wide Encodings are:

    1. SingleWide: encodes a single field value with size singleValueBits (currently 23) bits.
    2. DuoWide: encodes 2 field values with size duoValueBits (currently 10) bits each.
    3. MultiWide: encodeds up to 6 field values (because there are only 6 fields in an expression
       info record). The 1st word will be a header specifying the FieldIDs. Subsequent words are
       full 32-bit values for those respective fields.

    Extension Encodings
    ===================
    Some additional consideration: after UnlinkedCodeBlockGenerator::addExpressionInfo() is done,
    UnlinkedCodeBlockGenerator::applyModification() may insert, remove, add new bytecode due to
    generatorification. Hence, the InstPC the EncodedInfo defining each Entry needs to be adjusted
    accordingly. Similarly, we also need to adjust the Chapter marks in the EncodedInfo.

    Given that the EncodedInfo is a compressed form of the Entry fields, there may or may not be
    sufficient bits to encode the new InstPC. The straightforward implementation would be to
    insert some Wide Encodings to add additional delta for the InstPC. However, this will result
    in a lot of shifting of the EncodedInfo vector.

    For simplicity and performance (given these adjustments are more rare), rather than actually
    inserting additional EncodedInfo, we do an inplace replacement of the first EncodedInfo for the
    Entry with an Extension (see encodings above) with an offset to an extension section in the
    EncodedInfo vector (pass the end of the normal non-Extension info). This effectively behaves
    like a call to an extension routine to apply some additional EncodedInfo to adjust the InstPC
    as needed.

    The original first EncodedInfo for the Entry will be copied over as the first EncodedInfo in
    the extension section. The only exception to this is when the first EncodedInfo is a Basic
    word. By definition, the Basic word is the terminator i.e. it must be the last EncodedInfo in
    the sequence for each Entry.

    To terminate the extension island, we introduce an ExtensionEnd word that tells the decoder
    to resume decoding in the original EncodedInfo stream (effectively a return operation).
    This is needed if the replaced word is not a Basic word (which again is the terminator).
    If the replaced word is the Basic word, then we don't need to append an ExtensionEnd at the
    end of the extension and will execute the "return" operation.

    Miscellaneous details:
    1. Currently, the code expects that Extensions cannot be nested. There is no need to nest,
       and there are ASSERTs in the decoder to check for this. However, if we find a need in the
       future, this can certainly be changed.

    2. The inplace replacement works on the assumption that we can move a "complete" EncodedInfo
       over to the extension island. For all encodings, this is just a single word. The only
       exception is the MultiWide encoding, which are followed by N value words. Because the
       decoder expects these words to be contiguous, we move all the value words over too. The
       slots of the original value words will not be replaced by no-ops (SingleWide with 0).

    AbsInstPC and Chapters
    ======================
    One downside of this compressed encoding scheme is that every time we need the Entry value
    for a given InstPC, we need to decode from the start of the EncodedInfo vector to compute the
    cummulative value of all the deltas up to the Entry of interest. This can be slow if the
    EncodedInfo vector is large.

    To mitigate against this, we break the EncodedInfo regions into Chapters every
    numberOfWordsBetweenChapters (currently 10000) words or so. A Chapter is always started
    with an AbsInstPC (Absolute Instruction PC) word. Unlike other EncodedInfo words with defines
    delta values for fields to add, AbsInstPC defines an absolute value (currently 26 bits) for
    the InstPC to mark the start of the Chapter. If 26-bits isn't enough, we can always add
    SingleWide or MultiWide words to make up the difference of the InstPC for this Entry.

    Apart from resetting the cummulative InstPC to this new value, AbsInstPC also implicitly
    clears to 0 the cummulative delta for all the other fields. As a result, we can always jump
    to the start of a Chapter and start decoding from there (instead of starting from the start
    of the EncodedInfo vector). It limits the number of words that we need to decode to around
    numberOfWordsBetweenChapters.

    Now, all we need to know is which Chapter a given InstPC falls into.
 
    Finding Chapters
    ================
    For this, we have a vector of Chapter info, where each Chapter defines its starting InstPC
    and the EncodedInfo word index for the start of the first encoded Entry in that Chapter.

    The first Chapter always starts at InstPC 0 and EncodedInfo index 0. Hence, we do not emit
    a Chapter entry for the 1st Chapter. Hence, for most small functions that do not have
    more than numberOfWordsBetweenChapters EncodedInfo, there is no Chapter info in the
    ExpressionInfo.

    Additional Compression Details (of Basic word fields)
    =====================================================

    1. Column Reset on Line change

    When Line changes from the last Entry, we always reset the cummulative Column to 0. This makes
    sense because Column is relative to the start of the Line. Without this reset, the delta for
    Column may be some huge negative value (which hurts compression).

    2. Column same as Divot

    Column often has the same value as Divot. This is because a lot of websites use compacted
    and obfuscated JS code in a single line. Therefore, the column position is exactly the Divot
    position. So, the Basic word reserves the value sameAsDivotValue (currently 0b1111) as an
    indicator that the Column delta to apply is same as the Divot delta for this Entry. This
    reduces the number of Wide values we need to encode both if the Divot is large.

    3. Biased fields

    Divot, Line, and Column deltas are signed ints, not unsigned. This is because the evaluation
    of an expression isn't in sequential order. For example, consider this expression:

        foo(bar())
           ^   ^-------- Divot, Line, and Column for bar()
           `------------ Divot, Line, and Column for foo()

    The order of evaluation is first to evaluate the call to bar() followed by the call to foo().
    As a result, the Divot delta for the call to foo() is a negative value relative to the Divot
    for the call to bar(). Similarly, this is true for Line and Column.

    Since the delta values for these 3 fields are signed, instead of storing an unsigned value at
    their bitfield positions in the Basic word, we store a biased value: Divot + divotBias,
    Line + lineBias, and Column + columnBias. This maps the values into an unsigned, and makes it
    easy to do a bounds check against the max capacity of the bitfield.

    Similarly, ExpressionInfo::Diff which is used to track the delta for each field has signed int
    for these fields. This is in contrast to Entry and LineColumn where these fields are stored
    as unsigned.

    Backing Store and Shape
    =======================
    The ExpressionInfo and its backing store (with the exception of the contents of the
    LineColumnMap cache) is allocated as a contiguous slab. We first compute the size of the slab,
    then allocate it, and lastly use placement new to instantiate the ExpressionInfo.

    The shape of ExpressionInfo looks like this:

            ExpressionInfo: [ m_cachedLineColumns             ]
                            [ m_numberOfChapters              ]
                            [ m_numberOfEncodedInfo           ]
                            [ m_numberOfEncodedInfoExtensions ]
            Chapters Start: [ chapters()[0]                      ]
                            ...
                            [ chapters()[m_numberOfChapters - 1] ]
         EncodedInfo Start: [ encodedInfo()[0]                   ]
                            ...
                            [ encodedInfo()[m_numberOfEncodedInfo - 1] ]
          Extensions Start: [ encodedInfo()[m_numberOfEncodedInfo]     ]
                            ...
                            [ encodedInfo()[m_numberOfEncodedInfo + m_numberOfEncodedInfoExtensions - 1] ]
            Extensions End:
*/

struct ExpressionInfo::Diff {
    using FieldID = ExpressionInfo::FieldID;

    template<unsigned bitCount>
    void set(FieldID fieldID, unsigned value)
    {
        switch (fieldID) {
        case FieldID::InstPC:
            instPC += cast<unsigned, bitCount>(value);
            break;
        case FieldID::Divot:
            divot += cast<int, bitCount>(value);
            break;
        case FieldID::Start:
            start += cast<unsigned, bitCount>(value);
            break;
        case FieldID::End:
            end += cast<unsigned, bitCount>(value);
            break;
        case FieldID::Line:
            line += cast<int, bitCount>(value);
            break;
        case FieldID::Column:
            column += cast<int, bitCount>(value);
            break;
        }
    }

    void reset()
    {
        instPC = 0;
        divot = 0;
        start = 0;
        end = 0;
        line = 0;
        column = 0;
    }

    unsigned instPC { 0 };
    int divot { 0 };
    unsigned start { 0 };
    unsigned end { 0 };
    int line { 0 };
    int column { 0 };
};

// The type for divot, line, and column is intentionally int, not unsigned. These are
// diff values which can be negative. These asserts are just here to draw attention to
// this comment in case anyone naively changes their type.
static_assert(std::is_same_v<decltype(ExpressionInfo::Diff::divot), int>);
static_assert(std::is_same_v<decltype(ExpressionInfo::Diff::line), int>);
static_assert(std::is_same_v<decltype(ExpressionInfo::Diff::column), int>);

bool ExpressionInfo::EncodedInfo::isAbsInstPC() const
{
    if (value < (specialHeader << headerShift))
        return false;
    bool isMulti = (value >> multiBitShift) & 1;
    return !isMulti;
}

bool ExpressionInfo::EncodedInfo::isExtension() const
{
    if (value < (specialHeader << headerShift))
        return false;
    bool isMulti = (value >> multiBitShift) & 1;
    return isMulti;
}

auto ExpressionInfo::Encoder::encodeAbsInstPC(InstPC absInstPC) -> EncodedInfo
{
    unsigned word = specialHeader << headerShift
        | 0 << multiBitShift
        | (absInstPC & maxSpecialValue) << specialValueShift;
    return { word };
}

auto ExpressionInfo::Encoder::encodeExtension(unsigned offset) -> EncodedInfo
{
    RELEASE_ASSERT((offset & maxSpecialValue) == offset);
    unsigned word = specialHeader << headerShift
        | 1 << multiBitShift
        | (offset & maxSpecialValue) << specialValueShift;
    return { word };
}

auto constexpr ExpressionInfo::Encoder::encodeExtensionEnd() -> EncodedInfo
{
    unsigned word = wideHeader << headerShift
        | 0 << multiBitShift
        | static_cast<unsigned>(invalidFieldID) << firstFieldIDShift;
    return { word };
}

auto ExpressionInfo::Encoder::encodeSingle(FieldID fieldID, unsigned value) -> EncodedInfo
{
    unsigned word = wideHeader << headerShift
        | 0 << multiBitShift
        | static_cast<unsigned>(fieldID) << firstFieldIDShift
        | (value & maxSingleValue) << singleValueShift;
    return { word };
}

auto ExpressionInfo::Encoder::encodeDuo(FieldID fieldID1, unsigned value1, FieldID fieldID2, unsigned value2) -> EncodedInfo
{
    unsigned word = wideHeader << headerShift
        | 1 << multiBitShift
        | static_cast<unsigned>(fieldID1) << firstFieldIDShift
        | (value1 & maxDuoValue) << duoFirstValueShift
        | static_cast<unsigned>(fieldID2) << duoSecondFieldIDShift
        | (value2 & maxDuoValue) << duoSecondValueShift;
    return { word };
}

auto ExpressionInfo::Encoder::encodeMultiHeader(unsigned numWides, Wide* wides) -> EncodedInfo
{
    unsigned word = wideHeader << headerShift
        | 1 << multiBitShift
        | static_cast<unsigned>(invalidFieldID) << firstFieldIDShift
        | numWides << multiSizeShift;
    unsigned fieldShift = multiFirstFieldShift;
    for (unsigned i = 0; i < numWides; ++i) {
        word |= static_cast<unsigned>(wides[i].fieldID) << fieldShift;
        fieldShift -= fieldIDBits;
    }
    return { word };
}

auto ExpressionInfo::Encoder::encodeBasic(const Diff& diff) -> EncodedInfo
{
    ASSERT(diff.instPC <= maxInstPCValue);
    ASSERT(diff.start <= maxStartValue);
    ASSERT(diff.end <= maxEndValue);
    unsigned biasedDivot = diff.divot + divotBias;
    unsigned biasedLine = diff.line + lineBias;
    unsigned biasedColumn = diff.column == INT_MAX ? sameAsDivotValue : diff.column + columnBias;

    ASSERT(biasedDivot <= maxBiasedDivotValue);
    ASSERT(biasedLine <= maxBiasedLineValue);
    ASSERT(biasedColumn <= maxBiasedColumnValue || (diff.column == INT_MAX && biasedColumn == sameAsDivotValue));

    unsigned word = diff.instPC << instPCShift | biasedDivot << divotShift
        | diff.start << startShift | diff.end << endShift
        | biasedLine << lineShift | biasedColumn << columnShift;
    return { word };
}

void ExpressionInfo::Encoder::adjustInstPC(EncodedInfo* info, unsigned instPCDelta)
{
    unsigned infoIndex = info - &m_expressionInfoEncodedInfo[0];
    auto* firstInfo = info;
    unsigned firstValue = firstInfo->value;

    unsigned headerBits = firstValue >> headerShift;
    bool isMulti = (firstValue >> multiBitShift) & 1;
    unsigned firstFieldIDBits = (firstValue >> firstFieldIDShift) & fieldIDMask;

    bool isBasic = false;

    if (headerBits == specialHeader) {
        // Handle AbsInstPC.
        ASSERT(!isMulti); // firstWord cannot already be an Extension word.
        unsigned instPC = cast<unsigned, specialValueBits>(firstValue);
        unsigned updatedInstPC = instPC + instPCDelta;
        if (fits<unsigned, specialValueBits>(updatedInstPC)) {
            *firstInfo = encodeAbsInstPC(updatedInstPC);
            return;
        }
        goto emitExtension;
    }

    if (headerBits == wideHeader) {
        if (!isMulti) {
            // Handle SingleWide.
            ASSERT(firstFieldIDBits != invalidFieldID); // firstWord cannot already be an ExtensionEnd word.
            FieldID fieldID = static_cast<FieldID>(firstFieldIDBits);
            unsigned candidateInstPC = cast<unsigned, singleValueBits>(firstValue);
            unsigned updatedInstPC = candidateInstPC + instPCDelta;
            if (fieldID == FieldID::InstPC && fits<unsigned, singleValueBits>(updatedInstPC)) {
                *firstInfo = encodeSingle(FieldID::InstPC, updatedInstPC);
                return;
            }
            goto emitExtension;

        }

        if (firstFieldIDBits != invalidFieldID) {
            // Handle DuoWide.
            ASSERT(firstFieldIDBits != invalidFieldID); // firstWord cannot already be an ExtensionEnd word.
            FieldID fieldID = static_cast<FieldID>(firstFieldIDBits);
            unsigned candidateInstPC = cast<unsigned, duoValueBits>(firstValue >> duoFirstValueShift);
            unsigned updatedInstPC = candidateInstPC + instPCDelta;
            if (fieldID == FieldID::InstPC && fits<unsigned, duoValueBits>(updatedInstPC)) {
                FieldID fieldID2 = static_cast<FieldID>((firstValue >> duoSecondFieldIDShift) & fieldIDMask);
                unsigned value2 = cast<unsigned, duoValueBits>(firstValue >> duoSecondValueShift);
                *firstInfo = encodeDuo(FieldID::InstPC, updatedInstPC, fieldID2, value2);
                return;
            }
            goto emitExtension;
        }

        // Handle MultiWide.
        FieldID firstMultiFieldID = static_cast<FieldID>((firstValue >> multiFirstFieldShift) & fieldIDMask);
        if (firstMultiFieldID == FieldID::InstPC) {
            unsigned instPC = m_expressionInfoEncodedInfo[infoIndex + 1].value;
            unsigned updatedInstPC = instPC + instPCDelta;
            m_expressionInfoEncodedInfo[infoIndex + 1].value = updatedInstPC;
            return;
        }

        // We can't just move the MultiWide header to the extension: we have to move the
        // whole MultiWide record (i.e. multiple words). The Decoder relies on them to
        // being contiguous.
        unsigned numberOfFields = (firstValue >> multiSizeShift) & multiSizeMask;

        m_expressionInfoEncodedInfo.append({ firstValue }); // MultiWide header.
        for (unsigned i = 1; i < numberOfFields; ++i) {
            m_expressionInfoEncodedInfo.append(firstInfo[i]);
            firstInfo[i] = encodeSingle(FieldID::InstPC, 0); // Replace with a no-op.
        }
        // Save the last field in firstValue, and let the extension emitter below append it.
        firstValue = firstInfo[numberOfFields].value;
        firstInfo[numberOfFields] = encodeSingle(FieldID::InstPC, 0); // Replace with a no-op.
        goto emitExtension;
    }

    // Handle Basic.
    {
        unsigned instPC = cast<unsigned, instPCBits>(firstValue >> instPCShift);
        unsigned updatedInstPC = instPC + instPCDelta;
        if (updatedInstPC < maxInstPCValue) {
            unsigned replacement = firstValue & ((1u << instPCShift) - 1);
            replacement |= updatedInstPC << instPCShift;
            *firstInfo = { replacement };
            return;
        }
    }
    isBasic = true;

emitExtension:
    unsigned extensionOffset = m_expressionInfoEncodedInfo.size() - infoIndex;
    m_expressionInfoEncodedInfo[infoIndex] = encodeExtension(extensionOffset);

    // Because the Basic word is used as a terminator for the current Entry,
    // if the firstValue is a Basic word, it needs to come last. Otherwise, we should
    // just emit firstValue first. AbsInstPC and MultiWide relies on this for correctness.
    if (!isBasic)
        m_expressionInfoEncodedInfo.append({ firstValue });

    if (fits<unsigned, singleValueBits>(instPCDelta))
        m_expressionInfoEncodedInfo.append(encodeSingle(FieldID::InstPC, instPCDelta));
    else {
        // The wides array is really only to enable us to use encodeMultiHeader. Hence,
        // we don't really need to store instPCDelta as the value here. It can be any value
        // since it's not ued. However, to avoid confusion, we'll just populate it consistently.
        Wide wides[1] = { { instPCDelta, FieldID::InstPC } };
        m_expressionInfoEncodedInfo.append(encodeMultiHeader(1, wides));
        m_expressionInfoEncodedInfo.append({ instPCDelta });
    }

    if (isBasic) {
        // If we're terminating with the Basic word, then we don't need the
        // ExtensionEnd because the Basic word is an implied end.
        m_expressionInfoEncodedInfo.append({ firstValue });
    } else
        m_expressionInfoEncodedInfo.append(encodeExtensionEnd());
}

void ExpressionInfo::Encoder::encode(InstPC instPC, unsigned divot, unsigned startOffset, unsigned endOffset, LineColumn lineColumn)
{
    unsigned numWides = 0;
    std::array<Wide, 6> wides;

    auto appendWide = [&] (FieldID id, unsigned value) {
        wides[numWides++] = { value, id };
    };

    unsigned currentEncodedInfoIndex = m_expressionInfoEncodedInfo.size();
    unsigned chapterSize = currentEncodedInfoIndex - m_currentChapterStartIndex;
    if (chapterSize >= numberOfWordsBetweenChapters) {
        m_expressionInfoChapters.append({ instPC, currentEncodedInfoIndex });
        m_currentChapterStartIndex = currentEncodedInfoIndex;
        unsigned absInstPC = std::min(instPC, maxSingleValue);
        m_expressionInfoEncodedInfo.append(encodeAbsInstPC(absInstPC));
        m_entry.reset();
        m_entry.instPC = absInstPC;
    }

    Diff diff;
    diff.instPC = instPC - m_entry.instPC;
    diff.divot = divot - m_entry.divot;
    diff.start = startOffset;
    diff.end = endOffset;

    diff.line = lineColumn.line - m_entry.lineColumn.line;
    if (diff.line)
        m_entry.lineColumn.column = 0;

    diff.column = lineColumn.column - m_entry.lineColumn.column;

    bool sameDivotAndColumnDiff = diff.column == diff.divot;

    // Divot, line, and column diffs can negative values. To maximize the chance that they fit
    // in a Basic word, we apply a bias to these values. InstPC is always monotonically increasing
    // i.e. it's diff is always positive and unsigned. Start and end are already relative to divot
    // i.e. their diffs are always positive and unsigned. Hence, instPC, start, and end do not
    // require a bias.

    // Encode header:
    if (diff.instPC > maxInstPCValue) {
        appendWide(FieldID::InstPC, diff.instPC);
        diff.instPC = 0;
    }

    // Encode divot:
    if (diff.divot + divotBias > maxBiasedDivotValue) {
        appendWide(FieldID::Divot, diff.divot);
        diff.divot = 0;
    }

    // Encode start:
    if (diff.start > maxStartValue) {
        appendWide(FieldID::Start, diff.start);
        diff.start = 0;
    }

    // Encode end:
    if (diff.end > maxEndValue) {
        appendWide(FieldID::End, diff.end);
        diff.end = 0;
    }

    // Encode line:
    if (diff.line + lineBias > maxBiasedLineValue) {
        appendWide(FieldID::Line, diff.line);
        diff.line = 0;
    }

    // Encode column:
    if (sameDivotAndColumnDiff)
        diff.column = INT_MAX;
    else if (diff.column + columnBias > maxBiasedColumnValue) {
        appendWide(FieldID::Column, diff.column);
        diff.column = 0;
    }

    m_entry.instPC = instPC;
    m_entry.divot = divot;
    m_entry.lineColumn = lineColumn;

    // Canonicalize the wide EncodedInfo.
    {
        unsigned lastDuoIndex = numWides;
        unsigned numDuoWides = 0;

        // We want to process the InstPC wide (if present) last. This enables an InstPC wide to be emitted
        // first (if possible) to simplify the remap logic in adjustInst(). adjustInst() assumes that
        // the InstPC wide (if present) will likely be in the first word.
        for (unsigned i = numWides; i--; ) {
            auto& wide = wides[i];
            if (fits<duoValueBits>(wide)) {
                wide.order = Wide::SortOrder::Duo;
                numDuoWides++;
                lastDuoIndex = i;
            } else if (fits<singleValueBits>(wide))
                wide.order = Wide::SortOrder::Single;
            else
                wide.order = Wide::SortOrder::Multi;
        }

        if (numDuoWides & 1)
            wides[lastDuoIndex].order = Wide::SortOrder::Single;

        auto* widesData = wides.data();
        std::sort(widesData, widesData + numWides, [] (auto& a, auto& b) {
            if (static_cast<unsigned>(a.order) < static_cast<unsigned>(b.order))
                return true;
            if (static_cast<unsigned>(a.order) == static_cast<unsigned>(b.order))
                return a.fieldID < b.fieldID;
            return false;
        });

    }

    // Emit the wide EncodedInfo.
    for (unsigned i = 0; i < numWides; ++i) {
        auto& wide = wides[i];

        if (wide.order == Wide::SortOrder::Single) {
            m_expressionInfoEncodedInfo.append(encodeSingle(wide.fieldID, wide.value));
            continue;
        }

        if (wide.order == Wide::SortOrder::Duo) {
            auto& wide2 = wides[++i];
            ASSERT(fits<duoValueBits>(wide));
            ASSERT(fits<duoValueBits>(wide2));
            m_expressionInfoEncodedInfo.append(encodeDuo(wide.fieldID, wide.value, wide2.fieldID, wide2.value));
            continue;
        }

        ASSERT(wide.order == Wide::SortOrder::Multi);
        unsigned remainingWides = numWides - i;
        m_expressionInfoEncodedInfo.append(encodeMultiHeader(remainingWides, &wide));
        while (i < numWides)
            m_expressionInfoEncodedInfo.append({ wides[i++].value });
    }

    m_expressionInfoEncodedInfo.append(encodeBasic(diff));
}

template<unsigned bitCount>
bool ExpressionInfo::Encoder::fits(Wide wide)
{
    switch (wide.fieldID) {
    case FieldID::InstPC:
    case FieldID::Start:
    case FieldID::End:
        return fits<unsigned, bitCount>(wide.value);
    case FieldID::Divot:
    case FieldID::Line:
    case FieldID::Column:
        return fits<int, bitCount>(wide.value);
    }
    return false; // placate GCC.
}

template<typename T, unsigned bitCount>
bool ExpressionInfo::Encoder::fits(T value)
{
    struct Caster {
        T value : bitCount;
    } caster;
    caster.value = value;
    return caster.value == value;
}

MallocPtr<ExpressionInfo> ExpressionInfo::Encoder::createExpressionInfo()
{
    size_t numberOfChapters = m_expressionInfoChapters.size();
    size_t numberOfEncodedInfo = m_expressionInfoEncodedInfo.size() - m_numberOfEncodedInfoExtensions;
    size_t totalSize = ExpressionInfo::totalSizeInBytes(numberOfChapters, numberOfEncodedInfo, m_numberOfEncodedInfoExtensions);
    auto info = MallocPtr<ExpressionInfo>::malloc(totalSize);
    new (info.get()) ExpressionInfo(WTFMove(m_expressionInfoChapters), WTFMove(m_expressionInfoEncodedInfo), m_numberOfEncodedInfoExtensions);
    return info;
}

ExpressionInfo::Decoder::Decoder(const ExpressionInfo& expressionInfo)
    : m_startInfo(expressionInfo.encodedInfo())
    , m_endInfo(expressionInfo.endEncodedInfo())
    , m_endExtensionInfo(expressionInfo.endExtensionEncodedInfo())
    , m_currentInfo(m_startInfo)
    , m_nextInfo(m_startInfo)
{
}

// This constructor is only used by Encoder for remapping encodedInfo.
ExpressionInfo::Decoder::Decoder(Vector<ExpressionInfo::EncodedInfo>& encodedInfoVector)
    : m_startInfo(encodedInfoVector.begin())
    , m_endInfo(encodedInfoVector.end())
    , m_endExtensionInfo(encodedInfoVector.end())
    , m_currentInfo(m_startInfo)
    , m_nextInfo(m_startInfo)
{
}

void ExpressionInfo::Decoder::recacheInfo(Vector<ExpressionInfo::EncodedInfo>& encodedInfoVector)
{
    if (m_endInfo == encodedInfoVector.end())
        return; // Did not resize i.e nothing changed.

    m_currentInfo = &encodedInfoVector[m_currentInfo - m_startInfo];
    m_nextInfo = &encodedInfoVector[m_nextInfo - m_startInfo];

    m_endInfo = &encodedInfoVector[m_endInfo - m_startInfo];
    m_startInfo = encodedInfoVector.begin();
    m_endExtensionInfo = encodedInfoVector.end();
}

template<typename T, unsigned bitCount>
T ExpressionInfo::cast(unsigned value)
{
    struct Caster {
        T value : bitCount;
    };
    Caster caster;
    caster.value = value;
    return caster.value;
}

bool ExpressionInfo::isSpecial(unsigned value)
{
    return value >= (specialHeader << headerShift);
}

bool ExpressionInfo::isWideOrSpecial(unsigned value)
{
    return value >= (wideHeader << headerShift);
}

IterationStatus ExpressionInfo::Decoder::decode(std::optional<ExpressionInfo::InstPC> targetInstPC)
{
    m_currentInfo = m_nextInfo; // Go decode the next Entry.

    ASSERT(m_currentInfo <= m_endInfo);
    ASSERT(m_endInfo <= m_endExtensionInfo);
    auto* currentInfo = m_currentInfo;
    if (currentInfo == m_endInfo)
        return IterationStatus::Done;

    Diff diff;

    unsigned value = currentInfo->value;

    EncodedInfo* savedInfo = nullptr;
    bool hasAbsInstPC = false;
    InstPC currentInstPC = m_entry.instPC;

    // Decode wide words.
    while (isWideOrSpecial(value)) {
        bool isSpecial = ExpressionInfo::isSpecial(value);
        bool isMulti = (value >> multiBitShift) & 1;
        unsigned firstFieldIDBits = (value >> firstFieldIDShift) & fieldIDMask;

        if (isSpecial) {
            if (isMulti) {
                // Decode Extension word.
                unsigned extensionOffset = cast<unsigned, specialValueBits>(value);
                savedInfo = currentInfo;
                currentInfo += extensionOffset - 1; // -1 to compensate for the increment below.
                ASSERT(currentInfo >= m_endInfo - 1);
                ASSERT(currentInfo < m_endExtensionInfo);

            } else {
                // Decode AbsInstPC word.
                ASSERT(currentInfo == m_currentInfo);

                // We can't call m_entry.reset() here because we always scan up to the entry
                // above the one that we're looking for before declaring Done. Hence, we have
                // to defer any changes to m_entry until we know that the current entry does
                // not exceed what we're looking for, and that we can commit it.
                hasAbsInstPC = true;
                currentInstPC = 0;
                diff.reset();
                diff.set<specialValueBits>(FieldID::InstPC, value);
            }

        } else if (firstFieldIDBits == invalidFieldID && !isMulti) {
            // Decode ExtensionEnd word.
            ASSERT(savedInfo);
            currentInfo = savedInfo;
            // We need to clear savedInfo to indicate that we terminated the Extension with
            // ExtensionEnd. Otherwise, we need to restore currentInfo after we decode the Basic word
            // terminator.
            savedInfo = nullptr;

        } else if (firstFieldIDBits == invalidFieldID) {
            // Decode MultiWide word.
            unsigned numberOfFields = (value >> multiSizeShift) & multiSizeMask;
            unsigned fieldShift = multiFirstFieldShift;
            for (unsigned i = 0; i < numberOfFields; ++i) {
                ++currentInfo;
                auto fieldID = static_cast<FieldID>((value >> fieldShift) & fieldIDMask);
                diff.set<fullValueBits>(fieldID, currentInfo->value);
                fieldShift -= fieldIDBits;
            }

        } else if (isMulti) {
            // Decode DuoWide word.
            auto fieldID1 = static_cast<FieldID>(firstFieldIDBits);
            auto fieldID2 = static_cast<FieldID>((value >> duoSecondFieldIDShift) & fieldIDMask);
            diff.set<duoValueBits>(fieldID1, value >> duoFirstValueShift);
            diff.set<duoValueBits>(fieldID2, value >> duoSecondValueShift);

        } else {
            // Decode SingleWide word.
            diff.set<singleValueBits>(static_cast<FieldID>(firstFieldIDBits), value);
        }

        ++currentInfo;
        value = currentInfo->value;
    }

    // Decode Basic word.
    // We check the bounds against m_endExtensionInfo here because the Basic word may be in
    // the extensions section.
    ASSERT(currentInfo < m_endExtensionInfo);

    diff.instPC += value >> instPCShift;
    currentInstPC += diff.instPC;

    IterationStatus status = IterationStatus::Continue;

    // We want to find the entry whose InstPC is below the targetInstPC but not to exceed it.
    // This means by necessity, we must always decode the next one above it before we
    // know that we're done. If the current decode exceeds the target, then we need to
    // abort immediately and not commit any changes to m_entry.
    //
    // The only exception to this is that we need to at least decode 1 entry before
    // calling it quits. This only applies to opcodes at the start of the function before
    // the first ExpressionInfo entry. Our historical convention is to map those to the
    // first entry. The m_hasDecodedFirstEntry flag helps us achieve this.

    if (targetInstPC && m_hasDecodedFirstEntry && currentInstPC > targetInstPC.value()) {
        // We're done because we have reached our targetInstPC.
        status = IterationStatus::Done;

    } else {
        m_hasDecodedFirstEntry = true;

        if (hasAbsInstPC)
            m_entry.reset();
        m_entry.instPC = currentInstPC;

        diff.divot += cast<int, divotBits>((value >> divotShift) - divotBias);
        m_entry.divot += diff.divot;

        // Unlike other values, startOffset and endOffset are always relative
        // to the divot. Hence, they are never cummulative relative to the last expression
        // info entry.
        static constexpr unsigned startMask = (1 << startBits) - 1;
        diff.start += (value >> startShift) & startMask;
        m_entry.startOffset = diff.start; // Not cummulative.

        static constexpr unsigned endMask = (1 << endBits) - 1;
        diff.end += (value >> endShift) & endMask;
        m_entry.endOffset = diff.end; // Not cummulative.

        diff.line += cast<int, lineBits>((value >> lineShift) - lineBias);
        if (diff.line)
            m_entry.lineColumn.column = 0;
        m_entry.lineColumn.line += diff.line;

        static constexpr unsigned columnMask = (1 << columnBits) - 1;

        unsigned columnField = (value >> columnShift) & columnMask;
        diff.column += columnField == sameAsDivotValue ? diff.divot : cast<int, columnBits>(columnField - columnBias);
        m_entry.lineColumn.column += diff.column;
    }

    if (savedInfo) {
        // We got here because we are terminating an Extension with a Basic word.
        // So, we have to restore currentInfo for the next Entry.
        currentInfo = savedInfo;
    }

    m_nextInfo = ++currentInfo; // This is where the next Entry to decode will start.
    return status;
}

MallocPtr<ExpressionInfo> ExpressionInfo::createUninitialized(unsigned numberOfChapters, unsigned numberOfEncodedInfo, unsigned numberOfEncodedInfoExtensions)
{
    size_t totalSize = ExpressionInfo::totalSizeInBytes(numberOfChapters, numberOfEncodedInfo, numberOfEncodedInfoExtensions);
    auto info = MallocPtr<ExpressionInfo>::malloc(totalSize);
    new (info.get()) ExpressionInfo(numberOfChapters, numberOfEncodedInfo, numberOfEncodedInfoExtensions);
    return info;
}

ExpressionInfo::ExpressionInfo(unsigned numberOfChapters, unsigned numberOfEncodedInfo, unsigned numberOfEncodedInfoExtensions)
    : m_numberOfChapters(numberOfChapters)
    , m_numberOfEncodedInfo(numberOfEncodedInfo)
    , m_numberOfEncodedInfoExtensions(numberOfEncodedInfoExtensions)
{
}

ExpressionInfo::ExpressionInfo(Vector<Chapter>&& chapters, Vector<EncodedInfo>&& encodedInfo, unsigned numberOfEncodedInfoExtensions)
    : ExpressionInfo(chapters.size(), encodedInfo.size() - numberOfEncodedInfoExtensions, numberOfEncodedInfoExtensions)
{
    std::uninitialized_copy(chapters.begin(), chapters.end(), this->chapters());
    std::uninitialized_copy(encodedInfo.begin(), encodedInfo.end(), this->encodedInfo());
}

size_t ExpressionInfo::byteSize() const
{
    return totalSizeInBytes(m_numberOfChapters, m_numberOfEncodedInfo, m_numberOfEncodedInfoExtensions);
}

auto ExpressionInfo::lineColumnForInstPC(InstPC instPC) -> LineColumn
{
    auto iter = m_cachedLineColumns.find(instPC);
    if (iter != m_cachedLineColumns.end())
        return iter->value;

    auto entry = entryForInstPC(instPC);
    m_cachedLineColumns.add(instPC, entry.lineColumn);
    return entry.lineColumn;
}

auto ExpressionInfo::findChapterEncodedInfoJustBelow(InstPC instPC) const -> EncodedInfo*
{
    auto* chapters = this->chapters();
    unsigned low = 0;
    unsigned high = m_numberOfChapters;
    while (low < high) {
        unsigned mid = (low + high) / 2;
        if (chapters[mid].startInstPC <= instPC)
            low = mid + 1;
        else
            high = mid;
    }

    unsigned startIndex = 0;
    unsigned endIndex = m_numberOfEncodedInfo;

    if (low) {
        auto& chapter = chapters[low - 1];
        startIndex = chapter.startEncodedInfoIndex;
    }
    if (low + 1 < m_numberOfChapters)
        endIndex = chapters[low].startEncodedInfoIndex;

    ASSERT_UNUSED(endIndex, startIndex <= endIndex);
    return &encodedInfo()[startIndex];
}

auto ExpressionInfo::entryForInstPC(InstPC instPC) -> Entry
{
    Decoder decoder(*this);

    auto* chapterStart = findChapterEncodedInfoJustBelow(instPC);
    decoder.setNextInfo(chapterStart);
    while (decoder.decode(instPC) != IterationStatus::Done) { }
    return decoder.entry();
}

template<unsigned bitCount>
void ExpressionInfo::print(PrintStream& out, FieldID fieldID, unsigned value)
{
    switch (fieldID) {
    case FieldID::InstPC:
    case FieldID::Start:
    case FieldID::End:
        out.print(cast<unsigned, bitCount>(value));
        break;
    case FieldID::Divot:
    case FieldID::Line:
    case FieldID::Column:
        out.print(cast<int, bitCount>(value));
        break;
    }
};


void ExpressionInfo::dumpEncodedInfo(ExpressionInfo::EncodedInfo* start, ExpressionInfo::EncodedInfo* end)
{
    StringPrintStream out;

    EncodedInfo* curr = start;
    for (unsigned index = 0; curr < end; ++index) {
        unsigned value = curr->value;
        out.print("  [", index, "] ", RawPointer(curr), ": ", RawHex(value));

        if (isWideOrSpecial(value)) {
            bool isSpecial = ExpressionInfo::isSpecial(value);
            bool isMulti = (value >> multiBitShift) & 1;
            unsigned firstFieldIDBits = (value >> firstFieldIDShift) & fieldIDMask;

            if (isSpecial) {
                unsigned payload = cast<unsigned, specialValueBits>(value);
                if (isMulti)
                    out.println(" EXT ", payload); // Extension word.
                else
                    out.println(" ABS ", payload); // AbsInstPC word.

            } else if (firstFieldIDBits == invalidFieldID && !isMulti) {
                out.println(" XND"); // ExtensionEnd word.

            } else if (firstFieldIDBits == invalidFieldID) {
                // MultiWide word.
                unsigned numberOfFields = (value >> multiSizeShift) & multiSizeMask;
                unsigned fieldShift = multiFirstFieldShift;

                out.print(" MLT ", RawHex(value), " numFields ", numberOfFields);
                for (unsigned i = 1; i <= numberOfFields; ++i) {
                    auto fieldID = static_cast<FieldID>((value >> fieldShift) & fieldIDMask);
                    out.print(" | ", fieldID);
                    fieldShift -= fieldIDBits;
                }
                out.println();

                // MutiWide fields.
                fieldShift = multiFirstFieldShift;
                for (unsigned i = 0; i < numberOfFields; ++i) {
                    auto fieldID = static_cast<FieldID>((value >> fieldShift) & fieldIDMask);
                    out.print("    [", i, "] ", RawPointer(&curr[i + 1]), ": ", fieldID, " ", curr[i + 1].value);
                    fieldShift -= fieldIDBits;
                }
                curr += numberOfFields;
                index += numberOfFields;

            } else if (isMulti) {
                // DuoWide word.
                auto fieldID1 = static_cast<FieldID>(firstFieldIDBits);
                auto fieldID2 = static_cast<FieldID>((value >> duoSecondFieldIDShift) & fieldIDMask);

                out.print(" DUO ", fieldID1, " ");
                print<duoValueBits>(out, fieldID1, value >> duoFirstValueShift);
                out.print(" ", fieldID2, " ");
                print<duoValueBits>(out, fieldID2, value >> duoFirstValueShift);
                out.println();

            } else {
                // SingleWide word.
                auto fieldID = static_cast<FieldID>(firstFieldIDBits);

                out.print(" SNG ", fieldID, " ");
                print<singleValueBits>(out, fieldID, value);
                out.println();
            }
        } else {
            out.println(" BSC ",
                FieldID::InstPC, " ", cast<unsigned, instPCBits>(value >> instPCShift), " ",
                FieldID::Divot, " ", cast<unsigned, divotBits>(value >> divotShift), " ",
                FieldID::Start, " ", cast<unsigned, startBits>(value >> startShift), " ",
                FieldID::End, " ", cast<unsigned, endBits>(value >> endShift), " ",
                FieldID::Line, " ", cast<unsigned, lineBits>(value >> lineShift), " ",
                FieldID::Column, " ", cast<unsigned, columnBits>(value >> columnShift));
        }
        curr++;
    }
    dataLogLn(out.toString());
}

} // namespace JSC
