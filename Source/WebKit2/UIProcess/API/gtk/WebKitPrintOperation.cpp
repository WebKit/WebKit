/*
 * Copyright (C) 2012 Igalia S.L.
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
#include "WebKitPrintOperation.h"

#include "PrintInfo.h"
#include "WebKitPrintOperationPrivate.h"
#include "WebKitPrivate.h"
#include "WebKitWebViewBasePrivate.h"
#include "WebPageProxy.h"
#include <WebCore/GtkUtilities.h>
#include <WebCore/NotImplemented.h>
#include <glib/gi18n-lib.h>
#include <wtf/gobject/GRefPtr.h>

#ifdef HAVE_GTK_UNIX_PRINTING
#include <gtk/gtkunixprint.h>
#endif

using namespace WebKit;

enum {
    PROP_0,

    PROP_WEB_VIEW,
    PROP_PRINT_SETTINGS,
    PROP_PAGE_SETUP
};

enum {
    DONE,

    LAST_SIGNAL
};

struct _WebKitPrintOperationPrivate {
    WebKitWebView* webView;
    gulong webViewDestroyedId;

    GRefPtr<GtkPrintSettings> printSettings;
    GRefPtr<GtkPageSetup> pageSetup;
};

static guint signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE(WebKitPrintOperation, webkit_print_operation, G_TYPE_OBJECT)

static void webkitPrintOperationFinalize(GObject* object)
{
    WebKitPrintOperationPrivate* priv = WEBKIT_PRINT_OPERATION(object)->priv;
    g_signal_handler_disconnect(priv->webView, priv->webViewDestroyedId);

    priv->~WebKitPrintOperationPrivate();
    G_OBJECT_CLASS(webkit_print_operation_parent_class)->finalize(object);
}

static void webViewDestroyed(GtkWidget* webView, GObject* printOperation)
{
    g_object_unref(printOperation);
}

static void webkitPrintOperationConstructed(GObject* object)
{
    WebKitPrintOperation* printOperation = WEBKIT_PRINT_OPERATION(object);
    WebKitPrintOperationPrivate* priv = printOperation->priv;

    if (G_OBJECT_CLASS(webkit_print_operation_parent_class)->constructed)
        G_OBJECT_CLASS(webkit_print_operation_parent_class)->constructed(object);

    priv->webViewDestroyedId = g_signal_connect(priv->webView, "destroy", G_CALLBACK(webViewDestroyed), printOperation);
}

static void webkitPrintOperationGetProperty(GObject* object, guint propId, GValue* value, GParamSpec* paramSpec)
{
    WebKitPrintOperation* printOperation = WEBKIT_PRINT_OPERATION(object);

    switch (propId) {
    case PROP_WEB_VIEW:
        g_value_take_object(value, printOperation->priv->webView);
        break;
    case PROP_PRINT_SETTINGS:
        g_value_set_object(value, printOperation->priv->printSettings.get());
        break;
    case PROP_PAGE_SETUP:
        g_value_set_object(value, printOperation->priv->pageSetup.get());
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void webkitPrintOperationSetProperty(GObject* object, guint propId, const GValue* value, GParamSpec* paramSpec)
{
    WebKitPrintOperation* printOperation = WEBKIT_PRINT_OPERATION(object);

    switch (propId) {
    case PROP_WEB_VIEW:
        printOperation->priv->webView = WEBKIT_WEB_VIEW(g_value_get_object(value));
        break;
    case PROP_PRINT_SETTINGS:
        webkit_print_operation_set_print_settings(printOperation, GTK_PRINT_SETTINGS(g_value_get_object(value)));
        break;
    case PROP_PAGE_SETUP:
        webkit_print_operation_set_page_setup(printOperation, GTK_PAGE_SETUP(g_value_get_object(value)));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void webkit_print_operation_init(WebKitPrintOperation* printOperation)
{
    WebKitPrintOperationPrivate* priv = G_TYPE_INSTANCE_GET_PRIVATE(printOperation, WEBKIT_TYPE_PRINT_OPERATION, WebKitPrintOperationPrivate);
    printOperation->priv = priv;
    new (priv) WebKitPrintOperationPrivate();
}

static void webkit_print_operation_class_init(WebKitPrintOperationClass* printOperationClass)
{
    GObjectClass* gObjectClass = G_OBJECT_CLASS(printOperationClass);
    gObjectClass->finalize = webkitPrintOperationFinalize;
    gObjectClass->constructed = webkitPrintOperationConstructed;
    gObjectClass->get_property = webkitPrintOperationGetProperty;
    gObjectClass->set_property = webkitPrintOperationSetProperty;

    /**
     * WebKitPrintOperation:web-view:
     *
     * The #WebKitWebView that will be printed.
     */
    g_object_class_install_property(gObjectClass,
                                    PROP_WEB_VIEW,
                                    g_param_spec_object("web-view",
                                                        _("Web View"),
                                                        _("The web view that will be printed"),
                                                        WEBKIT_TYPE_WEB_VIEW,
                                                        static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)));

    /**
     * WebKitPrintOperation:print-settings:
     *
     * The initial #GtkPrintSettings for the print operation.
     */
    g_object_class_install_property(gObjectClass,
                                    PROP_PRINT_SETTINGS,
                                    g_param_spec_object("print-settings",
                                                        _("Print Settings"),
                                                        _("The initial print settings for the print operation"),
                                                        GTK_TYPE_PRINT_SETTINGS,
                                                        WEBKIT_PARAM_READWRITE));
    /**
     * WebKitPrintOperation:page-setup:
     *
     * The initial #GtkPageSetup for the print operation.
     */
    g_object_class_install_property(gObjectClass,
                                     PROP_PAGE_SETUP,
                                     g_param_spec_object("page-setup",
                                                         _("Page Setup"),
                                                         _("The initial page setup for the print operation"),
                                                         GTK_TYPE_PAGE_SETUP,
                                                         WEBKIT_PARAM_READWRITE));

    /**
     * WebKitPrintOperation::done:
     * @print_operation: the #WebKitPrintOperation on which the signal was emitted
     *
     * Emitted when the print operation has finished doing everything
     * required for printing.
     */
    signals[DONE] =
        g_signal_new("done",
                     G_TYPE_FROM_CLASS(gObjectClass),
                     G_SIGNAL_RUN_LAST,
                     0, 0, 0,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);

    g_type_class_add_private(printOperationClass, sizeof(WebKitPrintOperationPrivate));
}

