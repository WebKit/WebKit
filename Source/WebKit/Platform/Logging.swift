// Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if compiler(>=6.2.3)

// Needs the WTF.String conversions in StdlibExtras.swift.
#if ENABLE_BACK_FORWARD_LIST_SWIFT

import wtf

#if canImport(os)
import os
#endif

/// Swift equivalents of the WTF logging macros, which share their backends:
///
///     WebKit.LogChannel.BackForward().log("...")          // LOG, LOG_WITH_STREAM
///     WebKit.LogChannel.Loading().releaseLog.log("...")   // RELEASE_LOG
///     WebKit.LogChannel.Loading().releaseLog.error("...") // RELEASE_LOG_ERROR, and likewise info, debug, fault
///
/// `releaseLog` is an `os.Logger` on Cocoa, where RELEASE_LOG is os_log. Use only the `privacy:`
/// interpolation option so that code still builds against `ReleaseLogger` on other platforms.
extension WTFLogChannel {
    @inline(__always)
    func log(_ message: @autoclosure () -> String) {
        if WTF.logEnabledForSwift && state != .Off {
            WTF.logPreformatted(self, WTF.String(message()))
        }
    }

    #if canImport(os)
    var releaseLog: os.Logger {
        os.Logger(WTF.osLog(self))
    }
    #else
    var releaseLog: ReleaseLogger {
        ReleaseLogger(channel: self)
    }
    #endif
}

#if !canImport(os)

/// The subset of `os.OSLogPrivacy` available to portable code.
///
/// Ignored, since these backends do not redact.
struct ReleaseLogPrivacy {
    static var `public`: Self { Self() }
    static var `private`: Self { Self() }
    static var sensitive: Self { Self() }
    static var auto: Self { Self() }
}

struct ReleaseLogMessage: ExpressibleByStringInterpolation {
    struct StringInterpolation: StringInterpolationProtocol {
        fileprivate var output = ""

        init(literalCapacity: Int, interpolationCount: Int) {
            output.reserveCapacity(literalCapacity)
        }

        mutating func appendLiteral(_ literal: String) {
            output += literal
        }

        mutating func appendInterpolation<T>(_ value: @autoclosure () -> T, privacy: ReleaseLogPrivacy = .auto) {
            output += String(describing: value())
        }
    }

    fileprivate let output: String

    init(stringLiteral value: String) {
        output = value
    }

    init(stringInterpolation: StringInterpolation) {
        output = stringInterpolation.output
    }
}

/// Stands in for `os.Logger` where RELEASE_LOG is not os_log, e.g. journald or Android.
struct ReleaseLogger {
    let channel: WTFLogChannel

    func log(
        _ message: @autoclosure () -> ReleaseLogMessage,
        file: String = #fileID,
        line: UInt32 = #line,
        function: String = #function
    ) {
        send(.Default, message, file, line, function)
    }

    func notice(
        _ message: @autoclosure () -> ReleaseLogMessage,
        file: String = #fileID,
        line: UInt32 = #line,
        function: String = #function
    ) {
        send(.Default, message, file, line, function)
    }

    func info(
        _ message: @autoclosure () -> ReleaseLogMessage,
        file: String = #fileID,
        line: UInt32 = #line,
        function: String = #function
    ) {
        send(.Info, message, file, line, function)
    }

    func debug(
        _ message: @autoclosure () -> ReleaseLogMessage,
        file: String = #fileID,
        line: UInt32 = #line,
        function: String = #function
    ) {
        send(.Debug, message, file, line, function)
    }

    func error(
        _ message: @autoclosure () -> ReleaseLogMessage,
        file: String = #fileID,
        line: UInt32 = #line,
        function: String = #function
    ) {
        send(.Error, message, file, line, function)
    }

    func fault(
        _ message: @autoclosure () -> ReleaseLogMessage,
        file: String = #fileID,
        line: UInt32 = #line,
        function: String = #function
    ) {
        send(.Fault, message, file, line, function)
    }

    private func send(_ type: WTF.ReleaseLogType, _ message: () -> ReleaseLogMessage, _ file: String, _ line: UInt32, _ function: String) {
        guard channel.state != .Off else {
            return
        }
        WTF.releaseLogPreformatted(channel, type, WTF.String(file), line, WTF.String(function), WTF.String(message().output))
    }
}

#endif // !canImport(os)

#endif // ENABLE_BACK_FORWARD_LIST_SWIFT

#endif // compiler(>=6.2.3)
