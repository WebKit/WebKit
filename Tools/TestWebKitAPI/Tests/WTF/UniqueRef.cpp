/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "config.h"

#include <wtf/UniqueRef.h>
#include <wtf/Vector.h>

namespace TestWebKitAPI {
    
class A {
    WTF_MAKE_FAST_ALLOCATED;
};
class B {
    WTF_MAKE_FAST_ALLOCATED;
public:
    B(int a, int b, int c)
        : a(a)
        , b(b)
        , c(c)
    { };
    int a;
    int b;
    int c;
};
class C {
    WTF_MAKE_FAST_ALLOCATED;
public:
    C(UniqueRef<A>&& a)
        : a(WTFMove(a))
    { }
    UniqueRef<A> a;
};
class D : public A { };

void function(const UniqueRef<A> a)
{
    const A& b = a.get();
    const A* c = &a;
    UNUSED_PARAM(b);
    UNUSED_PARAM(c);
}

TEST(WTF, UniqueRef)
{
    UniqueRef<A> a = makeUniqueRef<A>();
    UniqueRef<B> b = makeUniqueRef<B>(1, 2, 3);
    B& c = b.get();
    const B& d = b.get();
    B* e = &b;
    const B* f = &b;
    UniqueRef<A> j = WTFMove(a);
    
    Vector<UniqueRef<B>> v;
    v.append(makeUniqueRef<B>(4, 5, 6));
    v.append(makeUniqueRef<B>(7, 8, 9));
    UniqueRef<B> g = v.takeLast();
    ASSERT_EQ(g->b, 8);
    ASSERT_EQ(v.last()->b, 5);
    
    C h(makeUniqueRef<A>());
    C i(makeUniqueRef<D>());
    
    UNUSED_PARAM(b);
    UNUSED_PARAM(c);
    UNUSED_PARAM(d);
    UNUSED_PARAM(e);
    UNUSED_PARAM(f);
    UNUSED_PARAM(g);
    UNUSED_PARAM(h);
    UNUSED_PARAM(i);
    UNUSED_PARAM(j);
}

} // namespace TestWebKitAPI
