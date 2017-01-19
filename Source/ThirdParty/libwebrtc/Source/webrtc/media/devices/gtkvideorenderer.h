/*
 *  Copyright (c) 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Definition of class GtkVideoRenderer that implements the abstract class
// cricket::VideoRenderer via GTK.

#ifndef WEBRTC_MEDIA_DEVICES_GTKVIDEORENDERER_H_
#define WEBRTC_MEDIA_DEVICES_GTKVIDEORENDERER_H_

#include <memory>

#include "webrtc/base/basictypes.h"
#include "webrtc/media/base/videosinkinterface.h"

typedef struct _GtkWidget GtkWidget;  // forward declaration, defined in gtk.h
namespace webrtc {
class VideoFrame;
}

namespace cricket {

class GtkVideoRenderer : public rtc::VideoSinkInterface<webrtc::VideoFrame> {
 public:
  GtkVideoRenderer(int x, int y);
  virtual ~GtkVideoRenderer();

  // Implementation of VideoSinkInterface.
  void OnFrame(const webrtc::VideoFrame& frame) override;

 private:
  bool SetSize(int width, int height);
  // Initialize the attributes when the first frame arrives.
  bool Initialize(int width, int height);
  // Pump the Gtk event loop until there are no events left.
  void Pump();
  // Check if the window has been closed.
  bool IsClosed() const;

  std::unique_ptr<uint8_t[]> image_;
  GtkWidget* window_;
  GtkWidget* draw_area_;
  // The initial position of the window.
  int initial_x_;
  int initial_y_;

  int width_;
  int height_;
};

}  // namespace cricket

#endif  // WEBRTC_MEDIA_DEVICES_GTKVIDEORENDERER_H_
