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
#include "AccessibilityAtspi.h"
#include "AccessibilityAtspiEnums.h"
#include "AccessibilityObjectInterface.h"
#include "IntRect.h"
#include <wtf/OptionSet.h>
#include <wtf/RefCounted.h>
#include <wtf/text/CString.h>

typedef struct _GDBusInterfaceVTable GDBusInterfaceVTable;
typedef struct _GVariant GVariant;
typedef struct _GVariantBuilder GVariantBuilder;

namespace WebCore {
class AXCoreObject;
class AccessibilityRootAtspi;

using RelationMap = HashMap<Atspi::Relation, Vector<RefPtr<AccessibilityObjectAtspi>>, IntHash<Atspi::Relation>, WTF::StrongEnumHashTraits<Atspi::Relation>>;

class AccessibilityObjectAtspi final : public RefCounted<AccessibilityObjectAtspi> {
public:
    static Ref<AccessibilityObjectAtspi> create(AXCoreObject*, AccessibilityRootAtspi*);
    ~AccessibilityObjectAtspi() = default;

    bool registerObject();
    void didUnregisterObject();

    enum class Interface : uint16_t {
        Accessible = 1 << 0,
        Component = 1 << 1,
        Text = 1 << 2,
        Value = 1 << 3,
        Hyperlink = 1 << 4,
        Hypertext = 1 << 5,
        Action = 1 << 6,
        Document = 1 << 7,
        Image = 1 << 8,
        Selection = 1 << 9,
        Table = 1 << 10,
        TableCell = 1 << 11,
        Collection =  1 << 12
    };
    const OptionSet<Interface>& interfaces() const { return m_interfaces; }

    void setParent(std::optional<AccessibilityObjectAtspi*>);
    WEBCORE_EXPORT std::optional<AccessibilityObjectAtspi*> parent() const;
    GVariant* parentReference() const;
    WEBCORE_EXPORT void updateBackingStore();
    WEBCORE_EXPORT bool isIgnored() const;

    void attach(AXCoreObject*);
    void detach();
    void elementDestroyed();
    void cacheDestroyed();

    int indexInParentForChildrenChanged(AccessibilityAtspi::ChildrenChanged);

    const String& path();
    GVariant* reference();
    GVariant* hyperlinkReference();
    void serialize(GVariantBuilder*) const;

    WEBCORE_EXPORT String id() const;
    WEBCORE_EXPORT CString name() const;
    WEBCORE_EXPORT CString description() const;
    WEBCORE_EXPORT String locale() const;
    WEBCORE_EXPORT Atspi::Role role() const;
    WEBCORE_EXPORT unsigned childCount() const;
    WEBCORE_EXPORT Vector<RefPtr<AccessibilityObjectAtspi>> children() const;
    WEBCORE_EXPORT AccessibilityObjectAtspi* childAt(unsigned) const;
    WEBCORE_EXPORT OptionSet<Atspi::State> states() const;
    bool isDefunct() const;
    void stateChanged(const char*, bool);
    WEBCORE_EXPORT HashMap<String, String> attributes() const;
    WEBCORE_EXPORT RelationMap relationMap() const;

    WEBCORE_EXPORT AccessibilityObjectAtspi* hitTest(const IntPoint&, Atspi::CoordinateType) const;
    WEBCORE_EXPORT IntRect elementRect(Atspi::CoordinateType) const;
    WEBCORE_EXPORT void scrollToMakeVisible(Atspi::ScrollType) const;
    WEBCORE_EXPORT void scrollToPoint(const IntPoint&, Atspi::CoordinateType) const;

    WEBCORE_EXPORT String text() const;
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
    WEBCORE_EXPORT IntPoint boundaryOffset(unsigned, TextGranularity) const;
    WEBCORE_EXPORT IntRect boundsForRange(unsigned, unsigned, Atspi::CoordinateType) const;
    struct TextAttributes {
        HashMap<String, String> attributes;
        int startOffset;
        int endOffset;
    };
    WEBCORE_EXPORT TextAttributes textAttributes(std::optional<unsigned> = std::nullopt, bool = false) const;
    WEBCORE_EXPORT IntPoint selectedRange() const;
    WEBCORE_EXPORT void setSelectedRange(unsigned, unsigned);
    void textInserted(const String&, const VisiblePosition&);
    void textDeleted(const String&, const VisiblePosition&);
    void textAttributesChanged();
    void selectionChanged(const VisibleSelection&);

