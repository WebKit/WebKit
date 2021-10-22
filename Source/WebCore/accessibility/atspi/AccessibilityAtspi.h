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
#include <wtf/CompletionHandler.h>
#include <wtf/FastMalloc.h>
#include <wtf/HashMap.h>
#include <wtf/Vector.h>
#include <wtf/WorkQueue.h>
#include <wtf/glib/GRefPtr.h>

typedef struct _GDBusConnection GDBusConnection;
typedef struct _GDBusInterfaceInfo GDBusInterfaceInfo;
typedef struct _GDBusInterfaceVTable GDBusInterfaceVTable;
typedef struct _GVariant GVariant;

namespace WebCore {
class AccessibilityObjectAtspi;
class AccessibilityRootAtspi;
enum class AccessibilityRole;

class AccessibilityAtspi {
    WTF_MAKE_FAST_ALLOCATED;
public:
    AccessibilityAtspi(const String&);
    ~AccessibilityAtspi() = default;

    const char* uniqueName() const;
    GVariant* nullReference() const;

    void registerRoot(AccessibilityRootAtspi&, Vector<std::pair<GDBusInterfaceInfo*, GDBusInterfaceVTable*>>&&, CompletionHandler<void(const String&)>&&);
    void unregisterRoot(AccessibilityRootAtspi&);
    String registerObject(AccessibilityObjectAtspi&, Vector<std::pair<GDBusInterfaceInfo*, GDBusInterfaceVTable*>>&&);
    void unregisterObject(AccessibilityObjectAtspi&);

    enum class ChildrenChanged { Added, Removed };
    void childrenChanged(AccessibilityObjectAtspi&, AccessibilityObjectAtspi&, ChildrenChanged);

    void stateChanged(AccessibilityObjectAtspi&, const char*, bool);

    void textChanged(AccessibilityObjectAtspi&, const char*, CString&&, unsigned, unsigned);
    void textAttributesChanged(AccessibilityObjectAtspi&);
    void textCaretMoved(AccessibilityObjectAtspi&, unsigned);
    void textSelectionChanged(AccessibilityObjectAtspi&);

    static const char* localizedRoleName(AccessibilityRole);

private:
    void ensureCache();
    void addAccessible(AccessibilityObjectAtspi&, const String&);
    void removeAccessible(AccessibilityObjectAtspi&);

    static GDBusInterfaceVTable s_cacheFunctions;

    Ref<WorkQueue> m_queue;
    GRefPtr<GDBusConnection> m_connection;
    HashMap<AccessibilityRootAtspi*, Vector<unsigned, 2>> m_rootObjects;
    HashMap<AccessibilityObjectAtspi*, Vector<unsigned, 20>> m_atspiObjects;
    unsigned m_cacheID { 0 };
    HashMap<String, AccessibilityObjectAtspi*> m_cache;
};

} // namespace WebCore

#endif // ENABLE(ACCESSIBILITY) && USE(ATSPI)
