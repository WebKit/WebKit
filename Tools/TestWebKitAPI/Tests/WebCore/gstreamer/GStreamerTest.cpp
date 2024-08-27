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

TEST_F(GStreamerTest, hevcProfileParsing)
{
    using namespace GStreamerCodecUtilities;

    ASSERT_STREQ(parseHEVCProfile("hev1.1.6.L93.B0"_s), "main");
    ASSERT_STREQ(parseHEVCProfile("hev1.2.4.L93.B0"_s), "main-10");
    ASSERT_STREQ(parseHEVCProfile("hev1.3.E.L93.B0"_s), "main-still-picture");
    ASSERT_STREQ(parseHEVCProfile("hev1.4.10.L186.BF.C8"_s), "monochrome");
    ASSERT_STREQ(parseHEVCProfile("hev1.4.10.L30.BD.C8"_s), "monochrome-10");
    ASSERT_STREQ(parseHEVCProfile("hev1.4.10.L30.B9.C8"_s), "monochrome-12");
    ASSERT_STREQ(parseHEVCProfile("hev1.4.10.L30.B1.C8"_s), "monochrome-16");
    ASSERT_STREQ(parseHEVCProfile("hev1.4.10.L30.B9.88"_s), "main-12");

    ASSERT_STREQ(parseHEVCProfile("hev1.4.10.L30.BE.08"_s), "main-444");
    ASSERT_STREQ(parseHEVCProfile("hev1.4.10.L30.BC.08"_s), "main-444-10");
    ASSERT_STREQ(parseHEVCProfile("hev1.4.10.L30.B8.08"_s), "main-444-12");

    ASSERT_STREQ(parseHEVCProfile("hev1.4.10.L30.BF.A8"_s), "main-intra");
    ASSERT_STREQ(parseHEVCProfile("hev1.4.10.L30.BD.A8"_s), "main-10-intra");
    ASSERT_STREQ(parseHEVCProfile("hev1.4.10.L30.B9.A8"_s), "main-12-intra");

    ASSERT_STREQ(parseHEVCProfile("hev1.4.10.L30.BE.28"_s), "main-444-intra");
    ASSERT_STREQ(parseHEVCProfile("hev1.4.10.L60.BC.28"_s), "main-444-10-intra");
    ASSERT_STREQ(parseHEVCProfile("hev1.4.10.L30.B0.20"_s), "main-444-16-intra");

    ASSERT_STREQ(parseHEVCProfile("hev1.5.20.L30.BE.0C"_s), "high-throughput-444");
    ASSERT_STREQ(parseHEVCProfile("hev1.5.20.L30.BC.0C"_s), "high-throughput-444-10");
    ASSERT_STREQ(parseHEVCProfile("hev1.5.20.L30.B0.0C"_s), "high-throughput-444-14");
    ASSERT_STREQ(parseHEVCProfile("hev1.5.20.L30.B0.24"_s), "high-throughput-444-16-intra");

    ASSERT_STREQ(parseHEVCProfile("hev1.9.200.L30.BF.8C"_s), "screen-extended-main");
    ASSERT_STREQ(parseHEVCProfile("hev1.9.200.L30.BD.8C"_s), "screen-extended-main-10");
    ASSERT_STREQ(parseHEVCProfile("hev1.9.200.L30.BE.0C"_s), "screen-extended-main-444");
    ASSERT_STREQ(parseHEVCProfile("hev1.9.200.L30.BC.0C"_s), "screen-extended-main-444-10");
    ASSERT_STREQ(parseHEVCProfile("hev1.9.200.L30.B0.0C"_s), "screen-extended-high-throughput-444-14");
}

