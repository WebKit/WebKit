/*
 * Copyright (C) 2008 Nuanti Ltd.
 * Copyright (C) 2009 Igalia S.L.
 * Copyright (C) 2009 Jan Alonzo
 *
 * Portions from Mozilla a11y, copyright as follows:
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
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

#include "config.h"
#include "AccessibilityObjectWrapperAtk.h"

#if HAVE(ACCESSIBILITY)

#include "AXObjectCache.h"
#include "AccessibilityListBox.h"
#include "AccessibilityRenderObject.h"
#include "AtomicString.h"
#include "CString.h"
#include "Document.h"
#include "Editor.h"
#include "Frame.h"
#include "FrameView.h"
#include "HostWindow.h"
#include "HTMLNames.h"
#include "InlineTextBox.h"
#include "IntRect.h"
#include "NotImplemented.h"
#include "RenderText.h"
#include "TextEncoding.h"

#include <atk/atk.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <libgail-util/gail-util.h>
#include <pango/pango.h>

using namespace WebCore;

static AccessibilityObject* fallbackObject()
{
    static AXObjectCache* fallbackCache = new AXObjectCache;
    static AccessibilityObject* object = 0;
    if (!object) {
        // FIXME: using fallbackCache->getOrCreate(ListBoxOptionRole) is a hack
        object = fallbackCache->getOrCreate(ListBoxOptionRole);
        object->ref();
    }

    return object;
}

// Used to provide const char* returns.
static const char* returnString(const String& str)
{
    static CString returnedString;
    returnedString = str.utf8();
    return returnedString.data();
}

static AccessibilityObject* core(WebKitAccessible* accessible)
{
    if (!accessible)
        return 0;

    return accessible->m_object;
}

static AccessibilityObject* core(AtkObject* object)
{
    if (!WEBKIT_IS_ACCESSIBLE(object))
        return 0;

    return core(WEBKIT_ACCESSIBLE(object));
}

static AccessibilityObject* core(AtkAction* action)
{
    return core(ATK_OBJECT(action));
}

static AccessibilityObject* core(AtkText* text)
{
    return core(ATK_OBJECT(text));
}

static AccessibilityObject* core(AtkEditableText* text)
{
    return core(ATK_OBJECT(text));
}

static AccessibilityObject* core(AtkComponent* component)
{
    return core(ATK_OBJECT(component));
}

static AccessibilityObject* core(AtkImage* image)
{
    return core(ATK_OBJECT(image));
}

static const gchar* webkit_accessible_get_name(AtkObject* object)
{
    AccessibilityObject* coreObject = core(object);
    if (coreObject->isControl()) {
        AccessibilityRenderObject* renderObject = static_cast<AccessibilityRenderObject*>(coreObject);
        AccessibilityObject* label = renderObject->correspondingLabelForControlElement();
        if (label) {
            AccessibilityRenderObject::AccessibilityChildrenVector children = label->children();
            // Currently, label->stringValue() should be an empty String. This
            // might not be the case down the road.
            String name = label->stringValue();
            for (unsigned i = 0; i < children.size(); ++i)
                name += children.at(i).get()->stringValue();
            return returnString(name);
        }
    }
    return returnString(coreObject->stringValue());
}

static const gchar* webkit_accessible_get_description(AtkObject* object)
{
    // TODO: the Mozilla MSAA implementation prepends "Description: "
    // Should we do this too?
    return returnString(core(object)->accessibilityDescription());
}

static void setAtkRelationSetFromCoreObject(AccessibilityObject* coreObject, AtkRelationSet* relationSet)
{
    AccessibilityRenderObject* accObject = static_cast<AccessibilityRenderObject*>(coreObject);
    if (accObject->isControl()) {
        AccessibilityObject* label = accObject->correspondingLabelForControlElement();
        if (label)
            atk_relation_set_add_relation_by_type(relationSet, ATK_RELATION_LABELLED_BY, label->wrapper());
    } else {
        AccessibilityObject* control = accObject->correspondingControlForLabelElement();
        if (control)
            atk_relation_set_add_relation_by_type(relationSet, ATK_RELATION_LABEL_FOR, control->wrapper());
    }
}

static gpointer webkit_accessible_parent_class = NULL;

static AtkObject* webkit_accessible_get_parent(AtkObject* object)
{
    // To work around bogus additional objects in the ancestry, we set the
    // parent when we ref the child. If the child knows its parent, use that.
    AtkObject* parent = ATK_OBJECT_CLASS(webkit_accessible_parent_class)->get_parent(object);
    if (parent)
        return parent;

    AccessibilityObject* coreParent = core(object)->parentObject();

    // The top level web view claims to not have a parent. This makes it
    // impossible for assistive technologies to ascend the accessible
    // hierarchy all the way to the application. (Bug 30489)
    if (!coreParent && core(object)->isWebArea()) {
        HostWindow* hostWindow = core(object)->document()->view()->hostWindow();
        if (hostWindow) {
            PlatformPageClient webView = hostWindow->platformPageClient();
            if (webView) {
                GtkWidget* webViewParent = gtk_widget_get_parent(webView);
                if (webViewParent)
                    return gtk_widget_get_accessible(webViewParent);
            }
        }
    }

    if (!coreParent)
        return NULL;

    return coreParent->wrapper();
}

static gint webkit_accessible_get_n_children(AtkObject* object)
{
    return core(object)->children().size();
}

static AtkObject* webkit_accessible_ref_child(AtkObject* object, gint index)
{
    AccessibilityObject* coreObject = core(object);

    g_return_val_if_fail(index >= 0, NULL);
    g_return_val_if_fail(static_cast<size_t>(index) < coreObject->children().size(), NULL);

    AccessibilityObject* coreChild = coreObject->children().at(index).get();

    if (!coreChild)
        return NULL;

    AtkObject* child = coreChild->wrapper();
    atk_object_set_parent(child, object);
    g_object_ref(child);

    return child;
}

static gint webkit_accessible_get_index_in_parent(AtkObject* object)
{
    // FIXME: This needs to be implemented.
    notImplemented();
    return 0;
}

static AtkAttributeSet* addAttributeToSet(AtkAttributeSet* attributeSet, const char* name, const char* value)
{
    AtkAttribute* attribute = static_cast<AtkAttribute*>(g_malloc(sizeof(AtkAttribute)));
    attribute->name = g_strdup(name);
    attribute->value = g_strdup(value);
    attributeSet = g_slist_prepend(attributeSet, attribute);

    return attributeSet;
}

static AtkAttributeSet* webkit_accessible_get_attributes(AtkObject* object)
{
    AtkAttributeSet* attributeSet = NULL;

    int headingLevel = core(object)->headingLevel();
    if (headingLevel) {
        String value = String::number(headingLevel);
        attributeSet = addAttributeToSet(attributeSet, "level", value.utf8().data());
    }
    return attributeSet;
}

static AtkRole atkRole(AccessibilityRole role)
{
    switch (role) {
    case UnknownRole:
        return ATK_ROLE_UNKNOWN;
    case ButtonRole:
        return ATK_ROLE_PUSH_BUTTON;
    case RadioButtonRole:
        return ATK_ROLE_RADIO_BUTTON;
    case CheckBoxRole:
        return ATK_ROLE_CHECK_BOX;
    case SliderRole:
        return ATK_ROLE_SLIDER;
    case TabGroupRole:
        return ATK_ROLE_PAGE_TAB_LIST;
    case TextFieldRole:
    case TextAreaRole:
        return ATK_ROLE_ENTRY;
    case StaticTextRole:
        return ATK_ROLE_TEXT;
    case OutlineRole:
        return ATK_ROLE_TREE;
    case MenuBarRole:
        return ATK_ROLE_MENU_BAR;
    case MenuRole:
        return ATK_ROLE_MENU;
    case MenuItemRole:
        return ATK_ROLE_MENU_ITEM;
    case ColumnRole:
        //return ATK_ROLE_TABLE_COLUMN_HEADER; // Is this right?
        return ATK_ROLE_UNKNOWN; // Matches Mozilla
    case RowRole:
        //return ATK_ROLE_TABLE_ROW_HEADER; // Is this right?
        return ATK_ROLE_LIST_ITEM; // Matches Mozilla
    case ToolbarRole:
        return ATK_ROLE_TOOL_BAR;
    case BusyIndicatorRole:
        return ATK_ROLE_PROGRESS_BAR; // Is this right?
    case ProgressIndicatorRole:
        //return ATK_ROLE_SPIN_BUTTON; // Some confusion about this role in AccessibilityRenderObject.cpp
        return ATK_ROLE_PROGRESS_BAR;
    case WindowRole:
        return ATK_ROLE_WINDOW;
    case ComboBoxRole:
        return ATK_ROLE_COMBO_BOX;
    case SplitGroupRole:
        return ATK_ROLE_SPLIT_PANE;
    case SplitterRole:
        return ATK_ROLE_SEPARATOR;
    case ColorWellRole:
        return ATK_ROLE_COLOR_CHOOSER;
    case ListRole:
        return ATK_ROLE_LIST;
    case ScrollBarRole:
        return ATK_ROLE_SCROLL_BAR;
    case GridRole: // Is this right?
    case TableRole:
        return ATK_ROLE_TABLE;
    case ApplicationRole:
        return ATK_ROLE_APPLICATION;
    case GroupRole:
    case RadioGroupRole:
        return ATK_ROLE_PANEL;
    case CellRole:
        return ATK_ROLE_TABLE_CELL;
    case LinkRole:
    case WebCoreLinkRole:
    case ImageMapLinkRole:
        return ATK_ROLE_LINK;
    case ImageMapRole:
    case ImageRole:
        return ATK_ROLE_IMAGE;
    case ListMarkerRole:
        return ATK_ROLE_TEXT;
    case WebAreaRole:
        //return ATK_ROLE_HTML_CONTAINER; // Is this right?
        return ATK_ROLE_DOCUMENT_FRAME;
    case HeadingRole:
        return ATK_ROLE_HEADING;
    case ListBoxRole:
        return ATK_ROLE_LIST;
    case ListBoxOptionRole:
        return ATK_ROLE_LIST_ITEM;
    default:
        return ATK_ROLE_UNKNOWN;
    }
}

static AtkRole webkit_accessible_get_role(AtkObject* object)
{
    AccessibilityObject* AXObject = core(object);

    if (!AXObject)
        return ATK_ROLE_UNKNOWN;

    // WebCore does not seem to have a role for list items
    if (AXObject->isGroup()) {
        AccessibilityObject* parent = AXObject->parentObjectUnignored();
        if (parent && parent->isList())
            return ATK_ROLE_LIST_ITEM;
    }

    // WebCore does not know about paragraph role, label role, or section role
    if (AXObject->isAccessibilityRenderObject()) {
        Node* node = static_cast<AccessibilityRenderObject*>(AXObject)->renderer()->node();
        if (node) {
            if (node->hasTagName(HTMLNames::pTag))
                return ATK_ROLE_PARAGRAPH;
            if (node->hasTagName(HTMLNames::labelTag))
                return ATK_ROLE_LABEL;
            if (node->hasTagName(HTMLNames::divTag))
                return ATK_ROLE_SECTION;
        }
    }

    // Note: Why doesn't WebCore have a password field for this
    if (AXObject->isPasswordField())
        return ATK_ROLE_PASSWORD_TEXT;

    return atkRole(AXObject->roleValue());
}

static void setAtkStateSetFromCoreObject(AccessibilityObject* coreObject, AtkStateSet* stateSet)
{
    // Please keep the state list in alphabetical order

    if (coreObject->isChecked())
        atk_state_set_add_state(stateSet, ATK_STATE_CHECKED);

    // FIXME: isReadOnly does not seem to do the right thing for
    // controls, so check explicitly for them
    if (!coreObject->isReadOnly() ||
        (coreObject->isControl() && coreObject->canSetValueAttribute()))
        atk_state_set_add_state(stateSet, ATK_STATE_EDITABLE);

    // FIXME: Put both ENABLED and SENSITIVE together here for now
    if (coreObject->isEnabled()) {
        atk_state_set_add_state(stateSet, ATK_STATE_ENABLED);
        atk_state_set_add_state(stateSet, ATK_STATE_SENSITIVE);
    }

    if (coreObject->canSetFocusAttribute())
        atk_state_set_add_state(stateSet, ATK_STATE_FOCUSABLE);

    if (coreObject->isFocused())
        atk_state_set_add_state(stateSet, ATK_STATE_FOCUSED);

    // TODO: ATK_STATE_HORIZONTAL

    if (coreObject->isIndeterminate())
        atk_state_set_add_state(stateSet, ATK_STATE_INDETERMINATE);

    if (coreObject->isMultiSelect())
        atk_state_set_add_state(stateSet, ATK_STATE_MULTISELECTABLE);

    // TODO: ATK_STATE_OPAQUE

    if (coreObject->isPressed())
        atk_state_set_add_state(stateSet, ATK_STATE_PRESSED);

    // TODO: ATK_STATE_SELECTABLE_TEXT

    if (coreObject->isSelected())
        atk_state_set_add_state(stateSet, ATK_STATE_SELECTED);

    // FIXME: Group both SHOWING and VISIBLE here for now
    // Not sure how to handle this in WebKit, see bug
    // http://bugzilla.gnome.org/show_bug.cgi?id=509650 for other
    // issues with SHOWING vs VISIBLE within GTK+
    if (!coreObject->isOffScreen()) {
        atk_state_set_add_state(stateSet, ATK_STATE_SHOWING);
        atk_state_set_add_state(stateSet, ATK_STATE_VISIBLE);
    }

    // Mutually exclusive, so we group these two
    if (coreObject->roleValue() == TextFieldRole)
        atk_state_set_add_state(stateSet, ATK_STATE_SINGLE_LINE);
    else if (coreObject->roleValue() == TextAreaRole)
        atk_state_set_add_state(stateSet, ATK_STATE_MULTI_LINE);

    // TODO: ATK_STATE_SENSITIVE

    // TODO: ATK_STATE_VERTICAL

    if (coreObject->isVisited())
        atk_state_set_add_state(stateSet, ATK_STATE_VISITED);
}

static AtkStateSet* webkit_accessible_ref_state_set(AtkObject* object)
{
    AtkStateSet* stateSet = ATK_OBJECT_CLASS(webkit_accessible_parent_class)->ref_state_set(object);
    AccessibilityObject* coreObject = core(object);

    if (coreObject == fallbackObject()) {
        atk_state_set_add_state(stateSet, ATK_STATE_DEFUNCT);
        return stateSet;
    }

    setAtkStateSetFromCoreObject(coreObject, stateSet);

    return stateSet;
}

static AtkRelationSet* webkit_accessible_ref_relation_set(AtkObject* object)
{
    AtkRelationSet* relationSet = ATK_OBJECT_CLASS(webkit_accessible_parent_class)->ref_relation_set(object);
    AccessibilityObject* coreObject = core(object);

    setAtkRelationSetFromCoreObject(coreObject, relationSet);

    return relationSet;
}

static void webkit_accessible_init(AtkObject* object, gpointer data)
{
    if (ATK_OBJECT_CLASS(webkit_accessible_parent_class)->initialize)
        ATK_OBJECT_CLASS(webkit_accessible_parent_class)->initialize(object, data);

    WEBKIT_ACCESSIBLE(object)->m_object = reinterpret_cast<AccessibilityObject*>(data);
}

static void webkit_accessible_finalize(GObject* object)
{
    // This is a good time to clear the return buffer.
    returnString(String());

    G_OBJECT_CLASS(webkit_accessible_parent_class)->finalize(object);
}

static void webkit_accessible_class_init(AtkObjectClass* klass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(klass);

    webkit_accessible_parent_class = g_type_class_peek_parent(klass);

    gobjectClass->finalize = webkit_accessible_finalize;

    klass->initialize = webkit_accessible_init;
    klass->get_name = webkit_accessible_get_name;
    klass->get_description = webkit_accessible_get_description;
    klass->get_parent = webkit_accessible_get_parent;
    klass->get_n_children = webkit_accessible_get_n_children;
    klass->ref_child = webkit_accessible_ref_child;
    klass->get_role = webkit_accessible_get_role;
    klass->ref_state_set = webkit_accessible_ref_state_set;
    klass->get_index_in_parent = webkit_accessible_get_index_in_parent;
    klass->get_attributes = webkit_accessible_get_attributes;
    klass->ref_relation_set = webkit_accessible_ref_relation_set;
}

GType
webkit_accessible_get_type(void)
{
    static volatile gsize type_volatile = 0;

    if (g_once_init_enter(&type_volatile)) {
        static const GTypeInfo tinfo = {
            sizeof(WebKitAccessibleClass),
            (GBaseInitFunc)NULL,
            (GBaseFinalizeFunc)NULL,
            (GClassInitFunc)webkit_accessible_class_init,
            (GClassFinalizeFunc)NULL,
            NULL, /* class data */
            sizeof(WebKitAccessible), /* instance size */
            0, /* nb preallocs */
            (GInstanceInitFunc)NULL,
            NULL /* value table */
        };

        GType type = g_type_register_static(ATK_TYPE_OBJECT,
                                            "WebKitAccessible", &tinfo, GTypeFlags(0));
        g_once_init_leave(&type_volatile, type);
    }

    return type_volatile;
}

