/*
 *  Copyright (c) 2017 Apple Inc. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#pragma once

namespace webrtc {

template<typename T> class NeverDestroyed {
    NeverDestroyed(const NeverDestroyed&) = delete;
    NeverDestroyed& operator=(const NeverDestroyed&) = delete;
public:
    template<typename... Args>
    NeverDestroyed(Args&&... args)
    {
        new (&m_storage) T(std::forward<Args>(args)...);
    }
    operator T&() { return *asPtr(); }
    T& get() { return *asPtr(); }
    T* operator&() { return asPtr(); }
private:
    typedef typename std::remove_const<T>::type* PointerType;
    PointerType asPtr() { return reinterpret_cast<PointerType>(&m_storage); }
    typename std::aligned_storage<sizeof(T), std::alignment_of<T>::value>::type m_storage;
};

}
