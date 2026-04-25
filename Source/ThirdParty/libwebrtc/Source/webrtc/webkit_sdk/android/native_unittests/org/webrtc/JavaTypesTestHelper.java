/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import java.util.HashMap;
import java.util.Map;

public class JavaTypesTestHelper {
  @CalledByNative
  public static Map createTestStringMap() {
    Map<String, String> testMap = new HashMap<String, String>();
    testMap.put("one", "1");
    testMap.put("two", "2");
    testMap.put("three", "3");
    return testMap;
  }
}