static gboolean webkit_accessible_action_do_action(AtkAction* action, gint i)
{
    g_return_val_if_fail(i == 0, FALSE);
    return core(action)->performDefaultAction();
}

static gint webkit_accessible_action_get_n_actions(AtkAction* action)
{
    return 1;
}

static const gchar* webkit_accessible_action_get_description(AtkAction* action, gint i)
{
    g_return_val_if_fail(i == 0, NULL);
    // TODO: Need a way to provide/localize action descriptions.
    notImplemented();
    return "";
}

static const gchar* webkit_accessible_action_get_keybinding(AtkAction* action, gint i)
{
    g_return_val_if_fail(i == 0, NULL);
    // FIXME: Construct a proper keybinding string.
    return returnString(core(action)->accessKey().string());
}

static const gchar* webkit_accessible_action_get_name(AtkAction* action, gint i)
{
    g_return_val_if_fail(i == 0, NULL);
    return returnString(core(action)->actionVerb());
}

static void atk_action_interface_init(AtkActionIface* iface)
{
    iface->do_action = webkit_accessible_action_do_action;
    iface->get_n_actions = webkit_accessible_action_get_n_actions;
    iface->get_description = webkit_accessible_action_get_description;
    iface->get_keybinding = webkit_accessible_action_get_keybinding;
    iface->get_name = webkit_accessible_action_get_name;
}

