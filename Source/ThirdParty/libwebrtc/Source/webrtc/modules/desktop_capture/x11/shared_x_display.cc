/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/x11/shared_x_display.h"

#include <X11/Xlib.h>

#include <algorithm>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"

namespace webrtc {

SharedXDisplay::SharedXDisplay(Display* display)
  : display_(display) {
  RTC_DCHECK(display_);
}

SharedXDisplay::~SharedXDisplay() {
  RTC_DCHECK(event_handlers_.empty());
  XCloseDisplay(display_);
}

// static
rtc::scoped_refptr<SharedXDisplay> SharedXDisplay::Create(
    const std::string& display_name) {
  Display* display =
      XOpenDisplay(display_name.empty() ? NULL : display_name.c_str());
  if (!display) {
    LOG(LS_ERROR) << "Unable to open display";
    return NULL;
  }
  return new SharedXDisplay(display);
}

// static
rtc::scoped_refptr<SharedXDisplay> SharedXDisplay::CreateDefault() {
  return Create(std::string());
}

void SharedXDisplay::AddEventHandler(int type, XEventHandler* handler) {
  event_handlers_[type].push_back(handler);
}

void SharedXDisplay::RemoveEventHandler(int type, XEventHandler* handler) {
  EventHandlersMap::iterator handlers = event_handlers_.find(type);
  if (handlers == event_handlers_.end())
    return;

  std::vector<XEventHandler*>::iterator new_end =
      std::remove(handlers->second.begin(), handlers->second.end(), handler);
  handlers->second.erase(new_end, handlers->second.end());

  // Check if no handlers left for this event.
  if (handlers->second.empty())
    event_handlers_.erase(handlers);
}

void SharedXDisplay::ProcessPendingXEvents() {
  // Hold reference to |this| to prevent it from being destroyed while
  // processing events.
  rtc::scoped_refptr<SharedXDisplay> self(this);

  // Find the number of events that are outstanding "now."  We don't just loop
  // on XPending because we want to guarantee this terminates.
  int events_to_process = XPending(display());
  XEvent e;

  for (int i = 0; i < events_to_process; i++) {
    XNextEvent(display(), &e);
    EventHandlersMap::iterator handlers = event_handlers_.find(e.type);
    if (handlers == event_handlers_.end())
      continue;
    for (std::vector<XEventHandler*>::iterator it = handlers->second.begin();
         it != handlers->second.end(); ++it) {
      if ((*it)->HandleXEvent(e))
        break;
    }
  }
}

}  // namespace webrtc
