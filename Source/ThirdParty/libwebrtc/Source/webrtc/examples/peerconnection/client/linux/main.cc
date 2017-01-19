/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <gtk/gtk.h>

#include "webrtc/examples/peerconnection/client/conductor.h"
#include "webrtc/examples/peerconnection/client/flagdefs.h"
#include "webrtc/examples/peerconnection/client/linux/main_wnd.h"
#include "webrtc/examples/peerconnection/client/peer_connection_client.h"

#include "webrtc/base/ssladapter.h"
#include "webrtc/base/thread.h"

class CustomSocketServer : public rtc::PhysicalSocketServer {
 public:
  CustomSocketServer(rtc::Thread* thread, GtkMainWnd* wnd)
      : thread_(thread), wnd_(wnd), conductor_(NULL), client_(NULL) {}
  virtual ~CustomSocketServer() {}

  void set_client(PeerConnectionClient* client) { client_ = client; }
  void set_conductor(Conductor* conductor) { conductor_ = conductor; }

  // Override so that we can also pump the GTK message loop.
  virtual bool Wait(int cms, bool process_io) {
    // Pump GTK events.
    // TODO(henrike): We really should move either the socket server or UI to a
    // different thread.  Alternatively we could look at merging the two loops
    // by implementing a dispatcher for the socket server and/or use
    // g_main_context_set_poll_func.
      while (gtk_events_pending())
        gtk_main_iteration();

    if (!wnd_->IsWindow() && !conductor_->connection_active() &&
        client_ != NULL && !client_->is_connected()) {
      thread_->Quit();
    }
    return rtc::PhysicalSocketServer::Wait(0/*cms == -1 ? 1 : cms*/,
                                                 process_io);
  }

 protected:
  rtc::Thread* thread_;
  GtkMainWnd* wnd_;
  Conductor* conductor_;
  PeerConnectionClient* client_;
};

int main(int argc, char* argv[]) {
  gtk_init(&argc, &argv);
  g_type_init();
  // g_thread_init API is deprecated since glib 2.31.0, see release note:
  // http://mail.gnome.org/archives/gnome-announce-list/2011-October/msg00041.html
#if !GLIB_CHECK_VERSION(2, 31, 0)
    g_thread_init(NULL);
#endif

  rtc::FlagList::SetFlagsFromCommandLine(&argc, argv, true);
  if (FLAG_help) {
    rtc::FlagList::Print(NULL, false);
    return 0;
  }

  // Abort if the user specifies a port that is outside the allowed
  // range [1, 65535].
  if ((FLAG_port < 1) || (FLAG_port > 65535)) {
    printf("Error: %i is not a valid port.\n", FLAG_port);
    return -1;
  }

  GtkMainWnd wnd(FLAG_server, FLAG_port, FLAG_autoconnect, FLAG_autocall);
  wnd.Create();

  rtc::AutoThread auto_thread;
  rtc::Thread* thread = rtc::Thread::Current();
  CustomSocketServer socket_server(thread, &wnd);
  thread->set_socketserver(&socket_server);

  rtc::InitializeSSL();
  // Must be constructed after we set the socketserver.
  PeerConnectionClient client;
  rtc::scoped_refptr<Conductor> conductor(
      new rtc::RefCountedObject<Conductor>(&client, &wnd));
  socket_server.set_client(&client);
  socket_server.set_conductor(conductor);

  thread->Run();

  // gtk_main();
  wnd.Destroy();

  thread->set_socketserver(NULL);
  // TODO(henrike): Run the Gtk main loop to tear down the connection.
  /*
  while (gtk_events_pending()) {
    gtk_main_iteration();
  }
  */
  rtc::CleanupSSL();
  return 0;
}
