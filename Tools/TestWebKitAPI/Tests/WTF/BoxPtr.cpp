/*
 * Copyright (C) 2013-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2020 Metrological Group B.V.
 * Copyright (C) 2020 Igalia S.L.
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

#include "Logger.h"
#include <wtf/BoxPtr.h>

namespace TestWebKitAPI {

struct BoxPtrLogger {
    BoxPtrLogger(const char* name);
    static BoxPtrLogger* create(const char* name) { return new BoxPtrLogger(name); }
    const char& name;
};

BoxPtrLogger::BoxPtrLogger(const char* name)
    : name { *name }
{
    log() << "create(" << name << ") ";
}

void boxPtrLoggerDeleter(BoxPtrLogger* logger)
{
    log() << "delete(" << &logger->name << ") ";
    delete logger;
}

};

namespace WTF {

WTF_DEFINE_BOXPTR_DELETER(TestWebKitAPI::BoxPtrLogger, TestWebKitAPI::boxPtrLoggerDeleter);

}

namespace TestWebKitAPI {

TEST(WTF_BoxPtr, Basic)
{
    {
        BoxPtr<BoxPtrLogger> empty;
        EXPECT_EQ(false, static_cast<bool>(empty));
    }
    EXPECT_STREQ("", takeLogStr().c_str());

    {
        BoxPtrLogger* a = BoxPtrLogger::create("a");
        BoxPtr<BoxPtrLogger> ptr = adoptInBoxPtr(a);
        EXPECT_EQ(true, static_cast<bool>(ptr));
        EXPECT_EQ(a, ptr->get());
        EXPECT_EQ(a->name, (*ptr)->name);
    }
    EXPECT_STREQ("create(a) delete(a) ", takeLogStr().c_str());

    {
        BoxPtrLogger* a = BoxPtrLogger::create("a");
        BoxPtr<BoxPtrLogger> p1 = adoptInBoxPtr(a);
        BoxPtr<BoxPtrLogger> p2(p1);
        EXPECT_EQ(a, p1->get());
        EXPECT_EQ(a, p2->get());
    }
    EXPECT_STREQ("create(a) delete(a) ", takeLogStr().c_str());

    {
        BoxPtrLogger* a = BoxPtrLogger::create("a");
        BoxPtr<BoxPtrLogger> p1 = adoptInBoxPtr(a);
        BoxPtr<BoxPtrLogger> p2 = p1;
        EXPECT_EQ(a, p1->get());
        EXPECT_EQ(a, p2->get());
    }
    EXPECT_STREQ("create(a) delete(a) ", takeLogStr().c_str());

    {
        BoxPtrLogger* a = BoxPtrLogger::create("a");
        BoxPtr<BoxPtrLogger> p1 = adoptInBoxPtr(a);
        BoxPtr<BoxPtrLogger> p2 = WTFMove(p1);
        EXPECT_EQ(false, static_cast<bool>(p1));
        EXPECT_EQ(a, p2->get());
    }
    EXPECT_STREQ("create(a) delete(a) ", takeLogStr().c_str());

    {
        BoxPtrLogger* a = BoxPtrLogger::create("a");
        BoxPtr<BoxPtrLogger> p1 = adoptInBoxPtr(a);
        BoxPtr<BoxPtrLogger> p2(WTFMove(p1));
        EXPECT_EQ(false, static_cast<bool>(p1));
        EXPECT_EQ(a, p2->get());
    }
    EXPECT_STREQ("create(a) delete(a) ", takeLogStr().c_str());

    {
        BoxPtrLogger* a = BoxPtrLogger::create("a");
        BoxPtr<BoxPtrLogger> ptr = adoptInBoxPtr(a);
        EXPECT_EQ(a, ptr->get());
        ptr = nullptr;
        EXPECT_EQ(false, static_cast<bool>(ptr));
    }
    EXPECT_STREQ("create(a) delete(a) ", takeLogStr().c_str());
}

TEST(WTF_BoxPtr, Assignment)
{
    {
        BoxPtrLogger* a = BoxPtrLogger::create("a");
        BoxPtrLogger* b = BoxPtrLogger::create("b");
        BoxPtr<BoxPtrLogger> p1 = adoptInBoxPtr(a);
        BoxPtr<BoxPtrLogger> p2 = adoptInBoxPtr(b);
        EXPECT_EQ(a, p1->get());
        EXPECT_EQ(b, p2->get());
        log() << "| ";
        p1 = p2;
        EXPECT_EQ(b, p1->get());
        EXPECT_EQ(b, p2->get());
        log() << "| ";
    }
    EXPECT_STREQ("create(a) create(b) | delete(a) | delete(b) ", takeLogStr().c_str());

    {
        BoxPtrLogger* a = BoxPtrLogger::create("a");
        BoxPtrLogger* b = BoxPtrLogger::create("b");
        BoxPtr<BoxPtrLogger> p1 = adoptInBoxPtr(a);
        BoxPtr<BoxPtrLogger> p2 = adoptInBoxPtr(b);
        EXPECT_EQ(a, p1->get());
        EXPECT_EQ(b, p2->get());
        log() << "| ";
        p1 = WTFMove(p2);
        EXPECT_EQ(b, p1->get());
        EXPECT_EQ(false, static_cast<bool>(p2));
        log() << "| ";
    }
    EXPECT_STREQ("create(a) create(b) | delete(a) | delete(b) ", takeLogStr().c_str());

    {
        BoxPtrLogger* a = BoxPtrLogger::create("a");
        BoxPtr<BoxPtrLogger> ptr = adoptInBoxPtr(a);
        EXPECT_EQ(a, ptr->get());
        log() << "| ";
        IGNORE_CLANG_WARNINGS_BEGIN("self-assign-overloaded")
        ptr = ptr;
        IGNORE_CLANG_WARNINGS_END
        EXPECT_EQ(a, ptr->get());
        log() << "| ";
    }
    EXPECT_STREQ("create(a) | | delete(a) ", takeLogStr().c_str());

    {
        BoxPtrLogger* a = BoxPtrLogger::create("a");
        BoxPtr<BoxPtrLogger> ptr = adoptInBoxPtr(a);
        EXPECT_EQ(a, ptr->get());
        IGNORE_CLANG_WARNINGS_BEGIN("self-move")
        ptr = WTFMove(ptr);
        IGNORE_CLANG_WARNINGS_END
        EXPECT_EQ(a, ptr->get());
    }
    EXPECT_STREQ("create(a) delete(a) ", takeLogStr().c_str());
}

TEST(WTF_BoxPtr, Operators)
{
    {
        BoxPtrLogger* a = BoxPtrLogger::create("a");
        BoxPtrLogger* b = BoxPtrLogger::create("b");
        BoxPtr<BoxPtrLogger> p1 = adoptInBoxPtr(a);
        BoxPtr<BoxPtrLogger> p2 = adoptInBoxPtr(b);
        EXPECT_EQ(p1, p1);
        EXPECT_NE(p1, p2);
    }
    EXPECT_STREQ("create(a) create(b) delete(b) delete(a) ", takeLogStr().c_str());

    {
        BoxPtrLogger* a = BoxPtrLogger::create("a");
        BoxPtr<BoxPtrLogger> p1 = adoptInBoxPtr(a);
        EXPECT_EQ(static_cast<bool>(p1), true);
        EXPECT_EQ(!p1, false);
    }
    EXPECT_STREQ("create(a) delete(a) ", takeLogStr().c_str());

    {
        BoxPtr<BoxPtrLogger> empty;
        BoxPtrLogger* a = BoxPtrLogger::create("a");
        BoxPtr<BoxPtrLogger> p1 = adoptInBoxPtr(a);
        EXPECT_NE(empty, p1);
        EXPECT_EQ(empty, empty);
    }
    EXPECT_STREQ("create(a) delete(a) ", takeLogStr().c_str());
}

};
