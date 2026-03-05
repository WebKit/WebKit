/*
 * Copyright (C) 2018 Igalia, S.L.
 * Copyright (C) 2018 Metrological Group B.V.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if USE(GSTREAMER)
#include "GStreamerTest.h"

#include <WebCore/GStreamerCodecUtilities.h>
#include <WebCore/GStreamerCommon.h>
#include <WebCore/GUniquePtrGStreamer.h>
#include <WebCore/IntSize.h>
#include <WebCore/PlatformVideoColorSpace.h>
#include <wtf/URL.h>
#include <wtf/glib/GMallocString.h>
#include <wtf/text/MakeString.h>

using namespace WebCore;

namespace TestWebKitAPI {

void GStreamerTest::SetUpTestSuite()
{
    ASSERT(!gst_is_initialized());
    gst_init(nullptr, nullptr);
    ASSERT(gst_is_initialized());
}

void GStreamerTest::TearDownTestSuite()
{
    ASSERT(gst_is_initialized());
    gst_deinit();
}

TEST_F(GStreamerTest, gstStructureGetters)
{
    GUniquePtr<GstStructure> structure(gst_structure_new("foo", "int-val", G_TYPE_INT, -5, "int64-val", G_TYPE_INT64, -10, "uint-val", G_TYPE_UINT, 5, "uint64-val", G_TYPE_UINT64, 18014398509481982, "double-val", G_TYPE_DOUBLE, 1.0, "bool-val", G_TYPE_BOOLEAN, TRUE, "str-val", G_TYPE_STRING, "hello-world", nullptr));
    ASSERT_EQ(gstStructureGet<int>(structure.get(), "int-val"_s), -5);
    ASSERT_TRUE(!gstStructureGet<int>(structure.get(), "int-val-noexist"_s).has_value());
    ASSERT_EQ(gstStructureGet<int64_t>(structure.get(), "int64-val"_s), -10);
    ASSERT_TRUE(!gstStructureGet<int64_t>(structure.get(), "int64-val-noexist"_s).has_value());
    ASSERT_EQ(gstStructureGet<unsigned>(structure.get(), "uint-val"_s), 5);
    ASSERT_TRUE(!gstStructureGet<unsigned>(structure.get(), "uint-val-noexist"_s).has_value());
    ASSERT_EQ(gstStructureGet<uint64_t>(structure.get(), "uint64-val"_s), 18014398509481982);
    ASSERT_TRUE(!gstStructureGet<uint64_t>(structure.get(), "uint64-val-noexist"_s).has_value());
    ASSERT_EQ(gstStructureGet<double>(structure.get(), "double-val"_s), 1.0);
    ASSERT_TRUE(!gstStructureGet<double>(structure.get(), "double-val-noexist"_s).has_value());
    ASSERT_EQ(gstStructureGet<bool>(structure.get(), "bool-val"_s), true);
    ASSERT_TRUE(!gstStructureGet<bool>(structure.get(), "bool-val-noexist"_s).has_value());
    ASSERT_EQ(gstStructureGetString(structure.get(), "str-val"_s), "hello-world"_s);
    ASSERT_TRUE(!gstStructureGetString(structure.get(), "str-val-noexist"_s));
    ASSERT_EQ(gstStructureGetName(structure.get()), "foo"_s);

    // webkit.org/b/276224
    auto emptyIntOpt = gstStructureGet<int>(structure.get(), "int-val-noexist2"_s);
    if (emptyIntOpt && *emptyIntOpt > 0)
        FAIL() << "emptyIntOpt should be empty, but has value " << *emptyIntOpt;

    GUniquePtr<GstStructure> arrays(gst_structure_new_from_string("bar, empty-array=(GstStructure) <>, struct-array=(GstStructure) <[s1, a=2], [s2, b=3]>"_s));
    GUniquePtr<GstStructure> s1(gst_structure_new_from_string("s1, a=2"_s));
    GUniquePtr<GstStructure> s2(gst_structure_new_from_string("s2, b=3"_s));
    ASSERT_TRUE(gstStructureGetArray<const GstStructure*>(arrays.get(), "empty-array"_s).isEmpty());

    Vector<const GstStructure*> structArray(gstStructureGetArray<const GstStructure*>(arrays.get(), "struct-array"_s));
    ASSERT_TRUE(gst_structure_is_equal(structArray.at(0), s1.get()));
    ASSERT_TRUE(gst_structure_is_equal(structArray.at(1), s2.get()));
    ASSERT_EQ(structArray.size(), 2);

    GUniquePtr<GstStructure> lists(gst_structure_new_from_string("bar, empty-list=(GstStructure) {}, struct-list=(GstStructure) {[s1, a=2], [s2, b=3]}"_s));
    ASSERT_TRUE(gstStructureGetList<const GstStructure*>(lists.get(), "empty-list"_s).isEmpty());

    Vector<const GstStructure*> structList(gstStructureGetList<const GstStructure*>(lists.get(), "struct-list"_s));
    ASSERT_TRUE(gst_structure_is_equal(structList.at(0), s1.get()));
    ASSERT_TRUE(gst_structure_is_equal(structList.at(1), s2.get()));
    ASSERT_EQ(structList.size(), 2);
}

TEST_F(GStreamerTest, gstStructureJSONSerializing)
{
    GUniquePtr<GstStructure> structure(gst_structure_new("foo", "int-val", G_TYPE_INT, 5, "str-val", G_TYPE_STRING, "foo", "bool-val", G_TYPE_BOOLEAN, TRUE, "uint64-val", G_TYPE_UINT64, 18014398509481982, "uint-val", G_TYPE_UINT, 2147483648, "int64-val", G_TYPE_INT64, 666, nullptr));
    auto jsonString = gstStructureToJSONString(structure.get());
    ASSERT_EQ(jsonString, "{\"int-val\":5,\"str-val\":\"foo\",\"bool-val\":1,\"uint64-val\":{\"$biguint\":\"18014398509481982\"},\"uint-val\":2147483648,\"int64-val\":{\"$bigint\":\"666\"}}"_s);

    GUniquePtr<GstStructure> innerStructure(gst_structure_new("bar", "boo", G_TYPE_BOOLEAN, FALSE, "double-val", G_TYPE_DOUBLE, 2.42, nullptr));
    gst_structure_set(structure.get(), "inner", GST_TYPE_STRUCTURE, innerStructure.get(), nullptr);
    jsonString = gstStructureToJSONString(structure.get());
    ASSERT_EQ(jsonString, "{\"int-val\":5,\"str-val\":\"foo\",\"bool-val\":1,\"uint64-val\":{\"$biguint\":\"18014398509481982\"},\"uint-val\":2147483648,\"int64-val\":{\"$bigint\":\"666\"},\"inner\":{\"boo\":0,\"double-val\":2.42}}"_s);

    GUniquePtr<GstStructure> structureWithList(gst_structure_new_from_string("foo, words=(string){ hello, world }"));
    jsonString = gstStructureToJSONString(structureWithList.get());
    ASSERT_EQ(jsonString, "{\"words\":[\"hello\",\"world\"]}"_s);
}

TEST_F(GStreamerTest, streamIdParsing)
{
    ASSERT_EQ(parseStreamId("bec5903f-df6d-4773-85bb-05be65fc1bb8"_s), 0x85bb05be65fc1bb8);
    ASSERT_EQ(parseStreamId("647a4b9b3856ef43869870dc9e8b490e7c76512c47f9cfa7e9cf236fb9d9693d/001"_s), 1);
    ASSERT_EQ(parseStreamId("123"_s), 123);
    ASSERT_TRUE(!parseStreamId("foo"_s).has_value());
}

TEST_F(GStreamerTest, hevcProfileParsing)
{
    using namespace GStreamerCodecUtilities;

    ASSERT_EQ(parseHEVCProfile("hev1.1.6.L93.B0"_s), "main"_s);
    ASSERT_EQ(parseHEVCProfile("hev1.2.4.L93.B0"_s), "main-10"_s);
    ASSERT_EQ(parseHEVCProfile("hev1.3.E.L93.B0"_s), "main-still-picture"_s);
    ASSERT_EQ(parseHEVCProfile("hev1.4.10.L186.BF.C8"_s), "monochrome"_s);
    ASSERT_EQ(parseHEVCProfile("hev1.4.10.L30.BD.C8"_s), "monochrome-10"_s);
    ASSERT_EQ(parseHEVCProfile("hev1.4.10.L30.B9.C8"_s), "monochrome-12"_s);
    ASSERT_EQ(parseHEVCProfile("hev1.4.10.L30.B1.C8"_s), "monochrome-16"_s);
    ASSERT_EQ(parseHEVCProfile("hev1.4.10.L30.B9.88"_s), "main-12"_s);

    ASSERT_EQ(parseHEVCProfile("hev1.4.10.L30.BE.08"_s), "main-444"_s);
    ASSERT_EQ(parseHEVCProfile("hev1.4.10.L30.BC.08"_s), "main-444-10"_s);
    ASSERT_EQ(parseHEVCProfile("hev1.4.10.L30.B8.08"_s), "main-444-12"_s);

    ASSERT_EQ(parseHEVCProfile("hev1.4.10.L30.BF.A8"_s), "main-intra"_s);
    ASSERT_EQ(parseHEVCProfile("hev1.4.10.L30.BD.A8"_s), "main-10-intra"_s);
    ASSERT_EQ(parseHEVCProfile("hev1.4.10.L30.B9.A8"_s), "main-12-intra"_s);

    ASSERT_EQ(parseHEVCProfile("hev1.4.10.L30.BE.28"_s), "main-444-intra"_s);
    ASSERT_EQ(parseHEVCProfile("hev1.4.10.L60.BC.28"_s), "main-444-10-intra"_s);
    ASSERT_EQ(parseHEVCProfile("hev1.4.10.L30.B0.20"_s), "main-444-16-intra"_s);

    ASSERT_EQ(parseHEVCProfile("hev1.5.20.L30.BE.0C"_s), "high-throughput-444"_s);
    ASSERT_EQ(parseHEVCProfile("hev1.5.20.L30.BC.0C"_s), "high-throughput-444-10"_s);
    ASSERT_EQ(parseHEVCProfile("hev1.5.20.L30.B0.0C"_s), "high-throughput-444-14"_s);
    ASSERT_EQ(parseHEVCProfile("hev1.5.20.L30.B0.24"_s), "high-throughput-444-16-intra"_s);

    ASSERT_EQ(parseHEVCProfile("hev1.9.200.L30.BF.8C"_s), "screen-extended-main"_s);
    ASSERT_EQ(parseHEVCProfile("hev1.9.200.L30.BD.8C"_s), "screen-extended-main-10"_s);
    ASSERT_EQ(parseHEVCProfile("hev1.9.200.L30.BE.0C"_s), "screen-extended-main-444"_s);
    ASSERT_EQ(parseHEVCProfile("hev1.9.200.L30.BC.0C"_s), "screen-extended-main-444-10"_s);
    ASSERT_EQ(parseHEVCProfile("hev1.9.200.L30.B0.0C"_s), "screen-extended-high-throughput-444-14"_s);
}

TEST_F(GStreamerTest, capsFromCodecString)
{
    using namespace GStreamerCodecUtilities;

#define TEST_CAPS_FROM_CODEC(codecString, expectedInputFormat, expectedOutputCaps) G_STMT_START { \
        auto [input, output] = capsFromCodecString(codecString, { });   \
        auto inputStructure = gst_caps_get_structure(input.get(), 0);   \
        const char* inputFormat = gst_structure_get_string(inputStructure, "format"); \
        ASSERT_STREQ(inputFormat, expectedInputFormat);                 \
        auto outputCaps = GMallocString::unsafeAdoptFromUTF8(gst_caps_to_string(output.get())); \
        ASSERT_STREQ(outputCaps.utf8(), expectedOutputCaps);             \
    } G_STMT_END

#define TEST_CAPS_FROM_CODEC_FULL(codecString, expectedInputCaps, expectedOutputCaps) G_STMT_START { \
        auto [input, output] = capsFromCodecString(codecString, { });   \
        auto inputCaps = GMallocString::unsafeAdoptFromUTF8(gst_caps_to_string(input.get())); \
        ASSERT_STREQ(inputCaps.utf8(), expectedInputCaps);               \
        auto outputCaps = GMallocString::unsafeAdoptFromUTF8(gst_caps_to_string(output.get())); \
        ASSERT_STREQ(outputCaps.utf8(), expectedOutputCaps);             \
    } G_STMT_END

    TEST_CAPS_FROM_CODEC("av01.0.01M.08"_s, "I420", "video/x-av1, profile=(string)main, bit-depth-luma=(uint)8, bit-depth-chroma=(uint)8, chroma-format=(string)4:2:0");
    TEST_CAPS_FROM_CODEC("av01.1.04M.10"_s, "I420_10LE", "video/x-av1, profile=(string)high, bit-depth-luma=(uint)10, bit-depth-chroma=(uint)10, chroma-format=(string)4:2:0");
    TEST_CAPS_FROM_CODEC("av01.2.04M.12"_s, "I420_12LE", "video/x-av1, profile=(string)professional, bit-depth-luma=(uint)12, bit-depth-chroma=(uint)12, chroma-format=(string)4:2:0");

    // AV1 levels, per spec valid values range from 00 to 31, but we support only up to 23.
    for (unsigned i = 0; i < 23; i++) {
        auto codecString = GMallocString::unsafeAdoptFromUTF8(g_strdup_printf("av01.0.%02dM.08", i));
        TEST_CAPS_FROM_CODEC(String(codecString.span()), "I420", "video/x-av1, profile=(string)main, bit-depth-luma=(uint)8, bit-depth-chroma=(uint)8, chroma-format=(string)4:2:0");
    }

    // AV1 monochrome.
    TEST_CAPS_FROM_CODEC("av01.0.00M.08.0"_s, "I420", "video/x-av1, profile=(string)main, bit-depth-luma=(uint)8, bit-depth-chroma=(uint)8, chroma-format=(string)4:2:0");
    TEST_CAPS_FROM_CODEC("av01.0.00M.08.1"_s, "I420", "video/x-av1, profile=(string)main, bit-depth-luma=(uint)8, bit-depth-chroma=(uint)8, chroma-format=(string)4:0:0");

    // AV1 sub-sampling.
    TEST_CAPS_FROM_CODEC("av01.1.00M.10.0.000"_s, "Y444_10LE", "video/x-av1, profile=(string)high, bit-depth-luma=(uint)10, bit-depth-chroma=(uint)10, chroma-format=(string)4:4:4");
    TEST_CAPS_FROM_CODEC("av01.2.00M.10.0.100"_s, "I422_10LE", "video/x-av1, profile=(string)professional, bit-depth-luma=(uint)10, bit-depth-chroma=(uint)10, chroma-format=(string)4:2:2");
    TEST_CAPS_FROM_CODEC("av01.0.00M.10.0.110"_s, "I420_10LE", "video/x-av1, profile=(string)main, bit-depth-luma=(uint)10, bit-depth-chroma=(uint)10, chroma-format=(string)4:2:0");
    TEST_CAPS_FROM_CODEC("av01.0.00M.10.0.111"_s, "I420_10LE", "video/x-av1, profile=(string)main, bit-depth-luma=(uint)10, bit-depth-chroma=(uint)10, chroma-format=(string)4:2:0");
    TEST_CAPS_FROM_CODEC("av01.0.00M.10.0.112"_s, "I420_10LE", "video/x-av1, profile=(string)main, bit-depth-luma=(uint)10, bit-depth-chroma=(uint)10, chroma-format=(string)4:2:0");

    // AV1 colorimetry.
    TEST_CAPS_FROM_CODEC_FULL("av01.0.00M.10.0.110.01"_s, "video/x-raw, format=(string)I420_10LE, interlace-mode=(string)progressive, pixel-aspect-ratio=(fraction)1/1, colorimetry=(string)bt709", "video/x-av1, profile=(string)main, bit-depth-luma=(uint)10, bit-depth-chroma=(uint)10, chroma-format=(string)4:2:0");
    TEST_CAPS_FROM_CODEC_FULL("av01.0.00M.10.0.110.09"_s, "video/x-raw, format=(string)I420_10LE, interlace-mode=(string)progressive, pixel-aspect-ratio=(fraction)1/1, colorimetry=(string)2:3:5:7", "video/x-av1, profile=(string)main, bit-depth-luma=(uint)10, bit-depth-chroma=(uint)10, chroma-format=(string)4:2:0");

    // AV1 bt709 transfer characteristics.
    TEST_CAPS_FROM_CODEC_FULL("av01.0.00M.10.0.110.01.01"_s, "video/x-raw, format=(string)I420_10LE, interlace-mode=(string)progressive, pixel-aspect-ratio=(fraction)1/1, colorimetry=(string)bt709", "video/x-av1, profile=(string)main, bit-depth-luma=(uint)10, bit-depth-chroma=(uint)10, chroma-format=(string)4:2:0");
    TEST_CAPS_FROM_CODEC_FULL("av01.0.00M.10.0.110.01.04"_s, "video/x-raw, format=(string)I420_10LE, interlace-mode=(string)progressive, pixel-aspect-ratio=(fraction)1/1, colorimetry=(string)bt709", "video/x-av1, profile=(string)main, bit-depth-luma=(uint)10, bit-depth-chroma=(uint)10, chroma-format=(string)4:2:0");

    // AV1 custom transfer characteristics.
    TEST_CAPS_FROM_CODEC_FULL("av01.0.00M.10.0.110.01.06"_s, "video/x-raw, format=(string)I420_10LE, interlace-mode=(string)progressive, pixel-aspect-ratio=(fraction)1/1, colorimetry=(string)2:3:16:1", "video/x-av1, profile=(string)main, bit-depth-luma=(uint)10, bit-depth-chroma=(uint)10, chroma-format=(string)4:2:0");
    TEST_CAPS_FROM_CODEC_FULL("av01.0.00M.10.0.110.01.13"_s, "video/x-raw, format=(string)I420_10LE, interlace-mode=(string)progressive, pixel-aspect-ratio=(fraction)1/1, colorimetry=(string)2:3:0:1", "video/x-av1, profile=(string)main, bit-depth-luma=(uint)10, bit-depth-chroma=(uint)10, chroma-format=(string)4:2:0");
    TEST_CAPS_FROM_CODEC_FULL("av01.0.00M.10.0.110.01.14"_s, "video/x-raw, format=(string)I420_10LE, interlace-mode=(string)progressive, pixel-aspect-ratio=(fraction)1/1, colorimetry=(string)2:3:13:1", "video/x-av1, profile=(string)main, bit-depth-luma=(uint)10, bit-depth-chroma=(uint)10, chroma-format=(string)4:2:0");
    TEST_CAPS_FROM_CODEC_FULL("av01.0.00M.10.0.110.01.15"_s, "video/x-raw, format=(string)I420_10LE, interlace-mode=(string)progressive, pixel-aspect-ratio=(fraction)1/1, colorimetry=(string)2:3:11:1", "video/x-av1, profile=(string)main, bit-depth-luma=(uint)10, bit-depth-chroma=(uint)10, chroma-format=(string)4:2:0");
    TEST_CAPS_FROM_CODEC_FULL("av01.0.00M.10.0.110.01.16"_s, "video/x-raw, format=(string)I420_10LE, interlace-mode=(string)progressive, pixel-aspect-ratio=(fraction)1/1, colorimetry=(string)2:3:14:1", "video/x-av1, profile=(string)main, bit-depth-luma=(uint)10, bit-depth-chroma=(uint)10, chroma-format=(string)4:2:0");

    // AV1 video full range flag.
    TEST_CAPS_FROM_CODEC_FULL("av01.0.00M.10.0.110.01.01.00.0"_s, "video/x-raw, format=(string)I420_10LE, interlace-mode=(string)progressive, pixel-aspect-ratio=(fraction)1/1, colorimetry=(string)2:1:5:1", "video/x-av1, profile=(string)main, bit-depth-luma=(uint)10, bit-depth-chroma=(uint)10, chroma-format=(string)4:2:0");
    TEST_CAPS_FROM_CODEC_FULL("av01.0.00M.10.0.110.01.01.00.1"_s, "video/x-raw, format=(string)I420_10LE, interlace-mode=(string)progressive, pixel-aspect-ratio=(fraction)1/1, colorimetry=(string)1:1:5:1", "video/x-av1, profile=(string)main, bit-depth-luma=(uint)10, bit-depth-chroma=(uint)10, chroma-format=(string)4:2:0");

#undef TEST_CAPS_FROM_CODEC
#undef TEST_CAPS_FROM_CODEC_FULL
}

TEST_F(GStreamerTest, displayAspectRatioCalculation)
{
#define TEST_DAR_CALCULATION(videoWidth, videoHeight, parN, parD, displayWidth, displayHeight) G_STMT_START { \
        auto caps = adoptGRef(gst_caps_new_simple("video/x-raw", "width", G_TYPE_INT, videoWidth, "height", G_TYPE_INT, videoHeight, "pixel-aspect-ratio", GST_TYPE_FRACTION, parN, parD, nullptr)); \
        \
        IntSize size; \
        GstVideoFormat format = GST_VIDEO_FORMAT_UNKNOWN; \
        int outParN = 0, outParD = 0, stride = 0; \
        double frameRate = 0.0; \
        PlatformVideoColorSpace colorSpace { }; \
        \
        bool ok = getVideoSizeAndFormatFromCaps(caps.get(), size, format, outParN, outParD, stride, frameRate, colorSpace); \
        \
        ASSERT_TRUE(ok); \
        EXPECT_EQ(size.width(), videoWidth); \
        EXPECT_EQ(size.height(), videoHeight); \
        EXPECT_EQ(outParN, parN); \
        EXPECT_EQ(outParD, parD); \
        \
        auto computedSize = getDisplaySize(size, outParN, outParD); \
        \
        ASSERT_TRUE(computedSize.has_value()); \
        EXPECT_EQ(computedSize.value().width(), displayWidth); \
        EXPECT_EQ(computedSize.value().height(), displayHeight); \
    } G_STMT_END

    TEST_DAR_CALCULATION(1280, 720, 1, 1, 1280, 720); // Square pixels 720p
    TEST_DAR_CALCULATION(720, 576, 16, 15, 768, 576); // PAL 4:3
    TEST_DAR_CALCULATION(352, 288, 12, 11, 384, 288); // CIF 352x288 (≈4:3)
    TEST_DAR_CALCULATION(1920, 1080, 4, 3, 2560, 1080); // Anamorphic HD (64:27)
    TEST_DAR_CALCULATION(720, 480, 10, 11, 720, 528); // NTSC 4:3 (PAR 10:11)
    TEST_DAR_CALCULATION(1280, 720, 4, 3, 1280, 540); // 720p non-square pixels (64:27)
    TEST_DAR_CALCULATION(720, 480, 32, 27, 720, 405); // NTSC widescreen 16:9
    TEST_DAR_CALCULATION(720, 480, 8, 9, 640, 480); // NTSC 4:3
    TEST_DAR_CALCULATION(1440, 1080, 4, 3, 1920, 1080); // HDV 1440x1080 anamorphic (16:9)
    TEST_DAR_CALCULATION(1024, 576, 11, 10, 1126, 576); // 1024x576 custom PAR (≈88:45)
    TEST_DAR_CALCULATION(330, 196, 7201628, 7170075, 331, 196); // 330x196 custom PAR (value too high)

#undef TEST_DAR_CALCULATION
}

TEST_F(GStreamerTest, protocolValidation)
{
    // Test protocols allowed by default
    ASSERT_TRUE(isProtocolAllowed(WTF::URL { "https://example.com/video.mp4"_s }));
    ASSERT_TRUE(isProtocolAllowed(WTF::URL { "file:///path/to/video.mp4"_s }));
    ASSERT_TRUE(isProtocolAllowed(WTF::URL { "http://example.com/video.mp4"_s }));
    ASSERT_TRUE(isProtocolAllowed(WTF::URL { "blob:https://example.com/uuid"_s }));
    ASSERT_TRUE(isProtocolAllowed(WTF::URL { "data:video/mp4;base64,data"_s }));
    ASSERT_TRUE(isProtocolAllowed(WTF::URL { "mediasourceblob://source-id"_s }));
    ASSERT_TRUE(isProtocolAllowed(WTF::URL { "mediastream://stream-id"_s }));

    // Test allowed protocols: mix of lower and upper-case
    ASSERT_TRUE(isProtocolAllowed(WTF::URL { "Https://example.com/video.mp4"_s }));
    ASSERT_TRUE(isProtocolAllowed(WTF::URL { "fILe:///path/to/video.mp4"_s }));
    ASSERT_TRUE(isProtocolAllowed(WTF::URL { "HTTP://example.com/video.mp4"_s }));
    ASSERT_TRUE(isProtocolAllowed(WTF::URL { "bloB:https://example.com/uuid"_s }));
    ASSERT_TRUE(isProtocolAllowed(WTF::URL { "DAta:video/mp4;base64,data"_s }));
    ASSERT_TRUE(isProtocolAllowed(WTF::URL { "mediasourceBLOB://source-id"_s }));

    // Test forbidden protocols
    ASSERT_FALSE(isProtocolAllowed(WTF::URL { "ftp://example.com/video.mp4"_s }));
    ASSERT_FALSE(isProtocolAllowed(WTF::URL { "rtsp://example.com/stream"_s }));
    ASSERT_FALSE(isProtocolAllowed(WTF::URL { "about:blank"_s }));
    ASSERT_FALSE(isProtocolAllowed(WTF::URL { "mediasource://source-id"_s }));

    // Allow ftp protocol
    g_setenv("WEBKIT_GST_ALLOWED_URI_PROTOCOLS", "ftp", TRUE);

    ASSERT_TRUE(isProtocolAllowed(WTF::URL { "https://example.com/video.mp4"_s }));
    ASSERT_TRUE(isProtocolAllowed(WTF::URL { "file:///path/to/video.mp4"_s }));
    ASSERT_TRUE(isProtocolAllowed(WTF::URL { "http://example.com/video.mp4"_s }));
    ASSERT_TRUE(isProtocolAllowed(WTF::URL { "blob:https://example.com/uuid"_s }));
    ASSERT_TRUE(isProtocolAllowed(WTF::URL { "data:video/mp4;base64,data"_s }));
    ASSERT_TRUE(isProtocolAllowed(WTF::URL { "mediasourceblob://source-id"_s }));
    ASSERT_TRUE(isProtocolAllowed(WTF::URL { "mediastream://stream-id"_s }));
    ASSERT_TRUE(isProtocolAllowed(WTF::URL { "ftp://example.com/video.mp4"_s }));
    ASSERT_FALSE(isProtocolAllowed(WTF::URL { "rtsp://example.com/stream"_s }));
    ASSERT_FALSE(isProtocolAllowed(WTF::URL { "about:blank"_s }));

    // Allow rtsp protocol
    g_setenv("WEBKIT_GST_ALLOWED_URI_PROTOCOLS", "rtsp", TRUE);

    ASSERT_TRUE(isProtocolAllowed(WTF::URL { "https://example.com/video.mp4"_s }));
    ASSERT_TRUE(isProtocolAllowed(WTF::URL { "file:///path/to/video.mp4"_s }));
    ASSERT_TRUE(isProtocolAllowed(WTF::URL { "http://example.com/video.mp4"_s }));
    ASSERT_TRUE(isProtocolAllowed(WTF::URL { "blob:https://example.com/uuid"_s }));
    ASSERT_TRUE(isProtocolAllowed(WTF::URL { "data:video/mp4;base64,data"_s }));
    ASSERT_TRUE(isProtocolAllowed(WTF::URL { "mediasourceblob://source-id"_s }));
    ASSERT_TRUE(isProtocolAllowed(WTF::URL { "mediastream://stream-id"_s }));
    ASSERT_TRUE(isProtocolAllowed(WTF::URL { "rtsp://example.com/stream"_s }));
    ASSERT_FALSE(isProtocolAllowed(WTF::URL { "ftp://example.com/video.mp4"_s }));
    ASSERT_FALSE(isProtocolAllowed(WTF::URL { "about:blank"_s }));

    // Allow ftp and rtsp protocols
    g_setenv("WEBKIT_GST_ALLOWED_URI_PROTOCOLS", "rtsp,ftp", TRUE);

    ASSERT_TRUE(isProtocolAllowed(WTF::URL { "https://example.com/video.mp4"_s }));
    ASSERT_TRUE(isProtocolAllowed(WTF::URL { "file:///path/to/video.mp4"_s }));
    ASSERT_TRUE(isProtocolAllowed(WTF::URL { "http://example.com/video.mp4"_s }));
    ASSERT_TRUE(isProtocolAllowed(WTF::URL { "blob:https://example.com/uuid"_s }));
    ASSERT_TRUE(isProtocolAllowed(WTF::URL { "data:video/mp4;base64,data"_s }));
    ASSERT_TRUE(isProtocolAllowed(WTF::URL { "mediasourceblob://source-id"_s }));
    ASSERT_TRUE(isProtocolAllowed(WTF::URL { "mediastream://stream-id"_s }));
    ASSERT_TRUE(isProtocolAllowed(WTF::URL { "rtsp://example.com/stream"_s }));
    ASSERT_TRUE(isProtocolAllowed(WTF::URL { "ftp://example.com/video.mp4"_s }));
    ASSERT_FALSE(isProtocolAllowed(WTF::URL { "about:blank"_s }));

    g_unsetenv("WEBKIT_GST_ALLOWED_URI_PROTOCOLS");
}

TEST_F(GStreamerTest, protocolValidationEnvironmentVariable)
{
    // Introduce typos in the environment variable
    g_setenv("WEBKIT_GST_ALLOWED_URI_PROTOCOLS", "rtsp;ftp", TRUE);
    ASSERT_FALSE(isProtocolAllowed(WTF::URL { "rtsp://example.com/stream"_s }));
    ASSERT_FALSE(isProtocolAllowed(WTF::URL { "ftp://example.com/video.mp4"_s }));

    g_setenv("WEBKIT_GST_ALLOWED_URI_PROTOCOLS", "rtsp,   ftp   ", TRUE);
    ASSERT_TRUE(isProtocolAllowed(WTF::URL { "rtsp://example.com/stream"_s }));
    ASSERT_TRUE(isProtocolAllowed(WTF::URL { "ftp://example.com/video.mp4"_s }));

    g_setenv("WEBKIT_GST_ALLOWED_URI_PROTOCOLS", "rtsp:ftp", TRUE);
    ASSERT_FALSE(isProtocolAllowed(WTF::URL { "rtsp://example.com/stream"_s }));
    ASSERT_FALSE(isProtocolAllowed(WTF::URL { "ftp://example.com/video.mp4"_s }));

    g_setenv("WEBKIT_GST_ALLOWED_URI_PROTOCOLS", "-rtsp-ftp", TRUE);
    ASSERT_FALSE(isProtocolAllowed(WTF::URL { "rtsp://example.com/stream"_s }));
    ASSERT_FALSE(isProtocolAllowed(WTF::URL { "ftp://example.com/video.mp4"_s }));

    g_unsetenv("WEBKIT_GST_ALLOWED_URI_PROTOCOLS");
}
} // namespace TestWebKitAPI

#endif // USE(GSTREAMER)
