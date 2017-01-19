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

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import <Foundation/Foundation.h>
#include <string.h>

#include "webrtc/base/checks.h"
#include "webrtc/typedefs.h"

namespace webrtc {
namespace test {

// TODO(henrika): move to shared location.
// See https://code.google.com/p/webrtc/issues/detail?id=4773 for details.
NSString* NSStringFromStdString(const std::string& stdString) {
  // std::string may contain null termination character so we construct
  // using length.
  return [[NSString alloc] initWithBytes:stdString.data()
                                  length:stdString.length()
                                encoding:NSUTF8StringEncoding];
}

std::string StdStringFromNSString(NSString* nsString) {
  NSData* charData = [nsString dataUsingEncoding:NSUTF8StringEncoding];
  return std::string(reinterpret_cast<const char*>([charData bytes]),
                     [charData length]);
}

// For iOS, resource files are added to the application bundle in the root
// and not in separate folders as is the case for other platforms. This method
// therefore removes any prepended folders and uses only the actual file name.
std::string IOSResourcePath(std::string name, std::string extension) {
  @autoreleasepool {
    NSString* path = NSStringFromStdString(name);
    NSString* fileName = path.lastPathComponent;
    NSString* fileType = NSStringFromStdString(extension);
    // Get full pathname for the resource identified by the name and extension.
    NSString* pathString = [[NSBundle mainBundle] pathForResource:fileName
                                                           ofType:fileType];
    return StdStringFromNSString(pathString);
  }
}

std::string IOSRootPath() {
  @autoreleasepool {
    NSBundle* mainBundle = [NSBundle mainBundle];
    return StdStringFromNSString(mainBundle.bundlePath) + "/";
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
    return StdStringFromNSString(tempDir);
  }
}

}  // namespace test
}  // namespace webrtc

#endif  // defined(WEBRTC_IOS)
