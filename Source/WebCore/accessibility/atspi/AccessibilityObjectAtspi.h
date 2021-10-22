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
#include "IntRect.h"
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
        Accessible = 1 << 0,
        Component = 1 << 1,
        Text = 1 << 2
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

    AccessibilityObjectAtspi* hitTest(const IntPoint&, uint32_t) const;
    IntRect elementRect(uint32_t) const;
    void scrollToMakeVisible(uint32_t) const;
    void scrollToPoint(const IntPoint&, uint32_t) const;

    String text() const;
    enum class TextGranularity {
        Character,
        WordStart,
        WordEnd,
        SentenceStart,
        SentenceEnd,
        LineStart,
        LineEnd,
        Paragraph
    };
    IntPoint boundaryOffset(unsigned, TextGranularity) const;
    IntRect boundsForRange(unsigned, unsigned, uint32_t) const;
    struct TextAttributes {
        HashMap<String, String> attributes;
        int startOffset;
        int endOffset;
    };
    TextAttributes textAttributes(std::optional<unsigned> = std::nullopt, bool = false) const;
    IntPoint selectedRange() const;
    void setSelectedRange(unsigned, unsigned);
    void textInserted(const String&, const VisiblePosition&);
    void textDeleted(const String&, const VisiblePosition&);
    void textAttributesChanged();
    void selectionChanged(const VisibleSelection&);

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

    bool focus() const;
    float opacity() const;

    static TextGranularity atspiBoundaryToTextGranularity(uint32_t);
    static TextGranularity atspiGranularityToTextGranularity(uint32_t);
    CString text(int, int) const;
    CString textAtOffset(int, TextGranularity, int&, int&) const;
    int characterAtOffset(int) const;
    IntRect textExtents(int, int, uint32_t) const;
    int offsetAtPoint(const IntPoint&, uint32_t) const;
    IntPoint boundsForSelection(const VisibleSelection&) const;
    bool selectionBounds(int&, int&) const;
    bool selectRange(int, int);
    TextAttributes textAttributesWithUTF8Offset(std::optional<int> = std::nullopt, bool = false) const;
    bool scrollToMakeVisible(int, int, uint32_t) const;
    bool scrollToPoint(int, int, uint32_t, int, int) const;

    static OptionSet<Interface> interfacesForObject(AXCoreObject&);

    static GDBusInterfaceVTable s_accessibleFunctions;
    static GDBusInterfaceVTable s_componentFunctions;
    static GDBusInterfaceVTable s_textFunctions;

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
