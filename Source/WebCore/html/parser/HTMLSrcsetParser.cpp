/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "HTMLSrcsetParser.h"

#include "HTMLParserIdioms.h"
#include "ParsingUtilities.h"

namespace WebCore {

static inline bool compareByDensity(const ImageCandidate& first, const ImageCandidate& second)
{
    return first.density < second.density;
}

enum DescriptorTokenizerState {
    Start,
    InParenthesis,
    AfterToken,
};

template<typename CharType>
static void appendDescriptorAndReset(const CharType*& descriptorStart, const CharType* position, Vector<StringView>& descriptors)
{
    if (position > descriptorStart)
        descriptors.append(StringView(descriptorStart, position - descriptorStart));
    descriptorStart = nullptr;
}

// The following is called appendCharacter to match the spec's terminology.
template<typename CharType>
static void appendCharacter(const CharType* descriptorStart, const CharType* position)
{
    // Since we don't copy the tokens, this just set the point where the descriptor tokens start.
    if (!descriptorStart)
        descriptorStart = position;
}

template<typename CharType>
static bool isEOF(const CharType* position, const CharType* end)
{
    return position >= end;
}

template<typename CharType>
static void tokenizeDescriptors(const CharType*& position, const CharType* attributeEnd, Vector<StringView>& descriptors)
{
    DescriptorTokenizerState state = Start;
    const CharType* descriptorsStart = position;
    const CharType* currentDescriptorStart = descriptorsStart;
    for (; ; ++position) {
        switch (state) {
        case Start:
            if (isEOF(position, attributeEnd)) {
                appendDescriptorAndReset(currentDescriptorStart, attributeEnd, descriptors);
                return;
            }
            if (isComma(*position)) {
                appendDescriptorAndReset(currentDescriptorStart, position, descriptors);
                ++position;
                return;
            }
            if (isHTMLSpace(*position)) {
                appendDescriptorAndReset(currentDescriptorStart, position, descriptors);
                currentDescriptorStart = position + 1;
                state = AfterToken;
            } else if (*position == '(') {
                appendCharacter(currentDescriptorStart, position);
                state = InParenthesis;
            } else
                appendCharacter(currentDescriptorStart, position);
            break;
        case InParenthesis:
            if (isEOF(position, attributeEnd)) {
                appendDescriptorAndReset(currentDescriptorStart, attributeEnd, descriptors);
                return;
            }
            if (*position == ')') {
                appendCharacter(currentDescriptorStart, position);
                state = Start;
            } else
                appendCharacter(currentDescriptorStart, position);
            break;
        case AfterToken:
            if (isEOF(position, attributeEnd))
                return;
            if (!isHTMLSpace(*position)) {
                state = Start;
                currentDescriptorStart = position;
                --position;
            }
            break;
        }
    }
}

static bool parseDescriptors(Vector<StringView>& descriptors, DescriptorParsingResult& result)
{
    for (auto& descriptor : descriptors) {
        if (descriptor.isEmpty())
            continue;
        unsigned descriptorCharPosition = descriptor.length() - 1;
        UChar descriptorChar = descriptor[descriptorCharPosition];
        descriptor = descriptor.substring(0, descriptorCharPosition);
        bool isValid = false;
        if (descriptorChar == 'x') {
            if (result.hasDensity() || result.hasHeight() || result.hasWidth())
                return false;
            float density = descriptor.toFloat(isValid);
            if (!isValid || density < 0)
                return false;
            result.setDensity(density);
        } else if (descriptorChar == 'w') {
#if ENABLE(PICTURE_SIZES)
            if (result.hasDensity() || result.hasWidth())
                return false;
            int resourceWidth = descriptor.toInt(isValid);
            if (!isValid || resourceWidth <= 0)
                return false;
            result.setResourceWidth(resourceWidth);
#else
            return false;
#endif
        } else if (descriptorChar == 'h') {
#if ENABLE(PICTURE_SIZES)
            // This is here only for future compat purposes.
            // The value of the 'h' descriptor is not used.
            if (result.hasDensity() || result.hasHeight())
                return false;
            int resourceHeight = descriptor.toInt(isValid);
            if (!isValid || resourceHeight <= 0)
                return false;
            result.setResourceHeight(resourceHeight);
#else
            return false;
#endif
        }
    }
    return true;
}

// http://picture.responsiveimages.org/#parse-srcset-attr
template<typename CharType>
static Vector<ImageCandidate> parseImageCandidatesFromSrcsetAttribute(const CharType* attributeStart, unsigned length)
{
    Vector<ImageCandidate> imageCandidates;

    const CharType* attributeEnd = attributeStart + length;

    for (const CharType* position = attributeStart; position < attributeEnd;) {
        // 4. Splitting loop: Collect a sequence of characters that are space characters or U+002C COMMA characters.
        skipWhile<CharType, isHTMLSpaceOrComma<CharType> >(position, attributeEnd);
        if (position == attributeEnd) {
            // Contrary to spec language - descriptor parsing happens on each candidate, so when we reach the attributeEnd, we can exit.
            break;
        }
        const CharType* imageURLStart = position;
        // 6. Collect a sequence of characters that are not space characters, and let that be url.

        skipUntil<CharType, isHTMLSpace<CharType> >(position, attributeEnd);
        const CharType* imageURLEnd = position;

        DescriptorParsingResult result;

        // 8. If url ends with a U+002C COMMA character (,)
        if (isComma(*(position - 1))) {
            // Remove all trailing U+002C COMMA characters from url.
            imageURLEnd = position - 1;
            reverseSkipWhile<CharType, isComma>(imageURLEnd, imageURLStart);
            ++imageURLEnd;
            // If url is empty, then jump to the step labeled splitting loop.
            if (imageURLStart == imageURLEnd)
                continue;
        } else {
            // Advancing position here (contrary to spec) to avoid an useless extra state machine step.
            // Filed a spec bug: https://github.com/ResponsiveImagesCG/picture-element/issues/189
            ++position;
            Vector<StringView> descriptorTokens;
            tokenizeDescriptors(position, attributeEnd, descriptorTokens);
            // Contrary to spec language - descriptor parsing happens on each candidate.
            // This is a black-box equivalent, to avoid storing descriptor lists for each candidate.
            if (!parseDescriptors(descriptorTokens, result))
                continue;
        }

        ASSERT(imageURLEnd > imageURLStart);
        unsigned imageURLLength = imageURLEnd - imageURLStart;
        imageCandidates.append(ImageCandidate(StringView(imageURLStart, imageURLLength), result, ImageCandidate::SrcsetOrigin));
        // 11. Return to the step labeled splitting loop.
    }
    return imageCandidates;
}

Vector<ImageCandidate> parseImageCandidatesFromSrcsetAttribute(StringView attribute)
{
    // FIXME: We should consider replacing the direct pointers in the parsing process with StringView and positions.
    if (attribute.is8Bit())
        return parseImageCandidatesFromSrcsetAttribute<LChar>(attribute.characters8(), attribute.length());
    else
        return parseImageCandidatesFromSrcsetAttribute<UChar>(attribute.characters16(), attribute.length());
}

static ImageCandidate pickBestImageCandidate(float deviceScaleFactor, Vector<ImageCandidate>& imageCandidates
#if ENABLE(PICTURE_SIZES)
    , unsigned sourceSize
#endif
    )
{
    bool ignoreSrc = false;
    if (imageCandidates.isEmpty())
        return ImageCandidate();

    // http://picture.responsiveimages.org/#normalize-source-densities
    for (auto& candidate : imageCandidates) {
#if ENABLE(PICTURE_SIZES)
        if (candidate.resourceWidth > 0) {
            candidate.density = static_cast<float>(candidate.resourceWidth) / static_cast<float>(sourceSize);
            ignoreSrc = true;
        } else
#endif
        if (candidate.density < 0)
            candidate.density = DefaultDensityValue;
    }

    std::stable_sort(imageCandidates.begin(), imageCandidates.end(), compareByDensity);

    unsigned i;
    for (i = 0; i < imageCandidates.size() - 1; ++i) {
        if ((imageCandidates[i].density >= deviceScaleFactor) && (!ignoreSrc || !imageCandidates[i].srcOrigin()))
            break;
    }

    if (imageCandidates[i].srcOrigin() && ignoreSrc) {
        ASSERT(i > 0);
        --i;
    }
    float winningDensity = imageCandidates[i].density;

    unsigned winner = i;
    // 16. If an entry b in candidates has the same associated ... pixel density as an earlier entry a in candidates,
    // then remove entry b
    while ((i > 0) && (imageCandidates[--i].density == winningDensity))
        winner = i;

    return imageCandidates[winner];
}

ImageCandidate bestFitSourceForImageAttributes(float deviceScaleFactor, const AtomicString& srcAttribute, const AtomicString& srcsetAttribute
#if ENABLE(PICTURE_SIZES)
    , unsigned sourceSize
#endif
    )
{
    if (srcsetAttribute.isNull()) {
        if (srcAttribute.isNull())
            return ImageCandidate();
        return ImageCandidate(StringView(srcAttribute), DescriptorParsingResult(), ImageCandidate::SrcOrigin);
    }

    Vector<ImageCandidate> imageCandidates = parseImageCandidatesFromSrcsetAttribute(StringView(srcsetAttribute));

    if (!srcAttribute.isEmpty())
        imageCandidates.append(ImageCandidate(StringView(srcAttribute), DescriptorParsingResult(), ImageCandidate::SrcOrigin));

    return pickBestImageCandidate(deviceScaleFactor, imageCandidates
#if ENABLE(PICTURE_SIZES)
        , sourceSize
#endif
        );
}

} // namespace WebCore
