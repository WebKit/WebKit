#include "config.h"
#include "Platform.h"
#include "ChromeClientGdk.h"
#include "ContextMenuClientGdk.h"
#include "Document.h"
#include "DragClient.h"
#include "EditorClientGdk.h"
#include "FrameGdk.h"
#include "FrameLoader.h"
#include "FrameLoaderClientGdk.h"
#include "FrameView.h"
#include "KURL.h"
#include "Page.h"
#include "PlatformString.h"
#include "ResourceHandleManager.h"

#if SVG_SUPPORT
#include "SVGNames.h"
#include "XLinkNames.h"
#include "SVGDocumentExtensions.h"
#endif

#include <gdk/gdk.h>
#include <gtk/gtk.h>

using namespace WebCore;

static GtkWidget* gURLBarEntry;
static FrameGdk* gFrame = 0;

static bool stringIsEqual(const char* str1, const char* str2)
{
    return 0 == strcmp(str1, str2);
}

static void handleGdkEvent(GtkWidget* widget, GdkEvent* event)
{
    gFrame->handleGdkEvent(event);
}

static String autocorrectURL(const String& url)
{
    String parsedURL = url;
    if (!url.startsWith("http://") && !url.startsWith("https://")
        && !url.startsWith("file://") && !url.startsWith("ftp://"))
        parsedURL = String("http://") + url;
    return parsedURL;
}

static void goToURLBarText(GtkWidget* urlBarEntry)
{
    String url(gtk_entry_get_text(GTK_ENTRY(urlBarEntry)));
    if (url.isEmpty())
        return;

    String parsedURL = autocorrectURL(url);
    gFrame->loader()->load(ResourceRequest(parsedURL));
}

static void goButtonClickedCallback(GtkWidget* widget, GtkWidget* entry)
{
    goToURLBarText(entry);
}

static void urlBarEnterCallback(GtkWidget* widget, GtkWidget* entry)
{
    goToURLBarText(entry);
}

static void registerRenderingAreaEvents(GtkWidget* win)
{
    gtk_widget_set_events(win, GDK_EXPOSURE_MASK
                         | GDK_BUTTON_PRESS_MASK
                         | GDK_BUTTON_RELEASE_MASK
                         | GDK_POINTER_MOTION_MASK
                         | GDK_KEY_PRESS_MASK
                         | GDK_KEY_RELEASE_MASK
                         | GDK_BUTTON_MOTION_MASK
                         | GDK_BUTTON1_MOTION_MASK
                         | GDK_BUTTON2_MOTION_MASK
                         | GDK_BUTTON3_MOTION_MASK);

    g_signal_connect(GTK_OBJECT(win), "expose-event", G_CALLBACK(handleGdkEvent), NULL);
    g_signal_connect(GTK_OBJECT(win), "key-press-event", G_CALLBACK(handleGdkEvent), NULL);
    g_signal_connect(GTK_OBJECT(win), "key-release-event", G_CALLBACK(handleGdkEvent), NULL);
    g_signal_connect(GTK_OBJECT(win), "button-press-event", G_CALLBACK(handleGdkEvent), NULL);
    g_signal_connect(GTK_OBJECT(win), "button-release-event", G_CALLBACK(handleGdkEvent), NULL);
    g_signal_connect(GTK_OBJECT(win), "motion-notify-event", G_CALLBACK(handleGdkEvent), NULL);
    g_signal_connect(GTK_OBJECT(win), "scroll-event", G_CALLBACK(handleGdkEvent), NULL);
}

static void frameResizeCallback(GtkWidget* widget, GtkAllocation* allocation, gpointer data)
{
    if (!gFrame)
        return;

    gFrame->view()->setFrameGeometry(IntRect(allocation->x,allocation->y,allocation->width,allocation->height));
    gFrame->forceLayout();
    gFrame->sendResizeEvent();
}

static void frameDestroyCallback(GtkWidget* widget, gpointer data)
{
    gtk_main_quit();
}

static void menuMainBackCallback(gpointer data)
{
    ASSERT(!data);
    gFrame->loader()->goBackOrForward(-1);
}

static void menuMainForwardCallback(gpointer data)
{
    ASSERT(!data);
    gFrame->loader()->goBackOrForward(1);
}

static void menuMainQuitCallback(gpointer data)
{
    gtk_main_quit();
}

int main(int argc, char* argv[]) 
{
    gtk_init(&argc, &argv);

    String url("http://www.google.com");
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

    GtkWidget* topLevelWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(topLevelWindow), 800, 600);
    gtk_widget_set_name(topLevelWindow, "GdkLauncher");
    GtkWidget* vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(topLevelWindow), vbox);
    g_signal_connect(G_OBJECT(topLevelWindow), "destroy", G_CALLBACK(frameDestroyCallback), NULL);

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

    GtkWidget* frameWindow = gtk_drawing_area_new();
    registerRenderingAreaEvents(frameWindow); 
    gtk_box_pack_start(GTK_BOX(vbox), frameWindow, TRUE, TRUE, 0);
    g_signal_connect(GTK_OBJECT(frameWindow), "size-allocate", G_CALLBACK(frameResizeCallback), NULL);
    gtk_widget_show(frameWindow);
    GTK_WIDGET_SET_FLAGS(frameWindow, GTK_CAN_FOCUS);

    gtk_widget_show_all(topLevelWindow);

    EditorClientGdk* editorClient = new EditorClientGdk;
    ContextMenuClient* contextMenuClient = new ContextMenuClientGdk;
    Page* page = new Page(new ChromeClientGdk, contextMenuClient, editorClient, 0);
    editorClient->setPage(page);
    FrameLoaderClientGdk* frameLoaderClient = new FrameLoaderClientGdk;
    gFrame = new FrameGdk(page, 0, frameLoaderClient);
    gFrame->setExitAfterLoading(exitAfterLoading);
    gFrame->setDumpRenderTreeAfterLoading(dumpRenderTree);

    FrameView* frameView = new FrameView(gFrame);
    gFrame->setView(frameView);
    frameView->ScrollView::setDrawable(frameWindow->window);

    gFrame->loader()->load(ResourceRequest(url));
    gtk_main();
#if 0 // FIXME: this crashes at the moment. needs to provide DragClient
    delete page;
#endif
    if (FrameLoader* loader = gFrame->loader())
        loader->stopAllLoaders();
    delete gFrame;
    return 0;
}

