/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

namespace WTF {

// A UnsafePointer<> can be used to hold a pointer whose lifetime is not guaranteed
// and where the dereferencing of that pointer is therefore unsafe. Once assigned
// to an UnsafePoniter<>, the pointer itself cannot be extracted from the class, but
// the class can still be used to test for pointer equality.
template<typename T>
class UnsafePointer {
public:
    typedef typename std::remove_pointer<T>::type ValueType;
    typedef ValueType* PtrType;

    UnsafePointer() : m_pointer(nullptr) { }
    UnsafePointer(PtrType pointer) : m_pointer(pointer) { }

    bool operator==(PtrType pointer) const { return pointer == m_pointer; }
    bool operator!=(PtrType pointer) const { return pointer != m_pointer; }
    operator bool() const { return m_pointer; }

private:
    PtrType m_pointer;
};

template<typename T>
bool operator==(typename UnsafePointer<T>::PtrType barePointer, const UnsafePointer<T>& unsafePointer)
{
    return unsafePointer == barePointer;
}

template<typename T>
bool operator!=(typename UnsafePointer<T>::PtrType barePointer, const UnsafePointer<T>& unsafePointer)
{
    return unsafePointer != barePointer;
}

} // namespace WTF

using WTF::UnsafePointer;