// Text

static gchar* webkit_accessible_text_get_text(AtkText* text, gint startOffset, gint endOffset)
{
    AccessibilityObject* coreObject = core(text);
    String ret;
    unsigned start = startOffset;
    int length = endOffset - startOffset;

    if (coreObject->isTextControl())
        ret = coreObject->doAXStringForRange(PlainTextRange(start, length));
    else
        ret = coreObject->textUnderElement().substring(start, length);

    return g_strdup(ret.utf8().data());
}

static GailTextUtil* getGailTextUtilForAtk(AtkText* textObject)
{
    gpointer data = g_object_get_data(G_OBJECT(textObject), "webkit-accessible-gail-text-util");
    if (data)
        return static_cast<GailTextUtil*>(data);

    GailTextUtil* gailTextUtil = gail_text_util_new();
    gail_text_util_text_setup(gailTextUtil, webkit_accessible_text_get_text(textObject, 0, -1));
    g_object_set_data_full(G_OBJECT(textObject), "webkit-accessible-gail-text-util", gailTextUtil, g_object_unref);
    return gailTextUtil;
}

static gchar* utf8Substr(const gchar* string, gint start, gint end)
{
    ASSERT(string);
    glong strLen = g_utf8_strlen(string, -1);
    if (start > strLen || end > strLen)
        return 0;
    gchar* startPtr = g_utf8_offset_to_pointer(string, start);
    gsize lenInBytes = g_utf8_offset_to_pointer(string, end) -  startPtr + 1;
    gchar* output = static_cast<gchar*>(g_malloc0(lenInBytes + 1));
    return g_utf8_strncpy(output, startPtr, end - start + 1);
}

