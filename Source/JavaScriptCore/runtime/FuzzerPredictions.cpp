/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "FuzzerPredictions.h"

namespace JSC {

static String readFileIntoString(const char* fileName)
{
    FILE* file = fopen(fileName, "r");
    RELEASE_ASSERT_WITH_MESSAGE(file, "Failed to open file %s", fileName);
    RELEASE_ASSERT(fseek(file, 0, SEEK_END) != -1);
    long bufferCapacity = ftell(file);
    RELEASE_ASSERT(bufferCapacity != -1);
    RELEASE_ASSERT(fseek(file, 0, SEEK_SET) != -1);
    Vector<char> buffer;
    buffer.resize(bufferCapacity);
    size_t readSize = fread(buffer.data(), 1, buffer.size(), file);
    fclose(file);
    RELEASE_ASSERT(readSize == buffer.size());
    return String(buffer.data(), buffer.size());
}

FuzzerPredictions::FuzzerPredictions(const char* filename)
{
    RELEASE_ASSERT_WITH_MESSAGE(filename, "prediction file must be specified using --fuzzerPredictionsFile=");

    String predictions = readFileIntoString(filename);
    const Vector<String>& lines = predictions.split('\n');
    for (const auto& line : lines) {
        // Predictions are stored in a text file, one prediction per line in the colon delimited format:
        // <lookup key>:<prediction in hex without leading 0x>
        // The lookup key is a pipe separated string with the format:
        // <filename>|<opcode>|<start offset>|<end offset>

        // The start and end offsets are 7-bit unsigned integers.
        // If start offset > 127, then both start and end offsets are 0.
        // If end offset > 127, then the end offset is 0.

        // Example predictions:
        // foo.js|op_construct|702|721:1000084
        // foo.js|op_call|748|760:408800
        // foo.js|op_bitnot|770|770:280000000

        // Predictions can be generated using PredictionFileCreatingFuzzerAgent.
        // Some opcodes are aliased together to make generating the predictions more straightforward.
        // For the aliases see: FileBasedFuzzerAgentBase::opcodeAliasForLookupKey()

        // FIXME: The current implementation only supports one prediction per lookup key.

        const Vector<String>& lineParts = line.split(':');
        RELEASE_ASSERT_WITH_MESSAGE(lineParts.size() == 2, "Expected line with two parts delimited by a colon. Found line with %lu parts.", lineParts.size());
        const String& lookupKey = lineParts[0];
        const String& predictionString = lineParts[1];
        bool ok;
        SpeculatedType prediction = predictionString.toUInt64Strict(&ok, 0x10);
        RELEASE_ASSERT_WITH_MESSAGE(ok, "Could not parse prediction from '%s'", predictionString.utf8().data());
        RELEASE_ASSERT(speculationChecked(prediction, SpecFullTop));
        m_predictions.set(lookupKey, prediction);
    }
}

Optional<SpeculatedType> FuzzerPredictions::predictionFor(const String& key)
{
    auto it = m_predictions.find(key);
    if (it == m_predictions.end())
        return WTF::nullopt;
    return it->value;
}

FuzzerPredictions& ensureGlobalFuzzerPredictions()
{
    static LazyNeverDestroyed<FuzzerPredictions> fuzzerPredictions;
    static std::once_flag initializeFuzzerPredictionsFlag;
    std::call_once(initializeFuzzerPredictionsFlag, [] {
        const char* fuzzerPredictionsFilename = Options::fuzzerPredictionsFile();
        fuzzerPredictions.construct(fuzzerPredictionsFilename);
    });
    return fuzzerPredictions;
}

} // namespace JSC
