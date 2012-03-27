//
// Copyright (c) 2011 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// stl_utils.h: Common STL utilities.

#ifndef COMMON_STLUTILS_H_
#define COMMON_STLUTILS_H_

namespace pp
{

struct Delete
{
    template<class T>
    void operator() (T x) { delete x; }
};

struct DeleteSecond
{
    template<class A, class B>
    void operator() (std::pair<A, B>& x) { delete x.second; }
};

} // namespace pp
#endif // COMMON_STLUTILS_H_