// This function is not completely general, is it's tied to the
// internals of WebCore's text presentation.
static gchar* convertUniCharToUTF8(const UChar* characters, gint length, int from, int to)
{
    CString stringUTF8 = UTF8Encoding().encode(characters, length, QuestionMarksForUnencodables);
    gchar* utf8String = utf8Substr(stringUTF8.data(), from, to);
    if (!g_utf8_validate(utf8String, -1, NULL)) {
        g_free(utf8String);
        return 0;
    }
    gsize len = strlen(utf8String);
    GString* ret = g_string_new_len(NULL, len);

    // WebCore introduces line breaks in the text that do not reflect
    // the layout you see on the screen, replace them with spaces
    while (len > 0) {
        gint index, start;
        pango_find_paragraph_boundary(utf8String, len, &index, &start);
        g_string_append_len(ret, utf8String, index);
        if (index == start)
            break;
        g_string_append_c(ret, ' ');
        utf8String += start;
        len -= start;
    }

    g_free(utf8String);
    return g_string_free(ret, FALSE);
}

static PangoLayout* getPangoLayoutForAtk(AtkText* textObject)
{
    AccessibilityObject* coreObject = core(textObject);

    HostWindow* hostWindow = coreObject->document()->view()->hostWindow();
    if (!hostWindow)
        return 0;
    PlatformPageClient webView = hostWindow->platformPageClient();
    if (!webView)
        return 0;

    GString* str = g_string_new(NULL);

    AccessibilityRenderObject* accObject = static_cast<AccessibilityRenderObject*>(coreObject);
    if (!accObject)
        return 0;
    RenderText* renderText = toRenderText(accObject->renderer());
    if (!renderText)
        return 0;

    // Create a string with the layout as it appears on the screen
    InlineTextBox* box = renderText->firstTextBox();
    while (box) {
        gchar *text = convertUniCharToUTF8(renderText->characters(), renderText->textLength(), box->start(), box->end());
        g_string_append(str, text);
        g_string_append(str, "\n");
        box = box->nextTextBox();
    }

    PangoLayout* layout = gtk_widget_create_pango_layout(static_cast<GtkWidget*>(webView), g_string_free(str, FALSE));
    g_object_set_data_full(G_OBJECT(textObject), "webkit-accessible-pango-layout", layout, g_object_unref);
    return layout;
}

