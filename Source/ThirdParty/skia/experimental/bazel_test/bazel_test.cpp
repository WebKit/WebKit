// Copyright 2020 Google LLC.
// Use of this source code is governed by a BSD-style license that can be found in the LICENSE file.

#include "include/core/SkCanvas.h"
#include "include/core/SkRect.h"
#include "include/core/SkTypes.h"
#include "include/svg/SkSVGCanvas.h"

#include "png.h"

#include <stdio.h>
#include <ctime>
#include <iostream>

void print_localtime() {
    std::time_t result = std::time(nullptr);
    std::cout << std::asctime(std::localtime(&result));
}

int main(int argc, char** argv) {
    SkDebugf("Hello world\n");
    print_localtime();
    // https://docs.bazel.build/versions/main/test-encyclopedia.html#role-of-the-test-runner
    if (png_access_version_number() == 10638) {
        printf("PASS\n"); // This tells the human the test passed.
        return 0; // This tells Bazel the test passed.
    }
    if (argc < -10) {
        std::unique_ptr<SkCanvas> not_used = SkSVGCanvas::Make({}, nullptr, 0);
        not_used->save();
    }
    printf("FAIL\n"); // This tells the human the test failed.
    return 1; // This tells Bazel the test failed.
}
