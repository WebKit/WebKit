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

#include <WebCore/FloatSize.h>
#include <WebCore/MediaPlayerEnums.h>
#include <functional>
#include <variant>
#include <wtf/MediaTime.h>
#include <wtf/Platform.h>
#include <wtf/PrintStream.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/text/AtomString.h>

typedef struct opaqueCMSampleBuffer *CMSampleBufferRef;
#if PLATFORM(COCOA)
typedef struct CF_BRIDGED_TYPE(id) __CVBuffer *CVPixelBufferRef;
#endif
typedef struct _GstSample GstSample;
typedef const struct opaqueCMFormatDescription *CMFormatDescriptionRef;

namespace WebCore {

class MockSampleBox;
class ProcessIdentity;

using TrackID = uint64_t;

class PlatformSample {
public:
    using VariantType = std::variant<const MockSampleBox*
#if PLATFORM(COCOA)
        , RetainPtr<CMSampleBufferRef>
#elif USE(GSTREAMER)
        , GstSample*
#endif
    >;
    PlatformSample(VariantType&& sample)
        : m_sample(WTFMove(sample))
    { }

    const MockSampleBox* mockSampleBox() const { return std::get<const MockSampleBox*>(m_sample); }

#if PLATFORM(COCOA)
    CMSampleBufferRef cmSampleBuffer() const { return std::get<RetainPtr<CMSampleBufferRef>>(m_sample).get(); }
#elif USE(GSTREAMER)
    GstSample* gstSample() const { return std::get<GstSample*>(m_sample); }
#endif

private:
    VariantType m_sample;
};

class MediaSample : public ThreadSafeRefCounted<MediaSample> {
public:
    virtual ~MediaSample() = default;

    virtual MediaTime presentationTime() const = 0;
    virtual MediaTime decodeTime() const = 0;
    virtual MediaTime duration() const = 0;
    virtual MediaTime presentationEndTime() const { return presentationTime() + duration(); }
    virtual TrackID trackID() const = 0;
    virtual size_t sizeInBytes() const = 0;
    virtual FloatSize presentationSize() const = 0;
    virtual void offsetTimestampsBy(const MediaTime&) = 0;
    virtual void setTimestamps(const MediaTime&, const MediaTime&) = 0;
    virtual bool isDivisable() const { return false; };
    enum DivideFlags { BeforePresentationTime, AfterPresentationTime };
    enum class UseEndTime : bool {
        DoNotUse,
        Use,
    };
    virtual std::pair<RefPtr<MediaSample>, RefPtr<MediaSample>> divide(const MediaTime&, UseEndTime = UseEndTime::DoNotUse)
    {
        ASSERT_NOT_REACHED();
        return { nullptr, nullptr };
    }
    virtual Ref<MediaSample> createNonDisplayingCopy() const = 0;

    enum SampleFlags {
        None = 0,
        IsSync = 1 << 0,
        IsNonDisplaying = 1 << 1,
        HasAlpha = 1 << 2,
        HasSyncInfo = 1 << 3,
        IsProtected = 1 << 4,
    };
    virtual SampleFlags flags() const = 0;
    virtual PlatformSample platformSample() const = 0;

    enum class Type : uint8_t {
        None,
        MockSampleBox,
        CMSampleBuffer,
        GStreamerSample,
    };
    virtual Type type() const = 0;

    virtual bool isImageDecoderAVFObjCSample() const { return false; }

    struct ByteRange {
        size_t byteOffset { 0 };
        size_t byteLength { 0 };
    };
    virtual std::optional<ByteRange> byteRange() const { return std::nullopt; }

    bool isSync() const { return flags() & IsSync; }
    bool isNonDisplaying() const { return flags() & IsNonDisplaying; }
    bool hasAlpha() const { return flags() & HasAlpha; }
    bool hasSyncInfo() const { return flags() & HasSyncInfo; }
    bool isProtected() const { return flags() & IsProtected; }

    virtual void dump(PrintStream& out) const
    {
        out.print("{PTS(", presentationTime(), "), DTS(", decodeTime(), "), duration(", duration(), "), flags(", (int)flags(), "), presentationSize(", presentationSize().width(), "x", presentationSize().height(), ")}");
    }

    String toJSONString() const
    {
        auto object = JSON::Object::create();

        object->setObject("pts"_s, presentationTime().toJSONObject());
        object->setObject("dts"_s, decodeTime().toJSONObject());
        object->setObject("duration"_s, duration().toJSONObject());
        object->setBoolean("isSync"_s, isSync());
        object->setBoolean("isNonDisplaying"_s, isNonDisplaying());
        object->setInteger("flags"_s, static_cast<unsigned>(flags()));
        object->setObject("presentationSize"_s, presentationSize().toJSONObject());

        return object->toJSONString();
    }
};

} // namespace WebCore

namespace WTF {

template<typename Type> struct LogArgument;
template <>
struct LogArgument<WebCore::MediaSample> {
    static String toString(const WebCore::MediaSample& sample)
    {
        return sample.toJSONString();
    }
};

} // namespace WTF