static gchar* webkit_accessible_text_get_text_after_offset(AtkText* text, gint offset, AtkTextBoundary boundaryType, gint* startOffset, gint* endOffset)
{
    return gail_text_util_get_text(getGailTextUtilForAtk(text), getPangoLayoutForAtk(text), GAIL_AFTER_OFFSET, boundaryType, offset, startOffset, endOffset);
}

static gchar* webkit_accessible_text_get_text_at_offset(AtkText* text, gint offset, AtkTextBoundary boundaryType, gint* startOffset, gint* endOffset)
{
    return gail_text_util_get_text(getGailTextUtilForAtk(text), getPangoLayoutForAtk(text), GAIL_AT_OFFSET, boundaryType, offset, startOffset, endOffset);
}

static gchar* webkit_accessible_text_get_text_before_offset(AtkText* text, gint offset, AtkTextBoundary boundaryType, gint* startOffset, gint* endOffset)
{
    return gail_text_util_get_text(getGailTextUtilForAtk(text), getPangoLayoutForAtk(text), GAIL_BEFORE_OFFSET, boundaryType, offset, startOffset, endOffset);
}

static gunichar webkit_accessible_text_get_character_at_offset(AtkText* text, gint offset)
{
    notImplemented();
    return NULL;
}

static gint webkit_accessible_text_get_caret_offset(AtkText* text)
{
    // TODO: Verify this for RTL text.
    return core(text)->selection().end().offsetInContainerNode();
}

static AtkAttributeSet* webkit_accessible_text_get_run_attributes(AtkText* text, gint offset, gint* start_offset, gint* end_offset)
{
    notImplemented();
    return NULL;
}

static AtkAttributeSet* webkit_accessible_text_get_default_attributes(AtkText* text)
{
    notImplemented();
    return NULL;
}

static void webkit_accessible_text_get_character_extents(AtkText* text, gint offset, gint* x, gint* y, gint* width, gint* height, AtkCoordType coords)
{
    IntRect extents = core(text)->doAXBoundsForRange(PlainTextRange(offset, 1));
    // FIXME: Use the AtkCoordType
    // Requires WebCore::ScrollView::contentsToScreen() to be implemented

#if 0
    switch(coords) {
    case ATK_XY_SCREEN:
        extents = core(text)->document()->view()->contentsToScreen(extents);
        break;
    case ATK_XY_WINDOW:
        // No-op
        break;
    }
#endif

    *x = extents.x();
    *y = extents.y();
    *width = extents.width();
    *height = extents.height();
}

static gint webkit_accessible_text_get_character_count(AtkText* text)
{
    AccessibilityObject* coreObject = core(text);

    if (coreObject->isTextControl())
        return coreObject->textLength();
    else
        return coreObject->textUnderElement().length();
}

static gint webkit_accessible_text_get_offset_at_point(AtkText* text, gint x, gint y, AtkCoordType coords)
{
    // FIXME: Use the AtkCoordType
    // TODO: Is it correct to ignore range.length?
    IntPoint pos(x, y);
    PlainTextRange range = core(text)->doAXRangeForPosition(pos);
    return range.start;
}

