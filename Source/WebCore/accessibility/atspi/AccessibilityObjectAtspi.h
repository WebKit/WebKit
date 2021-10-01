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
#include "AccessibilityObjectInterface.h"
#include <wtf/Atomics.h>
#include <wtf/Lock.h>
#include <wtf/OptionSet.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/text/CString.h>

typedef struct _GDBusInterfaceVTable GDBusInterfaceVTable;
typedef struct _GVariant GVariant;
typedef struct _GVariantBuilder GVariantBuilder;

namespace WebCore {
class AXCoreObject;
class AccessibilityRootAtspi;

class AccessibilityObjectAtspi final : public ThreadSafeRefCounted<AccessibilityObjectAtspi> {
public:
    static Ref<AccessibilityObjectAtspi> create(AXCoreObject*);
    ~AccessibilityObjectAtspi() = default;

    enum class Interface : uint8_t {
        Accessible = 1 << 0
    };
    const OptionSet<Interface>& interfaces() const { return m_interfaces; }

    void setRoot(AccessibilityRootAtspi*);
    AccessibilityRootAtspi* root() const;
    void setParent(std::optional<AccessibilityObjectAtspi*>);
    std::optional<AccessibilityObjectAtspi*> parent() const;
    void updateBackingStore();

    void attach(AXCoreObject*);
    void detach();
    void elementDestroyed();
    void cacheDestroyed();

    int indexInParentForChildrenChanged(AccessibilityAtspi::ChildrenChanged);

    const String& path();
    GVariant* reference();
    void serialize(GVariantBuilder*) const;

    String id() const;
    CString name() const;
    CString description() const;
    unsigned role() const;
    unsigned childCount() const;
    Vector<RefPtr<AccessibilityObjectAtspi>> children() const;
    AccessibilityObjectAtspi* childAt(unsigned) const;
    uint64_t state() const;
    void stateChanged(const char*, bool);
    HashMap<String, String> attributes() const;
    HashMap<uint32_t, Vector<RefPtr<AccessibilityObjectAtspi>>> relationMap() const;

private:
    explicit AccessibilityObjectAtspi(AXCoreObject*);

    int indexInParent() const;
    GVariant* parentReference() const;
    void childAdded(AccessibilityObjectAtspi&);
    void childRemoved(AccessibilityObjectAtspi&);

    String roleName() const;
    const char* localizedRoleName() const;
    void buildAttributes(GVariantBuilder*) const;
    void buildRelationSet(GVariantBuilder*) const;
    void buildInterfaces(GVariantBuilder*) const;

    static OptionSet<Interface> interfacesForObject(AXCoreObject&);

    static GDBusInterfaceVTable s_accessibleFunctions;

    AXCoreObject* m_axObject { nullptr };
    AXCoreObject* m_coreObject { nullptr };
    OptionSet<Interface> m_interfaces;
    AccessibilityRootAtspi* m_root WTF_GUARDED_BY_LOCK(m_rootLock) { nullptr };
    std::optional<AccessibilityObjectAtspi*> m_parent;
    Atomic<bool> m_isRegistered { false };
    String m_path;
    mutable int m_indexInParent { -1 };
    mutable Lock m_rootLock;
};

} // namespace WebCore

#endif // ENABLE(ACCESSIBILITY) && USE(ATSPI)
