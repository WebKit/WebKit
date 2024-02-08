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

#if USE(ATSPI)
#include "AccessibilityAtspiEnums.h"
#include "IntRect.h"
#include <wtf/FastMalloc.h>
#include <wtf/RefCounted.h>
#include <wtf/WeakPtr.h>

typedef struct _GDBusInterfaceVTable GDBusInterfaceVTable;
typedef struct _GVariant GVariant;

namespace WebCore {
class AccessibilityObjectAtspi;
class Page;

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(AccessibilityRootAtspi);
class AccessibilityRootAtspi final : public RefCounted<AccessibilityRootAtspi>, public CanMakeWeakPtr<AccessibilityRootAtspi> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(AccessibilityRootAtspi);
public:
    static Ref<AccessibilityRootAtspi> create(Page&);
    ~AccessibilityRootAtspi() = default;

    void registerObject(CompletionHandler<void(const String&)>&&);
    void unregisterObject();
    void setPath(String&&);

    const String& path() const { return m_path; }
    GVariant* reference() const;
    GVariant* parentReference() const;
    GVariant* applicationReference() const;
    AccessibilityObjectAtspi* child() const;
    void childAdded(AccessibilityObjectAtspi&);
    void childRemoved(AccessibilityObjectAtspi&);

    void serialize(GVariantBuilder*) const;

private:
    explicit AccessibilityRootAtspi(Page&);

    void embedded(const char* parentUniqueName, const char* parentPath);
    IntRect frameRect(Atspi::CoordinateType) const;

    static GDBusInterfaceVTable s_accessibleFunctions;
    static GDBusInterfaceVTable s_socketFunctions;
    static GDBusInterfaceVTable s_componentFunctions;

    SingleThreadWeakPtr<Page> m_page;
    String m_path;
    String m_parentUniqueName;
    String m_parentPath;
};

} // namespace WebCore

#endif // USE(ATSPI)
