/*
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
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

#include "config.h"

#include <mutex>
#include <wtf/PoisonedUniquePtr.h>

namespace TestWebKitAPI {

namespace {

uintptr_t g_poisonA;
uintptr_t g_poisonB;

using PoisonA = Poison<g_poisonA>;
using PoisonB = Poison<g_poisonB>;

static void initializePoisons()
{
    static std::once_flag initializeOnceFlag;
    std::call_once(initializeOnceFlag, [] {
        // Make sure we get 2 different poison values.
        g_poisonA = makePoison();
        while (!g_poisonB || g_poisonB == g_poisonA)
            g_poisonB = makePoison();
    });
}

struct Logger {
    Logger(const char* name, int& destructCount)
        : name(*name)
        , destructCount(destructCount)
    { }

    ~Logger() { ++destructCount; }

    const char& name;
    int& destructCount;
};
    
struct DerivedLogger : Logger {
    DerivedLogger(const char* name, int& destructCount)
        : Logger(name, destructCount)
    { }
};

struct Other {
    Other(const char*, int&)
    { }
};

} // anonymous namespace

TEST(WTF_PoisonedUniquePtr, DISABLED_Basic)
{
    initializePoisons();

    {
        PoisonedUniquePtr<PoisonA, Logger> empty;
        ASSERT_EQ(nullptr, empty.unpoisoned());
        ASSERT_EQ(0u, empty.bits());
    }
    {
        PoisonedUniquePtr<PoisonA, Logger> empty(nullptr);
        ASSERT_EQ(nullptr, empty.unpoisoned());
        ASSERT_EQ(0u, empty.bits());
    }

    {
        int aDestructCount = 0;
        Logger* a = new Logger("a", aDestructCount);
        {
            PoisonedUniquePtr<PoisonA, Logger> ptr(a);
            ASSERT_EQ(0, aDestructCount);
            ASSERT_EQ(a, ptr.unpoisoned());
            ASSERT_EQ(a, &*ptr);
            ASSERT_EQ(&a->name, &ptr->name);

#if ENABLE(POISON)
            uintptr_t ptrBits;
            std::memcpy(&ptrBits, &ptr, sizeof(ptrBits));
            ASSERT_TRUE(ptrBits != bitwise_cast<uintptr_t>(a));
#if ENABLE(POISON_ASSERTS)
            ASSERT_TRUE((PoisonedUniquePtr<PoisonA, Logger>::isPoisoned(ptrBits)));
#endif
#endif // ENABLE(POISON)
        }
        ASSERT_EQ(1, aDestructCount);

        int bDestructCount = 0;
        DerivedLogger* b = new DerivedLogger("b", bDestructCount);
        {
            PoisonedUniquePtr<PoisonA, Logger> ptr(b);
            ASSERT_EQ(0, bDestructCount);
            ASSERT_EQ(b, ptr.unpoisoned());
            ASSERT_EQ(b, &*ptr);
            ASSERT_EQ(&b->name, &ptr->name);
        }
        ASSERT_EQ(1, bDestructCount);
    }

    {
        int aDestructCount = 0;
        Logger* a = new Logger("a", aDestructCount);
        {
            PoisonedUniquePtr<PoisonA, Logger> ptr = a;
            ASSERT_EQ(0, aDestructCount);
            ASSERT_EQ(a, ptr.unpoisoned());
        }
        ASSERT_EQ(1, aDestructCount);

        int bDestructCount = 0;
        DerivedLogger* b = new DerivedLogger("b", bDestructCount);
        {
            PoisonedUniquePtr<PoisonA, Logger> ptr = b;
            ASSERT_EQ(0, bDestructCount);
            ASSERT_EQ(b, ptr.unpoisoned());
        }
        ASSERT_EQ(1, bDestructCount);
    }

    {
        int aDestructCount = 0;
        const char* aName = "a";
        {
            PoisonedUniquePtr<PoisonA, Logger> ptr = PoisonedUniquePtr<PoisonA, Logger>::create(aName, aDestructCount);
            ASSERT_EQ(0, aDestructCount);
            ASSERT_TRUE(nullptr != ptr.unpoisoned());
            ASSERT_EQ(aName, &ptr->name);
        }
        ASSERT_EQ(1, aDestructCount);

        int bDestructCount = 0;
        const char* bName = "b";
        {
            PoisonedUniquePtr<PoisonA, Logger> ptr = PoisonedUniquePtr<PoisonA, DerivedLogger>::create(bName, bDestructCount);
            ASSERT_EQ(0, bDestructCount);
            ASSERT_TRUE(nullptr != ptr.unpoisoned());
            ASSERT_EQ(bName, &ptr->name);
        }
        ASSERT_EQ(1, bDestructCount);
    }

    {
        int aDestructCount = 0;
        const char* aName = "a";
        {
            PoisonedUniquePtr<PoisonA, Logger> ptr = std::make_unique<Logger>(aName, aDestructCount);
            ASSERT_EQ(0, aDestructCount);
            ASSERT_TRUE(nullptr != ptr.unpoisoned());
            ASSERT_EQ(aName, &ptr->name);
        }
        ASSERT_EQ(1, aDestructCount);

        int bDestructCount = 0;
        const char* bName = "b";
        {
            PoisonedUniquePtr<PoisonA, Logger> ptr = std::make_unique<DerivedLogger>(bName, bDestructCount);
            ASSERT_EQ(0, bDestructCount);
            ASSERT_TRUE(nullptr != ptr.unpoisoned());
            ASSERT_EQ(bName, &ptr->name);
        }
        ASSERT_EQ(1, bDestructCount);

        int uniqueDestructCount = 0;
        const char* uniqueName = "unique";
        {
            PoisonedUniquePtr<PoisonA, DerivedLogger> ptr = std::make_unique<DerivedLogger>(uniqueName, uniqueDestructCount);
            ASSERT_EQ(0, uniqueDestructCount);
            ASSERT_TRUE(nullptr != ptr.unpoisoned());
            ASSERT_EQ(uniqueName, &ptr->name);
        }
        ASSERT_EQ(1, uniqueDestructCount);

        int uniqueDerivedDestructCount = 0;
        const char* uniqueDerivedName = "unique derived";
        {
            PoisonedUniquePtr<PoisonA, Logger> ptr = std::make_unique<DerivedLogger>(uniqueDerivedName, uniqueDerivedDestructCount);
            ASSERT_EQ(0, uniqueDerivedDestructCount);
            ASSERT_TRUE(nullptr != ptr.unpoisoned());
            ASSERT_EQ(uniqueDerivedName, &ptr->name);
        }
        ASSERT_EQ(1, uniqueDerivedDestructCount);
    }

    {
        int aDestructCount = 0;
        int bDestructCount = 0;
        Logger* a = new Logger("a", aDestructCount);
        Logger* b = new Logger("b", bDestructCount);
        {
            PoisonedUniquePtr<PoisonA, Logger> p1 = a;
            PoisonedUniquePtr<PoisonA, Logger> p2 = WTFMove(p1);
            ASSERT_EQ(aDestructCount, 0);
            ASSERT_EQ(nullptr, p1.unpoisoned());
            ASSERT_EQ(0u, p1.bits());
            ASSERT_EQ(a, p2.unpoisoned());

            PoisonedUniquePtr<PoisonA, Logger> p3 = b;
            PoisonedUniquePtr<PoisonB, Logger> p4 = WTFMove(p3);
            ASSERT_EQ(0, bDestructCount);
            ASSERT_EQ(nullptr, p3.unpoisoned());
            ASSERT_EQ(0u, p3.bits());
            ASSERT_EQ(b, p4.unpoisoned());
        }
        ASSERT_EQ(1, aDestructCount);
        ASSERT_EQ(1, bDestructCount);
    }

    {
        int aDestructCount = 0;
        int bDestructCount = 0;
        Logger* a = new Logger("a", aDestructCount);
        Logger* b = new Logger("b", bDestructCount);
        {
            PoisonedUniquePtr<PoisonA, Logger> p1 = a;
            PoisonedUniquePtr<PoisonA, Logger> p2(WTFMove(p1));
            ASSERT_EQ(0, aDestructCount);
            ASSERT_EQ(nullptr, p1.unpoisoned());
            ASSERT_EQ(0u, p1.bits());
            ASSERT_EQ(a, p2.unpoisoned());

            PoisonedUniquePtr<PoisonA, Logger> p3 = b;
            PoisonedUniquePtr<PoisonB, Logger> p4(WTFMove(p3));
            ASSERT_EQ(0, bDestructCount);
            ASSERT_EQ(nullptr, p3.unpoisoned());
            ASSERT_EQ(0u, p3.bits());
            ASSERT_EQ(b, p4.unpoisoned());
        }
        ASSERT_EQ(1, aDestructCount);
        ASSERT_EQ(1, bDestructCount);
    }

    {
        int aDestructCount = 0;
        int bDestructCount = 0;
        DerivedLogger* a = new DerivedLogger("a", aDestructCount);
        DerivedLogger* b = new DerivedLogger("b", bDestructCount);
        {
            PoisonedUniquePtr<PoisonA, Logger> p1 = a;
            PoisonedUniquePtr<PoisonA, Logger> p2 = WTFMove(p1);
            ASSERT_EQ(aDestructCount, 0);
            ASSERT_TRUE(!p1.unpoisoned());
            ASSERT_TRUE(!p1.bits());
            ASSERT_EQ(a, p2.unpoisoned());

            PoisonedUniquePtr<PoisonA, Logger> p3 = b;
            PoisonedUniquePtr<PoisonB, Logger> p4 = WTFMove(p3);
            ASSERT_EQ(bDestructCount, 0);
            ASSERT_TRUE(!p3.unpoisoned());
            ASSERT_TRUE(!p3.bits());
            ASSERT_EQ(b, p4.unpoisoned());
        }
        ASSERT_EQ(1, aDestructCount);
        ASSERT_EQ(1, bDestructCount);
    }

    {
        int aDestructCount = 0;
        int bDestructCount = 0;
        DerivedLogger* a = new DerivedLogger("a", aDestructCount);
        DerivedLogger* b = new DerivedLogger("b", bDestructCount);
        {
            PoisonedUniquePtr<PoisonA, Logger> p1 = a;
            PoisonedUniquePtr<PoisonA, Logger> p2(WTFMove(p1));
            ASSERT_EQ(aDestructCount, 0);
            ASSERT_TRUE(!p1.unpoisoned());
            ASSERT_TRUE(!p1.bits());
            ASSERT_EQ(a, p2.unpoisoned());

            PoisonedUniquePtr<PoisonA, Logger> p3 = b;
            PoisonedUniquePtr<PoisonB, Logger> p4(WTFMove(p3));
            ASSERT_EQ(bDestructCount, 0);
            ASSERT_TRUE(!p3.unpoisoned());
            ASSERT_TRUE(!p3.bits());
            ASSERT_EQ(b, p4.unpoisoned());
        }
        ASSERT_EQ(1, aDestructCount);
        ASSERT_EQ(1, bDestructCount);
    }

    {
        int aDestructCount = 0;
        Logger* a = new Logger("a", aDestructCount);
        {
            PoisonedUniquePtr<PoisonA, Logger> ptr(a);
            ASSERT_EQ(a, ptr.unpoisoned());
            ptr.clear();
            ASSERT_TRUE(!ptr.unpoisoned());
            ASSERT_TRUE(!ptr.bits());
            ASSERT_EQ(1, aDestructCount);
        }
        ASSERT_EQ(1, aDestructCount);
    }
}

TEST(WTF_PoisonedUniquePtr, DISABLED_Assignment)
{
    initializePoisons();

    {
        int aDestructCount = 0;
        int bDestructCount = 0;
        Logger* a = new Logger("a", aDestructCount);
        Logger* b = new Logger("b", bDestructCount);
        {
            PoisonedUniquePtr<PoisonA, Logger> ptr(a);
            ASSERT_EQ(0, aDestructCount);
            ASSERT_EQ(0, bDestructCount);
            ASSERT_EQ(a, ptr.unpoisoned());
            ptr = b;
            ASSERT_EQ(1, aDestructCount);
            ASSERT_EQ(0, bDestructCount);
            ASSERT_EQ(b, ptr.unpoisoned());
        }
        ASSERT_EQ(1, aDestructCount);
        ASSERT_EQ(1, bDestructCount);
    }

    {
        int aDestructCount = 0;
        Logger* a = new Logger("a", aDestructCount);
        {
            PoisonedUniquePtr<PoisonA, Logger> ptr(a);
            ASSERT_EQ(0, aDestructCount);
            ASSERT_EQ(a, ptr.unpoisoned());
            ptr = nullptr;
            ASSERT_EQ(1, aDestructCount);
            ASSERT_EQ(nullptr, ptr.unpoisoned());
        }
        ASSERT_EQ(1, aDestructCount);
    }

    {
        int aDestructCount = 0;
        int bDestructCount = 0;
        int cDestructCount = 0;
        int dDestructCount = 0;
        Logger* a = new Logger("a", aDestructCount);
        Logger* b = new Logger("b", bDestructCount);
        Logger* c = new Logger("c", cDestructCount);
        Logger* d = new Logger("d", dDestructCount);
        {
            PoisonedUniquePtr<PoisonA, Logger> p1(a);
            PoisonedUniquePtr<PoisonA, Logger> p2(b);
            ASSERT_EQ(0, aDestructCount);
            ASSERT_EQ(0, bDestructCount);
            ASSERT_EQ(a, p1.unpoisoned());
            ASSERT_EQ(b, p2.unpoisoned());
            p1 = WTFMove(p2);
            ASSERT_EQ(1, aDestructCount);
            ASSERT_EQ(0, bDestructCount);
            ASSERT_EQ(b, p1.unpoisoned());
            ASSERT_EQ(nullptr, p2.unpoisoned());

            PoisonedUniquePtr<PoisonA, Logger> p3(c);
            PoisonedUniquePtr<PoisonB, Logger> p4(d);
            ASSERT_EQ(0, cDestructCount);
            ASSERT_EQ(0, dDestructCount);
            ASSERT_EQ(c, p3.unpoisoned());
            ASSERT_EQ(d, p4.unpoisoned());
            p3 = WTFMove(p4);
            ASSERT_EQ(1, cDestructCount);
            ASSERT_EQ(0, dDestructCount);
            ASSERT_EQ(d, p3.unpoisoned());
            ASSERT_EQ(nullptr, p4.unpoisoned());
        }
        ASSERT_EQ(1, aDestructCount);
        ASSERT_EQ(1, bDestructCount);
        ASSERT_EQ(1, cDestructCount);
        ASSERT_EQ(1, dDestructCount);
    }

    {
        int aDestructCount = 0;
        int bDestructCount = 0;
        DerivedLogger* a = new DerivedLogger("a", aDestructCount);
        DerivedLogger* b = new DerivedLogger("b", bDestructCount);
        {
            PoisonedUniquePtr<PoisonA, Logger> ptr(a);
            ASSERT_EQ(0, aDestructCount);
            ASSERT_EQ(0, bDestructCount);
            ASSERT_EQ(a, ptr.unpoisoned());
            ptr = b;
            ASSERT_EQ(1, aDestructCount);
            ASSERT_EQ(0, bDestructCount);
            ASSERT_EQ(b, ptr.unpoisoned());
        }
        ASSERT_EQ(1, aDestructCount);
        ASSERT_EQ(1, bDestructCount);
    }

    {
        int aDestructCount = 0;
        int bDestructCount = 0;
        int cDestructCount = 0;
        int dDestructCount = 0;
        DerivedLogger* a = new DerivedLogger("a", aDestructCount);
        DerivedLogger* b = new DerivedLogger("b", bDestructCount);
        DerivedLogger* c = new DerivedLogger("c", cDestructCount);
        DerivedLogger* d = new DerivedLogger("d", dDestructCount);
        {
            PoisonedUniquePtr<PoisonA, Logger> p1(a);
            PoisonedUniquePtr<PoisonA, DerivedLogger> p2(b);
            ASSERT_EQ(0, aDestructCount);
            ASSERT_EQ(0, bDestructCount);
            ASSERT_EQ(a, p1.unpoisoned());
            ASSERT_EQ(b, p2.unpoisoned());
            p1 = WTFMove(p2);
            ASSERT_EQ(1, aDestructCount);
            ASSERT_EQ(0, bDestructCount);
            ASSERT_EQ(b, p1.unpoisoned());
            ASSERT_EQ(nullptr, p2.unpoisoned());

            PoisonedUniquePtr<PoisonA, Logger> p3(c);
            PoisonedUniquePtr<PoisonB, DerivedLogger> p4(d);
            ASSERT_EQ(0, cDestructCount);
            ASSERT_EQ(0, dDestructCount);
            ASSERT_EQ(c, p3.unpoisoned());
            ASSERT_EQ(d, p4.unpoisoned());
            p3 = WTFMove(p4);
            ASSERT_EQ(1, cDestructCount);
            ASSERT_EQ(0, dDestructCount);
            ASSERT_EQ(d, p3.unpoisoned());
            ASSERT_EQ(nullptr, p4.unpoisoned());
        }
        ASSERT_EQ(1, aDestructCount);
        ASSERT_EQ(1, bDestructCount);
        ASSERT_EQ(1, cDestructCount);
        ASSERT_EQ(1, dDestructCount);
    }

    {
        int aDestructCount = 0;
        Logger* a = new Logger("a", aDestructCount);
        {
            PoisonedUniquePtr<PoisonA, Logger> ptr(a);
            ASSERT_EQ(0, aDestructCount);
            ASSERT_EQ(a, ptr.unpoisoned());
            ptr = a;
            ASSERT_EQ(0, aDestructCount);
            ASSERT_EQ(a, ptr.unpoisoned());
        }
        ASSERT_EQ(1, aDestructCount);
    }

    {
        int aDestructCount = 0;
        Logger* a = new Logger("a", aDestructCount);
        {
            PoisonedUniquePtr<PoisonA, Logger> ptr(a);
            ASSERT_EQ(0, aDestructCount);
            ASSERT_EQ(a, ptr.unpoisoned());
#if COMPILER(CLANG)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#pragma clang diagnostic ignored "-Wself-move"
#endif
            ptr = WTFMove(ptr);
#if COMPILER(CLANG)
#pragma clang diagnostic pop
#endif
            ASSERT_EQ(0, aDestructCount);
            ASSERT_EQ(a, ptr.unpoisoned());
        }
        ASSERT_EQ(1, aDestructCount);
    }
}

TEST(WTF_PoisonedUniquePtr, DISABLED_Swap)
{
    initializePoisons();

    {
        int aDestructCount = 0;
        int bDestructCount = 0;
        Logger* a = new Logger("a", aDestructCount);
        Logger* b = new Logger("b", bDestructCount);
        {
            PoisonedUniquePtr<PoisonA, Logger> p1 = a;
            PoisonedUniquePtr<PoisonA, Logger> p2;
            ASSERT_EQ(a, p1.unpoisoned());
            ASSERT_TRUE(!p2.bits());
            ASSERT_TRUE(!p2.unpoisoned());
            p2.swap(p1);
            ASSERT_EQ(aDestructCount, 0);
            ASSERT_TRUE(!p1.bits());
            ASSERT_TRUE(!p1.unpoisoned());
            ASSERT_EQ(a, p2.unpoisoned());

            PoisonedUniquePtr<PoisonA, Logger> p3 = b;
            PoisonedUniquePtr<PoisonB, Logger> p4;
            ASSERT_EQ(b, p3.unpoisoned());
            ASSERT_TRUE(!p4.bits());
            ASSERT_TRUE(!p4.unpoisoned());
            p4.swap(p3);
            ASSERT_EQ(0, bDestructCount);
            ASSERT_TRUE(!p3.bits());
            ASSERT_TRUE(!p3.unpoisoned());
            ASSERT_EQ(b, p4.unpoisoned());
        }
        ASSERT_EQ(1, aDestructCount);
        ASSERT_EQ(1, bDestructCount);
    }

    {
        int aDestructCount = 0;
        int bDestructCount = 0;
        Logger* a = new Logger("a", aDestructCount);
        Logger* b = new Logger("b", bDestructCount);
        {
            PoisonedUniquePtr<PoisonA, Logger> p1 = a;
            PoisonedUniquePtr<PoisonA, Logger> p2;
            ASSERT_EQ(a, p1.unpoisoned());
            ASSERT_TRUE(!p2.bits());
            ASSERT_TRUE(!p2.unpoisoned());
            swap(p1, p2);
            ASSERT_EQ(0, aDestructCount);
            ASSERT_TRUE(!p1.bits());
            ASSERT_TRUE(!p1.unpoisoned());
            ASSERT_EQ(a, p2.unpoisoned());

            PoisonedUniquePtr<PoisonA, Logger> p3 = b;
            PoisonedUniquePtr<PoisonB, Logger> p4;
            ASSERT_EQ(b, p3.unpoisoned());
            ASSERT_TRUE(!p4.bits());
            ASSERT_TRUE(!p4.unpoisoned());
            swap(p3, p4);
            ASSERT_EQ(0, bDestructCount);
            ASSERT_TRUE(!p3.bits());
            ASSERT_TRUE(!p3.unpoisoned());
            ASSERT_EQ(b, p4.unpoisoned());
        }
        ASSERT_EQ(1, aDestructCount);
        ASSERT_EQ(1, bDestructCount);
    }
}

static PoisonedUniquePtr<PoisonA, Logger> poisonedPtrFoo(Logger* logger)
{
    return PoisonedUniquePtr<PoisonA, Logger>(logger);
}

TEST(WTF_PoisonedUniquePtr, DISABLED_ReturnValue)
{
    initializePoisons();

    {
        int aDestructCount = 0;
        Logger* a = new Logger("a", aDestructCount);
        {
            auto ptr = poisonedPtrFoo(a);
            ASSERT_EQ(0, aDestructCount);
            ASSERT_EQ(a, ptr.unpoisoned());
            ASSERT_EQ(a, &*ptr);
            ASSERT_EQ(&a->name, &ptr->name);
        }
        ASSERT_EQ(1, aDestructCount);
    }
}

} // namespace TestWebKitAPI

