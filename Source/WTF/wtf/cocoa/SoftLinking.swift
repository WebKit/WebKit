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

public import Foundation

/// Looks up a class from a soft-linked framework and returns it as its Swift metatype.
///
/// - Parameters:
///   - name: The name of the class, without any qualification; for example, `RVPresenter`.
///   - type: The Objective-C class, such as `RVPresenter.self`
/// - Returns: The Swift metatype, which may be used to initialize instances of the class or call class methods.
/// - Note: The framework must already be loaded, e.g. by the `is…FrameworkAvailable()` function that `SOFT_LINK_FRAMEWORK_FOR_HEADER` declares.
/// - Important: This function is unsafe, since it does not statically guarantee that `type` and `name` represent the same Objective-C class at runtime.
@_transparent
@_alwaysEmitIntoClient
@unsafe
public func softLinkedClass<T: NSObject>(named name: String, as type: T.Type) -> T.Type? {
    guard let cls = NSClassFromString(name) else {
        return nil
    }
    return unsafe unsafeBitCast(cls, to: T.Type.self)
}
