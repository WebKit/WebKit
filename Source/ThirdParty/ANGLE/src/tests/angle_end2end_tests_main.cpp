//
// Copyright 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "gtest/gtest.h"
#include "test_utils/runner/TestSuite.h"
#include "util/OSWindow.h"

void ANGLEProcessTestArgs(int *argc, char *argv[]);

// Register* functions handle setting up special tests that need complex parameterization.
// GoogleTest relies heavily on static initialization to register test functions. This can
// cause all sorts of issues if the right variables aren't initialized in the right order.
// This register function needs to be called explicitly after static initialization and
// before the test launcher starts. This is a safer and generally better way to initialize
// tests. It's also more similar to how the dEQP Test harness works. In the future we should
// likely specialize more register functions more like dEQP instead of relying on static init.
void RegisterContextCompatibilityTests();

namespace
{
constexpr char kTestExpectationsPath[] = "src/tests/angle_end2end_tests_expectations.txt";
}  // namespace

int main(int argc, char **argv)
{
    angle::TestSuite testSuite(&argc, argv);
    ANGLEProcessTestArgs(&argc, argv);
    RegisterContextCompatibilityTests();

    constexpr size_t kMaxPath = 512;
    std::array<char, kMaxPath> foundDataPath;
    if (!angle::FindTestDataPath(kTestExpectationsPath, foundDataPath.data(), foundDataPath.size()))
    {
        std::cerr << "Unable to find test expectations path (" << kTestExpectationsPath << ")\n";
        return EXIT_FAILURE;
    }

    // end2end test expectations only allow SKIP at the moment.
    testSuite.setTestExpectationsAllowMask(angle::GPUTestExpectationsParser::kGpuTestSkip |
                                           angle::GPUTestExpectationsParser::kGpuTestTimeout);

    if (!testSuite.loadAllTestExpectationsFromFile(std::string(foundDataPath.data())))
    {
        return EXIT_FAILURE;
    }

    return testSuite.run();
}