    WEBCORE_EXPORT double currentValue() const;
    WEBCORE_EXPORT bool setCurrentValue(double);
    WEBCORE_EXPORT double minimumValue() const;
    WEBCORE_EXPORT double maximumValue() const;
    WEBCORE_EXPORT double minimumIncrement() const;
    void valueChanged(double);

    WEBCORE_EXPORT URL url() const;

    WEBCORE_EXPORT String actionName() const;
    WEBCORE_EXPORT bool doAction() const;

    WEBCORE_EXPORT String documentAttribute(const String&) const;
    void loadEvent(const char*);

    WEBCORE_EXPORT unsigned selectionCount() const;
    WEBCORE_EXPORT AccessibilityObjectAtspi* selectedChild(unsigned) const;
    WEBCORE_EXPORT bool setChildSelected(unsigned, bool) const;
    WEBCORE_EXPORT bool clearSelection() const;
    void selectionChanged();

    WEBCORE_EXPORT AccessibilityObjectAtspi* cell(unsigned row, unsigned column) const;
    WEBCORE_EXPORT unsigned rowCount() const;
    WEBCORE_EXPORT unsigned columnCount() const;
    WEBCORE_EXPORT Vector<RefPtr<AccessibilityObjectAtspi>> cells() const;
    WEBCORE_EXPORT Vector<RefPtr<AccessibilityObjectAtspi>> rows() const;
    WEBCORE_EXPORT Vector<RefPtr<AccessibilityObjectAtspi>> rowHeaders() const;
    WEBCORE_EXPORT Vector<RefPtr<AccessibilityObjectAtspi>> columnHeaders() const;

    WEBCORE_EXPORT Vector<RefPtr<AccessibilityObjectAtspi>> cellRowHeaders() const;
    WEBCORE_EXPORT Vector<RefPtr<AccessibilityObjectAtspi>> cellColumnHeaders() const;
    WEBCORE_EXPORT unsigned rowSpan() const;
    WEBCORE_EXPORT unsigned columnSpan() const;
    WEBCORE_EXPORT std::pair<std::optional<unsigned>, std::optional<unsigned>> cellPosition() const;

private:
    AccessibilityObjectAtspi(AXCoreObject*, AccessibilityRootAtspi*);

    Vector<RefPtr<AccessibilityObjectAtspi>> wrapperVector(const Vector<RefPtr<AXCoreObject>>&) const;
    int indexInParent() const;
    void childAdded(AccessibilityObjectAtspi&);
    void childRemoved(AccessibilityObjectAtspi&);

    std::optional<Atspi::Role> effectiveRole() const;
    String effectiveRoleName() const;
    String roleName() const;
    const char* effectiveLocalizedRoleName() const;
    const char* localizedRoleName() const;
    void buildAttributes(GVariantBuilder*) const;
    void buildRelationSet(GVariantBuilder*) const;
    void buildInterfaces(GVariantBuilder*) const;
    void buildStates(GVariantBuilder*) const;

    bool focus() const;
    float opacity() const;

    static TextGranularity atspiBoundaryToTextGranularity(uint32_t);
    static TextGranularity atspiGranularityToTextGranularity(uint32_t);
    CString text(int, int) const;
    CString textAtOffset(int, TextGranularity, int&, int&) const;
    int characterAtOffset(int) const;
    std::optional<unsigned> characterOffset(UChar, int) const;
    std::optional<unsigned> characterIndex(UChar, unsigned) const;
    IntRect textExtents(int, int, Atspi::CoordinateType) const; 
    int offsetAtPoint(const IntPoint&, Atspi::CoordinateType) const;
    IntPoint boundsForSelection(const VisibleSelection&) const;
    bool selectionBounds(int&, int&) const;
    bool selectRange(int, int);
    TextAttributes textAttributesWithUTF8Offset(std::optional<int> = std::nullopt, bool = false) const;
    bool scrollToMakeVisible(int, int, Atspi::ScrollType) const;
    bool scrollToPoint(int, int, Atspi::CoordinateType, int, int) const;

