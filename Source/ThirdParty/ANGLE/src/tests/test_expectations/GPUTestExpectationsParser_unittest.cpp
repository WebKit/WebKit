//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// GPUTestExpectationsParser_unittest.cpp: Unit tests for GPUTestExpectationsParser*
//

#include "tests/test_expectations/GPUTestExpectationsParser.h"
#include <gtest/gtest.h>
#include "tests/test_expectations/GPUTestConfig.h"

using namespace angle;

namespace
{

class GPUTestConfigTester : public GPUTestConfig
{
  public:
    GPUTestConfigTester()
    {
        mConditions.reset();
        mConditions[GPUTestConfig::kConditionWin]    = true;
        mConditions[GPUTestConfig::kConditionNVIDIA] = true;
        mConditions[GPUTestConfig::kConditionD3D11]  = true;
    }
};

// A correct entry with a test that's skipped on all platforms should not lead
// to any errors, and should properly return the expectation SKIP.
TEST(GPUTestExpectationsParserTest, GPUTestExpectationsParserSkip)
{
    GPUTestConfigTester config;
    std::string line =
        R"(100 : dEQP-GLES31.functional.layout_binding.ubo.vertex_binding_max = SKIP)";
    GPUTestExpectationsParser parser;
    EXPECT_TRUE(parser.loadTestExpectations(config, line));
    EXPECT_TRUE(parser.getErrorMessages().empty());
    EXPECT_EQ(
        parser.getTestExpectation("dEQP-GLES31.functional.layout_binding.ubo.vertex_binding_max"),
        GPUTestExpectationsParser::kGpuTestSkip);
}

// A correct entry with a test that's failed on all platforms should not lead
// to any errors, and should properly return the expectation FAIL.
TEST(GPUTestExpectationsParserTest, GPUTestExpectationsParserFail)
{
    GPUTestConfigTester config;
    std::string line =
        R"(100 : dEQP-GLES31.functional.layout_binding.ubo.vertex_binding_max = FAIL)";
    GPUTestExpectationsParser parser;
    EXPECT_TRUE(parser.loadTestExpectations(config, line));
    EXPECT_TRUE(parser.getErrorMessages().empty());
    EXPECT_EQ(
        parser.getTestExpectation("dEQP-GLES31.functional.layout_binding.ubo.vertex_binding_max"),
        GPUTestExpectationsParser::kGpuTestFail);
}

// A correct entry with a test that's passed on all platforms should not lead
// to any errors, and should properly return the expectation PASS.
TEST(GPUTestExpectationsParserTest, GPUTestExpectationsParserPass)
{
    GPUTestConfigTester config;
    std::string line =
        R"(100 : dEQP-GLES31.functional.layout_binding.ubo.vertex_binding_max = PASS)";
    GPUTestExpectationsParser parser;
    EXPECT_TRUE(parser.loadTestExpectations(config, line));
    EXPECT_TRUE(parser.getErrorMessages().empty());
    EXPECT_EQ(
        parser.getTestExpectation("dEQP-GLES31.functional.layout_binding.ubo.vertex_binding_max"),
        GPUTestExpectationsParser::kGpuTestPass);
}

// A correct entry with a test that's timed out on all platforms should not lead
// to any errors, and should properly return the expectation TIMEOUT.
TEST(GPUTestExpectationsParserTest, GPUTestExpectationsParserTimeout)
{
    GPUTestConfigTester config;
    std::string line =
        R"(100 : dEQP-GLES31.functional.layout_binding.ubo.vertex_binding_max = TIMEOUT)";
    GPUTestExpectationsParser parser;
    EXPECT_TRUE(parser.loadTestExpectations(config, line));
    EXPECT_TRUE(parser.getErrorMessages().empty());
    EXPECT_EQ(
        parser.getTestExpectation("dEQP-GLES31.functional.layout_binding.ubo.vertex_binding_max"),
        GPUTestExpectationsParser::kGpuTestTimeout);
}

// A correct entry with a test that's flaky on all platforms should not lead
// to any errors, and should properly return the expectation FLAKY.
TEST(GPUTestExpectationsParserTest, GPUTestExpectationsParserFlaky)
{
    GPUTestConfigTester config;
    std::string line =
        R"(100 : dEQP-GLES31.functional.layout_binding.ubo.vertex_binding_max = FLAKY)";
    GPUTestExpectationsParser parser;
    EXPECT_TRUE(parser.loadTestExpectations(config, line));
    EXPECT_TRUE(parser.getErrorMessages().empty());
    EXPECT_EQ(
        parser.getTestExpectation("dEQP-GLES31.functional.layout_binding.ubo.vertex_binding_max"),
        GPUTestExpectationsParser::kGpuTestFlaky);
}

// A correct entry with a test that's skipped on windows should not lead
// to any errors, and should properly return the expectation SKIP on this
// tester.
TEST(GPUTestExpectationsParserTest, GPUTestExpectationsParserSingleLineWin)
{
    GPUTestConfigTester config;
    std::string line =
        R"(100 WIN : dEQP-GLES31.functional.layout_binding.ubo.vertex_binding_max = SKIP)";
    GPUTestExpectationsParser parser;
    EXPECT_TRUE(parser.loadTestExpectations(config, line));
    EXPECT_TRUE(parser.getErrorMessages().empty());
    EXPECT_EQ(
        parser.getTestExpectation("dEQP-GLES31.functional.layout_binding.ubo.vertex_binding_max"),
        GPUTestExpectationsParser::kGpuTestSkip);
}

// A correct entry with a test that's skipped on windows/NVIDIA should not lead
// to any errors, and should properly return the expectation SKIP on this
// tester.
TEST(GPUTestExpectationsParserTest, GPUTestExpectationsParserSingleLineWinNVIDIA)
{
    GPUTestConfigTester config;
    std::string line =
        R"(100 WIN NVIDIA : dEQP-GLES31.functional.layout_binding.ubo.vertex_binding_max = SKIP)";
    GPUTestExpectationsParser parser;
    EXPECT_TRUE(parser.loadTestExpectations(config, line));
    EXPECT_TRUE(parser.getErrorMessages().empty());
    EXPECT_EQ(
        parser.getTestExpectation("dEQP-GLES31.functional.layout_binding.ubo.vertex_binding_max"),
        GPUTestExpectationsParser::kGpuTestSkip);
}

// A correct entry with a test that's skipped on windows/NVIDIA/D3D11 should not
// lead to any errors, and should properly return the expectation SKIP on this
// tester.
TEST(GPUTestExpectationsParserTest, GPUTestExpectationsParserSingleLineWinNVIDIAD3D11)
{
    GPUTestConfigTester config;
    std::string line =
        R"(100 WIN NVIDIA D3D11 : dEQP-GLES31.functional.layout_binding.ubo.vertex_binding_max = SKIP)";
    GPUTestExpectationsParser parser;
    EXPECT_TRUE(parser.loadTestExpectations(config, line));
    EXPECT_TRUE(parser.getErrorMessages().empty());
    EXPECT_EQ(
        parser.getTestExpectation("dEQP-GLES31.functional.layout_binding.ubo.vertex_binding_max"),
        GPUTestExpectationsParser::kGpuTestSkip);
}

// Same as GPUTestExpectationsParserSingleLineWinNVIDIAD3D11, but verifying that the order
// of these conditions doesn't matter
TEST(GPUTestExpectationsParserTest, GPUTestExpectationsParserSingleLineWinNVIDIAD3D11OtherOrder)
{
    GPUTestConfigTester config;
    std::string line =
        R"(100 D3D11 NVIDIA WIN : dEQP-GLES31.functional.layout_binding.ubo.vertex_binding_max = SKIP)";
    GPUTestExpectationsParser parser;
    EXPECT_TRUE(parser.loadTestExpectations(config, line));
    EXPECT_TRUE(parser.getErrorMessages().empty());
    EXPECT_EQ(
        parser.getTestExpectation("dEQP-GLES31.functional.layout_binding.ubo.vertex_binding_max"),
        GPUTestExpectationsParser::kGpuTestSkip);
}

// A correct entry with a test that's skipped on mac should not lead
// to any errors, and should default to PASS on this tester (windows).
TEST(GPUTestExpectationsParserTest, GPUTestExpectationsParserSingleLineMac)
{
    GPUTestConfigTester config;
    std::string line =
        R"(100 MAC : dEQP-GLES31.functional.layout_binding.ubo.vertex_binding_max = SKIP)";
    GPUTestExpectationsParser parser;
    EXPECT_TRUE(parser.loadTestExpectations(config, line));
    EXPECT_TRUE(parser.getErrorMessages().empty());
    EXPECT_EQ(
        parser.getTestExpectation("dEQP-GLES31.functional.layout_binding.ubo.vertex_binding_max"),
        GPUTestExpectationsParser::kGpuTestPass);
}

// A correct entry with a test that has conflicting entries should not lead
// to any errors, and should default to PASS.
// (https:anglebug.com/3368) In the future, this condition should cause an
// error
TEST(GPUTestExpectationsParserTest, GPUTestExpectationsParserSingleLineConflict)
{
    GPUTestConfigTester config;
    std::string line =
        R"(100 WIN MAC : dEQP-GLES31.functional.layout_binding.ubo.vertex_binding_max = SKIP)";
    GPUTestExpectationsParser parser;
    EXPECT_TRUE(parser.loadTestExpectations(config, line));
    EXPECT_TRUE(parser.getErrorMessages().empty());
    EXPECT_EQ(
        parser.getTestExpectation("dEQP-GLES31.functional.layout_binding.ubo.vertex_binding_max"),
        GPUTestExpectationsParser::kGpuTestPass);
}

// A line without a bug ID should return an error and not add the expectation.
TEST(GPUTestExpectationsParserTest, GPUTestExpectationsParserMissingBugId)
{
    GPUTestConfigTester config;
    std::string line = R"( : dEQP-GLES31.functional.layout_binding.ubo.vertex_binding_max = SKIP)";
    GPUTestExpectationsParser parser;
    EXPECT_FALSE(parser.loadTestExpectations(config, line));
    EXPECT_EQ(parser.getErrorMessages().size(), 1u);
    if (parser.getErrorMessages().size() >= 1)
    {
        EXPECT_EQ(parser.getErrorMessages()[0], "Line 1 : entry with wrong format");
    }
    // Default behavior is to let missing tests pass
    EXPECT_EQ(
        parser.getTestExpectation("dEQP-GLES31.functional.layout_binding.ubo.vertex_binding_max"),
        GPUTestExpectationsParser::kGpuTestPass);
}

// A line without a bug ID should return an error and not add the expectation, (even if
// the line contains conditions that might be mistaken for a bug id)
TEST(GPUTestExpectationsParserTest, GPUTestExpectationsParserMissingBugIdWithConditions)
{
    GPUTestConfigTester config;
    std::string line =
        R"(WIN D3D11 : dEQP-GLES31.functional.layout_binding.ubo.vertex_binding_max = SKIP)";
    GPUTestExpectationsParser parser;
    EXPECT_FALSE(parser.loadTestExpectations(config, line));
    EXPECT_EQ(parser.getErrorMessages().size(), 1u);
    if (parser.getErrorMessages().size() >= 1)
    {
        EXPECT_EQ(parser.getErrorMessages()[0], "Line 1 : entry with wrong format");
    }
    // Default behavior is to let missing tests pass
    EXPECT_EQ(
        parser.getTestExpectation("dEQP-GLES31.functional.layout_binding.ubo.vertex_binding_max"),
        GPUTestExpectationsParser::kGpuTestPass);
}

// A line without a colon should return an error and not add the expectation.
TEST(GPUTestExpectationsParserTest, GPUTestExpectationsParserMissingColon)
{
    GPUTestConfigTester config;
    std::string line = R"(100 dEQP-GLES31.functional.layout_binding.ubo.vertex_binding_max = SKIP)";
    GPUTestExpectationsParser parser;
    EXPECT_FALSE(parser.loadTestExpectations(config, line));
    EXPECT_EQ(parser.getErrorMessages().size(), 1u);
    if (parser.getErrorMessages().size() >= 1)
    {
        EXPECT_EQ(parser.getErrorMessages()[0], "Line 1 : entry with wrong format");
    }
    // Default behavior is to let missing tests pass
    EXPECT_EQ(
        parser.getTestExpectation("dEQP-GLES31.functional.layout_binding.ubo.vertex_binding_max"),
        GPUTestExpectationsParser::kGpuTestPass);
}

// A wild character (*) at the end of a line should match any expectations that are a subset of that
// line. It should not greedily match to omany expectations that aren't in that subset.
TEST(GPUTestExpectationsParserTest, GPUTestExpectationsParserWildChar)
{
    GPUTestConfigTester config;
    std::string line = R"(100 : dEQP-GLES31.functional.layout_binding.ubo.* = SKIP)";
    GPUTestExpectationsParser parser;
    EXPECT_TRUE(parser.loadTestExpectations(config, line));
    EXPECT_TRUE(parser.getErrorMessages().empty());
    EXPECT_EQ(
        parser.getTestExpectation("dEQP-GLES31.functional.layout_binding.ubo.vertex_binding_max"),
        GPUTestExpectationsParser::kGpuTestSkip);
    // Also ensure the wild char is not too wild, only covers tests that are more specific
    EXPECT_EQ(parser.getTestExpectation(
                  "dEQP-GLES31.functional.program_interface_query.transform_feedback_varying."
                  "resource_list.vertex_fragment.builtin_gl_position"),
              GPUTestExpectationsParser::kGpuTestPass);
}

// A line without an equals should return an error and not add the expectation.
TEST(GPUTestExpectationsParserTest, GPUTestExpectationsParserMissingEquals)
{
    GPUTestConfigTester config;
    std::string line = R"(100 : dEQP-GLES31.functional.layout_binding.ubo.vertex_binding_max SKIP)";
    GPUTestExpectationsParser parser;
    EXPECT_FALSE(parser.loadTestExpectations(config, line));
    EXPECT_EQ(parser.getErrorMessages().size(), 1u);
    if (parser.getErrorMessages().size() >= 1)
    {
        EXPECT_EQ(parser.getErrorMessages()[0], "Line 1 : entry with wrong format");
    }
    // Default behavior is to let missing tests pass
    EXPECT_EQ(
        parser.getTestExpectation("dEQP-GLES31.functional.layout_binding.ubo.vertex_binding_max"),
        GPUTestExpectationsParser::kGpuTestPass);
}

// A line without an expectation should return an error and not add the expectation.
TEST(GPUTestExpectationsParserTest, GPUTestExpectationsParserMissingExpectation)
{
    GPUTestConfigTester config;
    std::string line = R"(100 : dEQP-GLES31.functional.layout_binding.ubo.vertex_binding_max =)";
    GPUTestExpectationsParser parser;
    EXPECT_FALSE(parser.loadTestExpectations(config, line));
    EXPECT_EQ(parser.getErrorMessages().size(), 1u);
    if (parser.getErrorMessages().size() >= 1)
    {
        EXPECT_EQ(parser.getErrorMessages()[0], "Line 1 : entry with wrong format");
    }
    // Default behavior is to let missing tests pass
    EXPECT_EQ(
        parser.getTestExpectation("dEQP-GLES31.functional.layout_binding.ubo.vertex_binding_max"),
        GPUTestExpectationsParser::kGpuTestPass);
}

// A line with an expectation that doesn't exist should return an error and not add the expectation.
TEST(GPUTestExpectationsParserTest, GPUTestExpectationsParserInvalidExpectation)
{
    GPUTestConfigTester config;
    std::string line =
        R"(100 : dEQP-GLES31.functional.layout_binding.ubo.vertex_binding_max = WRONG)";
    GPUTestExpectationsParser parser;
    EXPECT_FALSE(parser.loadTestExpectations(config, line));
    EXPECT_EQ(parser.getErrorMessages().size(), 1u);
    if (parser.getErrorMessages().size() >= 1)
    {
        EXPECT_EQ(parser.getErrorMessages()[0], "Line 1 : entry with wrong format");
    }
    // Default behavior is to let missing tests pass
    EXPECT_EQ(
        parser.getTestExpectation("dEQP-GLES31.functional.layout_binding.ubo.vertex_binding_max"),
        GPUTestExpectationsParser::kGpuTestPass);
}

// ChromeOS is reserved as a token, but doesn't actually check any conditions. Any tokens that
// do not check conditions should return an error and not add the expectation
// (https://anglebug.com/3363) Remove/update this test when ChromeOS is supported
TEST(GPUTestExpectationsParserTest, GPUTestExpectationsParserUnimplementedCondition)
{
    GPUTestConfigTester config;
    std::string line =
        R"(100 CHROMEOS : dEQP-GLES31.functional.layout_binding.ubo.vertex_binding_max = SKIP)";
    GPUTestExpectationsParser parser;
    EXPECT_FALSE(parser.loadTestExpectations(config, line));
    EXPECT_EQ(parser.getErrorMessages().size(), 1u);
    if (parser.getErrorMessages().size() >= 1)
    {
        EXPECT_EQ(parser.getErrorMessages()[0],
                  "Line 1 : entry invalid, likely unimplemented modifiers");
    }
    // Default behavior is to let missing tests pass
    EXPECT_EQ(
        parser.getTestExpectation("dEQP-GLES31.functional.layout_binding.ubo.vertex_binding_max"),
        GPUTestExpectationsParser::kGpuTestPass);
}

// If a line starts with a comment, it's ignored and should not be added to the list.
TEST(GPUTestExpectationsParserTest, GPUTestExpectationsParserComment)
{
    GPUTestConfigTester config;
    std::string line =
        R"(//100 : dEQP-GLES31.functional.layout_binding.ubo.vertex_binding_max = SKIP)";
    GPUTestExpectationsParser parser;
    EXPECT_TRUE(parser.loadTestExpectations(config, line));
    EXPECT_TRUE(parser.getErrorMessages().empty());
    EXPECT_EQ(
        parser.getTestExpectation("dEQP-GLES31.functional.layout_binding.ubo.vertex_binding_max"),
        GPUTestExpectationsParser::kGpuTestPass);
}

// A misspelled expectation should not be matched from getTestExpectation, and should lead to an
// unused expectation when later queried.
TEST(GPUTestExpectationsParserTest, GPUTestExpectationsParserMisspelledExpectation)
{
    GPUTestConfigTester config;
    std::string line =
        R"(100 : dEQP-GLES31.functionaal.layout_binding.ubo.* = SKIP)";  // "functionaal"
    GPUTestExpectationsParser parser;
    EXPECT_TRUE(parser.loadTestExpectations(config, line));
    EXPECT_TRUE(parser.getErrorMessages().empty());
    // Default behavior is to let missing tests pass
    EXPECT_EQ(
        parser.getTestExpectation("dEQP-GLES31.functional.layout_binding.ubo.vertex_binding_max"),
        GPUTestExpectationsParser::kGpuTestPass);
    EXPECT_EQ(parser.getUnusedExpectationsMessages().size(), 1u);
    if (parser.getUnusedExpectationsMessages().size() >= 1)
    {
        EXPECT_EQ(parser.getUnusedExpectationsMessages()[0], "Line 1: expectation was unused.");
    }
}

// Wild characters that match groups of expectations can be overridden with more specific lines.
// The parse should still compute correctly which lines were used and which were unused.
TEST(GPUTestExpectationsParserTest, GPUTestExpectationsParserOverrideExpectation)
{
    GPUTestConfigTester config;
    // Fail all layout_binding tests, but skip the layout_binding.ubo subset.
    std::string line = R"(100 : dEQP-GLES31.functional.layout_binding.* = FAIL
100 : dEQP-GLES31.functional.layout_binding.ubo.* = SKIP)";
    GPUTestExpectationsParser parser;
    EXPECT_TRUE(parser.loadTestExpectations(config, line));
    EXPECT_TRUE(parser.getErrorMessages().empty());
    // Default behavior is to let missing tests pass
    EXPECT_EQ(
        parser.getTestExpectation("dEQP-GLES31.functional.layout_binding.ubo.vertex_binding_max"),
        GPUTestExpectationsParser::kGpuTestSkip);
    // The FAIL expectation was unused because it was overridden.
    EXPECT_EQ(parser.getUnusedExpectationsMessages().size(), 1u);
    if (parser.getUnusedExpectationsMessages().size() >= 1)
    {
        EXPECT_EQ(parser.getUnusedExpectationsMessages()[0], "Line 1: expectation was unused.");
    }
    // Now try a test that doesn't match the override criteria
    EXPECT_EQ(parser.getTestExpectation("dEQP-GLES31.functional.layout_binding.image.test"),
              GPUTestExpectationsParser::kGpuTestFail);
    EXPECT_TRUE(parser.getUnusedExpectationsMessages().empty());
}

