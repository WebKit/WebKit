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

#if ENABLE_BACK_FORWARD_LIST_SWIFT || (ENABLE_IPC_TESTING_API && ENABLE_IPC_TESTING_SWIFT)

import WebKit_Internal
import wtf

/// The Swift spelling of the C++ MESSAGE_CHECK_* family (Connection.h).
///
/// A failed message check means the message being dispatched is malformed or malicious, so
/// dispatch of that message must be abandoned. In C++ that is a hidden `return` inside a macro;
/// in Swift it is a thrown error, which makes the early exit visible at the call site as `try`
/// and makes propagating it the compiler's job rather than the author's.
///
/// A receiver can name this to declare `throws(InvalidMessage)`, and catch it, but cannot construct
/// one: the stored properties are fileprivate, so the memberwise initializer is too and only
/// `messageCheck(_:)` can originate the failure. A receiver must never swallow it with `try?` or
/// `try!`.
struct InvalidMessage: Error {
    fileprivate let reason: StaticString
    fileprivate let function: StaticString
    fileprivate let file: StaticString
    fileprivate let line: UInt
}

/// Equivalent of MESSAGE_CHECK_BASE / MESSAGE_CHECK_WITH_MESSAGE_BASE.
///
/// Logs and crashes here rather than where the error is caught, so that the failing check is still
/// on the stack as it is for the C++ macros. Marking the in-flight message invalid needs the
/// connection, so that is left to the catch site.
///
@inline(__always)
func messageCheck(
    _ reason: StaticString = "",
    function: StaticString = #function,
    file: StaticString = #fileID,
    line: UInt = #line,
    body: () -> Bool
) throws(InvalidMessage) {
    guard body() else {
        logFailedMessageCheck(reason, function: function, file: file, line: line)
        throw InvalidMessage(reason: reason, function: function, file: file, line: line)
    }
}

// Lets generated dispatch mark every handler call with `try`, whether or not that handler throws.
@inline(__always)
@discardableResult
func mayThrowInvalidMessage<T>(_ result: T) throws(InvalidMessage) -> T {
    result
}

// Returns false if the message was invalid, so that generated dispatch can send a default reply.
@discardableResult
func dispatchMessage(
    on connection: IPC.Connection,
    body: () throws(InvalidMessage) -> Void
) -> Bool {
    do {
        try body()
    } catch {
        markMessageInvalid(error, on: connection)
        return false
    }
    return true
}

func markMessageInvalid(_ error: InvalidMessage, on connection: IPC.Connection) {
    connection.markCurrentlyDispatchedMessageAsInvalid(WTF.String(error.reason.description))
}

private func logFailedMessageCheck(
    _ reason: StaticString,
    function: StaticString,
    file: StaticString,
    line: UInt
) {
    IPC.Connection.logFailedMessageCheck(
        WTF.String(reason.description),
        WTF.String(function.description),
        WTF.String(file.description),
        UInt32(truncatingIfNeeded: line)
    )
}

#endif // ENABLE_BACK_FORWARD_LIST_SWIFT || (ENABLE_IPC_TESTING_API && ENABLE_IPC_TESTING_SWIFT)

#endif // compiler(>=6.2.3)
