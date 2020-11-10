/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc.examples.androidvoip;

import java.util.List;

public interface OnVoipClientTaskCompleted {
  void onGetLocalIPAddressCompleted(String localIPAddress);
  void onGetSupportedCodecsCompleted(List<String> supportedCodecs);
  void onVoipClientInitializationCompleted(boolean isSuccessful);
  void onStartSessionCompleted(boolean isSuccessful);
  void onStopSessionCompleted(boolean isSuccessful);
  void onStartSendCompleted(boolean isSuccessful);
  void onStopSendCompleted(boolean isSuccessful);
  void onStartPlayoutCompleted(boolean isSuccessful);
  void onStopPlayoutCompleted(boolean isSuccessful);
  void onUninitializedVoipClient();
}