#ifdef HAVE_GTK_UNIX_PRINTING
static bool webkitPrintOperationRunDialogUnix(WebKitPrintOperation* printOperation, GtkWindow* parent)
{
    GtkPrintUnixDialog* printDialog = GTK_PRINT_UNIX_DIALOG(gtk_print_unix_dialog_new(0, parent));
    gtk_print_unix_dialog_set_manual_capabilities(printDialog, static_cast<GtkPrintCapabilities>(0));

    WebKitPrintOperationPrivate* priv = printOperation->priv;
    if (priv->printSettings)
        gtk_print_unix_dialog_set_settings(printDialog, priv->printSettings.get());

    if (priv->pageSetup)
        gtk_print_unix_dialog_set_page_setup(printDialog, priv->pageSetup.get());

    gtk_print_unix_dialog_set_embed_page_setup(printDialog, TRUE);

    bool returnValue = false;
    if (gtk_dialog_run(GTK_DIALOG(printDialog)) == GTK_RESPONSE_OK) {
        priv->printSettings = adoptGRef(gtk_print_unix_dialog_get_settings(printDialog));
        priv->pageSetup = gtk_print_unix_dialog_get_page_setup(printDialog);
        returnValue = true;
    }

    gtk_widget_destroy(GTK_WIDGET(printDialog));

    return returnValue;
}
#endif // HAVE_GTK_UNIX_PRINTING

#ifdef G_OS_WIN32
static bool webkitPrintOperationRunDialogWin32(WebKitPrintOperation*, GtkWindow*)
{
    notImplemented();
    return false;
}
#endif

static void drawPagesForPrintingCompleted(WKErrorRef, void* context)
{
    GRefPtr<WebKitPrintOperation> printOperation = adoptGRef(WEBKIT_PRINT_OPERATION(context));
    WebPageProxy* page = webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(printOperation->priv->webView));
    page->endPrinting();
    g_signal_emit(printOperation.get(), signals[DONE], 0, NULL);
}

void webkitPrintOperationRunDialogForFrame(WebKitPrintOperation* printOperation, GtkWindow* parent, WebFrameProxy* webFrame)
{
    WebKitPrintOperationPrivate* priv = printOperation->priv;
    if (!parent) {
        GtkWidget* toplevel = gtk_widget_get_toplevel(GTK_WIDGET(priv->webView));
        if (WebCore::widgetIsOnscreenToplevelWindow(toplevel))
            parent = GTK_WINDOW(toplevel);
    }
#ifdef HAVE_GTK_UNIX_PRINTING
    bool shouldPrint = webkitPrintOperationRunDialogUnix(printOperation, parent);
#endif
#ifdef G_OS_WIN32
    bool shouldPrint = webkitPrintOperationRunDialogWin32(printOperation, parent);
#endif

    if (!shouldPrint) {
        g_signal_emit(printOperation, signals[DONE], 0, NULL);
        return;
    }

    PrintInfo printInfo(priv->printSettings.get(), priv->pageSetup.get());
    WebPageProxy* page = webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(priv->webView));
    page->drawPagesForPrinting(webFrame, printInfo, VoidCallback::create(g_object_ref(printOperation), &drawPagesForPrintingCompleted));
}

