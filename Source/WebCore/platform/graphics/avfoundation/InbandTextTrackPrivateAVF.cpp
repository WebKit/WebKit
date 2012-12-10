/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#if ENABLE(VIDEO) && USE(AVFOUNDATION) && HAVE(AVFOUNDATION_TEXT_TRACK_SUPPORT)

#include "InbandTextTrackPrivateAVF.h"

#include "Logging.h"
#include "MediaPlayerPrivateAVFoundation.h"
#include "SoftLinking.h"
#include <CoreMedia/CoreMedia.h>
#include <wtf/UnusedParam.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

SOFT_LINK_FRAMEWORK_OPTIONAL(CoreMedia)

SOFT_LINK_POINTER_OPTIONAL(CoreMedia, kCMTextMarkupAttribute_Alignment, CFStringRef)
SOFT_LINK_POINTER_OPTIONAL(CoreMedia, kCMTextMarkupAlignmentType_Start, CFStringRef)
SOFT_LINK_POINTER_OPTIONAL(CoreMedia, kCMTextMarkupAlignmentType_Middle, CFStringRef)
SOFT_LINK_POINTER_OPTIONAL(CoreMedia, kCMTextMarkupAlignmentType_End, CFStringRef)
SOFT_LINK_POINTER_OPTIONAL(CoreMedia, kCMTextMarkupAttribute_BoldStyle, CFStringRef)
SOFT_LINK_POINTER_OPTIONAL(CoreMedia, kCMTextMarkupAttribute_ItalicStyle, CFStringRef)
SOFT_LINK_POINTER_OPTIONAL(CoreMedia, kCMTextMarkupAttribute_UnderlineStyle, CFStringRef)
SOFT_LINK_POINTER_OPTIONAL(CoreMedia, kCMTextMarkupAttribute_TextPositionPercentageRelativeToWritingDirection, CFStringRef)
SOFT_LINK_POINTER_OPTIONAL(CoreMedia, kCMTextMarkupAttribute_WritingDirectionSizePercentage, CFStringRef)
SOFT_LINK_POINTER_OPTIONAL(CoreMedia, kCMTextMarkupAttribute_OrthogonalLinePositionPercentageRelativeToWritingDirection, CFStringRef)
SOFT_LINK_POINTER_OPTIONAL(CoreMedia, kCMTextMarkupAttribute_VerticalLayout, CFStringRef)
SOFT_LINK_POINTER_OPTIONAL(CoreMedia, kCMTextVerticalLayout_LeftToRight, CFStringRef)
SOFT_LINK_POINTER_OPTIONAL(CoreMedia, kCMTextVerticalLayout_RightToLeft, CFStringRef)

#define kCMTextMarkupAttribute_Alignment getkCMTextMarkupAttribute_Alignment()
#define kCMTextMarkupAlignmentType_Start getkCMTextMarkupAlignmentType_Start()
#define kCMTextMarkupAlignmentType_Middle getkCMTextMarkupAlignmentType_Middle()
#define kCMTextMarkupAlignmentType_End getkCMTextMarkupAlignmentType_End()
#define kCMTextMarkupAttribute_BoldStyle getkCMTextMarkupAttribute_BoldStyle()
#define kCMTextMarkupAttribute_ItalicStyle getkCMTextMarkupAttribute_ItalicStyle()
#define kCMTextMarkupAttribute_UnderlineStyle getkCMTextMarkupAttribute_UnderlineStyle()
#define kCMTextMarkupAttribute_TextPositionPercentageRelativeToWritingDirection getkCMTextMarkupAttribute_TextPositionPercentageRelativeToWritingDirection()
#define kCMTextMarkupAttribute_WritingDirectionSizePercentage getkCMTextMarkupAttribute_WritingDirectionSizePercentage()
#define kCMTextMarkupAttribute_OrthogonalLinePositionPercentageRelativeToWritingDirection getkCMTextMarkupAttribute_OrthogonalLinePositionPercentageRelativeToWritingDirection()
#define kCMTextMarkupAttribute_VerticalLayout getkCMTextMarkupAttribute_VerticalLayout()
#define kCMTextVerticalLayout_LeftToRight getkCMTextVerticalLayout_LeftToRight()
#define kCMTextVerticalLayout_RightToLeft getkCMTextVerticalLayout_RightToLeft()

