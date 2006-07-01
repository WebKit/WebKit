#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <gdk/gdk.h>
#include "config.h"
#include "FrameGdk.h"
#include "Page.h"
#include "Document.h"
#include "DocLoader.h"
#include "DOMImplementation.h"
//#include "HTMLDocument.h"

#include "Cache.h"
#include "EventNames.h"
//#include "htmlnames.h"

#if SVG_SUPPORT
#include "SVGNames.h"
#include "XLinkNames.h"
#include "SVGDocumentExtensions.h"
#endif

//painting
#include "RenderObject.h"
#include "GraphicsContext.h"


using namespace WebCore;
//using namespace HTMLNames;

static  FrameGdk *frame;
static  GdkWindow *win;

static void handle_event(GdkEvent *event)
{
    frame->handleGdkEvent(event);
}

int
main(int argc, char *argv[]) 
{
  GdkWindowAttr attr;
  GMainLoop *loop;
  gdk_init(&argc,&argv);
  gdk_event_handler_set ((GdkEventFunc)handle_event, NULL, NULL);
  loop = g_main_loop_new (NULL, TRUE);

  attr.width = 800;
  attr.height = 600;
  attr.window_type = GDK_WINDOW_TOPLEVEL;
  attr.wclass = GDK_INPUT_OUTPUT;
//see how where we handle motion here need to do the hint stuff
  attr.event_mask = ((GDK_ALL_EVENTS_MASK^GDK_POINTER_MOTION_HINT_MASK)); 
  win = gdk_window_new(NULL,&attr,0);
  frame = new FrameGdk(win);
  gdk_window_show(win);
  char *pg = " <html><head><title>Google</title> <body bgcolor=#ffffff text=#000000> <p><font size=-2/>2006 Google Hello bigworld from mike</p></body></html> ";
  if( argc >= 2 ) {
    printf("OPENING URL == %s \n", argv[1]);
    frame->openURL(argv[1]);
  } else {
/*
    frame->createEmptyDocument();
    frame->document()->open();
    frame->write(pg,strlen(pg));
    frame->document()->close();
*/
    frame->openURL("http://www.google.com");
  }

  while(1) {
    g_main_loop_run (loop);
  }
  return 0;
}