TEST_F(GStreamerTest, capsFromCodecString)
{
    using namespace GStreamerCodecUtilities;

#define TEST_CAPS_FROM_CODEC(codecString, expectedInputFormat, expectedOutputCaps) G_STMT_START { \
        auto [input, output] = capsFromCodecString(codecString);        \
        auto inputStructure = gst_caps_get_structure(input.get(), 0);   \
        const char* inputFormat = gst_structure_get_string(inputStructure, "format"); \
        ASSERT_STREQ(inputFormat, expectedInputFormat);                 \
        GUniquePtr<char> outputCaps(gst_caps_to_string(output.get()));  \
        ASSERT_STREQ(outputCaps.get(), expectedOutputCaps);             \
    } G_STMT_END

#define TEST_CAPS_FROM_CODEC_FULL(codecString, expectedInputCaps, expectedOutputCaps) G_STMT_START { \
        auto [input, output] = capsFromCodecString(codecString);        \
        GUniquePtr<char> inputCaps(gst_caps_to_string(input.get()));    \
        ASSERT_STREQ(inputCaps.get(), expectedInputCaps);               \
        GUniquePtr<char> outputCaps(gst_caps_to_string(output.get()));  \
        ASSERT_STREQ(outputCaps.get(), expectedOutputCaps);             \
    } G_STMT_END

    TEST_CAPS_FROM_CODEC("av01.0.01M.08"_s, "I420", "video/x-av1, profile=(string)main, bit-depth-luma=(uint)8, bit-depth-chroma=(uint)8, chroma-format=(string)4:2:0");
    TEST_CAPS_FROM_CODEC("av01.1.04M.10"_s, "I420_10LE", "video/x-av1, profile=(string)high, bit-depth-luma=(uint)10, bit-depth-chroma=(uint)10, chroma-format=(string)4:2:0");
    TEST_CAPS_FROM_CODEC("av01.2.04M.12"_s, "I420_12LE", "video/x-av1, profile=(string)professional, bit-depth-luma=(uint)12, bit-depth-chroma=(uint)12, chroma-format=(string)4:2:0");

    // AV1 levels, per spec valid values range from 00 to 31, but we support only up to 23.
    for (unsigned i = 0; i < 23; i++) {
        GUniquePtr<char> codecString(g_strdup_printf("av01.0.%02dM.08", i));
        TEST_CAPS_FROM_CODEC(makeString(span(codecString.get())), "I420", "video/x-av1, profile=(string)main, bit-depth-luma=(uint)8, bit-depth-chroma=(uint)8, chroma-format=(string)4:2:0");
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
    TEST_CAPS_FROM_CODEC_FULL("av01.0.00M.10.0.110.01"_s, "video/x-raw, format=(string)I420_10LE, interlace-mode=(string)progressive, pixel-aspect-ratio=(fraction)1/1, chroma-site=(string)jpeg, colorimetry=(string)bt709", "video/x-av1, profile=(string)main, bit-depth-luma=(uint)10, bit-depth-chroma=(uint)10, chroma-format=(string)4:2:0");
    TEST_CAPS_FROM_CODEC_FULL("av01.0.00M.10.0.110.09"_s, "video/x-raw, format=(string)I420_10LE, interlace-mode=(string)progressive, pixel-aspect-ratio=(fraction)1/1, chroma-site=(string)jpeg, colorimetry=(string)2:3:5:7", "video/x-av1, profile=(string)main, bit-depth-luma=(uint)10, bit-depth-chroma=(uint)10, chroma-format=(string)4:2:0");

    // AV1 bt709 transfer characteristics.
    TEST_CAPS_FROM_CODEC_FULL("av01.0.00M.10.0.110.01.01"_s, "video/x-raw, format=(string)I420_10LE, interlace-mode=(string)progressive, pixel-aspect-ratio=(fraction)1/1, chroma-site=(string)jpeg, colorimetry=(string)bt709", "video/x-av1, profile=(string)main, bit-depth-luma=(uint)10, bit-depth-chroma=(uint)10, chroma-format=(string)4:2:0");
    TEST_CAPS_FROM_CODEC_FULL("av01.0.00M.10.0.110.01.04"_s, "video/x-raw, format=(string)I420_10LE, interlace-mode=(string)progressive, pixel-aspect-ratio=(fraction)1/1, chroma-site=(string)jpeg, colorimetry=(string)bt709", "video/x-av1, profile=(string)main, bit-depth-luma=(uint)10, bit-depth-chroma=(uint)10, chroma-format=(string)4:2:0");

    // AV1 custom transfer characteristics.
    TEST_CAPS_FROM_CODEC_FULL("av01.0.00M.10.0.110.01.06"_s, "video/x-raw, format=(string)I420_10LE, interlace-mode=(string)progressive, pixel-aspect-ratio=(fraction)1/1, chroma-site=(string)jpeg, colorimetry=(string)2:3:16:1", "video/x-av1, profile=(string)main, bit-depth-luma=(uint)10, bit-depth-chroma=(uint)10, chroma-format=(string)4:2:0");
    TEST_CAPS_FROM_CODEC_FULL("av01.0.00M.10.0.110.01.13"_s, "video/x-raw, format=(string)I420_10LE, interlace-mode=(string)progressive, pixel-aspect-ratio=(fraction)1/1, chroma-site=(string)jpeg, colorimetry=(string)2:3:0:1", "video/x-av1, profile=(string)main, bit-depth-luma=(uint)10, bit-depth-chroma=(uint)10, chroma-format=(string)4:2:0");
    TEST_CAPS_FROM_CODEC_FULL("av01.0.00M.10.0.110.01.14"_s, "video/x-raw, format=(string)I420_10LE, interlace-mode=(string)progressive, pixel-aspect-ratio=(fraction)1/1, chroma-site=(string)jpeg, colorimetry=(string)2:3:13:1", "video/x-av1, profile=(string)main, bit-depth-luma=(uint)10, bit-depth-chroma=(uint)10, chroma-format=(string)4:2:0");
    TEST_CAPS_FROM_CODEC_FULL("av01.0.00M.10.0.110.01.15"_s, "video/x-raw, format=(string)I420_10LE, interlace-mode=(string)progressive, pixel-aspect-ratio=(fraction)1/1, chroma-site=(string)jpeg, colorimetry=(string)2:3:11:1", "video/x-av1, profile=(string)main, bit-depth-luma=(uint)10, bit-depth-chroma=(uint)10, chroma-format=(string)4:2:0");
    TEST_CAPS_FROM_CODEC_FULL("av01.0.00M.10.0.110.01.16"_s, "video/x-raw, format=(string)I420_10LE, interlace-mode=(string)progressive, pixel-aspect-ratio=(fraction)1/1, chroma-site=(string)jpeg, colorimetry=(string)2:3:14:1", "video/x-av1, profile=(string)main, bit-depth-luma=(uint)10, bit-depth-chroma=(uint)10, chroma-format=(string)4:2:0");

    // AV1 video full range flag.
    TEST_CAPS_FROM_CODEC_FULL("av01.0.00M.10.0.110.01.01.00.0"_s, "video/x-raw, format=(string)I420_10LE, interlace-mode=(string)progressive, pixel-aspect-ratio=(fraction)1/1, chroma-site=(string)jpeg, colorimetry=(string)2:1:5:1", "video/x-av1, profile=(string)main, bit-depth-luma=(uint)10, bit-depth-chroma=(uint)10, chroma-format=(string)4:2:0");
    TEST_CAPS_FROM_CODEC_FULL("av01.0.00M.10.0.110.01.01.00.1"_s, "video/x-raw, format=(string)I420_10LE, interlace-mode=(string)progressive, pixel-aspect-ratio=(fraction)1/1, chroma-site=(string)jpeg, colorimetry=(string)1:1:5:1", "video/x-av1, profile=(string)main, bit-depth-luma=(uint)10, bit-depth-chroma=(uint)10, chroma-format=(string)4:2:0");

#undef TEST_CAPS_FROM_CODEC
#undef TEST_CAPS_FROM_CODEC_FULL
}

} // namespace TestWebKitAPI

#endif // USE(GSTREAMER)
