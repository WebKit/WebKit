/*
 * Copyright (C) 2023 Igalia S.L.
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

#include "Test.h"
#include <wtf/glib/GThreadSafeWeakPtr.h>
#include <wtf/glib/GWeakPtr.h>

namespace TestWebKitAPI {

TEST(WTF_GWeakPtr, Basic)
{
    GWeakPtr<GObject> empty;
    EXPECT_NULL(empty.get());
    EXPECT_FALSE(empty);

    GObject* obj = nullptr;
    {
        obj = G_OBJECT(g_object_new(G_TYPE_OBJECT, nullptr));
        GWeakPtr<GObject> weak(obj);
        EXPECT_EQ(obj, weak.get());
        EXPECT_TRUE(weak);
        EXPECT_EQ(weak->ref_count, 1);
        g_clear_object(&obj);
        EXPECT_NULL(weak.get());
        EXPECT_FALSE(weak);
    }
    EXPECT_NULL(obj);

    {
        obj = G_OBJECT(g_object_new(G_TYPE_OBJECT, nullptr));
        GWeakPtr<GObject> weak(obj);
        EXPECT_EQ(obj, weak.get());
        EXPECT_TRUE(weak);
        g_object_ref(obj);
        EXPECT_EQ(weak->ref_count, 2);
        g_object_unref(obj);
        EXPECT_EQ(obj, weak.get());
        EXPECT_TRUE(weak);
        EXPECT_EQ(weak->ref_count, 1);
        g_clear_object(&obj);
        EXPECT_NULL(weak.get());
        EXPECT_FALSE(weak);
    }
    EXPECT_NULL(obj);
}

TEST(WTF_GWeakPtr, Reset)
{
    GObject* obj = nullptr;
    {
        obj = G_OBJECT(g_object_new(G_TYPE_OBJECT, nullptr));
        GWeakPtr<GObject> weak(obj);
        EXPECT_EQ(obj, weak.get());
        weak.reset();
        EXPECT_NULL(weak.get());
        EXPECT_FALSE(weak);
        EXPECT_EQ(obj->ref_count, 1);
        g_clear_object(&obj);
    }
    EXPECT_NULL(obj);

    {
        obj = G_OBJECT(g_object_new(G_TYPE_OBJECT, nullptr));
        GWeakPtr<GObject> weak(obj);
        EXPECT_EQ(obj, weak.get());
        weak = nullptr;
        EXPECT_NULL(weak.get());
        EXPECT_FALSE(weak);
        EXPECT_EQ(obj->ref_count, 1);
        g_clear_object(&obj);
    }
    EXPECT_NULL(obj);

    {
        obj = G_OBJECT(g_object_new(G_TYPE_OBJECT, nullptr));
        GWeakPtr<GObject> weak(obj);
        EXPECT_EQ(obj, weak.get());
        GObject* obj2 = G_OBJECT(g_object_new(G_TYPE_OBJECT, nullptr));
        weak.reset(obj2);
        EXPECT_EQ(obj2, weak.get());
        EXPECT_TRUE(weak);
        g_clear_object(&obj);
        EXPECT_EQ(obj2, weak.get());
        g_object_unref(obj2);
        EXPECT_NULL(weak.get());
        EXPECT_FALSE(weak);
    }
    EXPECT_NULL(obj);
}

TEST(WTF_GWeakPtr, Move)
{
    GObject* obj = nullptr;
    {
        obj = G_OBJECT(g_object_new(G_TYPE_OBJECT, nullptr));
        GWeakPtr<GObject> weak(obj);
        EXPECT_EQ(obj, weak.get());
        EXPECT_TRUE(weak);
        GWeakPtr<GObject> weak2(WTFMove(weak));
        EXPECT_NULL(weak.get());
        EXPECT_FALSE(weak);
        EXPECT_EQ(obj, weak2.get());
        EXPECT_TRUE(weak2);
        EXPECT_EQ(obj->ref_count, 1);
        g_clear_object(&obj);
        EXPECT_NULL(weak2.get());
        EXPECT_FALSE(weak2);
    }
    EXPECT_NULL(obj);

    {
        obj = G_OBJECT(g_object_new(G_TYPE_OBJECT, nullptr));
        GWeakPtr<GObject> weak(obj);
        EXPECT_EQ(obj, weak.get());
        EXPECT_TRUE(weak);
        GWeakPtr<GObject> weak2 = WTFMove(weak);
        EXPECT_NULL(weak.get());
        EXPECT_FALSE(weak);
        EXPECT_EQ(obj, weak2.get());
        EXPECT_TRUE(weak2);
        EXPECT_EQ(obj->ref_count, 1);
        g_clear_object(&obj);
        EXPECT_NULL(weak2.get());
        EXPECT_FALSE(weak2);
    }
    EXPECT_NULL(obj);
}

TEST(WTF_GThreadSafeWeakPtr, Basic)
{
    GThreadSafeWeakPtr<GObject> empty;
    EXPECT_NULL(empty.get());

    GObject* obj = G_OBJECT(g_object_new(G_TYPE_OBJECT, nullptr));
    GThreadSafeWeakPtr<GObject> weak(obj);
    EXPECT_EQ(obj, weak.get());
    g_clear_object(&obj);
    EXPECT_NULL(weak.get());
    EXPECT_NULL(obj);
}

TEST(WTF_GThreadSafeWeakPtr, Reset)
{
    GObject* obj = nullptr;
    {
        obj = G_OBJECT(g_object_new(G_TYPE_OBJECT, nullptr));
        GThreadSafeWeakPtr<GObject> weak(obj);
        EXPECT_EQ(obj, weak.get());
        weak.reset();
        EXPECT_NULL(weak.get());
        g_clear_object(&obj);
    }
    EXPECT_NULL(obj);

    {
        obj = G_OBJECT(g_object_new(G_TYPE_OBJECT, nullptr));
        GThreadSafeWeakPtr<GObject> weak(obj);
        EXPECT_EQ(obj, weak.get());
        weak = nullptr;
        EXPECT_NULL(weak.get());
        g_clear_object(&obj);
    }
    EXPECT_NULL(obj);

    {
        obj = G_OBJECT(g_object_new(G_TYPE_OBJECT, nullptr));
        GThreadSafeWeakPtr<GObject> weak(obj);
        EXPECT_EQ(obj, weak.get());
        GObject* obj2 = G_OBJECT(g_object_new(G_TYPE_OBJECT, nullptr));
        weak.reset(obj2);
        EXPECT_EQ(obj2, weak.get());
        g_clear_object(&obj);
        EXPECT_EQ(obj2, weak.get());
        g_object_unref(obj2);
        EXPECT_NULL(weak.get());
    }
    EXPECT_NULL(obj);
}

TEST(WTF_GThreadSafeWeakPtr, Move)
{
    GObject* obj = nullptr;
    {
        obj = G_OBJECT(g_object_new(G_TYPE_OBJECT, nullptr));
        GThreadSafeWeakPtr<GObject> weak(obj);
        EXPECT_EQ(obj, weak.get());
        GThreadSafeWeakPtr<GObject> weak2 = WTFMove(weak);
        EXPECT_NULL(weak.get());
        EXPECT_EQ(obj, weak2.get());
        g_clear_object(&obj);
        EXPECT_NULL(weak2.get());
    }
    EXPECT_NULL(obj);
}

} // namespace TestWebKitAPI
