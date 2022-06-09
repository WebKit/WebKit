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
#include <wtf/glib/GUniquePtr.h>

typedef struct _GDBusConnection GDBusConnection;
typedef struct _GDBusInterfaceInfo GDBusInterfaceInfo;
typedef struct _GDBusInterfaceVTable GDBusInterfaceVTable;
typedef struct _GDBusProxy GDBusProxy;
typedef struct _GVariant GVariant;

namespace WebCore {
class AccessibilityObjectAtspi;
class AccessibilityRootAtspi;
enum class AccessibilityRole;

class AccessibilityAtspi {
    WTF_MAKE_FAST_ALLOCATED;
public:
    AccessibilityAtspi(const String&);
    ~AccessibilityAtspi();

    WEBCORE_EXPORT RunLoop& runLoop() const;

    const char* uniqueName() const;
    GVariant* nullReference() const;
    bool hasEventListeners() const { return !m_eventListeners.isEmpty(); }

    void registerRoot(AccessibilityRootAtspi&, Vector<std::pair<GDBusInterfaceInfo*, GDBusInterfaceVTable*>>&&, CompletionHandler<void(const String&)>&&);
    void unregisterRoot(AccessibilityRootAtspi&);
    String registerObject(AccessibilityObjectAtspi&, Vector<std::pair<GDBusInterfaceInfo*, GDBusInterfaceVTable*>>&&);
    void unregisterObject(AccessibilityObjectAtspi&);
    String registerHyperlink(AccessibilityObjectAtspi&, Vector<std::pair<GDBusInterfaceInfo*, GDBusInterfaceVTable*>>&&);

    void parentChanged(AccessibilityObjectAtspi&);
    void parentChanged(AccessibilityRootAtspi&);
    enum class ChildrenChanged { Added, Removed };
    void childrenChanged(AccessibilityObjectAtspi&, AccessibilityObjectAtspi&, ChildrenChanged);
    void childrenChanged(AccessibilityRootAtspi&, AccessibilityObjectAtspi&, ChildrenChanged);

    void stateChanged(AccessibilityObjectAtspi&, const char*, bool);

    void textChanged(AccessibilityObjectAtspi&, const char*, CString&&, unsigned, unsigned);
    void textAttributesChanged(AccessibilityObjectAtspi&);
    void textCaretMoved(AccessibilityObjectAtspi&, unsigned);
    void textSelectionChanged(AccessibilityObjectAtspi&);

    void valueChanged(AccessibilityObjectAtspi&, double);

    void selectionChanged(AccessibilityObjectAtspi&);

    void loadEvent(AccessibilityObjectAtspi&, CString&&);

    static const char* localizedRoleName(AccessibilityRole);

    void addAccessible(AccessibilityObjectAtspi&);

#if ENABLE(DEVELOPER_MODE)
    using NotificationObserverParameter = std::variant<std::nullptr_t, String, bool, unsigned, Ref<AccessibilityObjectAtspi>>;
    using NotificationObserver = Function<void(AccessibilityObjectAtspi&, const char*, NotificationObserverParameter)>;
    WEBCORE_EXPORT void addNotificationObserver(void*, NotificationObserver&&);
    WEBCORE_EXPORT void removeNotificationObserver(void*);
#endif

private:
    void registerTrees() const;
    void initializeRegistry();
    void addEventListener(const char* dbusName, const char* eventName);
    void removeEventListener(const char* dbusName, const char* eventName);

    void ensureCache();
    void removeAccessible(AccessibilityObjectAtspi&);

    bool shouldEmitSignal(const char* interface, const char* name, const char* detail = "");

#if ENABLE(DEVELOPER_MODE)
    void notifyStateChanged(AccessibilityObjectAtspi&, const char*, bool) const;
    void notifySelectionChanged(AccessibilityObjectAtspi&) const;
    void notifyTextChanged(AccessibilityObjectAtspi&) const;
    void notifyTextCaretMoved(AccessibilityObjectAtspi&, unsigned) const;
    void notifyChildrenChanged(AccessibilityObjectAtspi&, AccessibilityObjectAtspi&, ChildrenChanged) const;
    void notifyValueChanged(AccessibilityObjectAtspi&) const;
    void notifyLoadEvent(AccessibilityObjectAtspi&, const CString&) const;
#endif

    static GDBusInterfaceVTable s_cacheFunctions;

    Ref<WorkQueue> m_queue;
    GRefPtr<GDBusConnection> m_connection;
    GRefPtr<GDBusProxy> m_registry;
    HashMap<CString, Vector<GUniquePtr<char*>>> m_eventListeners;
    HashMap<AccessibilityRootAtspi*, Vector<unsigned, 2>> m_rootObjects;
    HashMap<AccessibilityObjectAtspi*, Vector<unsigned, 20>> m_atspiObjects;
    HashMap<AccessibilityObjectAtspi*, Vector<unsigned, 20>> m_atspiHyperlinks;
    unsigned m_cacheID { 0 };
    HashMap<String, AccessibilityObjectAtspi*> m_cache;
    bool m_inGetItems { false };
#if ENABLE(DEVELOPER_MODE)
    HashMap<void*, NotificationObserver> m_notificationObservers;
#endif
};

} // namespace WebCore

#endif // ENABLE(ACCESSIBILITY) && USE(ATSPI)
