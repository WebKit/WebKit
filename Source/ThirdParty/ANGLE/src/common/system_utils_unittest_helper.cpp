//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// system_utils_unittest_helper.cpp: Helper to the SystemUtils.RunApp unittest

#include "common/system_utils_unittest_helper.h"
#include <string.h>
#include "common/system_utils.h"

int main(int argc, char **argv)
{
    if (argc != 3 || strcmp(argv[1], kRunAppTestArg1) != 0 || strcmp(argv[2], kRunAppTestArg2) != 0)
    {
        fprintf(stderr, "Expected command line:\n%s %s %s\n", argv[0], kRunAppTestArg1,
                kRunAppTestArg2);
        return EXIT_FAILURE;
    }

    std::string env = angle::GetEnvironmentVar(kRunAppTestEnvVarName);
    if (env == "")
    {
        printf("%s", kRunAppTestStdout);
        fprintf(stderr, "%s", kRunAppTestStderr);
    }
    else
    {
        fprintf(stderr, "%s", env.c_str());
    }

    return EXIT_SUCCESS;
}