static bool selectionBelongsToObject(AccessibilityObject *coreObject, VisibleSelection& selection)
{
    if (!coreObject->isAccessibilityRenderObject())
        return false;

    Node* node = static_cast<AccessibilityRenderObject*>(coreObject)->renderer()->node();
    return node == selection.base().containerNode();
}

static gint webkit_accessible_text_get_n_selections(AtkText* text)
{
    AccessibilityObject* coreObject = core(text);
    VisibleSelection selection = coreObject->selection();

    // We don't support multiple selections for now, so there's only
    // two possibilities
    // Also, we don't want to do anything if the selection does not
    // belong to the currently selected object. We have to check since
    // there's no way to get the selection for a given object, only
    // the global one (the API is a bit confusing)
    return !selectionBelongsToObject(coreObject, selection) || selection.isNone() ? 0 : 1;
}

static gchar* webkit_accessible_text_get_selection(AtkText* text, gint selection_num, gint* start_offset, gint* end_offset)
{
    AccessibilityObject* coreObject = core(text);
    VisibleSelection selection = coreObject->selection();

    // WebCore does not support multiple selection, so anything but 0 does not make sense for now.
    // Also, we don't want to do anything if the selection does not
    // belong to the currently selected object. We have to check since
    // there's no way to get the selection for a given object, only
    // the global one (the API is a bit confusing)
    if (selection_num != 0 || !selectionBelongsToObject(coreObject, selection)) {
        *start_offset = *end_offset = 0;
        return NULL;
    }

    *start_offset = selection.start().offsetInContainerNode();
    *end_offset = selection.end().offsetInContainerNode();

    return webkit_accessible_text_get_text(text, *start_offset, *end_offset);
}

static gboolean webkit_accessible_text_add_selection(AtkText* text, gint start_offset, gint end_offset)
{
    notImplemented();
    return FALSE;
}

static gboolean webkit_accessible_text_remove_selection(AtkText* text, gint selection_num)
{
    notImplemented();
    return FALSE;
}

static gboolean webkit_accessible_text_set_selection(AtkText* text, gint selection_num, gint start_offset, gint end_offset)
{
    notImplemented();
    return FALSE;
}

static gboolean webkit_accessible_text_set_caret_offset(AtkText* text, gint offset)
{
    AccessibilityObject* coreObject = core(text);

    // FIXME: We need to reimplement visiblePositionRangeForRange here
    // because the actual function checks the offset is within the
    // boundaries of text().length(), but text() only works for text
    // controls...
    VisiblePosition startPosition = coreObject->visiblePositionForIndex(offset);
    startPosition.setAffinity(DOWNSTREAM);
    VisiblePosition endPosition = coreObject->visiblePositionForIndex(offset);
    VisiblePositionRange range = VisiblePositionRange(startPosition, endPosition);

    coreObject->setSelectedVisiblePositionRange(range);
    return TRUE;
}

static void atk_text_interface_init(AtkTextIface* iface)
{
    iface->get_text = webkit_accessible_text_get_text;
    iface->get_text_after_offset = webkit_accessible_text_get_text_after_offset;
    iface->get_text_at_offset = webkit_accessible_text_get_text_at_offset;
    iface->get_character_at_offset = webkit_accessible_text_get_character_at_offset;
    iface->get_text_before_offset = webkit_accessible_text_get_text_before_offset;
    iface->get_caret_offset = webkit_accessible_text_get_caret_offset;
    iface->get_run_attributes = webkit_accessible_text_get_run_attributes;
    iface->get_default_attributes = webkit_accessible_text_get_default_attributes;
    iface->get_character_extents = webkit_accessible_text_get_character_extents;
    iface->get_character_count = webkit_accessible_text_get_character_count;
    iface->get_offset_at_point = webkit_accessible_text_get_offset_at_point;
    iface->get_n_selections = webkit_accessible_text_get_n_selections;
    iface->get_selection = webkit_accessible_text_get_selection;

    // set methods
    iface->add_selection = webkit_accessible_text_add_selection;
    iface->remove_selection = webkit_accessible_text_remove_selection;
    iface->set_selection = webkit_accessible_text_set_selection;
    iface->set_caret_offset = webkit_accessible_text_set_caret_offset;
}

// EditableText

static gboolean webkit_accessible_editable_text_set_run_attributes(AtkEditableText* text, AtkAttributeSet* attrib_set, gint start_offset, gint end_offset)
{
    notImplemented();
    return FALSE;
}

static void webkit_accessible_editable_text_set_text_contents(AtkEditableText* text, const gchar* string)
{
    // FIXME: string nullcheck?
    core(text)->setValue(String::fromUTF8(string));
}

static void webkit_accessible_editable_text_insert_text(AtkEditableText* text, const gchar* string, gint length, gint* position)
{
    // FIXME: string nullcheck?

    AccessibilityObject* coreObject = core(text);
    // FIXME: Not implemented in WebCore
    //coreObject->setSelectedTextRange(PlainTextRange(*position, 0));
    //coreObject->setSelectedText(String::fromUTF8(string));

    if (!coreObject->document() || !coreObject->document()->frame())
        return;
    coreObject->setSelectedVisiblePositionRange(coreObject->visiblePositionRangeForRange(PlainTextRange(*position, 0)));
    coreObject->setFocused(true);
    // FIXME: We should set position to the actual inserted text length, which may be less than that requested.
    if (coreObject->document()->frame()->editor()->insertTextWithoutSendingTextEvent(String::fromUTF8(string), false, 0))
        *position += length;
}

