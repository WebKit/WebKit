// Copyright (C) 2025 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
// BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
// THE POSSIBILITY OF SUCH DAMAGE.

#if ENABLE_SWIFT_DEMO_URI_SCHEME

import Foundation
internal import WebKit_Internal

extension Data {
    var bytes: [UInt8] {
        [UInt8](self)
    }
}

// This function is contrived to exercise bi-directional C++ -> Swift -> C++
// calls within the main WebKit target. It can be tested by enabling
// ENABLE_SWIFT_DEMO_DATA_URL_ENCODING, and then visiting
// x-swift-demo:// in the browser. The goal is to provide an easy
// but comprehensive test case to determine whether Swift-C++ interop
// is working in a given toolchain.
/// Return data for a PNG of a Swift logo.
@_expose(Cxx)
@_spi(Private) // FIXME: rdar://159211965
public func getSwiftLogoData() -> [UInt8] {
    guard WebKit.shouldShowSwiftDemoLogo() else {
        return []
    }
    // From "Download logo and guidelines" linked from
    // https://developer.apple.com/swift/resources/
    // Swift_logo_color.svg converted to PNG
    // The Swift logo is a trademark of Apple Inc.
    let swiftLogoBase64 = """
        iVBORw0KGgoAAAANSUhEUgAAAGoAAABqCAYAAABUIcSXAAAABGdBTUEAALGPC/xhBQAAAC\
        BjSFJNAAB6JgAAgIQAAPoAAACA6AAAdTAAAOpgAAA6mAAAF3CculE8AAAAUGVYSWZNTQAq\
        AAAACAACARIAAwAAAAEAAQAAh2kABAAAAAEAAAAmAAAAAAADoAEAAwAAAAEAAQAAoAIABA\
        AAAAEAAABqoAMABAAAAAEAAABqAAAAAMNytqgAAAI0aVRYdFhNTDpjb20uYWRvYmUueG1w\
        AAAAAAA8eDp4bXBtZXRhIHhtbG5zOng9ImFkb2JlOm5zOm1ldGEvIiB4OnhtcHRrPSJYTV\
        AgQ29yZSA2LjAuMCI+CiAgIDxyZGY6UkRGIHhtbG5zOnJkZj0iaHR0cDovL3d3dy53My5v\
        cmcvMTk5OS8wMi8yMi1yZGYtc3ludGF4LW5zIyI+CiAgICAgIDxyZGY6RGVzY3JpcHRpb2\
        4gcmRmOmFib3V0PSIiCiAgICAgICAgICAgIHhtbG5zOmV4aWY9Imh0dHA6Ly9ucy5hZG9i\
        ZS5jb20vZXhpZi8xLjAvIgogICAgICAgICAgICB4bWxuczp0aWZmPSJodHRwOi8vbnMuYW\
        RvYmUuY29tL3RpZmYvMS4wLyI+CiAgICAgICAgIDxleGlmOlBpeGVsWURpbWVuc2lvbj4x\
        MDEwPC9leGlmOlBpeGVsWURpbWVuc2lvbj4KICAgICAgICAgPGV4aWY6UGl4ZWxYRGltZW\
        5zaW9uPjE2ODc8L2V4aWY6UGl4ZWxYRGltZW5zaW9uPgogICAgICAgICA8ZXhpZjpDb2xv\
        clNwYWNlPjE8L2V4aWY6Q29sb3JTcGFjZT4KICAgICAgICAgPHRpZmY6T3JpZW50YXRpb2\
        4+MTwvdGlmZjpPcmllbnRhdGlvbj4KICAgICAgPC9yZGY6RGVzY3JpcHRpb24+CiAgIDwv\
        cmRmOlJERj4KPC94OnhtcG1ldGE+CsAowq0AABF4SURBVHgB7V0LvFTTGv/23vM+5xAqFd\
        FLJSIUSiF5Py+XyK2fPC8hutUVJaWHioqKhMjNT9crj+tRXt1IIiI9rnchEaV0zpn37Lnf\
        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\
        RU5ErkJggg==
        """
    guard let data = Data(base64Encoded: swiftLogoBase64) else {
        return []
    }
    return data.bytes
}

#endif