using namespace std;

namespace WebCore {

InbandTextTrackPrivateAVF::InbandTextTrackPrivateAVF(MediaPlayerPrivateAVFoundation* player)
    : m_player(player)
    , m_index(0)
    , m_havePartialCue(false)
    , m_hasBeenReported(false)
{
}

InbandTextTrackPrivateAVF::~InbandTextTrackPrivateAVF()
{
    disconnect();
}

void InbandTextTrackPrivateAVF::processCueAttributes(CFAttributedStringRef attributedString, StringBuilder& content, StringBuilder& settings)
{
    // Some of the attributes we translate into per-cue WebVTT settings are are repeated on each part of an attributed string so only
    // process the first instance of each.
    enum AttributeFlags {
        Line = 1 << 0,
        Position = 1 << 1,
        Size = 1 << 2,
        Vertical = 1 << 3,
        Align = 1 << 4
    };
    unsigned processed = 0;

    String attributedStringValue = CFAttributedStringGetString(attributedString);
    CFIndex length = attributedStringValue.length();
    if (!length)
        return;

    CFRange effectiveRange = CFRangeMake(0, 0);
    while ((effectiveRange.location + effectiveRange.length) < length) {

        CFDictionaryRef attributes = CFAttributedStringGetAttributes(attributedString, effectiveRange.location + effectiveRange.length, &effectiveRange);
        if (!attributes)
            continue;

        StringBuilder tagStart;
        String tagEnd;
        CFIndex attributeCount = CFDictionaryGetCount(attributes);
        Vector<const void*> keys(attributeCount);
        Vector<const void*> values(attributeCount);
        CFDictionaryGetKeysAndValues(attributes, keys.data(), values.data());

        for (CFIndex i = 0; i < attributeCount; ++i) {
            CFStringRef key = static_cast<CFStringRef>(keys[i]);
            CFTypeRef value = values[i];
            if (CFGetTypeID(key) != CFStringGetTypeID() || !CFStringGetLength(key))
                continue;

            if (CFStringCompare(key, kCMTextMarkupAttribute_Alignment, 0) == kCFCompareEqualTo) {
                CFStringRef valueString = static_cast<CFStringRef>(value);
                if (CFGetTypeID(valueString) != CFStringGetTypeID() || !CFStringGetLength(valueString))
                    continue;
                if (processed & Align)
                    continue;
                processed |= Align;

                if (CFStringCompare(valueString, kCMTextMarkupAlignmentType_Start, 0) == kCFCompareEqualTo)
                    settings.append("align:start ");
                else if (CFStringCompare(valueString, kCMTextMarkupAlignmentType_Middle, 0) == kCFCompareEqualTo)
                    settings.append("align:middle ");
                else if (CFStringCompare(valueString, kCMTextMarkupAlignmentType_End, 0) == kCFCompareEqualTo)
                    settings.append("align:end ");
                else
                    ASSERT_NOT_REACHED();

                continue;
            }

            if (CFStringCompare(key, kCMTextMarkupAttribute_BoldStyle, 0) == kCFCompareEqualTo) {
                if (static_cast<CFBooleanRef>(value) != kCFBooleanTrue)
                    continue;

                tagStart.append("<b>");
                tagEnd.insert("</b>", 0);
                continue;
            }

            if (CFStringCompare(key, kCMTextMarkupAttribute_ItalicStyle, 0) == kCFCompareEqualTo) {
                if (static_cast<CFBooleanRef>(value) != kCFBooleanTrue)
                    continue;

                tagStart.append("<i>");
                tagEnd.insert("</i>", 0);
                continue;
            }

            if (CFStringCompare(key, kCMTextMarkupAttribute_UnderlineStyle, 0) == kCFCompareEqualTo) {
                if (static_cast<CFBooleanRef>(value) != kCFBooleanTrue)
                    continue;

                tagStart.append("<u>");
                tagEnd.insert("</u>", 0);
                continue;
            }

            if (CFStringCompare(key, kCMTextMarkupAttribute_OrthogonalLinePositionPercentageRelativeToWritingDirection, 0) == kCFCompareEqualTo) {
                // Ignore the line position if the attributes also specify "size" so we keep WebVTT's default line logic
                if (CFDictionaryGetValue(attributes, kCMTextMarkupAttribute_WritingDirectionSizePercentage))
                    continue;
                if (CFGetTypeID(value) != CFNumberGetTypeID())
                    continue;
                if (processed & Line)
                    continue;
                processed |= Line;

                CFNumberRef valueNumber = static_cast<CFNumberRef>(value);
                double position;
                CFNumberGetValue(valueNumber, kCFNumberFloat64Type, &position);
                settings.append(String::format("line:%ld%% ", lrint(position)));
                continue;
            }

            if (CFStringCompare(key, kCMTextMarkupAttribute_TextPositionPercentageRelativeToWritingDirection, 0) == kCFCompareEqualTo) {
                if (CFGetTypeID(value) != CFNumberGetTypeID())
                    continue;
                if (processed & Position)
                    continue;
                processed |= Position;

                CFNumberRef valueNumber = static_cast<CFNumberRef>(value);
                double position;
                CFNumberGetValue(valueNumber, kCFNumberFloat64Type, &position);
                settings.append(String::format("position:%ld%% ", lrint(position)));
                continue;
            }

            if (CFStringCompare(key, kCMTextMarkupAttribute_WritingDirectionSizePercentage, 0) == kCFCompareEqualTo) {
                if (CFGetTypeID(value) != CFNumberGetTypeID())
                    continue;
                if (processed & Size)
                    continue;
                processed |= Size;

                CFNumberRef valueNumber = static_cast<CFNumberRef>(value);
                double position;
                CFNumberGetValue(valueNumber, kCFNumberFloat64Type, &position);
                settings.append(String::format("size:%ld%% ", lrint(position)));
                continue;
            }
        }
        content.append(tagStart);
        content.append(attributedStringValue.substring(effectiveRange.location, effectiveRange.length));
        content.append(tagEnd);
    }
}

void InbandTextTrackPrivateAVF::processCue(CFArrayRef attributedStrings, double time)
{
    if (m_havePartialCue) {
        // Cues do not have an explicit duration, they are displayed until the next "cue" (which might be empty) is emitted.
        m_currentCueEndTime = time;
        LOG(Media, "InbandTextTrackPrivateAVF::processCue flushing cue: start=%.2f, end=%.2f, settings=\"%s\", content=\"%s\" \n",
            m_currentCueStartTime, m_currentCueEndTime,
            m_currentCueSettings.toString().utf8().data(), m_currentCueContent.toString().utf8().data());
        m_player->flushCurrentCue(this);
        resetCueValues();
    }

    CFIndex count = CFArrayGetCount(attributedStrings);
    for (CFIndex i = 0; i < count; i++) {
        CFAttributedStringRef attributedString = static_cast<CFAttributedStringRef>(CFArrayGetValueAtIndex(attributedStrings, i));

        if (!attributedString || !CFAttributedStringGetLength(attributedString))
            continue;

        processCueAttributes(attributedString, m_currentCueContent, m_currentCueSettings);
        m_currentCueStartTime = time;
        m_havePartialCue = true;
    }
}

void InbandTextTrackPrivateAVF::disconnect()
{
    m_player = 0;
    m_index = 0;
}

void InbandTextTrackPrivateAVF::resetCueValues()
{
    if (m_havePartialCue && !m_currentCueEndTime) {
        LOG(Media, "InbandTextTrackPrivateAVF::resetCueValues flushing data for cue: start=%.2f, end=%.2f, settings=\"%s\", content=\"%s\" \n",
            m_currentCueStartTime, m_currentCueEndTime, m_currentCueSettings.toString().utf8().data(), m_currentCueContent.toString().utf8().data());
    }

    m_havePartialCue = false;
    m_currentCueId = String();
    m_currentCueStartTime = 0;
    m_currentCueEndTime = 0;
    m_currentCueSettings.clear();
    m_currentCueContent.clear();
}

void InbandTextTrackPrivateAVF::setMode(InbandTextTrackPrivate::Mode newMode)
{
    InbandTextTrackPrivate::Mode oldMode = mode();
    InbandTextTrackPrivate::setMode(newMode);

    if (oldMode == newMode)
        return;

    m_player->trackModeChanged();
}

} // namespace WebCore

#endif // ENABLE(VIDEO) && USE(AVFOUNDATION) && HAVE(AVFOUNDATION_TEXT_TRACK_SUPPORT)