static void webkit_accessible_editable_text_copy_text(AtkEditableText* text, gint start_pos, gint end_pos)
{
    notImplemented();
}

static void webkit_accessible_editable_text_cut_text(AtkEditableText* text, gint start_pos, gint end_pos)
{
    notImplemented();
}

static void webkit_accessible_editable_text_delete_text(AtkEditableText* text, gint start_pos, gint end_pos)
{
    AccessibilityObject* coreObject = core(text);
    // FIXME: Not implemented in WebCore
    //coreObject->setSelectedTextRange(PlainTextRange(start_pos, end_pos - start_pos));
    //coreObject->setSelectedText(String());

    if (!coreObject->document() || !coreObject->document()->frame())
        return;
    coreObject->setSelectedVisiblePositionRange(coreObject->visiblePositionRangeForRange(PlainTextRange(start_pos, end_pos - start_pos)));
    coreObject->setFocused(true);
    coreObject->document()->frame()->editor()->performDelete();
}

static void webkit_accessible_editable_text_paste_text(AtkEditableText* text, gint position)
{
    notImplemented();
}

static void atk_editable_text_interface_init(AtkEditableTextIface* iface)
{
    iface->set_run_attributes = webkit_accessible_editable_text_set_run_attributes;
    iface->set_text_contents = webkit_accessible_editable_text_set_text_contents;
    iface->insert_text = webkit_accessible_editable_text_insert_text;
    iface->copy_text = webkit_accessible_editable_text_copy_text;
    iface->cut_text = webkit_accessible_editable_text_cut_text;
    iface->delete_text = webkit_accessible_editable_text_delete_text;
    iface->paste_text = webkit_accessible_editable_text_paste_text;
}

static void contentsToAtk(AccessibilityObject* coreObject, AtkCoordType coordType, IntRect rect, gint* x, gint* y, gint* width = 0, gint* height = 0)
{
    FrameView* frameView = coreObject->documentFrameView();

    if (frameView) {
        switch (coordType) {
        case ATK_XY_WINDOW:
            rect = frameView->contentsToWindow(rect);
            break;
        case ATK_XY_SCREEN:
            rect = frameView->contentsToScreen(rect);
            break;
        }
    }

    if (x)
        *x = rect.x();
    if (y)
        *y = rect.y();
    if (width)
        *width = rect.width();
    if (height)
        *height = rect.height();
}

static IntPoint atkToContents(AccessibilityObject* coreObject, AtkCoordType coordType, gint x, gint y)
{
    IntPoint pos(x, y);

    FrameView* frameView = coreObject->documentFrameView();
    if (frameView) {
        switch (coordType) {
        case ATK_XY_SCREEN:
            return frameView->screenToContents(pos);
        case ATK_XY_WINDOW:
            return frameView->windowToContents(pos);
        }
    }

    return pos;
}

static AtkObject* webkit_accessible_component_ref_accessible_at_point(AtkComponent* component, gint x, gint y, AtkCoordType coordType)
{
    IntPoint pos = atkToContents(core(component), coordType, x, y);
    AccessibilityObject* target = core(component)->doAccessibilityHitTest(pos);
    if (!target)
        return NULL;
    g_object_ref(target->wrapper());
    return target->wrapper();
}

static void webkit_accessible_component_get_extents(AtkComponent* component, gint* x, gint* y, gint* width, gint* height, AtkCoordType coordType)
{
    IntRect rect = core(component)->elementRect();
    contentsToAtk(core(component), coordType, rect, x, y, width, height);
}

static gboolean webkit_accessible_component_grab_focus(AtkComponent* component)
{
    core(component)->setFocused(true);
    return core(component)->isFocused();
}

static void atk_component_interface_init(AtkComponentIface *iface)
{
    iface->ref_accessible_at_point = webkit_accessible_component_ref_accessible_at_point;
    iface->get_extents = webkit_accessible_component_get_extents;
    iface->grab_focus = webkit_accessible_component_grab_focus;
}

// Image

static void webkit_accessible_image_get_image_position(AtkImage* image, gint* x, gint* y, AtkCoordType coordType)
{
    IntRect rect = core(image)->elementRect();
    contentsToAtk(core(image), coordType, rect, x, y);
}

static const gchar* webkit_accessible_image_get_image_description(AtkImage* image)
{
    return returnString(core(image)->accessibilityDescription());
}

static void webkit_accessible_image_get_image_size(AtkImage* image, gint* width, gint* height)
{
    IntSize size = core(image)->size();

    if (width)
        *width = size.width();
    if (height)
        *height = size.height();
}

static void atk_image_interface_init(AtkImageIface* iface)
{
    iface->get_image_position = webkit_accessible_image_get_image_position;
    iface->get_image_description = webkit_accessible_image_get_image_description;
    iface->get_image_size = webkit_accessible_image_get_image_size;
}

