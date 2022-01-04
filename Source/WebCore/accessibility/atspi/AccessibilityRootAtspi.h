/*
 * Copyright (C) 2021 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#if ENABLE(ACCESSIBILITY) && USE(ATSPI)
#include "AccessibilityAtspi.h"
#include "IntRect.h"
#include <wtf/Atomics.h>
#include <wtf/FastMalloc.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/WeakPtr.h>

typedef struct _GVariant GVariant;

namespace WebCore {
class AccessibilityObjectAtspi;
class Page;

class AccessibilityRootAtspi final : public ThreadSafeRefCounted<AccessibilityRootAtspi, WTF::DestructionThread::Main> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<AccessibilityRootAtspi> create(Page&, AccessibilityAtspi&);
    ~AccessibilityRootAtspi() = default;

    void registerObject(CompletionHandler<void(const String&)>&&);
    void unregisterObject();
    void registerTree();
    bool isTreeRegistered() const { return m_isTreeRegistered.load(); }
    void setPath(String&&);

    const String& path() const { return m_path; }
    GVariant* reference() const;
    GVariant* parentReference() const;
    GVariant* applicationReference() const;
    AccessibilityAtspi& atspi() const { return m_atspi; }
    AccessibilityObjectAtspi* child() const;
    void childAdded(AccessibilityObjectAtspi&);
    void childRemoved(AccessibilityObjectAtspi&);

    void serialize(GVariantBuilder*) const;

private:
    AccessibilityRootAtspi(Page&, AccessibilityAtspi&);

    void embedded(const char* parentUniqueName, const char* parentPath);
    IntRect frameRect(uint32_t) const;

    static GDBusInterfaceVTable s_accessibleFunctions;
    static GDBusInterfaceVTable s_socketFunctions;
    static GDBusInterfaceVTable s_componentFunctions;

    AccessibilityAtspi& m_atspi;
    WeakPtr<Page> m_page;
    String m_path;
    String m_parentUniqueName;
    String m_parentPath;
    Atomic<bool> m_isTreeRegistered { false };
};

} // namespace WebCore

#endif // ENABLE(ACCESSIBILITY) && USE(ATSPI)
