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

#include <wtf/SwiftBridging.h>

/// Declares a Swift-only companion for a getter that returns `const _type&`.
///
/// Place it in the class body next to the getter it shadows:
/// @code
/// const String& NODELETE url() const LIFETIME_BOUND;
/// SWIFT_COPYING_ACCESSOR(url, String)
/// @endcode
///
/// Only for fields that are cheap to copy. Do not use it for large
/// non-refcounted fields such as a Vector, HashMap or EditorState; those need a
/// genuine borrow, which this macro does not provide. If the field's type is a
/// SWIFT_SHARED_REFERENCE, neither applies: hand Swift a pointer instead.
#ifdef __swift__
#define SWIFT_COPYING_ACCESSOR(_name, _type) \
    _type _name##CopyForSwift() const SWIFT_NAME(_name()) { return _name(); }
#else
#define SWIFT_COPYING_ACCESSOR(_name, _type)
#endif
