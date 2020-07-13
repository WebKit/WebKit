/*
 * Copyright (C) 2012-2020 Apple Inc. All rights reserved.
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
#include "InbandTextTrackPrivateAVF.h"

#if ENABLE(VIDEO) && (USE(AVFOUNDATION) || PLATFORM(IOS_FAMILY))

#include "ISOVTTCue.h"
#include "InbandTextTrackPrivateClient.h"
#include "Logging.h"
#include "MediaPlayer.h"
#include <JavaScriptCore/ArrayBuffer.h>
#include <JavaScriptCore/DataView.h>
#include <JavaScriptCore/Int8Array.h>
#include <pal/avfoundation/MediaTimeAVFoundation.h>
#include <pal/cf/CoreMediaSoftLink.h>
#include <wtf/MediaTime.h>
#include <wtf/StringPrintStream.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

AVFInbandTrackParent::~AVFInbandTrackParent() = default;

InbandTextTrackPrivateAVF::InbandTextTrackPrivateAVF(AVFInbandTrackParent* owner, CueFormat format)
    : InbandTextTrackPrivate(format)
    , m_owner(owner)
    , m_pendingCueStatus(None)
    , m_index(0)
    , m_hasBeenReported(false)
    , m_seeking(false)
    , m_haveReportedVTTHeader(false)
{
}

InbandTextTrackPrivateAVF::~InbandTextTrackPrivateAVF()
{
    disconnect();
}

static Optional<SRGBA<uint8_t>> makeSimpleColorFromARGBCFArray(CFArrayRef colorArray)
{
    if (CFArrayGetCount(colorArray) < 4)
        return WTF::nullopt;

    float componentArray[4];
    for (int i = 0; i < 4; i++) {
        CFNumberRef value = static_cast<CFNumberRef>(CFArrayGetValueAtIndex(colorArray, i));
        if (CFGetTypeID(value) != CFNumberGetTypeID())
            return WTF::nullopt;

        float component;
        CFNumberGetValue(value, kCFNumberFloatType, &component);
        componentArray[i] = component;
    }

    return convertToComponentBytes(SRGBA { componentArray[1], componentArray[2], componentArray[3], componentArray[0] });
}

Ref<InbandGenericCue> InbandTextTrackPrivateAVF::processCueAttributes(CFAttributedStringRef attributedString)
{
    using namespace PAL;
    // Some of the attributes we translate into per-cue WebVTT settings are repeated on each part of an attributed string so only
    // process the first instance of each.
    enum AttributeFlags {
        Line = 1 << 0,
        Position = 1 << 1,
        Size = 1 << 2,
        Vertical = 1 << 3,
        Align = 1 << 4,
        FontName = 1 << 5
    };
    unsigned processed = 0;

    auto cueData = InbandGenericCue::create();
    StringBuilder content;
    String attributedStringValue = CFAttributedStringGetString(attributedString);
    CFIndex length = attributedStringValue.length();
    if (!length)
        return cueData;


    CFRange effectiveRange = CFRangeMake(0, 0);
    while ((effectiveRange.location + effectiveRange.length) < length) {

        CFDictionaryRef attributes = CFAttributedStringGetAttributes(attributedString, effectiveRange.location + effectiveRange.length, &effectiveRange);
        if (!attributes)
            continue;

        StringBuilder tagStart;
        CFStringRef valueString;
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
                valueString = static_cast<CFStringRef>(value);
                if (CFGetTypeID(valueString) != CFStringGetTypeID() || !CFStringGetLength(valueString))
                    continue;
                if (processed & Align)
                    continue;
                processed |= Align;

                if (CFStringCompare(valueString, kCMTextMarkupAlignmentType_Start, 0) == kCFCompareEqualTo)
                    cueData->setAlign(GenericCueData::Alignment::Start);
                else if (CFStringCompare(valueString, kCMTextMarkupAlignmentType_Middle, 0) == kCFCompareEqualTo)
                    cueData->setAlign(GenericCueData::Alignment::Middle);
                else if (CFStringCompare(valueString, kCMTextMarkupAlignmentType_End, 0) == kCFCompareEqualTo)
                    cueData->setAlign(GenericCueData::Alignment::End);
                else
                    ASSERT_NOT_REACHED();

                continue;
            }

            if (CFStringCompare(key, kCMTextMarkupAttribute_BoldStyle, 0) == kCFCompareEqualTo) {
                if (static_cast<CFBooleanRef>(value) != kCFBooleanTrue)
                    continue;

                tagStart.appendLiteral("<b>");
                tagEnd = "</b>" + tagEnd;
                continue;
            }

            if (CFStringCompare(key, kCMTextMarkupAttribute_ItalicStyle, 0) == kCFCompareEqualTo) {
                if (static_cast<CFBooleanRef>(value) != kCFBooleanTrue)
                    continue;

                tagStart.appendLiteral("<i>");
                tagEnd = "</i>" + tagEnd;
                continue;
            }

            if (CFStringCompare(key, kCMTextMarkupAttribute_UnderlineStyle, 0) == kCFCompareEqualTo) {
                if (static_cast<CFBooleanRef>(value) != kCFBooleanTrue)
                    continue;

                tagStart.appendLiteral("<u>");
                tagEnd = "</u>" + tagEnd;
                continue;
            }

            if (CFStringCompare(key, kCMTextMarkupAttribute_OrthogonalLinePositionPercentageRelativeToWritingDirection, 0) == kCFCompareEqualTo) {
                if (CFGetTypeID(value) != CFNumberGetTypeID())
                    continue;
                if (processed & Line)
                    continue;
                processed |= Line;

                CFNumberRef valueNumber = static_cast<CFNumberRef>(value);
                double line;
                CFNumberGetValue(valueNumber, kCFNumberFloat64Type, &line);
                cueData->setLine(line);
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
                cueData->setPosition(position);
                continue;
            }

            if (CFStringCompare(key, kCMTextMarkupAttribute_WritingDirectionSizePercentage, 0) == kCFCompareEqualTo) {
                if (CFGetTypeID(value) != CFNumberGetTypeID())
                    continue;
                if (processed & Size)
                    continue;
                processed |= Size;

                CFNumberRef valueNumber = static_cast<CFNumberRef>(value);
                double size;
                CFNumberGetValue(valueNumber, kCFNumberFloat64Type, &size);
                cueData->setSize(size);
                continue;
            }

            if (CFStringCompare(key, kCMTextMarkupAttribute_VerticalLayout, 0) == kCFCompareEqualTo) {
                valueString = static_cast<CFStringRef>(value);
                if (CFGetTypeID(valueString) != CFStringGetTypeID() || !CFStringGetLength(valueString))
                    continue;
                
                if (CFStringCompare(valueString, kCMTextVerticalLayout_LeftToRight, 0) == kCFCompareEqualTo)
                    tagStart.append(leftToRightMark);
                else if (CFStringCompare(valueString, kCMTextVerticalLayout_RightToLeft, 0) == kCFCompareEqualTo)
                    tagStart.append(rightToLeftMark);
                continue;
            }

            if (CFStringCompare(key, kCMTextMarkupAttribute_BaseFontSizePercentageRelativeToVideoHeight, 0) == kCFCompareEqualTo) {
                if (CFGetTypeID(value) != CFNumberGetTypeID())
                    continue;
                
                CFNumberRef valueNumber = static_cast<CFNumberRef>(value);
                double baseFontSize;
                CFNumberGetValue(valueNumber, kCFNumberFloat64Type, &baseFontSize);
                cueData->setBaseFontSize(baseFontSize);
                continue;
            }

            if (CFStringCompare(key, kCMTextMarkupAttribute_RelativeFontSize, 0) == kCFCompareEqualTo) {
                if (CFGetTypeID(value) != CFNumberGetTypeID())
                    continue;
                
                CFNumberRef valueNumber = static_cast<CFNumberRef>(value);
                double relativeFontSize;
                CFNumberGetValue(valueNumber, kCFNumberFloat64Type, &relativeFontSize);
                cueData->setRelativeFontSize(relativeFontSize);
                continue;
            }

            if (CFStringCompare(key, kCMTextMarkupAttribute_FontFamilyName, 0) == kCFCompareEqualTo) {
                valueString = static_cast<CFStringRef>(value);
                if (CFGetTypeID(valueString) != CFStringGetTypeID() || !CFStringGetLength(valueString))
                    continue;
                if (processed & FontName)
                    continue;
                processed |= FontName;
                
                cueData->setFontName(valueString);
                continue;
            }

            if (CFStringCompare(key, kCMTextMarkupAttribute_ForegroundColorARGB, 0) == kCFCompareEqualTo) {
                CFArrayRef arrayValue = static_cast<CFArrayRef>(value);
                if (CFGetTypeID(arrayValue) != CFArrayGetTypeID())
                    continue;
                
                auto color = makeSimpleColorFromARGBCFArray(arrayValue);
                if (!color)
                    continue;
                cueData->setForegroundColor(*color);
                continue;
            }
            
            if (CFStringCompare(key, kCMTextMarkupAttribute_BackgroundColorARGB, 0) == kCFCompareEqualTo) {
                CFArrayRef arrayValue = static_cast<CFArrayRef>(value);
                if (CFGetTypeID(arrayValue) != CFArrayGetTypeID())
                    continue;
                
                auto color = makeSimpleColorFromARGBCFArray(arrayValue);
                if (!color)
                    continue;
                cueData->setBackgroundColor(*color);
                continue;
            }

            if (CFStringCompare(key, kCMTextMarkupAttribute_CharacterBackgroundColorARGB, 0) == kCFCompareEqualTo) {
                CFArrayRef arrayValue = static_cast<CFArrayRef>(value);
                if (CFGetTypeID(arrayValue) != CFArrayGetTypeID())
                    continue;
                
                auto color = makeSimpleColorFromARGBCFArray(arrayValue);
                if (!color)
                    continue;
                cueData->setHighlightColor(*color);
                continue;
            }
        }

        content.append(tagStart);
        content.append(attributedStringValue.substring(effectiveRange.location, effectiveRange.length));
        content.append(tagEnd);
    }

    if (content.length())
        cueData->setContent(content.toString());

    return cueData;
}

void InbandTextTrackPrivateAVF::processCue(CFArrayRef attributedStrings, CFArrayRef nativeSamples, const MediaTime& time)
{
    if (!client())
        return;

    processAttributedStrings(attributedStrings, time);
    processNativeSamples(nativeSamples, time);
}

void InbandTextTrackPrivateAVF::processAttributedStrings(CFArrayRef attributedStrings, const MediaTime& time)
{
    CFIndex count = attributedStrings ? CFArrayGetCount(attributedStrings) : 0;

    if (count)
        INFO_LOG(LOGIDENTIFIER, "added ", count, count > 1 ? " cues" : " cue", " at time ", time);

    Vector<Ref<InbandGenericCue>> arrivingCues;
    if (count) {
        for (CFIndex i = 0; i < count; i++) {
            CFAttributedStringRef attributedString = static_cast<CFAttributedStringRef>(CFArrayGetValueAtIndex(attributedStrings, i));

            if (!attributedString || !CFAttributedStringGetLength(attributedString))
                continue;

            auto cueData = processCueAttributes(attributedString);
            if (!cueData->content().length())
                continue;

            cueData->setStartTime(time);
            cueData->setEndTime(MediaTime::positiveInfiniteTime());

            // AVFoundation cue "position" is to the center of the text so adjust relative to the edge because we will use it to
            // set CSS "left".
            if (cueData->position() >= 0 && cueData->size() > 0)
                cueData->setPosition(cueData->position() - cueData->size() / 2);

            cueData->setStatus(GenericCueData::Status::Partial);

            arrivingCues.append(WTFMove(cueData));
        }
    }

    if (m_pendingCueStatus != None) {
        // Cues do not have an explicit duration, they are displayed until the next "cue" (which might be empty) is emitted.
        m_currentCueEndTime = time;

        if (m_currentCueEndTime >= m_currentCueStartTime) {
            for (auto& cueData : m_cues) {
                // See if one of the newly-arrived cues is an extension of this cue.
                Vector<Ref<InbandGenericCue>> nonExtensionCues;
                for (auto& arrivingCue : arrivingCues) {
                    if (!arrivingCue->doesExtendCueData(cueData))
                        nonExtensionCues.append(WTFMove(arrivingCue));
                    else
                        INFO_LOG(LOGIDENTIFIER, "found an extension cue ", cueData.get());
                }

                bool currentCueIsExtended = (arrivingCues.size() != nonExtensionCues.size());

                arrivingCues = WTFMove(nonExtensionCues);
                
                if (currentCueIsExtended)
                    continue;

                if (m_pendingCueStatus == Valid) {
                    cueData->setEndTime(m_currentCueEndTime);
                    cueData->setStatus(GenericCueData::Status::Complete);

                    INFO_LOG(LOGIDENTIFIER, "updating cue ", cueData.get());

                    client()->updateGenericCue(cueData);
                } else {
                    // We have to assume that the implicit duration is invalid for cues delivered during a seek because the AVF decode pipeline may not
                    // see every cue, so DO NOT update cue duration while seeking.
                    INFO_LOG(LOGIDENTIFIER, "ignoring cue delivered during seek ", cueData.get());
                }
            }
        } else
            ERROR_LOG(LOGIDENTIFIER, "negative length cue(s): ", MediaTimeRange { m_currentCueStartTime, m_currentCueEndTime });

        removeCompletedCues();
    }

    if (arrivingCues.isEmpty())
        return;

    m_currentCueStartTime = time;

    for (auto& cueData : arrivingCues) {
        m_cues.append(cueData.get());
        INFO_LOG(LOGIDENTIFIER, "adding cue ", cueData.get());
        client()->addGenericCue(cueData);
    }

    m_pendingCueStatus = seeking() ? DeliveredDuringSeek : Valid;
}

void InbandTextTrackPrivateAVF::beginSeeking()
{
    // Forget any partially accumulated cue data as the seek could be to a time outside of the cue's
    // range, which will mean that the next cue delivered will result in the current cue getting the
    // incorrect duration.
    resetCueValues();
    m_seeking = true;
}

void InbandTextTrackPrivateAVF::disconnect()
{
    m_owner = 0;
    m_index = 0;
}

void InbandTextTrackPrivateAVF::removeCompletedCues()
{
    if (client()) {
        long currentCue = m_cues.size() - 1;
        for (; currentCue >= 0; --currentCue) {
            auto& cue = m_cues[currentCue];
            if (cue->status() != GenericCueData::Status::Complete)
                continue;

            INFO_LOG(LOGIDENTIFIER, "removing cue ", cue.get());

            m_cues.remove(currentCue);
        }
    }

    if (m_cues.isEmpty())
        m_pendingCueStatus = None;

    m_currentCueStartTime = MediaTime::zeroTime();
    m_currentCueEndTime = MediaTime::zeroTime();
}

void InbandTextTrackPrivateAVF::resetCueValues()
{
    if (m_currentCueEndTime && m_cues.size())
        INFO_LOG(LOGIDENTIFIER, "flushing data for cues: start = ", m_currentCueStartTime);

    if (auto* client = this->client()) {
        for (auto& cue : m_cues)
            client->removeGenericCue(cue);
    }

    m_cues.shrink(0);
    m_pendingCueStatus = None;
    m_currentCueStartTime = MediaTime::zeroTime();
    m_currentCueEndTime = MediaTime::zeroTime();
}

void InbandTextTrackPrivateAVF::setMode(InbandTextTrackPrivate::Mode newMode)
{
    if (!m_owner)
        return;

    InbandTextTrackPrivate::Mode oldMode = mode();
    InbandTextTrackPrivate::setMode(newMode);

    if (oldMode == newMode)
        return;

    m_owner->trackModeChanged();
}

void InbandTextTrackPrivateAVF::processNativeSamples(CFArrayRef nativeSamples, const MediaTime& presentationTime)
{
    using namespace PAL;

    if (!nativeSamples)
        return;

    CFIndex count = CFArrayGetCount(nativeSamples);
    if (!count)
        return;

    INFO_LOG(LOGIDENTIFIER, count, " sample buffers at time ", presentationTime);

    for (CFIndex i = 0; i < count; i++) {
        RefPtr<ArrayBuffer> buffer;
        MediaTime duration;
        CMFormatDescriptionRef formatDescription;
        if (!readNativeSampleBuffer(nativeSamples, i, buffer, duration, formatDescription))
            continue;

        auto view = JSC::DataView::create(WTFMove(buffer), 0, buffer->byteLength());
        auto peekResult = ISOBox::peekBox(view, 0);
        if (!peekResult)
            continue;

        auto type = peekResult.value().first;
        auto boxLength = peekResult.value().second;
        if (boxLength > view->byteLength()) {
            ERROR_LOG(LOGIDENTIFIER, "chunk  type = '", type.toString(), "', size = ", (size_t)boxLength, " larger than buffer length!");
            continue;
        }

        INFO_LOG(LOGIDENTIFIER, "chunk  type = '", type.toString(), "', size = ", (size_t)boxLength);

        do {
            if (m_haveReportedVTTHeader || !formatDescription)
                break;

            CFDictionaryRef extensions = CMFormatDescriptionGetExtensions(formatDescription);
            if (!extensions)
                break;

            CFDictionaryRef sampleDescriptionExtensions = static_cast<CFDictionaryRef>(CFDictionaryGetValue(extensions, kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms));
            if (!sampleDescriptionExtensions)
                break;
            
            CFDataRef webvttHeaderData = static_cast<CFDataRef>(CFDictionaryGetValue(sampleDescriptionExtensions, CFSTR("vttC")));
            if (!webvttHeaderData)
                break;

            unsigned length = CFDataGetLength(webvttHeaderData);
            if (!length)
                break;

            // A WebVTT header is terminated by "One or more WebVTT line terminators" so append two line feeds to make sure the parser
            // reccognized this string as a full header.
            StringBuilder header;
            header.appendCharacters(reinterpret_cast<const unsigned char*>(CFDataGetBytePtr(webvttHeaderData)), length);
            header.append("\n\n");

            INFO_LOG(LOGIDENTIFIER, "VTT header ", &header);
            client()->parseWebVTTFileHeader(header.toString());
            m_haveReportedVTTHeader = true;
        } while (0);

        if (type == ISOWebVTTCue::boxTypeName()) {
            ISOWebVTTCue cueData = ISOWebVTTCue(presentationTime, duration);
            cueData.read(view);
            INFO_LOG(LOGIDENTIFIER, "VTT cue data ", cueData);
            client()->parseWebVTTCueData(WTFMove(cueData));
        }

        m_sampleInputBuffer.remove(0, (size_t)boxLength);
    }
}

bool InbandTextTrackPrivateAVF::readNativeSampleBuffer(CFArrayRef nativeSamples, CFIndex index, RefPtr<ArrayBuffer>& buffer, MediaTime& duration, CMFormatDescriptionRef& formatDescription)
{
    using namespace PAL;
#if OS(WINDOWS) && HAVE(AVCFPLAYERITEM_CALLBACK_VERSION_2)
    return false;
#else
    CMSampleBufferRef sampleBuffer = reinterpret_cast<CMSampleBufferRef>(const_cast<void*>(CFArrayGetValueAtIndex(nativeSamples, index)));
    if (!sampleBuffer)
        return false;

    CMSampleTimingInfo timingInfo;
    OSStatus status = CMSampleBufferGetSampleTimingInfo(sampleBuffer, index, &timingInfo);
    if (status) {
        ERROR_LOG(LOGIDENTIFIER, "CMSampleBufferGetSampleTimingInfo returned error ", status, "' for sample ", index);
        return false;
    }

    duration = PAL::toMediaTime(timingInfo.duration);

    CMBlockBufferRef blockBuffer = CMSampleBufferGetDataBuffer(sampleBuffer);
    size_t bufferLength = CMBlockBufferGetDataLength(blockBuffer);
    if (bufferLength < ISOBox::minimumBoxSize()) {
        ERROR_LOG(LOGIDENTIFIER, "CMSampleBuffer size length unexpectedly small ", bufferLength);
        return false;
    }

    m_sampleInputBuffer.grow(m_sampleInputBuffer.size() + bufferLength);
    CMBlockBufferCopyDataBytes(blockBuffer, 0, bufferLength, m_sampleInputBuffer.data() + m_sampleInputBuffer.size() - bufferLength);

    buffer = ArrayBuffer::create(m_sampleInputBuffer.data(), m_sampleInputBuffer.size());

    formatDescription = CMSampleBufferGetFormatDescription(sampleBuffer);

    return true;
#endif
}

} // namespace WebCore

#endif // ENABLE(VIDEO) && (USE(AVFOUNDATION) || PLATFORM(IOS_FAMILY))
