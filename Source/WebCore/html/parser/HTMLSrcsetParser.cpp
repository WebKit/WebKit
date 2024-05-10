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

#include "Element.h"
#include "HTMLParserIdioms.h"
#include "ParsingUtilities.h"
#include <wtf/ListHashSet.h>
#include <wtf/URL.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

static inline bool compareByDensity(const ImageCandidate& first, const ImageCandidate& second)
{
    return first.density < second.density;
}

enum DescriptorTokenizerState {
    Initial,
    InParenthesis,
    AfterToken,
};

template<typename CharType>
static void appendDescriptorAndReset(const CharType*& descriptorStart, const CharType* position, Vector<StringView>& descriptors)
{
    if (position > descriptorStart)
        descriptors.append(StringView { std::span(descriptorStart, position) });
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
static void tokenizeDescriptors(std::span<const CharType>& position, Vector<StringView>& descriptors)
{
    DescriptorTokenizerState state = Initial;
    const CharType* descriptorsStart = position.data();
    const CharType* currentDescriptorStart = descriptorsStart;
    for (; ; position = position.subspan(1)) {
        switch (state) {
        case Initial:
            if (position.empty()) {
                appendDescriptorAndReset(currentDescriptorStart, position.data() + position.size(), descriptors);
                return;
            }
            if (isComma(position.front())) {
                appendDescriptorAndReset(currentDescriptorStart, position.data(), descriptors);
                position = position.subspan(1);
                return;
            }
            if (isASCIIWhitespace(position.front())) {
                appendDescriptorAndReset(currentDescriptorStart, position.data(), descriptors);
                currentDescriptorStart = position.data() + 1;
                state = AfterToken;
            } else if (position.front() == '(') {
                appendCharacter(currentDescriptorStart, position.data());
                state = InParenthesis;
            } else
                appendCharacter(currentDescriptorStart, position.data());
            break;
        case InParenthesis:
            if (position.empty()) {
                appendDescriptorAndReset(currentDescriptorStart, position.data() + position.size(), descriptors);
                return;
            }
            if (position.front() == ')') {
                appendCharacter(currentDescriptorStart, position.data());
                state = Initial;
            } else
                appendCharacter(currentDescriptorStart, position.data());
            break;
        case AfterToken:
            if (position.empty())
                return;
            if (!isASCIIWhitespace(position.front())) {
                state = Initial;
                currentDescriptorStart = position.data();
                position = { position.data() - 1, position.data() + position.size() };
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
        descriptor = descriptor.left(descriptorCharPosition);
        if (descriptorChar == 'x') {
            if (result.hasDensity() || result.hasHeight() || result.hasWidth())
                return false;
            std::optional<double> density = parseValidHTMLFloatingPointNumber(descriptor);
            if (!density || density.value() < 0)
                return false;
            result.setDensity(density.value());
        } else if (descriptorChar == 'w') {
            if (result.hasDensity() || result.hasWidth())
                return false;
            std::optional<int> resourceWidth = parseValidHTMLNonNegativeInteger(descriptor);
            if (!resourceWidth || resourceWidth.value() <= 0)
                return false;
            result.setResourceWidth(resourceWidth.value());
        } else if (descriptorChar == 'h') {
            // This is here only for future compat purposes.
            // The value of the 'h' descriptor is not used.
            if (result.hasDensity() || result.hasHeight())
                return false;
            std::optional<int> resourceHeight = parseValidHTMLNonNegativeInteger(descriptor);
            if (!resourceHeight || resourceHeight.value() <= 0)
                return false;
            result.setResourceHeight(resourceHeight.value());
        } else
            return false;
    }
    return !result.hasHeight() || result.hasWidth();
}

// http://picture.responsiveimages.org/#parse-srcset-attr
template<typename CharType>
static Vector<ImageCandidate> parseImageCandidatesFromSrcsetAttribute(std::span<const CharType> attribute)
{
    Vector<ImageCandidate> imageCandidates;

    while (!attribute.empty()) {
        // 4. Splitting loop: Collect a sequence of characters that are space characters or U+002C COMMA characters.
        skipWhile<isHTMLSpaceOrComma>(attribute);
        if (attribute.empty()) {
            // Contrary to spec language - descriptor parsing happens on each candidate, so when we reach the attributeEnd, we can exit.
            break;
        }
        auto* imageURLStart = attribute.data();
        // 6. Collect a sequence of characters that are not space characters, and let that be url.

        skipUntil<isASCIIWhitespace>(attribute);
        auto* imageURLEnd = attribute.data();

        DescriptorParsingResult result;

        // 8. If url ends with a U+002C COMMA character (,)
        if (isComma(*(attribute.data() - 1))) {
            // Remove all trailing U+002C COMMA characters from url.
            imageURLEnd = attribute.data() - 1;
            reverseSkipWhile<isComma>(imageURLEnd, imageURLStart);
            ++imageURLEnd;
            // If url is empty, then jump to the step labeled splitting loop.
            if (imageURLStart == imageURLEnd)
                continue;
        } else {
            skipWhile<isASCIIWhitespace>(attribute);
            Vector<StringView> descriptorTokens;
            tokenizeDescriptors(attribute, descriptorTokens);
            // Contrary to spec language - descriptor parsing happens on each candidate.
            // This is a black-box equivalent, to avoid storing descriptor lists for each candidate.
            if (!parseDescriptors(descriptorTokens, result))
                continue;
        }

        ASSERT(imageURLEnd > imageURLStart);
        unsigned imageURLLength = imageURLEnd - imageURLStart;
        imageCandidates.append(ImageCandidate(StringViewWithUnderlyingString(std::span(imageURLStart, imageURLLength), String()), result, ImageCandidate::SrcsetOrigin));
        // 11. Return to the step labeled splitting loop.
    }
    return imageCandidates;
}

Vector<ImageCandidate> parseImageCandidatesFromSrcsetAttribute(StringView attribute)
{
    // FIXME: We should consider replacing the direct pointers in the parsing process with StringView and positions.
    if (attribute.is8Bit())
        return parseImageCandidatesFromSrcsetAttribute<LChar>(attribute.span8());
    else
        return parseImageCandidatesFromSrcsetAttribute<UChar>(attribute.span16());
}

void getURLsFromSrcsetAttribute(const Element& element, StringView attribute, ListHashSet<URL>& urls)
{
    if (attribute.isEmpty())
        return;

    for (auto& candidate : parseImageCandidatesFromSrcsetAttribute(attribute)) {
        if (candidate.isEmpty())
            continue;
        URL url { element.resolveURLStringIfNeeded(candidate.string.toString()) };
        if (!url.isNull())
            urls.add(url);
    }
}

String replaceURLsInSrcsetAttribute(const Element& element, StringView attribute, const HashMap<String, String>& replacementURLStrings)
{
    if (replacementURLStrings.isEmpty())
        return attribute.toString();

    auto imageCandidates = parseImageCandidatesFromSrcsetAttribute(attribute);
    StringBuilder result;
    for (const auto& candidate : imageCandidates) {
        if (&candidate != &imageCandidates[0])
            result.append(", "_s);

        auto resolvedURLString = element.resolveURLStringIfNeeded(candidate.string.toString());
        auto replacementURLString = replacementURLStrings.get(resolvedURLString);
        if (!replacementURLString.isEmpty())
            result.append(replacementURLString);
        else
            result.append(candidate.string.toString());
        if (candidate.density != UninitializedDescriptor)
            result.append(' ', candidate.density, 'x');
        if (candidate.resourceWidth != UninitializedDescriptor)
            result.append(' ', candidate.resourceWidth, 'w');
    }

    return result.toString();
}

static ImageCandidate pickBestImageCandidate(float deviceScaleFactor, Vector<ImageCandidate>& imageCandidates, float sourceSize)
{
    bool ignoreSrc = false;
    if (imageCandidates.isEmpty())
        return { };

    // http://picture.responsiveimages.org/#normalize-source-densities
    for (auto& candidate : imageCandidates) {
        if (candidate.resourceWidth > 0) {
            candidate.density = static_cast<float>(candidate.resourceWidth) / sourceSize;
            ignoreSrc = true;
        } else if (candidate.density < 0)
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

ImageCandidate bestFitSourceForImageAttributes(float deviceScaleFactor, const AtomString& srcAttribute, StringView srcsetAttribute, float sourceSize, Function<bool(const ImageCandidate&)>&& shouldIgnoreCandidateCallback)
{
    if (srcsetAttribute.isNull()) {
        if (srcAttribute.isNull())
            return { };
        return ImageCandidate(StringViewWithUnderlyingString(srcAttribute, srcAttribute), DescriptorParsingResult(), ImageCandidate::SrcOrigin);
    }

    Vector<ImageCandidate> imageCandidates = parseImageCandidatesFromSrcsetAttribute(srcsetAttribute);

    if (!srcAttribute.isEmpty())
        imageCandidates.append(ImageCandidate(StringViewWithUnderlyingString(srcAttribute, srcAttribute), DescriptorParsingResult(), ImageCandidate::SrcOrigin));

    if (shouldIgnoreCandidateCallback)
        imageCandidates.removeAllMatching(shouldIgnoreCandidateCallback);

    return pickBestImageCandidate(deviceScaleFactor, imageCandidates, sourceSize);
}

} // namespace WebCore
