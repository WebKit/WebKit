/*
 *  Copyright (C) 2008 Nuanti Ltd.
 *  Copyright (C) 2009 Gustavo Noronha Silva <gns@gnome.org>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "ContextMenu.h"
#include "ContextMenuClientGtk.h"

#include "HitTestResult.h"
#include "KURL.h"
#include "NotImplemented.h"
#include <wtf/text/CString.h>

#include <glib/gi18n-lib.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include "webkitprivate.h"

using namespace WebCore;

namespace WebKit {

ContextMenuClient::ContextMenuClient(WebKitWebView *webView)
    : m_webView(webView)
{
}

void ContextMenuClient::contextMenuDestroyed()
{
    delete this;
}

static GtkWidget* inputMethodsMenuItem (WebKitWebView* webView)
{
    if (gtk_major_version > 2 || (gtk_major_version == 2 && gtk_minor_version >= 10)) {
        GtkSettings* settings = webView ? gtk_widget_get_settings(GTK_WIDGET(webView)) : gtk_settings_get_default();

        gboolean showMenu = TRUE;
        if (settings)
            g_object_get(settings, "gtk-show-input-method-menu", &showMenu, NULL);
        if (!showMenu)
            return 0;
    }

    GtkWidget* menuitem = gtk_image_menu_item_new_with_mnemonic(
        _("Input _Methods"));

    WebKitWebViewPrivate* priv = WEBKIT_WEB_VIEW_GET_PRIVATE(webView);
    GtkWidget* imContextMenu = gtk_menu_new();
    gtk_im_multicontext_append_menuitems(GTK_IM_MULTICONTEXT(priv->imContext), GTK_MENU_SHELL(imContextMenu));

    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), imContextMenu);

    return menuitem;
}

// Values taken from gtktextutil.c
typedef struct {
  const char *label;
  gunichar ch;
} GtkUnicodeMenuEntry;
static const GtkUnicodeMenuEntry bidi_menu_entries[] = {
  { N_("LRM _Left-to-right mark"), 0x200E },
  { N_("RLM _Right-to-left mark"), 0x200F },
  { N_("LRE Left-to-right _embedding"), 0x202A },
  { N_("RLE Right-to-left e_mbedding"), 0x202B },
  { N_("LRO Left-to-right _override"), 0x202D },
  { N_("RLO Right-to-left o_verride"), 0x202E },
  { N_("PDF _Pop directional formatting"), 0x202C },
  { N_("ZWS _Zero width space"), 0x200B },
  { N_("ZWJ Zero width _joiner"), 0x200D },
  { N_("ZWNJ Zero width _non-joiner"), 0x200C }
};

static void insertControlCharacter(GtkWidget* widget)
{
    // GtkUnicodeMenuEntry* entry = (GtkUnicodeMenuEntry*)g_object_get_data(G_OBJECT(widget), "gtk-unicode-menu-entry");
    notImplemented();
}

static GtkWidget* unicodeMenuItem(WebKitWebView* webView)
{
    if (gtk_major_version > 2 || (gtk_major_version == 2 && gtk_minor_version >= 10)) {
        GtkSettings* settings = webView ? gtk_widget_get_settings(GTK_WIDGET(webView)) : gtk_settings_get_default();

        gboolean showMenu = TRUE;
        if (settings)
            g_object_get(settings, "gtk-show-unicode-menu", &showMenu, NULL);
        if (!showMenu)
            return 0;
    }

    GtkWidget* menuitem = gtk_image_menu_item_new_with_mnemonic(
        _("_Insert Unicode Control Character"));

    GtkWidget* unicodeContextMenu = gtk_menu_new();
    unsigned i;
    for (i = 0; i < G_N_ELEMENTS(bidi_menu_entries); i++) {
        GtkWidget* menuitem = gtk_menu_item_new_with_mnemonic(_(bidi_menu_entries[i].label));
        g_object_set_data(G_OBJECT(menuitem), "gtk-unicode-menu-entry", (gpointer)&bidi_menu_entries[i]);
        g_signal_connect(menuitem, "activate", G_CALLBACK(insertControlCharacter), 0);
        gtk_widget_show(menuitem);
        gtk_menu_shell_append(GTK_MENU_SHELL(unicodeContextMenu), menuitem);
        // FIXME: Make the item sensitive as insertControlCharacter() is implemented
        gtk_widget_set_sensitive(menuitem, FALSE);
    }

    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), unicodeContextMenu);

    return menuitem;
}

PlatformMenuDescription ContextMenuClient::getCustomMenuFromDefaultItems(ContextMenu* menu)
{
    GtkMenu* gtkmenu = menu->releasePlatformDescription();

    HitTestResult result = menu->hitTestResult();
    WebKitWebView* webView = m_webView;

    if (result.isContentEditable()) {

        GtkWidget* imContextMenu = inputMethodsMenuItem(webView);
        GtkWidget* unicodeContextMenu = unicodeMenuItem(webView);

        if (imContextMenu || unicodeContextMenu) {
            GtkWidget* separator = gtk_separator_menu_item_new();
            gtk_menu_shell_append(GTK_MENU_SHELL(gtkmenu), separator);
            gtk_widget_show(separator);
        }

        if (imContextMenu) {
            gtk_menu_shell_append(GTK_MENU_SHELL(gtkmenu), imContextMenu);
            gtk_widget_show(imContextMenu);
        }

        if (unicodeContextMenu) {
            gtk_menu_shell_append(GTK_MENU_SHELL(gtkmenu), unicodeContextMenu);
            gtk_widget_show(unicodeContextMenu);
        }

    }

    return gtkmenu;
}

void ContextMenuClient::contextMenuItemSelected(ContextMenuItem*, const ContextMenu*)
{
    notImplemented();
}

void ContextMenuClient::downloadURL(const KURL& url)
{
    WebKitNetworkRequest* networkRequest = webkit_network_request_new(url.string().utf8().data());

    webkit_web_view_request_download(m_webView, networkRequest);
    g_object_unref(networkRequest);
}

void ContextMenuClient::copyImageToClipboard(const HitTestResult&)
{
    notImplemented();
}

void ContextMenuClient::searchWithGoogle(const Frame*)
{
    notImplemented();
}

void ContextMenuClient::lookUpInDictionary(Frame*)
{
    notImplemented();
}

void ContextMenuClient::speak(const String&)
{
    notImplemented();
}

void ContextMenuClient::stopSpeaking()
{
    notImplemented();
}

bool ContextMenuClient::isSpeaking()
{
    notImplemented();
    return false;
}

}

