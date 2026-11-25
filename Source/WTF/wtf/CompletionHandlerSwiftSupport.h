/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <type_traits>
#include <wtf/Compiler.h>

// The parts of CompletionHandler that the `CxxCompletionHandler` Swift protocol in WTF's Swift module names. They are
// kept apart from CompletionHandler.h so that WTF's Swift does not import the rest of WTF along with them.

namespace WTF {

// The C ABI a Swift closure reduces to, used by the `CxxCompletionHandler` Swift protocol to build a
// CompletionHandler out of a Swift closure.
using SwiftClosureInvoke = void (* WTF_NONNULL)(void* WTF_NONNULL context, void* WTF_NULLABLE argument);
using SwiftClosureDestroy = void (* WTF_NONNULL)(void* WTF_NONNULL context);

#ifdef __swift__
// How a CompletionHandler passes its argument to a Swift closure, mirroring passArgumentToSwift(). The
// `CxxCompletionHandler` Swift protocol uses these to pick the initializer that matches.
struct SwiftNoArgument { };
struct SwiftCopiedArgument { };
struct SwiftTakenArgument { };

// Completion handlers with more than one argument also use this, and the Swift closure constructor
// rejects them.
template<typename... In> struct SwiftCompletionHandlerArgument {
    // Swift cannot use void as an associated type, so the tag stands in for it.
    using Type = SwiftNoArgument;
    using Passing = SwiftNoArgument;
};

template<typename In> struct SwiftCompletionHandlerArgument<In> {
    using Type = std::remove_cvref_t<In>;
    // This must not require a complete type, since it is instantiated with the CompletionHandler. That
    // is why it goes by the parameter's declaration, which passArgumentToSwift() static-asserts is an
    // rvalue reference exactly when the type is not copyable.
    using Passing = std::conditional_t<std::is_rvalue_reference_v<In>, SwiftTakenArgument, SwiftCopiedArgument>;
};
#endif

} // namespace WTF
