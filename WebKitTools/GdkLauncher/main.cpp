#include "webkitgtkpage.h"
#include "webkitgtkglobal.h"

#include <gtk/gtk.h>

#include <string.h>

static GtkWidget* gURLBarEntry;
static GtkWidget* gTopLevelWindow;
static WebKitGtkPage* gPage;
static gchar* gTitle;
static gint gProgress;

static bool stringIsEqual(const char* str1, const char* str2)
{
    return 0 == strcmp(str1, str2);
}

static gchar* autocorrectURL(const gchar* url)
{
    if (strncmp("http://", url, 7) != 0 && strncmp("https://", url, 8) != 0 && strncmp("file://", url, 7) != 0 && strncmp("ftp://", url, 6) != 0) {
        GString* string = g_string_new("http://");
        g_string_append(string, url);
        return g_string_free(string, FALSE);
    }
    
    return g_strdup(url);
}

static void goToURLBarText(GtkWidget* urlBarEntry)
{
    const gchar* url = gtk_entry_get_text(GTK_ENTRY(urlBarEntry));
    if (!url || strlen(url) == 0)
        return;

    gchar* parsedURL = autocorrectURL(url);
    webkit_gtk_page_open(gPage, parsedURL);
    g_free(parsedURL);
}

static void goButtonClickedCallback(GtkWidget*, GtkWidget* entry)
{
    goToURLBarText(entry);
}

static void urlBarEnterCallback(GtkWidget*, GtkWidget* entry)
{
    goToURLBarText(entry);
}

static void updateWindowTitle()
{
    GString* string = g_string_new(NULL);
    g_string_printf(string, "GdkLauncher %s  (%d/100)", gTitle, gProgress);
    gchar* title = g_string_free(string, FALSE);
    gtk_window_set_title(GTK_WINDOW(gTopLevelWindow), title);
    g_free(title);
}

static void titleChanged(WebKitGtkPage*, const gchar* title, const gchar* url, WebKitGtkPage*)
{
    gtk_entry_set_text(GTK_ENTRY(gURLBarEntry), url);

    if (gTitle)
        g_free(gTitle);
    gTitle = g_strdup(title);
    updateWindowTitle();
}

static void progressChanged(WebKitGtkPage*, gint progress, WebKitGtkPage*)
{
    gProgress = progress;
    updateWindowTitle();
}

static void frameDestroyCallback(GtkWidget*, gpointer)
{
    gtk_main_quit();
}

static void menuMainBackCallback(gpointer data)
{
    g_assert(!data);
    webkit_gtk_page_go_backward(gPage);
}

static void menuMainForwardCallback(gpointer data)
{
    g_assert(!data);
    webkit_gtk_page_go_forward(gPage);
}

static void menuMainQuitCallback(gpointer)
{
    gtk_main_quit();
}

int main(int argc, char* argv[]) 
{
    gtk_init(&argc, &argv);
    webkit_gtk_init();

    gchar* url = "http://www.google.com";
    bool exitAfterLoading = false;
    bool dumpRenderTree = false;
    for (int argPos = 1; argPos < argc; ++argPos) {
        char *currArg = argv[argPos];
        if (stringIsEqual(currArg, "-exit-after-loading"))
            exitAfterLoading = true;
        else if (stringIsEqual(currArg, "-exitafterloading"))
            exitAfterLoading = true;
        else if (stringIsEqual(currArg, "-exitafterload"))
            exitAfterLoading = true;
        else if (stringIsEqual(currArg, "-exit-after-load"))
            exitAfterLoading = true;
        else if (stringIsEqual(currArg, "-drt"))
            dumpRenderTree = true;
        else if (stringIsEqual(currArg, "-dump-render-tree"))
            dumpRenderTree = true;
        else if (stringIsEqual(currArg, "-dumprendertree"))
            dumpRenderTree = true;
        else
            url = autocorrectURL(currArg);
    }

    GtkWidget* menuMain = gtk_menu_new();
    GtkWidget* menuMainBack = gtk_menu_item_new_with_label("Back");
    gtk_menu_shell_append(GTK_MENU_SHELL(menuMain), menuMainBack);
    g_signal_connect_swapped(G_OBJECT(menuMainBack), "activate", G_CALLBACK(menuMainBackCallback), NULL);
    gtk_widget_show(menuMainBack);

    GtkWidget* menuMainForward = gtk_menu_item_new_with_label("Forward");
    gtk_menu_shell_append(GTK_MENU_SHELL(menuMain), menuMainForward);
    g_signal_connect_swapped(G_OBJECT(menuMainForward), "activate", G_CALLBACK(menuMainForwardCallback), NULL);
    gtk_widget_show(menuMainForward);

    GtkWidget* menuMainQuit = gtk_menu_item_new_with_label("Quit");
    gtk_menu_shell_append(GTK_MENU_SHELL(menuMain), menuMainQuit);
    g_signal_connect_swapped(G_OBJECT(menuMainQuit), "activate", G_CALLBACK(menuMainQuitCallback), NULL);
    gtk_widget_show(menuMainQuit);

    GtkWidget* menuMainRoot = gtk_menu_item_new_with_label("Main");
    gtk_widget_show(menuMainRoot);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuMainRoot), menuMain);

    GtkWidget* menuBar = gtk_menu_bar_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menuBar), menuMainRoot);

    gTopLevelWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(gTopLevelWindow), 800, 600);
    gtk_widget_set_name(gTopLevelWindow, "GdkLauncher");
    GtkWidget* vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(gTopLevelWindow), vbox);
    g_signal_connect(G_OBJECT(gTopLevelWindow), "destroy", G_CALLBACK(frameDestroyCallback), NULL);

    GtkWidget* hbox = gtk_hbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(vbox), menuBar, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    gURLBarEntry = gtk_entry_new();
    g_signal_connect(G_OBJECT(gURLBarEntry), "activate", G_CALLBACK(urlBarEnterCallback), (gpointer)gURLBarEntry);
    gtk_box_pack_start(GTK_BOX(hbox), gURLBarEntry, TRUE, TRUE, 0);

    GtkWidget* urlBarSubmitButton = gtk_button_new_with_label("Go");  
    gtk_box_pack_start(GTK_BOX(hbox), urlBarSubmitButton, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(urlBarSubmitButton), "clicked", G_CALLBACK(goButtonClickedCallback), (gpointer)gURLBarEntry);
    gtk_widget_show(vbox);

    GtkWidget* scrolledWindow = gtk_scrolled_window_new(NULL,NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledWindow),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gPage = WEBKIT_GTK_PAGE(webkit_gtk_page_new());
    gtk_container_add(GTK_CONTAINER(scrolledWindow), GTK_WIDGET(gPage));
    gtk_box_pack_start(GTK_BOX(vbox), scrolledWindow, TRUE, TRUE, 0);
    
    g_signal_connect(gPage, "title-changed", G_CALLBACK(titleChanged), gPage);
    g_signal_connect(gPage, "load-progress-changed", G_CALLBACK(progressChanged), gPage);
    webkit_gtk_page_open(gPage, url);

    gtk_widget_show_all(gTopLevelWindow);
    gtk_main();
    return 0;
}