static const GInterfaceInfo AtkInterfacesInitFunctions[] = {
    {(GInterfaceInitFunc)atk_action_interface_init,
     (GInterfaceFinalizeFunc) NULL, NULL},
    {(GInterfaceInitFunc)atk_editable_text_interface_init,
     (GInterfaceFinalizeFunc) NULL, NULL},
    {(GInterfaceInitFunc)atk_text_interface_init,
     (GInterfaceFinalizeFunc) NULL, NULL},
    {(GInterfaceInitFunc)atk_component_interface_init,
     (GInterfaceFinalizeFunc) NULL, NULL},
    {(GInterfaceInitFunc)atk_image_interface_init,
     (GInterfaceFinalizeFunc) NULL, NULL}
};

enum WAIType {
    WAI_ACTION,
    WAI_EDITABLE_TEXT,
    WAI_TEXT,
    WAI_COMPONENT,
    WAI_IMAGE
};

static GType GetAtkInterfaceTypeFromWAIType(WAIType type)
{
  switch (type) {
  case WAI_ACTION:
      return ATK_TYPE_ACTION;
  case WAI_EDITABLE_TEXT:
      return ATK_TYPE_EDITABLE_TEXT;
  case WAI_TEXT:
      return ATK_TYPE_TEXT;
  case WAI_COMPONENT:
      return ATK_TYPE_COMPONENT;
  case WAI_IMAGE:
      return ATK_TYPE_IMAGE;
  }

  return G_TYPE_INVALID;
}

static guint16 getInterfaceMaskFromObject(AccessibilityObject* coreObject)
{
    guint16 interfaceMask = 0;

    // Component interface is always supported
    interfaceMask |= 1 << WAI_COMPONENT;

    // Action
    if (!coreObject->actionVerb().isEmpty())
        interfaceMask |= 1 << WAI_ACTION;

    // Text & Editable Text
    AccessibilityRole role = coreObject->roleValue();

    if (role == StaticTextRole)
        interfaceMask |= 1 << WAI_TEXT;

    if (coreObject->isAccessibilityRenderObject() && coreObject->isTextControl()) {
        if (coreObject->isReadOnly())
            interfaceMask |= 1 << WAI_TEXT;
        else
            interfaceMask |= 1 << WAI_EDITABLE_TEXT;
    }

    // Image
    if (coreObject->isImage())
        interfaceMask |= 1 << WAI_IMAGE;

    return interfaceMask;
}

static const char* getUniqueAccessibilityTypeName(guint16 interfaceMask)
{
#define WAI_TYPE_NAME_LEN (30) /* Enough for prefix + 5 hex characters (max) */
    static char name[WAI_TYPE_NAME_LEN + 1];
    
    g_sprintf(name, "WAIType%x", interfaceMask);
    name[WAI_TYPE_NAME_LEN] = '\0';
    
    return name;
}

static GType getAccessibilityTypeFromObject(AccessibilityObject* coreObject)
{
    static const GTypeInfo typeInfo = {
        sizeof(WebKitAccessibleClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) NULL,
        (GClassFinalizeFunc) NULL,
        NULL, /* class data */
        sizeof(WebKitAccessible), /* instance size */
        0, /* nb preallocs */
        (GInstanceInitFunc) NULL,
        NULL /* value table */
    };

    guint16 interfaceMask = getInterfaceMaskFromObject(coreObject);
    const char* atkTypeName = getUniqueAccessibilityTypeName(interfaceMask);
    GType type = g_type_from_name(atkTypeName);
    if (type)
        return type;

    type = g_type_register_static(WEBKIT_TYPE_ACCESSIBLE,
                                  atkTypeName,
                                  &typeInfo, GTypeFlags(0));
    for (guint i = 0; i < G_N_ELEMENTS(AtkInterfacesInitFunctions); i++) {
        if (interfaceMask & (1 << i))
            g_type_add_interface_static(type,
                                        GetAtkInterfaceTypeFromWAIType(static_cast<WAIType>(i)),
                                        &AtkInterfacesInitFunctions[i]);
    }

    return type;
}

WebKitAccessible* webkit_accessible_new(AccessibilityObject* coreObject)
{
    GType type = getAccessibilityTypeFromObject(coreObject);
    AtkObject* object = static_cast<AtkObject*>(g_object_new(type, NULL));

    atk_object_initialize(object, coreObject);

    return WEBKIT_ACCESSIBLE(object);
}

AccessibilityObject* webkit_accessible_get_accessibility_object(WebKitAccessible* accessible)
{
    return accessible->m_object;
}

void webkit_accessible_detach(WebKitAccessible* accessible)
{
    ASSERT(accessible->m_object);

    // We replace the WebCore AccessibilityObject with a fallback object that
    // provides default implementations to avoid repetitive null-checking after
    // detachment.
    accessible->m_object = fallbackObject();
}

AtkObject* webkit_accessible_get_focused_element(WebKitAccessible* accessible)
{
    if (!accessible->m_object)
        return 0;

    RefPtr<AccessibilityObject> focusedObj = accessible->m_object->focusedUIElement();
    if (!focusedObj)
        return 0;

    return focusedObj->wrapper();
}

#endif // HAVE(ACCESSIBILITY)
