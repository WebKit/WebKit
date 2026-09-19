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

// Anything in WebCore needing to use Swift types or functions should include
// this rather than WebCoreSwift-Generated.h directly.

#ifdef __swift__
#warning "You're including WebCore-Swift.h from a C++ header file - don't do that. This may cause circular Swift<->C++ dependencies and build problems."
#endif

// WebCoreSwift-Generated.h is emitted by WebCore's Swift compilation, so its
// includers must build in the sub-target ordered after it.
#if defined(BUILDING_WITH_CMAKE) && !defined(WEBCORE_COMPILING_SWIFT_INTEROP_SUBTARGET)
#error "This source includes WebCore-Swift.h; add it to WebCore_SWIFT_INTEROP_SOURCES in Source/WebCore/CMakeLists.txt."
#endif

// If Swift function parameters or return types depend on C++ types, the
// relevant headers must be included here.

#include "WebCoreSwift-Generated.h"
