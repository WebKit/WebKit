/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#if defined(WEBRTC_IOS)

#import <Foundation/Foundation.h>
#include <string.h>

#import "sdk/objc/helpers/NSString+StdString.h"

#include "absl/strings/string_view.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace test {

// For iOS, resource files are added to the application bundle in the root
// and not in separate folders as is the case for other platforms. This method
// therefore removes any prepended folders and uses only the actual file name.
std::string IOSResourcePath(absl::string_view name, absl::string_view extension) {
  @autoreleasepool {
    NSString* path = [NSString stringForAbslStringView:name];
    NSString* fileName = path.lastPathComponent;
    NSString* fileType = [NSString stringForAbslStringView:extension];
    // Get full pathname for the resource identified by the name and extension.
    NSString* pathString = [[NSBundle mainBundle] pathForResource:fileName
                                                           ofType:fileType];
    return [NSString stdStringForString:pathString];
  }
}

std::string IOSRootPath() {
  @autoreleasepool {
    NSBundle* mainBundle = [NSBundle mainBundle];
    return [NSString stdStringForString:mainBundle.bundlePath] + "/";
  }
}

// For iOS, we don't have access to the output directory. Return the path to the
// temporary directory instead. This is mostly used by tests that need to write
// output files to disk.
std::string IOSOutputPath()  {
  @autoreleasepool {
    NSString* tempDir = NSTemporaryDirectory();
    if (tempDir == nil)
        tempDir = @"/tmp";
    return [NSString stdStringForString:tempDir];
  }
}

}  // namespace test
}  // namespace webrtc

#endif  // defined(WEBRTC_IOS)
