/*
 * Copyright (C) 2022 Igalia S.L.
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
#include <glib-object.h>
#include <wtf/glib/GRefPtr.h>

namespace TestWebKitAPI {

class GWeakPtr {
public:
    GWeakPtr() = default;

    explicit GWeakPtr(gpointer* obj)
        : m_ptr(obj)
    {
        g_object_add_weak_pointer(G_OBJECT(*m_ptr), m_ptr);
    }

    ~GWeakPtr()
    {
        if (*m_ptr)
            g_object_remove_weak_pointer(G_OBJECT(*m_ptr), m_ptr);
    }

    void set(gpointer* obj)
    {
        m_ptr = obj;
        g_object_add_weak_pointer(G_OBJECT(*m_ptr), m_ptr);
    }

private:
    gpointer* m_ptr { nullptr };
};

TEST(WTF_GRefPtr, Basic)
{
    GRefPtr<GObject> empty;
    EXPECT_NULL(empty.get());

    GObject* obj = nullptr;
    {
        obj = G_OBJECT(g_object_new(G_TYPE_OBJECT, nullptr));
        GWeakPtr weak(reinterpret_cast<gpointer*>(&obj));
        GRefPtr<GObject> o(obj);
        EXPECT_EQ(obj, o.get());
        EXPECT_EQ(obj->ref_count, 2);
        g_object_unref(obj);
        EXPECT_EQ(obj, o.get());
    }
    EXPECT_NULL(obj);

    {
        obj = G_OBJECT(g_object_new(G_TYPE_OBJECT, nullptr));
        GWeakPtr weak(reinterpret_cast<gpointer*>(&obj));
        GRefPtr<GObject> o = obj;
        EXPECT_EQ(obj, o.get());
        EXPECT_EQ(obj->ref_count, 2);
        g_object_unref(obj);
        EXPECT_EQ(obj, o.get());
    }
    EXPECT_NULL(obj);

    {
        obj = G_OBJECT(g_object_new(G_TYPE_OBJECT, nullptr));
        GWeakPtr weak(reinterpret_cast<gpointer*>(&obj));
        GRefPtr<GObject> o1 = obj;
        GRefPtr<GObject> o2(o1);
        EXPECT_EQ(obj, o1.get());
        EXPECT_EQ(obj, o2.get());
        EXPECT_EQ(obj->ref_count, 3);
        g_object_unref(obj);
        EXPECT_EQ(obj, o1.get());
        EXPECT_EQ(obj, o2.get());
    }
    EXPECT_NULL(obj);

    {
        obj = G_OBJECT(g_object_new(G_TYPE_OBJECT, nullptr));
        GWeakPtr weak(reinterpret_cast<gpointer*>(&obj));
        GRefPtr<GObject> o1 = obj;
        GRefPtr<GObject> o2 = o1;
        EXPECT_EQ(obj, o1.get());
        EXPECT_EQ(obj, o2.get());
        EXPECT_EQ(obj->ref_count, 3);
        g_object_unref(obj);
        EXPECT_EQ(obj, o1.get());
        EXPECT_EQ(obj, o2.get());
    }
    EXPECT_NULL(obj);

    {
        obj = G_OBJECT(g_object_new(G_TYPE_OBJECT, nullptr));
        GWeakPtr weak(reinterpret_cast<gpointer*>(&obj));
        GRefPtr<GObject> o1 = obj;
        GRefPtr<GObject> o2(WTFMove(o1));
        EXPECT_NULL(o1.get());
        EXPECT_EQ(obj, o2.get());
        EXPECT_EQ(obj->ref_count, 2);
        g_object_unref(obj);
        EXPECT_EQ(obj, o2.get());
    }
    EXPECT_NULL(obj);

    {
        obj = G_OBJECT(g_object_new(G_TYPE_OBJECT, nullptr));
        GWeakPtr weak(reinterpret_cast<gpointer*>(&obj));
        GRefPtr<GObject> o1 = obj;
        GRefPtr<GObject> o2 = WTFMove(o1);
        EXPECT_NULL(o1.get());
        EXPECT_EQ(obj, o2.get());
        EXPECT_EQ(obj->ref_count, 2);
        g_object_unref(obj);
        EXPECT_EQ(obj, o2.get());
    }
    EXPECT_NULL(obj);
}

TEST(WTF_GRefPtr, Adopt)
{
    GObject* obj = nullptr;
    {
        obj = G_OBJECT(g_object_new(G_TYPE_OBJECT, nullptr));
        GWeakPtr weak(reinterpret_cast<gpointer*>(&obj));
        GRefPtr<GObject> o(adoptGRef(obj));
        EXPECT_EQ(obj, o.get());
        EXPECT_EQ(obj->ref_count, 1);
    }
    EXPECT_NULL(obj);

    {
        obj = G_OBJECT(g_object_new(G_TYPE_OBJECT, nullptr));
        GWeakPtr weak(reinterpret_cast<gpointer*>(&obj));
        GRefPtr<GObject> o = adoptGRef(obj);
        EXPECT_EQ(obj, o.get());
        EXPECT_EQ(obj->ref_count, 1);
    }
    EXPECT_NULL(obj);
}

TEST(WTF_GRefPtr, Floating)
{
    GObject* obj = nullptr;
    {
        obj = G_OBJECT(g_object_new(G_TYPE_INITIALLY_UNOWNED, nullptr));
        EXPECT_TRUE(g_object_is_floating(obj));
        GWeakPtr weak(reinterpret_cast<gpointer*>(&obj));
        GRefPtr<GObject> o(obj);
        EXPECT_FALSE(g_object_is_floating(obj));
        EXPECT_EQ(obj, o.get());
        EXPECT_EQ(obj->ref_count, 1);
    }
    EXPECT_NULL(obj);

    {
        obj = G_OBJECT(g_object_new(G_TYPE_INITIALLY_UNOWNED, nullptr));
        EXPECT_TRUE(g_object_is_floating(obj));
        GWeakPtr weak(reinterpret_cast<gpointer*>(&obj));
        GRefPtr<GObject> o = obj;
        EXPECT_FALSE(g_object_is_floating(obj));
        EXPECT_EQ(obj, o.get());
        EXPECT_EQ(obj->ref_count, 1);
    }
    EXPECT_NULL(obj);

    {
        obj = G_OBJECT(g_object_new(G_TYPE_INITIALLY_UNOWNED, nullptr));
        EXPECT_TRUE(g_object_is_floating(obj));
        GWeakPtr weak(reinterpret_cast<gpointer*>(&obj));
        GRefPtr<GObject> o = adoptGRef(obj);
        EXPECT_TRUE(g_object_is_floating(obj));
        EXPECT_EQ(obj, o.get());
        EXPECT_EQ(obj->ref_count, 1);
        g_object_ref_sink(obj);
        EXPECT_EQ(obj->ref_count, 1);
    }
    EXPECT_NULL(obj);
}

TEST(WTF_GRefPtr, OutPtr)
{
    auto returnObject = [](GObject** obj) {
        *obj = G_OBJECT(g_object_new(G_TYPE_OBJECT, nullptr));
    };

    GObject* obj = nullptr;
    {
        GWeakPtr weak;
        GRefPtr<GObject> o;
        EXPECT_NULL(o.get());
        returnObject(&o.outPtr());
        EXPECT_NOT_NULL(o.get());
        obj = o.get();
        weak.set(reinterpret_cast<gpointer*>(&obj));
        EXPECT_EQ(obj->ref_count, 1);
    }
    EXPECT_NULL(obj);

    {
        GWeakPtr weak;
        GRefPtr<GObject> o;
        EXPECT_NULL(o.get());
        returnObject(&o.outPtr());
        EXPECT_NOT_NULL(o.get());
        GObject* obj2 = o.get();
        GWeakPtr weak2(reinterpret_cast<gpointer*>(&obj2));
        returnObject(&o.outPtr());
        EXPECT_NULL(obj2);
        EXPECT_NOT_NULL(o.get());
        obj = o.get();
        weak.set(reinterpret_cast<gpointer*>(&obj));
        EXPECT_EQ(obj->ref_count, 1);
    }
    EXPECT_NULL(obj);
}

TEST(WTF_GRefPtr, Clear)
{
    GObject* obj = G_OBJECT(g_object_new(G_TYPE_OBJECT, nullptr));
    GWeakPtr weak(reinterpret_cast<gpointer*>(&obj));
    GRefPtr<GObject> o = adoptGRef(obj);
    EXPECT_EQ(obj, o.get());
    EXPECT_EQ(obj->ref_count, 1);
    o.clear();
    EXPECT_NULL(o.get());
    EXPECT_NULL(obj);
}

TEST(WTF_GRefPtr, LeakRef)
{
    GObject* obj = nullptr;
    {
        obj = G_OBJECT(g_object_new(G_TYPE_OBJECT, nullptr));
        GWeakPtr weak(reinterpret_cast<gpointer*>(&obj));
        GRefPtr<GObject> o = adoptGRef(obj);
        EXPECT_EQ(obj, o.get());
        EXPECT_EQ(obj->ref_count, 1);
        auto* obj2 = o.leakRef();
        EXPECT_NULL(o.get());
        EXPECT_EQ(obj, obj2);
    }
    EXPECT_NOT_NULL(obj);
    EXPECT_EQ(obj->ref_count, 1);
    g_object_unref(obj);
}

} // namespace TestWebKitAPI