/**
 * webkit_print_operation_new:
 * @web_view: a #WebKitWebView
 *
 * Create a new #WebKitPrintOperation to print @web_view contents.
 *
 * Returns: (transfer full): a new #WebKitPrintOperation.
 */
WebKitPrintOperation* webkit_print_operation_new(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), 0);

    return WEBKIT_PRINT_OPERATION(g_object_new(WEBKIT_TYPE_PRINT_OPERATION, "web-view", webView, NULL));
}

/**
 * webkit_print_operation_get_print_settings:
 * @print_operation: a #WebKitPrintOperation
 *
 * Return the current print settings of @print_operation. It returns %NULL until
 * either webkit_print_operation_set_print_settings() or webkit_print_operation_run_dialog()
 * have been called.
 *
 * Returns: (transfer none): the current #GtkPrintSettings of @print_operation.
 */
GtkPrintSettings* webkit_print_operation_get_print_settings(WebKitPrintOperation* printOperation)
{
    g_return_val_if_fail(WEBKIT_IS_PRINT_OPERATION(printOperation), 0);

    return printOperation->priv->printSettings.get();
}

/**
 * webkit_print_operation_set_print_settings:
 * @print_operation: a #WebKitPrintOperation
 * @print_settings: a #GtkPrintSettings to set
 *
 * Set the current print settings of @print_operation. Current print settings are used for
 * the initial values of the print dialog when webkit_print_operation_run_dialog() is called.
 */
void webkit_print_operation_set_print_settings(WebKitPrintOperation* printOperation, GtkPrintSettings* printSettings)
{
    g_return_if_fail(WEBKIT_IS_PRINT_OPERATION(printOperation));
    g_return_if_fail(GTK_IS_PRINT_SETTINGS(printSettings));

    if (printOperation->priv->printSettings.get() == printSettings)
        return;

    printOperation->priv->printSettings = printSettings;
    g_object_notify(G_OBJECT(printOperation), "print-settings");
}

/**
 * webkit_print_operation_get_page_setup:
 * @print_operation: a #WebKitPrintOperation
 *
 * Return the current page setup of @print_operation. It returns %NULL until
 * either webkit_print_operation_set_print_settings() or webkit_print_operation_run_dialog()
 * have been called.
 *
 * Returns: (transfer none): the current #GtkPageSetup of @print_operation.
 */
GtkPageSetup* webkit_print_operation_get_page_setup(WebKitPrintOperation* printOperation)
{
    g_return_val_if_fail(WEBKIT_IS_PRINT_OPERATION(printOperation), 0);

    return printOperation->priv->pageSetup.get();
}

/**
 * webkit_print_operation_set_page_setup:
 * @print_operation: a #WebKitPrintOperation
 * @page_setup: a #GtkPageSetup to set
 *
 * Set the current page setup of @print_operation. Current page setup is used for the
 * initial values of the print dialog when webkit_print_operation_run_dialog() is called.
 */
void webkit_print_operation_set_page_setup(WebKitPrintOperation* printOperation, GtkPageSetup* pageSetup)
{
    g_return_if_fail(WEBKIT_IS_PRINT_OPERATION(printOperation));
    g_return_if_fail(GTK_IS_PAGE_SETUP(pageSetup));

    if (printOperation->priv->pageSetup.get() == pageSetup)
        return;

    printOperation->priv->pageSetup = pageSetup;
    g_object_notify(G_OBJECT(printOperation), "page-setup");
}

/**
 * webkit_print_operation_run_dialog:
 * @print_operation: a #WebKitPrintOperation
 * @parent: (allow-none): transient parent of the print dialog
 *
 * Run the print dialog using and start printing using the options selected by
 * the user. This method returns when the print operation is closed.
 * The WebKitPrintOperation::done signal is emitted when the printing
 * operation finishes.
 * Current print settings and page setup of @print_operation are updated with
 * options selected by the user when Print button is pressed in print dialog.
 * You can get the updated print settings and page setup by calling
 * webkit_print_operation_get_print_settings() and webkit_print_operation_get_page_setup()
 * after this method.
 */
void webkit_print_operation_run_dialog(WebKitPrintOperation* printOperation, GtkWindow* parent)
{
    g_return_if_fail(WEBKIT_IS_PRINT_OPERATION(printOperation));

    WebPageProxy* page = webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(printOperation->priv->webView));
    webkitPrintOperationRunDialogForFrame(printOperation, parent, page->mainFrame());
}

