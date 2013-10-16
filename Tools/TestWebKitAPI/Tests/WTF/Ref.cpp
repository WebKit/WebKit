/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#include "RefLogger.h"
#include <wtf/PassRef.h>
#include <wtf/Ref.h>
#include <wtf/RefPtr.h>

namespace TestWebKitAPI {

TEST(WTF_Ref, Basic)
{
    DerivedRefLogger a("a");

    {
        Ref<RefLogger> ptr(a);
        ASSERT_EQ(&a, &ptr.get());
        ASSERT_EQ(&a.name, &ptr->name);
    }
    ASSERT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        Ref<RefLogger> ptr(adoptRef(a));
        ASSERT_EQ(&a, &ptr.get());
        ASSERT_EQ(&a.name, &ptr->name);
    }
    ASSERT_STREQ("deref(a) ", takeLogStr().c_str());
}

TEST(WTF_Ref, Assignment)
{
    DerivedRefLogger a("a");
    RefLogger b("b");
    DerivedRefLogger c("c");

    {
        Ref<RefLogger> ptr(a);
        ASSERT_EQ(&a, &ptr.get());
        log() << "| ";
        ptr = b;
        ASSERT_EQ(&b, &ptr.get());
        log() << "| ";
    }
    ASSERT_STREQ("ref(a) | ref(b) deref(a) | deref(b) ", takeLogStr().c_str());

    {
        Ref<RefLogger> ptr(a);
        ASSERT_EQ(&a, &ptr.get());
        log() << "| ";
        ptr = c;
        ASSERT_EQ(&c, &ptr.get());
        log() << "| ";
    }
    ASSERT_STREQ("ref(a) | ref(c) deref(a) | deref(c) ", takeLogStr().c_str());

    {
        Ref<RefLogger> ptr(a);
        ASSERT_EQ(&a, &ptr.get());
        log() << "| ";
        ptr = adoptRef(b);
        ASSERT_EQ(&b, &ptr.get());
        log() << "| ";
    }
    ASSERT_STREQ("ref(a) | deref(a) | deref(b) ", takeLogStr().c_str());

    {
        Ref<RefLogger> ptr(a);
        ASSERT_EQ(&a, &ptr.get());
        log() << "| ";
        ptr = adoptRef(c);
        ASSERT_EQ(&c, &ptr.get());
        log() << "| ";
    }
    ASSERT_STREQ("ref(a) | deref(a) | deref(c) ", takeLogStr().c_str());
}

PassRef<RefLogger> passWithPassRef(PassRef<RefLogger> reference)
{
    return reference;
}

RefPtr<RefLogger> passWithPassRefPtr(PassRefPtr<RefLogger> reference)
{
    return reference;
}

TEST(WTF_Ref, ReturnValue)
{
    DerivedRefLogger a("a");
    RefLogger b("b");
    DerivedRefLogger c("c");

    {
        Ref<RefLogger> ptr(passWithPassRef(a));
        ASSERT_EQ(&a, &ptr.get());
    }
    ASSERT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        Ref<RefLogger> ptr(a);
        ASSERT_EQ(&a, &ptr.get());
        log() << "| ";
        ptr = passWithPassRef(b);
        ASSERT_EQ(&b, &ptr.get());
        log() << "| ";
    }
    ASSERT_STREQ("ref(a) | ref(b) deref(a) | deref(b) ", takeLogStr().c_str());

    {
        RefPtr<RefLogger> ptr(passWithPassRef(a));
        ASSERT_EQ(&a, ptr.get());
    }
    ASSERT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());

    {
        RefPtr<RefLogger> ptr(passWithPassRefPtr(passWithPassRef(a)));
        ASSERT_EQ(&a, ptr.get());
    }
    ASSERT_STREQ("ref(a) deref(a) ", takeLogStr().c_str());
}

} // namespace TestWebKitAPI
