/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/CompactPtr.h>
#include <wtf/CompactRefPtr.h>
#include <wtf/Packed.h>
#include <wtf/PackedRefPtr.h>

namespace WTF {

// Keep in mind that updating this pointer is not atomic since it can be a PackedPtr.
//
// This is interim migration type from PackedPtr to CompactPtr. Once CompactPtr becomes available with efficient size in macOS, we will replace them with CompactPtr and remove this type.
// But until that, we need to switch the underlying implementation.
//
// If CompactPtr is 32bit, it is more efficient than PackedPtr (6 bytes).
// We select underlying implementation based on CompactPtr's efficacy.
template<typename T>
using SizeEfficientPtr = std::conditional_t<CompactPtrTraits<T>::is32Bit, CompactPtr<T>, PackedPtr<T>>;

template<typename T>
using SizeEfficientRefPtr = std::conditional_t<CompactPtrTraits<T>::is32Bit, CompactRefPtr<T>, PackedRefPtr<T>>;

template<typename T>
using SizeEfficientPtrTraits = std::conditional_t<CompactPtrTraits<T>::is32Bit, CompactPtrTraits<T>, PackedPtrTraits<T>>;

}

using WTF::SizeEfficientPtr;
using WTF::SizeEfficientRefPtr;
using WTF::SizeEfficientPtrTraits;
