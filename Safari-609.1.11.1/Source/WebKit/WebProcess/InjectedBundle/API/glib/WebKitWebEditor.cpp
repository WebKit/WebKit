/*
 * Copyright (C) 2015 Red Hat Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2,1 of the License, or (at your option) any later version.
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
#include "WebKitWebEditor.h"

#include "WebKitWebEditorPrivate.h"
#include "WebKitWebPagePrivate.h"
#include <wtf/glib/WTFGType.h>

using namespace WebKit;
using namespace WebCore;

/**
 * SECTION: WebKitWebEditor
 * @Short_description: Access to editing capabilities of a #WebKitWebPage
 * @Title: WebKitWebEditor
 * @See_also: #WebKitWebPage
 *
 * The WebKitWebEditor provides access to various editing capabilities of
 * a #WebKitWebPage such as a possibility to react to the current selection in
 * #WebKitWebPage.
 *
 * Since: 2.10
 */
enum {
    SELECTION_CHANGED,

    LAST_SIGNAL
};

struct _WebKitWebEditorPrivate {
    WebKitWebPage* webPage;
};

static guint signals[LAST_SIGNAL] = { 0, };

WEBKIT_DEFINE_TYPE(WebKitWebEditor, webkit_web_editor, G_TYPE_OBJECT)

static void webkit_web_editor_class_init(WebKitWebEditorClass* klass)
{
    /**
     * WebKitWebEditor::selection-changed:
     * @editor: the #WebKitWebEditor on which the signal is emitted
     *
     * This signal is emitted for every selection change inside a #WebKitWebPage
     * as well as for every caret position change as the caret is a collapsed
     * selection.
     *
     * Since: 2.10
     */
    signals[SELECTION_CHANGED] = g_signal_new(
        "selection-changed",
        G_TYPE_FROM_CLASS(klass),
        G_SIGNAL_RUN_LAST,
        0, nullptr, nullptr,
        g_cclosure_marshal_VOID__VOID,
        G_TYPE_NONE, 0);
}

class PageEditorClient final : public API::InjectedBundle::EditorClient {
public:
    explicit PageEditorClient(WebKitWebEditor* editor)
        : m_editor(editor)
    {
    }

private:
    void didChangeSelection(WebPage&, StringImpl*) override
    {
        g_signal_emit(m_editor, signals[SELECTION_CHANGED], 0);
    }

    WebKitWebEditor* m_editor;
};

WebKitWebEditor* webkitWebEditorCreate(WebKitWebPage* webPage)
{
    WebKitWebEditor* editor = WEBKIT_WEB_EDITOR(g_object_new(WEBKIT_TYPE_WEB_EDITOR, nullptr));
    editor->priv->webPage = webPage;
    webkitWebPageGetPage(webPage)->setInjectedBundleEditorClient(makeUnique<PageEditorClient>(editor));
    return editor;
}

/**
 * webkit_web_editor_get_page:
 * @editor: a #WebKitWebEditor
 *
 * Gets the #WebKitWebPage that is associated with the #WebKitWebEditor that can
 * be used to access the #WebKitDOMDocument currently loaded into it.
 *
 * Returns: (transfer none): the associated #WebKitWebPage
 *
 * Since: 2.10
 */
WebKitWebPage* webkit_web_editor_get_page(WebKitWebEditor* editor)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EDITOR(editor), nullptr);

    return editor->priv->webPage;
}
