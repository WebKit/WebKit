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

/// Supplies the getter for a property shadowing a C++ getter that returns
/// `const T&`.
///
/// The clang importer renames such a getter to `__<name>Unsafe()` returning an
/// `UnsafePointer<T>`, so reading it requires `unsafe` at every call site. Declare
/// the property with the same name as the C++ getter and this macro fills in a
/// getter that copies the pointee:
///
///     extension WebKit.WebBackForwardListItem {
///         @CxxCopy var url: WTF.String
///     }
///
/// Only for C++ types that are cheap to copy — a WTF::String, URL, Markable or
/// identifier costs at most a refcount bump. Large non-refcounted types such as
/// a Vector, HashMap or EditorState need a genuine borrow, which this does not
/// provide.
@attached(accessor)
macro CxxCopy() = #externalMacro(module: "WebKitSwiftMacros", type: "CxxCopyAccessorMacro")

#endif // compiler(>=6.2.3)
