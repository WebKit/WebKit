/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>

#ifdef WEBRTC_IOS_ENABLE_COVERAGE
extern "C" void __llvm_profile_set_filename(const char* name);
#endif

namespace rtc {
namespace test {

void ConfigureCoverageReportPath() {
#ifdef WEBRTC_IOS_ENABLE_COVERAGE
  static dispatch_once_t once_token;
  dispatch_once(&once_token, ^{
    // Writes the profraw file to the Documents directory, where the app has
    // write rights.
    NSArray* paths =
        NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    NSString* documents_directory = [paths firstObject];
    NSString* file_name = [documents_directory stringByAppendingPathComponent:@"coverage.profraw"];

    // For documentation, see:
    // http://clang.llvm.org/docs/SourceBasedCodeCoverage.html
    __llvm_profile_set_filename([file_name cStringUsingEncoding:NSUTF8StringEncoding]);

    // Print the path for easier retrieval.
    NSLog(@"Coverage data at %@.", file_name);
  });
#endif  // ifdef WEBRTC_IOS_ENABLE_COVERAGE
}

}  // namespace test
}  // namespace rtc
