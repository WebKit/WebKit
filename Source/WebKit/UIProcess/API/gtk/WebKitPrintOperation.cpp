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

#include "WebKitPrintCustomWidgetPrivate.h"
#include "WebKitPrintOperationPrivate.h"
#include "WebKitPrivate.h"
#include "WebKitWebViewPrivate.h"
#include "WebPageProxy.h"
#include <WebCore/GtkUtilities.h>
#include <WebCore/GtkVersioning.h>
#include <WebCore/NotImplemented.h>
#include <glib/gi18n-lib.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/text/CString.h>

#if HAVE(GTK_UNIX_PRINTING)
#include <gtk/gtkunixprint.h>
#endif

using namespace WebKit;

/**
 * SECTION: WebKitPrintOperation
 * @Short_description: Controls a print operation
 * @Title: WebKitPrintOperation
 *
 * A #WebKitPrintOperation controls a print operation in WebKit. With
 * a similar API to #GtkPrintOperation, it lets you set the print
 * settings with webkit_print_operation_set_print_settings() or
 * display the print dialog with webkit_print_operation_run_dialog().
 *
 */

enum {
    PROP_0,
    PROP_WEB_VIEW,
    PROP_PRINT_SETTINGS,
    PROP_PAGE_SETUP,
    N_PROPERTIES,
};

static GParamSpec* sObjProperties[N_PROPERTIES] = { nullptr, };

enum {
    FINISHED,
    FAILED,
    CREATE_CUSTOM_WIDGET,

    LAST_SIGNAL
};

struct _WebKitPrintOperationPrivate {
    ~_WebKitPrintOperationPrivate()
    {
        if (webView)
            g_object_remove_weak_pointer(G_OBJECT(webView), reinterpret_cast<void**>(&webView));
    }

    WebKitWebView* webView;
    PrintInfo::PrintMode printMode;

    GRefPtr<GtkPrintSettings> printSettings;
    GRefPtr<GtkPageSetup> pageSetup;
};

static guint signals[LAST_SIGNAL] = { 0, };

WEBKIT_DEFINE_TYPE(WebKitPrintOperation, webkit_print_operation, G_TYPE_OBJECT)

