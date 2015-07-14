/*
 * Copyright (C) 2012-2015 Apple Inc. All rights reserved.
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

#if ENABLE(VIDEO) && (USE(AVFOUNDATION) || PLATFORM(IOS))

#include "InbandTextTrackPrivateAVF.h"

#include "ISOVTTCue.h"
#include "InbandTextTrackPrivateClient.h"
#include "Logging.h"
#include "MediaTimeAVFoundation.h"
#include <runtime/ArrayBuffer.h>
#include <runtime/DataView.h>
#include <runtime/Int8Array.h>
#include <wtf/MediaTime.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>
#include <wtf/unicode/CharacterNames.h>
#include <wtf/StringPrintStream.h>

#include "CoreMediaSoftLink.h"

namespace JSC {
class ArrayBuffer;
}

namespace WebCore {

AVFInbandTrackParent::~AVFInbandTrackParent()
{
}

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

static bool makeRGBA32FromARGBCFArray(CFArrayRef colorArray, RGBA32& color)
{
    if (CFArrayGetCount(colorArray) < 4)
        return false;

    float componentArray[4];
    for (int i = 0; i < 4; i++) {
        CFNumberRef value = static_cast<CFNumberRef>(CFArrayGetValueAtIndex(colorArray, i));
        if (CFGetTypeID(value) != CFNumberGetTypeID())
            return false;

        float component;
        CFNumberGetValue(value, kCFNumberFloatType, &component);
        componentArray[i] = component;
    }

    color = makeRGBA32FromFloats(componentArray[1], componentArray[2], componentArray[3], componentArray[0]);
    return true;
}

void InbandTextTrackPrivateAVF::processCueAttributes(CFAttributedStringRef attributedString, GenericCueData& cueData)
{
    // Some of the attributes we translate into per-cue WebVTT settings are are repeated on each part of an attributed string so only
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

    StringBuilder content;
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
                    cueData.setAlign(GenericCueData::Start);
                else if (CFStringCompare(valueString, kCMTextMarkupAlignmentType_Middle, 0) == kCFCompareEqualTo)
                    cueData.setAlign(GenericCueData::Middle);
                else if (CFStringCompare(valueString, kCMTextMarkupAlignmentType_End, 0) == kCFCompareEqualTo)
                    cueData.setAlign(GenericCueData::End);
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
                cueData.setLine(line);
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
                cueData.setPosition(position);
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
                cueData.setSize(size);
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
                cueData.setBaseFontSize(baseFontSize);
                continue;
            }

            if (CFStringCompare(key, kCMTextMarkupAttribute_RelativeFontSize, 0) == kCFCompareEqualTo) {
                if (CFGetTypeID(value) != CFNumberGetTypeID())
                    continue;
                
                CFNumberRef valueNumber = static_cast<CFNumberRef>(value);
                double relativeFontSize;
                CFNumberGetValue(valueNumber, kCFNumberFloat64Type, &relativeFontSize);
                cueData.setRelativeFontSize(relativeFontSize);
                continue;
            }

            if (CFStringCompare(key, kCMTextMarkupAttribute_FontFamilyName, 0) == kCFCompareEqualTo) {
                valueString = static_cast<CFStringRef>(value);
                if (CFGetTypeID(valueString) != CFStringGetTypeID() || !CFStringGetLength(valueString))
                    continue;
                if (processed & FontName)
                    continue;
                processed |= FontName;
                
                cueData.setFontName(valueString);
                continue;
            }

            if (CFStringCompare(key, kCMTextMarkupAttribute_ForegroundColorARGB, 0) == kCFCompareEqualTo) {
                CFArrayRef arrayValue = static_cast<CFArrayRef>(value);
                if (CFGetTypeID(arrayValue) != CFArrayGetTypeID())
                    continue;
                
                RGBA32 color;
                if (!makeRGBA32FromARGBCFArray(arrayValue, color))
                    continue;
                cueData.setForegroundColor(color);
            }
            
            if (CFStringCompare(key, kCMTextMarkupAttribute_BackgroundColorARGB, 0) == kCFCompareEqualTo) {
                CFArrayRef arrayValue = static_cast<CFArrayRef>(value);
                if (CFGetTypeID(arrayValue) != CFArrayGetTypeID())
                    continue;
                
                RGBA32 color;
                if (!makeRGBA32FromARGBCFArray(arrayValue, color))
                    continue;
                cueData.setBackgroundColor(color);
            }

            if (CFStringCompare(key, kCMTextMarkupAttribute_CharacterBackgroundColorARGB, 0) == kCFCompareEqualTo) {
                CFArrayRef arrayValue = static_cast<CFArrayRef>(value);
                if (CFGetTypeID(arrayValue) != CFArrayGetTypeID())
                    continue;
                
                RGBA32 color;
                if (!makeRGBA32FromARGBCFArray(arrayValue, color))
                    continue;
                cueData.setHighlightColor(color);
            }
        }

        content.append(tagStart);
        content.append(attributedStringValue.substring(effectiveRange.location, effectiveRange.length));
        content.append(tagEnd);
    }

    if (content.length())
        cueData.setContent(content.toString());
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
    LOG(Media, "InbandTextTrackPrivateAVF::processCue - %li cues at time %s\n", attributedStrings ? CFArrayGetCount(attributedStrings) : 0, toString(time).utf8().data());

    Vector<RefPtr<GenericCueData>> arrivingCues;
    if (attributedStrings) {
        CFIndex count = CFArrayGetCount(attributedStrings);
        for (CFIndex i = 0; i < count; i++) {
            CFAttributedStringRef attributedString = static_cast<CFAttributedStringRef>(CFArrayGetValueAtIndex(attributedStrings, i));
            
            if (!attributedString || !CFAttributedStringGetLength(attributedString))
                continue;
            
            RefPtr<GenericCueData> cueData = GenericCueData::create();
            processCueAttributes(attributedString, *cueData.get());
            if (!cueData->content().length())
                continue;
            
            arrivingCues.append(cueData);
            
            cueData->setStartTime(time);
            cueData->setEndTime(MediaTime::positiveInfiniteTime());
            
            // AVFoundation cue "position" is to the center of the text so adjust relative to the edge because we will use it to
            // set CSS "left".
            if (cueData->position() >= 0 && cueData->size() > 0)
                cueData->setPosition(cueData->position() - cueData->size() / 2);
            
            LOG(Media, "InbandTextTrackPrivateAVF::processCue(%p) - considering cue (\"%s\") for time = %s, position =  %.2f, line =  %.2f", this, cueData->content().utf8().data(), toString(cueData->startTime()).utf8().data(), cueData->position(), cueData->line());
            
            cueData->setStatus(GenericCueData::Partial);
        }
    }

    if (m_pendingCueStatus != None) {
        // Cues do not have an explicit duration, they are displayed until the next "cue" (which might be empty) is emitted.
        m_currentCueEndTime = time;

        if (m_currentCueEndTime >= m_currentCueStartTime) {
            for (auto& cueData : m_cues) {
                // See if one of the newly-arrived cues is an extension of this cue.
                Vector<RefPtr<GenericCueData>> nonExtensionCues;
                for (auto& arrivingCue : arrivingCues) {
                    if (!arrivingCue->doesExtendCueData(*cueData))
                        nonExtensionCues.append(arrivingCue);
                    else
                        LOG(Media, "InbandTextTrackPrivateAVF::processCue(%p) - found an extension cue (\"%s\") for time = %.2f, end = %.2f, position =  %.2f, line =  %.2f", this, arrivingCue->content().utf8().data(), arrivingCue->startTime().toDouble(), arrivingCue->endTime().toDouble(), arrivingCue->position(), arrivingCue->line());
                }

                bool currentCueIsExtended = (arrivingCues.size() != nonExtensionCues.size());

                arrivingCues = nonExtensionCues;
                
                if (currentCueIsExtended)
                    continue;

                if (m_pendingCueStatus == Valid) {
                    cueData->setEndTime(m_currentCueEndTime);
                    cueData->setStatus(GenericCueData::Complete);

                    LOG(Media, "InbandTextTrackPrivateAVF::processCue(%p) - updating cue \"%s\": start=%.2f, end=%.2f", this, cueData->content().utf8().data(), cueData->startTime().toDouble(), m_currentCueEndTime.toDouble());
                    client()->updateGenericCue(this, cueData.get());
                } else {
                    // We have to assume that the implicit duration is invalid for cues delivered during a seek because the AVF decode pipeline may not
                    // see every cue, so DO NOT update cue duration while seeking.
                    LOG(Media, "InbandTextTrackPrivateAVF::processCue(%p) - ignoring cue delivered during seek: start=%s, end=%s, content=\"%s\"", this, toString(cueData->startTime()).utf8().data(), toString(m_currentCueEndTime).utf8().data(), cueData->content().utf8().data());
                }
            }
        } else
            LOG(Media, "InbandTextTrackPrivateAVF::processCue negative length cue(s) ignored: start=%s, end=%s\n",  toString(m_currentCueStartTime).utf8().data(), toString(m_currentCueEndTime).utf8().data());

        removeCompletedCues();
    }

    if (arrivingCues.isEmpty())
        return;

    m_currentCueStartTime = time;

    for (auto& cueData : arrivingCues) {

        m_cues.append(cueData);
        
        LOG(Media, "InbandTextTrackPrivateAVF::processCue(%p) - adding cue \"%s\" for time = %.2f, end = %.2f, position =  %.2f, line =  %.2f", this, cueData->content().utf8().data(), cueData->startTime().toDouble(), cueData->endTime().toDouble(), cueData->position(), cueData->line());

        client()->addGenericCue(this, cueData.release());
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
            if (m_cues[currentCue]->status() != GenericCueData::Complete)
                continue;

            LOG(Media, "InbandTextTrackPrivateAVF::removeCompletedCues(%p) - removing cue \"%s\": start=%.2f, end=%.2f", this, m_cues[currentCue]->content().utf8().data(), m_cues[currentCue]->startTime().toDouble(), m_cues[currentCue]->endTime().toDouble());

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
        LOG(Media, "InbandTextTrackPrivateAVF::resetCueValues flushing data for cues: start=%s\n", toString(m_currentCueStartTime).utf8().data());

    if (client()) {
        for (size_t i = 0; i < m_cues.size(); i++)
            client()->removeGenericCue(this, m_cues[i].get());
    }

    m_cues.resize(0);
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
    if (!nativeSamples)
        return;

    CFIndex count = CFArrayGetCount(nativeSamples);
    if (!count)
        return;

    LOG(Media, "InbandTextTrackPrivateAVF::processNativeSamples - %li sample buffers at time %.2f\n", count, presentationTime.toDouble());

    for (CFIndex i = 0; i < count; i++) {
        RefPtr<ArrayBuffer> buffer;
        MediaTime duration;
        CMFormatDescriptionRef formatDescription;
        if (!readNativeSampleBuffer(nativeSamples, i, buffer, duration, formatDescription))
            continue;

        String type = ISOBox::peekType(buffer.get());
        size_t boxLength = ISOBox::peekLength(buffer.get());
        if (boxLength > buffer->byteLength()) {
            LOG(Media, "InbandTextTrackPrivateAVF::processNativeSamples(%p) - ERROR: chunk '%s' size (%zu) larger than buffer length (%u)!!", this, type.utf8().data(), boxLength, buffer->byteLength());
            continue;
        }

        LOG(Media, "InbandTextTrackPrivateAVF::processNativeSamples(%p) - chunk type = '%s', size = %zu", this, type.utf8().data(), boxLength);

        if (type == ISOWebVTTCue::boxType()) {
            ISOWebVTTCue cueData = ISOWebVTTCue(presentationTime, duration, buffer.get());
            LOG(Media, "    sample presentation time = %.2f, duration = %.2f", cueData.presentationTime().toDouble(), cueData.duration().toDouble());
            LOG(Media, "    id = \"%s\", settings = \"%s\", cue text = \"%s\"", cueData.id().utf8().data(), cueData.settings().utf8().data(), cueData.cueText().utf8().data());
            LOG(Media, "    sourceID = \"%s\", originalStartTime = \"%s\"", cueData.sourceID().utf8().data(), cueData.originalStartTime().utf8().data());

            client()->parseWebVTTCueData(this, cueData);
        }

        do {
            if (m_haveReportedVTTHeader)
                break;

            if (!formatDescription)
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
            header.append(reinterpret_cast<const unsigned char*>(CFDataGetBytePtr(webvttHeaderData)), length);
            header.append("\n\n");

            LOG(Media, "    vtt header = \n%s", header.toString().utf8().data());
            client()->parseWebVTTFileHeader(this, header.toString());
            m_haveReportedVTTHeader = true;
        } while (0);

        m_sampleInputBuffer.remove(0, boxLength);
    }
}

bool InbandTextTrackPrivateAVF::readNativeSampleBuffer(CFArrayRef nativeSamples, CFIndex index, RefPtr<ArrayBuffer>& buffer, MediaTime& duration, CMFormatDescriptionRef& formatDescription)
{
#if OS(WINDOWS) && HAVE(AVCFPLAYERITEM_CALLBACK_VERSION_2)
    return false;
#else
    CMSampleBufferRef const sampleBuffer = reinterpret_cast<CMSampleBufferRef>(const_cast<void*>(CFArrayGetValueAtIndex(nativeSamples, index)));
    if (!sampleBuffer)
        return false;

    CMSampleTimingInfo timingInfo;
    OSStatus status = CMSampleBufferGetSampleTimingInfo(sampleBuffer, index, &timingInfo);
    if (status) {
        LOG(Media, "InbandTextTrackPrivateAVF::readNativeSampleBuffer(%p) - CMSampleBufferGetSampleTimingInfo returned error %x for sample %li", this, static_cast<int>(status), index);
        return false;
    }

    duration = toMediaTime(timingInfo.duration);

    CMBlockBufferRef blockBuffer = CMSampleBufferGetDataBuffer(sampleBuffer);
    size_t bufferLength = CMBlockBufferGetDataLength(blockBuffer);
    if (bufferLength < ISOBox::boxHeaderSize()) {
        LOG(Media, "InbandTextTrackPrivateAVF::readNativeSampleBuffer(%p) - ERROR: CMSampleBuffer size length unexpectedly small (%zu)!!", this, bufferLength);
        return false;
    }

    m_sampleInputBuffer.resize(m_sampleInputBuffer.size() + bufferLength);
    CMBlockBufferCopyDataBytes(blockBuffer, 0, bufferLength, m_sampleInputBuffer.data() + m_sampleInputBuffer.size() - bufferLength);

    buffer = ArrayBuffer::create(m_sampleInputBuffer.data(), m_sampleInputBuffer.size());

    formatDescription = CMSampleBufferGetFormatDescription(sampleBuffer);

    return true;
#endif
}

} // namespace WebCore

#endif // ENABLE(VIDEO) && (USE(AVFOUNDATION) || PLATFORM(IOS))
