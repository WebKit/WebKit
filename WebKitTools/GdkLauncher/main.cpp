#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include "config.h"
#include "FrameGdk.h"
#include "Page.h"
#include "Document.h"
#include "DocLoader.h"
#include "DOMImplementation.h"

#include "Cache.h"
#include "EventNames.h"

#if SVG_SUPPORT
#include "SVGNames.h"
#include "XLinkNames.h"
#include "SVGDocumentExtensions.h"
#endif

#include "RenderObject.h"
#include "GraphicsContext.h"

using namespace WebCore;

class LauncherFrameGdk : public FrameGdk
{
public:
    LauncherFrameGdk(Page* page, Element* element) : FrameGdk(page, element), m_exitAfterLoading(false) {}
    LauncherFrameGdk(GdkDrawable* drawable) : FrameGdk(drawable), m_exitAfterLoading(false) {}
    virtual void handledOnloadEvents();
    void setExitAfterLoading(bool exitAfterLoading) { m_exitAfterLoading = exitAfterLoading; }
private:
    bool m_exitAfterLoading;
};

void LauncherFrameGdk::handledOnloadEvents()
{
    if (m_exitAfterLoading)
        gtk_main_quit();
}

static  LauncherFrameGdk *frame;
static  GdkWindow *win;

static void handle_event(GdkEvent *event)
{
    if (GDK_DELETE == event->type) {
        gtk_main_quit();
        return;
    }
    frame->handleGdkEvent(event);
}

int main(int argc, char *argv[]) 
{
    GdkWindowAttr attr;
    char *url;
    gdk_init(&argc,&argv);
    gdk_event_handler_set ((GdkEventFunc)handle_event, NULL, NULL);

    attr.width = 800;
    attr.height = 600;
    attr.window_type = GDK_WINDOW_TOPLEVEL;
    attr.wclass = GDK_INPUT_OUTPUT;
    //see how where we handle motion here need to do the hint stuff
    attr.event_mask = ((GDK_ALL_EVENTS_MASK^GDK_POINTER_MOTION_HINT_MASK)); 
    win = gdk_window_new(NULL,&attr,0);
    frame = new LauncherFrameGdk(win);
    gdk_window_show(win);
    char *pg = " <html><head><title>Google</title> <body bgcolor=#ffffff text=#000000> <p><font size=-2/>2006 Google Hello bigworld from mike</p></body></html> ";
    url = "http://www.google.com";
    bool exitAfterLoading = false;
    for (int argPos = 1; argPos < argc; ++argPos) {
        if (0 == strcmp(argv[argPos], "-exit-after-loading"))
            exitAfterLoading = true;
        else
            url = argv[argPos];
    }
    frame->setExitAfterLoading(exitAfterLoading);
    if (url) {
        printf("OPENING URL == %s \n", url);
        frame->openURL(url);
    } else {
/*
  frame->createEmptyDocument();
    frame->document()->open();
    frame->write(pg,strlen(pg));
    frame->document()->close();
*/
    }
    
    gtk_main();
    delete frame;
    gdk_window_destroy(win);
    return 0;
}