// This test is the same as GPUTestExpectationsParserOverrideExpectation, but verifying the order
// doesn't matter when overriding.
TEST(GPUTestExpectationsParserTest, GPUTestExpectationsParserOverrideExpectationOtherOrder)
{
    GPUTestConfigTester config;
    // Fail all layout_binding tests, but skip the layout_binding.ubo subset.
    std::string line = R"(100 : dEQP-GLES31.functional.layout_binding.ubo.* = SKIP
100 : dEQP-GLES31.functional.layout_binding.* = FAIL)";
    GPUTestExpectationsParser parser;
    EXPECT_TRUE(parser.loadTestExpectations(config, line));
    EXPECT_TRUE(parser.getErrorMessages().empty());
    // Default behavior is to let missing tests pass
    EXPECT_EQ(
        parser.getTestExpectation("dEQP-GLES31.functional.layout_binding.ubo.vertex_binding_max"),
        GPUTestExpectationsParser::kGpuTestSkip);
    // The FAIL expectation was unused because it was overridden.
    EXPECT_EQ(parser.getUnusedExpectationsMessages().size(), 1u);
    if (parser.getUnusedExpectationsMessages().size() >= 1)
    {
        EXPECT_EQ(parser.getUnusedExpectationsMessages()[0], "Line 2: expectation was unused.");
    }
    // Now try a test that doesn't match the override criteria
    EXPECT_EQ(parser.getTestExpectation("dEQP-GLES31.functional.layout_binding.image.test"),
              GPUTestExpectationsParser::kGpuTestFail);
    EXPECT_TRUE(parser.getUnusedExpectationsMessages().empty());
}

}  // anonymous namespace