    unsigned offsetInParent() const;

    unsigned hyperlinkCount() const;
    AccessibilityObjectAtspi* hyperlink(unsigned) const;
    std::optional<unsigned> hyperlinkIndex(unsigned) const;

    String localizedActionName() const;
    String actionKeyBinding() const;

    HashMap<String, String> documentAttributes() const;
    String documentLocale() const;

    String imageDescription() const;

    bool deselectSelectedChild(unsigned) const;
    bool isChildSelected(unsigned) const;
    bool selectAll() const;

    AccessibilityObjectAtspi* rowHeader(unsigned) const;
    AccessibilityObjectAtspi* columnHeader(unsigned) const;
    unsigned rowExtent(unsigned row, unsigned column) const;
    unsigned columnExtent(unsigned row, unsigned column) const;

    AccessibilityObjectAtspi* tableCaption() const;
    std::optional<unsigned> cellIndex(unsigned row, unsigned column) const;
    std::optional<unsigned> rowAtIndex(unsigned) const;
    std::optional<unsigned> columnAtIndex(unsigned) const;
    String rowDescription(unsigned) const;
    String columnDescription(unsigned) const;

    struct CollectionMatchRule {
        CollectionMatchRule(GVariant*);

        bool match(AccessibilityObjectAtspi&);
        bool matchInterfaces(AccessibilityObjectAtspi&);
        bool matchStates(AccessibilityObjectAtspi&);
        bool matchRoles(AccessibilityObjectAtspi&);
        bool matchAttributes(AccessibilityObjectAtspi&);

        struct {
            OptionSet<Atspi::State> value;
            uint16_t type { 0 };
        } states;

        struct {
            HashMap<String, Vector<String>> value;
            uint16_t type { 0 };
        } attributes;

        struct {
            Vector<Atspi::Role> value;
            uint16_t type { 0 };
        } roles;

        struct {
            Vector<String> value;
            uint16_t type { 0 };
        } interfaces;
    };
    Vector<RefPtr<AccessibilityObjectAtspi>> matches(CollectionMatchRule&, uint32_t sortOrder, uint32_t maxResultCount, bool traverse);
    void addMatchesInCanonicalOrder(Vector<RefPtr<AccessibilityObjectAtspi>>&, CollectionMatchRule&, uint32_t maxResultCount, bool traverse);

    static OptionSet<Interface> interfacesForObject(AXCoreObject&);

    static GDBusInterfaceVTable s_accessibleFunctions;
    static GDBusInterfaceVTable s_componentFunctions;
    static GDBusInterfaceVTable s_textFunctions;
    static GDBusInterfaceVTable s_valueFunctions;
    static GDBusInterfaceVTable s_hyperlinkFunctions;
    static GDBusInterfaceVTable s_hypertextFunctions;
    static GDBusInterfaceVTable s_actionFunctions;
    static GDBusInterfaceVTable s_documentFunctions;
    static GDBusInterfaceVTable s_imageFunctions;
    static GDBusInterfaceVTable s_selectionFunctions;
    static GDBusInterfaceVTable s_tableFunctions;
    static GDBusInterfaceVTable s_tableCellFunctions;
    static GDBusInterfaceVTable s_collectionFunctions;

    AXCoreObject* m_coreObject { nullptr };
    OptionSet<Interface> m_interfaces;
    AccessibilityRootAtspi* m_root { nullptr };
    std::optional<RefPtr<AccessibilityObjectAtspi>> m_parent;
    bool m_isRegistered { false };
    String m_path;
    String m_hyperlinkPath;
    int64_t m_lastSelectionChangedTime { -1 };
    mutable bool m_hasListMarkerAtStart;
    mutable int m_indexInParent { -1 };
};

} // namespace WebCore

#endif // USE(ATSPI)
