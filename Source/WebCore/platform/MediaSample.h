/*
 * Copyright (C) 2013-2021 Apple Inc. All rights reserved.
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

#pragma once

#include "FloatSize.h"
#include <JavaScriptCore/TypedArrays.h>
#include <wtf/EnumTraits.h>
#include <wtf/MediaTime.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/text/AtomString.h>

typedef struct opaqueCMSampleBuffer *CMSampleBufferRef;
typedef struct _GstSample GstSample;
typedef struct OpaqueMTPluginByteSource *MTPluginByteSourceRef;
typedef const struct opaqueCMFormatDescription *CMFormatDescriptionRef;

namespace WebCore {

class MockSampleBox;

struct PlatformSample {
    enum {
        None,
        MockSampleBoxType,
        CMSampleBufferType,
        GStreamerSampleType,
        ByteRangeSampleType,
    } type;
    union {
        MockSampleBox* mockSampleBox;
        CMSampleBufferRef cmSampleBuffer;
        GstSample* gstSample;
        std::pair<MTPluginByteSourceRef, CMFormatDescriptionRef> byteRangeSample;
    } sample;
};

class MediaSample : public ThreadSafeRefCounted<MediaSample> {
public:
    virtual ~MediaSample() = default;

    virtual MediaTime presentationTime() const = 0;
    virtual MediaTime decodeTime() const = 0;
    virtual MediaTime duration() const = 0;
    virtual AtomString trackID() const = 0;
    virtual void setTrackID(const String&) = 0;
    virtual size_t sizeInBytes() const = 0;
    virtual FloatSize presentationSize() const = 0;
    virtual void offsetTimestampsBy(const MediaTime&) = 0;
    virtual void setTimestamps(const MediaTime&, const MediaTime&) = 0;
    virtual bool isDivisable() const = 0;
    enum DivideFlags { BeforePresentationTime, AfterPresentationTime };
    enum class UseEndTime : bool {
        DoNotUse,
        Use,
    };
    virtual std::pair<RefPtr<MediaSample>, RefPtr<MediaSample>> divide(const MediaTime& presentationTime, UseEndTime = UseEndTime::DoNotUse) = 0;
    virtual Ref<MediaSample> createNonDisplayingCopy() const = 0;

    virtual RefPtr<JSC::Uint8ClampedArray> getRGBAImageData() const { return nullptr; }

    enum SampleFlags {
        None = 0,
        IsSync = 1 << 0,
        IsNonDisplaying = 1 << 1,
        HasAlpha = 1 << 2,
        HasSyncInfo = 1 << 3,
    };
    virtual SampleFlags flags() const = 0;
    virtual PlatformSample platformSample() = 0;

    struct ByteRange {
        size_t byteOffset { 0 };
        size_t byteLength { 0 };
    };
    virtual std::optional<ByteRange> byteRange() const = 0;

    enum class VideoRotation {
        None = 0,
        UpsideDown = 180,
        Right = 90,
        Left = 270,
    };
    virtual VideoRotation videoRotation() const { return VideoRotation::None; }
    virtual bool videoMirrored() const { return false; }
    virtual uint32_t videoPixelFormat() const { return 0; }

    bool isSync() const { return flags() & IsSync; }
    bool isNonDisplaying() const { return flags() & IsNonDisplaying; }
    bool hasAlpha() const { return flags() & HasAlpha; }
    bool hasSyncInfo() const { return flags() & HasSyncInfo; }

    virtual void dump(PrintStream&) const = 0;
    String toJSONString() const
    {
        auto object = JSON::Object::create();

        object->setObject("pts"_s, presentationTime().toJSONObject());
        object->setObject("dts"_s, decodeTime().toJSONObject());
        object->setObject("duration"_s, duration().toJSONObject());
        object->setInteger("flags"_s, static_cast<unsigned>(flags()));
        object->setObject("presentationSize"_s, presentationSize().toJSONObject());

        return object->toJSONString();
    }
};

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::MediaSample::VideoRotation> {
    using values = EnumValues<
        WebCore::MediaSample::VideoRotation,
        WebCore::MediaSample::VideoRotation::None,
        WebCore::MediaSample::VideoRotation::UpsideDown,
        WebCore::MediaSample::VideoRotation::Right,
        WebCore::MediaSample::VideoRotation::Left
    >;
};

template<typename Type> struct LogArgument;
template <>
struct LogArgument<WebCore::MediaSample> {
    static String toString(const WebCore::MediaSample& sample)
    {
        return sample.toJSONString();
    }
};

} // namespace WTF
