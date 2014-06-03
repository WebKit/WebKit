/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "HTMLSrcsetParser.h"

namespace WebCore {

typedef Vector<ImageWithScale> ImageCandidates;

static inline bool compareByScaleFactor(const ImageWithScale& first, const ImageWithScale& second)
{
    return first.scaleFactor() < second.scaleFactor();
}

static bool parseDescriptors(const String& attribute, size_t start, size_t end, float& imageScaleFactor)
{
    size_t descriptorStart;
    size_t descriptorEnd;
    size_t position = start;
    bool isFoundScaleFactor = false;
    bool isEmptyDescriptor = !(end > start);
    bool isValid = false;

    while (position < end) {
        while (isHTMLSpace(attribute[position]) && position < end)
            ++position;
        descriptorStart = position;
        while (isNotHTMLSpace(attribute[position]) && position < end)
            ++position;
        descriptorEnd = position;

        // Leave if there is only whitespace at the end of the descriptors
        if (descriptorEnd <= descriptorStart)
            break;

        --descriptorEnd;
        // This part differs from the spec as the current implementation only supports pixel density descriptors.
        if (attribute[descriptorEnd] != 'x')
            continue;

        if (isFoundScaleFactor)
            return false;

        if (attribute.is8Bit())
            imageScaleFactor = charactersToFloat(attribute.characters8() + descriptorStart, descriptorEnd - descriptorStart, &isValid);
        else
            imageScaleFactor = charactersToFloat(attribute.characters16() + descriptorStart, descriptorEnd - descriptorStart, &isValid);
        isFoundScaleFactor = true;
    }

    return isEmptyDescriptor || isValid;
}

// See the specifications for more details about the algorithm to follow.
// http://www.w3.org/TR/2013/WD-html-srcset-20130228/#processing-the-image-candidates.
static void parseImagesWithScaleFromSrcsetAttribute(const String& srcsetAttribute, ImageCandidates& imageCandidates)
{
    ASSERT(imageCandidates.isEmpty());

    size_t imageCandidateStart = 0;
    unsigned srcsetAttributeLength = srcsetAttribute.length();

    while (imageCandidateStart < srcsetAttributeLength) {
        float imageScaleFactor = 1;
        size_t separator;

        // 4. Splitting loop: Skip whitespace.
        size_t imageURLStart = srcsetAttribute.find(isNotHTMLSpace, imageCandidateStart);
        if (imageURLStart == notFound)
            break;
        // If The current candidate is either totally empty or only contains space, skipping.
        if (srcsetAttribute[imageURLStart] == ',') {
            imageCandidateStart = imageURLStart + 1;
            continue;
        }
        // 5. Collect a sequence of characters that are not space characters, and let that be url.
        size_t imageURLEnd = srcsetAttribute.find(isHTMLSpace, imageURLStart + 1);
        if (imageURLEnd == notFound) {
            imageURLEnd = srcsetAttributeLength;
            separator = srcsetAttributeLength;
        } else if (srcsetAttribute[imageURLEnd - 1] == ',') {
            --imageURLEnd;
            separator = imageURLEnd;
        } else {
            // 7. Collect a sequence of characters that are not "," (U+002C) characters, and let that be descriptors.
            size_t imageScaleStart = srcsetAttribute.find(isNotHTMLSpace, imageURLEnd + 1);
            imageScaleStart = (imageScaleStart == notFound) ? srcsetAttributeLength : imageScaleStart;
            size_t imageScaleEnd = srcsetAttribute.find(',' , imageScaleStart + 1);
            imageScaleEnd = (imageScaleEnd == notFound) ? srcsetAttributeLength : imageScaleEnd;

            if (!parseDescriptors(srcsetAttribute, imageScaleStart, imageScaleEnd, imageScaleFactor)) {
                imageCandidateStart = imageScaleEnd + 1;
                continue;
            }
            separator = imageScaleEnd;
        }
        ImageWithScale image(imageURLStart, imageURLEnd - imageURLStart, imageScaleFactor);
        imageCandidates.append(image);
        // 11. Return to the step labeled splitting loop.
        imageCandidateStart = separator + 1;
    }
}

ImageWithScale bestFitSourceForImageAttributes(float deviceScaleFactor, const String& srcAttribute, const String& srcsetAttribute)
{
    ImageCandidates imageCandidates;

    parseImagesWithScaleFromSrcsetAttribute(srcsetAttribute, imageCandidates);

    if (!srcAttribute.isEmpty()) {
        ImageWithScale srcPlaceholderImage;
        imageCandidates.append(srcPlaceholderImage);
    }

    if (imageCandidates.isEmpty())
        return ImageWithScale();

    std::stable_sort(imageCandidates.begin(), imageCandidates.end(), compareByScaleFactor);

    for (size_t i = 0; i < imageCandidates.size() - 1; ++i) {
        if (imageCandidates[i].scaleFactor() >= deviceScaleFactor)
            return imageCandidates[i];
    }

    // 16. If an entry b in candidates has the same associated ... pixel density as an earlier entry a in candidates,
    // then remove entry b
    size_t winner = imageCandidates.size() - 1;
    size_t previousCandidate = winner;
    float winningScaleFactor = imageCandidates.last().scaleFactor();
    while ((previousCandidate > 0) && (imageCandidates[--previousCandidate].scaleFactor() == winningScaleFactor))
        winner = previousCandidate;

    return imageCandidates[winner];
}

} // namespace WebCore
