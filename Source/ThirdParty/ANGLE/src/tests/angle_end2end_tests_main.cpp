//
// Copyright 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "gtest/gtest.h"
#include "test_utils/runner/TestSuite.h"

void ANGLEProcessTestArgs(int *argc, char *argv[]);

int main(int argc, char **argv)
{
    angle::TestSuite testSuite(&argc, argv);
    ANGLEProcessTestArgs(&argc, argv);
    return testSuite.run();
}
