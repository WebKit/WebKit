/*
 * Copyright (C) 2012 Igalia S.L.
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

#include "WebKitTestServer.h"
#include "WebViewTest.h"
#include <wtf/Vector.h>
#include <wtf/glib/GRefPtr.h>

static WebKitTestServer* kServer;

class ContextMenuTest: public WebViewTest {
public:
    enum ContextMenuItemStateFlags {
        Visible = 1 << 0,
        Enabled = 1 << 1,
        Checked = 1 << 2
    };

    void checkContextMenuEvent(GdkEvent* event)
    {
        g_assert_nonnull(event);
        g_assert_cmpint(event->type, ==, m_expectedEventType);

        switch (m_expectedEventType) {
        case GDK_BUTTON_PRESS:
            g_assert_cmpint(event->button.button, ==, 3);
            g_assert_cmpint(event->button.x, ==, m_menuPositionX);
            g_assert_cmpint(event->button.y, ==, m_menuPositionY);
            break;
        case GDK_KEY_PRESS:
            g_assert_cmpint(event->key.keyval, ==, GDK_KEY_Menu);
            break;
        case GDK_NOTHING:
            // GDK_NOTHING means that the context menu was triggered by the
            // popup-menu signal. We don't have anything to check here.
            break;
        default:
            g_assert_not_reached();
        }
    }

    static gboolean contextMenuCallback(WebKitWebView* webView, WebKitContextMenu* contextMenu, GdkEvent* event, WebKitHitTestResult* hitTestResult, ContextMenuTest* test)
    {
        g_assert_true(WEBKIT_IS_CONTEXT_MENU(contextMenu));
        test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(contextMenu));
        test->checkContextMenuEvent(event);
        g_assert_true(WEBKIT_IS_HIT_TEST_RESULT(hitTestResult));
        test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(hitTestResult));

        return test->contextMenu(contextMenu, event, hitTestResult);
    }

    static void contextMenuDismissedCallback(WebKitWebView*, ContextMenuTest* test)
    {
        test->contextMenuDismissed();
    }

    ContextMenuTest()
        : m_menuPositionX(0)
        , m_menuPositionY(0)
        , m_expectedEventType(GDK_BUTTON_PRESS)
    {
        g_signal_connect(m_webView, "context-menu", G_CALLBACK(contextMenuCallback), this);
        g_signal_connect(m_webView, "context-menu-dismissed", G_CALLBACK(contextMenuDismissedCallback), this);
    }

    ~ContextMenuTest()
    {
        g_signal_handlers_disconnect_matched(m_webView, G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, this);
    }

    virtual bool contextMenu(WebKitContextMenu*, GdkEvent*, WebKitHitTestResult*) = 0;

    virtual void contextMenuDismissed()
    {
        quitMainLoop();
    }

    GtkMenu* getPopupMenu()
    {
        GUniquePtr<GList> toplevels(gtk_window_list_toplevels());
        for (GList* iter = toplevels.get(); iter; iter = g_list_next(iter)) {
            if (!GTK_IS_WINDOW(iter->data))
                continue;

            GtkWidget* child = gtk_bin_get_child(GTK_BIN(iter->data));
            if (!GTK_IS_MENU(child))
                continue;

            if (gtk_menu_get_attach_widget(GTK_MENU(child)) == GTK_WIDGET(m_webView))
                return GTK_MENU(child);
        }
        g_assert_not_reached();
        return 0;
    }

    void checkActionState(GtkAction* action, unsigned state)
    {
        if (state & Visible)
            g_assert_true(gtk_action_get_visible(action));
        else
            g_assert_false(gtk_action_get_visible(action));

        if (state & Enabled)
            g_assert_true(gtk_action_get_sensitive(action));
        else
            g_assert_false(gtk_action_get_sensitive(action));

        if (GTK_IS_TOGGLE_ACTION(action)) {
            if (state & Checked)
                g_assert_true(gtk_toggle_action_get_active(GTK_TOGGLE_ACTION(action)));
            else
                g_assert_false(gtk_toggle_action_get_active(GTK_TOGGLE_ACTION(action)));
        }
    }

    GList* checkCurrentItemIsStockActionAndGetNext(GList* items, WebKitContextMenuAction stockAction, unsigned state)
    {
        g_assert_nonnull(items);
        g_assert_true(WEBKIT_IS_CONTEXT_MENU_ITEM(items->data));

        WebKitContextMenuItem* item = WEBKIT_CONTEXT_MENU_ITEM(items->data);
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(item));

        G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
        GtkAction* action = webkit_context_menu_item_get_action(item);
        g_assert_true(GTK_IS_ACTION(action));
        G_GNUC_END_IGNORE_DEPRECATIONS;

        GAction* gAction = webkit_context_menu_item_get_gaction(item);
        g_assert_true(G_IS_ACTION(gAction));

        g_assert_cmpint(webkit_context_menu_item_get_stock_action(item), ==, stockAction);

        checkActionState(action, state);

        return g_list_next(items);
    }

    GList* checkCurrentItemIsCustomActionAndGetNext(GList* items, const char* label, unsigned state)
    {
        g_assert_nonnull(items);
        g_assert_true(WEBKIT_IS_CONTEXT_MENU_ITEM(items->data));

        WebKitContextMenuItem* item = WEBKIT_CONTEXT_MENU_ITEM(items->data);
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(item));

        G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
        GtkAction* action = webkit_context_menu_item_get_action(item);
        g_assert_true(GTK_IS_ACTION(action));
        G_GNUC_END_IGNORE_DEPRECATIONS;

        GAction* gAction = webkit_context_menu_item_get_gaction(item);
        g_assert_true(G_IS_ACTION(gAction));
        g_assert_cmpstr(gtk_action_get_name(action), ==, g_action_get_name(gAction));
        g_assert_cmpint(gtk_action_get_sensitive(action), ==, g_action_get_enabled(gAction));
        if (GTK_IS_TOGGLE_ACTION(action)) {
            g_assert_true(g_variant_type_equal(g_action_get_state_type(gAction), G_VARIANT_TYPE_BOOLEAN));
            GRefPtr<GVariant> state = adoptGRef(g_action_get_state(gAction));
            g_assert_cmpint(gtk_toggle_action_get_active(GTK_TOGGLE_ACTION(action)), ==, g_variant_get_boolean(state.get()));
        } else
            g_assert_null(g_action_get_state_type(gAction));

        g_assert_cmpint(webkit_context_menu_item_get_stock_action(item), ==, WEBKIT_CONTEXT_MENU_ACTION_CUSTOM);
        g_assert_cmpstr(gtk_action_get_label(action), ==, label);

        checkActionState(action, state);

        return g_list_next(items);
    }

    GList* checkCurrentItemIsSubMenuAndGetNext(GList* items, const char* label, unsigned state, GList** subMenuIter)
    {
        g_assert_nonnull(items);
        g_assert_true(WEBKIT_IS_CONTEXT_MENU_ITEM(items->data));

        WebKitContextMenuItem* item = WEBKIT_CONTEXT_MENU_ITEM(items->data);
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(item));

        G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
        GtkAction* action = webkit_context_menu_item_get_action(item);
        g_assert_true(GTK_IS_ACTION(action));
        g_assert_cmpstr(gtk_action_get_label(action), ==, label);
        G_GNUC_END_IGNORE_DEPRECATIONS;

        GAction* gAction = webkit_context_menu_item_get_gaction(item);
        g_assert_true(G_IS_ACTION(gAction));

        checkActionState(action, state);

        WebKitContextMenu* subMenu = webkit_context_menu_item_get_submenu(item);
        g_assert_true(WEBKIT_IS_CONTEXT_MENU(subMenu));
        if (subMenuIter)
            *subMenuIter = webkit_context_menu_get_items(subMenu);

        return g_list_next(items);
    }

    GList* checkCurrentItemIsSeparatorAndGetNext(GList* items)
    {
        g_assert_nonnull(items);
        g_assert_true(WEBKIT_IS_CONTEXT_MENU_ITEM(items->data));

        WebKitContextMenuItem* item = WEBKIT_CONTEXT_MENU_ITEM(items->data);
        g_assert_true(webkit_context_menu_item_is_separator(item));

        return g_list_next(items);
    }

    static gboolean doRightClickIdleCallback(ContextMenuTest* test)
    {
        test->clickMouseButton(test->m_menuPositionX, test->m_menuPositionY, 3);
        return FALSE;
    }

    void showContextMenuAtPositionAndWaitUntilFinished(int x, int y)
    {
        m_menuPositionX = x;
        m_menuPositionY = y;
        g_idle_add(reinterpret_cast<GSourceFunc>(doRightClickIdleCallback), this);
        g_main_loop_run(m_mainLoop);
    }

    void showContextMenuAndWaitUntilFinished()
    {
        m_expectedEventType = GDK_BUTTON_PRESS;
        showContextMenuAtPositionAndWaitUntilFinished(0, 0);
    }

    static gboolean simulateEscKeyIdleCallback(ContextMenuTest* test)
    {
        test->keyStroke(GDK_KEY_Escape);
        return FALSE;
    }

    void dismissContextMenuAndWaitUntilFinished()
    {
        g_idle_add(reinterpret_cast<GSourceFunc>(simulateEscKeyIdleCallback), this);
        g_main_loop_run(m_mainLoop);
    }

    static gboolean emitPopupMenuSignalIdleCallback(ContextMenuTest* test)
    {
        test->emitPopupMenuSignal();
        return FALSE;
    }

    void showContextMenuTriggeredByPopupEventAndWaitUntilFinished()
    {
        m_expectedEventType = GDK_NOTHING;
        g_idle_add(reinterpret_cast<GSourceFunc>(emitPopupMenuSignalIdleCallback), this);
        g_main_loop_run(m_mainLoop);
    }

    static gboolean simulateMenuKeyIdleCallback(ContextMenuTest* test)
    {
        test->keyStroke(GDK_KEY_Menu);
        return FALSE;
    }

    void showContextMenuTriggeredByContextMenuKeyAndWaitUntilFinished()
    {
        m_expectedEventType = GDK_KEY_PRESS;
        g_idle_add(reinterpret_cast<GSourceFunc>(simulateMenuKeyIdleCallback), this);
        g_main_loop_run(m_mainLoop);
    }

    double m_menuPositionX;
    double m_menuPositionY;
    GdkEventType m_expectedEventType;
};

class ContextMenuDefaultTest: public ContextMenuTest {
public:
    MAKE_GLIB_TEST_FIXTURE(ContextMenuDefaultTest);

    enum DefaultMenuType {
        Navigation,
        Link,
        Image,
        LinkImage,
        Video,
        Audio,
        VideoLive,
        Editable,
        Selection
    };

    ContextMenuDefaultTest()
        : m_expectedMenuType(Navigation)
    {
    }

    bool contextMenu(WebKitContextMenu* contextMenu, GdkEvent* event, WebKitHitTestResult* hitTestResult)
    {
        GList* iter = webkit_context_menu_get_items(contextMenu);

        switch (m_expectedMenuType) {
        case Navigation:
            g_assert_false(webkit_hit_test_result_context_is_link(hitTestResult));
            g_assert_false(webkit_hit_test_result_context_is_image(hitTestResult));
            g_assert_false(webkit_hit_test_result_context_is_media(hitTestResult));
            g_assert_false(webkit_hit_test_result_context_is_editable(hitTestResult));
            g_assert_false(webkit_hit_test_result_context_is_selection(hitTestResult));
            iter = checkCurrentItemIsStockActionAndGetNext(iter, WEBKIT_CONTEXT_MENU_ACTION_GO_BACK, Visible);
            iter = checkCurrentItemIsStockActionAndGetNext(iter, WEBKIT_CONTEXT_MENU_ACTION_GO_FORWARD, Visible);
            iter = checkCurrentItemIsStockActionAndGetNext(iter, WEBKIT_CONTEXT_MENU_ACTION_STOP, Visible);
            iter = checkCurrentItemIsStockActionAndGetNext(iter, WEBKIT_CONTEXT_MENU_ACTION_RELOAD, Visible | Enabled);
            break;
        case Link:
            g_assert_true(webkit_hit_test_result_context_is_link(hitTestResult));
            g_assert_false(webkit_hit_test_result_context_is_image(hitTestResult));
            g_assert_false(webkit_hit_test_result_context_is_media(hitTestResult));
            g_assert_false(webkit_hit_test_result_context_is_editable(hitTestResult));
            g_assert_false(webkit_hit_test_result_context_is_selection(hitTestResult));
            iter = checkCurrentItemIsStockActionAndGetNext(iter, WEBKIT_CONTEXT_MENU_ACTION_OPEN_LINK, Visible | Enabled);
            iter = checkCurrentItemIsStockActionAndGetNext(iter, WEBKIT_CONTEXT_MENU_ACTION_OPEN_LINK_IN_NEW_WINDOW, Visible | Enabled);
            iter = checkCurrentItemIsStockActionAndGetNext(iter, WEBKIT_CONTEXT_MENU_ACTION_DOWNLOAD_LINK_TO_DISK, Visible | Enabled);
            iter = checkCurrentItemIsStockActionAndGetNext(iter, WEBKIT_CONTEXT_MENU_ACTION_COPY_LINK_TO_CLIPBOARD, Visible | Enabled);
            break;
        case Image:
            g_assert_false(webkit_hit_test_result_context_is_link(hitTestResult));
            g_assert_true(webkit_hit_test_result_context_is_image(hitTestResult));
            g_assert_false(webkit_hit_test_result_context_is_media(hitTestResult));
            g_assert_false(webkit_hit_test_result_context_is_editable(hitTestResult));
            g_assert_false(webkit_hit_test_result_context_is_selection(hitTestResult));
            iter = checkCurrentItemIsStockActionAndGetNext(iter, WEBKIT_CONTEXT_MENU_ACTION_OPEN_IMAGE_IN_NEW_WINDOW, Visible | Enabled);
            iter = checkCurrentItemIsStockActionAndGetNext(iter, WEBKIT_CONTEXT_MENU_ACTION_DOWNLOAD_IMAGE_TO_DISK, Visible | Enabled);
            iter = checkCurrentItemIsStockActionAndGetNext(iter, WEBKIT_CONTEXT_MENU_ACTION_COPY_IMAGE_TO_CLIPBOARD, Visible | Enabled);
            iter = checkCurrentItemIsStockActionAndGetNext(iter, WEBKIT_CONTEXT_MENU_ACTION_COPY_IMAGE_URL_TO_CLIPBOARD, Visible | Enabled);
            break;
        case LinkImage:
            g_assert_true(webkit_hit_test_result_context_is_link(hitTestResult));
            g_assert_true(webkit_hit_test_result_context_is_image(hitTestResult));
            g_assert_false(webkit_hit_test_result_context_is_media(hitTestResult));
            g_assert_false(webkit_hit_test_result_context_is_editable(hitTestResult));
            g_assert_false(webkit_hit_test_result_context_is_selection(hitTestResult));
            iter = checkCurrentItemIsStockActionAndGetNext(iter, WEBKIT_CONTEXT_MENU_ACTION_OPEN_LINK, Visible | Enabled);
            iter = checkCurrentItemIsStockActionAndGetNext(iter, WEBKIT_CONTEXT_MENU_ACTION_OPEN_LINK_IN_NEW_WINDOW, Visible | Enabled);
            iter = checkCurrentItemIsStockActionAndGetNext(iter, WEBKIT_CONTEXT_MENU_ACTION_DOWNLOAD_LINK_TO_DISK, Visible | Enabled);
            iter = checkCurrentItemIsStockActionAndGetNext(iter, WEBKIT_CONTEXT_MENU_ACTION_COPY_LINK_TO_CLIPBOARD, Visible | Enabled);
            iter = checkCurrentItemIsSeparatorAndGetNext(iter);
            iter = checkCurrentItemIsStockActionAndGetNext(iter, WEBKIT_CONTEXT_MENU_ACTION_OPEN_IMAGE_IN_NEW_WINDOW, Visible | Enabled);
            iter = checkCurrentItemIsStockActionAndGetNext(iter, WEBKIT_CONTEXT_MENU_ACTION_DOWNLOAD_IMAGE_TO_DISK, Visible | Enabled);
            iter = checkCurrentItemIsStockActionAndGetNext(iter, WEBKIT_CONTEXT_MENU_ACTION_COPY_IMAGE_TO_CLIPBOARD, Visible | Enabled);
            iter = checkCurrentItemIsStockActionAndGetNext(iter, WEBKIT_CONTEXT_MENU_ACTION_COPY_IMAGE_URL_TO_CLIPBOARD, Visible | Enabled);
            break;
        case Video:
            g_assert_false(webkit_hit_test_result_context_is_link(hitTestResult));
            g_assert_false(webkit_hit_test_result_context_is_image(hitTestResult));
            g_assert_true(webkit_hit_test_result_context_is_media(hitTestResult));
            g_assert_false(webkit_hit_test_result_context_is_editable(hitTestResult));
            g_assert_false(webkit_hit_test_result_context_is_selection(hitTestResult));
            iter = checkCurrentItemIsStockActionAndGetNext(iter, WEBKIT_CONTEXT_MENU_ACTION_MEDIA_PLAY, Visible | Enabled);
            iter = checkCurrentItemIsStockActionAndGetNext(iter, WEBKIT_CONTEXT_MENU_ACTION_MEDIA_MUTE, Visible);
            iter = checkCurrentItemIsStockActionAndGetNext(iter, WEBKIT_CONTEXT_MENU_ACTION_TOGGLE_MEDIA_CONTROLS, Visible | Enabled | Checked);
            iter = checkCurrentItemIsStockActionAndGetNext(iter, WEBKIT_CONTEXT_MENU_ACTION_TOGGLE_MEDIA_LOOP, Visible | Enabled);
            iter = checkCurrentItemIsStockActionAndGetNext(iter, WEBKIT_CONTEXT_MENU_ACTION_ENTER_VIDEO_FULLSCREEN, Visible | Enabled);
            iter = checkCurrentItemIsSeparatorAndGetNext(iter);
            iter = checkCurrentItemIsStockActionAndGetNext(iter, WEBKIT_CONTEXT_MENU_ACTION_COPY_VIDEO_LINK_TO_CLIPBOARD, Visible | Enabled);
            iter = checkCurrentItemIsStockActionAndGetNext(iter, WEBKIT_CONTEXT_MENU_ACTION_OPEN_VIDEO_IN_NEW_WINDOW, Visible | Enabled);
            iter = checkCurrentItemIsStockActionAndGetNext(iter, WEBKIT_CONTEXT_MENU_ACTION_DOWNLOAD_VIDEO_TO_DISK, Visible | Enabled);
            break;
        case Audio:
            g_assert_false(webkit_hit_test_result_context_is_link(hitTestResult));
            g_assert_false(webkit_hit_test_result_context_is_image(hitTestResult));
            g_assert_true(webkit_hit_test_result_context_is_media(hitTestResult));
            g_assert_false(webkit_hit_test_result_context_is_editable(hitTestResult));
            g_assert_false(webkit_hit_test_result_context_is_selection(hitTestResult));
            iter = checkCurrentItemIsStockActionAndGetNext(iter, WEBKIT_CONTEXT_MENU_ACTION_MEDIA_PLAY, Visible | Enabled);
            iter = checkCurrentItemIsStockActionAndGetNext(iter, WEBKIT_CONTEXT_MENU_ACTION_MEDIA_MUTE, Visible);
            iter = checkCurrentItemIsStockActionAndGetNext(iter, WEBKIT_CONTEXT_MENU_ACTION_TOGGLE_MEDIA_CONTROLS, Visible | Enabled | Checked);
            iter = checkCurrentItemIsStockActionAndGetNext(iter, WEBKIT_CONTEXT_MENU_ACTION_TOGGLE_MEDIA_LOOP, Visible | Enabled);
            iter = checkCurrentItemIsStockActionAndGetNext(iter, WEBKIT_CONTEXT_MENU_ACTION_ENTER_VIDEO_FULLSCREEN, Visible);
            iter = checkCurrentItemIsSeparatorAndGetNext(iter);
            iter = checkCurrentItemIsStockActionAndGetNext(iter, WEBKIT_CONTEXT_MENU_ACTION_COPY_AUDIO_LINK_TO_CLIPBOARD, Visible | Enabled);
            iter = checkCurrentItemIsStockActionAndGetNext(iter, WEBKIT_CONTEXT_MENU_ACTION_OPEN_AUDIO_IN_NEW_WINDOW, Visible | Enabled);
            iter = checkCurrentItemIsStockActionAndGetNext(iter, WEBKIT_CONTEXT_MENU_ACTION_DOWNLOAD_AUDIO_TO_DISK, Visible | Enabled);
            break;
        case VideoLive:
            g_assert_false(webkit_hit_test_result_context_is_link(hitTestResult));
            g_assert_false(webkit_hit_test_result_context_is_image(hitTestResult));
            g_assert_true(webkit_hit_test_result_context_is_media(hitTestResult));
            g_assert_false(webkit_hit_test_result_context_is_editable(hitTestResult));
            g_assert_false(webkit_hit_test_result_context_is_selection(hitTestResult));
            iter = checkCurrentItemIsStockActionAndGetNext(iter, WEBKIT_CONTEXT_MENU_ACTION_MEDIA_PLAY, Visible | Enabled);
            iter = checkCurrentItemIsStockActionAndGetNext(iter, WEBKIT_CONTEXT_MENU_ACTION_MEDIA_MUTE, Visible | Enabled);
            iter = checkCurrentItemIsStockActionAndGetNext(iter, WEBKIT_CONTEXT_MENU_ACTION_TOGGLE_MEDIA_CONTROLS, Visible | Enabled);
            iter = checkCurrentItemIsStockActionAndGetNext(iter, WEBKIT_CONTEXT_MENU_ACTION_TOGGLE_MEDIA_LOOP, Visible | Enabled);
            iter = checkCurrentItemIsStockActionAndGetNext(iter, WEBKIT_CONTEXT_MENU_ACTION_ENTER_VIDEO_FULLSCREEN, Visible | Enabled);
            break;
        case Editable:
            g_assert_false(webkit_hit_test_result_context_is_link(hitTestResult));
            g_assert_false(webkit_hit_test_result_context_is_image(hitTestResult));
            g_assert_false(webkit_hit_test_result_context_is_media(hitTestResult));
            g_assert_true(webkit_hit_test_result_context_is_editable(hitTestResult));
            g_assert_false(webkit_hit_test_result_context_is_selection(hitTestResult));
            iter = checkCurrentItemIsStockActionAndGetNext(iter, WEBKIT_CONTEXT_MENU_ACTION_CUT, Visible);
            iter = checkCurrentItemIsStockActionAndGetNext(iter, WEBKIT_CONTEXT_MENU_ACTION_COPY, Visible);
            iter = checkCurrentItemIsStockActionAndGetNext(iter, WEBKIT_CONTEXT_MENU_ACTION_PASTE, Visible | Enabled);
            iter = checkCurrentItemIsStockActionAndGetNext(iter, WEBKIT_CONTEXT_MENU_ACTION_DELETE, Visible);
            iter = checkCurrentItemIsSeparatorAndGetNext(iter);
            iter = checkCurrentItemIsStockActionAndGetNext(iter, WEBKIT_CONTEXT_MENU_ACTION_SELECT_ALL, Visible | Enabled);
            iter = checkCurrentItemIsStockActionAndGetNext(iter, WEBKIT_CONTEXT_MENU_ACTION_INSERT_EMOJI, Visible | Enabled);
            iter = checkCurrentItemIsSeparatorAndGetNext(iter);
            iter = checkCurrentItemIsStockActionAndGetNext(iter, WEBKIT_CONTEXT_MENU_ACTION_UNICODE, Visible | Enabled);
            break;
        case Selection:
            g_assert_false(webkit_hit_test_result_context_is_link(hitTestResult));
            g_assert_false(webkit_hit_test_result_context_is_image(hitTestResult));
            g_assert_false(webkit_hit_test_result_context_is_media(hitTestResult));
            g_assert_false(webkit_hit_test_result_context_is_editable(hitTestResult));
            g_assert_true(webkit_hit_test_result_context_is_selection(hitTestResult));
            iter = checkCurrentItemIsStockActionAndGetNext(iter, WEBKIT_CONTEXT_MENU_ACTION_COPY, Visible | Enabled);
            break;
        default:
            g_assert_not_reached();
        }

        if (webkit_settings_get_enable_developer_extras(webkit_web_view_get_settings(m_webView))) {
            iter = checkCurrentItemIsSeparatorAndGetNext(iter);
            iter = checkCurrentItemIsStockActionAndGetNext(iter, WEBKIT_CONTEXT_MENU_ACTION_INSPECT_ELEMENT, Visible | Enabled);
        }
        g_assert_null(iter);

        quitMainLoop();

        return true;
    }

    DefaultMenuType m_expectedMenuType;
};

static void prepareContextMenuTestView(ContextMenuDefaultTest* test)
{
    GUniquePtr<char> baseDir(g_strdup_printf("file://%s/", Test::getResourcesDir().data()));
    const char* linksHTML =
        "<html><body>"
        " <a style='position:absolute; left:1; top:1' href='http://www.webkitgtk.org' title='WebKitGTK Title'>WebKitGTK Website</a>"
        " <img style='position:absolute; left:1; top:10' src='blank.ico' width=5 height=5></img>"
        " <a style='position:absolute; left:1; top:20' href='http://www.webkitgtk.org/logo' title='WebKitGTK Logo'><img src='blank.ico' width=5 height=5></img></a>"
        " <input style='position:absolute; left:1; top:30' size='10'></input>"
        " <video style='position:absolute; left:1; top:50' width='300' height='300' controls='controls' preload='none'><source src='silence.webm' type='video/webm' /></video>"
        " <audio style='position:absolute; left:1; top:60' width='50' height='20' controls='controls' preload='none'><source src='track.ogg' type='audio/ogg' /></audio>"
        " <p style='position:absolute; left:1; top:90' id='text_to_select'>Lorem ipsum.</p>"
        " <script>"
        "  window.getSelection().removeAllRanges();"
        "  var select_range = document.createRange();"
        "  select_range.selectNodeContents(document.getElementById('text_to_select'));"
        "  window.getSelection().addRange(select_range);"
        " </script>"
        "</body></html>";
    test->loadHtml(linksHTML, baseDir.get());
    test->waitUntilLoadFinished();
}

static void testContextMenuDefaultMenu(ContextMenuDefaultTest* test, gconstpointer)
{
    test->showInWindowAndWaitUntilMapped();

    prepareContextMenuTestView(test);

    // Context menu for selection.
    // This test should always be the first because any other click removes the selection.
    test->m_expectedMenuType = ContextMenuDefaultTest::Selection;
    test->showContextMenuAtPositionAndWaitUntilFinished(2, 115);

    // Context menu for document.
    test->m_expectedMenuType = ContextMenuDefaultTest::Navigation;
    test->showContextMenuAtPositionAndWaitUntilFinished(0, 0);

    // Context menu for link.
    test->m_expectedMenuType = ContextMenuDefaultTest::Link;
    test->showContextMenuAtPositionAndWaitUntilFinished(1, 1);

    // Context menu for image.
    test->m_expectedMenuType = ContextMenuDefaultTest::Image;
    test->showContextMenuAtPositionAndWaitUntilFinished(1, 10);

    // Enable developer extras now, so that inspector element
    // will be shown in the default context menu.
    webkit_settings_set_enable_developer_extras(webkit_web_view_get_settings(test->m_webView), TRUE);

    // Context menu for image link.
    test->m_expectedMenuType = ContextMenuDefaultTest::LinkImage;
    test->showContextMenuAtPositionAndWaitUntilFinished(1, 20);

    // Context menu for video.
    test->m_expectedMenuType = ContextMenuDefaultTest::Video;
    test->showContextMenuAtPositionAndWaitUntilFinished(1, 50);

    // Context menu for audio.
    test->m_expectedMenuType = ContextMenuDefaultTest::Audio;
    test->showContextMenuAtPositionAndWaitUntilFinished(1, 60);

    // Context menu for editable.
    test->m_expectedMenuType = ContextMenuDefaultTest::Editable;
    test->showContextMenuAtPositionAndWaitUntilFinished(5, 35);
}

static void testPopupEventSignal(ContextMenuDefaultTest* test, gconstpointer)
{
    test->showInWindowAndWaitUntilMapped();

    prepareContextMenuTestView(test);

    test->m_expectedMenuType = ContextMenuDefaultTest::Selection;
    test->showContextMenuTriggeredByPopupEventAndWaitUntilFinished();
}

static void testContextMenuKey(ContextMenuDefaultTest* test, gconstpointer)
{
    test->showInWindowAndWaitUntilMapped();

    prepareContextMenuTestView(test);

    test->m_expectedMenuType = ContextMenuDefaultTest::Selection;
    test->showContextMenuTriggeredByContextMenuKeyAndWaitUntilFinished();
}

class ContextMenuCustomTest: public ContextMenuTest {
public:
    MAKE_GLIB_TEST_FIXTURE(ContextMenuCustomTest);

    bool contextMenu(WebKitContextMenu* contextMenu, GdkEvent*, WebKitHitTestResult* hitTestResult)
    {
        // Append our custom item to the default menu.
        G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
        if (m_action)
            webkit_context_menu_append(contextMenu, webkit_context_menu_item_new(m_action.get()));
        else if (m_gAction)
            webkit_context_menu_append(contextMenu, webkit_context_menu_item_new_from_gaction(m_gAction.get(), m_gActionTitle.data(), m_expectedTarget.get()));
        G_GNUC_END_IGNORE_DEPRECATIONS;
        quitMainLoop();

        return false;
    }

    GtkMenuItem* getMenuItem(GtkMenu* menu, const gchar* itemLabel)
    {
        GUniquePtr<GList> items(gtk_container_get_children(GTK_CONTAINER(menu)));
        for (GList* iter = items.get(); iter; iter = g_list_next(iter)) {
            GtkMenuItem* child = GTK_MENU_ITEM(iter->data);
            if (g_str_equal(itemLabel, gtk_menu_item_get_label(child)))
                return child;
        }
        g_assert_not_reached();
        return 0;
    }

    void activateMenuItem()
    {
        g_assert_nonnull(m_itemToActivateLabel);
        GtkMenu* menu = getPopupMenu();
        GtkMenuItem* item = getMenuItem(menu, m_itemToActivateLabel);
        gtk_menu_shell_activate_item(GTK_MENU_SHELL(menu), GTK_WIDGET(item), TRUE);
        m_itemToActivateLabel = nullptr;
    }

    static gboolean activateMenuItemIdleCallback(gpointer userData)
    {
        ContextMenuCustomTest* test = static_cast<ContextMenuCustomTest*>(userData);
        test->activateMenuItem();
        return FALSE;
    }

    void activateCustomMenuItemAndWaitUntilActivated(const char* actionLabel)
    {
        m_activated = m_toggled = false;
        m_itemToActivateLabel = actionLabel;
        g_idle_add(activateMenuItemIdleCallback, this);
        g_main_loop_run(m_mainLoop);
    }

    void toggleCustomMenuItemAndWaitUntilToggled(const char* actionLabel)
    {
        activateCustomMenuItemAndWaitUntilActivated(actionLabel);
    }

    static void actionActivatedCallback(ContextMenuCustomTest* test, GVariant* target)
    {
        if (test->m_gAction) {
            if (g_action_get_state_type(test->m_gAction.get())) {
                GRefPtr<GVariant> state = adoptGRef(g_action_get_state(test->m_gAction.get()));
                g_action_change_state(test->m_gAction.get(), g_variant_new_boolean(!g_variant_get_boolean(state.get())));
            } else {
                test->m_activated = true;
                if (test->m_expectedTarget)
                    g_assert_true(g_variant_equal(test->m_expectedTarget.get(), target));
                else
                    g_assert_null(target);
            }
        } else
            test->m_activated = true;
    }

    static void actionToggledCallback(ContextMenuCustomTest* test)
    {
        test->m_toggled = true;
    }

    void setAction(GtkAction* action)
    {
        m_action = action;
        m_gAction = nullptr;
        m_expectedTarget = nullptr;
        if (GTK_IS_TOGGLE_ACTION(action))
            g_signal_connect_swapped(action, "toggled", G_CALLBACK(actionToggledCallback), this);
        else
            g_signal_connect_swapped(action, "activate", G_CALLBACK(actionActivatedCallback), this);
    }

    void setAction(GAction* action, const char* title, GVariant* target = nullptr)
    {
        m_gAction = action;
        m_gActionTitle = title;
        m_action = nullptr;
        m_expectedTarget = target;
        g_signal_connect_swapped(action, "activate", G_CALLBACK(actionActivatedCallback), this);
        if (g_action_get_state_type(action))
            g_signal_connect_swapped(action, "change-state", G_CALLBACK(actionToggledCallback), this);
    }

    GRefPtr<GtkAction> m_action;
    GRefPtr<GAction> m_gAction;
    CString m_gActionTitle;
    GRefPtr<GVariant> m_expectedTarget;
    const char* m_itemToActivateLabel { nullptr };
    bool m_activated { false };
    bool m_toggled { false };
};

static void testContextMenuPopulateMenu(ContextMenuCustomTest* test, gconstpointer)
{
    test->showInWindowAndWaitUntilMapped();

    test->loadHtml("<html><body>WebKitGTK Context menu tests</body></html>", "file:///");
    test->waitUntilLoadFinished();

    // Create a custom menu item.
    GRefPtr<GtkAction> action = adoptGRef(gtk_action_new("WebKitGTKCustomAction", "Custom _Action", nullptr, nullptr));
    test->setAction(action.get());
    test->showContextMenuAndWaitUntilFinished();
    test->activateCustomMenuItemAndWaitUntilActivated(gtk_action_get_label(action.get()));
    g_assert_true(test->m_activated);
    g_assert_false(test->m_toggled);

    // Create a custom toggle menu item.
    GRefPtr<GtkAction> toggleAction = adoptGRef(GTK_ACTION(gtk_toggle_action_new("WebKitGTKCustomToggleAction", "Custom _Toggle Action", nullptr, nullptr)));
    test->setAction(toggleAction.get());
    test->showContextMenuAndWaitUntilFinished();
    test->toggleCustomMenuItemAndWaitUntilToggled(gtk_action_get_label(toggleAction.get()));
    g_assert_false(test->m_activated);
    g_assert_true(test->m_toggled);

    // Create a custom menu item using GAction.
    GRefPtr<GAction> gAction = adoptGRef(G_ACTION(g_simple_action_new("WebKitGTKCustomGAction", nullptr)));
    test->setAction(gAction.get(), "Custom _GAction");
    test->showContextMenuAndWaitUntilFinished();
    test->activateCustomMenuItemAndWaitUntilActivated("Custom _GAction");
    g_assert_true(test->m_activated);
    g_assert_false(test->m_toggled);

    // Create a custom toggle menu item using GAction.
    GRefPtr<GAction> toggleGAction = adoptGRef(G_ACTION(g_simple_action_new_stateful("WebKitGTKCustomToggleGAction", nullptr, g_variant_new_boolean(FALSE))));
    test->setAction(toggleGAction.get(), "Custom _Toggle GAction");
    test->showContextMenuAndWaitUntilFinished();
    test->toggleCustomMenuItemAndWaitUntilToggled("Custom _Toggle GAction");
    g_assert_false(test->m_activated);
    g_assert_true(test->m_toggled);

    // Create a custom menu item using GAction with a target.
    gAction = adoptGRef(G_ACTION(g_simple_action_new("WebKitGTKCustomGActionWithTarget", G_VARIANT_TYPE_STRING)));
    test->setAction(gAction.get(), "Custom _GAction With Target", g_variant_new_string("WebKitGTKCustomGActionTarget"));
    test->showContextMenuAndWaitUntilFinished();
    test->activateCustomMenuItemAndWaitUntilActivated("Custom _GAction With Target");
    g_assert_true(test->m_activated);
}

class ContextMenuCustomFullTest: public ContextMenuTest {
public:
    MAKE_GLIB_TEST_FIXTURE(ContextMenuCustomFullTest);

    bool contextMenu(WebKitContextMenu* contextMenu, GdkEvent*, WebKitHitTestResult*)
    {
        // Clear proposed menu and build our own.
        webkit_context_menu_remove_all(contextMenu);
        g_assert_cmpint(webkit_context_menu_get_n_items(contextMenu), ==, 0);

        // Add actions from stock.
        webkit_context_menu_prepend(contextMenu, webkit_context_menu_item_new_from_stock_action(WEBKIT_CONTEXT_MENU_ACTION_GO_BACK));
        webkit_context_menu_append(contextMenu, webkit_context_menu_item_new_from_stock_action(WEBKIT_CONTEXT_MENU_ACTION_GO_FORWARD));
        webkit_context_menu_insert(contextMenu, webkit_context_menu_item_new_separator(), 2);

        // Add custom actions.
        G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
        GRefPtr<GtkAction> action = adoptGRef(gtk_action_new("WebKitGTKCustomAction", "Custom _Action", nullptr, nullptr));
        gtk_action_set_sensitive(action.get(), FALSE);
        webkit_context_menu_insert(contextMenu, webkit_context_menu_item_new(action.get()), -1);
        GRefPtr<GtkAction> toggleAction = adoptGRef(GTK_ACTION(gtk_toggle_action_new("WebKitGTKCustomToggleAction", "Custom _Toggle Action", nullptr, nullptr)));
        gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(toggleAction.get()), TRUE);
        webkit_context_menu_append(contextMenu, webkit_context_menu_item_new(toggleAction.get()));
        webkit_context_menu_append(contextMenu, webkit_context_menu_item_new_separator());
        GRefPtr<GAction> gAction = adoptGRef(G_ACTION(g_simple_action_new("WebKitGTKCustomGAction", nullptr)));
        g_simple_action_set_enabled(G_SIMPLE_ACTION(gAction.get()), FALSE);
        webkit_context_menu_insert(contextMenu, webkit_context_menu_item_new_from_gaction(gAction.get(), "Custom _GAction", nullptr), -1);
        GRefPtr<GAction> toggleGAction = adoptGRef(G_ACTION(g_simple_action_new_stateful("WebKitGTKCustomToggleGAction", nullptr, g_variant_new_boolean(TRUE))));
        webkit_context_menu_append(contextMenu, webkit_context_menu_item_new_from_gaction(toggleGAction.get(), "Custom T_oggle GAction", nullptr));
        webkit_context_menu_append(contextMenu, webkit_context_menu_item_new_separator());
        G_GNUC_END_IGNORE_DEPRECATIONS;

        // Add a submenu.
        GRefPtr<WebKitContextMenu> subMenu = adoptGRef(webkit_context_menu_new());
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(subMenu.get()));
        webkit_context_menu_insert(subMenu.get(), webkit_context_menu_item_new_from_stock_action_with_label(WEBKIT_CONTEXT_MENU_ACTION_STOP, "Stop Load"), 0);
        webkit_context_menu_insert(subMenu.get(), webkit_context_menu_item_new_from_stock_action(WEBKIT_CONTEXT_MENU_ACTION_RELOAD), -1);
        webkit_context_menu_append(contextMenu, webkit_context_menu_item_new_with_submenu("Load options", subMenu.get()));
        webkit_context_menu_append(contextMenu, webkit_context_menu_item_new_separator());

        // Move Load submenu before custom actions.
        webkit_context_menu_move_item(contextMenu, webkit_context_menu_last(contextMenu), 3);
        webkit_context_menu_move_item(contextMenu, webkit_context_menu_last(contextMenu), 3);

        // If last item is a separator, remove it.
        if (webkit_context_menu_item_is_separator(webkit_context_menu_last(contextMenu)))
            webkit_context_menu_remove(contextMenu, webkit_context_menu_last(contextMenu));

        // Check the menu.
        GList* iter = webkit_context_menu_get_items(contextMenu);

        iter = checkCurrentItemIsStockActionAndGetNext(iter, WEBKIT_CONTEXT_MENU_ACTION_GO_BACK, Visible | Enabled);
        iter = checkCurrentItemIsStockActionAndGetNext(iter, WEBKIT_CONTEXT_MENU_ACTION_GO_FORWARD, Visible | Enabled);
        iter = checkCurrentItemIsSeparatorAndGetNext(iter);

        GList* subMenuIter = 0;
        iter = checkCurrentItemIsSubMenuAndGetNext(iter, "Load options", Visible | Enabled, &subMenuIter);
        subMenuIter = checkCurrentItemIsStockActionAndGetNext(subMenuIter, WEBKIT_CONTEXT_MENU_ACTION_STOP, Visible | Enabled);
        subMenuIter = checkCurrentItemIsStockActionAndGetNext(subMenuIter, WEBKIT_CONTEXT_MENU_ACTION_RELOAD, Visible | Enabled);
        iter = checkCurrentItemIsSeparatorAndGetNext(iter);

        iter = checkCurrentItemIsCustomActionAndGetNext(iter, "Custom _Action", Visible);
        iter = checkCurrentItemIsCustomActionAndGetNext(iter, "Custom _Toggle Action", Visible | Enabled | Checked);
        iter = checkCurrentItemIsSeparatorAndGetNext(iter);
        iter = checkCurrentItemIsCustomActionAndGetNext(iter, "Custom _GAction", Visible);
        iter = checkCurrentItemIsCustomActionAndGetNext(iter, "Custom T_oggle GAction", Visible | Enabled | Checked);
        g_assert_null(iter);

        quitMainLoop();

        return true;
    }
};

static void testContextMenuCustomMenu(ContextMenuCustomFullTest* test, gconstpointer)
{
    test->showInWindowAndWaitUntilMapped();

    test->loadHtml("<html><body>WebKitGTK Context menu tests</body></html>", "file:///");
    test->waitUntilLoadFinished();

    test->showContextMenuAndWaitUntilFinished();
}

class ContextMenuDisabledTest: public ContextMenuTest {
public:
    MAKE_GLIB_TEST_FIXTURE(ContextMenuDisabledTest);

    enum DisableMode {
        IgnoreClicks,
        IgnoreDefaultMenu
    };

    static gboolean buttonPressEventCallback(GtkWidget*, GdkEvent* event, ContextMenuDisabledTest* test)
    {
        if (event->button.button != 3)
            return FALSE;
        return test->rightButtonPressed();
    }

    ContextMenuDisabledTest()
        : m_disableMode(IgnoreClicks)
    {
        g_signal_connect(m_webView, "button-press-event", G_CALLBACK(buttonPressEventCallback), this);
    }

    bool contextMenu(WebKitContextMenu* contextMenu, GdkEvent*, WebKitHitTestResult*)
    {
        if (m_disableMode == IgnoreClicks)
            g_assert_not_reached();
        else
            quitMainLoop();

        return true;
    }

    bool rightButtonPressed()
    {
        if (m_disableMode == IgnoreClicks) {
            quitMainLoopAfterProcessingPendingEvents();
            return true;
        }
        return false;
    }

    DisableMode m_disableMode;
};

static void testContextMenuDisableMenu(ContextMenuDisabledTest* test, gconstpointer)
{
    test->showInWindowAndWaitUntilMapped();

    test->loadHtml("<html><body>WebKitGTK Context menu tests</body></html>", "file:///");
    test->waitUntilLoadFinished();

    test->m_disableMode = ContextMenuDisabledTest::IgnoreDefaultMenu;
    test->showContextMenuAndWaitUntilFinished();

    test->m_disableMode = ContextMenuDisabledTest::IgnoreClicks;
    test->showContextMenuAndWaitUntilFinished();
}

class ContextMenuSubmenuTest: public ContextMenuTest {
public:
    MAKE_GLIB_TEST_FIXTURE(ContextMenuSubmenuTest);

    bool contextMenu(WebKitContextMenu* contextMenu, GdkEvent*, WebKitHitTestResult*)
    {
        size_t menuSize = webkit_context_menu_get_n_items(contextMenu);
        GRefPtr<WebKitContextMenu> subMenu = adoptGRef(webkit_context_menu_new());
        webkit_context_menu_append(contextMenu, webkit_context_menu_item_new_with_submenu("SubMenuItem", subMenu.get()));
        g_assert_cmpuint(webkit_context_menu_get_n_items(contextMenu), ==, menuSize + 1);

        GRefPtr<WebKitContextMenu> subMenu2 = adoptGRef(webkit_context_menu_new());
        GRefPtr<WebKitContextMenuItem> item = webkit_context_menu_item_new_from_stock_action(WEBKIT_CONTEXT_MENU_ACTION_OPEN_LINK);

        // Add submenu to newly created item.
        g_assert_null(webkit_context_menu_item_get_submenu(item.get()));
        webkit_context_menu_item_set_submenu(item.get(), subMenu2.get());
        g_assert_true(webkit_context_menu_item_get_submenu(item.get()) == subMenu2.get());

        // Replace the submenu.
        webkit_context_menu_item_set_submenu(item.get(), 0);
        g_assert_null(webkit_context_menu_item_get_submenu(item.get()));

        // Try to add a submenu already added to another item.
        removeLogFatalFlag(G_LOG_LEVEL_WARNING);
        webkit_context_menu_item_set_submenu(item.get(), subMenu.get());
        addLogFatalFlag(G_LOG_LEVEL_WARNING);
        g_assert_null(webkit_context_menu_item_get_submenu(item.get()));

        // A removed submenu shouldn't have a parent.
        webkit_context_menu_item_set_submenu(item.get(), subMenu2.get());
        g_assert_true(webkit_context_menu_item_get_submenu(item.get()) == subMenu2.get());

        quitMainLoop();

        return true;
    }
};

static void testContextMenuSubMenu(ContextMenuSubmenuTest* test, gconstpointer)
{
    test->showInWindowAndWaitUntilMapped();

    test->loadHtml("<html><body>WebKitGTK Context menu tests</body></html>", "file:///");
    test->waitUntilLoadFinished();

    test->showContextMenuAndWaitUntilFinished();
}

class ContextMenuDismissedTest: public ContextMenuTest {
public:
    MAKE_GLIB_TEST_FIXTURE(ContextMenuDismissedTest);

    ContextMenuDismissedTest()
        : m_dismissed(false)
    {
    }

    bool contextMenu(WebKitContextMenu* contextMenu, GdkEvent*, WebKitHitTestResult*)
    {
        quitMainLoop();
        // Show the default context menu.
        return false;
    }

    void contextMenuDismissed()
    {
        m_dismissed = true;
        ContextMenuTest::contextMenuDismissed();
    }

    void showContextMenuAndWaitUntilDismissed()
    {
        showContextMenuAndWaitUntilFinished();
        dismissContextMenuAndWaitUntilFinished();
    }

    bool m_dismissed;
};

static void testContextMenuDismissed(ContextMenuDismissedTest* test, gconstpointer)
{
    test->showInWindowAndWaitUntilMapped();

    test->loadHtml("<html><body>WebKitGTK Context menu tests</body></html>", "file:///");
    test->waitUntilLoadFinished();

    test->showContextMenuAndWaitUntilDismissed();
    g_assert_true(test->m_dismissed);
}

class ContextMenuWebExtensionTest: public ContextMenuTest {
public:
    MAKE_GLIB_TEST_FIXTURE(ContextMenuWebExtensionTest);

    void deserializeContextMenuFromUserData(GVariant* userData)
    {
        m_actions.clear();
        if (!userData)
            return;

        GVariantIter iter;
        g_variant_iter_init(&iter, userData);
        m_actions.reserveInitialCapacity(g_variant_iter_n_children(&iter));

        uint32_t item;
        while (g_variant_iter_next(&iter, "u", &item))
            m_actions.uncheckedAppend(static_cast<WebKitContextMenuAction>(item));
    }

    bool contextMenu(WebKitContextMenu* menu, GdkEvent*, WebKitHitTestResult*)
    {
        deserializeContextMenuFromUserData(webkit_context_menu_get_user_data(menu));
        GList* items = webkit_context_menu_get_items(menu);
        g_assert_cmpuint(g_list_length(items), ==, m_actions.size());

        unsigned actionIndex = 0;
        for (GList* it = items; it; it = g_list_next(it)) {
            WebKitContextMenuItem* item = WEBKIT_CONTEXT_MENU_ITEM(it->data);
            g_assert_cmpuint(webkit_context_menu_item_get_stock_action(item), ==, m_actions[actionIndex++]);
        }

        quitMainLoop();

        return true;
    }

    Vector<WebKitContextMenuAction> m_actions;
};

static void testContextMenuWebExtensionMenu(ContextMenuWebExtensionTest* test, gconstpointer)
{
    test->showInWindowAndWaitUntilMapped();
    test->loadHtml("<html><body>WebKitGTK Context menu tests<br>"
        "<a style='position:absolute; left:1; top:10' href='http://www.webkitgtk.org'>WebKitGTK Website</a></body></html>",
        "ContextMenuTestDefault");
    test->waitUntilLoadFinished();

    // Default context menu.
    test->showContextMenuAtPositionAndWaitUntilFinished(1, 1);
    g_assert_cmpuint(test->m_actions.size(), ==, 4);
    g_assert_cmpuint(test->m_actions[0], ==, WEBKIT_CONTEXT_MENU_ACTION_GO_BACK);
    g_assert_cmpuint(test->m_actions[1], ==, WEBKIT_CONTEXT_MENU_ACTION_GO_FORWARD);
    g_assert_cmpuint(test->m_actions[2], ==, WEBKIT_CONTEXT_MENU_ACTION_STOP);
    g_assert_cmpuint(test->m_actions[3], ==, WEBKIT_CONTEXT_MENU_ACTION_RELOAD);

    // Link menu.
    test->showContextMenuAtPositionAndWaitUntilFinished(1, 11);
    g_assert_cmpuint(test->m_actions.size(), ==, 4);
    g_assert_cmpuint(test->m_actions[0], ==, WEBKIT_CONTEXT_MENU_ACTION_OPEN_LINK);
    g_assert_cmpuint(test->m_actions[1], ==, WEBKIT_CONTEXT_MENU_ACTION_OPEN_LINK_IN_NEW_WINDOW);
    g_assert_cmpuint(test->m_actions[2], ==, WEBKIT_CONTEXT_MENU_ACTION_DOWNLOAD_LINK_TO_DISK);
    g_assert_cmpuint(test->m_actions[3], ==, WEBKIT_CONTEXT_MENU_ACTION_COPY_LINK_TO_CLIPBOARD);

    // Custom menu.
    test->loadHtml("<html><body></body></html>", "ContextMenuTestCustom");
    test->waitUntilLoadFinished();
    test->showContextMenuAndWaitUntilFinished();
    g_assert_cmpuint(test->m_actions.size(), ==, 4);
    g_assert_cmpuint(test->m_actions[0], ==, WEBKIT_CONTEXT_MENU_ACTION_STOP);
    g_assert_cmpuint(test->m_actions[1], ==, WEBKIT_CONTEXT_MENU_ACTION_RELOAD);
    g_assert_cmpuint(test->m_actions[2], ==, WEBKIT_CONTEXT_MENU_ACTION_NO_ACTION);
    g_assert_cmpuint(test->m_actions[3], ==, WEBKIT_CONTEXT_MENU_ACTION_INSPECT_ELEMENT);

    // Menu cleared by the web process.
    test->loadHtml("<html><body></body></html>", "ContextMenuTestClear");
    test->waitUntilLoadFinished();
    test->showContextMenuAndWaitUntilFinished();
    g_assert_cmpuint(test->m_actions.size(), ==, 0);
}

class ContextMenuWebExtensionNodeTest: public ContextMenuTest {
public:
    MAKE_GLIB_TEST_FIXTURE(ContextMenuWebExtensionNodeTest);

    struct Node {
        enum {
            NodeUnknown = 0,
            NodeElement = 1,
            NodeText = 3
        };
        typedef unsigned Type;

        CString name;
        Type type;
        CString contents;
        CString parentName;
    };

    void deserializeNodeFromUserData(GVariant* userData)
    {
        GVariantIter iter;
        g_variant_iter_init(&iter, userData);

        const char* key;
        GVariant* value;
        while (g_variant_iter_next(&iter, "{&sv}", &key, &value)) {
            if (!strcmp(key, "Name") && g_variant_classify(value) == G_VARIANT_CLASS_STRING)
                m_node.name = g_variant_get_string(value, nullptr);
            else if (!strcmp(key, "Type") && g_variant_classify(value) == G_VARIANT_CLASS_UINT32)
                m_node.type = g_variant_get_uint32(value);
            else if (!strcmp(key, "Contents") && g_variant_classify(value) == G_VARIANT_CLASS_STRING)
                m_node.contents = g_variant_get_string(value, nullptr);
            else if (!strcmp(key, "Parent") && g_variant_classify(value) == G_VARIANT_CLASS_STRING)
                m_node.parentName = g_variant_get_string(value, nullptr);
            g_variant_unref(value);
        }
    }

    bool contextMenu(WebKitContextMenu* menu, GdkEvent*, WebKitHitTestResult*)
    {
        deserializeNodeFromUserData(webkit_context_menu_get_user_data(menu));
        quitMainLoop();

        return true;
    }

    Node m_node;
};

static void testContextMenuWebExtensionNode(ContextMenuWebExtensionNodeTest* test, gconstpointer)
{
    test->showInWindowAndWaitUntilMapped();
    test->loadHtml("<html><body><p style='position:absolute; left:1; top:1'>WebKitGTK Context menu tests</p><br>"
        "<a style='position:absolute; left:1; top:100' href='http://www.webkitgtk.org'>WebKitGTK Website</a></body></html>",
        "ContextMenuTestNode");
    test->waitUntilLoadFinished();

    test->showContextMenuAtPositionAndWaitUntilFinished(0, 0);
    g_assert_cmpstr(test->m_node.name.data(), ==, "HTML");
    g_assert_cmpuint(test->m_node.type, ==, ContextMenuWebExtensionNodeTest::Node::NodeElement);
    g_assert_cmpstr(test->m_node.contents.data(), ==, "WebKitGTK Context menu testsWebKitGTK Website");
    g_assert_cmpstr(test->m_node.parentName.data(), ==, "#document");

    test->showContextMenuAtPositionAndWaitUntilFinished(1, 20);
    g_assert_cmpstr(test->m_node.name.data(), ==, "#text");
    g_assert_cmpuint(test->m_node.type, ==, ContextMenuWebExtensionNodeTest::Node::NodeText);
    g_assert_cmpstr(test->m_node.contents.data(), ==, "WebKitGTK Context menu tests");
    g_assert_cmpstr(test->m_node.parentName.data(), ==, "P");

    // Link menu.
    test->showContextMenuAtPositionAndWaitUntilFinished(1, 101);
    g_assert_cmpstr(test->m_node.name.data(), ==, "#text");
    g_assert_cmpuint(test->m_node.type, ==, ContextMenuWebExtensionNodeTest::Node::NodeText);
    g_assert_cmpstr(test->m_node.contents.data(), ==, "WebKitGTK Website");
    g_assert_cmpstr(test->m_node.parentName.data(), ==, "A");
}

static void writeNextChunk(SoupMessage* message)
{
    GUniquePtr<char> filePath(g_build_filename(Test::getResourcesDir().data(), "silence.webm", nullptr));
    char* contents;
    gsize contentsLength;
    if (!g_file_get_contents(filePath.get(), &contents, &contentsLength, nullptr)) {
        soup_message_set_status(message, SOUP_STATUS_NOT_FOUND);
        soup_message_body_complete(message->response_body);
        return;
    }

    soup_message_body_append(message->response_body, SOUP_MEMORY_TAKE, contents, contentsLength);
    soup_message_body_complete(message->response_body);
}

static void serverCallback(SoupServer* server, SoupMessage* message, const char* path, GHashTable*, SoupClientContext*, gpointer)
{
    if (message->method != SOUP_METHOD_GET) {
        soup_message_set_status(message, SOUP_STATUS_NOT_IMPLEMENTED);
        return;
    }

    soup_message_set_status(message, SOUP_STATUS_OK);

    if (g_str_equal(path, "/live-stream")) {
        static const char* html =
            "<html><body>"
            " <video style='position:absolute; left:1; top:1' width='300' height='300'>"
            "  <source src='/live-stream.webm' type='video/webm' />"
            " </video>"
            "</body></html>";
        soup_message_body_append(message->response_body, SOUP_MEMORY_STATIC, html, strlen(html));
    } else if (g_str_equal(path, "/live-stream.webm")) {
        soup_message_headers_set_encoding(message->response_headers, SOUP_ENCODING_CHUNKED);
        g_signal_connect(message, "wrote_headers", G_CALLBACK(writeNextChunk), nullptr);
        g_signal_connect(message, "wrote_chunk", G_CALLBACK(writeNextChunk), nullptr);
        return;
    }

    soup_message_body_complete(message->response_body);
}

static void testContextMenuLiveStream(ContextMenuDefaultTest* test, gconstpointer)
{
    test->showInWindowAndWaitUntilMapped();

    test->loadURI(kServer->getURIForPath("/live-stream").data());
    test->waitUntilLoadFinished();

    test->m_expectedMenuType = ContextMenuDefaultTest::VideoLive;
    test->showContextMenuAtPositionAndWaitUntilFinished(1, 1);
}

void beforeAll()
{
    kServer = new WebKitTestServer();
    kServer->run(serverCallback);

    ContextMenuDefaultTest::add("WebKitWebView", "default-menu", testContextMenuDefaultMenu);
    ContextMenuDefaultTest::add("WebKitWebView", "context-menu-key", testContextMenuKey);
    ContextMenuDefaultTest::add("WebKitWebView", "popup-event-signal", testPopupEventSignal);
    ContextMenuDefaultTest::add("WebKitWebView", "live-stream", testContextMenuLiveStream);
    ContextMenuCustomTest::add("WebKitWebView", "populate-menu", testContextMenuPopulateMenu);
    ContextMenuCustomFullTest::add("WebKitWebView", "custom-menu", testContextMenuCustomMenu);
    ContextMenuDisabledTest::add("WebKitWebView", "disable-menu", testContextMenuDisableMenu);
    ContextMenuSubmenuTest::add("WebKitWebView", "submenu", testContextMenuSubMenu);
    ContextMenuDismissedTest::add("WebKitWebView", "menu-dismissed", testContextMenuDismissed);
    ContextMenuWebExtensionTest::add("WebKitWebPage", "context-menu", testContextMenuWebExtensionMenu);
    ContextMenuWebExtensionNodeTest::add("WebKitWebPage", "context-menu-node", testContextMenuWebExtensionNode);
}

void afterAll()
{
    delete kServer;
}
