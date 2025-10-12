/*
 * Copyright (C) 2025 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WPEPlatformTest.h"

namespace TestWebKitAPI {

WPEPlatformTest::~WPEPlatformTest()
{
    checkWatchedObjects();
}

void WPEPlatformTest::objectFinalized(WPEPlatformTest* test, GObject* finalizedObject)
{
    test->m_watchedObjects.remove(finalizedObject);
}

void WPEPlatformTest::assertObjectIsDeletedWhenTestFinishes(gpointer object)
{
    g_assert_true(G_IS_OBJECT(object));
    auto addResult = m_watchedObjects.add(G_OBJECT(object));
    if (addResult.isNewEntry)
        g_object_weak_ref(G_OBJECT(object), reinterpret_cast<GWeakNotify>(objectFinalized), this);
}

void WPEPlatformTest::checkWatchedObjects()
{
    bool leakedObjects = !m_watchedObjects.isEmpty();
    if (!leakedObjects)
        return;

    g_print("Leaked objects:");
    while (auto* object = m_watchedObjects.takeAny()) {
        g_print(" %s(%p - %u left)", g_type_name_from_instance(reinterpret_cast<GTypeInstance*>(object)), object, object->ref_count);
        g_object_weak_unref(object, reinterpret_cast<GWeakNotify>(objectFinalized), this);
    }
    g_print("\n");
    g_assert_false(leakedObjects);
}

} // namespace TestWebKitAPI