static void webkitPrintOperationConstructed(GObject* object)
{
    G_OBJECT_CLASS(webkit_print_operation_parent_class)->constructed(object);

    WebKitPrintOperationPrivate* priv = WEBKIT_PRINT_OPERATION(object)->priv;
    g_object_add_weak_pointer(G_OBJECT(priv->webView), reinterpret_cast<void**>(&priv->webView));
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

static gboolean webkitPrintOperationAccumulatorObjectHandled(GSignalInvocationHint*, GValue* returnValue, const GValue* handlerReturn, gpointer)
{
    void* object = g_value_get_object(handlerReturn);
    if (object)
        g_value_set_object(returnValue, object);

    return !object;
}

static void webkit_print_operation_class_init(WebKitPrintOperationClass* printOperationClass)
{
    GObjectClass* gObjectClass = G_OBJECT_CLASS(printOperationClass);
    gObjectClass->constructed = webkitPrintOperationConstructed;
    gObjectClass->get_property = webkitPrintOperationGetProperty;
    gObjectClass->set_property = webkitPrintOperationSetProperty;

    /**
     * WebKitPrintOperation:web-view:
     *
     * The #WebKitWebView that will be printed.
     */
    sObjProperties[PROP_WEB_VIEW] =
        g_param_spec_object(
            "web-view",
            _("Web View"),
            _("The web view that will be printed"),
            WEBKIT_TYPE_WEB_VIEW,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    /**
     * WebKitPrintOperation:print-settings:
     *
     * The initial #GtkPrintSettings for the print operation.
     */
    sObjProperties[PROP_PRINT_SETTINGS] =
        g_param_spec_object(
            "print-settings",
            _("Print Settings"),
            _("The initial print settings for the print operation"),
            GTK_TYPE_PRINT_SETTINGS,
            WEBKIT_PARAM_READWRITE);
    /**
     * WebKitPrintOperation:page-setup:
     *
     * The initial #GtkPageSetup for the print operation.
     */
    sObjProperties[PROP_PAGE_SETUP] =
        g_param_spec_object(
            "page-setup",
            _("Page Setup"),
            _("The initial page setup for the print operation"),
            GTK_TYPE_PAGE_SETUP,
            WEBKIT_PARAM_READWRITE);

    g_object_class_install_properties(gObjectClass, N_PROPERTIES, sObjProperties);

    /**
     * WebKitPrintOperation::finished:
     * @print_operation: the #WebKitPrintOperation on which the signal was emitted
     *
     * Emitted when the print operation has finished doing everything
     * required for printing.
     */
    signals[FINISHED] =
        g_signal_new("finished",
                     G_TYPE_FROM_CLASS(gObjectClass),
                     G_SIGNAL_RUN_LAST,
                     0, 0, 0,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);

    /**
     * WebKitPrintOperation::failed:
     * @print_operation: the #WebKitPrintOperation on which the signal was emitted
     * @error: the #GError that was triggered
     *
     * Emitted when an error occurs while printing. The given @error, of the domain
     * %WEBKIT_PRINT_ERROR, contains further details of the failure.
     * The #WebKitPrintOperation::finished signal is emitted after this one.
     */
    signals[FAILED] =
        g_signal_new(
            "failed",
            G_TYPE_FROM_CLASS(gObjectClass),
            G_SIGNAL_RUN_LAST,
            0, 0, 0,
            g_cclosure_marshal_VOID__BOXED,
            G_TYPE_NONE, 1,
            G_TYPE_ERROR | G_SIGNAL_TYPE_STATIC_SCOPE);

    /**
     * WebKitPrintOperation::create-custom-widget:
     * @print_operation: the #WebKitPrintOperation on which the signal was emitted
     *
     * Emitted when displaying the print dialog with webkit_print_operation_run_dialog().
     * The returned #WebKitPrintCustomWidget will be added to the print dialog and
     * it will be owned by the @print_operation. However, the object is guaranteed
     * to be alive until the #WebKitPrintCustomWidget::apply is emitted.
     *
     * Returns: (transfer full): A #WebKitPrintCustomWidget that will be embedded in the dialog.
     *
     * Since: 2.16
     */
    signals[CREATE_CUSTOM_WIDGET] =
        g_signal_new(
            "create-custom-widget",
            G_TYPE_FROM_CLASS(gObjectClass),
            G_SIGNAL_RUN_LAST,
            0,
            webkitPrintOperationAccumulatorObjectHandled, 0,
            g_cclosure_marshal_generic,
            WEBKIT_TYPE_PRINT_CUSTOM_WIDGET, 0);
}

#if HAVE(GTK_UNIX_PRINTING)
static void notifySelectedPrinterCallback(GtkPrintUnixDialog* dialog, GParamSpec*, WebKitPrintCustomWidget* printCustomWidget)
{
    webkitPrintCustomWidgetEmitUpdateCustomWidgetSignal(printCustomWidget, gtk_print_unix_dialog_get_page_setup(dialog), gtk_print_unix_dialog_get_settings(dialog));
}

static WebKitPrintOperationResponse webkitPrintOperationRunDialog(WebKitPrintOperation* printOperation, GtkWindow* parent)
{
    GtkPrintUnixDialog* printDialog = GTK_PRINT_UNIX_DIALOG(gtk_print_unix_dialog_new(0, parent));
    gtk_print_unix_dialog_set_manual_capabilities(printDialog, static_cast<GtkPrintCapabilities>(GTK_PRINT_CAPABILITY_NUMBER_UP
                                                                                                 | GTK_PRINT_CAPABILITY_NUMBER_UP_LAYOUT
                                                                                                 | GTK_PRINT_CAPABILITY_PAGE_SET
                                                                                                 | GTK_PRINT_CAPABILITY_REVERSE
                                                                                                 | GTK_PRINT_CAPABILITY_COPIES
                                                                                                 | GTK_PRINT_CAPABILITY_COLLATE
                                                                                                 | GTK_PRINT_CAPABILITY_SCALE));

    WebKitPrintOperationPrivate* priv = printOperation->priv;
    // Make sure the initial settings of the GtkPrintUnixDialog is a valid
    // GtkPrintSettings object to work around a crash happening in the GTK+
    // file print backend. https://bugzilla.gnome.org/show_bug.cgi?id=703784.
    if (!priv->printSettings)
        priv->printSettings = adoptGRef(gtk_print_settings_new());
    gtk_print_unix_dialog_set_settings(printDialog, priv->printSettings.get());

    if (priv->pageSetup)
        gtk_print_unix_dialog_set_page_setup(printDialog, priv->pageSetup.get());

    gtk_print_unix_dialog_set_embed_page_setup(printDialog, TRUE);

    GRefPtr<WebKitPrintCustomWidget> customWidget;
    g_signal_emit(printOperation, signals[CREATE_CUSTOM_WIDGET], 0, &customWidget.outPtr());
    if (customWidget) {
        const gchar* widgetTitle = webkit_print_custom_widget_get_title(customWidget.get());
        GtkWidget* widget = webkit_print_custom_widget_get_widget(customWidget.get());

        g_signal_connect(printDialog, "notify::selected-printer", G_CALLBACK(notifySelectedPrinterCallback), customWidget.get());
        gtk_print_unix_dialog_add_custom_tab(printDialog, widget, gtk_label_new(widgetTitle));
    }

    WebKitPrintOperationResponse returnValue = WEBKIT_PRINT_OPERATION_RESPONSE_CANCEL;
    if (gtk_dialog_run(GTK_DIALOG(printDialog)) == GTK_RESPONSE_OK) {
        priv->printSettings = adoptGRef(gtk_print_unix_dialog_get_settings(printDialog));
        priv->pageSetup = gtk_print_unix_dialog_get_page_setup(printDialog);
        returnValue = WEBKIT_PRINT_OPERATION_RESPONSE_PRINT;
        if (customWidget)
            webkitPrintCustomWidgetEmitCustomWidgetApplySignal(customWidget.get());
    }

    gtk_widget_destroy(GTK_WIDGET(printDialog));

    return returnValue;
}
#else
// TODO: We need to add an implementation for Windows.
static WebKitPrintOperationResponse webkitPrintOperationRunDialog(WebKitPrintOperation*, GtkWindow*)
{
    notImplemented();
    return WEBKIT_PRINT_OPERATION_RESPONSE_CANCEL;
}
#endif

static void drawPagesForPrintingCompleted(API::Error* wkPrintError, WebKitPrintOperation* printOperation)
{
    // When running synchronously WebPageProxy::printFrame() calls endPrinting().
    if (printOperation->priv->printMode == PrintInfo::PrintModeAsync && printOperation->priv->webView)
        webkitWebViewGetPage(printOperation->priv->webView).endPrinting();

    const WebCore::ResourceError& resourceError = wkPrintError ? wkPrintError->platformError() : WebCore::ResourceError();
    if (!resourceError.isNull()) {
        GUniquePtr<GError> printError(g_error_new_literal(g_quark_from_string(resourceError.domain().utf8().data()),
            toWebKitError(resourceError.errorCode()), resourceError.localizedDescription().utf8().data()));
        g_signal_emit(printOperation, signals[FAILED], 0, printError.get());
    }
    g_signal_emit(printOperation, signals[FINISHED], 0, NULL);
}

static void webkitPrintOperationPrintPagesForFrame(WebKitPrintOperation* printOperation, WebFrameProxy* webFrame, GtkPrintSettings* printSettings, GtkPageSetup* pageSetup)
{
    PrintInfo printInfo(printSettings, pageSetup, printOperation->priv->printMode);
    auto& page = webkitWebViewGetPage(printOperation->priv->webView);
    g_object_ref(printOperation);
    page.drawPagesForPrinting(webFrame, printInfo, [printOperation](API::Error* printError) {
        drawPagesForPrintingCompleted(printError, adoptGRef(printOperation).get());
    });
}

WebKitPrintOperationResponse webkitPrintOperationRunDialogForFrame(WebKitPrintOperation* printOperation, GtkWindow* parent, WebFrameProxy* webFrame)
{
    WebKitPrintOperationPrivate* priv = printOperation->priv;
    if (!parent) {
        GtkWidget* toplevel = gtk_widget_get_toplevel(GTK_WIDGET(priv->webView));
        if (WebCore::widgetIsOnscreenToplevelWindow(toplevel))
            parent = GTK_WINDOW(toplevel);
    }

    WebKitPrintOperationResponse response = webkitPrintOperationRunDialog(printOperation, parent);
    if (response == WEBKIT_PRINT_OPERATION_RESPONSE_CANCEL)
        return response;

    webkitPrintOperationPrintPagesForFrame(printOperation, webFrame, priv->printSettings.get(), priv->pageSetup.get());
    return response;
}

void webkitPrintOperationSetPrintMode(WebKitPrintOperation* printOperation, PrintInfo::PrintMode printMode)
{
    printOperation->priv->printMode = printMode;
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
    g_object_notify_by_pspec(G_OBJECT(printOperation), sObjProperties[PROP_PRINT_SETTINGS]);
}

/**
 * webkit_print_operation_get_page_setup:
 * @print_operation: a #WebKitPrintOperation
 *
 * Return the current page setup of @print_operation. It returns %NULL until
 * either webkit_print_operation_set_page_setup() or webkit_print_operation_run_dialog()
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
    g_object_notify_by_pspec(G_OBJECT(printOperation), sObjProperties[PROP_PAGE_SETUP]);
}

/**
 * webkit_print_operation_run_dialog:
 * @print_operation: a #WebKitPrintOperation
 * @parent: (allow-none): transient parent of the print dialog
 *
 * Run the print dialog and start printing using the options selected by
 * the user. This method returns when the print dialog is closed.
 * If the print dialog is cancelled %WEBKIT_PRINT_OPERATION_RESPONSE_CANCEL
 * is returned. If the user clicks on the print button, %WEBKIT_PRINT_OPERATION_RESPONSE_PRINT
 * is returned and the print operation starts. In this case, the #WebKitPrintOperation::finished
 * signal is emitted when the operation finishes. If an error occurs while printing, the signal
 * #WebKitPrintOperation::failed is emitted before #WebKitPrintOperation::finished.
 * If the print dialog is not cancelled current print settings and page setup of @print_operation
 * are updated with options selected by the user when Print button is pressed in print dialog.
 * You can get the updated print settings and page setup by calling
 * webkit_print_operation_get_print_settings() and webkit_print_operation_get_page_setup()
 * after this method.
 *
 * Returns: the #WebKitPrintOperationResponse of the print dialog
 */
WebKitPrintOperationResponse webkit_print_operation_run_dialog(WebKitPrintOperation* printOperation, GtkWindow* parent)
{
    g_return_val_if_fail(WEBKIT_IS_PRINT_OPERATION(printOperation), WEBKIT_PRINT_OPERATION_RESPONSE_CANCEL);

    auto& page = webkitWebViewGetPage(printOperation->priv->webView);
    return webkitPrintOperationRunDialogForFrame(printOperation, parent, page.mainFrame());
}

/**
 * webkit_print_operation_print:
 * @print_operation: a #WebKitPrintOperation
 *
 * Start a print operation using current print settings and page setup
 * without showing the print dialog. If either print settings or page setup
 * are not set with webkit_print_operation_set_print_settings() and
 * webkit_print_operation_set_page_setup(), the default options will be used
 * and the print job will be sent to the default printer.
 * The #WebKitPrintOperation::finished signal is emitted when the printing
 * operation finishes. If an error occurs while printing the signal
 * #WebKitPrintOperation::failed is emitted before #WebKitPrintOperation::finished.
 */
void webkit_print_operation_print(WebKitPrintOperation* printOperation)
{
    g_return_if_fail(WEBKIT_IS_PRINT_OPERATION(printOperation));

    WebKitPrintOperationPrivate* priv = printOperation->priv;
    GRefPtr<GtkPrintSettings> printSettings = priv->printSettings ? priv->printSettings : adoptGRef(gtk_print_settings_new());
    GRefPtr<GtkPageSetup> pageSetup = priv->pageSetup ? priv->pageSetup : adoptGRef(gtk_page_setup_new());

    auto& page = webkitWebViewGetPage(printOperation->priv->webView);
    webkitPrintOperationPrintPagesForFrame(printOperation, page.mainFrame(), printSettings.get(), pageSetup.get());
}
