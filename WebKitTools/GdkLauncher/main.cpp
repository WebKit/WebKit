#include "config.h"
#include "ChromeClientGdk.h"
#include "Document.h"
#include "EditorClientGdk.h"
#include "FrameGdk.h"
#include "FrameLoader.h"
#include "FrameLoaderClientGdk.h"
#include "FrameView.h"
#include "KURL.h"
#include "Page.h"
#include "PlatformString.h"

#if SVG_SUPPORT
#include "SVGNames.h"
#include "XLinkNames.h"
#include "SVGDocumentExtensions.h"
#endif

#include <gdk/gdk.h>
#include <gtk/gtk.h>

using namespace WebCore;

static  FrameGdk *frame;
static  GdkWindow *win;

static void handle_event(GdkEvent *event)
{
    if (GDK_DELETE == event->type) {
        gtk_main_quit();
        return;
    }
    frame->handleGdkEvent(event);
}

int strEq(const char *str1, const char *str2)
{
    if (0 == strcmp(str1, str2))
        return 1;
    return 0;
}

int main(int argc, char *argv[]) 
{
    gdk_init(&argc,&argv);
    gdk_event_handler_set((GdkEventFunc)handle_event, NULL, NULL);

    GdkWindowAttr attr;
    attr.width = 800;
    attr.height = 600;
    attr.window_type = GDK_WINDOW_TOPLEVEL;
    attr.wclass = GDK_INPUT_OUTPUT;
    attr.event_mask = ((GDK_ALL_EVENTS_MASK^GDK_POINTER_MOTION_HINT_MASK)); 
    win = gdk_window_new(NULL, &attr, 0);
    gdk_window_show(win);

    // parse command-line arguments
    char *url = "http://www.google.com";
    bool exitAfterLoading = false;
    bool dumpRenderTree = false;
    for (int argPos = 1; argPos < argc; ++argPos) {
        char *currArg = argv[argPos];
        if (strEq(currArg, "-exit-after-loading"))
            exitAfterLoading = true;
        else if (strEq(currArg, "-exitafterloading"))
            exitAfterLoading = true;
        else if (strEq(currArg, "-exitafterload"))
            exitAfterLoading = true;
        else if (strEq(currArg, "-exit-after-load"))
            exitAfterLoading = true;
        else if (strEq(currArg, "-drt"))
            dumpRenderTree = true;
        else if (strEq(currArg, "-dump-render-tree"))
            dumpRenderTree = true;
        else if (strEq(currArg, "-dumprendertree"))
            dumpRenderTree = true;
        else
            url = currArg;
    }

    EditorClientGdk *editorClient = new EditorClientGdk();
    Page* page = new Page(new ChromeClientGdk(), 0, editorClient, 0);
    editorClient->setPage(page);
    FrameLoaderClientGdk* frameLoaderClient = new FrameLoaderClientGdk();
    frame = new FrameGdk(page, 0, frameLoaderClient);
    frame->setExitAfterLoading(exitAfterLoading);
    frame->setDumpRenderTreeAfterLoading(dumpRenderTree);
    FrameView* frameView = new FrameView(frame);
    frame->setView(frameView);
    frameView->ScrollView::setDrawable(win);

#if 0
    String pg(" <html><head><title>Google</title> <body bgcolor=#ffffff text=#000000> <p><font size=-2/>2006 Google Hello bigworld from mike</p></body></html> ");
    frame->loader()->begin();
    frame->document()->open();
    frame->document()->write(pg);
    frame->document()->close();
#else
    printf("OPENING URL == %s \n", url);
    KURL kurl(url);
    frame->loader()->load(kurl, 0);
#endif

    gtk_main();
    delete frame;
    gdk_window_destroy(win);
    return 0;
}
