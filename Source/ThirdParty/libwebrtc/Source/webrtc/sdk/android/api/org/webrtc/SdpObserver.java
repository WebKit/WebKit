/*
 *  Copyright 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

/** Interface for observing SDP-related events. */
public interface SdpObserver {
  /** Called on success of Create{Offer,Answer}(). */
  public void onCreateSuccess(SessionDescription sdp);

  /** Called on success of Set{Local,Remote}Description(). */
  public void onSetSuccess();

  /** Called on error of Create{Offer,Answer}(). */
  public void onCreateFailure(String error);

  /** Called on error of Set{Local,Remote}Description(). */
  public void onSetFailure(String error);
}
