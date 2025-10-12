/*
 * Copyright (C) 2012-2025 Apple Inc. All rights reserved.
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
#include <wtf/MediaTime.h>
#include <wtf/StringPrintStream.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/cf/TypeCastsCF.h>
#include <wtf/cf/VectorCF.h>
#include <wtf/text/CString.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>
#include <wtf/unicode/CharacterNames.h>

#include <pal/cf/CoreMediaSoftLink.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(InbandTextTrackPrivateAVF);

InbandTextTrackPrivateAVF::InbandTextTrackPrivateAVF(TrackID trackID, CueFormat format, ModeChangedCallback&& callback)
    : InbandTextTrackPrivate(format)
    , m_modeChangedCallback(WTFMove(callback))
    , m_pendingCueStatus(None)
    , m_index(0)
    , m_trackID(trackID)
    , m_hasBeenReported(false)
    , m_seeking(false)
    , m_haveReportedVTTHeader(false)
{
}

InbandTextTrackPrivateAVF::~InbandTextTrackPrivateAVF()
{
    disconnect();
}

static std::optional<SRGBA<uint8_t>> makeSimpleColorFromARGBCFArray(CFArrayRef colorArray)
{
    if (CFArrayGetCount(colorArray) < 4)
        return std::nullopt;

    std::array<float, 4> componentArray;
    for (int i = 0; i < 4; ++i) {
        auto value = dynamic_cf_cast<CFNumberRef>(CFArrayGetValueAtIndex(colorArray, i));
        if (!value)
            return std::nullopt;

        float component;
        CFNumberGetValue(value, kCFNumberFloatType, &component);
        componentArray[i] = component;
    }

    return convertColor<SRGBA<uint8_t>>(SRGBA<float> { componentArray[1], componentArray[2], componentArray[3], componentArray[0] });
}

Ref<InbandGenericCue> InbandTextTrackPrivateAVF::processCueAttributes(CFAttributedStringRef attributedString)
{
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
        String tagEnd;
        CFIndex attributeCount = CFDictionaryGetCount(attributes);
        Vector<const void*> keys(attributeCount);
        Vector<const void*> values(attributeCount);
        CFDictionaryGetKeysAndValues(attributes, keys.mutableSpan().data(), values.mutableSpan().data());

        for (CFIndex i = 0; i < attributeCount; ++i) {
            auto key = dynamic_cf_cast<CFStringRef>(keys[i]);
            CFTypeRef value = values[i];
            if (!key || !CFStringGetLength(key))
                continue;

            if (CFStringCompare(key, PAL::kCMTextMarkupAttribute_Alignment, 0) == kCFCompareEqualTo) {
                auto valueString = dynamic_cf_cast<CFStringRef>(value);
                if (!valueString || !CFStringGetLength(valueString))
                    continue;
                if (processed & Align)
                    continue;
                processed |= Align;

                if (CFStringCompare(valueString, PAL::kCMTextMarkupAlignmentType_Start, 0) == kCFCompareEqualTo)
                    cueData->setAlign(GenericCueData::Alignment::Start);
                else if (CFStringCompare(valueString, PAL::kCMTextMarkupAlignmentType_Middle, 0) == kCFCompareEqualTo)
                    cueData->setAlign(GenericCueData::Alignment::Middle);
                else if (CFStringCompare(valueString, PAL::kCMTextMarkupAlignmentType_End, 0) == kCFCompareEqualTo)
                    cueData->setAlign(GenericCueData::Alignment::End);
                else
                    ASSERT_NOT_REACHED();

                continue;
            }

            if (CFStringCompare(key, PAL::kCMTextMarkupAttribute_BoldStyle, 0) == kCFCompareEqualTo) {
                if (value != kCFBooleanTrue)
                    continue;

                tagStart.append("<b>"_s);
                tagEnd = makeString("</b>"_s, tagEnd);
                continue;
            }

            if (CFStringCompare(key, PAL::kCMTextMarkupAttribute_ItalicStyle, 0) == kCFCompareEqualTo) {
                if (value != kCFBooleanTrue)
                    continue;

                tagStart.append("<i>"_s);
                tagEnd = makeString("</i>"_s, tagEnd);
                continue;
            }

            if (CFStringCompare(key, PAL::kCMTextMarkupAttribute_UnderlineStyle, 0) == kCFCompareEqualTo) {
                if (value != kCFBooleanTrue)
                    continue;

                tagStart.append("<u>"_s);
                tagEnd = makeString("</u>"_s, tagEnd);
                continue;
            }

            if (CFStringCompare(key, PAL::kCMTextMarkupAttribute_OrthogonalLinePositionPercentageRelativeToWritingDirection, 0) == kCFCompareEqualTo) {
                auto valueNumber = dynamic_cf_cast<CFNumberRef>(value);
                if (!valueNumber)
                    continue;
                if (processed & Line)
                    continue;
                processed |= Line;

                double line;
                CFNumberGetValue(valueNumber, kCFNumberFloat64Type, &line);
                cueData->setLine(line);
                continue;
            }

            if (CFStringCompare(key, PAL::kCMTextMarkupAttribute_TextPositionPercentageRelativeToWritingDirection, 0) == kCFCompareEqualTo) {
                auto valueNumber = dynamic_cf_cast<CFNumberRef>(value);
                if (!valueNumber)
                    continue;
                if (processed & Position)
                    continue;
                processed |= Position;

                double position;
                CFNumberGetValue(valueNumber, kCFNumberFloat64Type, &position);
                cueData->setPosition(position);
                continue;
            }

            if (CFStringCompare(key, PAL::kCMTextMarkupAttribute_WritingDirectionSizePercentage, 0) == kCFCompareEqualTo) {
                auto valueNumber = dynamic_cf_cast<CFNumberRef>(value);
                if (!valueNumber)
                    continue;
                if (processed & Size)
                    continue;
                processed |= Size;

                double size;
                CFNumberGetValue(valueNumber, kCFNumberFloat64Type, &size);
                cueData->setSize(size);
                continue;
            }

            if (CFStringCompare(key, PAL::kCMTextMarkupAttribute_VerticalLayout, 0) == kCFCompareEqualTo) {
                auto valueString = dynamic_cf_cast<CFStringRef>(value);
                if (!valueString || !CFStringGetLength(valueString))
                    continue;
                
                if (CFStringCompare(valueString, PAL::kCMTextVerticalLayout_LeftToRight, 0) == kCFCompareEqualTo)
                    tagStart.append(leftToRightMark);
                else if (CFStringCompare(valueString, PAL::kCMTextVerticalLayout_RightToLeft, 0) == kCFCompareEqualTo)
                    tagStart.append(rightToLeftMark);
                continue;
            }

            if (CFStringCompare(key, PAL::kCMTextMarkupAttribute_BaseFontSizePercentageRelativeToVideoHeight, 0) == kCFCompareEqualTo) {
                auto valueNumber = dynamic_cf_cast<CFNumberRef>(value);
                if (!valueNumber)
                    continue;

                double baseFontSize;
                CFNumberGetValue(valueNumber, kCFNumberFloat64Type, &baseFontSize);
                cueData->setBaseFontSize(baseFontSize);
                continue;
            }

            if (CFStringCompare(key, PAL::kCMTextMarkupAttribute_RelativeFontSize, 0) == kCFCompareEqualTo) {
                auto valueNumber = dynamic_cf_cast<CFNumberRef>(value);
                if (!valueNumber)
                    continue;

                double relativeFontSize;
                CFNumberGetValue(valueNumber, kCFNumberFloat64Type, &relativeFontSize);
                cueData->setRelativeFontSize(relativeFontSize);
                continue;
            }

            if (CFStringCompare(key, PAL::kCMTextMarkupAttribute_FontFamilyName, 0) == kCFCompareEqualTo) {
                auto valueString = dynamic_cf_cast<CFStringRef>(value);
                if (!valueString || !CFStringGetLength(valueString))
                    continue;
                if (processed & FontName)
                    continue;
                processed |= FontName;
                
                cueData->setFontName(valueString);
                continue;
            }

            if (CFStringCompare(key, PAL::kCMTextMarkupAttribute_ForegroundColorARGB, 0) == kCFCompareEqualTo) {
                auto arrayValue = dynamic_cf_cast<CFArrayRef>(value);
                if (!arrayValue)
                    continue;

                auto color = makeSimpleColorFromARGBCFArray(arrayValue);
                if (!color)
                    continue;
                cueData->setForegroundColor(*color);
                continue;
            }

            if (CFStringCompare(key, PAL::kCMTextMarkupAttribute_BackgroundColorARGB, 0) == kCFCompareEqualTo) {
                auto arrayValue = dynamic_cf_cast<CFArrayRef>(value);
                if (!arrayValue)
                    continue;

                auto color = makeSimpleColorFromARGBCFArray(arrayValue);
                if (!color)
                    continue;
                cueData->setBackgroundColor(*color);
                continue;
            }

            if (CFStringCompare(key, PAL::kCMTextMarkupAttribute_CharacterBackgroundColorARGB, 0) == kCFCompareEqualTo) {
                auto arrayValue = dynamic_cf_cast<CFArrayRef>(value);
                if (!arrayValue)
                    continue;

                auto color = makeSimpleColorFromARGBCFArray(arrayValue);
                if (!color)
                    continue;
                cueData->setHighlightColor(*color);
                continue;
            }
        }

        content.append(tagStart);
        content.append(StringView(attributedStringValue).substring(effectiveRange.location, effectiveRange.length));
        content.append(tagEnd);
    }

    // AVFoundation cue "position" is to the center of the text so calculate the correct position and positionAlign
    // relative to the cue's indicated alignment, size, and initial position.
    if (cueData->position() >= 0 && cueData->size() >= 0) {
        switch (cueData->align()) {
        case GenericCueData::Alignment::None:
            // By default, VTT cues alignment align as "start"
            [[fallthrough]];
        case GenericCueData::Alignment::Middle:
            // AVFoundation generates "middle" alignment cues for single line cues
            // and cues multi-line cues with lines of equal length, so just treat
            // "middle" alignment the same as "start"
            [[fallthrough]];
        case GenericCueData::Alignment::Start:
            cueData->setPositionAlign(GenericCueData::Alignment::Start);
            cueData->setPosition(cueData->position() - cueData->size() / 2);
            break;
        case GenericCueData::Alignment::End:
            cueData->setPositionAlign(GenericCueData::Alignment::End);
            cueData->setPosition(cueData->position() + cueData->size() / 2);
        }
    }

    if (content.length())
        cueData->setContent(content.toString());

    return cueData;
}

void InbandTextTrackPrivateAVF::processCue(CFArrayRef attributedStrings, CFArrayRef nativeSamples, const MediaTime& time)
{
    if (!hasClients())
        return;

    if (attributedStrings && CFArrayGetCount(attributedStrings))
        processAttributedStrings(attributedStrings, time);
    if (nativeSamples && CFArrayGetCount(nativeSamples))
        processVTTSamples(nativeSamples, time);
}

void InbandTextTrackPrivateAVF::processAttributedStrings(CFArrayRef attributedStrings, const MediaTime& time)
{
    ASSERT(isMainThread());
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
            cueData->setStatus(GenericCueData::Status::Partial);

            arrivingCues.append(WTFMove(cueData));
        }
    }

    if (m_pendingCueStatus != None) {
        // Cues do not have an explicit duration, they are displayed until the next "cue" (which might be empty) is emitted.
        m_currentCueEndTime = time;

        if (m_currentCueEndTime >= m_currentCueStartTime) {
            for (Ref cueData : m_cues) {
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

                    notifyMainThreadClient([&cueData](auto& client) {
                        downcast<InbandTextTrackPrivateClient>(client).updateGenericCue(cueData);
                    });
                } else {
                    // We have to assume the implicit duration is invalid for cues delivered during a seek because the AVF decode pipeline may not
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

    for (Ref cueData : arrivingCues) {
        m_cues.append(cueData.get());
        INFO_LOG(LOGIDENTIFIER, "adding cue ", cueData.get());
        notifyMainThreadClient([&cueData](auto& client) {
            downcast<InbandTextTrackPrivateClient>(client).addGenericCue(cueData);
        });
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
    m_index = 0;
}

void InbandTextTrackPrivateAVF::removeCompletedCues()
{
    if (hasClients()) {
        m_cues.removeAllMatching([&](auto cue) {
            if (cue->status() != GenericCueData::Status::Complete)
                return false;

            INFO_LOG(LOGIDENTIFIER, "removing cue ", cue.get());
            return true;
        });
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

    auto cues = std::exchange(m_cues, Vector<Ref<InbandGenericCue>> { });
    notifyMainThreadClient([cues = WTFMove(cues)](auto& client) {
        for (auto& cue : cues)
            downcast<InbandTextTrackPrivateClient>(client).removeGenericCue(cue);
    });

    m_pendingCueStatus = None;
    m_currentCueStartTime = MediaTime::zeroTime();
    m_currentCueEndTime = MediaTime::zeroTime();
}

void InbandTextTrackPrivateAVF::setMode(InbandTextTrackPrivate::Mode newMode)
{
    if (newMode == mode())
        return;

    InbandTextTrackPrivate::setMode(newMode);
    m_modeChangedCallback();
}

bool InbandTextTrackPrivateAVF::processVTTFileHeader(CMFormatDescriptionRef formatDescription)
{
    ASSERT(!m_haveReportedVTTHeader);
    ASSERT(formatDescription);

    RefPtr<ArrayBuffer> buffer;

    auto extensions = PAL::CMFormatDescriptionGetExtensions(formatDescription);
    if (!extensions)
        return false;

    auto sampleDescriptionExtensions = static_cast<CFDictionaryRef>(CFDictionaryGetValue(extensions, PAL::kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms));
    if (!sampleDescriptionExtensions)
        return false;

    RetainPtr webvttHeaderData = static_cast<CFDataRef>(CFDictionaryGetValue(sampleDescriptionExtensions, CFSTR("vttC")));
    if (!webvttHeaderData)
        return false;

    auto headerData = span(webvttHeaderData.get());
    if (headerData.empty())
        return false;

    auto identifier = LOGIDENTIFIER;
    notifyMainThreadClient([headerData = WTFMove(headerData), identifier, this](auto& client) {
        // A WebVTT header is terminated by "One or more WebVTT line terminators" so append two line feeds to make sure the parser
        // reccognizes this string as a full header.
        auto header = makeString(headerData, "\n\n"_s);

        INFO_LOG(identifier, "VTT header ", header);
        downcast<InbandTextTrackPrivateClient>(client).parseWebVTTFileHeader(WTFMove(header));
    });

    return true;
}

void InbandTextTrackPrivateAVF::processVTTSample(CMSampleBufferRef sampleBuffer, const MediaTime& presentationTime)
{
    CMFormatDescriptionRef formatDescription;
    if (!readVTTSampleBuffer(sampleBuffer, formatDescription))
        return;

    CMSampleTimingInfo timingInfo;
    auto status = PAL::CMSampleBufferGetSampleTimingInfo(sampleBuffer, 0, &timingInfo);
    if (status) {
        ERROR_LOG(LOGIDENTIFIER, "CMSampleBufferGetSampleTimingInfo returned error ", status);
        return;
    }

    while (true) {
        RefPtr buffer = ArrayBuffer::create(m_sampleInputBuffer);
        Ref view = JSC::DataView::create(WTFMove(buffer), 0, buffer->byteLength());

        auto peekResult = ISOBox::peekBox(view, 0);
        if (!peekResult)
            break;

        auto type = peekResult->first;
        auto boxLength = peekResult.value().second;
        ALWAYS_LOG(LOGIDENTIFIER, "chunk type = '", type, "', size = ", boxLength);

        if (boxLength > view->byteLength()) {
            ERROR_LOG(LOGIDENTIFIER, "ISO box larger than buffer length!");
            break;
        }

        if (!m_haveReportedVTTHeader && formatDescription)
            m_haveReportedVTTHeader = processVTTFileHeader(formatDescription);

        if (type == ISOWebVTTCue::boxTypeName()) {
            ISOWebVTTCue cueData = ISOWebVTTCue(presentationTime, PAL::toMediaTime(timingInfo.duration));
            cueData.read(view);

            notifyMainThreadClient([cueData = WTFMove(cueData)](auto& client) mutable {
                downcast<InbandTextTrackPrivateClient>(client).parseWebVTTCueData(WTFMove(cueData));
            });
        }

        m_sampleInputBuffer.removeAt(0, (size_t)boxLength);
    }
}

void InbandTextTrackPrivateAVF::processVTTSamples(CFArrayRef nativeSamples, const MediaTime& presentationTime)
{
    ASSERT(isMainThread());

    if (!nativeSamples)
        return;

    auto count = CFArrayGetCount(nativeSamples);
    if (!count)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, count, " sample buffers at time ", presentationTime);

    for (CFIndex i = 0; i < count; i++) {
        auto sampleBuffer = reinterpret_cast<CMSampleBufferRef>(const_cast<void*>(CFArrayGetValueAtIndex(nativeSamples, i)));
        if (sampleBuffer)
            processVTTSample(sampleBuffer, presentationTime);
    }
}

bool InbandTextTrackPrivateAVF::readVTTSampleBuffer(CMSampleBufferRef sampleBuffer, CMFormatDescriptionRef& formatDescription)
{
#if OS(WINDOWS) && HAVE(AVCFPLAYERITEM_CALLBACK_VERSION_2)
    return false;
#else
    auto blockBuffer = PAL::CMSampleBufferGetDataBuffer(sampleBuffer);
    auto bufferLength = PAL::CMBlockBufferGetDataLength(blockBuffer);
    if (bufferLength < ISOBox::minimumBoxSize()) {
        ERROR_LOG(LOGIDENTIFIER, "CMSampleBuffer size length unexpectedly small ", bufferLength);
        return false;
    }

    m_sampleInputBuffer.grow(m_sampleInputBuffer.size() + bufferLength);
    PAL::CMBlockBufferCopyDataBytes(blockBuffer, 0, bufferLength, m_sampleInputBuffer.mutableSpan().last(bufferLength).data());

    formatDescription = PAL::CMSampleBufferGetFormatDescription(sampleBuffer);

    return true;
#endif
}

} // namespace WebCore

#endif // ENABLE(VIDEO) && (USE(AVFOUNDATION) || PLATFORM(IOS_FAMILY))
